#include "stdafx.h"
#include "LoftyCAD.h"


// Neighbourhood functions - use for picking when dragging a 3D object.

// Helpers for point-in-polygon test.
// From Sunday, "Inclusion of a point in a polygon" http://geomalgorithms.com/a03-_inclusion.html
// isLeft(): tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2  on the line
//            <0 for P2  right of the line
//    See: Algorithm 1 "Area of Triangles and Polygons"
float
isLeft(Point2D P0, Point2D P1, Point2D P2)
{
    return ((P1.x - P0.x) * (P2.y - P0.y)
            - (P2.x - P0.x) * (P1.y - P0.y));
}

// Find if a point is in a polygon.
// From Sunday, "Inclusion of a point in a polygon" http://geomalgorithms.com/a03-_inclusion.html
// Winding number test for a point in a polygon
//      Input:   P = a point,
//               V[] = vertex points of a polygon V[n+1] with V[n]=V[0]
//      Return:  wn = the winding number (=0 only when P is outside)
int
point_in_polygon2D(Point2D P, Point2D* V, int n)
{
    int    wn = 0;    // the  winding number counter
    int i;

    // loop through all edges of the polygon
    for (i = 0; i < n; i++)
    {   // edge from V[i] to  V[i+1]
        if (V[i].y <= P.y)
        {          // start y <= P.y
            if (V[i + 1].y  > P.y)      // an upward crossing
            if (isLeft(V[i], V[i + 1], P) > 0)  // P left of  edge
                ++wn;            // have  a valid up intersect
        }
        else
        {                        // start y > P.y (no test needed)
            if (V[i + 1].y <= P.y)     // a downward crossing
            if (isLeft(V[i], V[i + 1], P) < 0)  // P right of  edge
                --wn;            // have  a valid down intersect
        }
    }
    return wn;
}

// Helper for find_in_neighbourhood:
// Find any snappable component in obj, within snapping distance of point.
// obj may be a point or a straight edge.
Object *
find_in_neighbourhood_point(Point *point, Object *obj)
{
    Point *p;
    Point2D pt;
    float a, b, c, dx, dy, dz;
    Edge *e;
    Face *f;
    Volume *vol;
    Group *group;
    Object *o;
    int i;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point *)obj;
        if (p == point)
            return NULL;
        if (near_pt(point, p, snap_tol))
            return obj;
        break;

    case OBJ_EDGE:
        e = (Edge *)obj;
        if (e->endpoints[0] == point || e->endpoints[1] == point)
            return NULL;

        if (find_in_neighbourhood_point(point, (Object *)e->endpoints[0]))
            return (Object *)e->endpoints[0];
        if (find_in_neighbourhood_point(point, (Object *)e->endpoints[1]))
            return (Object *)e->endpoints[1];
        if (dist_point_to_edge(point, e) < snap_tol)
            return obj;
        break;

    case OBJ_FACE:
        f = (Face *)obj;
        
        // Test for hits on edges.
        for (i = 0; i < f->n_edges; i++)
        {
            Object *test = find_in_neighbourhood_point(point, (Object *)f->edges[i]);

            if (test != NULL)
                return test;
        }

        // Test point against interior of face.
        a = fabsf(f->normal.A);
        b = fabsf(f->normal.B);
        c = fabsf(f->normal.C);
        
        // make sure point is in plane first
        dx = point->x - f->normal.refpt.x;
        dy = point->y - f->normal.refpt.y;
        dz = point->z - f->normal.refpt.z;
        if (fabsf(a * dx + b * dy + c * dz) > snap_tol)
            return NULL;

        if (c > b && c > a)
        {
            pt.x = point->x;
            pt.y = point->y;
        }
        else if (b > a && b > c)
        {
            pt.x = point->x;
            pt.y = point->z;
        }
        else
        {
            pt.x = point->y;
            pt.y = point->z;
        }

        if (point_in_polygon2D(pt, f->view_list2D, f->n_view2D))
            return obj; 

        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (f = (Face *)vol->faces.head; f != NULL; f = (Face *)f->hdr.next)
        {
            Object *test = find_in_neighbourhood_point(point, (Object *)f);

            if (test != NULL)
                return test;
        }
        break;

    case OBJ_GROUP:
        group = (Group *)obj;
        for (o = group->obj_list.head; o != NULL; o = o->next)
        {
            Object *test = find_in_neighbourhood_point(point, o);

            if (test != NULL)
                return test;
        }
        break;
    }

    return NULL;
}

