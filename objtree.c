#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>


// Free lists for Edges, Points and Objects. Only singly linked.
ListHead free_list_edge = { NULL, NULL };
ListHead free_list_pt = { NULL, NULL };
ListHead free_list_obj = { NULL, NULL };

// List head for ZPolyEdge free list. Used by G-code visualisation.
ListHead free_list_zedge = { NULL, NULL };

#ifdef DEBUG_FREELISTS
// Some counters
int n_alloc_obj = 0;
int n_alloc_pt = 0;
int n_alloc_edge = 0;
#endif

// Creation functions for objects
Object *obj_new(void)
{
    Object *obj;

    // Try and obtain an object from the free list first
    if (free_list_obj.head != NULL)
    {
        obj = free_list_obj.head;
        free_list_obj.head = free_list_obj.head->next;
        if (free_list_obj.head == NULL)
            free_list_obj.tail = NULL;
        memset(obj, 0, sizeof(Object));
    }
    else
    {
        obj = calloc(1, sizeof(Object));
#ifdef DEBUG_FREELISTS
        n_alloc_obj++;
#endif
    }

    obj->type = OBJ_NONE;
    obj->ID = 0;
    return obj;
}

// Get me a fresh clean Point!
Point* point_new_raw()
{
    Point* pt;

    // Try and obtain a point from the free list first
    if (free_list_pt.head != NULL)
    {
        pt = (Point*)free_list_pt.head;
        free_list_pt.head = free_list_pt.head->next;
        if (free_list_pt.head == NULL)
            free_list_pt.tail = NULL;
        memset(pt, 0, sizeof(Point));
    }
    else
    {
        pt = calloc(1, sizeof(Point));
#ifdef DEBUG_FREELISTS
        n_alloc_pt++;
#endif
    }
    return pt;
}

Point *point_new(float x, float y, float z)
{
    Point* pt = point_new_raw();

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = objid++;
    pt->x = x;
    pt->y = y;
    pt->z = z;
    return pt;
}

// Copy just the coordinates from the given point.
Point *point_newp(Point *p)
{
    Point* pt = point_new_raw();

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = objid++;
    pt->x = p->x;
    pt->y = p->y;
    pt->z = p->z;
    return pt;
}

// The same, but don't store or increment an objid. Used for viewlists.
Point* point_newv(float x, float y, float z)
{
    Point* pt = point_new_raw();

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = 0;
    pt->x = x;
    pt->y = y;
    pt->z = z;
    return pt;
}

Point* point_newpv(Point* p)
{
    Point* pt = point_new_raw();

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = 0;
    pt->x = p->x;
    pt->y = p->y;
    pt->z = p->z;
    return pt;
}

// Edges. 
Edge *edge_new(EDGE edge_type)
{
    FreeEdge* fe;
    Edge* e;

    if (free_list_edge.head != NULL)
    {
        fe = (FreeEdge *)free_list_edge.head;
        free_list_edge.head = free_list_edge.head->next;
        if (free_list_edge.head == NULL)
            free_list_edge.tail = NULL;
        memset(fe, 0, sizeof(FreeEdge));
    }
    else
    {
        fe = calloc(1, sizeof(FreeEdge));
#ifdef DEBUG_FREELISTS
        n_alloc_edge++;
#endif
    }

    e = (Edge*)fe;
    e->hdr.type = OBJ_EDGE;
    e->hdr.ID = objid++;
    e->hdr.show_dims = edge_type & EDGE_CONSTRUCTION;
    e->type = edge_type;

    if ((edge_type & ~EDGE_CONSTRUCTION) == EDGE_ARC)
        ((ArcEdge*)e)->ecc = 1.0f;

    return e;
}

Face *face_new(FACE face_type, Plane norm)
{
    Face *face = calloc(1, sizeof(Face));

    face->hdr.type = OBJ_FACE;
    face->hdr.ID = objid++;
    face->type = face_type;
    face->normal = norm;

    // Have a stab at allocating the edge array. Always a power of 2.
    switch (face_type & ~FACE_CONSTRUCTION)
    {
    case FACE_TRI:
    case FACE_RECT:
    case FACE_CIRCLE:
        face->hdr.show_dims = face_type & FACE_CONSTRUCTION;
        // fallthrough
    case FACE_CYLINDRICAL:
    default:                // barrel/bezier types here too
        face->max_edges = 4;
        break;

    case FACE_HEX:
        face->hdr.show_dims = face_type & FACE_CONSTRUCTION;
        face->max_edges = 8;
        break;

    case FACE_FLAT:
        face->max_edges = 16;
        break;
    }

    face->edges = (Edge **)malloc(face->max_edges * sizeof(struct Edge *));

    // Allocate the 2D view list array
    if (IS_FLAT(face))
    {
        face->n_alloc2D = 16;
        face->view_list2D = malloc(face->n_alloc2D * sizeof(Point2D));  
    }

    return face;
}

