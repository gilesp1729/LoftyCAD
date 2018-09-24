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

// set 3x3 to identity
void
mat_set_ident_3x3(float *mat)
{
    mat[0] = 1.0f;
    mat[1] = 0.0f;
    mat[2] = 0.0f;
    mat[3] = 0.0f;
    mat[4] = 1.0f;
    mat[5] = 0.0f;
    mat[6] = 0.0f;
    mat[7] = 0.0f;
    mat[8] = 1.0f;
}

// copy 3x3
void
mat_copy_3x3(float *from, float *to)
{
    int i;
    for (i = 0; i < 9; i++)
        to[i] = from[i];
}

// multiply 3x3 on left by m, put result back into mat: mat = m * mat
void
mat_mult_3x3(float *m, float *mat)
{
    float res[9];

    res[0] = m[0] * mat[0] + m[1] * mat[3] + m[2] * mat[6];
    res[1] = m[0] * mat[1] + m[1] * mat[4] + m[2] * mat[7];
    res[2] = m[0] * mat[2] + m[1] * mat[5] + m[2] * mat[8];

    res[3] = m[3] * mat[0] + m[4] * mat[3] + m[5] * mat[6];
    res[4] = m[3] * mat[1] + m[4] * mat[4] + m[5] * mat[7];
    res[5] = m[3] * mat[2] + m[4] * mat[5] + m[5] * mat[8];

    res[6] = m[6] * mat[0] + m[7] * mat[3] + m[8] * mat[6];
    res[7] = m[6] * mat[1] + m[7] * mat[4] + m[8] * mat[7];
    res[8] = m[6] * mat[2] + m[7] * mat[5] + m[8] * mat[8];

    mat_copy_3x3(res, mat);
}

// Evaluate a transform matrix and characterise it. Build both forward and inverse matrices.
void
evaluate_transform(Transform *xform)
{
    float scale[9], rotate_x[9], rotate_y[9], rotate_z[9];
    float inv_scale[9], inv_rotate_x[9], inv_rotate_y[9], inv_rotate_z[9];

    // Start with the identity
    mat_set_ident_3x3(scale);
    mat_set_ident_3x3(rotate_x);
    mat_set_ident_3x3(rotate_y);
    mat_set_ident_3x3(rotate_z);
    mat_set_ident_3x3(inv_scale);
    mat_set_ident_3x3(inv_rotate_x);
    mat_set_ident_3x3(inv_rotate_y);
    mat_set_ident_3x3(inv_rotate_z);
    mat_set_ident_3x3(xform->mat);
    mat_set_ident_3x3(xform->inv_mat);

    xform->flags = 0;
    if (xform->enable_scale)
    {
        if (xform->sx != 1.0f || xform->sy != 1.0f || xform->sz != 1.0f)
        {
            xform->flags |= XF_SCALE_NONUNITY;
            scale[0] = xform->sx;
            scale[4] = xform->sy;
            scale[8] = xform->sz;
            inv_scale[0] = 1.0f / xform->sx;
            inv_scale[4] = 1.0f / xform->sy;
            inv_scale[8] = 1.0f / xform->sz;
        }
    }

    if (xform->enable_rotation)
    {
        if (xform->rx != 0.0f)
        {
            float cosrx = cosf(xform->rx / RADF);
            float sinrx = sinf(xform->rx / RADF);

            xform->flags |= XF_ROTATE_X;
            rotate_x[4] = cosrx;
            rotate_x[5] = -sinrx;
            rotate_x[7] = sinrx;
            rotate_x[8] = cosrx;
            inv_rotate_x[4] = cosrx;
            inv_rotate_x[5] = sinrx;
            inv_rotate_x[7] = -sinrx;
            inv_rotate_x[8] = cosrx;
        }

        if (xform->ry != 0.0f)
        {
            float cosry = cosf(xform->ry / RADF);
            float sinry = sinf(xform->ry / RADF);

            xform->flags |= XF_ROTATE_Y;
            rotate_y[0] = cosry;
            rotate_y[2] = sinry;
            rotate_y[6] = -sinry;
            rotate_y[8] = cosry;
            inv_rotate_y[0] = cosry;
            inv_rotate_y[2] = -sinry;
            inv_rotate_y[6] = sinry;
            inv_rotate_y[8] = cosry;
        }

        if (xform->rz != 0.0f)
        {
            float cosrz = cosf(xform->rz / RADF);
            float sinrz = sinf(xform->rz / RADF);

            xform->flags |= XF_ROTATE_Z;
            rotate_z[0] = cosrz;
            rotate_z[1] = -sinrz;
            rotate_z[3] = sinrz;
            rotate_z[4] = cosrz;
            inv_rotate_z[0] = cosrz;
            inv_rotate_z[1] = sinrz;
            inv_rotate_z[3] = -sinrz;
            inv_rotate_z[4] = cosrz;
        }
    }

    // Forward transform = RzRyRxS
    if (xform->flags & XF_SCALE_NONUNITY)
        mat_copy_3x3(scale, xform->mat);
    if (xform->flags & XF_ROTATE_X)
        mat_mult_3x3(rotate_x, xform->mat);
    if (xform->flags & XF_ROTATE_Y)
        mat_mult_3x3(rotate_y, xform->mat);
    if (xform->flags & XF_ROTATE_Z)
        mat_mult_3x3(rotate_z, xform->mat);

    // Inverse transform is the same, but multiply the inverse matrices in the reverse order
    if (xform->flags & XF_ROTATE_Z)
        mat_copy_3x3(inv_rotate_z, xform->inv_mat);
    if (xform->flags & XF_ROTATE_Y)
        mat_mult_3x3(inv_rotate_y, xform->inv_mat);
    if (xform->flags & XF_ROTATE_X)
        mat_mult_3x3(inv_rotate_x, xform->inv_mat);
    if (xform->flags & XF_SCALE_NONUNITY)
        mat_mult_3x3(inv_scale, xform->inv_mat);
}

