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

        // If there's only one edge, arbirarily return endpoint 0.
        if (prev_edge == NULL)
            return 0;

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

// Find a tangent at the given length within the edge.
// No checks are done on length. It is assumed to be within the edge length.
void
edge_tangent_to_length(Edge* e, int first_index, float len, Plane* tangent)
{
    Point* p;
    int rc = 0;
    int last_index = 1 - first_index;
    float accum_length = 0;
    float el;
    int i;

    switch (e->type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
        tangent->A = e->endpoints[last_index]->x - e->endpoints[first_index]->x;
        tangent->B = e->endpoints[last_index]->y - e->endpoints[first_index]->y;
        tangent->C = e->endpoints[last_index]->z - e->endpoints[first_index]->z;
        normalise_plane(tangent);
        tangent->refpt.x = e->endpoints[first_index]->x + tangent->A * len;
        tangent->refpt.y = e->endpoints[first_index]->y + tangent->B * len;
        tangent->refpt.z = e->endpoints[first_index]->z + tangent->C * len;
        break;

    case EDGE_ARC:
    case EDGE_BEZIER:
        if (first_index == 1)
            accum_length = e->edge_length;

        for (i = 0, p = (Point*)e->view_list.head; p->hdr.next != NULL; i++, p = (Point*)p->hdr.next)
        {
            Point* next_p = (Point*)p->hdr.next;

            // Accumulate the length from first_index. 
            // Take care: the VL is ordered from endpoint[0] to [1], which may not be
            // the same order as first_index to last_index.
            if (first_index == 0)
            {
                el = length(p, next_p);
                if (accum_length + el >= len)
                {
                    len -= accum_length;
                    tangent->A = next_p->x - p->x;
                    tangent->B = next_p->y - p->y;
                    tangent->C = next_p->z - p->z;
                    normalise_plane(tangent);
                    tangent->refpt.x = p->x + tangent->A * len;
                    tangent->refpt.y = p->y + tangent->B * len;
                    tangent->refpt.z = p->z + tangent->C * len;
                    break;
                }
                accum_length += el;
            }
            else
            {
                el = length(p, next_p);
                if (accum_length - el <= len)
                {
                    len -= accum_length;
                    tangent->A = p->x - next_p->x;
                    tangent->B = p->y - next_p->y;
                    tangent->C = p->z - next_p->z;
                    normalise_plane(tangent);
                    tangent->refpt.x = next_p->x + tangent->A * len;
                    tangent->refpt.y = next_p->y + tangent->B * len;
                    tangent->refpt.z = next_p->z + tangent->C * len;
                    break;
                }
                accum_length -= el;
            }
        }
        break;
    }
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
    
    switch (e->type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
        tangent->A = e->endpoints[last_index]->x - e->endpoints[first_index]->x;
        tangent->B = e->endpoints[last_index]->y - e->endpoints[first_index]->y;
        tangent->C = e->endpoints[last_index]->z - e->endpoints[first_index]->z;
        tangent->refpt = *e->endpoints[first_index];

        rc = intersect_line_plane(tangent, pl, &pt);
        *ret_len = length(e->endpoints[first_index], &pt);
        tangent->refpt = pt;
        
        // Check that pt is within ebox, and return 0 if it isn't.
        if (!in_bbox(&pt, ebox, (float)SMALL_COORD))
            rc = 0;

        // If we're off the end (rc == 2) check that an endpoint is also
        // within the ebox, using a wider tolerance. If not, return 0.
        if (rc == 2)
        {
            if (!in_bbox(e->endpoints[first_index], ebox, tolerance) && !in_bbox(e->endpoints[last_index], ebox, tolerance))
                rc = 0;
        }
        normalise_plane(tangent);
        return rc;

    case EDGE_ARC:
    case EDGE_BEZIER:
        // Check for true intersetions first before admitting off-end conditions.
        for (p = (Point*)e->view_list.head; p->hdr.next != NULL; p = (Point*)p->hdr.next)
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
                // We have a true intersection within the ebox.
                // Accumulate the length from first_index. Don't worry about the little bit
                // of intersected line in the VL.
                // Take care: the VL is ordered from endpoint[0] to [1], which may not be
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
                normalise_plane(tangent);
                return rc;
            }
        }

        // If we come out here, there was no intersection. Check again for off-end.
        accum_length = 0;
        for (p = (Point*)e->view_list.head; p->hdr.next != NULL; p = (Point*)p->hdr.next)
        {
            Point* next_p = (Point*)p->hdr.next;

            accum_length += length(p, next_p);
            tangent->A = next_p->x - p->x;
            tangent->B = next_p->y - p->y;
            tangent->C = next_p->z - p->z;
            tangent->refpt = *p;

            rc = intersect_line_plane(tangent, pl, &pt);
            if (!in_bbox(&pt, ebox, (float)SMALL_COORD))
                rc = 0;
            if (rc == 2)
            {
                if (!in_bbox(p, ebox, tolerance) && !in_bbox(next_p, ebox, tolerance))
                    rc = 0;
            }
            if (rc > 0)
            {
                // We have an off-end intersection and an endpoint within the ebox.
                // Accumulate the length from first_index as before.
                if (first_index == 1)
                {
                    accum_length = e->edge_length - accum_length;
                    tangent->A = -tangent->A;
                    tangent->B = -tangent->B;
                    tangent->C = -tangent->C;
                    tangent->refpt = *next_p;
                }
                *ret_len = accum_length;
                normalise_plane(tangent);
                return rc;
            }
        }
        break;
    }
    return rc;
}

