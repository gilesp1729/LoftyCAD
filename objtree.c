#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// A constantly incrementing ID. 
// Should not need to worry about it overflowing 32-bits (4G objects!)
// Start at 1 as an ID of zero is used to check for an unreferenced object.
static unsigned int objid = 1;

// The highest object ID encountered when reading in a file.
static unsigned int maxobjid = 0;

// The save count for the tree. Used to protect against multiple writing 
// of shared objects.
static unsigned int save_count = 1;

// A free list for Points. Only singly linked.
static Point *free_list = NULL;

// Creation functions for objects
Object *obj_new(void)
{
    Object *obj = calloc(1, sizeof(Object));

    obj->type = OBJ_NONE;
    obj->ID = 0;
    return obj;
}

Point *point_new(float x, float y, float z)
{
    Point   *pt;
    
    // Try and obtain a point from the free list first
    if (free_list != NULL)
    {
        pt = free_list;
        free_list = (Point *)free_list->hdr.next;
        memset(pt, 0, sizeof(Point));
    }
    else
    {
        pt = calloc(1, sizeof(Point));
    }

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
    Point   *pt = calloc(1, sizeof(Point));

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = objid++;
    pt->x = p->x;
    pt->y = p->y;
    pt->z = p->z;
    return pt;
}

// Edges. 
Edge *edge_new(EDGE edge_type)
{
    StraightEdge *se;
    ArcEdge *ae;
    BezierEdge *be;

    switch (edge_type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
    default:  // just to shut compiler up
        se = calloc(1, sizeof(StraightEdge));
        se->edge.hdr.type = OBJ_EDGE;
        se->edge.hdr.ID = objid++;
        se->edge.type = edge_type;
        return (Edge *)se;

    case EDGE_ARC:
        ae = calloc(1, sizeof(ArcEdge));
        ae->edge.hdr.type = OBJ_EDGE;
        ae->edge.hdr.ID = objid++;
        ae->edge.type = edge_type;
        return (Edge *)ae;

    case EDGE_BEZIER:
        be = calloc(1, sizeof(BezierEdge));
        be->edge.hdr.type = OBJ_EDGE;
        be->edge.hdr.ID = objid++;
        be->edge.type = edge_type;
        return (Edge *)be;
    }
}

Face *face_new(FACE face_type, Plane norm)
{
    Face *face = calloc(1, sizeof(Face));

    face->hdr.type = OBJ_FACE;
    face->hdr.ID = objid++;
    face->type = face_type;
    face->normal = norm;

    switch (face_type)
    {
    case FACE_RECT:
    case FACE_CIRCLE:
    case FACE_CYLINDRICAL:
        face->max_edges = 4;
        break;

    default:  // general and flat faces may have many edges
        face->max_edges = 16;
        break;
    }

    face->edges = (Edge **)malloc(face->max_edges * sizeof(struct Edge *));

    return face;
}

Volume *vol_new(Face *attached_to)
{
    Volume *vol = calloc(1, sizeof(Volume));

    vol->hdr.type = OBJ_VOLUME;
    vol->hdr.ID = objid++;
    vol->attached_to = attached_to;
    return vol;
}

// Link and unlink objects in a double linked list
void link(Object *new_obj, Object **obj_list)
{
    new_obj->next = *obj_list;
    if (*obj_list == NULL)
    {
        *obj_list = new_obj;
        new_obj->prev = NULL;
    }
    else
    {
        (*obj_list)->prev = new_obj;
        *obj_list = new_obj;
    }
}

void delink(Object *obj, Object **obj_list)
{
    if (obj->prev != NULL)
        obj->prev->next = obj->next;
    else
        *obj_list = obj->next;

    if (obj->next != NULL)
        obj->next->prev = obj->prev;
}

void 
link_tail(Object *new_obj, Object **obj_list)
{
    new_obj->next = NULL;
    if (*obj_list == NULL)
    {
        *obj_list = new_obj;
        new_obj->prev = NULL;
    }
    else
    {
        Object *last;

        for (last = *obj_list; last->next != NULL; last = last->next)
            ;

        last->next = new_obj;
        new_obj->prev = last;
    }
}

