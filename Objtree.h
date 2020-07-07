// Definitions for the data structures for the object tree.

#ifndef __OBJTREE_H__
#define __OBJTREE_H__

// Opaque pointers to C++ objects used by CGAL
typedef void Mesh;
typedef void Vertex_index;
typedef void Face_index;

// Distinguishes objects on the main tree.
typedef enum
{
    OBJ_NONE,       // must be lowest
    OBJ_POINT,
    OBJ_EDGE,
    OBJ_FACE,
    OBJ_VOLUME,
    OBJ_GROUP,
    OBJ_TRANSFORM,
    OBJ_MAX         // must be highest
} OBJECT;

// What kind of edge this is
typedef enum
{
    EDGE_STRAIGHT = 0, 
    EDGE_ARC = 1,
    EDGE_BEZIER = 2,
    EDGE_CONSTRUCTION = 0x8000      // OR this in to indicate a construction edge
} EDGE;

// What kind of face this is
typedef enum
{
    FACE_RECT,                      // There are 4 edges, and they are constrained (initially) to be rectangular.
    FACE_CIRCLE,                    // A complete circle, lying in one plane.
    FACE_FLAT,                      // Any number of edges making a closed face, lying generally in one plane.
                                    // Only flat faces (up to here) may be extruded.
    FACE_CYLINDRICAL,               // A simply-curved face bounded by an opposing pair of arcs, the other pair straights.
    FACE_BARREL,                    // A compound-curved face bounded by one opposing pair of arcs, the other pair arcs or beziers.
    FACE_BEZIER,                    // A compound-curved face bounded by two opposing pairs of beziers.
    FACE_CONSTRUCTION = 0x8000      // OR this in to indicate a construction face
} FACE;

// Locking level. They must agree with the object types, and be ascending.
typedef enum
{
    LOCK_NONE = OBJ_NONE,          // Fully unlocked
    LOCK_POINTS = OBJ_POINT,       // The points are locked; the edges and faces may be selected
    LOCK_EDGES = OBJ_EDGE,         // The edges are locked; the faces may be selected
    LOCK_FACES = OBJ_FACE,         // The faces are locked; only the whole volume can be selected
    LOCK_VOLUME = OBJ_VOLUME,      // The whole volume is locked; nothing can be selected or changed
    LOCK_GROUP = OBJ_GROUP         // For groups only: the group is locked and cannot be selected or moved
} LOCK;

// Operation to use when rendering a volume or group.
typedef enum
{
    OP_UNION,                       // Object is unioned with its containing (parent) tree. Default for solids.
    OP_INTERSECTION,                // Object is intersected with parent tree. Default for negative volumes (holes)
    OP_DIFFERENCE,                  // Object is subtracted from its parent tree.
    OP_NONE,                        // (groups only) Group contents are rendered into the parent tree directly.
    OP_MAX                          // must be last + 1
} OPERATION;

// Point flags (only to be used on points in view lists, as they are not shared)
typedef enum
{
    FLAG_NONE = 0,                  // Nothing special
    FLAG_NEW_FACET,                 // The point begins a new facet (e.g. of a cylinder face).
                                    // In this case only, the XYZ of the point is not a point,
                                    // but a new facet normal. The real points follow.
    FLAG_NEW_CONTOUR                // The point begins a new boundary contour
} PFLAG;

// Material structure.
typedef struct Material
{
    BOOL            valid;          // TRUE if the material hs been set.
    BOOL            hidden;         // TRUE if NOT showing this material (default=0 is to show)
    float           color[3];       // Colors in [0.0, 1.0] for diffuse and ambient lighting
    float           shiny;          // Shininess in [1,50]
    char            name[64];       // Descriptive name
} Material;

