#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Routines to build up faces and volumes.

// Search for a closed chain of connected edges (having coincident endpoints within tol)
// and if found, make a group out of them. The chain may be closed or open.
Group*
group_connected_edges(Edge* edge)
{
    Object* obj, * nextobj;
    Edge* e;
    Group* group;
    int n_edges = 0;
    int pass;
    typedef struct End          // an end of the growing chain
    {
        int which_end;          // endpoint 0 or 1
        Edge* edge;             // which edge it is on
    } End;
    End end0, end1;
    BOOL legit = FALSE;
    int max_passes = 0;

    if (((Object*)edge)->type != OBJ_EDGE)
        return NULL;

    // Put the first edge in the list, removing it from the object tree.
    // It had better be in the object tree to start with! Check to be sure,
    // and while here, find out how many top-level edges there are as an
    // upper bound on passes later on.
    for (obj = object_tree.obj_list.head; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_EDGE)
        {
            max_passes++;
            if (obj == (Object*)edge)
                legit = TRUE;
        }
    }
    if (!legit)
        return NULL;

    // make a group for the edges
    group = group_new();
    delink_group((Object*)edge, &object_tree);
    link_group((Object*)edge, group);
    end0.edge = edge;
    end0.which_end = 0;
    end1.edge = edge;
    end1.which_end = 1;

    // Make passes over the tree, greedy-grouping edges as they are found to connect.
    // If the chain is closed, we should add at least 2 edges per pass, and could add
    // a lot more than that with favourable ordering.
    for (pass = 0; pass < max_passes; pass++)
    {
        BOOL advanced = FALSE;

        for (obj = object_tree.obj_list.head; obj != NULL; obj = nextobj)
        {
            nextobj = obj->next;

            if (obj->type != OBJ_EDGE)
                continue;

            e = (Edge*)obj;
            if (near_pt(e->endpoints[0], end0.edge->endpoints[end0.which_end], snap_tol))
            {
                // endpoint 0 of obj connects to end0. Put obj in the list.
                delink_group(obj, &object_tree);
                link_group(obj, group);

                // Share the endpoints. Return unused points to the free list.
                purge_obj((Object *)e->endpoints[0]);
                e->endpoints[0] = end0.edge->endpoints[end0.which_end];

                // Update end0 to point to the other end of the new edge we just added
                end0.edge = e;
                end0.which_end = 1;
                n_edges++;
                advanced = TRUE;
            }

            // Check for endpoint 1 connecting to end0 similarly
            else if (near_pt(e->endpoints[1], end0.edge->endpoints[end0.which_end], snap_tol))
            {
                delink_group(obj, &object_tree);
                link_group(obj, group);
                purge_obj((Object*)e->endpoints[1]);
                e->endpoints[1] = end0.edge->endpoints[end0.which_end];
                end0.edge = e;
                end0.which_end = 0;
                n_edges++;
                advanced = TRUE;
            }

            // And the same for end1. New edges are linked at the tail. 
            else if (near_pt(e->endpoints[0], end1.edge->endpoints[end1.which_end], snap_tol))
            {
                delink_group(obj, &object_tree);
                link_tail_group(obj, group);
                purge_obj((Object*)e->endpoints[0]);
                e->endpoints[0] = end1.edge->endpoints[end1.which_end];
                end1.edge = e;
                end1.which_end = 1;
                n_edges++;
                advanced = TRUE;
            }

            else if (near_pt(e->endpoints[1], end1.edge->endpoints[end1.which_end], snap_tol))
            {
                delink_group(obj, &object_tree);
                link_tail_group(obj, group);
                purge_obj((Object*)e->endpoints[1]);
                e->endpoints[1] = end1.edge->endpoints[end1.which_end];
                end1.edge = e;
                end1.which_end = 0;
                n_edges++;
                advanced = TRUE;
            }

            if (near_pt(end0.edge->endpoints[end0.which_end], end1.edge->endpoints[end1.which_end], snap_tol))
            {
                // We have closed the chain.
                purge_obj((Object*)end0.edge->endpoints[end0.which_end]);
                end0.edge->endpoints[end0.which_end] = end1.edge->endpoints[end1.which_end];
                group->hdr.lock = LOCK_POINTS;
                return group;
            }
        }

        // Every pass should advance at least one of the ends. Break out if we can't.
        if (!advanced)
            break;
    }

    group->hdr.lock = LOCK_POINTS;
    return group;
}

// Check if a group is an edge group. Very cursory check.
BOOL
is_edge_group(Group* group)
{
    if (group == NULL)
        return FALSE;
    if (group->hdr.type != OBJ_GROUP)
        return FALSE;
    if (group->hdr.lock > LOCK_EDGES)
        return FALSE;
    if (group->obj_list.head == NULL)
        return FALSE;
    if (group->obj_list.head->type != OBJ_EDGE)
        return FALSE;

    return TRUE;
}

// Check if an edge group is closed.
BOOL
is_closed_edge_group(Group* group)
{
    Edge* first, * last;

    if (!is_edge_group(group))
        return FALSE;

    first = (Edge*)group->obj_list.head;
    last = (Edge*)group->obj_list.tail;
    if (first->endpoints[0] == last->endpoints[0])
        return TRUE;
    if (first->endpoints[0] == last->endpoints[1])
        return TRUE;
    if (first->endpoints[1] == last->endpoints[0])
        return TRUE;
    if (first->endpoints[1] == last->endpoints[1])
        return TRUE;

    return FALSE;
}

