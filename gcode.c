#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// G-code visualisation as a thin tube around the line segments of a ZPolyEdge.

// Number of points in the endcap circles. This must be even to take advantage of symmetry.
#define NPTS 6

// Circular (approximated) endcaps. The first point lies on the ZPolyEdge and the rest of the
// circle lies below, of diameter = layer_height.
typedef struct Endcap
{
    Point3D pts[NPTS];
} Endcap;

// Precalculated offsets of endcap points for different NPTS values, expressed as multiples
// of the layer height. The offsets are from point[0] in the XZ plane of the endcap.
#if NPTS == 6
float xcap[NPTS] = { 0, -0.5f, -0.5f, 0, 0.5f, 0.5f };
float zcap[NPTS] = { 0, -0.33333f, -0.66666f, -1, -0.66666f, -0.33333f };
#endif // NPTS




// 2D direction and dot product helpers
void
dirn(Point2D* p0, Point2D* p1, Point2D* d)
{
    float len;

    d->x = p1->x - p0->x;
    d->y = p1->y - p0->y;
    len = sqrtf(d->x * d->x + d->y * d->y);
    d->x /= len;
    d->y /= len;
}

float
dot2d(Point2D* d0, Point2D* d1)
{
    return d0->x * d1->x + d0->y * d1->y;
}

// Make an endcap circle at curr.  The line is in (normalised) direction d0. 
// The next line is in direction d1. If d1 is not NULL, the endcap is mitered
// between the two directions.
void
endcap(Point2D* curr, Point2D *d0, Point2D *d1, float z, Endcap* cap)
{
    Point3D c[NPTS];
    Point2D adj_d0;
    int i;
    float lensq, corr;

    // X/Y offsets are subtracted here.
    float xoffset = (float)(bed_xmax - bed_xmin) / 2;
    float yoffset = (float)(bed_ymax - bed_ymin) / 2;

    // The endcap is a polygon, anti-clockwise as seen looking along direction d0.
    if (d1 == NULL)
    {
        for (i = 0; i < NPTS; i++)
        {
            c[i].x = -d0->y * xcap[i];
            c[i].y = d0->x * xcap[i];
            c[i].z = zcap[i];
        }
    }
    else
    {
        // Adjust direction for the turn between d0 and d1. Cope with near-parallel cases.
        adj_d0.x = (d1->x - d0->x) / 2;
        adj_d0.y = (d1->y - d0->y) / 2;
        lensq = adj_d0.x * adj_d0.x + adj_d0.y * adj_d0.y;
        if (lensq < 0.01)
        {
            // Same direction or near - just use the standard endcap
            for (i = 0; i < NPTS; i++)
            {
                c[i].x = -d0->y * xcap[i];
                c[i].y = d0->x * xcap[i];
                c[i].z = zcap[i];
            }
        }
        else
        {
            // Adjust size of endcap to cope with increasing angle between the lines.
            // This works up to 90 degrees (protected by caller)
            corr = sqrtf(1 - lensq);
            lensq = sqrtf(lensq);
            adj_d0.x /= (lensq * corr);
            adj_d0.y /= (lensq * corr);
            for (i = 0; i < NPTS; i++)
            {
                c[i].x = adj_d0.x * xcap[i];
                c[i].y = adj_d0.y * xcap[i];
                c[i].z = zcap[i];
            }
        }
    }

    for (i = 0; i < NPTS; i++)
    {
        cap->pts[i].x = curr->x - xoffset + c[i].x * layer_height;
        cap->pts[i].y = curr->y - yoffset + c[i].y * layer_height;
        cap->pts[i].z = z + c[i].z * layer_height;
    }
}

// Put out faces (quads) between two endcaps.
void
tube(Endcap* e0, Endcap* e1)
{
    int i, j, o; 
    Plane norm;

    for (i = 0; i < NPTS; i++)
    {
        j = i + 1;          // next point
        if (j == NPTS)
            j = 0;

        o = i - (NPTS / 2) + 1;     // opposite point for facet normal
        if (o < 0)
            o += NPTS;

        norm.A = e0->pts[i].x - e0->pts[o].x;
        norm.B = e0->pts[i].y - e0->pts[o].y;
        norm.C = e0->pts[i].z - e0->pts[o].z;
        normalise_plane(&norm);
        glNormal3f(norm.A, norm.B, norm.C);

        glVertex3f(e0->pts[i].x, e0->pts[i].y, e0->pts[i].z);
        glVertex3f(e0->pts[j].x, e0->pts[j].y, e0->pts[j].z);
        glVertex3f(e1->pts[j].x, e1->pts[j].y, e1->pts[j].z);
        glVertex3f(e1->pts[i].x, e1->pts[i].y, e1->pts[i].z);
    }
}


// Put out faces for the line segments of the ZPolyEdge view_list. 
void
spaghetti(ZPolyEdge *zedge)
{
    int i;
    Endcap endcap0, endcap1;
    Point2D d0, d1;
    float bend;

    glBegin(GL_QUADS);

    for (i = 1; i < zedge->n_view; i++)
    {
        Point2D* p = &zedge->view_list[i];

        // Get the direction cosines of the current and next segments
        dirn(&zedge->view_list[i - 1], &zedge->view_list[i], &d0);
        if (i == 1)  // start the tube off
            endcap(&zedge->view_list[0], &d0, NULL, zedge->z, &endcap0);

        if (i == zedge->n_view - 1)
        {
            // We have arrived at the end. Finish the last tube.
            endcap(&zedge->view_list[i], &d0, NULL, zedge->z, &endcap1);
            tube(&endcap0, &endcap1);
            break;
        }

        dirn(&zedge->view_list[i], &zedge->view_list[i + 1], &d1);
        bend = dot2d(&d0, &d1);

        // Calculate the next endcap. If the included angle between this segment and the next
        // is less than 90, close off the tube and start again (to prevent miter spikes)

        if (bend < -0.01)
        {
            endcap(&zedge->view_list[i], &d0, NULL, zedge->z, &endcap1);
            tube(&endcap0, &endcap1);
            endcap(&zedge->view_list[i], &d1, NULL, zedge->z, &endcap0);
        }
        else
        {
            endcap(&zedge->view_list[i], &d0, &d1, zedge->z, &endcap1);
            tube(&endcap0, &endcap1);
            endcap0 = endcap1;
        }
    }

    glEnd();
}

