#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// generation of clipped view lists, including clipping to volumes


// Returns TRUE if the bboxes do NOT intersect.
BOOL
bbox_out(Volume *vol1, Volume *vol2)
{
    if (vol1->bbox.xmin > vol2->bbox.xmax)
        return TRUE;
    if (vol1->bbox.xmax < vol2->bbox.xmin)
        return TRUE;

    if (vol1->bbox.ymin > vol2->bbox.ymax)
        return TRUE;
    if (vol1->bbox.ymax < vol2->bbox.ymin)
        return TRUE;

    if (vol1->bbox.zmin > vol2->bbox.zmax)
        return TRUE;
    if (vol1->bbox.zmax < vol2->bbox.zmin)
        return TRUE;

    return FALSE;
}

// Determine intersection of a face(t) to a collection of volumes in a group (Phase 1)
void
gen_view_list_clipped_tree1(Face *face, Point *facet, Group *tree)
{
    Object *o;
    Volume *vol;
    Face *f;

    for (o = tree->obj_list; o != NULL; o = o->next)
    {
        switch (o->type)
        {
        case OBJ_VOLUME:
            vol = (Volume *)o;

            // Don't self-intersect
            if (face->vol == vol)
                break;

            // Make sure the raw view lists for the vol are up to date
            // (this will usualy return quickly, but we have to do it)
            for (f = vol->faces; f != NULL; f = (Face *)f->hdr.next)
                gen_view_list_face(f, FALSE);

            gen_view_list_clipped1(face, facet, (Volume *)o);
            break;

        case OBJ_GROUP:
            gen_view_list_clipped_tree1(face, facet, (Group *)o);
            break;
        }
    }
}

// Determine intersection of a face(t) to a volume. Phase 1: Generate the polygon, if any,
// and append it to the face's spare list. Once all such polygons are generated,
// we can clip the face(t) to their union as appropriate.
void 
gen_view_list_clipped1(Face *face, Point *facet, Volume *vol)
{

    // No bbox intersection - nothing to do
    if (bbox_out(face->vol, vol))
        return;
}

// Phase 2: Clip a facet to its collected intersections stored in the face's spare list,
// and put the results in the clipped view list.
void
gen_view_list_clipped2(Face *face)
{
}
