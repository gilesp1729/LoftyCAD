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
        if (!nz(face1->normal.A - face->normal.A))
            return NULL;
        if (!nz(face1->normal.B - face->normal.B))
            return NULL;
        if (!nz(face1->normal.C - face->normal.C))
            return NULL;

        dx = face->normal.refpt.x - face1->normal.refpt.x;
        dy = face->normal.refpt.y - face1->normal.refpt.y;
        dz = face->normal.refpt.z - face1->normal.refpt.z;
        if (fabsf(face1->normal.A * dx + face1->normal.B * dy + face1->normal.C * dz) > tolerance)
            return NULL;

        // If we got through that, now test if face and face1 overlap
        for (i = 0; i < face->n_view; i++)
        {
            if (point_in_polygon2D(face->view_list2D[i], face1->view_list2D, face1->n_view))
                return obj;  // this point is in
        }

        return NULL;    // no points found, polygons do not overlap

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (f = vol->faces; f != NULL; f = (Face *)f->hdr.next)
        {
            Object *test = find_in_neighbourhood_face(face, (Object *)f);

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
find_in_neighbourhood(Object *match_obj)
{
    Object *obj, *ret_obj = NULL;

    if (match_obj == NULL)
        return NULL;

    for (obj = object_tree; obj != NULL; obj = obj->next)
    {
        Object *test = NULL;

        switch (match_obj->type)
        {
        case OBJ_POINT:
            test = find_in_neighbourhood_point((Point *)match_obj, obj);
            break;

        case OBJ_FACE:
            test = find_in_neighbourhood_face((Face *)match_obj, obj);
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

