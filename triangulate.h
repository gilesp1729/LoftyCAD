// Definitions for the triangulation functions.

#ifndef __TRI_H__
#define __TRI_H__

// View list point is valid coordinate
#define VALID_VP(v) ((v) != NULL && (v)->flags != FLAG_NEW_FACET)

// bounding boxes
void clear_bbox(Bbox *box);
void expand_bbox(Bbox *box, Point *p);
void union_bbox(Bbox *box1, Bbox *box2, Bbox *u);
BOOL intersects_bbox(Bbox *box1, Bbox *box2);

// Regenerate a view list
void invalidate_all_view_lists(Object *parent, Object *obj, float dx, float dy, float dz);
void gen_view_list_tree_surfaces(Group *tree);
void gen_view_list_vol_surface(Volume *vol);
void gen_view_list_vol(Volume *vol);
void gen_view_list_face(Face *face);
void update_view_list_2D(Face *face);
void gen_view_list_arc(ArcEdge *ae);
void gen_view_list_bez(BezierEdge *be);
void free_view_list_face(Face *face);

// Adjacency lists and surface view lists
void gen_view_list_tree_surfaces(Group *tree);
void gen_view_list_vol_surface(Volume *vol);
void gen_adj_list_tree_volumes(Group *tree, Object **rep_list);
void gen_adj_list_volume(Group *tree, Volume *vol);

// Clip a view list (clipviewlist.c)
void init_clip_tess(void);
void gen_view_list_surface(Face *face, Point *facet);

// Triangulate and render
void init_triangulator(void);
void tess_vertex(GLUtesselator *tess, Point *p);
void face_shade(GLUtesselator *tess, Face *face, BOOL selected, BOOL highlighted, BOOL locked);

// Export to STL (export.c)
void export_object_tree(Group *tree, char *filename);

// Import from STL and various formats (import.c)
BOOL read_stl_to_group(Group *group, char *filename);
BOOL read_gts_to_group(Group *group, char *filename);
BOOL read_off_to_group(Group *group, char *filename);

extern GLUtesselator *rtess;

#endif // __TRI_H__