#include "stdafx.h"
#include "LoftyCAD.h"

// geometry functions

// Find a ray through the viewable frustum from the given window location. The XYZ and ABC of the
// passed Plane struct define a point and direction vector of the ray.
void
ray_from_eye(GLint x, GLint y, Plane *line)
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
// Return FALSE if there was no reasonable intersection.
BOOL
intersect_ray_plane(GLint x, GLint y, Plane *picked_plane, Point *new_point)
{
    Plane line;

    ray_from_eye(x, y, &line);

    return intersect_line_plane(&line, picked_plane, new_point) > 0;
}

// Intersect a ray through a mouse (window) coordinate with a line (edge).
// Find the nearest point on an edge to the ray, and return that.
// (the mouse is snapped to the edge). Return FALSE if no point can be found.
// Algorithm and notation from D. Sunday, "Distance between Lines", http://geomalgorithms.com/a07-_distance.html
BOOL
snap_ray_edge(GLint x, GLint y, Edge *edge, Point *new_point)
{
    Plane u, v, w0;
    float a, b, c, d, e, sc, denom;

    if (edge->type != EDGE_STRAIGHT)
        return FALSE; 

    // Express the lines in point/direction form (as Plane structs, for easy dotting later)
    u.refpt = *edge->endpoints[0];
    u.A = edge->endpoints[1]->x - edge->endpoints[0]->x;
    u.B = edge->endpoints[1]->y - edge->endpoints[0]->y;
    u.C = edge->endpoints[1]->z - edge->endpoints[0]->z;
    ray_from_eye(x, y, &v);
    w0.refpt = v.refpt;
    w0.A = u.refpt.x - v.refpt.x;
    w0.B = u.refpt.y - v.refpt.y;
    w0.C = u.refpt.z - v.refpt.z;

    // calculate the dot products used in the solution for the closest points.
    a = pldot(&u, &u);
    b = pldot(&u, &v);
    c = pldot(&v, &v);
    d = pldot(&u, &w0);
    e = pldot(&v, &w0);
    denom = a * c - b * b;
    if (nz(denom))
        return FALSE;       // lines are parallel
   
    // Solve for the closest point on line u (the edge passed in). We don't care about the
    // other closest point, or the distance between them here.
    sc = (b * e - c * d) / denom;
    //tc = (a * e - b * d) / denom;   // the other point on line v

    new_point->x = u.refpt.x + sc * u.A;
    new_point->y = u.refpt.y + sc * u.B;
    new_point->z = u.refpt.z + sc * u.C;

    return TRUE;
}

// Find the shortest distance from a point to an edge.
// Algorithm and notation from D.Sunday, "Lines and Distance of a Point to a Line", http://geomalgorithms.com/a02-_lines.html
float
dist_point_to_edge(Point *P, Edge *S)
{
    Point v, w, Pb;
    float c1, c2, b;

    //  Vector v = S.P1 - S.P0;
    //  Vector w = P - S.P0;
    v.x = S->endpoints[1]->x - S->endpoints[0]->x;
    v.y = S->endpoints[1]->y - S->endpoints[0]->y;
    v.z = S->endpoints[1]->z - S->endpoints[0]->z;
    w.x = P->x - S->endpoints[0]->x;
    w.y = P->y - S->endpoints[0]->y;
    w.z = P->z - S->endpoints[0]->z;

    c1 = pdot(&w, &v);
    if (c1 <= 0)
        return length(P, S->endpoints[0]);

    c2 = pdot(&v, &v);
    if (c2 <= c1)
        return length(P, S->endpoints[1]);

    b = c1 / c2;

    //Point Pb = S.P0 + b * v;
    Pb.x = S->endpoints[0]->x + b * v.x;
    Pb.y = S->endpoints[0]->y + b * v.y;
    Pb.z = S->endpoints[0]->z + b * v.z;

    return length(P, &Pb);
}

// Intersect a line with a plane (the line is represented as a Plane struct)
// (ref: Wikipedia, Line-plane intersection, vector form, and/or Sunday)
// No test is made for endpoints of the line.
// Returns: 1 - intersects, 0 - no intersection, -1 - line lies in the plane
int
intersect_line_plane(Plane *line, Plane *plane, Point *new_point)
{
    float Ldotn, dpdotn, d;
    Point dp;

    Ldotn = plane->A * line->A + plane->B * line->B + plane->C * line->C;
    dp.x = plane->refpt.x - line->refpt.x;
    dp.y = plane->refpt.y - line->refpt.y;
    dp.z = plane->refpt.z - line->refpt.z;

    dpdotn = dp.x * plane->A + dp.y * plane->B + dp.z * plane->C;
    if (fabsf(Ldotn) < SMALL_COORD)
    {
        if (fabsf(dpdotn) < SMALL_COORD)    // the line lines in the plane
            return -1;
        else
            return 0;                       // they do not intersect
    }

    d = dpdotn / Ldotn;
    new_point->x = d * line->A + line->refpt.x;
    new_point->y = d * line->B + line->refpt.y;
    new_point->z = d * line->C + line->refpt.z;

    return 1;
}