// Helper for find_in_neighbourhood:
// Find any snappable component in obj, within snapping distance of face.
// obj may be a face or a volume.
Object *
find_in_neighbourhood_face(Face *face, Object *obj)
{
    Face *f, *face1;
    Volume *vol;
    Object *o;
    float dx, dy, dz;
    int i;

    switch (obj->type)
    {
    case OBJ_FACE:
        face1 = (Face *)obj;
        // Don't test non-flat faces, and don't self-test.
        if (!IS_FLAT(face1))
            return NULL;
        if (face1 == face)
            return NULL;

        // Easy tests first.
        // Test if normals are the same, and the refpts lie in close to the same plane
        // Allow normals to be exactly opposite (e.g. one up one down)
        if (!nz(fabsf(face1->normal.A) - fabsf(face->normal.A)))
            return NULL;
        if (!nz(fabsf(face1->normal.B) - fabsf(face->normal.B)))
            return NULL;
        if (!nz(fabsf(face1->normal.C) - fabsf(face->normal.C)))
            return NULL;

        dx = face->normal.refpt.x - face1->normal.refpt.x;
        dy = face->normal.refpt.y - face1->normal.refpt.y;
        dz = face->normal.refpt.z - face1->normal.refpt.z;
        if (fabsf(face1->normal.A * dx + face1->normal.B * dy + face1->normal.C * dz) > snap_tol)
            return NULL;

        // If we got through that, now test if face and face1 overlap
        for (i = 0; i < face->n_view2D; i++)
        {
            if (point_in_polygon2D(face->view_list2D[i], face1->view_list2D, face1->n_view2D))
                return obj;  // this point is in. TODO: we need to test not just points - rect intersects are easily missed
        }

        return NULL;    // no points found, polygons do not overlap

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (f = (Face *)vol->faces.head; f != NULL; f = (Face *)f->hdr.next)
        {
            Object *test = find_in_neighbourhood_face(face, (Object *)f);

            if (test != NULL)
                return test;
        }
        break;

    case OBJ_GROUP:
        for (o = ((Group *)obj)->obj_list.head; o != NULL; o = o->next)
        {
            Object *test = find_in_neighbourhood_face(face, o);

            if (test != NULL)
                return test;
        }
        break;
    }

    return NULL;
}

// Find any object within snapping distance of the given object:
// For points, returns all objects at or passing near the coordinate.
// For faces, returns faces parallel and close to the face.
// For volumes, returns faces parallel and close to any face.
Object *
find_in_neighbourhood(Object *match_obj, Group *tree)
{
    Object *obj, *ret_obj = NULL;
    Face *f;
    Volume *vol;

    if (match_obj == NULL)
        return NULL;

    for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
    {
        Object *test = NULL, *test2 = NULL;

        switch (match_obj->type)
        {
        case OBJ_POINT:
            test = find_in_neighbourhood_point((Point *)match_obj, obj);
            break;

        case OBJ_EDGE:
            // only test the endpoints
            test = find_in_neighbourhood_point(((Edge*)match_obj)->endpoints[1], obj);
            if (test == NULL)
                test = find_in_neighbourhood_point(((Edge*)match_obj)->endpoints[0], obj);
            break;

        case OBJ_FACE:
            test = find_in_neighbourhood_face((Face *)match_obj, obj);
            break;

        case OBJ_VOLUME:
            // When moving volumes, need to HL faces. Combinatorial explosion of tests..
            vol = (Volume *)match_obj;
            for (f = (Face *)vol->faces.head; f != NULL; f = (Face *)f->hdr.next)
            {
                test = find_in_neighbourhood_face(f, obj);
                if (test != NULL)
                    break;
            }
            break;
        }

        if (test != NULL)
        {
            // return the lowest priority object.
            if (ret_obj == NULL || test->type < ret_obj->type)
                ret_obj = test;
        }
    }

    return ret_obj;
}