// Disconnect the shared points of all the edges in an edge group, in preparation
// for ungrouping them.
void
disconnect_edges_in_group(Group* group)
{
    Edge* e, *next_edge;
    Point* pt;
    int initial, final;

    if (!is_edge_group(group))
        return;

    // Find the initial point
    e = (Edge*)group->obj_list.head;
    next_edge = (Edge*)group->obj_list.head->next;
    if (e->endpoints[0] == next_edge->endpoints[0])
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (e->endpoints[0] == next_edge->endpoints[1])
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (e->endpoints[1] == next_edge->endpoints[0])
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else if (e->endpoints[1] == next_edge->endpoints[1])
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else
    {
        return;     // edges are not connected
    }

    for (; e->hdr.next != NULL; e = next_edge)
    {
        next_edge = (Edge*)e->hdr.next;

        if (next_edge->endpoints[0] == pt)
        {
            next_edge->endpoints[0] = point_newp(pt);
            final = 1;
        }
        else if (next_edge->endpoints[1] == pt)
        {
            next_edge->endpoints[1] = point_newp(pt);
            final = 0;
        }
        else
        {
            return;
        }
        pt = next_edge->endpoints[final];
    }

    e = (Edge*)group->obj_list.head;
    if (pt == e->endpoints[initial])
        e->endpoints[initial] = point_newpv(pt);
}

// Make a face object out of a closed group of connected edges.
// The original group is deleted and the face is put into the object tree.
Face*
make_face(Group* group)
{
    Face* face = NULL;
    Edge* e, * next_edge, * prev_edge;
    Plane norm;
    BOOL reverse = FALSE;
    int initial, final, n_edges;
    ListHead plist = { NULL, NULL };
    Point* pt;
    int i;
    FACE face_type;
    EDGE edge_types[4], min_type, max_type;

    // Check (quickly) that the group is closed.
    if (!is_closed_edge_group(group))
        return NULL;

    // Determine normal of points gathered up so far. From this we decide
    // which order to build the final edge array on the face. Join the
    // endpoints up into a list (the next/prev pointers aren't used for
    // anything else)
    e = (Edge*)group->obj_list.head;
    next_edge = (Edge*)group->obj_list.head->next;
    if (e->endpoints[0] == next_edge->endpoints[0])
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (e->endpoints[0] == next_edge->endpoints[1])
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (e->endpoints[1] == next_edge->endpoints[0])
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else if (e->endpoints[1] == next_edge->endpoints[1])
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else
    {
        ASSERT(FALSE, "The edges aren't connected");
        return NULL;
    }

    link_tail((Object*)e->endpoints[initial], &plist);
    n_edges = 1;

    for (; e->hdr.next != NULL; e = next_edge)
    {
        next_edge = (Edge*)e->hdr.next;

        if (e->hdr.next->type != OBJ_EDGE)
            return NULL;

        // Strip construction edges, as we can't consistently create them
        e->type &= ~EDGE_CONSTRUCTION;

        if (next_edge->endpoints[0] == pt)
        {
            link_tail((Object*)next_edge->endpoints[0], &plist);
            final = 1;
        }
        else if (next_edge->endpoints[1] == pt)
        {
            link_tail((Object*)next_edge->endpoints[1], &plist);
            final = 0;
        }
        else
        {
            return NULL;
        }
        pt = next_edge->endpoints[final];
        n_edges++;
    }

    // Get the normal and see if we need to reverse. This depends on the face being generally
    // in one plane (it may be a bit curved, but a full circle cylinder will confuse things..)
    polygon_normal((Point*)plist.head, &norm);
    if (dot(norm.A, norm.B, norm.C, facing_plane->A, facing_plane->B, facing_plane->C) < 0)
    {
        reverse = TRUE;
        norm.A = -norm.A;
        norm.B = -norm.B;
        norm.C = -norm.C;
    }
    norm.refpt = *((Edge*)group->obj_list.head)->endpoints[initial];

    // Add any Bezier control points and circle centres into the points list
    // so we can test them for being in the same plane later on
    for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
    {
        switch (e->type)
        {
        case EDGE_ARC:
            link_tail((Object*)((ArcEdge*)e)->centre, &plist);
            break;
        case EDGE_BEZIER:
            link_tail((Object*)((BezierEdge*)e)->ctrlpoints[0], &plist);
            link_tail((Object*)((BezierEdge*)e)->ctrlpoints[1], &plist);
            break;
        }
    }

    // Make the face, of whatever type matches the input edges
    // Determine if the face is really flat by checking distance to a plane defined
    // by the normal we found above.
    max_type = EDGE_STRAIGHT;
    min_type = EDGE_BEZIER;
    if (n_edges != 4)
    {
        // More than 4 edges cannot be a curved face, so make it flat
        face_type = (n_edges == 3) ? FACE_TRI : FACE_FLAT;
    }
    else if (polygon_planar((Point*)plist.head, &norm))
    {
        // If all edges lie in one plane, make it flat too
        face_type = FACE_FLAT;
    }
    else
    {
        // We have a non-flat face bounded by 4 edges. Find the different edge types
        for (i = 0, e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next, i++)
        {
            edge_types[i] = e->type;
            if (e->type < min_type)
                min_type = e->type;
            if (e->type > max_type)
                max_type = e->type;
        }
        ASSERT(i == 4, "Already tested this");
        if (edge_types[0] != edge_types[2] || edge_types[1] != edge_types[3] || max_type == EDGE_STRAIGHT)
            face_type = FACE_FLAT;          // must be flat, even if out of plane
        else if (min_type == EDGE_STRAIGHT)
            face_type = FACE_CYLINDRICAL;   // two straight, other two arc or bez
        else if (min_type == EDGE_ARC)
            face_type = FACE_BARREL;    // two arc, other two arc or bez
        else if (min_type == EDGE_BEZIER)
            face_type = FACE_BEZIER;    // four bez

        // Make sure opposing pairs of curved edges have their step counts in agreement
        for (i = 0, e = (Edge*)group->obj_list.head; e != NULL && i < 2; e = (Edge*)e->hdr.next, i++)
        {
            Edge* o = (Edge*)e->hdr.next->next;
            int n_steps;

            ASSERT(e->type == o->type, "Already tested this");
            if (e->type >= EDGE_ARC)
            {
                // Set step counts to the maximum of the two, and zero the step size so it gets
                // recalculated again with the next view list update.
                n_steps = e->nsteps;
                if (o->nsteps > n_steps)
                    n_steps = o->nsteps;
                e->nsteps = n_steps;
                o->nsteps = n_steps;
                e->stepsize = 0;
                o->stepsize = 0;
                e->stepping = TRUE;
                o->stepping = TRUE;
                e->view_valid = FALSE;
                o->view_valid = FALSE;
            }
        }
    }

    face = face_new(face_type, norm);
    face->n_edges = n_edges;
    if (face_type == FACE_FLAT)
    {
        while (face->max_edges <= n_edges)
            face->max_edges <<= 1;          // round up to a power of 2
        if (face->max_edges > 16)           // does it need any more than the default 16?
            face->edges = realloc(face->edges, face->max_edges * sizeof(Edge*));
    }

    // Populate the edge list. 
    if (reverse)
    {
        face->initial_point = next_edge->endpoints[final];
        for (i = 0, e = next_edge; e != NULL; e = prev_edge, i++)
        {
            prev_edge = (Edge*)e->hdr.prev;
            face->edges[i] = e;
            delink_group((Object*)e, group);
        }
    }
    else
    {
        face->initial_point = ((Edge*)group->obj_list.head)->endpoints[initial];
        for (i = 0, e = (Edge*)group->obj_list.head; e != NULL; e = next_edge, i++)
        {
            next_edge = (Edge*)e->hdr.next;
            face->edges[i] = e;
            delink_group((Object*)e, group);
        }
    }

    // Delete the group
    ASSERT(group->obj_list.head == NULL, "Edge group is not empty");
    delink_group((Object*)group, &object_tree);
    purge_obj((Object*)group);

    // Finally, update the view list for the face
    face->view_valid = FALSE;
    gen_view_list_face(face);

    return face;
}

