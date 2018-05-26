// Definitions for the triangulation functions.

#ifndef __TRI_H__
#define __TRI_H__

// Regenerate a view list
void invalidate_all_view_lists(Object *parent, Object *obj, float dx, float dy, float dz);
void gen_view_list_face(Face *face);
void update_view_list_2D(Face *face);
void gen_view_list_arc(ArcEdge *ae);
void gen_view_list_bez(BezierEdge *be);
void free_view_list(Point *view_list);
void free_view_list_face(Face *face);

// Triangulate and render
void init_triangulator(void);
void face_shade(GLUtesselator *tess, Face *face, BOOL selected, BOOL highlighted, BOOL locked);

// Export to STL (export.c)
void export_object(GLUtesselator *tess, Object *obj);
void export_object_tree(Group *tree, char *filename);

extern GLUtesselator *rtess;

#endif // __TRI_H__