// Picking helpers: return an intersecting object with a ray. Also return the distance
// to the ray's near plane (the refpt of the Plane) to help with sorting.
// For faces, only viewable faces are considered (normal towards eye)
Object* pick_point(Point* p, LOCK parent_lock, Plane* line, float *dist)
{
    Point point;

    if (dist_point_to_ray(p, line, &point) < snap_tol)
    {
        *dist = length(&line->refpt, &point);
        return (Object*)p;
    }

    return NULL;
}

Object* pick_edge(Edge* e, LOCK parent_lock, Plane* line, float* dist)
{
    Point point;
    Point* p;
    Object* test;
    ArcEdge* ae;
    BezierEdge* be;

    // Check if the endpoints are hit first.
    if (parent_lock < LOCK_POINTS)
    {
        test = pick_point(e->endpoints[0], parent_lock, line, dist);
        if (test != NULL)
            return test;
        test = pick_point(e->endpoints[1], parent_lock, line, dist);
        if (test != NULL)
            return test;
    }

    switch (e->type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
        if (dist_ray_to_edge(line, e, &point) < snap_tol)
        {
            *dist = length(&line->refpt, &point);
            return (Object*)e;
        }
        break;

    case EDGE_ARC:
        ae = (ArcEdge*)e;
        test = pick_point(ae->centre, parent_lock, line, dist);
        if (test != NULL)
            return test;
        goto test_edge;

    case EDGE_BEZIER:
        be = (BezierEdge*)e;
        test = pick_point(be->ctrlpoints[0], parent_lock, line, dist);
        if (test != NULL)
            return test;
        test = pick_point(be->ctrlpoints[1], parent_lock, line, dist);
        if (test != NULL)
            return test;

    test_edge:
        if (!e->view_valid)
            return NULL;
        for (p = (Point *)e->view_list.head; p->hdr.next != NULL; p = (Point *)p->hdr.next)
        {
            if (dist_ray_to_segment(line, p, (Point *)p->hdr.next, &point) < snap_tol)
            {
                *dist = length(&line->refpt, &point);
                return (Object*)e;
            }
        }
        break;
    }

    return NULL;
}

