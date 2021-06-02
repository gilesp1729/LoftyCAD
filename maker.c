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

// Check if an edge group is closed. Do this by matching up endpoints.
// Take special care with groups having only one or two elements.
BOOL
is_closed_edge_group(Group* group)
{
    Edge* first, * last;

    if (!is_edge_group(group))
        return FALSE;

    first = (Edge*)group->obj_list.head;
    last = (Edge*)group->obj_list.tail;
    if (first == last)              // one element, it's always open
        return FALSE;
    else if ((Edge *)first->hdr.next == last)   // two elements
    {
        if (first->endpoints[0] == last->endpoints[0] && first->endpoints[1] == last->endpoints[1])
            return TRUE;
        if (first->endpoints[0] == last->endpoints[1] && first->endpoints[1] == last->endpoints[0])
            return TRUE;
    }
    else
    {
        // Otherwise, just check that there's a match between any endpoints
        // of the head and tail edges.
        if (first->endpoints[0] == last->endpoints[0])
            return TRUE;
        if (first->endpoints[0] == last->endpoints[1])
            return TRUE;
        if (first->endpoints[1] == last->endpoints[0])
            return TRUE;
        if (first->endpoints[1] == last->endpoints[1])
            return TRUE;
    }
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
        e->endpoints[initial] = point_newp(pt);
}

// Make a face object out of a closed group of connected edges.
// If requested, the original group is cleared and should be deleted by the caller.
// Reversing can be automatic (test against facing plane) or user controlled.
Face*
make_face(Group* group, BOOL clear_group, BOOL auto_reverse, BOOL reverse)
{
    Face* face = NULL;
    Edge* e, * next_edge, * prev_edge;
    Plane norm;
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
    // Automatic reversing is done by testing against the facing plane.
    polygon_normal((Point*)plist.head, &norm);
    if (auto_reverse)
        reverse = dot(norm.A, norm.B, norm.C, facing_plane->A, facing_plane->B, facing_plane->C) < 0;

    if (reverse)
    {
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
                // Set step counts to the maximum of the two.
                n_steps = e->nsteps;
                if (o->nsteps > n_steps)
                    n_steps = o->nsteps;
                e->nsteps = n_steps;
                o->nsteps = n_steps;
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
            if (clear_group)
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
            if (clear_group)
                delink_group((Object*)e, group);
        }
    }

    face->view_valid = FALSE;

    // Group must be cleared out by now (delete will happen later)
    // Update the view list. But if retaining the group, don't do
    // anything else (it will be done later)
    if (clear_group)
    {
        ASSERT(group->obj_list.head == NULL, "Edge group is not empty");

        // Finally, update the view list for the face
        gen_view_list_face(face);
    }

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


