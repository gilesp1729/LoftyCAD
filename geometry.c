#include "stdafx.h"
#include "LoftyCAD.h"

// geometry functions

// Find a ray through the viewable frustum from the given window location. The XYZ and ABC of the
// passed Plane struct define a point and direction vector of the ray.
void
ray(GLint x, GLint y, Plane *line)
{
    GLdouble modelMatrix[16], projMatrix[16], nearp[3], farp[3];
    GLint    viewport[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);
    gluUnProject(x, viewport[3] - y, 0, modelMatrix, projMatrix, viewport, &nearp[0], &nearp[1], &nearp[2]);
    gluUnProject(x, viewport[3] - y, 1, modelMatrix, projMatrix, viewport, &farp[0], &farp[1], &farp[2]);
    line->refpt.x = (float)nearp[0];
    line->refpt.y = (float)nearp[1];
    line->refpt.z = (float)nearp[2];
    line->A = (float)(farp[0] - nearp[0]);
    line->B = (float)(farp[1] - nearp[1]);
    line->C = (float)(farp[2] - nearp[2]);
}

// Intersect a ray through a mouse (window) coordinate with a plane.
// (ref: Wikipedia, Line-plane intersection, vector form)
// Return FALSE if there was no reasonable intersection.
BOOL
intersect_ray_plane(GLint x, GLint y, Plane *picked_plane, Point *new_point)
{
    Plane line;
    float Ldotn, dpdotn, d;
    Point dp;

    ray(x, y, &line);

    // Determine parallel
    Ldotn = picked_plane->A * line.A + picked_plane->B * line.B + picked_plane->C * line.C;
    if (fabsf(Ldotn) < 1.0e-6)
        return FALSE;   // they do not intersect, or line lies in plane

    dp.x = picked_plane->refpt.x - line.refpt.x;
    dp.y = picked_plane->refpt.y - line.refpt.y;
    dp.z = picked_plane->refpt.z - line.refpt.z;

    dpdotn = dp.x * picked_plane->A + dp.y * picked_plane->B + dp.z * picked_plane->C;
    d = dpdotn / Ldotn;

    new_point->x = d * line.A + line.refpt.x;
    new_point->y = d * line.B + line.refpt.y;
    new_point->z = d * line.C + line.refpt.z;

    return TRUE;
}

// Intersect a ray through a mouse (window) coordinate with a line (edge).
// Find the nearest point on an edge to the ray, and return that.
// (the mouse is snapped to the edge)
BOOL
snap_ray_edge(GLint x, GLint y, Edge *edge, Point *new_point)
{
    return TRUE;
}

#if 0 // not needed
// Find the area of a rect (the sign gives the direction of the normal vector)
float
rect_area2D(Point *p0, Point *p1, Point *p2, Point *p3)
{
    float area;

    area = p0->x * p1->y - p1->x * p0->y;
    area += p1->x * p2->y - p2->x * p1->y;
    area += p2->x * p3->y - p3->x * p2->y;
    area += p3->x * p0->y - p0->x * p3->y;

    return 0.5f * area;
}

// Find the area of a polygon (sign gives direction of normal)
float
polygon_area2D(Point *list)
{
    float area = 0;
    Point *p;
    Point *first = list;

    for (p = list; p->hdr.next != NULL; p = (Point *)p->hdr.next)
    {
        Point *q = (Point *)p->hdr.next;

        area += p->x * q->y - q->x * p->y;
    }
    area += p->x * first->y - first->x * p->y;

    return 0.5f * area;
}
#endif

#if 0 // Python code
#determinant of matrix a
def det(a) :
return a[0][0] * a[1][1] * a[2][2] + a[0][1] * a[1][2] * a[2][0] + a[0][2] * a[1][0] * a[2][1] - a[0][2] * a[1][1] * a[2][0] - a[0][1] * a[1][0] * a[2][2] - a[0][0] * a[1][2] * a[2][1]

#unit normal vector of plane defined by points a, b, and c
def unit_normal(a, b, c) :
x = det([[1, a[1], a[2]],
[1, b[1], b[2]],
[1, c[1], c[2]]])
y = det([[a[0], 1, a[2]],
[b[0], 1, b[2]],
[c[0], 1, c[2]]])
z = det([[a[0], a[1], 1],
[b[0], b[1], 1],
[c[0], c[1], 1]])
magnitude = (x**2 + y**2 + z**2)**.5
return (x / magnitude, y / magnitude, z / magnitude)

