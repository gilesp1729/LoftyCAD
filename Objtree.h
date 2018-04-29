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
    FACE_GENERAL                    // A compound-curved face (e.g. 4 bezier edges)
} FACE;

// Locking level. They must agree with the object types.
typedef enum
{
    LOCK_NONE = OBJ_NONE,           // Fully unlocked
    LOCK_POINTS = OBJ_POINT,       // The points are locked; the edges and faces may be selected
    LOCK_EDGES = OBJ_EDGE,         // The edges are locked; the faces may be selected
    LOCK_FACES = OBJ_FACE,         // The faces are locked; only the whole volume can be selected
    LOCK_VOLUME = OBJ_VOLUME        // The whole volume is locked; nothing can be selected or changed
} LOCK;

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
    LOCK            lock;           // The locking level of this object. It is only relevant
                                    // for top level objects (i.e. in the object tree)
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
} Point;

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
    float           stepsize;       // If zero, the curve is stepped out dynamically based on 
                                    // a flatness tolerance. If not, this is the angular step
                                    // (for arcs), or the parameter step (for beziers).
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
                                    // or a parallel to the cylinder (if bounded by arcs, circles or beziers)
    struct Volume   *vol;           // What volume references this face
    struct Face     *pair;          // Points to a paired face (end of prism) It is initially a coincident face 
                                    // with the opposite normal; they move apart when extruding. 
                                    // If a face has a pair, some changes (like scaling) have to be
                                    // performed with reference to the paired face.
    struct Point    *initial_point; // Point in the first edge that the face starts from. Used to allow
                                    // view lists to be built up independent of the order of points
                                    // in any edge.
    struct Point    *view_list;     // List of XYZ coordinates of GL points to be rendered as a line loop
                                    // (for the edges) and a polygon (for the face). Regenerated whenever
                                    // something has changed. Doubly linked list.
    BOOL            view_valid;     // is TRUE if the view list is up to date.
} Face;

typedef struct Volume
{
    struct Object   hdr;            // Header
    struct Face     *faces;         // Doubly linked list of faces making up the volume
    struct Face     *attached_to;   // Face this volume was started and extruded from, if any.
                                    // If this face is moved or extruded, other objects will
                                    // move with it.
} Volume;

// Prototypes for object functions: 

// Object creation
Object *obj_new();
Point *point_new(float x, float y, float z);
Point *point_newp(Point *p);
Edge *edge_new(EDGE edge_type);
Face *face_new(FACE face_type, Plane norm);
Volume *vol_new();

// Link and delink from lists
void link(Object *new_obj, Object **obj_list);
void delink(Object *obj, Object **obj_list);
void link_tail(Object *new_obj, Object **obj_list);

// Copy and move object
Object *copy_obj(Object *obj, float xoffset, float yoffset, float zoffset);
void move_obj(Object *obj, float xoffset, float yoffset, float zoffset);
void clear_move_copy_flags(Object *obj);
Face *clone_face_reverse(Face *face);

// Find object in tree or as child of another object
BOOL find_obj(Object *parent, Object *obj);
Object *find_top_level_parent(Object *tree, Object *obj);

// Regenerate a view list
void invalidate_all_view_lists(Object *parent, Object *obj, float dx, float dy, float dz);
void gen_view_list_face(Face *face);
void gen_view_list_arc(ArcEdge *ae);
void gen_view_list_bez(BezierEdge *be);

// Write and read a tree to a file
void serialise_tree(Object *tree, char *filename);
BOOL deserialise_tree(Object **tree, char *filename);

// Delete an object, or the whole tree
void purge_obj(Object *obj);
void purge_tree(Object *tree);

#endif /* __OBJTREE_H__ */
