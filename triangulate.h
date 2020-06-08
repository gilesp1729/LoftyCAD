// Definitions for the triangulation functions.

#ifndef __TRI_H__
#define __TRI_H__

// View list point is valid coordinate for continuing a contour
#define VALID_VP(v) ((v) != NULL && (v)->flags != FLAG_NEW_FACET)


// Bounding boxes
void clear_bbox(Bbox *box);
void expand_bbox(Bbox *box, Point *p);
void union_bbox(Bbox *box1, Bbox *box2, Bbox *u);
BOOL intersects_bbox(Bbox *box1, Bbox *box2);

// Regenerate a view list
void invalidate_all_view_lists(Object *parent, Object *obj, float dx, float dy, float dz);
void gen_view_list_vol_surface(Volume *vol);
void gen_view_list_face(Face *face);
void update_view_list_2D(Face *face);
void gen_view_list_arc(ArcEdge *ae);
void gen_view_list_bez(BezierEdge *be);
void free_view_list_face(Face *face);
void free_view_list_edge(Edge *edge);

// Surface meshes
BOOL gen_view_list_vol(Volume *vol);
BOOL gen_view_list_tree_volumes(Group *tree);
void gen_view_list_tree_surfaces(Group *tree, Group *parent_tree);
BOOL mesh_merge_op(OPERATION op, Mesh *mesh1, Mesh *mesh2);

// Clip a view list (clipviewlist.c)
void init_clip_tess(void);
void gen_view_list_surface(Face *face);

// Mesh functions, and interface to CGAL (mesh.cpp)
Mesh *mesh_new(int material);
Mesh *mesh_copy(Mesh *from);
void mesh_destroy(Mesh *mesh);
void mesh_add_vertex(Mesh *mesh, float x, float y, float z, Vertex_index *vi);
void mesh_add_face(Mesh *mesh, Vertex_index *v1, Vertex_index *v2, Vertex_index *v3, Face_index *fi);
BOOL mesh_union(Mesh **mesh1, Mesh *mesh2);
BOOL mesh_intersection(Mesh **mesh1, Mesh *mesh2);
BOOL mesh_difference(Mesh **mesh1, Mesh *mesh2);

typedef void(*FaceCoordCB)(void* arg, float x[3], float y[3], float z[3]);
typedef void(*FaceCoordMaterialCB)(void* arg, int mat, float x[3], float y[3], float z[3]);
typedef void(*FaceVertexCB)(void *arg, int nv, Vertex_index *vi);
typedef void(*VertexCB)(void* arg, Vertex_index* v, float x, float y, float z);
typedef void(*VertexCB_D)(void* arg, Vertex_index* v, double x, double y, double z);

void mesh_foreach_vertex(Mesh* mesh, VertexCB callback, void* callback_arg);
void mesh_foreach_vertex_d(Mesh* mesh, VertexCB_D callback, void* callback_arg);
void mesh_foreach_face_vertices(Mesh *mesh, FaceVertexCB callback, void *callback_arg);
void mesh_foreach_face_coords(Mesh* mesh, FaceCoordCB callback, void* callback_arg);
void mesh_foreach_face_coords_mat(Mesh* mesh, FaceCoordMaterialCB callback, void* callback_arg);
int mesh_num_vertices(Mesh *mesh);
int mesh_num_faces(Mesh *mesh);

// Triangulate and render
void init_triangulator(void);
void tess_vertex(GLUtesselator *tess, Point *p);
void face_shade(GLUtesselator *tess, Face *face, PRESENTATION pres, BOOL locked);

// Export to STL (export.c)
void export_object_tree(Group *tree, char *filename, int file_index);
void mesh_write_off(char* prefix, int id, Mesh* mesh);


// Import from STL and various formats (import.c)
BOOL read_stl_to_group(Group *group, char *filename);
BOOL read_amf_to_group(Group* group, char* filename);
BOOL read_obj_to_group(Group* group, char* filename);
BOOL read_off_to_group(Group* group, char* filename);

extern GLUtesselator *rtess;
extern ListHead xform_list;

#endif // __TRI_H__