// Header for any type of object.
typedef struct Object
{
    OBJECT          type;           // What kind of object this is.
    unsigned int    ID;             // ID of object for serialisation. For points, a value of 0 
                                    // indicates that it is in the free list or a view list, and
                                    // not pointed to by any other object in the object tree.
    unsigned int    save_count;     // Number of times the tree has been serialised. Shared objects
                                    // will not be written again if their save_count is up to date
                                    // with the current save count for the tree.
    struct Object   *copied_to;     // When an object is copied, the original is set to point to
                                    // the copy here. This allows sharing to be kept the same.
    BOOL            show_dims;      // If this object has dimensions, they will be shown all the time.
    LOCK            lock;           // The locking level of this object. It is only relevant
                                    // for top level objects (i.e. in the object tree)
    struct Group    *parent_group;  // For top level objects, the parent group into whose list this
                                    // object is linked.
    struct Object   *next;          // The next object in any list, e.g. the entire object tree, 
                                    // or the list of faces in a volume, etc. Not always used if
                                    // there is a fixed array of objects, e.g. two end points
                                    // for an edge. NULL if the last.
    struct Object   *prev;          // The previous object in a list. NULL if the first.
    int             tv_flags;       // State flags for treeview (the state of the TVITEM struct)
                                    // Indicates if the object is expanded (shows children)
} Object;

// Point (vertex). Points and edges are the only kind of objects that can be shared.
typedef struct Point
{
    struct Object   hdr;            // Header
    float           x;              // Coordinates
    float           y;
    float           z;
    BOOL            moved;          // When a point is moved, this is set TRUE. This stops shared
                                    // points from being moved twice.
    float           decay;          // Decay factor for smooth moves.
    float           cosine;         // Cosine of normals factor for smooth moves.
    unsigned int    drawn;          // Drawn number increments and stops shared points being drawn twice.
    PFLAG           flags;          // Flag to indicate start of facet or contour in view lists.
    Vertex_index    *vi;            // Index to CGAL mesh vertex (used when building meshes)
    struct Edge     *start_list;    // List of edges that start at this point (i.e. this point is endpoint 0)
    struct Point    *bucket_next;   // Next point in sorting bucket (used for searching points by coordinate)
} Point;

// 2D point struct.
typedef struct Point2D
{
    float x;
    float y;
} Point2D;

// Plane definition
typedef struct Plane
{
    struct Point    refpt;          // Point that lies on the plane
    float           A;              // Vector of plane equation
    float           B;              // A(x-x1) + B(y-y1) + C(z-z1) = 0
    float           C;
} Plane;

// Standard planes 
typedef enum
{
    PLANE_XY,                       // Plane is parallel to the XY plane, etc. from above
    PLANE_YZ,
    PLANE_XZ,
    PLANE_MINUS_XY,                 // And from below
    PLANE_MINUS_YZ,
    PLANE_MINUS_XZ,
    PLANE_GENERAL                   // Plane is not parallel to any of the above
} PLANE;

// List header containing a head and a tail pointer.
typedef struct ListHead
{
    struct Object *head;
    struct Object *tail;
} ListHead;

// Edges of various kinds. Edges are shared when used in faces, or they can start on their own
// in the object tree.
typedef struct Edge
{
    struct Object   hdr;            // Header
    EDGE            type;           // What kind of edge this is
    unsigned int    drawn;          // Drawn number increments and stops shared edges being drawn twice.
    BOOL            corner;         // If TRUE, this edge is a round or chamfer corner.
    struct Point    *endpoints[2];  // Two endpoints (valid for any edge type)
    struct Edge     *start_next;    // Next edge in the starting list (edges that start at endpoint 0. Only used when importing)
    struct ListHead view_list;      // List of intermediate points on the curve, generated 
                                    // between the two endpoints, to be flat within the
                                    // specified tolerance. Only used for arcs and beziers.
    BOOL            view_valid;     // is TRUE if the view list is up to date.
    BOOL            stepping;       // If FALSE, the curve is stepped out dynamically based on 
                                    // a flatness tolerance. If TRUE, stepsize is the angular step
                                    // (for arcs), or the parameter step (for beziers).
    float           stepsize;       // The stepsize for curves as above.
    int             nsteps;         // The number of steps actually generated (arcs and beziers)
} Edge;