#dot product of vectors a and b
def dot(a, b) :
return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

#cross product of vectors a and b
def cross(a, b) :
x = a[1] * b[2] - a[2] * b[1]
y = a[2] * b[0] - a[0] * b[2]
z = a[0] * b[1] - a[1] * b[0]
return (x, y, z)

#area of polygon poly
def area(poly) :
if len(poly) < 3 : # not a plane - no area
    return 0

    total = [0, 0, 0]
    for i in range(len(poly)) :
        vi1 = poly[i]
        if i is len(poly) - 1 :
            vi2 = poly[0]
        else :
        vi2 = poly[i + 1]
        prod = cross(vi1, vi2)
        total[0] += prod[0]
        total[1] += prod[1]
        total[2] += prod[2]
        result = dot(total, unit_normal(poly[0], poly[1], poly[2]))
        return abs(result / 2)

#endif /* Python code */

float
dot(float x0, float y0, float z0, float x1, float y1, float z1)
{
    return x0*x1 + y0*y1 + z0*z1;
}

void
cross(float x0, float y0, float z0, float x1, float y1, float z1, float *xc, float *yc, float *zc)
{
    *xc = y0*z1 - z0*y1;
    *yc = z0*x1 - x0*z1;
    *zc = x0*y1 - y0*x1;
}

// Unit normal, assuming at least 3 points in list and they are not collinear
void
normal(Point *list, Plane *norm)
{
    Point *a = list;
    Point *b = (Point *)a->hdr.next;
    Point *c = (Point *)b->hdr.next;
    Point cp;
    float length;

    cross(b->x - a->x, b->y - a->y, b->z - a->z, c->x - a->x, c->y - a->y, c->z - a->z, &cp.x, &cp.y, &cp.z);
    length = (float)sqrt(cp.x * cp.x + cp.y * cp.y + cp.z * cp.z);
    // Don't check, as this happens normally during extrusion of zero-height rects
    //ASSERT(length > 0.0001, "Normal of collinear points");
    norm->A = cp.x / length;
    norm->B = cp.y / length;
    norm->C = cp.z / length;
    norm->refpt.x = a->x;
    norm->refpt.y = a->y;
    norm->refpt.z = a->z;
}

// normal from 3 separate points
void
normal3(Point *b, Point *a, Point *c, Plane *norm)
{
    Point cp;
    float length;

    cross(b->x - a->x, b->y - a->y, b->z - a->z, c->x - a->x, c->y - a->y, c->z - a->z, &cp.x, &cp.y, &cp.z);
    length = (float)sqrt(cp.x * cp.x + cp.y * cp.y + cp.z * cp.z);
    norm->A = cp.x / length;
    norm->B = cp.y / length;
    norm->C = cp.z / length;
    norm->refpt.x = a->x;
    norm->refpt.y = a->y;
    norm->refpt.z = a->z;
}

// Returns the angle at a, between b and c, relative to a normal n.
// The angle can be in [-pi, pi].
float
angle3(Point *b, Point *a, Point *c, Plane *n)
{
    float cosa = dot(b->x - a->x, b->y - a->y, b->z - a->z, c->x - a->x, c->y - a->y, c->z - a->z);
    float angle;
    Plane cp;

    cosa /= length(a, b);
    cosa /= length(a, c);
    angle = acosf(cosa);
    cross(b->x - a->x, b->y - a->y, b->z - a->z, c->x - a->x, c->y - a->y, c->z - a->z, &cp.A, &cp.B, &cp.C);
    if (pldot(n, &cp) < 0)
        angle = -angle;
    return angle;
}

float
length(Point *p0, Point *p1)
{
    float x0 = p0->x;
    float y0 = p0->y;
    float z0 = p0->z;
    float x1 = p1->x;
    float y1 = p1->y;
    float z1 = p1->z;

    return (float)sqrt((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0) + (z1 - z0)*(z1 - z0));
}

#if 0 // not needed
// Area of a 3D polygon, and its normal
float
polygon_area(Point *list)
{
    Point sum;
    Point *p;
    Point *first = list;
    Point cp;

    sum.x = 0;
    sum.y = 0;
    sum.z = 0;
    for (p = list; p->hdr.next != NULL; p = (Point *)p->hdr.next)
    {
        Point *q = (Point *)p->hdr.next;

        cross(p->x, p->y, p->z, q->x, q->y, q->z, &cp.x, &cp.y, &cp.z);
        sum.x += cp.x;
        sum.y += cp.y;
        sum.z += cp.z;
    }
    cross(p->x, p->y, p->z, first->x, first->y, first->z, &cp.x, &cp.y, &cp.z);
    sum.x += cp.x;
    sum.y += cp.y;
    sum.z += cp.z;

    return 0.5f * dot(sum.x, sum.y, sum.z, norm.x, norm.y, norm.z);
}