// Return the (signed) distance between a point and a plane
float
distance_point_plane(Plane *plane, Point *p)
{
    return
        plane->A * (p->x - plane->refpt.x)
        +
        plane->B * (p->y - plane->refpt.y)
        +
        plane->C * (p->z - plane->refpt.z);
}

// Dot and cross products given separate components.
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

// normal from 3 separate points. Returns FALSE if not defined.
BOOL
normal3(Point *b, Point *a, Point *c, Plane *norm)
{
    Point cp;
    float length;

    cross(b->x - a->x, b->y - a->y, b->z - a->z, c->x - a->x, c->y - a->y, c->z - a->z, &cp.x, &cp.y, &cp.z);
    length = (float)sqrt(cp.x * cp.x + cp.y * cp.y + cp.z * cp.z);
    if (nz(length))
        return FALSE;

    norm->A = cp.x / length;
    norm->B = cp.y / length;
    norm->C = cp.z / length;
    norm->refpt.x = a->x;
    norm->refpt.y = a->y;
    norm->refpt.z = a->z;
    return TRUE;
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

float
length_squared(Point *p0, Point *p1)
{
    float x0 = p0->x;
    float y0 = p0->y;
    float z0 = p0->z;
    float x1 = p1->x;
    float y1 = p1->y;
    float z1 = p1->z;

    return (x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0) + (z1 - z0)*(z1 - z0);
}

// Normal of a 3D polygon expressed as a list of Points
void
polygon_normal(Point *list, Plane *norm)
{
    Point *p;
    Point *first = list;
    Point cp;

    norm->A = 0;
    norm->B = 0;
    norm->C = 0;
    for (p = list; p->hdr.next != NULL; p = (Point *)p->hdr.next)
    {
        Point *q = (Point *)p->hdr.next;

        pcross(p, q, &cp);
        norm->A += cp.x;
        norm->B += cp.y;
        norm->C += cp.z;
    }
    pcross(p, first, &cp);
    norm->A += cp.x;
    norm->B += cp.y;
    norm->C += cp.z;

    normalise_plane(norm);
}

// multiply a 4x4 by 4-vector 
void
mat_mult_by_row(float *m, float *v, float *res)
{
    res[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3] * v[3];
    res[1] = m[4] * v[0] + m[5] * v[1] + m[6] * v[2] + m[7] * v[3];
    res[2] = m[8] * v[0] + m[9] * v[1] + m[10] * v[2] + m[11] * v[3];
    res[3] = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15] * v[3];
}

// multiply a 4x4 by a 4-vector by column. Use doubles as the precision is needed 
// for generating view list points for arcs.
void
mat_mult_by_col_d(double *m, double *v, double *res)
{
    res[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12] * v[3];
    res[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13] * v[3];
    res[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
    res[3] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];
}

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