Volume *vol_new(void)
{
    Volume *vol = calloc(1, sizeof(Volume));

    vol->hdr.type = OBJ_VOLUME;
    vol->hdr.ID = objid++;
    vol->op = OP_UNION;
    clear_bbox(&vol->bbox);
    vol->point_bucket = init_buckets();

    return vol;
}

Group *group_new(void)
{
    Group *grp = calloc(1, sizeof(Group));

    grp->hdr.type = OBJ_GROUP;
    grp->hdr.ID = objid++;
    grp->hdr.lock = LOCK_VOLUME;
    grp->op = OP_NONE;
    clear_bbox(&grp->bbox);
    return grp;
}

// Test if an object is in the object tree at the top level.
BOOL
is_top_level_object(Object *obj, Group *tree)
{
    Object *o;

    for (o = tree->obj_list.head; o != NULL; o = o->next)
    {
        if (obj == o)
            return TRUE;
    }
    return FALSE;
}


// Find an object owned by another object; return TRUE if it is found.
BOOL
find_obj(Object *parent, Object *obj)
{
    int i;
    EDGE type;
    Edge *edge;
    ArcEdge *ae;
    BezierEdge *be;
    Face *face;
    Volume *vol;

    switch (parent->type)
    {
    case OBJ_POINT:
        // a Point cannot be a parent of anything
        return FALSE;

    case OBJ_EDGE:
        edge = (Edge *)parent;
        if ((Object *)edge->endpoints[0] == obj)
            return TRUE;
        if ((Object *)edge->endpoints[1] == obj)
            return TRUE;
        type = ((Edge *)parent)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge *)parent;
            if ((Object *)ae->centre == obj)
                return TRUE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)parent;
            if ((Object *)be->ctrlpoints[0] == obj)
                return TRUE;
            if ((Object *)be->ctrlpoints[1] == obj)
                return TRUE;
            break;
        }

        break;

    case OBJ_FACE:
        face = (Face *)parent;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            if ((Object *)edge == obj)
                return TRUE;
            if (find_obj((Object *)edge, obj))
                return TRUE;
        }
        if (face->text != NULL)
        {
            if ((Object *)&face->text->origin == obj)
                return TRUE;
            if ((Object *)&face->text->endpt == obj)
                return TRUE;
        }
        break;

    case OBJ_VOLUME:
        vol = (Volume *)parent;
        for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
        {
            if ((Object *)face == obj)
                return TRUE;
            if (find_obj((Object *)face, obj))
                return TRUE;
        }
        break;

    case OBJ_GROUP:
        ASSERT(FALSE, "Shouldn't be calling find_obj on a group");
        break;
    }

    return FALSE;
}

// Find the parent object (i.e. in the object tree or in a group) for the given object.
// The parent object maintains the lock on all its components. Will not return a group (unless given).
// There are two versions: a "deep" version which searches the object tree and all groups,
// and a shallow version which does not go down into groups.
Object *
find_parent_object(Group *tree, Object *obj, BOOL deep_search)
{
    Object *top_level;

    // Special case for groups and volumes, just return the object.
    if (obj->type == OBJ_VOLUME || obj->type == OBJ_GROUP)
        return obj;

    // Special case for faces, as we can get to the volume quickly.
    if (obj->type == OBJ_FACE)
    {
        Face *f = (Face *)obj;

        if (f->vol != NULL)
            return (Object *)f->vol;
    }

    // Nothing for it but to search the tree exhaustively.
    for (top_level = tree->obj_list.head; top_level != NULL; top_level = top_level->next)
    {
        if (top_level->type == OBJ_GROUP)
        {
            if (deep_search)
            {
                Object *o = find_parent_object((Group *)top_level, obj, deep_search);
                if (o != NULL)
                    return o;
            }
        }
        else if (top_level == obj || find_obj(top_level, obj))
        {
            return top_level;
        }
    }
    return NULL;
}


