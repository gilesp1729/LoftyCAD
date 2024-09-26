// Definitions for the geometry functions.

#ifndef __GEOM_H__
#define __GEOM_H__

#define PI 3.14159265359
#define RAD 57.2957795131
#define RADF 57.29577f

// A large impossible coordinate value
#define LARGE_COORD 999999

// A small coordinate value for testing
#define SMALL_COORD 0.000001

// test for "near" zero.
#define nz(val)  (fabs(val) < SMALL_COORD)

// test for near points (within the snapping tolerance, or the small coord tolerance)
// DEBUG: Flag nearness comparisons that pass with tol, but fail with (tol * tol)
#define DEBUG_NEAR_PT_TOL
#ifdef DEBUG_NEAR_PT_TOL
BOOL near_pt(Point* p1, Point* p2, double tol);
#else
#define near_pt(p1, p2, tol) \
    (   \
        fabs((p1)->x - (p2)->x) < tol  \
        &&  \
        fabs((p1)->y - (p2)->y) < tol  \
        &&  \
        fabs((p1)->z - (p2)->z) < tol  \
    )
#endif

// This one is only used for text placement.
#define near_pt_xyz(p1, x1, y1, z1, tol) \
    (   \
        fabs((p1)->x - (x1)) < tol  \
        &&  \
        fabs((p1)->y - (y1)) < tol  \
        &&  \
        fabs((p1)->z - (z1)) < tol  \
    )

// Tensions for bezier control points used in bez rect and circle faces.
// 
// The circle tension is for a quarter-circle bezier approximation
// as a fraction of the radius.
// = for an included angle a, (4/3) * tan(a/4)
// = for a circle divied into n equal parts, (4/3) * tan(pi/2n) 
// = for n=4, (4/3) * tan(pi/8) ~ 0.55228
#define BEZ_QTR_RADIUS_TENSION 0.55228475f

// The tension of 0.333333 as a fraction of the endpoint-to-endpoint distance
// is used when the included angle is small, and we are calculating the
// tensions automatically. (the tension factor converges to this as the
// angle --> 0)
#define BEZ_RECT_TENSION 0.333333f
#define BEZ_DEFAULT_TENSION 0.333333f

// prototypes.
void ray_from_eye(GLint x, GLint y, Plane *line);
BOOL intersect_ray_plane(GLint x, GLint y, Plane *picked_plane, Point *new_point);
int intersect_line_plane(Plane *line, Plane *plane, Point *new_point);
int intersect_segment_plane(double x0, double y0, double z0, double x1, double y1, double z1,
                            Plane* plane, Point* new_point);
double distance_point_plane(Plane *plane, Point *p);
BOOL snap_ray_edge(GLint x, GLint y, Edge *edge, Point *new_point);
double dist_ray_to_edge(Plane* v, Edge* edge, Point* new_point);
double dist_ray_to_segment(Plane* v, Point* p1, Point* p2, Point* new_point);
double dist_point_to_edge(Point *P, Edge *S);
double dist_point_to_perp_line(Point* P, Edge* S, Point* Pb);
double dist_point_to_perp_plane(Point* P, Plane* S, Point* Pb);
double dist_point_to_perp_planeref(Point* P, PlaneRef* S, Point* Pb);
double dist_point_to_ray(Point* P, Plane* v, Point* Pb);
void normal_list(Point *list, Plane *norm);
void polygon_normal(Point* list, Plane* norm);
BOOL polygon_planar(Point* list, Plane* norm);
BOOL normal3(Point *b, Point *a, Point *c, Plane *norm);
double angle3(Point *b, Point *a, Point *c, Plane *n);
void mat_mult_by_row(double *m, double *v, double *res);
void mat_mult_by_col_d(double *m, double *v, double *res);

double dot(double x0, double y0, double z0, double x1, double y1, double z1);
double pdot(Point *p1, Point *p2);
double pldot(Plane *p1, Plane *p2);
double length(Point* p0, Point* p1);
double length_squared(Point* p0, Point* p1);
double area_triangle(Point* a, Point* b, Point* c);
void cross(double x0, double y0, double z0, double x1, double y1, double z1, double*xc, double*yc, double*zc);
void pcross(Point *p1, Point *p2, Point *cp);
void plcross(Plane *p1, Plane *p2, Plane *cp);
BOOL normalise_point(Point *p);
BOOL normalised(Plane* p);
BOOL normalise_plane(Plane* p);

void new_length(Point* p0, Point* p1, double len);
void new_length_mid(Point* p0, Point* p1, double len);

void snap_to_grid(Plane *plane, Point *point, BOOL inhibit_snapping);
void snap_to_scale(double*length, BOOL inhibit_snapping);
char *display_rounded(char *buf, double val);
void snap_2d_angle(double x0, double y0, double*x1, double*y1, int angle_tol);
double cleanup_angle_and_snap(double angle, BOOL snap_to_45);
void snap_to_angle(Plane *plane, Point *p0, Point *p1, int angle_tol);
BOOL centre_3pt_circle(Point *p1, Point *p2, Point *p3, Plane *pl, Point *centre, BOOL *clockwise);
BOOL centre_2pt_tangent_circle(Point *p1, Point *p2, Point *p, Plane *pl, Point *centre, BOOL *clockwise);
void look_at_centre_d(Point c, Point p1, Plane n, double matrix[16]);

#endif