Object* pick_face(Face* f, LOCK parent_lock, Plane* line, float* dist)
{
    Point point;
    Point* p;
    Point2D pt;
    float a, b, c;

    switch (f->type & ~FACE_CONSTRUCTION)
    {
    case FACE_TRI:
    case FACE_RECT:
    case FACE_FLAT:
    case FACE_CIRCLE:
        // If a flat face is turning away, no need to consider it. We also don't
        // want to pick edges bounded by faces both turned away.
        if (pldot(line, &f->normal) >= 0)
            return NULL;

        // Check if the edges are hit first.
        if (parent_lock < LOCK_EDGES)
        {
            int i;

            for (i = 0; i < f->n_edges; i++)
            {
                Object* test = pick_edge(f->edges[i], parent_lock, line, dist);

                if (test != NULL)
                    return test;
            }
        }

        // Find the intersection point and check if it lies in the face.
        if (intersect_line_plane(line, &f->normal, &point) > 0)
        {
            a = fabsf(f->normal.A);
            b = fabsf(f->normal.B);
            c = fabsf(f->normal.C);

            if (c > b && c > a)
            {
                pt.x = point.x;
                pt.y = point.y;
            }
            else if (b > a && b > c)
            {
                pt.x = point.x;
                pt.y = point.z;
            }
            else
            {
                pt.x = point.y;
                pt.y = point.z;
            }

            if (point_in_polygon2D(pt, f->view_list2D, f->n_view2D))
            {
                *dist = length(&line->refpt, &point);
                return (Object*)f;
            }
        }
        break;

    case FACE_CYLINDRICAL:
    case FACE_BARREL:
    case FACE_BEZIER:
        // Check if the edges are hit first.
        if (parent_lock < LOCK_EDGES)
        {
            int i;

            for (i = 0; i < f->n_edges; i++)
            {
                Object* test = pick_edge(f->edges[i], parent_lock, line, dist);

                if (test != NULL)
                    return test;
            }
        }

        // These faces have facets in their view lists. Test each one separately. Could be slow...
        for (p = (Point*)f->view_list.head; p != NULL; )
        {
            Plane facet_normal;
            Point2D facet2D[5];

            ASSERT(p->flags == FLAG_NEW_FACET, "Expecting a facet normal");
            facet_normal.A = p->x;
            facet_normal.B = p->y;
            facet_normal.C = p->z;
            p = (Point *)p->hdr.next;           // get first point of 4
            facet_normal.refpt = *p;

            // test normal facing away first
            if (pldot(line, &facet_normal) >= 0)
                goto next_facet;

            // test for intersection within facet
            if (intersect_line_plane(line, &facet_normal, &point) > 0)
            {
                a = fabsf(facet_normal.A);
                b = fabsf(facet_normal.B);
                c = fabsf(facet_normal.C);

                if (c > b && c > a)
                {
                    pt.x = point.x;
                    pt.y = point.y;
                    facet2D[0].x = p->x;
                    facet2D[0].y = p->y;
                    p = (Point*)p->hdr.next; 
                    facet2D[1].x = p->x;
                    facet2D[1].y = p->y;
                    p = (Point*)p->hdr.next; 
                    facet2D[2].x = p->x;
                    facet2D[2].y = p->y;
                    p = (Point*)p->hdr.next; 
                    facet2D[3].x = p->x;
                    facet2D[3].y = p->y;
                }
                else if (b > a && b > c)
                {
                    pt.x = point.x;
                    pt.y = point.z;
                    facet2D[0].x = p->x;
                    facet2D[0].y = p->z;
                    p = (Point*)p->hdr.next;
                    facet2D[1].x = p->x;
                    facet2D[1].y = p->z;
                    p = (Point*)p->hdr.next;
                    facet2D[2].x = p->x;
                    facet2D[2].y = p->z;
                    p = (Point*)p->hdr.next;
                    facet2D[3].x = p->x;
                    facet2D[3].y = p->z;
                }
                else
                {
                    pt.x = point.y;
                    pt.y = point.z;
                    facet2D[0].x = p->y;
                    facet2D[0].y = p->z;
                    p = (Point*)p->hdr.next;
                    facet2D[1].x = p->y;
                    facet2D[1].y = p->z;
                    p = (Point*)p->hdr.next;
                    facet2D[2].x = p->y;
                    facet2D[2].y = p->z;
                    p = (Point*)p->hdr.next;
                    facet2D[3].x = p->y;
                    facet2D[3].y = p->z;
                }

                p = (Point*)p->hdr.next;
                facet2D[4] = facet2D[0];        // close the polygon
                if (point_in_polygon2D(pt, facet2D, 4))
                {
                    *dist = length(&line->refpt, &point);
                    return (Object*)f;
                }
            }
        next_facet:
            while (p != NULL && p->flags != FLAG_NEW_FACET)
                p = (Point*)p->hdr.next;
        }
        break;
    }

    return NULL;
}