// Insert an edge at a corner point; either a single straight (chamfer) or an arc (round).
// Optionally, restrict the radius or chamfer to prevent rounds colliding at short edges.
void
insert_chamfer_round(Point* pt, Face* parent, float size, EDGE edge_type, BOOL restricted)
{
    Edge* e[2] = { NULL, NULL };
    int i, k, end[2], eindex[2];
    Point orig_pt = *pt;
    Edge* ne;
    float len0, len1, backoff;

    if (parent->hdr.type != OBJ_FACE)
        return;     // We can't do this

    // Find the two edges that share this point
    k = 0;
    for (i = 0; i < parent->n_edges; i++)
    {
        if (parent->edges[i]->endpoints[0] == pt)
        {
            e[k] = parent->edges[i];
            eindex[k] = i;
            end[k] = 0;
            k++;
        }
        else if (parent->edges[i]->endpoints[1] == pt)
        {
            e[k] = parent->edges[i];
            eindex[k] = i;
            end[k] = 1;
            k++;
        }
    }

    if (k != 2)
    {
        ASSERT(FALSE, "Point should belong to 2 edges");
        return;   // something is wrong; the point does not belong to two edges
    }

    ASSERT(pt == e[0]->endpoints[end[0]], "Wtf?");
    ASSERT(pt == e[1]->endpoints[end[1]], "Wtf?");

    // Back the original point up by "size" along its edge. Watch short edges.
    len0 = length(e[0]->endpoints[0], e[0]->endpoints[1]);
    len1 = length(e[1]->endpoints[0], e[1]->endpoints[1]);
    backoff = size;
    if (restricted)
    {
        if (backoff > len0 / 3)
            backoff = len0 / 3;
        if (backoff > len1 / 3)
            backoff = len1 / 3;
    }
    new_length(e[0]->endpoints[1 - end[0]], e[0]->endpoints[end[0]], len0 - backoff);

    // Add a new point for the other edge
    e[1]->endpoints[end[1]] = point_newp(&orig_pt);

    // Back this one up too
    new_length(e[1]->endpoints[1 - end[1]], e[1]->endpoints[end[1]], len1 - backoff);

    // Add a new edge
    ne = edge_new(edge_type);
    ne->endpoints[0] = e[0]->endpoints[end[0]];
    ne->endpoints[1] = e[1]->endpoints[end[1]];
    if (edge_type == EDGE_ARC)
    {
        ArcEdge* ae = (ArcEdge*)ne;

        ae->centre = point_new(0, 0, 0);
        ae->normal = parent->normal;
        if (!centre_2pt_tangent_circle(ne->endpoints[0], ne->endpoints[1], &orig_pt, &parent->normal, ae->centre, &ae->clockwise))
            ne->type = EDGE_STRAIGHT;
    }

    // Grow the edges array if it won't take the new edge
    if (parent->n_edges >= parent->max_edges)
    {
        while (parent->max_edges <= parent->n_edges)
            parent->max_edges <<= 1;          // round up to a power of 2
        parent->edges = realloc(parent->edges, parent->max_edges * sizeof(Edge*));
    }

    // Shuffle the edges array down and insert it
    // Special case if e[] = 0 and n_edges-1, insert at the end
    if (eindex[0] == 0 && eindex[1] == parent->n_edges - 1)
    {
        parent->edges[parent->n_edges] = ne;
    }
    else
    {
        for (k = parent->n_edges - 1; k >= eindex[1]; --k)
            parent->edges[k + 1] = parent->edges[k];
        parent->edges[eindex[1]] = ne;
    }
    parent->n_edges++;
    ne->corner = TRUE;      // mark this as a corner edge
}

