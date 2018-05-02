// Definitions for the geometry functions.

#ifndef __GEOM_H__
#define __GEOM_H__

#define PI 3.1415926f
#define RAD 57.29577f

// test for "near" zero.
#define nz(val)  (fabsf(val) < 0.00001)

BOOL intersect_ray_plane(GLint x, GLint y, Plane *picked_plane, Point *new_point);
BOOL snap_ray_edge(GLint x, GLint y, Edge *edge, Point *new_point);
void normal_list(Point *list, Plane *norm);
void polygon_normal(Point *list, Plane *norm);
void normal3(Point *b, Point *a, Point *c, Plane *norm);
float angle3(Point *b, Point *a, Point *c, Plane *n);
void mat_mult_by_row(float *m, float *v, float *res);
void mat_mult_by_col(float *m, float *v, float *res);

float dot(float x0, float y0, float z0, float x1, float y1, float z1);
float pdot(Point *p1, Point *p2);
float pldot(Plane *p1, Plane *p2);
float length(Point *p0, Point *p1);
void cross(float x0, float y0, float z0, float x1, float y1, float z1, float *xc, float *yc, float *zc);
void pcross(Point *p1, Point *p2, Point *cp);
void plcross(Plane *p1, Plane *p2, Plane *cp);
BOOL normalise_point(Point *p);
BOOL normalise_plane(Plane *p);
void new_length(Point *p0, Point *p1, float len);

void snap_to_grid(Plane *plane, Point *point);
void snap_to_scale(float *length);
char *display_rounded(char *buf, float val);
void snap_2d_angle(float x0, float y0, float *x1, float *y1, int angle_tol);
void snap_to_angle(Plane *plane, Point *p0, Point *p1, int angle_tol);
BOOL centre_3pt_circle(Point *p1, Point *p2, Point *p3, Plane *pl, Point *centre, BOOL *clockwise);
void look_at_centre(Point c, Point p1, Plane n, float matrix[16]);


#endif