// Apply a transform to an XYZ coordinate. It must have already been evaluated.
void
transform_xyz(Transform *xform, float x, float y, float z, float *tx, float *ty, float *tz)
{
    float *mat;

    if (xform->flags == 0)
    {
        *tx = x;
        *ty = y;
        *tz = z;
        return;                             // Nothing to see here
    }

    x -= xform->xc;                         // Bring xform centre to origin
    y -= xform->yc;
    z -= xform->zc;
    mat = xform->mat;

    // Optimise some common cases to save multiply-adds.
    switch (xform->flags)
    {
    case XF_SCALE_NONUNITY:                 // Just a scale
        *tx = (x * mat[0]) + xform->xc;
        *ty = (y * mat[4]) + xform->yc;
        *tz = (z * mat[8]) + xform->zc;
        break;

    //case XF_ROTATE_X:                       // Simple rotations about a single axis: 4 multiply-adds
    //    break; 

    // Add these in if found worth doing.
    //case XF_ROTATE_X | XF_SCALE_NONUNITY:   // Simple rotations with scale: 5 multiply-adds

    default:                                // Do a general 3x3 multiply by vector
        *tx = x * mat[0] + y * mat[1] + z * mat[2] + xform->xc;
        *ty = x * mat[3] + y * mat[4] + z * mat[5] + xform->yc;
        *tz = x * mat[6] + y * mat[7] + z * mat[8] + xform->zc;
        break;
    }
}

// Apply a list of transforms to an XYZ coordinate. We have to do them one at a time
// since they will have different centres.
void
transform_list_xyz(ListHead *xform_list, float x, float y, float z, float *tx, float *ty, float *tz)
{
    Transform *xf;

    *tx = x;
    *ty = y;
    *tz = z;
    for (xf = (Transform *)xform_list->head; xf != NULL; xf = (Transform *)xf->hdr.next)
    {
        transform_xyz(xf, x, y, z, tx, ty, tz);
        x = *tx;
        y = *ty;
        z = *tz;
    }
}
