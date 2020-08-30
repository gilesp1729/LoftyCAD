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

// Make an endcap circle at curr.  The normalised directions are given since
// we always have them (saving doing repeated sqrts to get them). If a direction is NULL, 
// it will be a closed endcap perpendicular to the edge.
void
endcap(Point2D* curr, Point2D *d0, Point2D *d1, float z, Endcap* cap)
{
    Point3D c[NPTS];
    int i;

    // X/Y offsets are subtracted here.
    float xoffset = (float)(bed_xmax - bed_xmin) / 2;
    float yoffset = (float)(bed_ymax - bed_ymin) / 2;

    // The endcap is a polygon, anti-clockwise as seen looking along direction d0.
    c[0].x = c[0].y = c[0].z = 0;
    for (i = 0; i < NPTS; i++)
    {
        c[i].x = -d0->y * xcap[i];




    }



    // The first point is on the ZPolyEdge.
    cap->pts[0].x = curr->x - xoffset;
    cap->pts[0].y = curr->y - yoffset;
    cap->pts[0].z = z;







}

// Put out faces (quads) between two endcaps.
void
tube(Endcap* e0, Endcap* e1)
{




}

// Put out faces for the line segments of the ZPolyEdge view_list. 
void
spaghetti(ZPolyEdge *zedge)
{
    int i;
    Endcap endcap0, endcap1;
    Point2D d0, d1;
    float bend;

    glBegin(GL_QUAD_STRIP);

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
        if (bend < 0)
        {
            endcap(&zedge->view_list[i], &d0, NULL, zedge->z, &endcap1);
            tube(&endcap0, &endcap1);
            endcap(&zedge->view_list[i + 1], &d1, NULL, zedge->z, &endcap0);
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

