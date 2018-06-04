// Definitions for the data structures for the object tree.

#ifndef __OBJTREE_H__
#define __OBJTREE_H__

// Distinguishes objects on the main tree.
typedef enum
{
    OBJ_NONE,       // must be lowest
    OBJ_POINT,
    OBJ_EDGE,
    OBJ_FACE,
    OBJ_VOLUME,
    OBJ_GROUP,
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
    FACE_RECT,                      // There are 4 edges, and they are constrained to be rectangular.
    FACE_CIRCLE,                    // A complete circle, lying in one plane.
    FACE_FLAT,                      // Any number of edges making a closed face, lying in one plane.
                                    // Only flat faces (up to here) may be extruded.
    FACE_CYLINDRICAL,               // A simply curved face (2 straight edges opposite, and 2 curved)
    FACE_GENERAL,                   // A compound-curved face (e.g. 4 bezier edges)
    FACE_CONSTRUCTION = 0x8000      // OR this in to indicate a construction face
} FACE;

// Locking level. They must agree with the object types, and be ascending.
typedef enum
{
    LOCK_NONE = OBJ_NONE,          // Fully unlocked
    LOCK_POINTS = OBJ_POINT,       // The points are locked; the edges and faces may be selected
    LOCK_EDGES = OBJ_EDGE,         // The edges are locked; the faces may be selected
    LOCK_FACES = OBJ_FACE,         // The faces are locked; only the whole volume can be selected
    LOCK_VOLUME = OBJ_VOLUME       // The whole volume is locked; nothing can be selected or changed
} LOCK;

// Point flags
typedef enum
{
    FLAG_NONE = 0,                  // Nothing special
    FLAG_NEW_FACET,                 // The point begins a new facet (e.g. of a cylinder face).
                                    // In this case only, the XYZ of the point is not a point,
                                    // but a new facet normal. The real points follow.
    FLAG_NEW_EXT_CONTOUR,           // The point begins a new exterior boundary contour
    FLAG_NEW_INT_CONTOUR            // The point begins a new interior contour (a hole)
} PFLAG;

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
    PFLAG           flags;          // Flag to indicate start of facet or contour in view lists.
    GtsObject       *gts_object;    // GTS vertex or edge, used when building triangle meshes
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

// Edges of various kinds. Edges are shared; they do not appear in a list unless they are in the
// top level object tree.
typedef struct Edge
{
    struct Object   hdr;            // Header
    EDGE            type;           // What kind of edge this is
    struct Point    *endpoints[2];  // Two endpoints (valid for any edge type)
    struct Point    *view_list;     // List of intermediate points on the curve, generated 
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
    struct Point    *ctrlpoints[2]; // Two control points
} BezierEdge;

// Face bounded by edges (in a list)
typedef struct Face
{
    struct Object   hdr;            // Header
    FACE            type;           // What type of face this is
    struct Edge     **edges;        // Points to a growable array of pointers to edges bounding the face
    int             n_edges;        // the number of edges in the above array
    int             max_edges;      // the allocated size of the above array
    Plane           normal;         // What plane is the face lying in (if flat) 
    struct Volume   *vol;           // What volume references (owns) this face
    struct Face     *pair;          // Points to a paired face (end of prism) It is initially a coincident face 
                                    // with the opposite normal; they move apart when extruding. 
    struct Point    *initial_point; // Point in the first edge that the face starts from. Used to allow
                                    // view lists to be built up independent of the order of points
                                    // in any edge.
    struct Point    *view_list;     // List(s) of XYZ coordinates of GL points to be rendered as line loop(s)
                                    // (for the edges) and polygon(s) (for the face). Point flags indicate
                                    // the presence of multiple facets. Regenerated whenever
                                    // something has changed. Doubly linked list.
    struct Point    *spare_list;    // List of spare points allocated by the tessellator's combine
                                    // callback. They are recorded here so they can be freed. Also used
                                    // for intermediate results while clipping.
    BOOL            view_valid;     // is TRUE if the view list is up to date.
    struct Point2D  *view_list2D;   // Array of 2D points for the view list, for quick point-in-polygon
                                    // testing. Indexed [0] to [N-1], with [N] = [0].
    int             n_view2D;       // Number of points in the 2D view list.
    int             n_alloc2D;      // Alloced size of 2D view list (in units of sizeof(Point2D))
} Face;

