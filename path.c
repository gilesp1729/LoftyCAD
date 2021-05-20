#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Helpers to place objects along paths. The paths may be single edges
// or edge groups. 
// - find the total length of a path
// - find the length along the path to an intersection with a plane
// - find the tangent ABC of the path at an intersection with a plane
float
path_total_length(Object* obj)
{
    if (obj->type == OBJ_EDGE)
    {
        Edge* e = (Edge*)curr_path;

        return length(e->endpoints[0], e->endpoints[1]);
    }
    else
    {
        ASSERT(FALSE, "Complex path not supported (yet)");
        return 0;
    }
}

void
path_tangent_to_intersect(Object* obj, Plane* pl, Plane* tangent)
{
    if (obj->type == OBJ_EDGE)
    {
        Edge* e = (Edge*)obj;

        tangent->A = e->endpoints[1]->x - e->endpoints[0]->x;
        tangent->B = e->endpoints[1]->y - e->endpoints[0]->y;
        tangent->C = e->endpoints[1]->z - e->endpoints[0]->z;
    }
    else
    {
    }
}

float
path_length_to_intersect(Object* obj, Plane* pl)
{
    if (obj->type == OBJ_EDGE)
    {
        Plane tangent;
        Point pt;

        path_tangent_to_intersect(obj, pl, &tangent);
        intersect_line_plane(&tangent, pl, &pt);
        return length(((Edge*)obj)->endpoints[0], &pt);
    }
    else
    {
    }
}


