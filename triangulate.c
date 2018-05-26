#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Triangulation of view lists, including clipping to volumes

// Tessellator for rendering to GL.
GLUtesselator *rtess;

// A large impossible coordinate value
#define LARGE_COORD 999999

// Clear a volume bounding box to empty
void
clear_bbox(Volume *vol)
{
    vol->bbox.xmin = LARGE_COORD;
    vol->bbox.xmax = -LARGE_COORD;
    vol->bbox.ymin = LARGE_COORD;
    vol->bbox.ymax = -LARGE_COORD;
    vol->bbox.zmin = LARGE_COORD;
    vol->bbox.zmax = -LARGE_COORD;
}

// Expand a volume bounding box to include a point
void
expand_bbox(Volume *vol, Point *p)
{
    if (vol == NULL)
        return;

    if (p->x < vol->bbox.xmin)
        vol->bbox.xmin = p->x;
    else if (p->x > vol->bbox.xmax)
        vol->bbox.xmax = p->x;

    if (p->y < vol->bbox.ymin)
        vol->bbox.ymin = p->y;
    else if (p->y > vol->bbox.ymax)
        vol->bbox.ymax = p->y;

    if (p->z < vol->bbox.zmin)
        vol->bbox.zmin = p->z;
    else if (p->z > vol->bbox.zmax)
        vol->bbox.zmax = p->z;
}

// Enforce constraints on an arc edge e, when one of its points p has been moved.
void
enforce_arc_constraints(Edge *e, Point *p, float dx, float dy, float dz)
{
    ArcEdge *ae = (ArcEdge *)e;

    if (p == e->endpoints[0])
    {
        // Endpoint 0 has moved, this will force a recalculation of the radius.
        // Move endpoint 1 to suit the new radius
        float rad = length(ae->centre, e->endpoints[0]);
        Plane e1;

        e1.A = e->endpoints[1]->x - ae->centre->x;
        e1.B = e->endpoints[1]->y - ae->centre->y;
        e1.C = e->endpoints[1]->z - ae->centre->z;
        normalise_plane(&e1);
        e->endpoints[1]->x = ae->centre->x + e1.A * rad;
        e->endpoints[1]->y = ae->centre->y + e1.B * rad;
        e->endpoints[1]->z = ae->centre->z + e1.C * rad;
    }
    else if (p == e->endpoints[1])
    {
        // The other endpoint has moved. Update the first one similarly
        float rad = length(ae->centre, e->endpoints[1]);
        Plane e0;

        e0.A = e->endpoints[0]->x - ae->centre->x;
        e0.B = e->endpoints[0]->y - ae->centre->y;
        e0.C = e->endpoints[0]->z - ae->centre->z;
        normalise_plane(&e0);
        e->endpoints[0]->x = ae->centre->x + e0.A * rad;
        e->endpoints[0]->y = ae->centre->y + e0.B * rad;
        e->endpoints[0]->z = ae->centre->z + e0.C * rad;
    }
    else if (p == ae->centre)
    {
        // The centre has moved. We are moving the whole edge.
        e->endpoints[0]->x += dx;
        e->endpoints[0]->y += dy;
        e->endpoints[0]->z += dz;
        e->endpoints[1]->x += dx;
        e->endpoints[1]->y += dy;
        e->endpoints[1]->z += dz;
    }
}

// Mark all view lists as invalid for parts of a parent object, when a part of it
// has moved.
//
// Do this all the way down (even though gen_view_list will recompute all the children anyway)
// since we may encounter arc edges that need constraints updated when, say, an 
// endpoint has moved. We also pass in how much it has moved.
void
invalidate_all_view_lists(Object *parent, Object *obj, float dx, float dy, float dz)
{
    Face *f;
    Object *o;
    int i;

    if (parent->type == OBJ_GROUP)
    {
        for (o = ((Group *)parent)->obj_list; o != NULL; o = o->next)
            invalidate_all_view_lists(o, obj, dx, dy, dz);
    }
    else if (parent->type == OBJ_VOLUME)
    {
        clear_bbox((Volume *)parent);
        for (f = ((Volume *)parent)->faces; f != NULL; f = (Face *)f->hdr.next)
            invalidate_all_view_lists((Object *)f, obj, dx, dy, dz);
    }
    else if (parent->type == OBJ_FACE)
    {
        f = (Face *)parent;
        f->view_valid = FALSE;
        for (i = 0; i < f->n_edges; i++)
            invalidate_all_view_lists((Object *)f->edges[i], obj, dx, dy, dz);
    }
    else if (parent->type == OBJ_EDGE)
    {
        switch (((Edge *)parent)->type & ~EDGE_CONSTRUCTION)
        {
        case EDGE_ARC:
            ((Edge *)parent)->view_valid = FALSE;

            // Check that the obj in question is an endpoint or the centre,
            // and update the other point(s) in the arc to suit.
            if (obj->type == OBJ_POINT)
            {
                enforce_arc_constraints((Edge *)parent, (Point *)obj, dx, dy, dz);
            }
            else if (obj->type == OBJ_EDGE)
            {
                enforce_arc_constraints((Edge *)parent, ((Edge *)obj)->endpoints[0], dx, dy, dz);
                enforce_arc_constraints((Edge *)parent, ((Edge *)obj)->endpoints[1], dx, dy, dz);
            }

            break;

        case EDGE_BEZIER:
            ((Edge *)parent)->view_valid = FALSE;
            break;
        }
    }
}

