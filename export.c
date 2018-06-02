#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// globals for STL file writing
// File to export STL to (global so callbacks can see it)
FILE *stl;

// Write a single GTS triangle out to the STL file
int
export_triangle(GtsTriangle *t)
{
    int i;
    GtsVertex *v[3];
    double A, B, C;
    Plane norm;

    gts_triangle_vertices(t, &v[0], &v[1], &v[2]);
    gts_triangle_normal(t, &A, &B, &C);
    norm.A = (float)A;
    norm.B = (float)B;
    norm.C = (float)C;
    normalise_plane(&norm);

    fprintf_s(stl, "facet normal %f %f %f\n", norm.A, norm.B, norm.C);
    fprintf_s(stl, "  outer loop\n");
    for (i = 0; i < 3; i++)
        fprintf_s(stl, "    vertex %f %f %f\n", v[i]->p.x, v[i]->p.y, v[i]->p.z);
    fprintf_s(stl, "  endloop\n");
    fprintf_s(stl, "endfacet\n");

    return 1;
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
        gts_surface_foreach_face(vol->vis_surface, (GtsFunc)export_triangle, NULL);
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
