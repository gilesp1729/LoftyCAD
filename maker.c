#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Routines to build up faces and volumes.

// Search for a closed chain of connected edges (having coincident endpoints within tol)
// and if found, make a group out of them. If the chain is closed, the group is locked
// at the edge level; if the chain is found but open, the group is locked at point level.
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
                end1.edge = e;
                end1.which_end = 1;
                n_edges++;
                advanced = TRUE;
            }

            else if (near_pt(e->endpoints[1], end1.edge->endpoints[end1.which_end], snap_tol))
            {
                delink_group(obj, &object_tree);
                link_tail_group(obj, group);
                end1.edge = e;
                end1.which_end = 0;
                n_edges++;
                advanced = TRUE;
            }

            if (near_pt(end0.edge->endpoints[end0.which_end], end1.edge->endpoints[end1.which_end], snap_tol))
            {
                // We have closed the chain. Mark the group as a closed edge group by
                // setting its lock to Edges (it's hacky, but the lock is written to the
                // file, and it's not used for anything else)
                group->hdr.lock = LOCK_EDGES;
                return group;
            }
        }

        // Every pass should advance at least one of the ends. If we can't close,
        // return the edges unchanged to the object tree and return NULL.
        if (!advanced)
            break;
    }

    // If only the first edge has been grouped, return it to the object tree,
    // discard the group and return NULL.
    if (end0.edge == end1.edge)
    {
        delink_group((Object*)end0.edge, group);
        link_tail_group((Object*)end0.edge, &object_tree);
        purge_obj((Object*)group);
    }

    // Return an open edge group.
    group->hdr.lock = LOCK_POINTS;
    return group;

#if 0
    // We have not been able to close the chain. Ungroup everything we've grouped
    // so far and return.
    for (obj = group->obj_list.head; obj != NULL; obj = nextobj)
    {
        nextobj = obj->next;
        delink_group(obj, group);
        link_tail_group(obj, &object_tree);
    }
    purge_obj((Object*)group);

    return NULL;
#endif

}

// Make a face object out of a closed group of connected edges, sharing points as we go.
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

    // Check that the group is locked at Edges (made by group-connected_edges)
    // meaning that it is closed.
    if (group->hdr.lock != LOCK_EDGES)
        return NULL;

    // Determine normal of points gathered up so far. From this we decide
    // which order to build the final edge array on the face. Join the
    // endpoints up into a list (the next/prev pointers aren't used for
    // anything else)
    e = (Edge*)group->obj_list.head;
    next_edge = (Edge*)group->obj_list.head->next;
    if (near_pt(e->endpoints[0], next_edge->endpoints[0], snap_tol))
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (near_pt(e->endpoints[0], next_edge->endpoints[1], snap_tol))
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (near_pt(e->endpoints[1], next_edge->endpoints[0], snap_tol))
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else if (near_pt(e->endpoints[1], next_edge->endpoints[1], snap_tol))
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

        if (near_pt(next_edge->endpoints[0], pt, snap_tol))
        {
            next_edge->endpoints[0] = pt;       // share the point
            link_tail((Object*)next_edge->endpoints[0], &plist);
            final = 1;
        }
        else if (near_pt(next_edge->endpoints[1], pt, snap_tol))
        {
            next_edge->endpoints[1] = pt;
            link_tail((Object*)next_edge->endpoints[1], &plist);
            final = 0;
        }
        else
        {
            return FALSE;
        }
        pt = next_edge->endpoints[final];
        n_edges++;
    }

    // Share the last point back to the beginning
    ASSERT(near_pt(((Edge*)group->obj_list.head)->endpoints[initial], pt, snap_tol), "The edges don't close at the starting point");
    next_edge->endpoints[final] = ((Edge*)group->obj_list.head)->endpoints[initial];

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
        face_type = FACE_FLAT;
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

    // Populate the edge list, sharing points along the way. 
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