// Struct describing an edge group, its normal and centroid, and distance along the
// path used for sorting.
typedef struct
{
    Group* egrp;        // Edge group at this section
    Plane norm;         // Normal of the edge group
    float param;        // How far (as a fraction) along the path or principal direction it is
    Plane principal;    // The plane normal to the path at this section
    Bbox  ebox;         // BBox for the edge group
    ListHead face_list; // List of faces making up the endcap (for nose and tail edge groups only)
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

// Helper to make faces for an endcap edge group and attach them to a volume.
// Allows reversing for the nose. Return FALSE if there is a problem,
// such as mismatching point counts.
BOOL
make_endcap_faces(LoftedGroup* lg, Volume* vol, BOOL reverse)
{
    Plane symmetry;

    // Compute the plane of symmetry, tangent to the egrp's principal direction,
    // and going through its quasi-centroid.
    plcross(&lg->principal, up_direction, &symmetry);
    symmetry.refpt = lg->norm.refpt;

    // Run around the egrp looking for points on either side of the plane of symmetry.
    // Make sure there are an equal number of them on each side.



    // Find an edge in the group that crosses the plane of symmetry
    dist = distance_point_plane(&symmetry, the_point);





}



extern LoftParams default_loft;

// Make a lofted volume from a group of sections, represented as edge groups.
// There may or may not be a current path. 
// The input group is retained to allow editing and re-lofting.
Volume *
make_lofted_volume(Group* group)
{
    Group* clone, * egrp, * first_egrp = NULL;
    Edge *e, *prev_e;
    Face* face;
    Volume* vol, *new_vol;
    Object* obj;
    Plane principal;
    Point *pt;
    int num_groups, num_edges;
    Bbox box;
    int i, j;
    LoftedGroup* lg;
    LoftParams* loft;
    float total_length;
    ListHead* contour_lists;
    Edge** contour;
    int *band_nsteps;

    // Some sanity checks on the input. Some will be enforced outside.
    ASSERT(group->hdr.type == OBJ_GROUP, "Must be a group");

    // Ensure the edge groups all have the same number and type of edges.
    // There can only be 4, 6 or 8 of them for now. Also, there must be at 
    // least 2 edge groups.
    num_groups = group->n_members;
    vol = NULL;
    if (group->obj_list.tail->type == OBJ_VOLUME)
    {
        vol = (Volume*)group->obj_list.tail;
        num_groups--;
    }
    if (num_groups < 2)
        ERR_RETURN("There must be at least 2 edge groups");

    // Count and check all the edge groups. Stop when any attached volume is reached.
    num_edges = 0;
    for (obj = group->obj_list.head; obj != NULL && obj != (Object *)vol; obj = obj->next)
    {
        egrp = (Group*)obj;
        if (!is_closed_edge_group(egrp))
            ERR_RETURN("Not a closed edge group");

        if (num_edges == 0)
        {
            num_edges = egrp->n_members;
            if (num_edges < 4)
                ERR_RETURN("Edge groups must have at least 4 edges");
            first_egrp = egrp;
        }
        else
        {
            // Only check the count here. Check edge types match after groups are 
            // (potentially) reversed and rotated into alignment.
            if (egrp->n_members != num_edges)
                ERR_RETURN("Edge groups do not match");
        }
    }

    // Allocate this first so we can be sure its pointer has changed from the old one
    new_vol = vol_new();

    // Once we have checked everything for errors:
    // - remove any existing lofted volume from the group, and
    // - clone the main group so it can be retained. The new volume gets separate edges.
    if (vol != NULL)
    {
        delink_group((Object*)vol, group);
        purge_obj((Object*)vol);
    }
    vol = new_vol;
    vol->hdr.lock = LOCK_FACES;
    clone = (Group *)copy_obj((Object*)group, 0, 0, 0, TRUE);
    clear_move_copy_flags((Object*)group);

    // Ensure there is a valid LoftParams
    if (group->loft == NULL)
    {
        // Set it to defaults
        int n_bays = group->n_members - 1;

        group->loft = malloc(sizeof(LoftParams) + n_bays * sizeof(float));
        memcpy(group->loft, &default_loft, sizeof(LoftParams));
        group->loft->n_bays = n_bays;
        for (i = 1; i < group->loft->n_bays; i++)
            group->loft->bay_tensions[i] = group->loft->bay_tensions[0];
    }
    loft = group->loft;

    // Allocate the LoftedGroup array, and other variable length arrays.
    lg = (LoftedGroup*)calloc(num_groups, sizeof(LoftedGroup));
    contour_lists = (ListHead*)calloc(num_edges, sizeof(ListHead));
    contour = (Edge**)calloc(num_edges, sizeof(Edge*));
    band_nsteps = (int*)calloc(num_edges, sizeof(int*));

    // Determine centroid and normal of points in each edge group. The centroid is not
    // needed exactly; the midpoint of the edge group's bbox will do.
    clear_bbox(&box);
    for (i = 0, egrp = (Group*)clone->obj_list.head; egrp != NULL; i++, egrp = (Group*)egrp->hdr.next)
    {
        Edge* next_edge;
        int initial, final;
        ListHead plist = { NULL, NULL };

        clear_bbox(&lg[i].ebox);
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
        expand_bbox(&lg[i].ebox, e->endpoints[initial]);
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
                expand_bbox(&lg[i].ebox, next_edge->endpoints[0]);
                final = 1;
            }
            else if (next_edge->endpoints[1] == pt)
            {
                link_tail((Object*)next_edge->endpoints[1], &plist);
                expand_bbox(&lg[i].ebox, next_edge->endpoints[1]);
                final = 0;
            }
            else
            {
                return NULL;
            }
            pt = next_edge->endpoints[final];
        }

        polygon_normal((Point*)plist.head, &lg[i].norm);

        // Put the quasi-centroid into the larger bbox, and store it in the normal refpt.
        lg[i].norm.refpt.x = (lg[i].ebox.xmin + lg[i].ebox.xmax) / 2;
        lg[i].norm.refpt.y = (lg[i].ebox.ymin + lg[i].ebox.ymax) / 2;
        lg[i].norm.refpt.z = (lg[i].ebox.zmin + lg[i].ebox.zmax) / 2;
        expand_bbox(&box, &lg[i].norm.refpt);
    }

    // Sort the sections into order and reverse if needed. 
    // The direction is given by the principal direction.
    if (curr_path != NULL)
    {
        float len;

        total_length = path_total_length(curr_path);
        for (i = 0; i < num_groups; i++)
        {
            // Determine direction of path where it passes through lg[i]
            if (path_tangent_to_intersect(curr_path, &lg[i].norm, &lg[i].ebox, &lg[i].principal, &len))
            {
                if (pldot(&lg[i].norm, &lg[i].principal) < 0)
                {
                    // Reverse the edge group in-place if it goes the opposite
                    // way to the principal direction.
                    lg[i].norm.A = -lg[i].norm.A;
                    lg[i].norm.B = -lg[i].norm.B;
                    lg[i].norm.C = -lg[i].norm.C;
                    reverse(&lg[i].egrp->obj_list);
                }
                // Sorting distance as fraction of path length
                lg[i].param = len / total_length;
            }
            else
            {
                // No intersection was found. This is not what the user intended,
                // so we error out.
                purge_obj((Object*)vol);
                purge_obj((Object*)clone);
                free(lg);
                free(contour_lists);
                free(contour);
                free(band_nsteps);
                ERR_RETURN("Not all edge groups intersect the path");
            }
        }
    }
    else    // no path, assume (generally) axis-aligned row of centroids in the box
    {
        float dx = box.xmax - box.xmin;
        float dy = box.ymax - box.ymin;
        float dz = box.zmax - box.zmin;

        if (dx > dy && dx > dz)
        {
            // Principal direction is the x-direction.
            // Use the negative so the nose points up the +ve x axis.
            principal.A = -dx;
            principal.B = 0;
            principal.C = 0;

            // Express sorting dist as fraction of dx in order to get sign right
            for (i = 0; i < num_groups; i++)
            {
                if (pldot(&lg[i].norm, &principal) < 0)
                {
                    // Reverse the edge group in-place.
                    lg[i].norm.A = -lg[i].norm.A;
                    lg[i].norm.B = -lg[i].norm.B;
                    lg[i].norm.C = -lg[i].norm.C;
                    reverse(&lg[i].egrp->obj_list);
                }
                lg[i].param = (box.xmax - lg[i].norm.refpt.x) / dx;
                lg[i].principal = principal;
                total_length = box.xmax - box.xmin;
            }
        }
        else if (dy > dx && dy > dz)
        {
            // The y-direction
            principal.B = -dy;
            principal.A = 0;
            principal.C = 0;

            for (i = 0; i < num_groups; i++)
            {
                if (pldot(&lg[i].norm, &principal) < 0)
                {
                    // Reverse the edge group in-place.
                    lg[i].norm.A = -lg[i].norm.A;
                    lg[i].norm.B = -lg[i].norm.B;
                    lg[i].norm.C = -lg[i].norm.C;
                    reverse(&lg[i].egrp->obj_list);
                }
                lg[i].param = (box.ymax - lg[i].norm.refpt.y) / dy;
                lg[i].principal = principal;
                total_length = box.ymax - box.ymin;
            }
        }
        else
        {
            // The z-direction
            principal.C = -dz;
            principal.A = 0;
            principal.B = 0;

            for (i = 0; i < num_groups; i++)
            {
                if (pldot(&lg[i].norm, &principal) < 0)
                {
                    // Reverse the edge group in-place.
                    lg[i].norm.A = -lg[i].norm.A;
                    lg[i].norm.B = -lg[i].norm.B;
                    lg[i].norm.C = -lg[i].norm.C;
                    reverse(&lg[i].egrp->obj_list);
                }
                lg[i].param = (box.zmax - lg[i].norm.refpt.z) / dz;
                lg[i].principal = principal;
                total_length = box.zmax - box.zmin;
            }
        }
    }

    // Sort by distance (lg.param) along the axis or the path.
    qsort(lg, num_groups, sizeof(LoftedGroup), compare_lofted_groups);

    // Choose an edge in the first section as the key edge. If there is a plane of symmetry,
    // find the uppermost edge (in the defined up-direction) that crosses the plane. Rotate the
    // first section so the key edge is at the head of the edge group.








 
    // Find the corresponding edge in each section. Various methods have been tried, and 
    // robustness is an issue. Use an angular test summed across all the edges.

    // Accumulate the direction cosines of the first edge group.
    for (e = (Edge*)lg[0].egrp->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
    {
        int c = first_point_index(e);

        e->dirn.A = e->endpoints[1 - c]->x - e->endpoints[c]->x;
        e->dirn.B = e->endpoints[1 - c]->y - e->endpoints[c]->y;
        e->dirn.C = e->endpoints[1 - c]->z - e->endpoints[c]->z;
        normalise_plane((Plane*)&e->dirn);
    }

    // Compare directions of edges in each edge group with the previous.
    for (i = 1; i < num_groups; i++)
    {
        float angle = 0;
        float min_angle = 999999.0f;
        Edge* min_edge = NULL;  // shhh compiler

        // Accumulate the direction cosines of the current edge group.
        for (e = (Edge*)lg[i].egrp->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            int c = first_point_index(e);

            e->dirn.A = e->endpoints[1 - c]->x - e->endpoints[c]->x;
            e->dirn.B = e->endpoints[1 - c]->y - e->endpoints[c]->y;
            e->dirn.C = e->endpoints[1 - c]->z - e->endpoints[c]->z;
            normalise_plane((Plane*)&e->dirn);
        }

        // Try all the rotations of the current edge group.
        for (j = 0; j < num_edges; j++)
        {
            Edge* e0, * e1;
            float angle = 0;
            float cosa;

            // Walk down the current and previous EG's summing the angles between the
            // corresponding edges. But do not consider rotations where the
            // edge types do not match.
            for
            (
                e0 = (Edge*)lg[i - 1].egrp->obj_list.head, e1 = (Edge*)lg[i].egrp->obj_list.head;
                e1 != NULL;
                e0 = (Edge*)e0->hdr.next, e1 = (Edge*)e1->hdr.next
            )
            {
                if (e0->type != e1->type)
                    goto next_rotation;
                cosa = pldot((Plane*)&e1->dirn, (Plane*)&e0->dirn);
                if (cosa > 1)   // protect against domain error in acosf
                    cosa = 1;
                angle += acosf(cosa);
            }

            if (angle < min_angle)
            {
                min_angle = angle;
                min_edge = (Edge*)lg[i].egrp->obj_list.head;
            }

        next_rotation:
            // Go to the next rotation.
            rotate(&lg[i].egrp->obj_list, lg[i].egrp->obj_list.head->next);
        }
        
        // Rotate the edge group into alignment with min_edge up first.
        // If there were no valid rotations, bail out. Edge types cannot be made to match.
        if (min_edge == NULL)
        {
            purge_obj((Object*)vol);
            purge_obj((Object*)clone);
            free(lg);
            free(contour_lists);
            free(contour);
            free(band_nsteps);
            ERR_RETURN("Edge types in groups don't match");
        }
        rotate(&lg[i].egrp->obj_list, min_edge);

        // While here, if follow_path is TRUE, calculate the bay tensions for each bay.
        if (loft->follow_path)
        {
            Plane pl0 = lg[i - 1].principal;
            Plane pl1 = lg[i].principal;
            float theta, chord;

            normalise_plane(&pl0);
            normalise_plane(&pl1);
            theta = acosf(pldot(&pl0, &pl1));

            // The usual (4/3) tan (theta/4) formula for tension is as a fraction
            // of the radius, but our tensions are as multiples of the chord
            // length (distance between endpoints). Correct this to get
            // a better approximation. Watch out for near-zero divides, though.
            // As theta -> 0, the tension -> 0.33333
            chord = 2.0f * sinf(theta / 2.0f);
            if (fabsf(theta) < 0.05)
                loft->bay_tensions[i - 1] = BEZ_DEFAULT_TENSION;
            else
                loft->bay_tensions[i - 1] = (4.0f / 3.0f) * tanf(theta / 4.0f) / chord;
        }
    }

    // Assign section edges to bands (a generally horizontal strip of faces on both sides
    // of the volume that must have matching step counts). The key edge, its opposite number,
    // and any internal edges created later in the endcap faces, all have band 0.

    // Accumulate the max step count for each edge in each band.
    for (i = 0; i < num_groups; i++)
    {
        Edge* ne, * pe;

        egrp = lg[i].egrp;
        e = (Edge*)egrp->obj_list.head;
        e->band = 0;
        ne = e->hdr.next;
        pe = (Edge*)egrp->obj_list.tail;
        for (j = 1; j < num_edges; j++)  // this loop will terminate early
        {
            ne->band = pe->band = j;
            if (ne->nsteps > band_nsteps[j])
                band_nsteps[j] = ne->nsteps;
            if (pe->nsteps > band_nsteps[j])
                band_nsteps[j] = pe->nsteps;

            // Move to next band. We should meet at the bottom.
            ne = ne->hdr.next;
            pe = pe->hdr.prev;
            ASSERT(ne != NULL && pe != NULL, "Edge group must have an even number of points");
            if (ne == pe)
            {
                ne->band = 0;
                break;
            }
        }
    }






#if 0
    // Ensure corresponding edges in the sections have matching step counts. 
    for (i = 0; i < num_groups; i++)
    {
        egrp = lg[i].egrp;
        for (j = 0, e = (Edge*)egrp->obj_list.head; e != NULL; j++, e = (Edge*)e->hdr.next)
        {
            if (e->nsteps > sect_nsteps[j])
                sect_nsteps[j] = e->nsteps;
        }
    }

#define MAX(a, b)   ((a) > (b) ? (a) : (b))
    if (num_edges == 4 || (num_edges & 1))
    {
        if (num_edges == 4)
        {
            // If the endcap is going to be a single bezier face:
            // Check that opposing edges have the same step count, as they will meet up in the endcap.
            // We don't care about the plane of symmetry in this case.
            sect_nsteps[0] = sect_nsteps[2] = MAX(sect_nsteps[0], sect_nsteps[2]);
            sect_nsteps[1] = sect_nsteps[3] = MAX(sect_nsteps[1], sect_nsteps[3]);
        }

        // If a single bezier face or an odd-numbered (flat) face:
        // Create the faces for the endcaps. Just use the standard make_face call.
        // The nose face gets reversed. Don't link them into the volume yet,
        // as we need themn to be linked into the lg entry's list now.
        face = make_face(lg[0].egrp, FALSE, FALSE, TRUE);
        face->vol = vol;
        if (face->type > vol->max_facetype)
            vol->max_facetype = face->type;
        link_tail((Object*)face, &lg[0].face_list);

        face = make_face(lg[num_groups - 1].egrp, FALSE, FALSE, FALSE);
        face->vol = vol;
        if (face->type > vol->max_facetype)
            vol->max_facetype = face->type;
        link_tail((Object*)face, &lg[num_groups - 1].face_list);
    }
    else
    {
        // TODO: Decide based on plane of symmetry, which points are on each side
        // Join up points and make faces in each endcap, and make sure all step counts are tickety-boo.
        // Accumulate faces in list attached to lg[0] and lg[num_groups-1], reverse normals
        // for those in lg[0]
        make_endcap_faces(&lg[0], vol, sect_nsteps, TRUE);
        make_endcap_faces(&lg[num_groups - 1], vol, sect_nsteps, FALSE);
    }

    // Set edges to have the correct max step counts accumulated above.
    // TODO: take care of internal edges when n < 4
    for (i = 0; i < num_groups; i++)
    {
        egrp = lg[i].egrp;
        for (j = 0, e = (Edge*)egrp->obj_list.head; e != NULL; j++, e = (Edge*)e->hdr.next)
        {
            e->nsteps = sect_nsteps[j];
            e->view_valid = FALSE;
        }
    }
#endif

    // Join corresponding points with bezier edges. Gather the edges into contour lists.
    for (i = 1; i < num_groups; i++)
    {
        int steps = (int)((lg[i].param - lg[i - 1].param) * total_length / default_stepsize + 1);

        for
        (
            j = 0, e = (Edge*)lg[i].egrp->obj_list.head, prev_e = (Edge*)lg[i - 1].egrp->obj_list.head;
            e != NULL;
            j++, e = (Edge*)e->hdr.next, prev_e = (Edge*)prev_e->hdr.next
        )
        {
            int ei = first_point_index(e);
            int prev_ei = first_point_index(prev_e);
            Edge* ne;

            // Add the edge to its list.
            ne = edge_new(EDGE_BEZIER);
            ne->endpoints[0] = prev_e->endpoints[prev_ei];
            ne->endpoints[1] = e->endpoints[ei];
            ((BezierEdge*)ne)->ctrlpoints[0] = point_newp(ne->endpoints[0]);
            ((BezierEdge*)ne)->ctrlpoints[1] = point_newp(ne->endpoints[1]);

            // Setup step count
            ne->nsteps = steps;
            ne->view_valid = FALSE;

            link_tail((Object*)ne, &contour_lists[j]);
        }
    }

    // Points to head of each contour list
    for (j = 0; j < num_edges; j++)
        contour[j] = (Edge*)contour_lists[j].head;

    for (i = 1; i < num_groups; i++)
    {
        // Now that all the contour edges have been created, join the edges up into faces.
        // Since the edge groups are closed, the last face joins back to the head edge.
        // Also calculate the control points of the contour edges.
        for
        (
            j = 0, e = (Edge*)lg[i].egrp->obj_list.head, prev_e = (Edge*)lg[i - 1].egrp->obj_list.head;
            e != NULL;
            j++, e = (Edge*)e->hdr.next, prev_e = (Edge*)prev_e->hdr.next
        )
        {
            int ftype, ci, cn;
            Plane dummy = { 0, };
            Plane plprev, plnext;
            Edge* c;
            float lj;
            BOOL angle_break, join_smooth;

            switch (e->type)
            {
            case EDGE_STRAIGHT:
                ftype = FACE_CYLINDRICAL;
                break;
            case EDGE_ARC:
                ftype = FACE_BARREL;
                break;
            case EDGE_BEZIER:
                ftype = FACE_BEZIER;
                break;
            }

            face = face_new(ftype, dummy);
            face->n_edges = 4;
            face->edges[0] = prev_e;
            if (j < num_edges - 1)
                face->edges[1] = contour[j+1];
            else
                face->edges[1] = contour[0];
            face->edges[2] = e;
            face->edges[3] = contour[j];
            face->initial_point = first_point(prev_e);
            face->vol = vol;
            if (ftype > vol->max_facetype)
                vol->max_facetype = ftype;
            link_tail((Object *)face, &vol->faces);

            // Calculate the positions of the contours' control points.
            // Take care at the end of the list.
            if (contour[j]->hdr.next != NULL)
            {
                ci = edge_direction(contour[j], &plprev);
                cn = edge_direction((Edge *)contour[j]->hdr.next, &plnext);
                angle_break = pldot(&plprev, &plnext) < cosf(loft->body_angle_break / RADF);
                join_smooth = FALSE;
                if (loft->follow_path)
                {
                    // Follow curve of path. The bay tensions have been calculated in advance. (TODO!)
                    plnext = lg[i].principal;
                    normalise_plane(&plnext);
                    join_smooth = TRUE;
                }
                else if (!angle_break)
                {
                    // Average the directions to prev and next endpoints, and use that for the edges on both sides.
                    // (i.e. ctrl[1-ci] of curr, ctrl[cn] of next)
                    plnext.A = (plprev.A + plnext.A) / 2;
                    plnext.B = (plprev.B + plnext.B) / 2;
                    plnext.C = (plprev.C + plnext.C) / 2;
                    join_smooth = TRUE;
                }
                if (join_smooth)
                {
                    c = contour[j];
                    lj = length(c->endpoints[0], c->endpoints[1]);
                    ((BezierEdge*)c)->ctrlpoints[1-ci]->x = c->endpoints[1-ci]->x - (plnext.A * loft->bay_tensions[i - 1] * lj);
                    ((BezierEdge*)c)->ctrlpoints[1-ci]->y = c->endpoints[1-ci]->y - (plnext.B * loft->bay_tensions[i - 1] * lj);
                    ((BezierEdge*)c)->ctrlpoints[1-ci]->z = c->endpoints[1-ci]->z - (plnext.C * loft->bay_tensions[i - 1] * lj);

                    c = (Edge*)contour[j]->hdr.next;
                    lj = length(c->endpoints[0], c->endpoints[1]);
                    ((BezierEdge*)c)->ctrlpoints[cn]->x = c->endpoints[cn]->x + (plnext.A * loft->bay_tensions[i] * lj);
                    ((BezierEdge*)c)->ctrlpoints[cn]->y = c->endpoints[cn]->y + (plnext.B * loft->bay_tensions[i] * lj);
                    ((BezierEdge*)c)->ctrlpoints[cn]->z = c->endpoints[cn]->z + (plnext.C * loft->bay_tensions[i] * lj);
                }
            }
        }

        // Advance to next contour
        for (j = 0; j < num_edges; j++)
            contour[j] = (Edge*)contour[j]->hdr.next;
    }

    // Form the endcaps. User has a choice of angle-break or smooth at the ends of the contours.
    // The angle used for testing the angle-break criterion at the end of the contour is the smallest of:
    // - the angle to the centroid of the end edge group,
    // - the angles to the two endcap edges incident on the common point,
    // projected onto a plane perpendicular to its principal direction (to be robust when the endcap
    // is not perpendicular). Decide which endcap edge, if any, the contour blends into.

    // Nose endcap (at start of group)
    for (j = 0; j < num_edges; j++)
    {
        Plane pl, plend, pltest, pl_proj, plend_proj, pltest_proj;
        Edge* c;
        int ci;
        float cosmax, costest, lj;

        c = (Edge *)contour_lists[j].head;
        ci = edge_direction(c, &pl);                // points into contour
        project(&pl, &lg[0].principal, &pl_proj);         // project onto principal plane
        point_direction(&lg[0].norm.refpt, c->endpoints[ci], &plend);
        project(&plend, &lg[0].principal, &plend_proj);
        cosmax = pldot(&plend_proj, &pl_proj);
        
        if (loft->nose_join_mode != JOIN_NOSECONE)
        {
            // Check the edges incident on the common point (c->endpoints[ci]) and see if
            // they make a smaller angle (greater dot product). If so, use that for the test.
            // Go through generated faces here not just edge group points.
            for (face = (Face *)lg[0].face_list.head; face != NULL; face = (Face *)face->hdr.next)
            {
                for (i = 0; i < face->n_edges; i++)
                {
                    e = face->edges[i];
                    // Be careful with the directions
                    if (e->endpoints[0] == c->endpoints[ci])
                        point_direction(e->endpoints[1], e->endpoints[0], &pltest);
                    else if (e->endpoints[1] == c->endpoints[ci])
                        point_direction(e->endpoints[0], e->endpoints[1], &pltest);
                    else
                        continue;

                    project(&pltest, &lg[0].principal, &pltest_proj);
                    costest = pldot(&pltest_proj, &pl_proj);
                    if (costest > cosmax)
                    {
                        cosmax = costest;
                        plend = pltest;
                    }
                }
            }
        }

        if (cosmax > cosf(loft->nose_angle_break / RADF) && loft->nose_join_mode != JOIN_STERN)
        {
            // make the first ctrl point straight into the end group's plane (don't average them)
            lj = length(c->endpoints[0], c->endpoints[1]);
            ((BezierEdge*)c)->ctrlpoints[ci]->x = c->endpoints[ci]->x + (plend.A * loft->nose_tension * lj);
            ((BezierEdge*)c)->ctrlpoints[ci]->y = c->endpoints[ci]->y + (plend.B * loft->nose_tension * lj);
            ((BezierEdge*)c)->ctrlpoints[ci]->z = c->endpoints[ci]->z + (plend.C * loft->nose_tension * lj);
        }
    }

    // Link the face(s) into the volume now that they have been joined up in the nose.
    // The volume's face list will not be empty, so just append the face list to it.
    vol->faces.tail->next = lg[0].face_list.head;
    vol->faces.tail = lg[0].face_list.tail;

    // Tail endcap (at end of group)
    for (j = 0; j < num_edges; j++)
    {
        Plane pl, plend, pltest, pl_proj, plend_proj, pltest_proj;
        Edge* c;
        int ci;
        float cosmax, costest, lj;

        c = (Edge*)contour_lists[j].tail;
        ci = edge_direction(c, &pl);                // points out of contour
        project(&pl, &lg[num_groups - 1].principal, &pl_proj);         // project onto principal plane
        point_direction(c->endpoints[1-ci], &lg[num_groups-1].norm.refpt, &plend);
        project(&plend, &lg[num_groups - 1].principal, &plend_proj);
        cosmax = pldot(&plend_proj, &pl_proj);

        if (loft->tail_join_mode != JOIN_NOSECONE)
        {
            // Check the edges incident on the common point (c->endpoints[1-ci]) and see if
            // they make a smaller angle (greater dot product). If so, use that for the test.
            for (face = (Face *)lg[num_groups - 1].face_list.head; face != NULL; face = (Face *)face->hdr.next)
            {
                for (i = 0; i < face->n_edges; i++)
                {
                    e = face->edges[i];
                    // Be careful with the directions
                    if (e->endpoints[0] == c->endpoints[1 - ci])
                        point_direction(e->endpoints[0], e->endpoints[1], &pltest);
                    else if (e->endpoints[1] == c->endpoints[1 - ci])
                        point_direction(e->endpoints[1], e->endpoints[0], &pltest);
                    else
                        continue;

                    project(&pltest, &lg[num_groups - 1].principal, &pltest_proj);
                    costest = pldot(&pltest_proj, &pl_proj);
                    if (costest > cosmax)
                    {
                        cosmax = costest;
                        plend = pltest;
                    }
                }
            }
        }

        if (cosmax > cosf(loft->tail_angle_break / RADF) && loft->tail_join_mode != JOIN_STERN)
        {
            // make the first ctrl point straight into the end group's plane (don't average them)
            lj = length(c->endpoints[0], c->endpoints[1]);
            ((BezierEdge*)c)->ctrlpoints[1-ci]->x = c->endpoints[1-ci]->x - (plend.A * loft->tail_tension * lj);
            ((BezierEdge*)c)->ctrlpoints[1-ci]->y = c->endpoints[1-ci]->y - (plend.B * loft->tail_tension * lj);
            ((BezierEdge*)c)->ctrlpoints[1-ci]->z = c->endpoints[1-ci]->z - (plend.C * loft->tail_tension * lj);
        }
    }

    // Link the face(s) into the volume now that they have been joined up in the tail.
    vol->faces.tail->next = lg[num_groups - 1].face_list.head;
    vol->faces.tail = lg[num_groups - 1].face_list.tail;

    // Clean up by deleting the clone and its edge groups. 
    // Clear the edges out first. 
    for (obj = clone->obj_list.head; obj != NULL; obj = obj->next)
    {
        Object* o, * onext;

        egrp = (Group*)obj;
        for (o = egrp->obj_list.head; o != NULL; o = onext)
        {
            onext = o->next;
            delink_group(o, egrp);
        }
    }
    purge_obj((Object *)clone);
    free(lg);
    free(contour_lists);
    free(contour);
    free(band_nsteps);

    return vol;
}

// Given an input edge group, clone it along the current path (which may be an open edge group
// containing curved edges) and loft the resulting sections. The input edge group is retained.
Volume *
make_tubed_volume(Group* group)
{




    return NULL; // TEMP until we get something in here
}