typedef struct StraightEdge
{
    struct Edge     edge;           // Edge
} StraightEdge;

typedef struct ArcEdge
{
    struct Edge     edge;           // Edge
    Plane           normal;         // Normal vector (only ABC are used)
    BOOL            clockwise;      // If TRUE, the points go clockwise from endpoint 0
                                    // to endpoint 1, as seen from the facing plane.
                                    // If FALSE they go anticlockwise.
    struct Point    *centre;        // The centre of the arc.
} ArcEdge;

typedef struct BezierEdge
{
    struct Edge     edge;           // Edge
    struct Point    *ctrlpoints[2]; // Two control points, corresponding in order to the edge endpoints
    struct Point    *bezctl[4];      // End and control points copied into the correct order for building
                                    // the face view list, to help with Bezier surface interpolation
} BezierEdge;

// Contour in a multi-contour face. This information is used when reversing and extruding faces.
// If it isn't present, the face's edges array, initial point and n_edges define the only contour.
typedef struct Contour
{
    int             edge_index;     // Index into main edges array of the starting edge
    int             ip_index;       // Whether the contour's initial point is endpoint[0] or [1] on the starting edge
    int             n_edges;        // Number of edges in this contour
} Contour;

// This structure is used for text, to allow it to be edited/regenerated while it
// is still a face at top level (not being extruded into a volume yet)
typedef struct Text
{
    char string[80];                // Text string
    char font[32];                  // Font name
    BOOL bold;                      // TRUE if bold
    BOOL italic;                    // TRUE if italic
    Point origin;                   // Copy of picked_point where the text began
    Point endpt;                    // Copy of new_point where the text was dragged out
    Plane plane;                    // Copy of plane text lies in
} Text;

// Face bounded by edges, in one or more contours.
typedef struct Face
{
    struct Object   hdr;            // Header
    FACE            type;           // What type of face this is
    struct Edge     **edges;        // Points to a growable array of pointers to edges bounding the face
    int             n_edges;        // the number of edges in the above array
    int             max_edges;      // the allocated size of the above array
    Plane           normal;         // What plane is the face lying in (if flat) 
    struct Volume   *vol;           // What volume references (owns) this face
    BOOL            paired;         // TRUE for the faces that can be extruded and have a height to 
                                    // their opposite number with an equal and opposite normal.
    BOOL            has_corners;    // TRUE for faces that contain corner edges in their edge list.
                                    // These faces cannot have corner faces adjacent to them.
    float           extrude_height; // Extrude height to the paired face. 
                                    // If negative, it's a hole (face normals face inwards)
    BOOL            corner;         // If TRUE, this face is extruded from a round or chamfer corner.
    float           color_decay;    // Color attenuation factor for halo faces (derived from points)
    struct Point    *initial_point; // Point in the first edge that the face starts from. Used to allow
                                    // view lists to be built up independent of the order of points
                                    // in any edge.
    struct Contour  *contours;      // Growable array of Contour structures, representing starting edges
                                    // of contours in multi-contour faces (e.g. those coming from fonts).
                                    // May be NULL, in which case there is only one contour.
    int             n_contours;     // Number of contours in the above array (0 if array is empty)
    Text            *text;          // If the face is text, its string, font etc. are here.
    struct ListHead view_list;      // List(s) of XYZ coordinates of GL points to be rendered as line loop(s)
                                    // (for the edges) and polygon(s) (for the face). Point flags indicate
                                    // the presence of multiple facets. Regenerated whenever
                                    // something has changed. Doubly linked list.
    struct ListHead spare_list;     // List of spare points allocated by the tessellator's combine
                                    // callback. They are recorded here so they can be freed. Also used
                                    // for intermediate results while clipping.
    BOOL            view_valid;     // is TRUE if the view list is up to date.
    struct Point2D  *view_list2D;   // Array of 2D points for the view list, for quick point-in-polygon
                                    // testing. Indexed [0] to [N-1], with [N] = [0].
    int             n_view2D;       // Number of points in the 2D view list.
    int             n_alloc2D;      // Alloced size of 2D view list (in units of sizeof(Point2D))
} Face;

