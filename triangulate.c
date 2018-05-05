#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// triangulation

// callbacks for rendering tessellated stuff to GL
#if 0
void beginData(GLenum type, void * polygon_data);
void  vertexData(void * vertex_data, void * polygon_data);
void endData(void * polygon_data);
void combineData(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData, void * polygon_data);
void errorData(GLenum errno, void * polygon_data);
#endif


// callbacks for outputting triangles to an STL file




// Render an object to triangles
void
render_object(Object *obj)
{
}

// Render every solid object in the tree to triangles and display to GL
void
render_object_tree(Object *tree)
{
    Object *obj;
    GLUtesselator *tess = gluNewTess();

    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, render_beginData);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, render_vertexData);
    gluTessCallback(tess, GLU_TESS_END_DATA, render_endData);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, render_combineData);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA, render_errorData);

    for (obj = tree; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_VOLUME)
            render_object(obj);
    }

    gluDeleteTess(tess);
}

// Tessellate every solid object in the tree to triangles and export to STL
void
export_object_tree(Object *tree, char *filename)
{
}

#if 0

GLUtesselator *tess = gluNewTess();

gluTessCallback(tess, GLU_TESS_BEGIN_DATA, function);
gluTessCallback(tess, GLU_TESS_VERTEX_DATA, function);
gluTessCallback(tess, GLU_TESS_END_DATA, function);
gluTessCallback(tess, GLU_TESS_COMBINE_DATA, function);
gluTessCallback(tess, GLU_TESS_ERROR_DATA, function);


gluTesNormal(tess, A, B, C);
gluTessBeginPolygon(tess, some_data);
gluTessBeginContour(tess);
gluTessVertex(tess, coords, some_data);
....
gluTessEndContour(tess);
gluTessEndPolygon(tess);

or
gluNextContour(tess, type); // interior or exterior, ccw or cw

gluDeleteTess(tess);


#endif