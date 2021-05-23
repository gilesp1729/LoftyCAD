#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Helpers to find the first point of an edge group (the one not in common with the
// next edge)
int
first_point_index(Edge* edge)
{
    Edge* next_edge = (Edge*)edge->hdr.next;

    // Special case where next_edge is NULL (the last edge in the edge group)
    if (next_edge == NULL)
    {
        Edge* prev_edge = (Edge*)edge->hdr.prev;

        if (edge->endpoints[0] == prev_edge->endpoints[0])
            return 0;
        else if (edge->endpoints[0] == prev_edge->endpoints[1])
            return 0;
        else if (edge->endpoints[1] == prev_edge->endpoints[0])
            return 1;
        else if (edge->endpoints[1] == prev_edge->endpoints[1])
            return 1;
        else
            ASSERT(FALSE, "Edges do not join up");
    }
    else
    {
        if (edge->endpoints[0] == next_edge->endpoints[0])
            return 1;
        else if (edge->endpoints[0] == next_edge->endpoints[1])
            return 1;
        else if (edge->endpoints[1] == next_edge->endpoints[0])
            return 0;
        else if (edge->endpoints[1] == next_edge->endpoints[1])
            return 0;
        else
            ASSERT(FALSE, "Edges do not join up");
    }
    return -1;
}

Point*
first_point(Edge* e)
{
    int i = first_point_index(e);

    if (i < 0)
        return NULL;
    return e->endpoints[i];
}

// Helper to find the direction of an edge, in order from first point to the other end.
// Return it in the (normalised) ABC of a Plane, and return the edge's first point index.
// The refpt of the Plane is set to the first point.
int
edge_direction(Edge* e, Plane* pl)
{
    int i = first_point_index(e);
    Point* p0 = e->endpoints[i];
    Point* p1 = e->endpoints[1 - i];

    pl->A = p1->x - p0->x;
    pl->B = p1->y - p0->y;
    pl->C = p1->z - p0->z;
    pl->refpt = *p0;
    normalise_plane(pl);

    return i;
}

// Similarly, given two points.
void
point_direction(Point* p0, Point* p1, Plane* pl)
{
    pl->A = p1->x - p0->x;
    pl->B = p1->y - p0->y;
    pl->C = p1->z - p0->z;
    pl->refpt = *p0;
    normalise_plane(pl);
}

// Helper to project a vector AP onto a plane through A parallel to the
// principal plane.
// 
// AP is expressed as a Plane whose refpt is A (as returned from edge_direction
// or point_direction)
// 
// Return a normalised vector of its projection with refpt A.
void
project(Plane* ap, Plane* princ, Plane* proj)
{
    float perp_factor =
        (princ->A * ap->A + princ->B * ap->B + princ->C * ap->C)
        /
        (princ->A * princ->A + princ->B * princ->B + princ->C * princ->C);

    proj->A = ap->A - princ->A * perp_factor;
    proj->B = ap->B - princ->B * perp_factor;
    proj->C = ap->C - princ->C * perp_factor;
    proj->refpt = ap->refpt;
    normalise_plane(proj);
}


// Helpers to place objects along paths. The paths may be single edges
// or edge groups. 
// - find the total length of a path
// - find the length along the path to an intersection with a plane
// - find the tangent ABC of the path at an intersection with a plane

// Single edge routines.

// Return the total length of the edge.
float
edge_total_length(Edge* e)
{
    Point* p;
    float total_length = 0;

    switch (e->type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
        return length(e->endpoints[0], e->endpoints[1]);

    case EDGE_ARC:
    case EDGE_BEZIER:
        for (p = (Point *)e->view_list.head; p->hdr.next != NULL; p = (Point*)p->hdr.next)
            total_length += length(p, (Point *)p->hdr.next);
        return total_length;
    }

    return 0;  // catch-all
}