#endif

// multiply a 4x4 by 4-vector 
void
mat_mult_by_row(float *m, float *v, float *res)
{
    res[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3] * v[3];
    res[1] = m[4] * v[0] + m[5] * v[1] + m[6] * v[2] + m[7] * v[3];
    res[2] = m[8] * v[0] + m[9] * v[1] + m[10] * v[2] + m[11] * v[3];
    res[3] = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15] * v[3];
}

void
mat_mult_by_col(float *m, float *v, float *res)
{
    res[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12] * v[3];
    res[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13] * v[3];
    res[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
    res[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];
}

// test for "near" zero.
#define nz(val)  (fabsf(val) < 0.00001)


// Snap a point to the grid. It must lie in the given plane. If the plane is
// not axis aligned, we can't snap anything (it would move out of plane)
void
snap_to_grid(Plane *plane, Point *point)
{
    if (nz(plane->A) && nz(plane->B))
    {
        snap_to_scale(&point->x);
        snap_to_scale(&point->y);
    }
    else if (nz(plane->B) && nz(plane->C))
    {
        snap_to_scale(&point->y);
        snap_to_scale(&point->z);
    }
    else if (nz(plane->A) && nz(plane->C))
    {
        snap_to_scale(&point->x);
        snap_to_scale(&point->z);
    }
}

// Snap p0-p1 to an angle snap tolerance. p1 may be moved.
// p1 must be snapped to the grid after calling this.
void
snap_to_angle(Plane *plane, Point *p0, Point *p1, int angle_tol)
{
    if (nz(plane->A) && nz(plane->B))
    {
        snap_2d_angle(p0->x, p0->y, &p1->x, &p1->y, angle_tol);
    }
    else if (nz(plane->B) && nz(plane->C))
    {
        snap_2d_angle(p0->y, p0->z, &p1->y, &p1->z, angle_tol);
    }
    else if (nz(plane->A) && nz(plane->C))
    {
        snap_2d_angle(p0->x, p0->z, &p1->x, &p1->z, angle_tol);
    }
}

// Snap a length to the grid snapping distance.
void
snap_to_scale(float *length)
{
    float snap;

    // This assumes grid scale and tolerance are powers of 10.
    if (snapping_to_grid)
        snap = grid_snap;
    else
        snap = tolerance;
    *length = roundf(*length / snap) * snap;
}

// Snap to an angle in 2D. angle_tol is in degrees. 
void
snap_2d_angle(float x0, float y0, float *x1, float *y1, int angle_tol)
{
    float length = (float)sqrt((*x1 - x0)*(*x1 - x0) + (*y1 - y0)*(*y1 - y0));
    int theta = (int)(atan2f(*y1 - y0, *x1 - x0) * 57.29577);
    int tol_half = angle_tol / 2;
    float theta_rad;

    // round theta to a multiple of angle_tol
    if (theta < 0)
        theta += 360;
    theta = ((theta + tol_half) / angle_tol) * angle_tol; 
    theta_rad = theta / 57.29577f;
    *x1 = x0 + length * cosf(theta_rad);
    *y1 = y0 + length * sinf(theta_rad);
}

// Display a coordinate or length, rounded to the tolerance.
// buf must be char[64]
char *
display_rounded(char *buf, float val)
{
    sprintf_s(buf, 64, "%.*f", tol_log, val); 
    return buf;
}

// Ensure plane's A,B,C are of unit length.
void
normalise_plane(Plane *p)
{
    float length = (float)sqrt(p->A * p->A + p->B * p->B + p->C * p->C);

    p->A = p->A / length;
    p->B = p->B / length;
    p->C = p->C / length;
}

// Dot and cross products between two planes.
float
pldot(Plane *p1, Plane *p2)
{
    return p1->A*p2->A + p1->B*p2->B + p1->C*p2->C;
}

void
plcross(Plane *p1, Plane *p2, Plane *cp)
{
    cross(p1->A, p1->B,p1->C, p2->A, p2->B, p2->C, &cp->A, &cp->B, &cp->C);
}

// Ensure vector (represented as a Point) is of unit length.
void
normalise_point(Point *p)
{
    float length = (float)sqrt(p->x * p->x + p->y * p->y + p->z * p->z);

    p->x = p->x / length;
    p->y = p->y / length;
    p->z = p->z / length;
}

// Dot and cross products between two vectors represented as Points.
float
pdot(Point *p1, Point *p2)
{
    return p1->x*p2->x + p1->y*p2->y + p1->z*p2->z;
}

void
pcross(Point *p1, Point *p2, Point *cp)
{
    cross(p1->x, p1->y, p1->z, p2->x, p2->y, p2->z, &cp->x, &cp->y, &cp->z);
}

// Find the centre of a circle, given 3 points. Returns FALSE if it can't.
// The 3 points are already known to lie in plane pl. The centre will too.
// From D. Sunday, "Intersection of Lines and Planes", http://geomalgorithms.com/a05-_intersect-1.html
BOOL
centre_3pt_circle(Point *p1, Point *p2, Point *p3, Plane *pl, Point *centre, BOOL *clockwise)
{
    Plane n1, n2, n3, n1xn2, n2xn3, n3xn1;
    float d1, d2, d3, denom;

    // Determine the 3 planes. Two of them bisect the lines p1-p2 and p2-p3.
    n1 = *pl;
    //normalise_plane(&n1);  // should not need this, it should already be normalised

    n2.refpt.x = (p1->x + p2->x) / 2;
    n2.refpt.y = (p1->y + p2->y) / 2;
    n2.refpt.z = (p1->z + p2->z) / 2;
    n2.A = p2->x - p1->x;
    n2.B = p2->y - p1->y;
    n2.C = p2->z - p1->z;
    normalise_plane(&n2);

    n3.refpt.x = (p2->x + p3->x) / 2;
    n3.refpt.y = (p2->y + p3->y) / 2;
    n3.refpt.z = (p2->z + p3->z) / 2;
    n3.A = p3->x - p2->x;
    n3.B = p3->y - p2->y;
    n3.C = p3->z - p2->z;
    normalise_plane(&n3);

    // Compute all the cross products
    plcross(&n1, &n2, &n1xn2);
    plcross(&n2, &n3, &n2xn3);
    plcross(&n3, &n1, &n3xn1);

    // test for near-parallel
    denom = pldot(&n1, &n2xn3);
    if (nz(denom))
        return FALSE;

    // compute the "D" terms in the plane equations (Ax + By + Cz + D = 0)
    // they are negated, as they are used negated in the centre calculation
    d1 = n1.A * n1.refpt.x + n1.B * n1.refpt.y + n1.C * n1.refpt.z;
    d2 = n2.A * n2.refpt.x + n2.B * n2.refpt.y + n2.C * n2.refpt.z;
    d3 = n3.A * n3.refpt.x + n3.B * n3.refpt.y + n3.C * n3.refpt.z;

    // Return the centre, and the sense of the arc (clockwise or a/clock, relative to 
    // the facing plane)
    centre->x = (d1 * n2xn3.A + d2 * n3xn1.A + d3 * n1xn2.A) / denom;
    centre->y = (d1 * n2xn3.B + d2 * n3xn1.B + d3 * n1xn2.B) / denom;
    centre->z = (d1 * n2xn3.C + d2 * n3xn1.C + d3 * n1xn2.C) / denom;
    *clockwise = denom < 0;

    return TRUE;
}

// Translates point c to the origin, then maps c-p1 and n vectors onto XY plane.
// Returns 4x4 matrix which needs to be post multiplied to the modelview.
void
look_at_centre(Point c, Point p1, Plane n, float matrix[16])
{
    Plane pp1, nxpp1;

    pp1.A = p1.x - c.x;
    pp1.B = p1.y - c.y;
    pp1.C = p1.z - c.z;
    normalise_plane(&pp1);
    plcross(&n, &pp1, &nxpp1);

    // set rotation part
    matrix[0] = pp1.A;
    matrix[1] = pp1.B;
    matrix[2] = pp1.C;
    matrix[3] = 0;
    matrix[4] = nxpp1.A;
    matrix[5] = nxpp1.B;
    matrix[6] = nxpp1.C;
    matrix[7] = 0;
    matrix[8] = n.A;
    matrix[9] = n.B;
    matrix[10] = n.C;
    matrix[11] = 0;

    // set translation part (unlike gluLookAt, bring C to the origin)
    matrix[12] = c.x;
    matrix[13] = c.y;
    matrix[14] = c.z;
    matrix[15] = 1;
}

