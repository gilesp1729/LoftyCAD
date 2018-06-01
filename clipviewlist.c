#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// generation of clipped view lists, including clipping to volumes

// Tessellator for rendering to GTS surface.
GLUtesselator *gtess = NULL;

// count of vertices received so far in the polygon
int gts_tess_count;

// count of triangles output so far in the polygon
int gts_tess_tri_count;

// Points stored for the next triangle
Point gts_tess_points[3];

// What kind of triangle sequence is being output (GL_TRIANGLES, TRIANGLE_STRIP or TRIANGLE_FAN)
GLenum gts_tess_sequence;

#if 0
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
#endif

// add one triangle to the volume surface
void
gts_tess_write(void * polygon_data)
{
    Face *face = (Face *)polygon_data;
    GtsSurface *surface = face->vol->vis_surface;
    GtsFace *gf;
    GtsEdge *e[3];
    GtsVertex *v[3];
    Point *np;
    int i;

    // If the points have not been seen before, search the point bucket for uses
    // from prevous faces in the volume. If all else fails, make new GTS vertices for them.
    for (i = 0; i < 3; i++)
    {
        if (gts_tess_points[i].gts_object == NULL)
        {
            BOOL found = FALSE;

            for (np = face->vol->point_list; np != NULL; np = (Point *)np->hdr.next)
            {
                if (near_pt(np, &gts_tess_points[i]))
                {
                    found = TRUE;
                    break;
                }
            }

            if (found)
            {
                v[i] = GTS_VERTEX(np->gts_object);
            }
            else
            {
                v[i] = GTS_VERTEX(gts_object_new(GTS_OBJECT_CLASS(surface->vertex_class)));
                v[i]->p.x = gts_tess_points[i].x;
                v[i]->p.y = gts_tess_points[i].y;
                v[i]->p.z = gts_tess_points[i].z;
                gts_tess_points[i].gts_object = GTS_OBJECT(v[i]);

                // Copy the point with its coordinates, and stash it in the point bucket.
                np = point_newp(&gts_tess_points[i]);
                np->hdr.ID = 0;
                objid--;
                np->gts_object = GTS_OBJECT(v[i]);
                link((Object *)np, (Object **)&face->vol->point_list);
            }
        }
        else
        {
            v[i] = GTS_VERTEX(gts_tess_points[i].gts_object);
        }
    }

    // Search the edge bucket for existing edge entries and use them when they are found.
    for (i = 0; i < 3; i++)
    {
        GtsEdge *ge;
        BOOL found = FALSE;
        int inext = i + 1;

        if (inext == 3)
            inext = 0;

        for (np = face->vol->edge_list; np != NULL; np = (Point *)np->hdr.next)
        {
            ge = GTS_EDGE(np->gts_object);
            if (ge->segment.v1 == v[i] && ge->segment.v2 == v[inext])
            {
                found = TRUE;
                break;
            }
            if (ge->segment.v1 == v[inext] && ge->segment.v2 == v[i])
            {
                found = TRUE;
                break;
            }
        }

        if (found)
        {
            e[i] = ge;
        }
        else
        {
            e[i] = gts_edge_new(surface->edge_class, v[i], v[inext]);

            // Create a Point to contain the GTS edge (the coords are not used) and store in 
            // the edge bucket. We use Points here since they are quickly recycled through a
            // free list.

            // TODO make this robust against bad triangles (edges with 2 coincident vertices)
            np = point_new(0, 0, 0);
            np->hdr.ID = 0;
            objid--;
            np->gts_object = GTS_OBJECT(e[i]);
            link((Object *)np, (Object **)&face->vol->edge_list);
        }
    }

    gf = gts_face_new(surface->face_class, e[0], e[1], e[2]);
    gts_surface_add_face(surface, gf);
}

// callbacks for exporting tessellated stuff to a GTS surface
void
gts_tess_beginData(GLenum type, void * polygon_data)
{
    gts_tess_sequence = type;
    gts_tess_count = 0;
    gts_tess_tri_count = 0;
}

void
gts_tess_vertexData(void * vertex_data, void * polygon_data)
{
    Point *v = (Point *)vertex_data;

    if (gts_tess_count < 3)
    {
        gts_tess_points[gts_tess_count++] = *v;
    }
    else
    {
        switch (gts_tess_sequence)
        {
        case GL_TRIANGLES:
            gts_tess_write(polygon_data);
            gts_tess_count = 0;
            gts_tess_points[gts_tess_count++] = *v;
            break;

        case GL_TRIANGLE_FAN:
            gts_tess_write(polygon_data);
            gts_tess_points[1] = gts_tess_points[2];
            gts_tess_points[2] = *v;
            break;

        case GL_TRIANGLE_STRIP:
            gts_tess_write(polygon_data);
            if (gts_tess_tri_count & 1)
                gts_tess_points[0] = gts_tess_points[2];
            else
                gts_tess_points[1] = gts_tess_points[2];
            gts_tess_points[2] = *v;
            break;
        }
    }
}

void
gts_tess_endData(void * polygon_data)
{
    // write out the last triangle
    if (gts_tess_count == 3)
        gts_tess_write(polygon_data);
}

void
gts_tess_combineData(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData, void * polygon_data)
{
    // Allocate a new Point for the new vertex, and (TODO:) hang it off the face's spare vertices list.
    // It will be freed when the view list is regenerated.
    Point *p = point_new((float)coords[0], (float)coords[1], (float)coords[2]);
    p->hdr.ID = 0;
    objid--;

    *outData = p;
}

void gts_tess_errorData(GLenum errno, void * polygon_data)
{
    ASSERT(FALSE, "tesselator error");
}

// Initialise the tessellator
void
init_clip_tess(void)
{
    gtess = gluNewTess();
    gluTessCallback(gtess, GLU_TESS_BEGIN_DATA, (void(__stdcall *)(void))gts_tess_beginData);
    gluTessCallback(gtess, GLU_TESS_VERTEX_DATA, (void(__stdcall *)(void))gts_tess_vertexData);
    gluTessCallback(gtess, GLU_TESS_END_DATA, (void(__stdcall *)(void))gts_tess_endData);
    gluTessCallback(gtess, GLU_TESS_COMBINE_DATA, (void(__stdcall *)(void))gts_tess_combineData);
    gluTessCallback(gtess, GLU_TESS_ERROR_DATA, (void(__stdcall *)(void))gts_tess_errorData);
}

// Generate triangulated surface for the face and add it to its parent volume.
void
gen_view_list_surface(Face *face, Point *facet)
{
    Point *v, *vfirst;

    v = face->view_list;
    while (v != NULL)
    {
        if (v->flags == FLAG_NEW_FACET)
            v = (Point *)v->hdr.next;
        vfirst = v;
        gluTessBeginPolygon(gtess, face);
        gluTessBeginContour(gtess);
        while (VALID_VP(v))
        {
            tess_vertex(gtess, v);

            // Skip coincident points for robustness (don't create zero-area triangles)
            while (v->hdr.next != NULL && near_pt(v, (Point *)v->hdr.next))
                v = (Point *)v->hdr.next;

            v = (Point *)v->hdr.next;

            // If face(t) is closed, skip the closing point. Watch for dups at the end.
            while (v != NULL && near_pt(v, vfirst))
                v = (Point *)v->hdr.next;
        }
        gluTessEndContour(gtess);
        gluTessEndPolygon(gtess);
    }
}

