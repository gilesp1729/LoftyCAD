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

    // Check if the endpoints are hit first.
    if (parent_lock < LOCK_POINTS)
    {
        Object* test = pick_point(e->endpoints[0], parent_lock, line, dist);

        if (test != NULL)
            return test;
        test = pick_point(e->endpoints[1], parent_lock, line, dist);
        if (test != NULL)
            return test;
    }

    if (dist_ray_to_edge(line, e, &point) < snap_tol)
    {
        *dist = length(&line->refpt, &point);
        return (Object*)e;
    }
    return NULL;
}

Object* pick_face(Face* f, LOCK parent_lock, Plane* line, float* dist)
{
    Point point;
    Point2D pt;
    float a, b, c, dx, dy, dz;

    // If face is turning away, no need to consider it.
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
    if (intersect_line_plane(line, &f->normal , &point) > 0)
    {
        a = fabsf(f->normal.A);
        b = fabsf(f->normal.B);
        c = fabsf(f->normal.C);

        // make sure point is in plane first
        dx = point.x - f->normal.refpt.x;
        dy = point.y - f->normal.refpt.y;
        dz = point.z - f->normal.refpt.z;
        if (fabsf(a * dx + b * dy + c * dz) > snap_tol)
            return NULL;

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
            // Special case: if we are in STATE_NONE and we have a face on a fully locked volume,
            // just look straight through it so we can pick things behind it. However, we must still
            // be able to right-click things, so don't do it if force_pick is set TRUE.
            if
                (
                    !force_pick
                    &&
                    test->type == OBJ_FACE
                    &&
                    ((Face*)test)->vol != NULL
                    &&
                    ((Face*)test)->vol->hdr.lock >= LOCK_VOLUME
                    )
            {
                raw_picked_obj = test;
                test = NULL;
            }
            else
            {
                // Return the lowest priority object closest to the eye.
                if (ret_obj == NULL || test->type < ret_obj->type || dist < ret_dist)
                    ret_obj = test;
                if (ret_obj == NULL || test->type <= ret_obj->type)
                    ret_dist = dist;
            }
        }
    }

    // Some special cases:
    // If the object is in a group, then return the group.
    // If the object is a face, but it belongs to a volume that is locked at the
    // face or volume level, then return the parent volume instead.
    // Do the face test first, as if we have its volume we can find any parent
    // group quickly without a full search.
    if (ret_obj != NULL)
    {
        if (ret_obj->type == OBJ_FACE && ((Face*)ret_obj)->vol != NULL)
        {
            Face* face = (Face*)ret_obj;

            raw_picked_obj = ret_obj;
            if (face->vol->hdr.lock >= LOCK_FACES)
                ret_obj = (Object*)face->vol;

            if (face->vol->hdr.parent_group->hdr.parent_group != NULL)
                ret_obj = find_top_level_parent(&object_tree, (Object*)face->vol->hdr.parent_group);  // this is fast
            if (ret_obj->lock >= ret_obj->type && !force_pick)
                ret_obj = NULL; // this object is locked
        }
        else
        {
            Object* parent = find_top_level_parent(&object_tree, ret_obj);               // this is not so fast

            if (parent != NULL && parent->type == OBJ_GROUP)
                ret_obj = parent;
        }
    }

    return ret_obj;
}