// Spacings and angles governing tubing copies.
// The spacings are given as fractions of the largest size of the edge bounding box.
#define MAX_ANGLE   90
#define MIN_SPACING 1.5f
#define MAX_SPACING 4.0f

// Subdivide an edge. Args similar to path_subdivide.
void
edge_subdivide(Edge *e, Plane *initial_tangent, float initial_len, float max_ebox, Plane **tangents, int *n_tangents, int *max_tangents)
{
    Plane end_tangent;
    int i, n_copies;

    // Find the tangent at the far end of the edge
    edge_tangent_to_length(e, first_point_index(e), e->edge_length - tolerance, &end_tangent);

    // Number of internal copies in the edge (not counting the far end copy)
    n_copies = (int)((e->edge_length - initial_len) / (MAX_SPACING * max_ebox));
    if (n_copies > 0)
    {
        float delta_len = (e->edge_length - initial_len) / (n_copies + 1);

        for (i = 0; i < n_copies; i++)
        {
            initial_len += delta_len;
            edge_tangent_to_length(e, first_point_index(e), initial_len, &(*tangents)[(*n_tangents)++]);
            if (*n_tangents == *max_tangents)
            {
                *max_tangents *= 2;
                *tangents = realloc(*tangents, *max_tangents * sizeof(Plane));
            }
        }
    }

    // Final tangent position goes at end of the edge
    // Average angle with next edge (e->hdr.next) if it exists, in case of a discontinuity of angle
    if (e->hdr.next != NULL)
    {
        Plane next_tangent;

        edge_tangent_to_length((Edge *)e->hdr.next, first_point_index((Edge *)e->hdr.next), tolerance, &next_tangent);
        end_tangent.A = (end_tangent.A + next_tangent.A) / 2;
        end_tangent.B = (end_tangent.B + next_tangent.B) / 2;
        end_tangent.C = (end_tangent.C + next_tangent.C) / 2;
    }

    (*tangents)[(*n_tangents)++] = end_tangent;
    if (*n_tangents == *max_tangents)
    {
        *max_tangents *= 2;
        *tangents = realloc(*tangents, *max_tangents * sizeof(Plane));
    }
}


// Path routines.

// Determine if a path is closed. For a single edge, return TRUE if the
// edge endpoints are coincident.
// NOTE: At present, you cannot create closed paths. 
BOOL
path_is_closed(Object* obj)
{
    if (obj->type == OBJ_EDGE)
    {
        Edge* e = (Edge*)obj;

        return near_pt(e->endpoints[0], e->endpoints[1], snap_tol);
    }
    return is_closed_edge_group((Group*)obj);
}