// Helper to find the greatest radius to the path within the points of
// an edge's view list.
float
dist_view_list_to_path(Edge* e, Edge* path)
{
    float r;
    float rad = 0;
    Point* p;
    Point dummy;

    if (e->type == EDGE_ARC || e->type == EDGE_BEZIER)
    {
        for (p = (Point *)e->view_list.head; p != NULL; p = (Point *)p->hdr.next)
        {
            r = dist_point_to_perp_line(p, path, &dummy);
            if (r > rad)
                rad = r;
        }
    }
    return rad;
}

// Make a body of revolution by revolving the given edge group around the 
// current path. Open edge groups are completed with circles at the poles.
// If successful, the volume is returned. The edge group is retained.
// A negative volume (hole) may be generated.
Volume *
make_body_of_revolution(Group* group, BOOL negative)
{
    ListHead plist = { NULL, NULL };
    Group* clone, *orig;
    Volume* vol;
    Face* f;
    Edge* e, *ne, *na, *o, *path, *prev_ne, *prev_na, *first_ne, *first_na;
    ArcEdge* nae;
    Point* eip, * oip, * old_eip, * old_oip, * first_eip, * first_oip;
    Point *pt;
    Point top_centre, bottom_centre;
    Plane axis, group_norm, top_axis, outward;
    int idx, initial, final, n_steps;
    BOOL open = !is_closed_edge_group(group);
    BOOL wind_reverse;
    float r, rad;

    // Some checks first.
    if (!is_edge_group(group))
        return NULL;
    if (curr_path == NULL)
        return NULL;
    if (curr_path->type == OBJ_GROUP)
        path = (Edge*)(((Group*)curr_path)->obj_list.head);
    else
        path = (Edge*)curr_path;
    ASSERT(path->hdr.type == OBJ_EDGE && ((Edge *)path)->type == EDGE_STRAIGHT, "Path must be a straight edge");
    if (path->hdr.type != OBJ_EDGE || ((Edge*)path)->type != EDGE_STRAIGHT)
        return NULL;

    // Find the initial point index. 
    // If there is only one edge, arbitrarily use endpoint 0 for the initial point.
    e = (Edge*)group->obj_list.head;
    ne = (Edge*)group->obj_list.head->next;
    if (ne != NULL)
    {
        if (e->endpoints[0] == ne->endpoints[0])
        {
            initial = 1;
            pt = e->endpoints[0];
        }
        else if (e->endpoints[0] == ne->endpoints[1])
        {
            initial = 1;
            pt = e->endpoints[0];
        }
        else if (e->endpoints[1] == ne->endpoints[0])
        {
            initial = 0;
            pt = e->endpoints[1];
        }
        else if (e->endpoints[1] == ne->endpoints[1])
        {
            initial = 0;
            pt = e->endpoints[1];
        }
        else
        {
            ASSERT(FALSE, "The edges aren't connected");
            return NULL;
        }
    }
    else
    {
        ne = e;
        initial = 0;
        pt = e->endpoints[1];
        final = 1;
    }

    // Start remembering the radii to the axis, as well as the top and
    // bottom points' centres, in case the group is open and we need to 
    // put in circle faces to close the volume.
    rad = dist_point_to_perp_line(e->endpoints[initial], path, &top_centre);
    r = dist_view_list_to_path(e, path);
    if (r > rad)
        rad = r;

    // Connect up all the edges by sharing their common points, and remember
    // the points in a list so we can easily find their normal and hence their
    // winding direction
    link_tail((Object*)e->endpoints[initial], &plist);
    for (; e->hdr.next != NULL; e = ne)
    {
        ne = (Edge*)e->hdr.next;

        if (e->hdr.next->type != OBJ_EDGE)
            return NULL;

        // Strip construction edges, as we can't consistently create them
        e->type &= ~EDGE_CONSTRUCTION;

        if (ne->endpoints[0] == pt)
        {
            link_tail((Object*)ne->endpoints[0], &plist);
            final = 1;
        }
        else if (ne->endpoints[1] == pt)
        {
            link_tail((Object*)ne->endpoints[1], &plist);
            final = 0;
        }
        else
        {
            return FALSE;
        }
        pt = ne->endpoints[final];
        r = dist_point_to_perp_line(pt, path, &bottom_centre);
        if (r > rad)
            rad = r;
        r = dist_view_list_to_path(ne, path);
        if (r > rad)
            rad = r;
    }

    r = dist_point_to_perp_line(ne->endpoints[final], path, &bottom_centre);
    if (r > rad)
        rad = r;

    idx = initial;

    // Determine the normal of the group points
    // If group is open, we need to add in top_centre and bottom_centre 
    if (open)
    {
        link_tail((Object*)pt, &plist);
        link_tail((Object*)&bottom_centre, &plist);
        link_tail((Object*)&top_centre, &plist);
    }
    polygon_normal((Point*)plist.head, &group_norm);

    // The plane derived from the path used as an axis
    axis.A = path->endpoints[1]->x - path->endpoints[0]->x;
    axis.B = path->endpoints[1]->y - path->endpoints[0]->y;
    axis.C = path->endpoints[1]->z - path->endpoints[0]->z;
    normalise_plane(&axis);

    // Determine winding direction of the faces we are about to generate. It depends on
    // the direction of the axis and the group normal (and also on whether we want
    // a positive volume or a hole)
    plcross(&group_norm, &axis, &outward);

    // Find if the outward vector really points outward (away from the axis)
    // and hence determine which way to wind the faces' edge ordering.
    e = (Edge*)group->obj_list.head;
    top_axis.A = e->endpoints[initial]->x - top_centre.x;
    top_axis.B = e->endpoints[initial]->y - top_centre.y;
    top_axis.C = e->endpoints[initial]->z - top_centre.z;
    wind_reverse = (pldot(&outward, &top_axis) < 0) ^ negative;

    // Work out the number of steps in all the arcs. They must all be the
    // same, so use the worst case (from the largest radius to the axis gathered above)
    n_steps = (int)(2 * PI / (2.0 * acos(1.0 - tolerance / rad)));

    // Clone the edge list (twice) in the same location, and fix any edges' nsteps.
    orig = (Group*)copy_obj((Object*)group, 0, 0, 0, TRUE);
    clear_move_copy_flags((Object*)group);
    clone = (Group*)copy_obj((Object*)group, 0, 0, 0, TRUE);
    clear_move_copy_flags((Object*)group);
    e = (Edge*)orig->obj_list.head;
    o = (Edge *)clone->obj_list.head;

    // Add the first pair of edges (a circle and a zero-width straight)
    // and put the closing circle face in if the group is open.
    eip = e->endpoints[idx];
    oip = o->endpoints[idx];
    prev_ne = edge_new(EDGE_STRAIGHT);
    prev_ne->endpoints[0] = eip;
    prev_ne->endpoints[1] = oip;
    prev_na = edge_new(EDGE_ARC);
    prev_na->endpoints[0] = oip;
    prev_na->endpoints[1] = eip;
    prev_na->nsteps = n_steps;
    prev_na->stepping = TRUE;
    nae = (ArcEdge*)prev_na;
    nae->centre = point_new(0, 0, 0);
    dist_point_to_perp_line(oip, path, nae->centre);
    nae->clockwise = FALSE;
    nae->normal = axis;
    nae->normal.refpt = *nae->centre;

    old_eip = eip;
    old_oip = oip;
    first_ne = prev_ne;
    first_na = prev_na;
    first_eip = eip;
    first_oip = oip;

    idx = 1 - idx;      // point to far end of edge in group
    ne = na = NULL;     // shhh compiler

    vol = vol_new();
    vol->hdr.lock = LOCK_FACES;
    vol->op = negative ? OP_INTERSECTION : OP_UNION;

    // Proceed down the edge group adding arc edges and barrel faces to the volume
    for 
    (
        e = (Edge *)orig->obj_list.head, o = (Edge*)clone->obj_list.head;
        e != NULL; 
        e = (Edge*)e->hdr.next, o = (Edge*)o->hdr.next
    )
    {
        Plane n = { 0, };
        FACE ftype = (e->type == EDGE_STRAIGHT) ? FACE_RECT : FACE_CYLINDRICAL;

        eip = e->endpoints[idx];
        oip = o->endpoints[idx];

        // If were rejoining to the initial point, use the first arc edge we generated
        if (e->hdr.next == NULL && !open)
        {
            ne = first_ne;
            na = first_na;
            eip = first_eip;
            oip = first_oip;
        }
        else
        {
            ne = edge_new(EDGE_STRAIGHT);
            ne->endpoints[0] = eip;
            ne->endpoints[1] = oip;
            na = edge_new(EDGE_ARC);
            na->endpoints[0] = oip;
            na->endpoints[1] = eip;
            na->nsteps = n_steps;
            na->stepping = TRUE;
            nae = (ArcEdge*)na;
            nae->centre = point_new(0, 0, 0);
            dist_point_to_perp_line(oip, path, nae->centre);
            nae->clockwise = FALSE;
            nae->normal = axis;
            nae->normal.refpt = *nae->centre;
        }

        f = face_new(ftype, n);
        f->n_edges = 4;
        if (wind_reverse)
        {
            f->edges[0] = ne;
            f->edges[1] = o;
            f->edges[2] = prev_ne;
            f->edges[3] = e;
            f->initial_point = eip;
        }
        else
        {
            f->edges[0] = ne;
            f->edges[1] = e;
            f->edges[2] = prev_ne;
            f->edges[3] = o;
            f->initial_point = oip;
        }
        f->vol = vol;
        link((Object*)f, &vol->faces);
        if (ftype > vol->max_facetype)
            vol->max_facetype = ftype;

        ftype = (e->type == EDGE_STRAIGHT) ? FACE_CYLINDRICAL : FACE_BARREL;
        f = face_new(ftype, n);
        f->n_edges = 4;
        if (wind_reverse)
        {
            f->edges[0] = prev_na;
            f->edges[1] = e;
            f->edges[2] = na;
            f->edges[3] = o;
            f->initial_point = old_oip;
        }
        else
        {
            f->edges[0] = prev_na;
            f->edges[1] = o;
            f->edges[2] = na;
            f->edges[3] = e;
            f->initial_point = old_eip;
        }
        f->vol = vol;
        link((Object*)f, &vol->faces);
        if (ftype > vol->max_facetype)
            vol->max_facetype = ftype;

        delink_group((Object*)e, orig);
        delink_group((Object*)o, clone);

        prev_ne = ne;
        prev_na = na;
        old_eip = eip;
        old_oip = oip;

        // Find the far end of the next pair of edges
        if (e->hdr.next == NULL)
            break;

        ne = (Edge*)e->hdr.next;
        if (e->endpoints[idx] == ne->endpoints[0])
            idx = 1;
        else
            idx = 0;
    }

    // Add the first and last circles if group was open
    if (open)
    {
        Face* circle;
        Plane norm;

        norm.A = top_centre.x - bottom_centre.x;
        norm.B = top_centre.y - bottom_centre.y;
        norm.C = top_centre.z - bottom_centre.z;
        norm.refpt = top_centre;
        normalise_plane(&norm);
        if (negative)
        {
            norm.A = -norm.A;
            norm.B = -norm.B;
            norm.C = -norm.C;
        }
        circle = face_new(FACE_CIRCLE, norm);
        if (wind_reverse)
        {
            circle->edges[0] = first_na;
            circle->edges[1] = first_ne;
        }
        else
        {
            circle->edges[0] = first_ne;
            circle->edges[1] = first_na;
        }
        circle->n_edges = 2;
        circle->initial_point = first_eip;
        circle->vol = vol;
        link((Object*)circle, &vol->faces);

        norm.A = -norm.A;
        norm.B = -norm.B;
        norm.C = -norm.C;
        norm.refpt = bottom_centre;
        circle = face_new(FACE_CIRCLE, norm);
        if (wind_reverse)
        {
            circle->edges[0] = ne;
            circle->edges[1] = na;
        }
        else
        {
            circle->edges[0] = na;
            circle->edges[1] = ne;
        }
        circle->n_edges = 2;
        circle->initial_point = eip;
        circle->vol = vol;
        link((Object*)circle, &vol->faces);
    }

    // Finally, remove the cloned edge groups
    ASSERT(orig->obj_list.head == NULL, "Edge group is not empty");
    ASSERT(clone->obj_list.head == NULL, "Edge group is not empty");
    purge_obj((Object*)orig);
    purge_obj((Object*)clone);

    return vol;
}

