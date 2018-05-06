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
face_shade(GLUtesselator *tess, Face *face, BOOL selected, BOOL highlighted, BOOL locked)
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
        gluTessBeginPolygon(tess, &face->normal);
        gluTessBeginContour(tess);
        for (v = face->view_list; v != NULL; v = (Point *)v->hdr.next)
            tess_vertex(tess, v);
        gluTessEndContour(tess);
        gluTessEndPolygon(tess);
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
            gluTessBeginPolygon(tess, &norm);
            gluTessBeginContour(tess);
            tess_vertex(tess, last);
            tess_vertex(tess, lprev);
            tess_vertex(tess, vnext);
            tess_vertex(tess, v);
            gluTessEndContour(tess);
            gluTessEndPolygon(tess);

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

// globals for STL file writing
// File to export STL to (global so callbacks can see it)
FILE *stl;

// count of vertices received so far in the polygon
int stl_count;

// count of triangles output so far in the polygon
int stl_tri_count;

// Points stored for the next triangle
Point stl_points[3];

// Normal for the current polygon
Plane stl_normal;

// What kind of triangle sequence is being output (GL_TRIANGLES, TRIANGLE_STRIP or TRIANGLE_FAN)
GLenum stl_sequence;

// Write a single triangle out to the STL file
void
stl_write(void)
{
    int i;

    fprintf_s(stl, "facet normal %f %f %f\n", stl_normal.A, stl_normal.B, stl_normal.C);
    fprintf_s(stl, "  outer loop\n");
    for (i = 0; i < 3; i++)
        fprintf_s(stl, "    vertex %f %f %f\n", stl_points[i].x, stl_points[i].y, stl_points[i].z);
    fprintf_s(stl, "  endloop\n");
    fprintf_s(stl, "endfacet\n");
}

// callbacks for exporting tessellated stuff to an STL file
void 
export_beginData(GLenum type, void * polygon_data)
{
    Plane *norm = (Plane *)polygon_data;

    stl_sequence = type;
    stl_normal = *norm;
    stl_count = 0;
    stl_tri_count = 0;
}

void 
export_vertexData(void * vertex_data, void * polygon_data)
{
    Point *v = (Point *)vertex_data;

    if (stl_count < 3)
    {
        stl_points[stl_count++] = *v;
    }
    else
    {
        switch (stl_sequence)
        {
        case GL_TRIANGLES:
            stl_write();
            stl_count = 0;
            break;

        case GL_TRIANGLE_FAN:
            stl_write();
            stl_points[1] = stl_points[2];
            stl_count = 2;
            break;

        case GL_TRIANGLE_STRIP:
            stl_write();
            if (stl_tri_count & 1)
                stl_points[0] = stl_points[2];
            else
                stl_points[1] = stl_points[2];
            stl_count = 2;
            break;
        }
    }
}

void 
export_endData(void * polygon_data)
{
}

void 
export_combineData(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData, void * polygon_data)
{
    // Allocate a new Point for the new vertex, and (TODO:) hang it off the face's spare vertices list.
    // It will be freed when the view list is regenerated.
    Point *p = point_new((float)coords[0], (float)coords[1], (float)coords[2]);
    p->hdr.ID = 0;
    objid--;

    *outData = p;
}

void export_errorData(GLenum errno, void * polygon_data)
{
    ASSERT(FALSE, "tesselator error");
}


// Render an volume or face object to triangles
void
export_object(GLUtesselator *tess, Object *obj)
{
    Face *face;

    switch (obj->type)
    {
    case OBJ_FACE:
        face = (Face *)obj;
        face_shade(tess, face, FALSE, FALSE, LOCK_NONE);
        break;

    case OBJ_VOLUME:
        for (face = ((Volume *)obj)->faces; face != NULL; face = (Face *)face->hdr.next)
            export_object(tess, (Object *)face);
        break;
    }
}

// Tessellate every solid object in the tree to triangles and export to STL
void
export_object_tree(Object *tree, char *filename)
{
    Object *obj;
    GLUtesselator *tess = gluNewTess();

    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (void(__stdcall *)(void))export_beginData);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void(__stdcall *)(void))export_vertexData);
    gluTessCallback(tess, GLU_TESS_END_DATA, (void(__stdcall *)(void))export_endData);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void(__stdcall *)(void))export_combineData);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void(__stdcall *)(void))export_errorData);

    fopen_s(&stl, filename, "wt");
    if (stl == NULL)
        return;
    fprintf_s(stl, "solid %s\n", curr_title);

    for (obj = tree; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_VOLUME)
            export_object(tess, obj);
    }

    fprintf_s(stl, "endsolid %s\n", curr_title);
    fclose(stl);

    gluDeleteTess(tess);
}