// Return the total length of the path. Call this first before calling
// any others.
float
path_total_length(Object* obj)
{
    Edge* e;

    if (obj->type == OBJ_EDGE)
    {
        e = (Edge*)obj;
        e->edge_length = edge_total_length(e);
        return e->edge_length;
    }
    else
    {
        Group* group = (Group*)obj;
        float total_length = 0;

        ASSERT(is_edge_group(group), "Path is not an edge group");
        for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            // Find the current edge length, and store in this edge.
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
        Plane tangent_candidate;
        float len;
        int num_found = 0;

        ASSERT(is_edge_group(group), "Path is not an edge group");
        *ret_len = 0;
        memset(tangent, 0, sizeof(Plane));

        // If the path is not straight, there's a chance it will intersect pl twice.
        // Take only the intersections within the bbox and within the edge.
        for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            // Average tangent angles when neighbouring edge endpoints both fall within ebox (or more than two)
            // Tolerate off-end at the end of the path to assist tubing.
            if (edge_tangent_to_intersect(e, first_point_index(e), pl, ebox, &tangent_candidate, &len) > 0)
            {
                num_found++;
                *ret_len += len + total_length;
                tangent->A += tangent_candidate.A;
                tangent->B += tangent_candidate.B;
                tangent->C += tangent_candidate.C;
                tangent->refpt.x += tangent_candidate.refpt.x;
                tangent->refpt.y += tangent_candidate.refpt.y;
                tangent->refpt.z += tangent_candidate.refpt.z;
            }
            total_length += e->edge_length;
        }
        if (num_found > 0)
        {
            *ret_len /= num_found;
            tangent->A /= num_found;
            tangent->B /= num_found;
            tangent->C /= num_found;
            tangent->refpt.x /= num_found;
            tangent->refpt.y /= num_found;
            tangent->refpt.z /= num_found;
            return TRUE;
        }
    }

    return FALSE;
}

// Given an initial tangent and its length along the path (as returned from path_tangent_to_intersect)
// Return an array of tangents (points/directions) along the path at suitable locations
// for tubing. One at each end of a straight edge, and arcs/beziers subdivided so as to 
// make no more than max_angle of deviation between them. Return the number of tangents
// stored in the array.
int
path_subdivide(Object* obj, Plane* initial_tangent, Bbox *ebox, float initial_len, Plane **tangents)
{
    int n_tangents = 0, max_tangents = 8;   // must be a power of 2
    float max_ebox = 0;

    // Allocate a tangents array; it may be enlarged later.
    *tangents = calloc(max_tangents, sizeof(Plane));

    // Find the largest side of the ebox
    max_ebox = ebox->xmax - ebox->xmin;
    if (ebox->ymax - ebox->ymin > max_ebox)
        max_ebox = ebox->ymax - ebox->ymin;
    if (ebox->zmax - ebox->zmin > max_ebox)
        max_ebox = ebox->zmax - ebox->zmin;

    if (obj->type == OBJ_EDGE)
    {
        Edge* e = (Edge*)obj;

        edge_subdivide(e, initial_tangent, initial_len, max_ebox, tangents, &n_tangents, &max_tangents);
    }
    else
    {
        Group* group = (Group*)obj;
        Edge* e;
        float total_length = 0;

        ASSERT(is_edge_group(group), "Path is not an edge group");

        // Find the first edge whose summed length exceeds initial_len, and put
        // in tangents at changes of edge from that point on.
        // TODO: closed paths.
        for (e = (Edge*)group->obj_list.head; e != NULL; e = (Edge*)e->hdr.next)
        {
            total_length += e->edge_length;
            if (total_length < initial_len)
                continue;

            // Short runs are not processed for now, as there are just too many cases that cannot be handled.
            edge_subdivide(e, initial_tangent, initial_len, max_ebox, tangents, &n_tangents, &max_tangents);

            // Zero the initial length and set initial tangent to end of previous edge
            initial_len = 0;
            initial_tangent = tangents[n_tangents - 1];
        }
    }
    return n_tangents;
}