// Clear the moved and copied_to flags on all points referenced by the object.
// Call this after move_obj or copy_obj.
void
clear_move_copy_flags(Object *obj)
{
    int i;
    Point *p;
    EDGE type;
    Edge *edge;
    Face *face;
    Volume *vol;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point *)obj;
        p->moved = FALSE;
        obj->copied_to = NULL;
        break;

    case OBJ_EDGE:
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        edge = (Edge *)obj;
        clear_move_copy_flags((Object *)edge->endpoints[0]);
        clear_move_copy_flags((Object *)edge->endpoints[1]);
        obj->copied_to = NULL;
#if 0 // probably don't need to do anything else here (arc centres and bezier ctrl points are never shared)
        switch (type)
        {
        case EDGE_STRAIGHT:
            break;

        case EDGE_ARC:    // TODO others
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
#endif
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            clear_move_copy_flags((Object *)edge);
        }
        for (p = face->view_list; p != NULL; p = (Point *)p->hdr.next)
            clear_move_copy_flags((Object *)p);
        obj->copied_to = NULL;
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            clear_move_copy_flags((Object *)face);
        obj->copied_to = NULL;
        break;
    }
}

// Copy any object, with an offset on all its point coordinates.
Object *
copy_obj(Object *obj, float xoffset, float yoffset, float zoffset)
{
    int i;
    Object *new_obj = NULL;
    Point *p;
    EDGE type;
    Edge *edge, *new_edge;
    Face *face, *new_face;
    Volume *vol, *new_vol;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point *)obj;
        if (obj->copied_to != NULL)
        {
            new_obj = obj->copied_to;
        }
        else
        {
            new_obj = (Object *)point_new(p->x + xoffset, p->y + yoffset, p->z + zoffset);
            obj->copied_to = new_obj;
        }
        break;

    case OBJ_EDGE:
        if (obj->copied_to != NULL)
        {
            new_obj = obj->copied_to;
        }
        else
        {
            // Copy the edge.
            new_obj = (Object *)edge_new(((Edge *)obj)->type);
            obj->copied_to = new_obj;

            // Copy the points
            edge = (Edge *)obj;
            new_edge = (Edge *)new_obj;
            new_edge->endpoints[0] = (Point *)copy_obj((Object *)edge->endpoints[0], xoffset, yoffset, zoffset);
            new_edge->endpoints[1] = (Point *)copy_obj((Object *)edge->endpoints[1], xoffset, yoffset, zoffset);
            type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
            switch (type)
            {
            case EDGE_STRAIGHT:
                break;

            case EDGE_ARC:    // TODO others
            case EDGE_BEZIER:
                ASSERT(FALSE, "Not implemented");
                break;
            }
        }

        break;

    case OBJ_FACE:
        face = (Face *)obj;
        new_face = face_new(face->type, face->normal);
        new_obj = (Object *)new_face;

        // Copy the edges
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            new_edge = (Edge *)copy_obj((Object *)edge, xoffset, yoffset, zoffset);
            new_face->edges[i] = new_edge;
        }
        new_face->n_edges = face->n_edges;
        new_face->max_edges = face->max_edges;

        // Set the initial point corresponding to the original
        edge = face->edges[0];
        new_edge = new_face->edges[0];
        if (face->initial_point = edge->endpoints[0])
        {
            new_face->initial_point = new_edge->endpoints[0];
        }
        else
        {
            ASSERT(face->initial_point == edge->endpoints[1], "Point order messed up");
            new_face->initial_point = new_edge->endpoints[1];
        }

#if 0      // TODO do I have to do anything else ehere?
        switch (((Edge *)face->edges[0])->type)
        {
        case EDGE_STRAIGHT:
            break;

        case EDGE_ARC:     // TODO others
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
#endif
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        new_vol = vol_new(NULL);
        new_obj = (Object *)new_vol;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
        {
            new_face = (Face *)copy_obj((Object *)face, xoffset, yoffset, zoffset);
            link_tail((Object *)new_face, (Object **)&new_vol->faces);
        }
        break;
    }
 
    return new_obj;
}