// Make a body of revolution by revolving the given edge group around the 
// current path. Open edge groups are completed with circles at the poles.
// If successful, the edge group is deleted and the volume is returned.
Volume *
make_body_of_revolution(Group* group)
{
    ListHead plist = { NULL, NULL };
    Group* clone;
    Volume* vol;
    Face* f;
    Edge* e, *ne, *na, *o, *path, *prev_ne, *prev_na;
    ArcEdge* nae;
    Point* eip, * oip, *old_eip, *pt;
    Point top_centre, bottom_centre;
    Plane axis;
    int idx, initial, final, n_steps;
    BOOL open = group->hdr.lock == LOCK_POINTS;
    float r, rad;

    // Some checks first.
    if (curr_path == NULL)
        return NULL;
    if (curr_path->type == OBJ_GROUP)
        path = (Edge*)(((Group*)curr_path)->obj_list.head);
    else
        path = (Edge*)curr_path;
    ASSERT(path->hdr.type == OBJ_EDGE, "Path needs to be an edge");
    if (path->hdr.type != OBJ_EDGE)
        return NULL;

    // Join the edges up by sharing their endpoints.
    e = (Edge*)group->obj_list.head;
    ne = (Edge*)group->obj_list.head->next;
    if (near_pt(e->endpoints[0], ne->endpoints[0], snap_tol))
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (near_pt(e->endpoints[0], ne->endpoints[1], snap_tol))
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (near_pt(e->endpoints[1], ne->endpoints[0], snap_tol))
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else if (near_pt(e->endpoints[1], ne->endpoints[1], snap_tol))
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else
    {
        ASSERT(FALSE, "The edges aren't connected");
        return NULL;
    }

    // Start remembering the radii to the axis, as well as the top and
    // bottom points' centres, in case the group is open and we need to 
    // put in circle faces to close the volume.
    rad = dist_point_to_perp_line(e->endpoints[initial], path, &top_centre);

    for (; e->hdr.next != NULL; e = ne)
    {
        ne = (Edge*)e->hdr.next;

        if (e->hdr.next->type != OBJ_EDGE)
            return NULL;

        // Strip construction edges, as we can't consistently create them
        e->type &= ~EDGE_CONSTRUCTION;

        if (near_pt(ne->endpoints[0], pt, snap_tol))
        {
            ne->endpoints[0] = pt;       // share the point
            final = 1;
        }
        else if (near_pt(ne->endpoints[1], pt, snap_tol))
        {
            ne->endpoints[1] = pt;
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
    }

    // If closed, share the last point back to the beginning.
    if (!open)
    {
        ASSERT(near_pt(((Edge*)group->obj_list.head)->endpoints[initial], pt, snap_tol), "The edges don't close at the starting point");
        ne->endpoints[final] = ((Edge*)group->obj_list.head)->endpoints[initial];
    }
    r = dist_point_to_perp_line(ne->endpoints[final], path, &bottom_centre);
    if (r > rad)
        rad = r;

    idx = initial;

#if 0
    // Determine the normal
    polygon_normal((Point*)plist.head, &group_norm);
#endif

    // Work out the number of steps in all the arcs. They must all be the
    // same, so use the worst case (from the largest radius to the axis)
    n_steps = (int)(2 * PI / (2.0 * acos(1.0 - tolerance / rad)));

    // The plane derived from the path used as an axis
    axis.A = path->endpoints[1]->x - path->endpoints[0]->x;
    axis.B = path->endpoints[1]->y - path->endpoints[0]->y;
    axis.C = path->endpoints[1]->z - path->endpoints[0]->z;
    normalise_plane(&axis);

    // Clone the edge list in the same location
    clone = (Group *)copy_obj((Object*)group, 0, 0, 0);
    clear_move_copy_flags((Object*)group);
    e = (Edge*)group->obj_list.head;
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
    // TODO: Set nae->clockwise somehow?
    nae->clockwise = FALSE;
    nae->normal = axis;
    nae->normal.refpt = *nae->centre;
    old_eip = eip;

    if (open)
    {
       // Face* circle = face_new(FACE_CIRCLE, &norm);



    }

    idx = 1 - idx;      // point to far end of edge in group

    vol = vol_new();

    // Proceed down the edge group adding arc edges and barrel faces to the volume
    for 
    (
        e = (Edge *)group->obj_list.head, o = (Edge*)clone->obj_list.head;
        e != NULL; 
        e = (Edge*)e->hdr.next, o = (Edge*)o->hdr.next
    )
    {
        Plane n = { 0, };
        FACE ftype = (e->type == EDGE_STRAIGHT) ? FACE_RECT : FACE_CYLINDRICAL;

        eip = e->endpoints[idx];
        oip = o->endpoints[idx];
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
        // TODO: Set nae->clockwise somehow?
        nae->clockwise = FALSE;
        nae->normal = axis;
        nae->normal.refpt = *nae->centre;

        f = face_new(ftype, n);
        f->edges[0] = ne;
        f->edges[1] = e;
        f->edges[2] = prev_ne;
        f->edges[3] = o;
        f->n_edges = 4;
        f->initial_point = oip;
        f->vol = vol;
        link((Object*)f, &vol->faces);

        ftype = (e->type == EDGE_STRAIGHT) ? FACE_CYLINDRICAL : FACE_BARREL;
        f = face_new(ftype, n);
        f->edges[0] = prev_na;
        f->edges[1] = o;
        f->edges[2] = na;
        f->edges[3] = e;
        f->n_edges = 4;
        f->initial_point = old_eip;
        f->vol = vol;
        link((Object*)f, &vol->faces);

        delink_group((Object*)e, group);
        delink_group((Object*)o, clone);

        // Find the far end of the next pair of edges
        ne = (Edge *)e->hdr.next;
        if (ne == NULL)
            break;

        if (e->endpoints[idx] = ne->endpoints[0])
            idx = 1;
        else
            idx = 0;

        prev_ne = ne;
        prev_na = na;
        old_eip = eip;
    }

    // Add the last circle if group was open
    if (open)
    {
        // Face* circle = face_new(FACE_CIRCLE, &norm);



    }

    // Finally, remove the old edge groups
    ASSERT(group->obj_list.head == NULL, "Edge group is not empty");
    ASSERT(clone->obj_list.head == NULL, "Edge group is not empty");
    delink_group((Object*)group, &object_tree);
    purge_obj((Object*)group);
    purge_obj((Object*)clone);

    return vol;
}
