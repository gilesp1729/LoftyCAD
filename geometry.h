// Definitions for the geometry functions.

#ifndef __GEOM_H__
#define __GEOM_H__

BOOL intersect_ray_plane(GLint x, GLint y, Plane *picked_plane, Point *new_point);
BOOL snap_ray_edge(GLint x, GLint y, Edge *edge, Point *new_point);
void normal(Point *list, Plane *norm);
void normal3(Point *b, Point *a, Point *c, Plane *norm);
void mat_mult_by_row(float *m, float *v, float *res);
float dot(float x0, float y0, float z0, float x1, float y1, float z1);
float length(Point *p0, Point *p1);
void cross(float x0, float y0, float z0, float x1, float y1, float z1, float *xc, float *yc, float *zc);
void snap_to_grid(Plane *plane, Point *point);
void snap_to_scale(float *length);
char *display_rounded(char *buf, float val);

#endif