// Copy a face, but reverse all the edges so the normal points the opposite
// way. Make sure the edges containing the initial points still line up.
Face 
*clone_face_reverse(Face *face)
{
    Face *clone = face_new(face->type, face->normal);
    Object *e;
    Object *ne = NULL;
    Edge *edge;
    Point *last_point;
    int i, idx;
    //char buf[256];

    clone->normal.A = -clone->normal.A;
    clone->normal.B = -clone->normal.B;
    clone->normal.C = -clone->normal.C;
    clone->n_edges = face->n_edges;
    clone->max_edges = face->max_edges;         // TODO - handle case where face->edges has been grown

    // Set the initial point. 
    last_point = face->initial_point;

    // Copy the edges, reversing the order
    for (i = 0; i < face->n_edges; i++)
    {
        e = (Object *)face->edges[i];
        ne = copy_obj(e, 0, 0, 0);
        if (i == 0)
            idx = 0;
        else
            idx = face->n_edges - i;
        clone->edges[idx] = (Edge *)ne;

        // Follow the chain of points from e->initial_point
        switch (face->edges[i]->type)
        {
        case EDGE_STRAIGHT:
            edge = (Edge *)e;
            if (last_point == edge->endpoints[0])
            {
                idx = 1;
            }
            else
            {
                ASSERT(last_point == edge->endpoints[1], "Cloned edges don't join up");
                idx = 0;
            }
            last_point = edge->endpoints[idx];
            if (i == 0)
                clone->initial_point = ((Edge *)ne)->endpoints[idx];
            break;

        case EDGE_ARC:     // TODO others
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
    }

#if 0
    sprintf_s(buf, 256, "Clone %d IP %d\r\n", clone->hdr.ID, clone->initial_point->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)clone->edges[0])->endpoints[0]->hdr.ID, ((StraightEdge *)clone->edges[0])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)clone->edges[1])->endpoints[0]->hdr.ID, ((StraightEdge *)clone->edges[1])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)clone->edges[2])->endpoints[0]->hdr.ID, ((StraightEdge *)clone->edges[2])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)clone->edges[3])->endpoints[0]->hdr.ID, ((StraightEdge *)clone->edges[3])->endpoints[1]->hdr.ID);
    Log(buf);
#endif

    return clone;
}

// Move any object by an offset on all its point coordinates.
void
move_obj(Object *obj, float xoffset, float yoffset, float zoffset)
{
    int i;
    Point *p;
    EDGE type;
    Edge *edge;
    Face *face;
    Volume *vol;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point *)obj;
        if (!p->moved)
        {
            p->x += xoffset;
            p->y += yoffset;
            p->z += zoffset;
            p->moved = TRUE;
        }
        break;

    case OBJ_EDGE:
        edge = (Edge *)obj;
        move_obj((Object *)edge->endpoints[0], xoffset, yoffset, zoffset);
        move_obj((Object *)edge->endpoints[1], xoffset, yoffset, zoffset);
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_STRAIGHT:
            break;

        case EDGE_ARC:     // TODO others
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }

        break;

    case OBJ_FACE:
        face = (Face *)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            move_obj((Object *)edge, xoffset, yoffset, zoffset);
        }
        face->view_valid = FALSE;
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            move_obj((Object *)face, xoffset, yoffset, zoffset);
        break;
    }
}

// Find an object owned by another object; return TRUE if it is found.
BOOL
find_obj(Object *parent, Object *obj)
{
    int i;
    EDGE type;
    Edge *edge;
    Face *face;
    Volume *vol;

    switch (parent->type)
    {
    case OBJ_POINT:
        ASSERT(FALSE, "A Point cannot be a parent");
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
        case EDGE_STRAIGHT:
            break;

        case EDGE_ARC:    // TODO others
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
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
        break;

    case OBJ_VOLUME:
        vol = (Volume *)parent;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
        {
            if ((Object *)face == obj)
                return TRUE;
            if (find_obj((Object *)face, obj))
                return TRUE;
        }
        break;
    }

    return FALSE;
}

// Find the top-level parent object (i.e. in the object tree) for the given object.
Object *
find_top_level_parent(Object *tree, Object *obj)
{
    Object *top_level;

    for (top_level = tree; top_level != NULL; top_level = top_level->next)
    {
        if (top_level == obj || find_obj(top_level, obj))
            return top_level;
    }
    return NULL;
}