#if 0
    GLuint buffer[512];
    GLint num_hits;
    GLuint min_obj = 0;
    OBJECT priority = OBJ_MAX;
    Object* obj = NULL;
    OBJECT min_priority = OBJ_MAX;
    BOOL edit_in_groups = FALSE;        // This is very problematical wrt. moving objects or sub-components.

    // Find the object under the cursor, if any
    glSelectBuffer(512, buffer);
    glRenderMode(GL_SELECT);
    Draw(TRUE, x_pick, y_pick, 3, 3);
    num_hits = glRenderMode(GL_RENDER);
    raw_picked_obj = NULL;

    // if (num_hits < 0)
    //     DebugBreak();
    if (num_hits > 0)
    {
        int n = 0;
        int i;
#ifdef DEBUG_PICK
        int j, len;
        char buf[512];
        size_t size = 512;
        char* p = buf;
        char* obj_prefix[] = { "N", "P", "E", "F", "V", "G" };
#endif
        GLuint min_depth = 0xFFFFFFFF;
        GLuint depth = 0xFFFFFFFF;

        for (i = 0; i < num_hits; i++)
        {
            int num_objs = buffer[n];

            if (num_objs == 0)
            {
                n += 3;         // skip count, min and max
                continue;       // see if the next hit has anything for us
            }

            // buffer = {{num objs, min depth, max depth, obj name, ...}, ...}

            // find top-of-stack and what kind of object it is
            obj = (Object*)buffer[n + num_objs + 2];
            if (obj == NULL)
            {
                // no object worth picking
                priority = OBJ_MAX;
            }
            else
            {
                priority = obj->type;
                depth = buffer[n + 1];

                // special case: if we are in STATE_NONE and we have a face on a fully locked volume,
                // just look straight through it so we can pick things behind it. However, we must still
                // be able to right-click things, so don't do it if force_pick is set TRUE.
                if
                    (
                        !force_pick
                        &&
                        obj->type == OBJ_FACE
                        &&
                        ((Face*)obj)->vol != NULL
                        &&
                        ((Face*)obj)->vol->hdr.lock >= LOCK_VOLUME
                        )
                {
                    raw_picked_obj = obj;
                    obj = NULL;
                    priority = OBJ_MAX;
                    depth = 0xFFFFFFFF;
                }
            }

            // store the lowest priority object, closest to the viewer
            if (priority < min_priority || depth < min_depth)
            {
                min_depth = buffer[n + 1];
                min_priority = priority;
                min_obj = (GLuint)obj; //  buffer[n + num_objs + 2];  // top of stack is last element in buffer
            }

#ifdef DEBUG_PICK_ALL
            if (view_debug)
            {
                len = sprintf_s(p, size, "objs %d min %x max %x: ", buffer[n], buffer[n + 1], buffer[n + 2]);
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

        // Some special cases:
        // If the object is in a group, then return the group.
        // If the object is a face, but it belongs to a volume that is locked at the
        // face or volume level, then return the parent volume instead.
        // Do the face test first, as if we have its volume we can find any parent
        // group quickly without a full search.
        obj = (Object*)min_obj;
        if (obj != NULL)
        {
            if (obj->type == OBJ_FACE && ((Face*)obj)->vol != NULL)
            {
                Face* face = (Face*)obj;

                raw_picked_obj = obj;
                if (face->vol->hdr.lock >= LOCK_FACES)
                    obj = (Object*)face->vol;

                if (!edit_in_groups && face->vol->hdr.parent_group->hdr.parent_group != NULL)
                    obj = find_top_level_parent(&object_tree, (Object*)face->vol->hdr.parent_group);  // this is fast
                if (obj->lock >= obj->type && !force_pick)
                    obj = NULL; // this object is locked
            }
            else
            {
                Object* parent = find_top_level_parent(&object_tree, obj);               // this is not so fast
                if (!edit_in_groups && parent != NULL && parent->type == OBJ_GROUP)
                    obj = parent;
            }
        }

#ifdef DEBUG_PICK
        if (view_debug)
        {
            if (obj == NULL)
                len = sprintf_s(p, size, "Picked: NULL\r\n");
            else
                len = sprintf_s(p, size, "Picked: %s%d\r\n", obj_prefix[obj->type], obj->ID);

            Log(buf);
        }
#endif
    }

    return obj;
#endif // 0


// Pick all top-level objects intersecting the given rect and select.
void
Pick_all_in_rect(GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick)
{


}



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

