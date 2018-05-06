#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// triangulation

// Tessellator for rendering to GL.
GLUtesselator *rtess;

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
face_shade(Face *face, BOOL selected, BOOL highlighted, BOOL locked)
{
    Point   *v, *last;
    int i;

    gen_view_list_face(face);

    switch (face->type)
    {
    case FACE_RECT:
    case FACE_CIRCLE:
    case FACE_FLAT:
#ifdef OLD_FACE_SHADE
        // Simple traverse of the face's view list.
        glBegin(GL_POLYGON);
        color(OBJ_FACE, selected, highlighted, locked);
        glNormal3f(face->normal.A, face->normal.B, face->normal.C);
        for (v = face->view_list; v != NULL; v = (Point *)v->hdr.next)
            glVertex3f(v->x, v->y, v->z);
        glEnd();
#else
        color(OBJ_FACE, selected, highlighted, locked);
     // This doesn't seem to work - put it in the begin callback
     //   gluTessNormal(rtess, face->normal.A, face->normal.B, face->normal.C);
        gluTessBeginPolygon(rtess, &face->normal);
        gluTessBeginContour(rtess);
        for (v = face->view_list; v != NULL; v = (Point *)v->hdr.next)
            tess_vertex(rtess, v);
        gluTessEndContour(rtess);
        gluTessEndPolygon(rtess);
#endif
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

#if 0 //def OLD_FACE_SHADE
        glBegin(GL_QUADS);
        color(OBJ_FACE, selected, highlighted, locked);
        for (v = face->view_list; v->hdr.next != NULL; v = (Point *)v->hdr.next)
        {
            Point *vnext = (Point *)v->hdr.next;
            Point *lprev = (Point *)last->hdr.prev;
            Plane norm;

            normal3(vnext, lprev, last, &norm);
            glNormal3f(norm.A, norm.B, norm.C);
            glVertex3f(last->x, last->y, last->z);
            glVertex3f(lprev->x, lprev->y, lprev->z);
            glVertex3f(vnext->x, vnext->y, vnext->z);
            glVertex3f(v->x, v->y, v->z);

            last = lprev;
        }
        glEnd();
#else
        color(OBJ_FACE, selected, highlighted, locked);
        for (i = 0, v = face->view_list; v->hdr.next != NULL; v = (Point *)v->hdr.next, i++)
        {
            Point *vnext = (Point *)v->hdr.next;
            Point *lprev = (Point *)last->hdr.prev;
            Plane norm;

            normal3(vnext, lprev, last, &norm);
            gluTessBeginPolygon(rtess, &norm);
            gluTessBeginContour(rtess);
            tess_vertex(rtess, last);
            tess_vertex(rtess, lprev);
            tess_vertex(rtess, vnext);
            tess_vertex(rtess, v);
            gluTessEndContour(rtess);
            gluTessEndPolygon(rtess);

            last = lprev;
            if (i >= face->edges[1]->nsteps)
                break;
        }
#endif
        break;

    case FACE_GENERAL:
        ASSERT(FALSE, "Draw face general not implemented");
        break;
    }
}


#if 0

// Render an object to triangles
void
render_object(Object *obj)
{
}

// Render every solid object in the tree to triangles and display to GL
void
render_object_tree(Object *tree)
{
}

// Tessellate every solid object in the tree to triangles and export to STL
void
export_object_tree(Object *tree, char *filename)
{
    Object *obj;
    GLUtesselator *tess = gluNewTess();

    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, export_beginData);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, export_vertexData);
    gluTessCallback(tess, GLU_TESS_END_DATA, export_endData);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, export_combineData);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA, export_errorData);

    for (obj = tree; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_VOLUME)
            render_object(obj);
    }

    gluDeleteTess(tess);
}
#endif