// Find the highest parent object or group (i.e. in the object tree) for the given object.
Object *
find_top_level_parent(Group *tree, Object *obj)
{
    Object *top_level;
    
    // If we're starting with a group, just go up to the highest level quickly;
    // otherwise we have to do the search.
    if (obj->type == OBJ_GROUP)
        top_level = obj;
    else
        top_level = find_parent_object(tree, obj, TRUE);

    if (top_level == NULL)
        return NULL;
    for (; top_level->parent_group->hdr.parent_group != NULL; top_level = (Object *)top_level->parent_group)
        ;
    return top_level;
}

// Purge an object. Points are put in the free list.
void
purge_obj_top(Object *obj, OBJECT top_type)
{
    int i;
    Edge *e;
    ArcEdge *ae;
    BezierEdge *be;
    EDGE type;
    Face *face;
    Face *next_face = NULL;
    Object *next_obj = NULL;
    Object *o;
    Volume *vol;
    Group *group;

    switch (obj->type)
    {
    case OBJ_POINT:
        if (obj->ID == 0)
            break;              // it's already been freed
        obj->ID = 0;
        free_point(obj);
        break;

    case OBJ_EDGE:
        e = (Edge *)obj;
        purge_obj_top((Object *)e->endpoints[0], top_type);
        purge_obj_top((Object *)e->endpoints[1], top_type);
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            purge_obj_top((Object *)ae->centre, top_type);
            free_view_list_edge(e);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            purge_obj_top((Object *)be->ctrlpoints[0], top_type);
            purge_obj_top((Object *)be->ctrlpoints[1], top_type);
            free_view_list_edge(e);
            break;
        }
        //if (top_type <= OBJ_FACE)       // If this edge is not part of a volume, we can safely delete it
        if (obj->ID == 0)
            break;
        obj->ID = 0;
        free_edge(obj);
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        free_view_list_face(face);
        for (i = 0; i < face->n_edges; i++)
            purge_obj_top((Object *)face->edges[i], top_type);
        free(face->edges);
        free(face->view_list2D);
        if (face->contours != NULL)
            free(face->contours);
        if (face->text != NULL)
            free(face->text);
        free(obj);
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = (Face *)vol->faces.head; face != NULL; face = next_face)
        {
            next_face = (Face *)face->hdr.next;
            purge_obj_top((Object *)face, top_type);
        }
        free_bucket(vol->point_bucket);
        free(obj);
        break;

    case OBJ_GROUP:
        group = (Group *)obj;
        for (o = group->obj_list.head; o != NULL; o = next_obj)
        {
            next_obj = o->next;
            purge_obj(o);       // purge these objects played by themselves (not belonging to a top-level object)
        }
        if (group->mesh != NULL)
            mesh_destroy(group->mesh);
        free(obj);
        break;
    }
}

void
purge_obj(Object *obj)
{
    // Pass the type of the top-level object being purged.
    purge_obj_top(obj, obj->type);
}

// Free the ZPolyEdges attached to their own group. They are recycled in their own
// free list. The group is left alone to be reused.
void
purge_zpoly_edges(Group* group)
{
    ASSERT(group->hdr.lock == LOCK_VOLUME, "Group is not a ZPolyEdge group");
    if (free_list_zedge.head == NULL)
        free_list_zedge.head = group->obj_list.head;
    else
        free_list_zedge.tail->next = group->obj_list.head;
    free_list_zedge.tail = group->obj_list.tail;

    group->n_members = 0;
    group->obj_list.head = NULL;
    group->obj_list.tail = NULL;
}

// Free a list of temporary edges. They and their points have ID's of zero.
// Points are never shared and may be placed directly in the free list.
void
purge_temp_edge_list(ListHead* list)
{
    Object* obj;
    Object* nextobj = NULL;
    EDGE type;
    Edge* e;
    ArcEdge *ae;
    BezierEdge* be;

    for (obj = list->head; obj != NULL; obj = nextobj)
    {
        nextobj = obj->next;
        ASSERT(obj->type == OBJ_EDGE, "Only edges should be in here");
        ASSERT(obj->ID == 0, "Only temporary edges should be in here");
        e = (Edge*)obj;
        free_point((Object*)e->endpoints[0]);
        free_point((Object*)e->endpoints[1]);
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            free_point((Object*)ae->centre);
            free_view_list_edge(e);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            free_point((Object*)be->ctrlpoints[0]);
            free_point((Object*)be->ctrlpoints[1]);
            free_view_list_edge(e);
            break;
        }
        free_edge(obj);
    }

    list->head = NULL;
    list->tail = NULL;
}

