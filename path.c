#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

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
    switch (e->type)
    {
    case EDGE_STRAIGHT:
        return length(e->endpoints[0], e->endpoints[1]);




    }
}

// Return length along path to intersect with pl, and the tangent at pl.
// Returns: 1 - intersects, 0 - no intersection, -1 - line lies in the plane
// Returns 2 if intersection is off the end of the edge, but is valid
// (as for intersect_line_plane)
int
edge_tangent_to_intersect(Edge *e, Plane* pl, Bbox *ebox, Plane* tangent, float* ret_len)
{
    Point pt;
    int rc = 0;

    switch (e->type)
    {
    case EDGE_STRAIGHT:
        tangent->A = e->endpoints[1]->x - e->endpoints[0]->x;
        tangent->B = e->endpoints[1]->y - e->endpoints[0]->y;
        tangent->C = e->endpoints[1]->z - e->endpoints[0]->z;
        tangent->refpt = *e->endpoints[0];

        rc = intersect_line_plane(tangent, pl, &pt);
        *ret_len = length(e->endpoints[0], &pt);
        
        // Check that pt is within ebox, and return 0 if it isn't.
        if (!in_bbox(&pt, ebox, SMALL_COORD))
            rc = 0;

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
            // Accumulate the total length of edges so far, and store in this edge.
            e->prev_total_length = total_length;
            total_length += edge_total_length(e);
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
        return edge_tangent_to_intersect(e, pl, ebox, tangent, ret_len) > 0;
    }
    else
    {
        Group* group = (Group*)obj;
        Edge* e;
        ASSERT(is_edge_group(group), "Path is not an edge group");

        // If the path is not straight, there's a chance it will intersect pl twice.
        // Take only the intersections within the bbox and within the edge.
        for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            if (edge_tangent_to_intersect(e, pl, ebox, tangent, ret_len) == 1)
            {
                *ret_len += e->prev_total_length;
                return TRUE;
            }
        }
    }
    return FALSE;
}