// Regenerate the unclipped view list for a face. While here, also calculate the outward
// normal for the face.
void
gen_view_list_face(Face *face)
{
    int i;
    Edge *e;
    Point *last_point;
    Point *p, *v;
    Object **list;
    //char buf[256];

    if (face->view_valid)
        return;

    free_view_list_face(face);

    // For flat faces, construct the view list directly, as there is only one facet. 
    // But for cylinders, we construct it in the spare list, and then rearrange it into 
    // the real view list to make facets.
    if (IS_FLAT(face))
        list = (Object **)&face->view_list;
    else
        list = (Object **)&face->spare_list;
    
    // Add points at tail of list, to preserve order
    // First the start point
    p = point_newp(face->initial_point);
    p->hdr.ID = 0;
    objid--;        // prevent explosion of objid's
    link_tail((Object *)p, list);
    expand_bbox(face->vol, p);

#if DEBUG_VIEW_LIST_RECT_FACE
    sprintf_s(buf, 256, "Face %d IP %d\r\n", face->hdr.ID, face->initial_point->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[0])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[0])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[1])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[1])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[2])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[2])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[3])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[3])->endpoints[1]->hdr.ID);
    Log(buf);
#endif
    last_point = face->initial_point;

    for (i = 0; i < face->n_edges; i++)
    {
        e = face->edges[i];

        // Then the subsequent points. Edges will follow in order, but their points
        // may be reversed.
        switch (e->type & ~EDGE_CONSTRUCTION)
        {
        case EDGE_STRAIGHT:
            if (last_point == e->endpoints[0])
            {
                last_point = e->endpoints[1];
            }
            else
            {
                ASSERT(last_point == e->endpoints[1], "Point order messed up");
                last_point = e->endpoints[0];
            }

            p = point_newp(last_point);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, list);
            expand_bbox(face->vol, p);
            break;

        case EDGE_ARC:
            gen_view_list_arc((ArcEdge *)e);
            goto copy_view_list;

        case EDGE_BEZIER:
            gen_view_list_bez((BezierEdge *)e);
        copy_view_list:
            if (last_point == e->endpoints[0])
            {
                last_point = e->endpoints[1];

                // copy the view list forwards. Skip the first point as it has already been added
                for (v = (Point *)e->view_list->hdr.next; v != NULL; v = (Point *)v->hdr.next)
                {
                    p = point_newp(v);
                    p->hdr.ID = 0;
                    objid--;
                    link_tail((Object *)p, list);
                    expand_bbox(face->vol, p);
                }
            }
            else
            {
                ASSERT(last_point == e->endpoints[1], "Point order messed up");
                last_point = e->endpoints[0];

                // copy the view list backwards, skipping the last point.
                for (v = (Point *)e->view_list; v->hdr.next->next != NULL; v = (Point *)v->hdr.next)
                    ;

                for (; v != NULL; v = (Point *)v->hdr.prev)
                {
                    p = point_newp(v);
                    p->hdr.ID = 0;
                    objid--;
                    link_tail((Object *)p, list);
                    expand_bbox(face->vol, p);
                }
            }

            p = point_newp(last_point);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, list);
            expand_bbox(face->vol, p);
            break;
        }
    }

    if (IS_FLAT(face))
    {
        // calculate the normal vector.  Store a new refpt here too, in case something has moved.
        polygon_normal(face->view_list, &face->normal);
        face->normal.refpt = *face->edges[0]->endpoints[0];

        // Update the 2D view list
        update_view_list_2D(face);
    }
    else 
    {
        Point *last = p;

        ASSERT((face->type & ~FACE_CONSTRUCTION) == FACE_CYLINDRICAL, "Only cylinder faces should be here");

        // Rearrange the cylinder view list into a set of facets, each with its own normal.
        for (i = 0, v = face->spare_list; v->hdr.next != NULL; v = (Point *)v->hdr.next, i++)
        {
            Point *vnext = (Point *)v->hdr.next;
            Point *lprev = (Point *)last->hdr.prev;
            Plane norm;

            // A new facet point containing the normal
            normal3(last, lprev, vnext, &norm);
            p = point_new(norm.A, norm.B, norm.C);
            p->hdr.ID = 0;
            objid--;
            p->flags = FLAG_NEW_FACET;
            link_tail((Object *)p, (Object **)&face->view_list);

            // Four points for the quad
            p = point_newp(v);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);
            p = point_newp(vnext);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);
            p = point_newp(lprev);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);
            p = point_newp(last);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);

            // Walk last backwards until we meet in the middle
            last = lprev;
            if (i >= face->edges[1]->nsteps)
                break;
        }

        // We are finished with the spare list for now, so free it
        free_view_list(face->spare_list);
        face->spare_list = NULL;
    }

    face->view_valid = TRUE;
}