// Clean out a view list, by putting all the points on the free list.
// The points already have ID's of 0. 
void
free_view_list(Point *view_list)
{
    Point *p;

    if (free_list == NULL)
    {
        free_list = view_list;
    }
    else
    {
        for (p = free_list; p->hdr.next != NULL; p = (Point *)p->hdr.next)
            ;   // run down to the last free element
        p->hdr.next = (Object *)view_list;
    }
}

void
free_view_list_face(Face *face)
{
    free_view_list(face->view_list);
    face->view_list = NULL;
    face->view_valid = FALSE;
}

void
free_view_list_arc(ArcEdge *edge)
{
    free_view_list(edge->view_list);
    edge->view_list = NULL;
    edge->view_valid = FALSE;
}

void
free_view_list_bez(BezierEdge *edge)
{
    free_view_list(edge->view_list);
    edge->view_list = NULL;
    edge->view_valid = FALSE;
}


// Regenerate the view list for a face. While here, also calculate the outward
// normal for the face.
void
gen_view_list_face(Face *face)
{
    int i;
    Edge *e;
    ArcEdge *ae;
    Point *last_point;
    Point *p;
    //char buf[256];

    if (face->view_valid)
        return;

    free_view_list_face(face);

    // Add points at tail of list, to preserve order
    // First the start point
    p = point_newp(face->initial_point);
    p->hdr.ID = 0;
    objid--;        // prevent explosion of objid's
    link_tail((Object *)p, (Object **)&face->view_list);

#if 0
    sprintf_s(buf, 256, "Face %d IP %d\r\n", face->hdr.ID, face->initial_point->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[0])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[0])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[1])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[1])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[2])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[2])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge *)face->edges[3])->endpoints[0]->hdr.ID, ((StraightEdge *)face->edges[3])->endpoints[1]->hdr.ID);
    Log(buf);
#endif
    last_point = face->initial_point;

    for (i = 0; i < face->n_edges; i++)
    {
        e = face->edges[i];

        // Then the subsequent points. Edges will follow in order, but their points
        // may be reversed.
        switch (e->type)
        {
        case EDGE_STRAIGHT:
            if (last_point == e->endpoints[0])
            {
                last_point = e->endpoints[1];
            }
            else
            {
                ASSERT(last_point == e->endpoints[1], "Point order messed up");
                last_point = e->endpoints[0];
            }

            p = point_newp(last_point);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);
            break;

        case EDGE_ARC:
            ae = (ArcEdge *)e;
            gen_view_list_arc(ae);
            if (last_point == e->endpoints[0])
            {
                last_point = e->endpoints[1];
                
                // TODO copy the view list forwards

            }
            else
            {
                ASSERT(last_point == e->endpoints[1], "Point order messed up");
                last_point = e->endpoints[0];

                // TODO copy the view list backwards


            }

            p = point_newp(last_point);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);
            break;





        case EDGE_BEZIER:     // TODO others
            ASSERT(FALSE, "Not implemented");
            break;
        }
    }

    face->view_valid = TRUE;

    // calculate the normal vector
    normal(face->view_list, &face->normal);
}

// Generate the view list for an arc edge.
void
gen_view_list_arc(ArcEdge *ae)
{
    Edge *edge = (Edge *)ae;
    Plane n = ae->normal;
    Point *p;
    float rad = length(&n.refpt, edge->endpoints[0]);
    float t, theta;
    float matrix[16];
    float v[4];
    float res[4];

    if (ae->view_valid)
        return;

    free_view_list_arc(ae);

    // transform arc to XY plane, centre at origin, endpoint 0 on x axis
    look_at_centre(n.refpt, *edge->endpoints[0], n, matrix);

    // angle between two vectors c-p0 and c-p1
    theta = angle3(edge->endpoints[0], &ae->normal.refpt, edge->endpoints[1], &n);
    
    if (ae->clockwise)  // Clockwise angles go negative
    {
        if (theta > 0)
            theta -= 2 * PI;

        // draw arc from p1 (on x axis) to p2. Steps of 5 degrees (TODO: make this parametric on tol etc)
        for (t = 0; t > theta; t -= 5.0f / RAD)
        {
            v[0] = rad * cosf(t);
            v[1] = rad * sinf(t);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            p = point_new(res[0], res[1], res[2]);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&ae->view_list);
        }
    }
    else
    {
        if (theta < 0)
            theta += 2 * PI;

        for (t = 0; t < theta; t += 5.0f / RAD)  
        {
            v[0] = rad * cosf(t);
            v[1] = rad * sinf(t);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            p = point_new(res[0], res[1], res[2]);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&ae->view_list);
        }
    }
}

