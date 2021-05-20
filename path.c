#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Helpers to place objects along paths. The paths may be single edges
// or edge groups. 
// - find the total length of a path
// - find the length along the path to an intersection with a plane
// - find the tangent ABC of the path at an intersection with a plane

// Single edge routines.
float
edge_total_length(Edge* e)
{
    switch (e->type)
    {
    case EDGE_STRAIGHT:
        return length(e->endpoints[0], e->endpoints[1]);




    }
}




// Path routines.
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
            total_length += edge_total_length(e);
        return total_length;
    }
}

// Return length along path to intersect with pl, and the tangent at pl.
float
path_tangent_to_intersect(Object* obj, Plane* pl, Plane* tangent)
{
    if (obj->type == OBJ_EDGE)
    {
        Edge* e = (Edge*)obj;
        Point pt;

        tangent->A = e->endpoints[1]->x - e->endpoints[0]->x;
        tangent->B = e->endpoints[1]->y - e->endpoints[0]->y;
        tangent->C = e->endpoints[1]->z - e->endpoints[0]->z;
        tangent->refpt = *e->endpoints[0];
        intersect_line_plane(tangent, pl, &pt);
        return length(((Edge*)obj)->endpoints[0], &pt);
    }
    else
    {
        Group* group = (Group*)obj;

        ASSERT(is_edge_group(group), "Path is not an edge group");

        // If the path is not straight, there's a chance it will intersect pl twice.
        // To avoid this, we use the refpt of pl instead of the intersection and
        // find the closest point on the path.


    }
}