// Bounding box for a volume
typedef struct Bbox
{
    float xmin;                     // Min and max coordinates for the volume
    float xmax;
    float ymin;
    float ymax;
    float zmin;
    float zmax;
} Bbox;

// Volume struct. This is the usual top-level 3D object.
typedef struct Volume
{
    struct Object   hdr;            // Header
    struct Bbox     bbox;           // Bounding box in 3D
    struct Bbox     old_bbox;       // Bbox prior to any move (required to update damaged surfaces)
    BOOL            repair_surface; // If TRUE, this volume needs its surface updated.
    BOOL            vol_neg;        // If TRUE, this volume is negative, or a hole (face normals face inwards)
    struct Object   *adj_list;      // Singly linked list of other volumes whose bboxes intersect this one
    struct Point    *point_list;    // Doubly linked list of Points whose coordinates are copied from 
                                    // child faces' view lists. (use a Point list as can easily be freed)
                                    // Allows sharing of GTS vertices when building triangle meshes.
    struct Point    *edge_list;     // List edges for sharing similarly (use a Point list as can easily be freed)
    GtsSurface      *full_surface;  // GTS surface for volume; it will be clipped to others to make vis_surface
    GtsSurface      *vis_surface;   // GTS surface for volume; visible surface that results from clipping
    struct Face     *faces;         // Doubly linked list of faces making up the volume

    // Debugging only
    struct Edge     *inter_edge_list; // Doubly linked list of edges representing the intersection curve
                                    // of surface with other surfaces
    BOOL            inter_closed;   // TRUE if GTS thinks the said curve is closed
} Volume;

// The group struct is used for groups, and also for the main object tree.
typedef struct Group
{
    struct Object   hdr;            // Header
    char            title[256];     // A name for the group
    struct Object   *obj_list;      // Doubly linked list of objects making up the group
} Group;

// Externs

extern unsigned int objid;
extern Point *free_list_pt;
extern Object *free_list_obj;

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
Face *make_flat_face(Edge *edge);

// Link and delink from doubly linked lists
void link(Object *new_obj, Object **obj_list);
void delink(Object *obj, Object **obj_list);
void link_tail(Object *new_obj, Object **obj_list);
void link_group(Object *new_obj, Group *group);
void delink_group(Object *obj, Group *group);
void link_tail_group(Object *new_obj, Group *group);
void free_point_list(Point *pt_list);

// ...and for singly linked lists (using prev pointer to point to arbitrary object)
void link_single(Object *new_obj, Object **obj_list);
void link_single_checked(Object *new_obj, Object **obj_list);
void free_obj_list(Object *obj_list);

// Copy and move object
Object *copy_obj(Object *obj, float xoffset, float yoffset, float zoffset);
void move_obj(Object *obj, float xoffset, float yoffset, float zoffset);
void clear_move_copy_flags(Object *obj);
Face *clone_face_reverse(Face *face);

// Find object in tree, at a location, or as child of another object
BOOL find_obj(Object *parent, Object *obj);
Object *find_in_neighbourhood(Object *match_obj, Group *tree);
Object *find_parent_object(Group *tree, Object *obj, BOOL deep_search);
Object *find_top_level_parent(Group *tree, Object *obj);
BOOL is_top_level_object(Object *obj, Group *tree);

// Write and read a tree to a file (serialise.c)
void serialise_tree(Group *tree, char *filename);
BOOL deserialise_tree(Group *tree, char *filename);
void write_checkpoint(Group *tree, char *filename);
BOOL read_checkpoint(Group *tree, char *filename, int generation);
void clean_checkpoints(char *filename);

// Delete an object, or the whole tree
void purge_obj(Object *obj);
void purge_tree(Object *tree);

#endif /* __OBJTREE_H__ */