// Bounding box for a volume or a group
typedef struct Bbox
{
    float           xmin;          // Min and max coordinates for the volume
    float           xmax;
    float           ymin;
    float           ymax;
    float           zmin;
    float           zmax;
    float           xc;            // Centre of bbox, calculated when required
    float           yc;
    float           zc;
} Bbox;

// Flags that characterise transforms. They are a bitfield. If it is zero,
// the transform is unitary and does not affect the result, so can be skipped.
typedef enum
{
    XF_SCALE_NONUNITY = 1,          // There is a non-unity scale to be applied.
    XF_ROTATE_X = 2,                // There is a rotation about the X axis.
    XF_ROTATE_Y = 4,                // There is a rotation about the Y axis.
    XF_ROTATE_Z = 8                 // There is a rotation about the Z axis.
} TRANSFLAGS;

// Transform for a volume or a group. In this order:
// subtract centre, do scaling, do rotation, add back centre.
typedef struct Transform
{
    struct Object   hdr;            // Header, to allow list insertion
    float           xc;             // Centre of transform, copied from centre of vol/group bbox
    float           yc;
    float           zc;
    float           sx;             // Scale factors. Scaling is applied first.
    float           sy;
    float           sz;
    BOOL            enable_scale;   // TRUE to allow the scaling to be applied
    float           rx;             // Rotation angles, in degrees, about axes. Rotations are applied second.
    float           ry;
    float           rz;
    BOOL            enable_rotation; // TRUE to allow the rotation to be applied
    double          mat[9];         // Net matrix from scales and rotates
    double          inv_mat[9];     // Inverse matrix
    TRANSFLAGS      flags;          // Transform flag bits, that allow optimisation of the speed of application.
} Transform;

// Volume struct. This is the usual top-level 3D object.
typedef struct Volume
{
    struct Object   hdr;            // Header
    struct Bbox     bbox;           // Bounding box for the volume in 3D, based on untransformed points.
    struct Transform *xform;        // Transform to be applied to volume
    OPERATION       op;             // Operation to use when combining volume with tree
                                    // NOTE THE ABOVE MUST FOLLOW immediately after header
    int             material;       // Material index (0 is the default)
    BOOL            measured;       // If TRUE, all faces are paired, and the volume has dimensions (l/w/h)
    struct Point    ***point_bucket;  // Bucket structure of Points whose coordinates are copied from 
                                    // child faces' view lists. Allow sharing points when importing
                                    // STL meshes, and sharing of mesh vertices when building triangle meshes.
    Mesh            *mesh;          // Surface mesh for this volume.
    BOOL            mesh_valid;     // If TRUE, the mesh is up to date.
    BOOL            mesh_merged;    // If TRUE, the mesh has been merged to its parent group mesh.
    struct ListHead faces;          // Doubly linked list of faces making up the volume
} Volume;

// The group struct is used for groups, and also for the main object tree.
typedef struct Group
{
    struct Object   hdr;            // Header
    struct Bbox     bbox;           // Bounding box for the group in 3D, based on bboxes of volumes in the group
    struct Transform *xform;        // Transform to be applied to group
    OPERATION       op;             // Operation to use when combining group with tree
                                    // NOTE THE ABOVE MUST FOLLOW immediately after header
    char            title[256];     // A name for the group
    Mesh            *mesh;          // Mesh for the complete group
    BOOL            mesh_valid;     // If TRUE, the mesh is up to date (but not necessarily complete)
    BOOL            mesh_merged;    // If TRUE, the mesh has been merged to its parent group mesh.
    BOOL            mesh_complete;  // If TRUE, all volumes have been completely merged to this mesh.
                                    // (otherwise, some will need to be added separately to the output)
    struct ListHead obj_list;       // Doubly linked list of objects making up the group
} Group;