// Generate the view list for a bezier edge.
void
gen_view_list_bez(BezierEdge *be)
{
    ASSERT(FALSE, "Not implemented");
}

// Purge an object. Points are put in the free list.
void
purge_obj(Object *obj)
{
    int i;
    Edge *e;
    EDGE type;
    Face *face;
    Face *next_face = NULL;
    Volume *vol;

    switch (obj->type)
    {
    case OBJ_POINT:
        if (obj->ID == 0)
            break;              // it's already been freed
        obj->next = (Object *)free_list;
        free_list = (Point *)obj;
        obj->ID = 0;
        break;

    case OBJ_EDGE:
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_STRAIGHT:
            e = (Edge *)obj;
            purge_obj((Object *)e->endpoints[0]);
            purge_obj((Object *)e->endpoints[1]);
            break;

        case EDGE_ARC:    // TODO others. Be very careful if Points are ever in a list, as freeing them will cause problems...
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
        //free(obj);     // TODO: Don't do this. The edge may be shared. (Might need to use a free list or ref counts.)
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        free_view_list_face(face);
        for (i = 0; i < face->n_edges; i++)
            purge_obj((Object *)face->edges[i]);
        free(obj);
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = vol->faces; face != NULL; face = next_face)
        {
            next_face = (Face *)face->hdr.next;
            purge_obj((Object *)face);
        }
        free(obj);
        break;
    }
}

// Purge a tree, freeing everything in it, except for Points, which are
// placed in the free list.
void
purge_tree(Object *tree)
{
    Object *obj;
    Object *nextobj = NULL;

    for (obj = tree; obj != NULL; obj = nextobj)
    {
        nextobj = obj->next;
        purge_obj(obj);
    }
}

// names of things that make the serialised format a little easier to read
char *objname[] = { "(none)", "POINT", "EDGE", "FACE", "VOLUME" };
char *locktypes[] = { "N", "P", "E", "F", "V" };
char *edgetypes[] = { "STRAIGHT", "ARC", "BEZIER" };
char *facetypes[] = { "RECT", "CIRCLE", "FLAT", "CYLINDRICAL", "GENERAL" };