// Quick helper to shoot out an error message and return NULL.
#define ERR_RETURN(str)     { show_status("Error: ", (str)); return NULL; }

// Helper to find the first point of an edge group (the one not in common with the
// next edge)
Point *
first_point(Edge* edge)
{
    Edge* next_edge = (Edge *)edge->hdr.next;

    // Special case where next_edge is NULL (the last edge in the edge group)
    if (next_edge == NULL)
    {
        Edge* prev_edge = (Edge*)edge->hdr.prev;

        if (edge->endpoints[0] == prev_edge->endpoints[0])
            return edge->endpoints[0];
        else if (edge->endpoints[0] == prev_edge->endpoints[1])
            return edge->endpoints[0];
        else if (edge->endpoints[1] == prev_edge->endpoints[0])
            return edge->endpoints[1];
        else if (edge->endpoints[1] == prev_edge->endpoints[1])
            return edge->endpoints[1];
        else
            ASSERT(FALSE, "Edges do not join up");
    }
    else
    {
        if (edge->endpoints[0] == next_edge->endpoints[0])
            return edge->endpoints[1];
        else if (edge->endpoints[0] == next_edge->endpoints[1])
            return edge->endpoints[1];
        else if (edge->endpoints[1] == next_edge->endpoints[0])
            return edge->endpoints[0];
        else if (edge->endpoints[1] == next_edge->endpoints[1])
            return edge->endpoints[0];
        else
            ASSERT(FALSE, "Edges do not join up");
    }
    return NULL;
}