// Return length along path to intersect with pl, and the tangent at pl.
// Returns: 1 - intersects, 0 - no intersection, -1 - line lies in the plane
// Returns 2 if intersection is off the end of the edge, but is valid
// (as for intersect_line_plane)
int
edge_tangent_to_intersect(Edge *e, int first_index, Plane* pl, Bbox *ebox, Plane* tangent, float* ret_len)
{
    Point pt;
    Point* p;
    int rc = 0;
    int last_index = 1 - first_index;
    float accum_length = 0;
    int i;

    switch (e->type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
        tangent->A = e->endpoints[last_index]->x - e->endpoints[first_index]->x;
        tangent->B = e->endpoints[last_index]->y - e->endpoints[first_index]->y;
        tangent->C = e->endpoints[last_index]->z - e->endpoints[first_index]->z;
        tangent->refpt = *e->endpoints[first_index];

        rc = intersect_line_plane(tangent, pl, &pt);
        *ret_len = length(e->endpoints[first_index], &pt);
        
        // Check that pt is within ebox, and return 0 if it isn't.
        if (!in_bbox(&pt, ebox, (float)SMALL_COORD))
            rc = 0;
        break;

    case EDGE_ARC:
    case EDGE_BEZIER:
        for (i = 0, p = (Point*)e->view_list.head; p->hdr.next != NULL; i++, p = (Point*)p->hdr.next)
        {
            Point* next_p = (Point *)p->hdr.next;

            accum_length += length(p, next_p);
            tangent->A = next_p->x - p->x;
            tangent->B = next_p->y - p->y;
            tangent->C = next_p->z - p->z;
            tangent->refpt = *p;
            rc = intersect_line_plane(tangent, pl, &pt);
            if (!in_bbox(&pt, ebox, (float)SMALL_COORD))
                rc = 0;
            if (rc == 1)
            {
                // We have an intersection within the ebox.
                // Accumulate the length from first_index. Don't worry about the little bit
                // of intersected line in the VL.
                // TODO Take care: the VL is ordered from endpoint[0] to [1], which may not be
                // the same order as first_index to last_index.
                if (first_index == 1)
                {
                    accum_length = e->edge_length - accum_length;
                    tangent->A = -tangent->A;
                    tangent->B = -tangent->B;
                    tangent->C = -tangent->C;
                    tangent->refpt = *next_p;
                }
                *ret_len = accum_length;
                break;
            }
        }
        break;
    }

    return rc;
}


// Path routines.

// Return the total length of the path. Call this first before calling
// any others.
float
path_total_length(Object* obj)
{
    if (obj->type == OBJ_EDGE)
    {
        return edge_total_length((Edge*)obj);
    }
    else
    {
        Group* group = (Group*)obj;
        Edge* e;
        float total_length = 0;

        ASSERT(is_edge_group(group), "Path is not an edge group");
        for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            // Accumulate the current length and total length of edges so far, and store in this edge.
            e->edge_length = edge_total_length(e);
            total_length += e->edge_length;
        }
        return total_length;
    }
}

// Return length along path to intersect with pl, and the tangent at pl.
BOOL
path_tangent_to_intersect(Object* obj, Plane* pl, Bbox *ebox, Plane* tangent, float *ret_len)
{
    if (obj->type == OBJ_EDGE)
    {
        Edge* e = (Edge*)obj;

        // The intersection can be off the end of the edge, that's OK.
        // But fail for parallels or not in bbox.
        return edge_tangent_to_intersect(e, 0, pl, ebox, tangent, ret_len) > 0;
    }
    else
    {
        Group* group = (Group*)obj;
        Edge* e;
        float total_length = 0;

        ASSERT(is_edge_group(group), "Path is not an edge group");

        // If the path is not straight, there's a chance it will intersect pl twice.
        // Take only the intersections within the bbox and within the edge.
        for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            if (edge_tangent_to_intersect(e, first_point_index(e), pl, ebox, tangent, ret_len) == 1)
            {
                // Add in the lengths of the non-intersecting edges found so far.
                *ret_len += total_length;
                return TRUE;
            }
            total_length += e->edge_length;
        }
    }
    return FALSE;
}