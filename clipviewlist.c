#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// generation of clipped view lists, including clipping to volumes

// Returns TRUE if the volume's bbox crosses the face's plane. There are
// 8 points to test, but some components can be skipped if a component of
// the normal is zero, and some points can be skipped if the normal is
// axis-aligned.
BOOL
bbox_in_plane(Plane *norm, Volume *vol)
{
    if (!nz(norm->A))
    {
    }
    // TODO sim. B, C

    return TRUE;
}

// Clip a face to a group
void
gen_view_list_clipped_tree(Face *face, Group *tree)
{
    Object *o;

    for (o = tree->obj_list; o != NULL; o = o->next)
    {
        switch (o->type)
        {
        case OBJ_VOLUME:
            gen_view_list_clipped(face, (Volume *)o);
            break;

        case OBJ_GROUP:
            gen_view_list_clipped_tree(face, (Group *)o);
            break;
        }
    }
}

// Clip a face to a volume. The face may have multiple facets; each is clipped separately
// and the results added into the clipped view list.
void 
gen_view_list_clipped(Face *face, Volume *vol)
{
}