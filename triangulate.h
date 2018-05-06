// Definitions for the triangulation functions.

#ifndef __TRI_H__
#define __TRI_H__

void init_triangulator(void);
void face_shade(Face *face, BOOL selected, BOOL highlighted, BOOL locked);
void render_object(Object *obj);
void render_object_tree(Object *tree);
void export_object_tree(Object *tree, char *filename);

extern GLUtesselator *rtess;

#endif // __TRI_H__