// Ensure plane's A,B,C are of unit length. Return FALSE if the length is zero.
BOOL
normalise_plane(Plane *p)
{
    float length = (float)sqrt(p->A * p->A + p->B * p->B + p->C * p->C);

    if (nz(length))
        return FALSE;

    p->A = p->A / length;
    p->B = p->B / length;
    p->C = p->C / length;
    return TRUE;
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

// Ensure vector (represented as a Point) is of unit length. Return FALSE if the length is zero.
BOOL
normalise_point(Point *p)
{
    float length = (float)sqrt(p->x * p->x + p->y * p->y + p->z * p->z);

    if (nz(length))
        return FALSE;

    p->x = p->x / length;
    p->y = p->y / length;
    p->z = p->z / length;
    return TRUE;
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

// Find the centre of a circle tangent to two lines p-p1 and p-p2, passing through p1 and p2.
// Return the sense of the arc from p1 to p2.
// From https://stackoverflow.com/questions/39235049/find-center-of-circle-defined-by-2-points-and-their-tangent-intersection
BOOL
centre_2pt_tangent_circle(Point *p1, Point *p2, Point *p, Plane *pl, Point *centre, BOOL *clockwise)
{
    Point mid;
    float lsq, dmsq, coeff;
    Plane pp1, pp2, pp1xpp2;

    mid.x = (p1->x + p2->x) / 2;
    mid.y = (p1->y + p2->y) / 2;
    mid.z = (p1->z + p2->z) / 2;

    lsq = length_squared(p, p1);
    dmsq = length_squared(p, &mid);
    if (nz(dmsq))
        return FALSE;
    coeff = lsq / dmsq;

    centre->x = p->x - coeff * (p->x - mid.x);
    centre->y = p->y - coeff * (p->y - mid.y);
    centre->z = p->z - coeff * (p->z - mid.z);

    pp1.A = p1->x - centre->x;
    pp1.B = p1->y - centre->y;
    pp1.C = p1->z - centre->z;
    pp2.A = p2->x - centre->x;
    pp2.B = p2->y - centre->y;
    pp2.C = p2->z - centre->z;
    plcross(&pp1, &pp2, &pp1xpp2);
    *clockwise = pldot(pl, &pp1xpp2) < 0;

    return TRUE;
}

// Translates point c to the origin, then maps c-p1 and n vectors onto XY plane.
// Returns 4x4 matrix which needs to be post multiplied to the modelview.
void
look_at_centre_d(Point c, Point p1, Plane n, double matrix[16])
{
    Plane pp1, nxpp1;

    pp1.A = p1.x - c.x;
    pp1.B = p1.y - c.y;
    pp1.C = p1.z - c.z;
    normalise_plane(&pp1);
    plcross(&n, &pp1, &nxpp1);          // TODO! make these all use double.

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

// make the line p0-p1 a new length of len, by moving p1.
void
new_length(Point *p0, Point *p1, float len)
{
    Point v;

    v.x = p1->x - p0->x;
    v.y = p1->y - p0->y;
    v.z = p1->z - p0->z;
    normalise_point(&v);
    p1->x = p0->x + v.x * len;
    p1->y = p0->y + v.y * len;
    p1->z = p0->z + v.z * len;
}

// Allocate the point bucket list structure.
Point ***
init_buckets(void)
{
    Point ***bl = calloc(n_buckets, sizeof(Point **));
    int i;

    for (i = 0; i < n_buckets; i++)
        bl[i] = calloc(n_buckets, sizeof(Point *));

    return bl;
}

// Find a bucket header for a given point. The buckets are bottom-inclusive, top-exclusive in X and Y.
Point *
find_bucket(Point *p, Point ***bucket)
{
    int bias = n_buckets / 2;
    int i = (int)floor(p->x / bucket_size) + bias;
    int j = (int)floor(p->y / bucket_size) + bias;
    Point **bh;
    
    if (i < 0)
        i = 0;
    else if (i >= n_buckets)
        i = n_buckets - 1;
    bh = bucket[i];

    if (j < 0)
        j = 0;
    else if (j >= n_buckets)
        j = n_buckets - 1;
    
    return bh[j];
}

// Clear a bucket structure to empty, but don't free anything.
void empty_bucket(Point ***bucket)
{
    int i, j;

    for (i = 0; i < n_buckets; i++)
    {
        Point **bh = bucket[i];

        for (j = 0; j < n_buckets; j++)
            bh[j] = NULL;
    }
}

// Free all the Points a bucket references, and clear the buckets to empty.
void free_bucket_points(Point ***bucket)
{
    int i, j;
    Point *p, *nextp;

    for (i = 0; i < n_buckets; i++)
    {
        Point **bh = bucket[i];

        for (j = 0; j < n_buckets; j++)
        {
            for (p = bh[j]; p != NULL; p = nextp)
            {
                nextp = p->bucket_next;
                p->hdr.next = (Object *)free_list_pt;
                free_list_pt = p;
            }
            bh[j] = NULL;
        }
    }
}

// Free a bucket structure, and all the Points it references.
void free_bucket(Point ***bucket)
{
    int i, j;
    Point *p, *nextp;

    for (i = 0; i < n_buckets; i++)
    {
        Point **bh = bucket[i];

        for (j = 0; j < n_buckets; j++)
        {
            for (p = bh[j]; p != NULL; p = nextp)
            {
                nextp = p->bucket_next;
                p->hdr.next = (Object *)free_list_pt;
                free_list_pt = p;
            }
            bh[j] = NULL;
        }
        free(bucket[i]);
    }
    free(bucket);
}