Object* pick_object(Object* obj, LOCK parent_lock, Plane* line, float* dist)
{
    Object* test = NULL;
    Object* o;
    Face* f;

    switch (obj->type)
    {
    case OBJ_POINT:
        test = pick_point((Point*)obj, parent_lock, line, dist);
        break;

    case OBJ_EDGE:
        test = pick_edge((Edge*)obj, parent_lock, line, dist);
        break;

    case OBJ_FACE:
        test = pick_face((Face*)obj, parent_lock, line, dist);
        break;

    case OBJ_VOLUME:
        for (f = (Face*)((Volume*)obj)->faces.head; f != NULL; f = (Face*)f->hdr.next)
        {
            test = pick_face(f, parent_lock, line, dist);
            if (test != NULL)
                break;
        }
        break;

    case OBJ_GROUP:
        for (o = ((Group*)obj)->obj_list.head; o != NULL; o = o->next)
        {
            test = pick_object(o, obj->lock, line, dist);
            if (test != NULL)
                break;
        }
        break;
    }

    return test;
}

// Pick an object: find the frontmost object under the cursor, or NULL if nothing is there.
// Only execption is that picking a face in a locked volume will pick the parent volume instead.
// Setting force_pick ensures that even locked objects can be picked (use for the context menu)
Object*
Pick(GLint x_pick, GLint y_pick, BOOL force_pick)
{
    Object* test = NULL;
    Object* ret_obj = NULL;
    Object* obj;
    Object* parent;
    Plane line;
    float dist = LARGE_COORD;
    float ret_dist = LARGE_COORD;

    // Get ray from eye position.
    ray_from_eye(x_pick, y_pick, &line);
    normalise_plane(&line);

    // Loop though top-level objects.
    for (obj = object_tree.obj_list.head; obj != NULL; obj = obj->next)
    {
        test = pick_object(obj, obj->lock, &line, &dist);

        if (test != NULL)
        {
            parent = find_top_level_parent(&object_tree, test);

            // Special cases: if we are in STATE_NONE and we have a face on a fully locked volume,
            // or some other object locked it is own level, just look straight through it 
            // so we can pick things behind it. However, we must still
            // be able to right-click things, so don't do it if force_pick is set TRUE.
            if
            (
                !force_pick
                &&
                test->type == OBJ_FACE          // Face on a locked volume
                &&
                ((Face*)test)->vol != NULL
                &&
                ((Face*)test)->vol->hdr.lock >= LOCK_VOLUME
            )
            {
                raw_picked_obj = test;
                test = NULL;
            }
            else if
            (
                !force_pick
                &&
                test->type == OBJ_FACE          // Face out on its own, locked at face level
                &&
                ((Face*)test)->vol == NULL
                &&
                test->lock >= LOCK_FACES
            )
            {
                raw_picked_obj = test;
                test = NULL;
            }
            else if
            (
                !force_pick
                &&
                test->type == OBJ_FACE          // Face on a volume in a locked group
                &&
                ((Face*)test)->vol != NULL
                &&
                parent != NULL
                &&
                parent->lock == LOCK_GROUP
            )
            {
                raw_picked_obj = test;
                test = NULL;
            }
            else
            {
                // Return the object closest to the eye.
                if (ret_obj == NULL || dist < ret_dist)
                    ret_obj = test;
                ret_dist = dist;
            }
        }
    }

    // Some more special cases:

    // If the object is in a locked group, then return the group. Take account
    // of nested groups and return the topmost locked parent group.

    // If the object is a face, but it belongs to a volume that is locked at the
    // face or volume level, then return the parent volume instead.
    // Do the face test first, as if we have its volume we can find any parent
    // group quickly without a full search.
    parent_picked = NULL;
    if (ret_obj != NULL)
    {
        raw_picked_obj = ret_obj;
        parent = NULL;
        if (ret_obj->type == OBJ_FACE && ((Face*)ret_obj)->vol != NULL)
        {
            Face* face = (Face*)ret_obj;

            if (face->vol->hdr.lock >= LOCK_FACES)
                ret_obj = (Object*)face->vol;

            if (face->vol->hdr.parent_group != NULL && face->vol->hdr.parent_group->hdr.parent_group != NULL)
                parent = (Object *)face->vol->hdr.parent_group;
        }
        else
        {
            // Some other sort of object. See if it is in a group.
            if (ret_obj->parent_group != NULL && ret_obj->parent_group->hdr.parent_group != NULL)
                parent = (Object *)ret_obj->parent_group;
        }

        // Look up the ownership chain for the topmost locked group.
        // Return it if found. Otherwise just return the original ret_obj.

        // LOCK_GROUP, VOLUME = ordinary group, locked
        // LOCK_EDGES, POINTS = edge groups, locked
        // LOCK_FACES is the only case that is unlocked.
        if (parent != NULL)
        {
            for (; (Object*)parent->parent_group != NULL; parent = (Object*)parent->parent_group)
            {
                if (parent->lock != LOCK_FACES)
                    ret_obj = parent;
            }
        }

        // See if the final object (whatever it is) is fully locked and can't be picked.
        // (unless force picking)
        if (ret_obj->lock >= ret_obj->type && !force_pick)
            ret_obj = NULL;

        // Keep a pointer to the immediate parent group of whatever is returned, in case we need to highlight it.
        if (ret_obj->parent_group != NULL && ret_obj->parent_group->hdr.parent_group != NULL)
            parent_picked = (Object*)ret_obj->parent_group;
    }

    return ret_obj;
}