void
update_view_list_2D(Face *face)
{
    int i;
    Point *v;

    // Update the 2D view list as seen from the facing plane closest to the face normal,
    // to facilitate quick point-in-polygon testing.
    if (!IS_FLAT(face))
        return;

    for (i = 0, v = face->view_list; v != NULL; v = (Point *)v->hdr.next, i++)
    {
        float a = fabsf(face->normal.A);
        float b = fabsf(face->normal.B);
        float c = fabsf(face->normal.C);

        if (c > b && c > a)
        {
            face->view_list2D[i].x = v->x;
            face->view_list2D[i].y = v->y;
        }
        else if (b > a && b > c)
        {
            face->view_list2D[i].x = v->x;
            face->view_list2D[i].y = v->z;
        }
        else
        {
            face->view_list2D[i].x = v->y;
            face->view_list2D[i].y = v->z;
        }

        if (i == face->n_alloc2D - 1)
        {
            face->n_alloc2D *= 2;
            face->view_list2D = realloc(face->view_list2D, face->n_alloc2D * sizeof(Point2D));
        }
    }
    face->view_list2D[i] = face->view_list2D[0];    // copy first point for fast poly testing
    face->n_view2D = i;
}

// Clean out a view list, by putting all the points on the free list.
// The points already have ID's of 0. 
void
free_view_list(Point *view_list)
{
    Point *p;

    if (free_list == NULL)
    {
        free_list = view_list;
    }
    else
    {
        for (p = free_list; p->hdr.next != NULL; p = (Point *)p->hdr.next)
            ;   // run down to the last free element
        p->hdr.next = (Object *)view_list;
    }
}

void
free_view_list_face(Face *face)
{
    free_view_list(face->view_list);
    free_view_list(face->spare_list);
    face->view_list = NULL;
    face->spare_list = NULL;
    face->view_valid = FALSE;
    face->n_view2D = 0;
}

void
free_view_list_edge(Edge *edge)
{
    free_view_list(edge->view_list);
    edge->view_list = NULL;
    edge->view_valid = FALSE;
}

