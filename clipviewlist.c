#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// generation of clipped view lists, including clipping to volumes

// Tessellator for rendering to mesh.
GLUtesselator *clip_tess = NULL;

// count of vertices received so far in the polygon
int clip_tess_count;

// count of triangles output so far in the polygon
int clip_tess_tri_count;

// Points stored for the next triangle
Point clip_tess_points[3];

// What kind of triangle sequence is being output (GL_TRIANGLES, TRIANGLE_STRIP or TRIANGLE_FAN)
GLenum clip_tess_sequence;

// add one triangle to the volume surface
void
clip_tess_write(void * polygon_data)
{
    Face *face = (Face *)polygon_data;
    Mesh *mesh = face->vol->mesh;
    Vertex_index *v[3];
    Face_index *fi;
    Point *np;
    int i;

#ifdef CHECK_ZERO_AREA
    ASSERT(area_triangle(&clip_tess_points[0], &clip_tess_points[1], &clip_tess_points[2]) > SMALL_COORD, "Zero area triangle!!");
#endif

    // If the points have not been seen before, search the point bucket for uses
    // from prevous faces in the volume. If all else fails, make new mesh vertices for them.
    for (i = 0; i < 3; i++)
    {
        if (clip_tess_points[i].vi == NULL)
        {
            BOOL found = FALSE;
            Point **b = find_bucket(&clip_tess_points[i], face->vol->point_bucket);

            for (np = *b; np != NULL; np = np->bucket_next)
            {
                if (near_pt(np, &clip_tess_points[i], SMALL_COORD))
                {
                    found = TRUE;
                    break;
                }
            }

            if (found)
            {
                v[i] = np->vi;
            }
            else
            {
                double tx, ty, tz;

                // Transform these coords before adding vertex to mesh. Clip_tess_pts is untransformed.
                transform_list_xyz(&xform_list, clip_tess_points[i].x, clip_tess_points[i].y, clip_tess_points[i].z, &tx, &ty, &tz);
                mesh_add_vertex(mesh, tx, ty, tz, &v[i]);
                clip_tess_points[i].vi = v[i];

                // Copy the point with its coordinates, and stash it in the point bucket.
                np = point_newpv(&clip_tess_points[i]);
                np->vi = v[i];
                np->bucket_next = *b;
                *b = np;
            }
        }
        else
        {
            v[i] = clip_tess_points[i].vi;
        }
    }

    mesh_add_face(mesh, &v[0], &v[1], &v[2], &fi);
    clip_tess_tri_count++;
}

// callbacks for exporting tessellated stuff to a mesh 
void
clip_tess_beginData(GLenum type, void * polygon_data)
{
    clip_tess_sequence = type;
    clip_tess_count = 0;
    clip_tess_tri_count = 0;
}

void
clip_tess_vertexData(void * vertex_data, void * polygon_data)
{
    Point *v = (Point *)vertex_data;

    if (clip_tess_count < 3)
    {
        clip_tess_points[clip_tess_count++] = *v;
    }
    else
    {
        switch (clip_tess_sequence)
        {
        case GL_TRIANGLES:
            clip_tess_write(polygon_data);
            clip_tess_count = 0;
            clip_tess_points[clip_tess_count++] = *v;
            break;

        case GL_TRIANGLE_FAN:
            clip_tess_write(polygon_data);
            clip_tess_points[1] = clip_tess_points[2];
            clip_tess_points[2] = *v;
            break;

        case GL_TRIANGLE_STRIP:
            clip_tess_write(polygon_data);
            if (clip_tess_tri_count & 1)
                clip_tess_points[0] = clip_tess_points[2];
            else
                clip_tess_points[1] = clip_tess_points[2];
            clip_tess_points[2] = *v;
            break;
        }
    }
}

void
clip_tess_endData(void * polygon_data)
{
    // write out the last triangle
    if (clip_tess_count == 3)
        clip_tess_write(polygon_data);
}

void
clip_tess_combineData(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData, void * polygon_data)
{
    // Allocate a new Point for the new vertex, and (TODO:) hang it off the face's spare vertices list.
    // It will be freed when the view list is regenerated.
    Point *p = point_newv((float)coords[0], (float)coords[1], (float)coords[2]);

    *outData = p;
}

void clip_tess_errorData(GLenum errno, void * polygon_data)
{
    ASSERT(FALSE, "tesselator error");
}

// Initialise the tessellator
void
init_clip_tess(void)
{
    clip_tess = gluNewTess();
    gluTessCallback(clip_tess, GLU_TESS_BEGIN_DATA, (void(__stdcall *)(void))clip_tess_beginData);
    gluTessCallback(clip_tess, GLU_TESS_VERTEX_DATA, (void(__stdcall *)(void))clip_tess_vertexData);
    gluTessCallback(clip_tess, GLU_TESS_END_DATA, (void(__stdcall *)(void))clip_tess_endData);
    gluTessCallback(clip_tess, GLU_TESS_COMBINE_DATA, (void(__stdcall *)(void))clip_tess_combineData);
    gluTessCallback(clip_tess, GLU_TESS_ERROR_DATA, (void(__stdcall *)(void))clip_tess_errorData);
}

// Generate triangulated surface for the face and add it to its parent volume.
void
gen_view_list_surface(Face *face)
{
    Point *v, *vfirst;

    v = (Point *)face->view_list.head;
    {
        while (v != NULL)
        {
            if (v->flags == FLAG_NEW_FACET)
                v = (Point *)v->hdr.next;
            vfirst = v;
            gluTessBeginPolygon(clip_tess, face);
            gluTessBeginContour(clip_tess);
            while (VALID_VP(v))
            {
                if (v->flags == FLAG_NEW_CONTOUR)
                {
                    gluTessEndContour(clip_tess);
                    gluTessBeginContour(clip_tess);
                }

                tess_vertex(clip_tess, v);

                // Skip coincident points for robustness (don't create zero-area triangles)
                while (v->hdr.next != NULL && near_pt(v, (Point *)v->hdr.next, SMALL_COORD))
                    v = (Point *)v->hdr.next;

                v = (Point *)v->hdr.next;

                // If face(t) is closed, skip the closing point. Watch for dups at the end.
                while (v != NULL && near_pt(v, vfirst, SMALL_COORD))
                    v = (Point *)v->hdr.next;
            }
            gluTessEndContour(clip_tess);
            gluTessEndPolygon(clip_tess);
        }
    }
}