// Purge a tree, freeing everything in it, except for Points, which are
// placed in the free list. 
void
purge_tree(Group *tree, BOOL preserve_objects, ListHead *saved_list)
{
    Object *obj;
    Object *nextobj = NULL;

    if (preserve_objects)
    {
        *saved_list = tree->obj_list;
    }
    else
    {
        for (obj = tree->obj_list.head; obj != NULL; obj = nextobj)
        {
            nextobj = obj->next;
            purge_obj(obj);
        }
    }

    tree->obj_list.head = NULL;
    tree->obj_list.tail = NULL;
    if (tree->mesh != NULL)
        mesh_destroy(tree->mesh);
    tree->mesh = NULL;
    tree->mesh_valid = FALSE;
    tree->mesh_complete = FALSE;
}

// Can we extrude this face? Any face can be extruded, as long it has a valid normal
// and is not a corner face or a construction face.
BOOL
extrudible(Object* obj)
{
    Face* face = (Face*)obj;

    if (obj == NULL || obj->type != OBJ_FACE)
        return FALSE;
    if (face->type & FACE_CONSTRUCTION)
        return FALSE;
    if (!normalised(&face->normal))
        return FALSE;
    if (face->corner)
        return FALSE;

    return TRUE;
}

// Calculate the extruded heights for a volume that was created by extruding faces.
// Mark faces having a valid oppsite number (and a height to it) as being paired.
// If the extrude heights are negative, the volume is a hole (it will be intersected)
void
calc_extrude_heights(Volume* vol)
{
    Face* last_face, * prev_last, *f, *g;

    // An extruded volume made in LoftyCAD always has its last two faces
    // initially extruded. If they were not, we assume the volue is imported and
    // may not have any parallel faces (you can still extrude, but no heights will be shown)
    last_face = (Face *)vol->faces.tail;
    prev_last = (Face *)last_face->hdr.prev;

    // Unpair everything first
    vol->measured = FALSE;
    for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
        f->paired = FALSE;

    // if this is ~ -1 then normals are opposite, and the faces are paired.
    if (!nz(pldot(&last_face->normal, &prev_last->normal) + 1.0f))
        return;         // forget it. Nothing is paired.

    last_face->extrude_height = 
        -distance_point_plane(&last_face->normal, &prev_last->normal.refpt);
    prev_last->extrude_height = last_face->extrude_height;
    last_face->paired = TRUE;
    prev_last->paired = TRUE;

    // Stop here for multi-contour faces (such as text) as only the top and bottom faces can be paired
    if (last_face->n_contours > 1)
        return;

    vol->measured = TRUE;   // assume it is, unless we find an exception

    // TODO: Handle explicitly-given render op separately somehow
    vol->op = last_face->extrude_height < 0 ? OP_INTERSECTION : OP_UNION;

    // Check the rest of the faces, skipping those that are already paired.
    // If all the faces are flat and paired (other than possible round-corner faces)
    // then the volume is measured (dimensions may be shown and changed)
    for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
    {
        if (f->paired)
            continue;
        //if (!extrudible((Object*)f))
        if (!IS_FLAT(f))
        {
            vol->measured = FALSE;
            continue;
        }

        for (g = (Face*)f->hdr.next; g != NULL; g = (Face*)g->hdr.next)
        {
            //if (!extrudible((Object*)g))
            if (!IS_FLAT(g))
                continue;

            if (nz(pldot(&g->normal, &f->normal) + 1.0f))
            {
                ASSERT(!g->paired, "Pairing with already paired face?");
                g->extrude_height =
                    -distance_point_plane(&g->normal, &f->normal.refpt);
                f->extrude_height = g->extrude_height;
                g->paired = TRUE;
                f->paired = TRUE;
                break;
            }
        }
        if (g == NULL)
        {
            // A pair was not found for this face.
            vol->measured = FALSE;
        }
    }
}