// Generate the view list for an arc edge.
void
gen_view_list_arc(ArcEdge *ae)
{
    Edge *edge = (Edge *)ae;
    Plane n = ae->normal;
    Point *p;
    float rad = length(ae->centre, edge->endpoints[0]);
    float t, theta, step;
    float matrix[16];
    float v[4];
    float res[4];
    int i;

    if (edge->view_valid)
        return;

    free_view_list_edge(edge);

    // transform arc to XY plane, centre at origin, endpoint 0 on x axis
    look_at_centre(*ae->centre, *edge->endpoints[0], n, matrix);

    // angle between two vectors c-p0 and c-p1. If the points are the same, we are
    // drawing a full circle. (where "same" means coincident - they are always distinct structures)
    if (near_pt(edge->endpoints[0], edge->endpoints[1]))
        theta = ae->clockwise ? -2 * PI : 2 * PI;
    else
        theta = angle3(edge->endpoints[0], ae->centre, edge->endpoints[1], &n);

    // step for angle. This may be fixed in advance.
    if (edge->stepping && edge->nsteps > 0)
    {
        step = edge->stepsize;
    }
    else
    {
        step = 2.0f * acosf(1.0f - tolerance / rad);
        edge->stepsize = step;
    }
    i = 0;

    if (ae->clockwise)  // Clockwise angles go negative
    {
#ifdef DEBUG_VIEW_LIST_ARC
        Log("Clockwise arc:");
#endif
        if (theta > 0)
            theta -= 2 * PI;

        // draw arc from p1 (on x axis) to p2. 
        for (t = 0, i = 0; t > theta; t -= step, i++)
        {
            v[0] = rad * cosf(t);
            v[1] = rad * sinf(t);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            p = point_new(res[0], res[1], res[2]);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&edge->view_list);
#ifdef DEBUG_VIEW_LIST_ARC
            {
                char buf[64];
                sprintf_s(buf, 64, "%f %f %f\r\n", res[0], res[1], res[2]);
                Log(buf);
            }
#endif
        }
    }
    else
    {
#ifdef DEBUG_VIEW_LIST_ARC
        Log("Anticlockwise arc:");
#endif
        if (theta < 0)
            theta += 2 * PI;

        for (t = 0, i = 0; t < theta; t += step, i++)
        {
            v[0] = rad * cosf(t);
            v[1] = rad * sinf(t);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            p = point_new(res[0], res[1], res[2]);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&edge->view_list);
#ifdef DEBUG_VIEW_LIST_ARC
            {
                char buf[64];
                sprintf_s(buf, 64, "%f %f %f\r\n", res[0], res[1], res[2]);
                Log(buf);
            }
#endif
        }
    }

    edge->nsteps = i;

    // Make sure the last point is in the view list
    p = point_newp(edge->endpoints[1]);
    p->hdr.ID = 0;
    objid--;
    link_tail((Object *)p, (Object **)&edge->view_list);

    edge->view_valid = TRUE;
}

// Iterative bezier edge drawing.
void
iterate_bez
(
BezierEdge *be,
float x1, float y1, float z1,
float x2, float y2, float z2,
float x3, float y3, float z3,
float x4, float y4, float z4
)
{
    Point *p;
    Edge *e = (Edge *)be;
    float t;

    // the first point has already been output, so start at stepsize
    for (t = e->stepsize; t < 1.0001f; t += e->stepsize)
    {
        float mt = 1.0f - t;
        float c0 = mt * mt * mt;
        float c1 = 3 * mt * mt * t;
        float c2 = 3 * mt * t * t;
        float c3 = t * t * t;
        float x = c0 * x1 + c1 * x2 + c2 * x3 + c3 * x4;
        float y = c0 * y1 + c1 * y2 + c2 * y3 + c3 * y4;
        float z = c0 * z1 + c1 * z2 + c2 * z3 + c3 * z4;

        p = point_new(x, y, z);
        p->hdr.ID = 0;
        objid--;
        link_tail((Object *)p, (Object **)&e->view_list);
        e->nsteps++;
    }
}

// Length squared shortcut
#define LENSQ(x1, y1, z1, x2, y2, z2) ((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) + (z2-z1)*(z2-z1))