// Struct describing an edge group, its normal and centroid, and distance along the
// path used for sorting.
typedef struct
{
    Group* egrp;
    Plane norm;
    float param;
} LoftedGroup;

// qsort comparo function.
int
compare_lofted_groups(const void* elem1, const void* elem2)
{
    LoftedGroup* g1 = (LoftedGroup*)elem1;
    LoftedGroup* g2 = (LoftedGroup*)elem2;

    if (g1->param < g2->param)
        return -1;
    else if (g1->param > g2->param)
        return 1;
    else
        return 0;
}

// Make a lofted volume from a group of sections, represented as edge groups.
// There may or may not be a current path. If there is not, the endcaps are truncated.
// The input group is retained to allow editing and re-lofting.
Volume *
make_lofted_volume(Group* group)
{
    Group* clone, * egrp, * first_egrp = NULL;
    Edge* first_edge;
    Object* obj;
    Plane principal, pl;
    Point endpt;
    Point* first_pt;
    int num_groups, num_edges;
    Bbox box;
    int i;
    LoftedGroup* lg;
    float path_length;

    // Some sanity checks on the input. Some will be enforced outside.
    ASSERT(group->hdr.type == OBJ_GROUP, "Must be a group");

    // Ensure the edge groups all have the same number and type of edges.
    num_groups = group->n_members;
    num_edges = 0;
    for (obj = group->obj_list.head; obj != NULL; obj = obj->next)
    {
        Edge* e, * e0;

        egrp = (Group*)obj;
        if (!is_closed_edge_group(egrp))
            ERR_RETURN("Not a closed edge group");

        if (num_edges == 0)
        {
            num_edges = egrp->n_members;
            first_egrp = egrp;
        }
        else
        {
            if (egrp->n_members != num_edges)
                ERR_RETURN("Edge groups do not match");

            for 
            (
                e = (Edge *)egrp->obj_list.head, e0 = (Edge *)first_egrp->obj_list.head; 
                e != NULL; 
                e = (Edge *)e->hdr.next, e0 = (Edge *)e0->hdr.next
            )
            {
                if (e->type != e0->type)
                    ERR_RETURN("Edges do not match");
            }
        }
    }

    // Once we have checked everything for errors, clone the main group so it can be retained.
    clone = (Group *)copy_obj((Object*)group, 0, 0, 0, TRUE);
    clear_move_copy_flags((Object*)group);

    // Allocate the LoftedGroup array.
    lg = (LoftedGroup*)malloc(num_groups * sizeof(LoftedGroup));

    // Determine where the edge groups' centroids are.
    // If there is a current path, sort the sections by the centroids' positions along it.
    // If not, determine the bounding box of the centroids and use the principal direction.
    clear_bbox(&box);
    if (curr_path != NULL)
    {
        if (curr_path->type == OBJ_EDGE)
        {
            Edge* e = (Edge*)curr_path;

            expand_bbox(&box, e->endpoints[0]);
            expand_bbox(&box, e->endpoints[1]);
            path_length = length(e->endpoints[0], e->endpoints[1]);
        }
        else
        {
            // Find the first and last points in the edge group making up the path.
            // TODO: To come later.
            ERR_RETURN("Complex path not supported (yet)");
        }
    }

    // Determine centroid and normal of points in each edge group. The centroid is not
    // needed exactly; the midpoint of the edge group's bbox will do.
    //for (i = 0, egrp = (Group*)clone->obj_list.head; egrp != NULL; i++, egrp = (Group*)egrp->hdr.next)
    for (i = 0, egrp = (Group*)group->obj_list.head; egrp != NULL; i++, egrp = (Group*)egrp->hdr.next)
    {
        Edge* e, * next_edge;
        int initial, final;
        Point* pt;
        ListHead plist = { NULL, NULL };
        Bbox ebox;

        clear_bbox(&ebox);
        lg[i].egrp = egrp;

        // Join endpoints up into a list (the next/prev pointers aren't used for
        // anything else)
        e = (Edge*)egrp->obj_list.head;
        next_edge = (Edge*)egrp->obj_list.head->next;
        if (e->endpoints[0] == next_edge->endpoints[0])
        {
            initial = 1;
            pt = e->endpoints[0];
        }
        else if (e->endpoints[0] == next_edge->endpoints[1])
        {
            initial = 1;
            pt = e->endpoints[0];
        }
        else if (e->endpoints[1] == next_edge->endpoints[0])
        {
            initial = 0;
            pt = e->endpoints[1];
        }
        else if (e->endpoints[1] == next_edge->endpoints[1])
        {
            initial = 0;
            pt = e->endpoints[1];
        }
        else
        {
            ASSERT(FALSE, "The edges aren't connected");
            return NULL;
        }

        link_tail((Object*)e->endpoints[initial], &plist);
        expand_bbox(&ebox, e->endpoints[initial]);
        for (; e->hdr.next != NULL; e = next_edge)
        {
            next_edge = (Edge*)e->hdr.next;

            if (e->hdr.next->type != OBJ_EDGE)
                return NULL;

            // Strip construction edges, as we can't consistently create them
            e->type &= ~EDGE_CONSTRUCTION;

            if (next_edge->endpoints[0] == pt)
            {
                link_tail((Object*)next_edge->endpoints[0], &plist);
                expand_bbox(&ebox, next_edge->endpoints[0]);
                final = 1;
            }
            else if (next_edge->endpoints[1] == pt)
            {
                link_tail((Object*)next_edge->endpoints[1], &plist);
                expand_bbox(&ebox, next_edge->endpoints[1]);
                final = 0;
            }
            else
            {
                return NULL;
            }
            pt = next_edge->endpoints[final];
        }

        // Get the normal and see if we need to reverse. Compare it to the normal for the first section.
        polygon_normal((Point*)plist.head, &lg[i].norm);
        if (i > 0 && pldot(&lg[i].norm, &lg[0].norm) < 0)
        {
            // Reverse the edge group in-place.
            lg[i].norm.A = -lg[i].norm.A;
            lg[i].norm.B = -lg[i].norm.B;
            lg[i].norm.C = -lg[i].norm.C;
            reverse(&lg[i].egrp->obj_list);
        }

        // Put the quasi-centroid into the larger bbox, and store it in the normal refpt.
        lg[i].norm.refpt.x = (ebox.xmin + ebox.xmax) / 2;
        lg[i].norm.refpt.y = (ebox.ymin + ebox.ymax) / 2;
        lg[i].norm.refpt.z = (ebox.zmin + ebox.zmax) / 2;
        expand_bbox(&box, &lg[i].norm.refpt);
    }

    // Sort the sections into order. The direction is given by the first section's normal.
    // Put the path (or the bbox principal direction) into that order first.
    if (curr_path != NULL)
    {
        Edge* e = (Edge*)curr_path;

        principal.A = e->endpoints[1]->x - e->endpoints[0]->x;
        principal.B = e->endpoints[1]->y - e->endpoints[0]->y;
        principal.C = e->endpoints[1]->z - e->endpoints[0]->z;
        if (pldot(&principal, &lg[0].norm) > 0)
        {
            principal.refpt = *e->endpoints[0];
            endpt = *e->endpoints[1];
        }
        else
        {
            principal.A = -principal.A;
            principal.B = -principal.B;
            principal.C = -principal.C;
            principal.refpt = *e->endpoints[1];
            endpt = *e->endpoints[0];
        }

        // Sorting distance as fraction of path length
        for (i = 0; i < num_groups; i++)
            lg[i].param = length(&principal.refpt, &lg[i].norm.refpt) / path_length;
    }
    else    // no path, assume (generally) axis-aligned row of centroids in the box
    {
        float dx = box.xmax - box.xmin;
        float dy = box.ymax - box.ymin;
        float dz = box.zmax - box.zmin;

        if (dx > dy && dx > dz)
        {
            // Principal direction is the x-direction.
            if (lg[0].norm.A > 0)
            {
                principal.A = dx;
                principal.refpt.x = box.xmin;
                endpt.x = box.xmax;
            }
            else  // it's the negative X direction
            {
                principal.A = -dx;
                principal.refpt.x = box.xmax;
                endpt.x = box.xmin;
            }
            principal.B = 0;
            principal.C = 0;
            endpt.y = principal.refpt.y = (box.ymax + box.ymin) / 2;
            endpt.z = principal.refpt.z = (box.zmax + box.zmin) / 2;

            // Express sorting dist as fraction of dx in order to get sign right
            for (i = 0; i < num_groups; i++)
                lg[i].param = (lg[i].norm.refpt.x - principal.refpt.x) / dx;
        }
        else if (dy > dx && dy > dz)
        {
            // The y-direction
            if (lg[0].norm.B > 0)
            {
                principal.B = dy;
                principal.refpt.y = box.ymin;
                endpt.y = box.ymax;
            }
            else  // it's the negative X direction
            {
                principal.B = -dy;
                principal.refpt.y = box.ymax;
                endpt.y = box.ymin;
            }
            principal.A = 0;
            principal.C = 0;
            endpt.x = principal.refpt.x = (box.xmax + box.xmin) / 2;
            endpt.z = principal.refpt.z = (box.zmax + box.zmin) / 2;

            for (i = 0; i < num_groups; i++)
                lg[i].param = (lg[i].norm.refpt.y - principal.refpt.y) / dy;
        }
        else
        {
            // The z-direction
            if (lg[0].norm.C > 0)
            {
                principal.C = dz;
                principal.refpt.z = box.zmin;
                endpt.z = box.zmax;
            }
            else  // it's the negative X direction
            {
                principal.C = -dz;
                principal.refpt.z = box.zmax;
                endpt.z = box.zmin;
            }
            principal.A = 0;
            principal.B = 0;
            endpt.x = principal.refpt.x = (box.xmax + box.xmin) / 2;
            endpt.y = principal.refpt.y = (box.ymax + box.ymin) / 2;

            for (i = 0; i < num_groups; i++)
                lg[i].param = (lg[i].norm.refpt.z - principal.refpt.z) / dz;
        }
    }

    // Sort by distance from the principal refpt.
    qsort(lg, num_groups, sizeof(LoftedGroup), compare_lofted_groups);


    // Choose a point in the first section as the principal point. Find the corresponding
    // point in each section by finding the closest point to a plane defined by the path
    // (or principal axis) and the principal point.
    first_edge = (Edge *)lg[0].egrp->obj_list.head;
    first_pt = first_point(first_edge);
    normal3(first_pt, &principal.refpt, &endpt, &pl);

    for (i = 1; i < num_groups; i++)
    {
        Edge* e;
        float min_dist = 999999.0f;
        Edge* min_edge = NULL;  // shhh compiler

        for (e = (Edge *)lg[i].egrp->obj_list.head; e != NULL; e = (Edge *)e->hdr.next)
        {
            float dist = fabsf(distance_point_plane(&pl, first_point(e)));

            if (dist < min_dist)
            {
                min_dist = dist;
                min_edge = e;
            }
        }
        
        // Rotate the edge group into alignment with min_edge up first.
        rotate(&lg[i].egrp->obj_list, min_edge);
    }

    // Join corresponding points with bezier edges.


    // If the path exists and extends beyond the first/last section, the endcap is curved
    // to (approximately!) pass through the endpoint of the path. Otherwise it is flat.





    return NULL; // TEMP until we get something in here
}

// Given an input edge group, clone it along the current path (which may be an open edge group
// containing curved edges) and loft the resulting sections. The input edge group is retained.
Volume *
make_tubed_volume(Group* group)
{




    return NULL; // TEMP until we get something in here
}
