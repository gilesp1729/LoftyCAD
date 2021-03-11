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
#define nz(val)  (fabsf(val) < SMALL_COORD)

// test for near points (within the snapping tolerance, or the small coord tolerance)
#define near_pt(p1, p2, tol) \
    (   \
        fabsf((p1)->x - (p2)->x) < tol  \
        &&  \
        fabsf((p1)->y - (p2)->y) < tol  \
        &&  \
        fabsf((p1)->z - (p2)->z) < tol  \
    )
#define near_pt_xyz(p1, x1, y1, z1, tol) \
    (   \
        fabsf((p1)->x - (x1)) < tol  \
        &&  \
        fabsf((p1)->y - (y1)) < tol  \
        &&  \
        fabsf((p1)->z - (z1)) < tol  \
    )

void ray_from_eye(GLint x, GLint y, Plane *line);
BOOL intersect_ray_plane(GLint x, GLint y, Plane *picked_plane, Point *new_point);
int intersect_line_plane(Plane *line, Plane *plane, Point *new_point);
float distance_point_plane(Plane *plane, Point *p);
BOOL snap_ray_edge(GLint x, GLint y, Edge *edge, Point *new_point);
float dist_ray_to_edge(Plane* v, Edge* edge, Point* new_point);
float dist_ray_to_segment(Plane* v, Point* p1, Point* p2, Point* new_point);
float dist_point_to_edge(Point *P, Edge *S);
float dist_point_to_perp_line(Point* P, Edge* S, Point* Pb);
float dist_point_to_perp_plane(Point* P, Plane* S, Point* Pb);
float dist_point_to_ray(Point* P, Plane* v, Point* Pb);
void normal_list(Point *list, Plane *norm);
void polygon_normal(Point* list, Plane* norm);
BOOL polygon_planar(Point* list, Plane* norm);
BOOL normal3(Point *b, Point *a, Point *c, Plane *norm);
float angle3(Point *b, Point *a, Point *c, Plane *n);
void mat_mult_by_row(float *m, float *v, float *res);
void mat_mult_by_col_d(double *m, double *v, double *res);

float dot(float x0, float y0, float z0, float x1, float y1, float z1);
float pdot(Point *p1, Point *p2);
float pldot(Plane *p1, Plane *p2);
float length(Point* p0, Point* p1);
float length_squared(Point* p0, Point* p1);
float area_triangle(Point* a, Point* b, Point* c);
void cross(float x0, float y0, float z0, float x1, float y1, float z1, float *xc, float *yc, float *zc);
void pcross(Point *p1, Point *p2, Point *cp);
void plcross(Plane *p1, Plane *p2, Plane *cp);
BOOL normalise_point(Point *p);
BOOL normalised(Plane* p);
BOOL normalise_plane(Plane* p);

void new_length(Point* p0, Point* p1, float len);
void new_length_mid(Point* p0, Point* p1, float len);

void snap_to_grid(Plane *plane, Point *point, BOOL inhibit_snapping);
void snap_to_scale(float *length, BOOL inhibit_snapping);
char *display_rounded(char *buf, float val);
void snap_2d_angle(float x0, float y0, float *x1, float *y1, int angle_tol);
float cleanup_angle_and_snap(float angle, BOOL snap_to_45);
void snap_to_angle(Plane *plane, Point *p0, Point *p1, int angle_tol);
BOOL centre_3pt_circle(Point *p1, Point *p2, Point *p3, Plane *pl, Point *centre, BOOL *clockwise);
BOOL centre_2pt_tangent_circle(Point *p1, Point *p2, Point *p, Plane *pl, Point *centre, BOOL *clockwise);
void look_at_centre_d(Point c, Point p1, Plane n, double matrix[16]);

#endif