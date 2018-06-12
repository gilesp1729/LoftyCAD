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

// add one triangle to the volume surface
void
gts_tess_write(void * polygon_data)
{
    Face *face = (Face *)polygon_data;
    GtsSurface *surface = face->vol->full_surface;
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
#if 0
    // This should work for GTS files, but it trips up on zero-width triangles...
    if (face->n_edges == 3)
    {
        // Special case for triangular faces: pass them through directly to GTS.
        gts_tess_points[0] = *v;
        gts_tess_points[1] = *(Point *)v->hdr.next;
        gts_tess_points[2] = *(Point *)v->hdr.next->next;
        gts_tess_write(face);
    }
    else
#endif
    {
        // use the tessellator
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
}

#ifdef DEBUG_HIGHLIGHTING_ENABLED
// Debugging routine to build a list of edges for the intersection curve.
void build_edge(gpointer data, gpointer user_data)
{
    GtsEdge *ge = GTS_EDGE(data);
    Volume *vol = (Volume *)user_data;
    Edge *e = edge_new(EDGE_STRAIGHT);

    e->endpoints[0] = point_new((float)ge->segment.v1->p.x, (float)ge->segment.v1->p.y, (float)ge->segment.v1->p.z);
    e->endpoints[0]->hdr.ID = 0;
    objid--;
    e->endpoints[1] = point_new((float)ge->segment.v2->p.x, (float)ge->segment.v2->p.y, (float)ge->segment.v2->p.z);
    e->endpoints[1]->hdr.ID = 0;
    objid--;
    link((Object *)e, (Object **)&vol->inter_edge_list);
}
#endif

// Stop on cost function for gts_coarsen. Designed to weed out near-coincident points
// without really altering the surface much.
BOOL
stop_on_cost(double cost, unsigned int nedge, double *max_cost)
{
    if (cost > *max_cost)
        return TRUE;
    OutputDebugString("Cleaning up degenerate edge\n");
    return FALSE;
}

// Do the boolean operation on the GTS surfaces. Don't delete any surfaces (caller responsibility)
GtsSurface *
boolean_surfaces(GtsSurface *s1, GtsSurface *s2, BOOL_OPERATION operation, Volume *vol)
{
    GtsSurfaceInter *si;
    GNode *tree1, *tree2;
    BOOL closed, is_open1, is_open2;
    GtsSurface *s3 = NULL;
    double stop_cost = 4 * tolerance * tolerance;
    BOOL check;
    BOOL failed = FALSE;

    OutputDebugString("BBTrees\n");
    tree1 = gts_bb_tree_surface(s1);
    is_open1 = gts_surface_volume(s1) < 0. ? TRUE : FALSE;
    tree2 = gts_bb_tree_surface(s2);
    is_open2 = gts_surface_volume(s2) < 0. ? TRUE : FALSE;

    OutputDebugString("surface_inter_new\n");
    si = gts_surface_inter_new(gts_surface_inter_class(),
                               s1, s2, tree1, tree2, is_open1, is_open2);
    OutputDebugString("surface_inter_check\n");
    check = gts_surface_inter_check(si, &closed);
    if (!check)
    {
        ASSERT(FALSE, "Surface intersection curve is not orientable");
        failed = TRUE;
        goto cleanup;
    }
    if (!closed && si->edges)
    {
        ASSERT(FALSE, "Surface intersection curve has edges, but is not closed");
        failed = TRUE;
        goto cleanup;
    }

#ifdef DEBUG_HIGHLIGHTING_ENABLED
    if (debug_view_inter && vol != NULL)
    {
        // DEBUG create a bunch of edges from si->edges
        free_obj_list((Object *)vol->inter_edge_list);
        vol->inter_edge_list = NULL;
        g_slist_foreach(si->edges, (GFunc)build_edge, vol);
        vol->inter_closed = closed;
    }
#endif
    OutputDebugString("surface_new\n");
    s3 = gts_surface_new(gts_surface_class(), gts_face_class(), gts_edge_class(), gts_vertex_class());

    switch (operation)
    {
    case BOOL_UNION:                    // s1 union s2
        OutputDebugString("surface union\n");
        gts_surface_inter_boolean(si, s3, GTS_1_OUT_2);
        gts_surface_inter_boolean(si, s3, GTS_2_OUT_1);
        break;

    case BOOL_INTERSECTION:             // s1 intersection s2
        gts_surface_inter_boolean(si, s3, GTS_1_IN_2);
        gts_surface_inter_boolean(si, s3, GTS_2_IN_1);
        break;

    case BOOL_DIFFERENCE:               // s1 - s2
        gts_surface_inter_boolean(si, s3, GTS_1_OUT_2);
        gts_surface_inter_boolean(si, s3, GTS_2_IN_1);
        // TODO - this may not be needed, as s2 may have inward normals anyway
        //gts_surface_foreach_face(si->s2, (GtsFunc)gts_triangle_revert, NULL);
        //gts_surface_foreach_face(s2, (GtsFunc)gts_triangle_revert, NULL);
        break;

    default:                            // Pass the others through to GTS
        OutputDebugString("surface 1out2\n");
        gts_surface_inter_boolean(si, s3, operation);
        break;
    }

    OutputDebugString("Clean up degenerate triangles\n");
    gts_surface_coarsen(s3, NULL, NULL, NULL, NULL, stop_on_cost, &stop_cost, 0);

cleanup:
#ifdef DEBUG_CAPTURE_FAILED_SI
    if (failed)
    {
        FILE *fptr;
        // capture failed surfaces
        debug_surface_print_stats("s1", 0, s1);
        fopen_s(&fptr, "s1.gts", "wt");
        gts_surface_write(s1, fptr);
        fclose(fptr);
        debug_surface_print_stats("s2", 0, s2);
        fopen_s(&fptr, "s2.gts", "wt");
        gts_surface_write(s2, fptr);
        fclose(fptr);
        DebugBreak();
    }
#endif
    OutputDebugString("destroy bbtrees\n");
    gts_bb_tree_destroy(tree1, TRUE);
    gts_bb_tree_destroy(tree2, TRUE);
    OutputDebugString("destroy surface inter\n");
    gts_object_destroy(GTS_OBJECT(si));

    return s3;
}