// Recursive bezier edge drawing.
void
recurse_bez
(
BezierEdge *be,
float x1, float y1, float z1,
float x2, float y2, float z2,
float x3, float y3, float z3,
float x4, float y4, float z4
)
{
    Point *p;
    Edge *e = (Edge *)be;

    // Calculate all the mid-points of the line segments
    float x12 = (x1 + x2) / 2;
    float y12 = (y1 + y2) / 2;
    float z12 = (z1 + z2) / 2;

    float x23 = (x2 + x3) / 2;
    float y23 = (y2 + y3) / 2;
    float z23 = (z2 + z3) / 2;

    float x34 = (x3 + x4) / 2;
    float y34 = (y3 + y4) / 2;
    float z34 = (z3 + z4) / 2;

    float x123 = (x12 + x23) / 2;
    float y123 = (y12 + y23) / 2;
    float z123 = (z12 + z23) / 2;

    float x234 = (x23 + x34) / 2;
    float y234 = (y23 + y34) / 2;
    float z234 = (z23 + z34) / 2;

    float x1234 = (x123 + x234) / 2;
    float y1234 = (y123 + y234) / 2;
    float z1234 = (z123 + z234) / 2;

    float x14 = (x1 + x4) / 2;
    float y14 = (y1 + y4) / 2;
    float z14 = (z1 + z4) / 2;

    // Do a length test < the grid unit, and a curve flatness test < the tolerance.
    // Test length squared (to save the sqrts)
    if
        (
        LENSQ(x1, y1, z1, x4, y4, z4) < grid_snap * grid_snap
        ||
        LENSQ(x1234, y1234, z1234, x14, y14, z14) < tolerance * tolerance
        )
    {
        // Add (x4, y4, z4) as a point to the view list
        p = point_new(x4, y4, z4);
        p->hdr.ID = 0;
        objid--;
        link_tail((Object *)p, (Object **)&e->view_list);
        e->nsteps++;
    }
    else
    {
        // Continue subdivision
        recurse_bez(be, x1, y1, z1, x12, y12, z12, x123, y123, z123, x1234, y1234, z1234);
        recurse_bez(be, x1234, y1234, z1234, x234, y234, z234, x34, y34, z34, x4, y4, z4);
    }
}

// Generate the view list for a bezier edge.
void
gen_view_list_bez(BezierEdge *be)
{
    Edge *e = (Edge *)be;
    Point *p;

    if (e->view_valid)
        return;

    free_view_list_edge(e);

    // Put the first endpoint on the view list
    p = point_newp(e->endpoints[0]);
    p->hdr.ID = 0;
    objid--;
    link_tail((Object *)p, (Object **)&e->view_list);

    // Perform fixed step division if stepsize > 0
    if (e->stepping && e->nsteps > 0)
    {
        e->nsteps = 0;
        iterate_bez
            (
            be,
            e->endpoints[0]->x, e->endpoints[0]->y, e->endpoints[0]->z,
            be->ctrlpoints[0]->x, be->ctrlpoints[0]->y, be->ctrlpoints[0]->z,
            be->ctrlpoints[1]->x, be->ctrlpoints[1]->y, be->ctrlpoints[1]->z,
            e->endpoints[1]->x, e->endpoints[1]->y, e->endpoints[1]->z
            );
    }
    else
    {
        // Subdivide the bezier
        e->nsteps = 0;
        recurse_bez
            (
            be,
            e->endpoints[0]->x, e->endpoints[0]->y, e->endpoints[0]->z,
            be->ctrlpoints[0]->x, be->ctrlpoints[0]->y, be->ctrlpoints[0]->z,
            be->ctrlpoints[1]->x, be->ctrlpoints[1]->y, be->ctrlpoints[1]->z,
            e->endpoints[1]->x, e->endpoints[1]->y, e->endpoints[1]->z
            );
        e->stepsize = 1.0f / e->nsteps;
    }

    e->view_valid = TRUE;
}


// callbacks for rendering tessellated stuff to GL
void 
render_beginData(GLenum type, void * polygon_data)
{
    Plane *norm = (Plane *)polygon_data;

    glNormal3f(norm->A, norm->B, norm->C);
    glBegin(type);
}

void 
render_vertexData(void * vertex_data, void * polygon_data)
{
    Point *v = (Point *)vertex_data;

    glVertex3f(v->x, v->y, v->z);
}

void 
render_endData(void * polygon_data)
{
    glEnd();
}

void 
render_combineData(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData, void * polygon_data)
{
    // Allocate a new Point for the new vertex, and (TODO:) hang it off the face's spare vertices list.
    // It will be freed when the view list is regenerated.
    Point *p = point_new((float)coords[0], (float)coords[1], (float)coords[2]);
    p->hdr.ID = 0;
    objid--;

    *outData = p;
}

void render_errorData(GLenum errno, void * polygon_data)
{
    ASSERT(FALSE, "tesselator error");
}

// Shortcut to pass a Point to gluTessVertex, both as coords and the vertex data pointer
void
tess_vertex(GLUtesselator *tess, Point *p)
{
    double coords[3] = { p->x, p->y, p->z };

    gluTessVertex(tess, coords, p);
}