// Externs

extern unsigned int objid;
extern ListHead free_list_pt;
extern ListHead free_list_obj;

// Flatness test
#define IS_FLAT(face)       \
    (       \
        ((face)->type & ~FACE_CONSTRUCTION) == FACE_RECT     \
        || \
        ((face)->type & ~FACE_CONSTRUCTION) == FACE_CIRCLE    \
        || \
        ((face)->type & ~FACE_CONSTRUCTION) == FACE_FLAT    \
    )

// Prototypes for object functions: 

// Object creation
Object *obj_new();
Point *point_new(float x, float y, float z);
Point *point_newp(Point *p);
Edge *edge_new(EDGE edge_type);
Face *face_new(FACE face_type, Plane norm);
Volume *vol_new(void);
Group *group_new(void);
Transform *xform_new(void);
Face *make_flat_face(Edge *edge);

// Link and delink from doubly linked lists (list.c)
void link(Object *new_obj, ListHead *obj_list);
void delink(Object *obj, ListHead *obj_list);
void link_tail(Object *new_obj, ListHead *obj_list);
void link_group(Object *new_obj, Group *group);
void delink_group(Object *obj, Group *group);
void link_tail_group(Object *new_obj, Group *group);
void free_point_list(ListHead *pt_list);

// ...and for singly linked lists (using prev pointer to point to arbitrary object)
void link_single(Object *new_obj, ListHead *obj_list);
void link_single_checked(Object *new_obj, ListHead *obj_list);
void free_obj_list(ListHead *obj_list);

// bucket stuff (list.c)
Point ***init_buckets(void);
Point **find_bucket(Point *p, Point ***bucket);
void empty_bucket(Point ***bucket);
void free_bucket_points(Point ***bucket);
void free_bucket(Point ***bucket);

// Copy and move object (mover.c)
Object *copy_obj(Object *obj, float xoffset, float yoffset, float zoffset);
void move_obj(Object *obj, float xoffset, float yoffset, float zoffset);
void calc_halo_params(Face* face, ListHead *halo);
void move_halo_around_face(Face* face, float xoffset, float yoffset, float zoffset);
BOOL find_corner_edges(Object* obj, Object* parent, ListHead *halo);
void move_corner_edges(ListHead *halo, float xoffset, float yoffset, float zoffset);
void clear_move_copy_flags(Object *obj);
Face *clone_face_reverse(Face *face);

// Rotate-90 and reflect object in place (note: this is not the same as a transform)
void find_obj_pivot(Object* obj, float* xc, float* yc, float* zc);
void rotate_obj_90_facing(Object* obj, float xc, float yc, float zc);
void reflect_obj_facing(Object* obj, float xc, float yc, float zc);

// Find object in tree, at a location, or as child of another object
BOOL find_obj(Object *parent, Object *obj);
Object *find_in_neighbourhood(Object *match_obj, Group *tree);
Object *find_parent_object(Group *tree, Object *obj, BOOL deep_search);
Object *find_top_level_parent(Group *tree, Object *obj);
BOOL is_top_level_object(Object *obj, Group *tree);
void build_parent_xform_list(Object *obj, Object *parent, ListHead *xform_list);

// Write and read a tree to a file (serialise.c)
void serialise_tree(Group *tree, char *filename);
BOOL deserialise_tree(Group *tree, char *filename, BOOL importing);
void write_checkpoint(Group *tree, char *filename);
BOOL read_checkpoint(Group *tree, char *filename, int generation);
void clean_checkpoints(char *filename);

// Delete an object, or the whole tree
void purge_obj(Object *obj);
void purge_obj_top(Object *obj, OBJECT type);
void purge_tree(Group *tree, BOOL preserve_objects, ListHead *saved_list);

// Extrude heights/dimensions
BOOL extrudible(Object* obj);
void calc_extrude_heights(Volume* vol);

#endif /* __OBJTREE_H__ */
