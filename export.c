#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// globals for STL file writing
// File to export STL to (global so callbacks can see it)
FILE *stl;


// Write a single mesh triangle with normal out to the STL file
void
export_triangle(void *arg, float x[3], float y[3], float z[3])
{
    int i;
    float A, B, C, length;

    glBegin(GL_POLYGON);
    cross(x[1] - x[0], y[1] - y[0], z[1] - z[0], x[2] - x[0], y[2] - y[0], z[2] - z[0], &A, &B, &C);
    length = (float)sqrt(A * A + B * B + C * C);
    if (!nz(length))
    {
        A /= length;
        B /= length;
        C /= length;
        glNormal3d(A, B, C);
    }

    fprintf_s(stl, "facet normal %f %f %f\n", A, B, C);
    fprintf_s(stl, "  outer loop\n");
    for (i = 0; i < 3; i++)
        fprintf_s(stl, "    vertex %f %f %f\n", x[i], y[i], z[i]);
    fprintf_s(stl, "  endloop\n");
    fprintf_s(stl, "endfacet\n");
}

// Render an volume or face object to triangles
void
export_object(Object *obj)
{
    Object *o;
    Volume *vol;

    switch (obj->type)
    {
    case OBJ_VOLUME:
        vol = (Volume *)obj;
        // gen_view_list_vol(vol); // not necessary, as has already been done.
        mesh_foreach_face(((Volume *)obj)->mesh, export_triangle, NULL);
        break;

    case OBJ_GROUP:
        for (o = ((Group *)obj)->obj_list; o != NULL; o = o->next)
            export_object(o);
        break;
    }
}

// export every volume to STL
void
export_object_tree(Group *tree, char *filename)
{
    Object *obj;

    fopen_s(&stl, filename, "wt");
    if (stl == NULL)
        return;
    fprintf_s(stl, "solid %s\n", tree->title);

    for (obj = tree->obj_list; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_VOLUME || obj->type == OBJ_GROUP)
            export_object(obj);
    }

    fprintf_s(stl, "endsolid %s\n", tree->title);
    fclose(stl);
}


#ifdef OLD_EXPORT_CODE  // Old code to export direct from GL tessellator

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
    stl_tri_count++;
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
            stl_points[stl_count++] = *v;
            break;

        case GL_TRIANGLE_FAN:
            stl_write();
            stl_points[1] = stl_points[2];
            stl_points[2] = *v;
            break;

        case GL_TRIANGLE_STRIP:
            stl_write();
            if (stl_tri_count & 1)
                stl_points[0] = stl_points[2];
            else
                stl_points[1] = stl_points[2];
            stl_points[2] = *v;
            break;
        }
    }
}

void
export_endData(void * polygon_data)
{
    // write out the last triangle
    if (stl_count == 3)
        stl_write();
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
    Object *o;

    switch (obj->type)
    {
    case OBJ_FACE:
        face = (Face *)obj;
        gen_view_list_face(face);
        face_shade(tess, face, FALSE, FALSE, LOCK_NONE);
        break;

    case OBJ_VOLUME:
        for (face = ((Volume *)obj)->faces; face != NULL; face = (Face *)face->hdr.next)
            export_object(tess, (Object *)face);
        break;

    case OBJ_GROUP:
        for (o = ((Group *)obj)->obj_list; o != NULL; o = o->next)
            export_object(tess, o);
        break;
    }
}

// Tessellate every solid object in the tree to triangles and export to STL
void
export_object_tree(Group *tree, char *filename)
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
    fprintf_s(stl, "solid %s\n", tree->title);

    for (obj = tree->obj_list; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_VOLUME || obj->type == OBJ_GROUP)
            export_object(tess, obj);
    }

    fprintf_s(stl, "endsolid %s\n", tree->title);
    fclose(stl);

    gluDeleteTess(tess);
}

#endif