// Serialise an object. Children go out before their parents, in general.
void
serialise_obj(Object *obj, FILE *f)
{
    int i;
    Edge *e;
    EDGE type, constr;
    ArcEdge *ae;
    Face *face;
    Volume *vol;

    // check for object already saved
    if (obj->save_count == save_count)
        return;

    // write out referenced objects first
    switch (obj->type)
    {
    case OBJ_POINT:
        // Points don't reference anything
        break;

    case OBJ_EDGE:
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        constr = ((Edge *)obj)->type & EDGE_CONSTRUCTION;
        fprintf_s(f, "BEGIN %d\n", obj->ID);
        e = (Edge *)obj;
        serialise_obj((Object *)e->endpoints[0], f);
        serialise_obj((Object *)e->endpoints[1], f);
#if 0  // probably don't need anything here
        switch (type)
        {
        case EDGE_STRAIGHT:
            break;

        case EDGE_ARC:     // TODO others
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
#endif
        break;

    case OBJ_FACE:
        fprintf_s(f, "BEGIN %d\n", obj->ID);
        face = (Face *)obj;
        for (i = 0; i < face->n_edges; i++)
            serialise_obj((Object *)face->edges[i], f);
        break;

    case OBJ_VOLUME:
        fprintf_s(f, "BEGIN %d\n", obj->ID);
        vol = (Volume *)obj;
        if (vol->attached_to != NULL)
            serialise_obj((Object *)vol->attached_to, f);
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            serialise_obj((Object *)face, f);
        break;
    }

    // Now write the object itself
    fprintf_s(f, "%s %d %s ", objname[obj->type], obj->ID, locktypes[obj->lock]);
    switch (obj->type)
    {
    case OBJ_POINT:
        fprintf_s(f, "%f %f %f\n", ((Point *)obj)->x, ((Point *)obj)->y, ((Point *)obj)->z);
        break;

    case OBJ_EDGE:
        fprintf_s(f, "%s%s ", edgetypes[type], constr ? "(C)" : "");
        e = (Edge *)obj;
        fprintf_s(f, "%d %d ", e->endpoints[0]->hdr.ID, e->endpoints[1]->hdr.ID);
        switch (type)
        {
        case EDGE_STRAIGHT:
            fprintf_s(f, "\n");
            break;

        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            fprintf_s(f, "%s %f %f %f %f %f %f",
                ae->clockwise ? "C" : "AC",
                ae->normal.refpt.x, ae->normal.refpt.y, ae->normal.refpt.z,
                ae->normal.A, ae->normal.B, ae->normal.C);
            break;

        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        fprintf_s(f, "%s %d %f %f %f %f %f %f ",
            facetypes[face->type],
            face->initial_point->hdr.ID,
            face->normal.refpt.x, face->normal.refpt.y, face->normal.refpt.z,
            face->normal.A, face->normal.B, face->normal.C);
        for (i = 0; i < face->n_edges; i++)
            fprintf_s(f, "%d ", face->edges[i]->hdr.ID);
        fprintf_s(f, "\n");
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        fprintf_s(f, "%d ", vol->attached_to != NULL ? vol->attached_to->hdr.ID : 0);
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            fprintf_s(f, "%d ", face->hdr.ID);
        fprintf_s(f, "\n");
        break;
    }

    obj->save_count = save_count;
}

// Serialise an object tree to a file.
void
serialise_tree(Object *tree, char *filename)
{
    FILE *f;
    Object *obj;
    
    fopen_s(&f, filename, "wt");
    fprintf_s(f, "TITLE %s\n", curr_title);
    fprintf_s(f, "SCALE %f %f %f %d\n", half_size, grid_snap, tolerance, angle_snap);

    save_count++;
    for (obj = tree; obj != NULL; obj = obj->next)
        serialise_obj(obj, f);

    fclose(f);
}

// Check obj array size and grow if necessary (process for each obj type read in)
static void 
check_and_grow(unsigned int id, Object ***object, unsigned int *objsize)
{
    if (id > maxobjid)
        maxobjid = id;

    if (id >= *objsize)
    {
        *objsize *= 2;
        // TODO - make sure the new bit of the object array is zeroed out
        *object = (Object **)realloc(*object, sizeof(Object *)* (*objsize));
    }
}

// Find a lock type from its letter.
LOCK
locktype_of(char *tok)
{
    int i;

    for (i = 0; i <= LOCK_VOLUME; i++)
    {
        if (tok[0] == locktypes[i][0])
            return i;
    }

    return 0;
}


// Deserialise a tree from file. 
BOOL
deserialise_tree(Object **tree, char *filename)
{
    FILE *f;
    char buf[512];
    int stack[10], stkptr;
    int objsize = 1000;
    Object **object;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    // initialise the object array
    object = (Object **)calloc(objsize, sizeof(Object *));
    stkptr = 0;
    maxobjid = 0;

    // read the file line by line
    while (TRUE)
    {
        char *nexttok = NULL;
        char *tok;
        unsigned int id;
        LOCK lock;

        if (fgets(buf, 512, f) == NULL)
            break;

        tok = strtok_s(buf, " \t\n", &nexttok);
        if (strcmp(tok, "TITLE") == 0)
        {
            tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
            if (tok != NULL)
                strcpy_s(curr_title, 256, tok);
        }
        else if (strcmp(tok, "SCALE") == 0)
        {
            tok = strtok_s(NULL, " \t\n", &nexttok);
            half_size = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            grid_snap = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            tolerance = (float)atof(tok);
            tol_log = (int)log10f(1.0f / tolerance);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            angle_snap = atoi(tok);
        }
        else if (strcmp(tok, "BEGIN") == 0)
        {
            // Stack the object ID being constructed
            tok = strtok_s(NULL, " \t\n", &nexttok);
            stack[stkptr++] = atoi(tok);
        }
        else if (strcmp(tok, "POINT") == 0)
        {
            Point *p;
            float x, y, z;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);  // swallow up the lock type (it's ignored for points)
    
            tok = strtok_s(NULL, " \t\n", &nexttok);
            x = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            y = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            z = (float)atof(tok);

            p = point_new(x, y, z);
            p->hdr.ID = id;
            object[id] = (Object *)p;
            if (stkptr == 0)
                link_tail((Object *)p, tree);
        }
        else if (strcmp(tok, "EDGE") == 0)
        {
            int end0, end1;
            Edge *edge;
            ArcEdge *ae;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strcmp(tok, "STRAIGHT") == 0)
            {
                edge = edge_new(EDGE_STRAIGHT);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok);
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok);
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                edge->endpoints[0] = (Point *)object[end0];
                edge->endpoints[1] = (Point *)object[end1];
            }
            else if (strcmp(tok, "ARC") == 0)
            {
                edge = edge_new(EDGE_ARC);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok);
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok);
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                edge->endpoints[0] = (Point *)object[end0];
                edge->endpoints[1] = (Point *)object[end1];

                ae = (ArcEdge *)edge;
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->clockwise = strcmp(tok, "C") == 0;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.refpt.x = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.refpt.y = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.refpt.z = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.A = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.B = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.C = (float)atof(tok);
            }
            else
            {
                // TODO other edge types
                ASSERT(FALSE, "Not implemented");
            }

            edge->hdr.ID = id;
            edge->hdr.lock = lock;
            object[id] = (Object *)edge;
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed edge record");
            stkptr--;
            if (stkptr == 0)
                link_tail((Object *)edge, tree);
        }
        else if (strcmp(tok, "FACE") == 0)
        {
            int pid;
            Face *face;
            Plane norm;
            FACE type;
            Point *init_pt;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strcmp(tok, "RECT") == 0)
            {
                type = FACE_RECT;
            }
            else
            {
                // TODO other types
                ASSERT(FALSE, "Not implemented");
            }

            tok = strtok_s(NULL, " \t\n", &nexttok);
            pid = atoi(tok);
            ASSERT(pid != 0 && object[pid] != NULL && object[pid]->type == OBJ_POINT, "Bad initial point ID");
            init_pt = (Point *)object[pid];

            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.refpt.x = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.refpt.y = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.refpt.z = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.A = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.B = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.C = (float)atof(tok);

            face = face_new(type, norm);
            face->initial_point = init_pt;

            while(TRUE)
            {
                int eid;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                eid = atoi(tok);
                ASSERT(eid > 0 && object[eid] != NULL, "Bad edge ID");

                if (face->n_edges >= face->max_edges)
                {
                    // TODO grow array

                }

                face->edges[face->n_edges++] = (Edge *)object[eid];
            }

            face->hdr.ID = id;
            face->hdr.lock = lock;
            object[id] = (Object *)face;
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed face record");
            stkptr--;
            if (stkptr == 0)
                link_tail((Object *)face, tree);
        }
        else if (strcmp(tok, "VOLUME") == 0)
        {
            Volume *vol;
            Face *attached_to = NULL;
            int fid;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            fid = atoi(tok);
            if (fid != 0)
                attached_to = (Face *)object[fid];

            vol = vol_new(attached_to);
            vol->hdr.ID = id;
            vol->hdr.lock = lock;
            object[id] = (Object *)vol;

            while (TRUE)
            {
                int fid;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                fid = atoi(tok);
                ASSERT(fid > 0 && object[fid] != NULL, "Bad face ID");

                ((Face *)object[fid])->vol = vol;
                link_tail(object[fid], (Object **)&vol->faces);
            }

            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed volume record");
            stkptr--;
            ASSERT(stkptr == 0, "ID stack not empty");
            link_tail((Object *)vol, tree);
        }
    }

    objid = maxobjid + 1;
    save_count = 1;
    free(object);
    fclose(f);

    return TRUE;
}