// Pick all top-level objects intersecting the given rect and select.
void
Pick_all_in_rect(GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick)
{
    // Unfinished!

}


// Old code

#if 0
    GLuint buffer[4096];
    GLint num_hits;
    Object* obj = NULL;
    Object* list = NULL;
    Object* parent;

    // Find the objects within the rect
    glSelectBuffer(4096, buffer);           // TODO: This may be too small. There are a lot of null hits.
    glRenderMode(GL_SELECT);
    Draw(TRUE, x_pick, y_pick, w_pick, h_pick);
    num_hits = glRenderMode(GL_RENDER);

    if (num_hits > 0)
    {
        int n = 0;
        int i;
#ifdef DEBUG_PICK
        int j, len;
        char buf[512];
        size_t size = 512;
        char* p = buf;
        char* obj_prefix[] = { "N", "P", "E", "F", "V" };
#endif

        for (i = 0; i < num_hits; i++)
        {
            int num_objs = buffer[n];

            if (num_objs == 0)
            {
                n += 3;  // skip count, min and max
                continue;
            }

            // buffer = {{num objs, min depth, max depth, obj name, ...}, ...}

            // find top-of-stack and its parent
            obj = (Object*)buffer[n + num_objs + 2];
            if (obj != NULL)
            {
                parent = find_top_level_parent(&object_tree, obj);
                if (parent == NULL)
                {
                    n += num_objs + 3;
                    continue;
                }

                // If parent is not already in the list, add it
                if (obj != NULL)
                    link_single_checked(parent, &selection);

#ifdef DEBUG_PICK_ALL
                if (view_debug)
                {
                    len = sprintf_s(p, size, "(%d) objs %d min %x max %x: ", n, buffer[n], buffer[n + 1], buffer[n + 2]);
                    p += len;
                    size -= len;
                    n += 3;
                    for (j = 0; j < num_objs; j++)
                    {
                        Object* obj = (Object*)buffer[n];

                        if (obj == NULL)
                            len = sprintf_s(p, size, "NULL ");
                        else
                            len = sprintf_s(p, size, "%s%d ", obj_prefix[obj->type], obj->ID);
                        p += len;
                        size -= len;
                        n++;
                    }
                    len = sprintf_s(p, size, "\r\n");
                    Log(buf);
                    p = buf;
                    size = 512;
                }
                else  // not logging, still need to skip the data
#endif
                {
                    n += num_objs + 3;
                }
            }
            else
            {
                n += num_objs + 3;
            }
        }

#ifdef DEBUG_PICK
        if (view_debug)
        {
            len = sprintf_s(p, size, "Select buffer used: %d\r\n", n);
            Log(buf);
        }
#endif
    }
#endif // 0