// initialise tessellator for rendering
void
init_triangulator(void)
{
    rtess = gluNewTess();
    gluTessCallback(rtess, GLU_TESS_BEGIN_DATA, (void(__stdcall *)(void))render_beginData);
    gluTessCallback(rtess, GLU_TESS_VERTEX_DATA, (void(__stdcall *)(void))render_vertexData);
    gluTessCallback(rtess, GLU_TESS_END_DATA, (void(__stdcall *)(void))render_endData);
    gluTessCallback(rtess, GLU_TESS_COMBINE_DATA, (void(__stdcall *)(void))render_combineData);
    gluTessCallback(rtess, GLU_TESS_ERROR_DATA, (void(__stdcall *)(void))render_errorData);
    //gluTessProperty(rtess, ...);
}

// Shade in a face by triangulating its view list.
void
face_shade(GLUtesselator *tess, Face *face, BOOL selected, BOOL highlighted, BOOL locked)
{
    Point   *v;
    Plane norm;

    gen_view_list_face(face);

#ifdef DEBUG_FACE_SHADE
    Log("Face view list:\r\n");
    for (v = face->view_list; v->hdr.next != NULL; v = (Point *)v->hdr.next)
    {
        char buf[64];
        sprintf_s(buf, 64, "%d %f %f %f\r\n", v->flags, v->x, v->y, v->z);
        Log(buf);
    }
#endif       
    color(OBJ_FACE, face->type & FACE_CONSTRUCTION, selected, highlighted, locked);

    // If there are no facets, just use the face normal
    norm = face->normal;
    v = face->view_list;
    while (v != NULL)
    {
        if (v->flags == FLAG_NEW_FACET)
        {
            norm.A = v->x;
            norm.B = v->y;
            norm.C = v->z;
            v = (Point *)v->hdr.next;
        }
        gluTessBeginPolygon(tess, &norm);
        gluTessBeginContour(tess);
        while (v != NULL && v->flags != FLAG_NEW_FACET)
        {
            if (v->flags == FLAG_NEW_CONTOUR)
                gluNextContour(tess, GLU_UNKNOWN);  // TODO work out how best to get this type in here
            tess_vertex(tess, v);
            v = (Point *)v->hdr.next;
        }
        gluTessEndContour(tess);
        gluTessEndPolygon(tess);
    }
}

#if 0 // old code
    switch (face->type)
    {
    case FACE_RECT | FACE_CONSTRUCTION:
    case FACE_CIRCLE | FACE_CONSTRUCTION:
    case FACE_RECT:
    case FACE_CIRCLE:
    case FACE_FLAT:
        color(OBJ_FACE, face->type & FACE_CONSTRUCTION, selected, highlighted, locked);
     // This doesn't seem to work - put it in the begin callback
     //   gluTessNormal(rtess, face->normal.A, face->normal.B, face->normal.C);
        gluTessBeginPolygon(tess, &face->normal);
        gluTessBeginContour(tess);
        for (v = face->view_list; v != NULL; v = (Point *)v->hdr.next)
            tess_vertex(tess, v);
        gluTessEndContour(tess);
        gluTessEndPolygon(tess);
        break;

    case FACE_CYLINDRICAL:
        // These view lists need to be read from the bottom edge (forward)
        // and the top edge (backward) together, and formed into quads.
        for (last = face->view_list; last->hdr.next != NULL; last = (Point *)last->hdr.next)
            ;
#ifdef DEBUG_SHADE_CYL_FACE
        Log("Cyl view list:\r\n");
        for (v = face->view_list; v->hdr.next != NULL; v = (Point *)v->hdr.next)
        {
            char buf[64];
            sprintf_s(buf, 64, "%f %f %f\r\n", v->x, v->y, v->z);
            Log(buf);
        }
#endif       
        color(OBJ_FACE, FALSE, selected, highlighted, locked);
        for (i = 0, v = face->view_list; v->hdr.next != NULL; v = (Point *)v->hdr.next, i++)
        {
            Point *vnext = (Point *)v->hdr.next;
            Point *lprev = (Point *)last->hdr.prev;
            Plane norm;

            normal3(last, lprev, vnext, &norm);
            gluTessBeginPolygon(tess, &norm);
            gluTessBeginContour(tess);
            tess_vertex(tess, v);
            tess_vertex(tess, vnext);
            tess_vertex(tess, lprev);
            tess_vertex(tess, last);
            gluTessEndContour(tess);
            gluTessEndPolygon(tess);

            last = lprev;
            if (i >= face->edges[1]->nsteps)
                break;
        }
        break;

    case FACE_GENERAL:
        ASSERT(FALSE, "Draw face general not implemented");
        break;
    }
#endif // old code


