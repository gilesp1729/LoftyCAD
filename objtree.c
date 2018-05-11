#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>


// A free list for Points. Only singly linked.
Point *free_list = NULL;

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
    Point   *pt;

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

    switch (face_type & ~FACE_CONSTRUCTION)
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

Volume *vol_new(void)
{
    Volume *vol = calloc(1, sizeof(Volume));

    vol->hdr.type = OBJ_VOLUME;
    vol->hdr.ID = objid++;
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

// Test if an object is in the object tree at the top level.
BOOL
is_top_level_object(Object *obj, Object *tree)
{
    Object *o;

    for (o = tree; o != NULL; o = o->next)
    {
        if (obj == o)
            return TRUE;
    }
    return FALSE;
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
    ArcEdge *ae;
    BezierEdge *be;
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
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            clear_move_copy_flags((Object *)ae->centre);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            clear_move_copy_flags((Object *)be->ctrlpoints[0]);
            clear_move_copy_flags((Object *)be->ctrlpoints[1]);
            break;
        }
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
    ArcEdge *ae, *nae;
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
            new_obj->lock = obj->lock;
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
            new_obj->lock = obj->lock;
            obj->copied_to = new_obj;

            // Copy the points
            edge = (Edge *)obj;
            new_edge = (Edge *)new_obj;
            new_edge->endpoints[0] = (Point *)copy_obj((Object *)edge->endpoints[0], xoffset, yoffset, zoffset);
            new_edge->endpoints[1] = (Point *)copy_obj((Object *)edge->endpoints[1], xoffset, yoffset, zoffset);
            type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
            switch (type)
            {
            case EDGE_ARC: 
                ae = (ArcEdge *)edge;
                nae = (ArcEdge *)new_edge;
                nae->centre = (Point *)copy_obj((Object *)ae->centre, xoffset, yoffset, zoffset);
                nae->clockwise = ae->clockwise;
                nae->normal = ae->normal;
                break;

            case EDGE_BEZIER:   // TODO others
                ASSERT(FALSE, "Copy Bez Not implemented");
                break;
            }
        }

        break;

    case OBJ_FACE:
        face = (Face *)obj;
        new_face = face_new(face->type, face->normal);
        new_obj = (Object *)new_face;
        new_obj->lock = obj->lock;

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
        if (face->initial_point == edge->endpoints[0])
        {
            new_face->initial_point = new_edge->endpoints[0];
        }
        else
        {
            ASSERT(face->initial_point == edge->endpoints[1], "Point order messed up");
            new_face->initial_point = new_edge->endpoints[1];
        }
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        new_vol = vol_new();
        new_obj = (Object *)new_vol;
        new_obj->lock = obj->lock;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
        {
            new_face = (Face *)copy_obj((Object *)face, xoffset, yoffset, zoffset);
            new_face->vol = new_vol;
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
    ArcEdge *ae, *nae;
    Point *last_point;
    int i, idx;
    //char buf[256];

    // Swap the normal around
    clone->normal.A = -clone->normal.A;
    clone->normal.B = -clone->normal.B;
    clone->normal.C = -clone->normal.C;
    clone->n_edges = face->n_edges;
    clone->max_edges = face->max_edges;         // TODO - handle case where face->edges has been grown
    
    // pair the face with its clone
    clone->pair = face;
    face->pair = clone;

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

        switch (face->edges[i]->type)
        {
        case EDGE_ARC: 
            ae = (ArcEdge *)e;
            nae = (ArcEdge *)ne;
            nae->clockwise = !ae->clockwise;
            nae->normal.A = -ae->normal.A;
            nae->normal.B = -ae->normal.B;
            nae->normal.C = -ae->normal.C;
            break;

        case EDGE_BEZIER:    // TODO others
            ASSERT(FALSE, "CloneReverse Bez Not implemented");
            break;
        }
    }

#ifdef DEBUG_REVERSE_RECT_FACE
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
    ArcEdge *ae;
    BezierEdge *be;
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
        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            move_obj((Object *)ae->centre, xoffset, yoffset, zoffset);
            edge->view_valid = FALSE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            move_obj((Object *)be->ctrlpoints[0], xoffset, yoffset, zoffset);
            move_obj((Object *)be->ctrlpoints[1], xoffset, yoffset, zoffset);
            edge->view_valid = FALSE;
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
        face->normal.refpt.x += xoffset;    // don't forget to move the normal refpt too
        face->normal.refpt.y += yoffset;
        face->normal.refpt.z += zoffset;
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

// Mark all view lists as invalid for parts of a parent object, when a part of it
// has moved.
//
// Do this all the way down (even though gen_view_list will recompute all the children anyway)
// since we may encounter arc edges that need constraints updated when, say, an 
// endpoint has moved. We also pass in how much it has moved.
void
invalidate_all_view_lists(Object *parent, Object *obj, float dx, float dy, float dz)
{
    Face *f;
    ArcEdge *ae;
    int i;

    if (parent->type == OBJ_VOLUME)
    {
        for (f = ((Volume *)parent)->faces; f != NULL; f = (Face *)f->hdr.next)
            invalidate_all_view_lists((Object *)f, obj, dx, dy, dz);
    }
    else if (parent->type == OBJ_FACE)
    {
        f = (Face *)parent;
        f->view_valid = FALSE;
        for (i = 0; i < f->n_edges; i++)
            invalidate_all_view_lists((Object *)f->edges[i], obj, dx, dy, dz);
    }
    else if (parent->type == OBJ_EDGE)
    {
        switch (((Edge *)parent)->type & ~EDGE_CONSTRUCTION)
        {
        case EDGE_ARC:
            ae = (ArcEdge *)parent;
            ((Edge *)parent)->view_valid = FALSE;

            // Check that the obj in question is an endpoint or the centre,
            // and update the other point(s) to suit.
            if (obj->type == OBJ_POINT)
            {
                Point *p = (Point *)obj;
                Edge *e = (Edge *)parent;

                if (p == e->endpoints[0])
                {
                    // Endpoint 0 has moved, this will force a recalculation of the radius.
                    // Move endpoint 1 to suit the new radius
                    float rad = length(ae->centre, e->endpoints[0]);
                    Plane e1;

                    e1.A = e->endpoints[1]->x - ae->centre->x;
                    e1.B = e->endpoints[1]->y - ae->centre->y;
                    e1.C = e->endpoints[1]->z - ae->centre->z;
                    normalise_plane(&e1);
                    e->endpoints[1]->x = ae->centre->x + e1.A * rad;
                    e->endpoints[1]->y = ae->centre->y + e1.B * rad;
                    e->endpoints[1]->z = ae->centre->z + e1.C * rad;
                }
                else if (p == e->endpoints[1])
                {
                    // The other endpoint has moved. Update the first one similarly
                    float rad = length(ae->centre, e->endpoints[1]);
                    Plane e0;

                    e0.A = e->endpoints[0]->x - ae->centre->x;
                    e0.B = e->endpoints[0]->y - ae->centre->y;
                    e0.C = e->endpoints[0]->z - ae->centre->z;
                    normalise_plane(&e0);
                    e->endpoints[0]->x = ae->centre->x + e0.A * rad;
                    e->endpoints[0]->y = ae->centre->y + e0.B * rad;
                    e->endpoints[0]->z = ae->centre->z + e0.C * rad;
                }
                else if (p == ae->centre)
                {
                    // The centre has moved. We are moving the whole edge.
                    e->endpoints[0]->x += dx;
                    e->endpoints[0]->y += dy;
                    e->endpoints[0]->z += dz;
                    e->endpoints[1]->x += dx;
                    e->endpoints[1]->y += dy;
                    e->endpoints[1]->z += dz;
                }
            }
            break;

        case EDGE_BEZIER:
            ((Edge *)parent)->view_valid = FALSE;
            break;
        }
    }
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
free_view_list_edge(Edge *edge)
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
    Point *last_point;
    Point *p, *v;
    BOOL arc_cw;
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

#if DEBUG_VIEW_LIST_RECT_FACE
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
        switch (e->type & ~EDGE_CONSTRUCTION)
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
            gen_view_list_arc((ArcEdge *)e);

            // Circles have arcs with shared coincident endpoints. In this case, the clockwiseness
            // of the are will determine whether the view_list is copied forwards or backwards
            // (since in the below, the last_point will always match endpoint 0)
            if (e->endpoints[0] == e->endpoints[1])
                arc_cw = ((ArcEdge *)e)->clockwise;
            else
                arc_cw = FALSE;

            goto copy_view_list;

        case EDGE_BEZIER:
            gen_view_list_bez((BezierEdge *)e);
        copy_view_list:
            if (last_point == e->endpoints[0] && !arc_cw)
            {
                last_point = e->endpoints[1];
                
                // copy the view list forwards. Skip the first point as it has already been added
                for (v = (Point *)e->view_list->hdr.next; v != NULL; v = (Point *)v->hdr.next)
                {
                    p = point_newp(v);
                    p->hdr.ID = 0;
                    objid--;
                    link_tail((Object *)p, (Object **)&face->view_list);
                }
            }
            else
            {
                ASSERT(last_point == e->endpoints[1], "Point order messed up");
                last_point = e->endpoints[0];

                // copy the view list backwards, skipping the last point.
                for (v = (Point *)e->view_list; v->hdr.next->next != NULL; v = (Point *)v->hdr.next)
                    ;

                for ( ; v != NULL; v = (Point *)v->hdr.prev)
                {
                    p = point_newp(v);
                    p->hdr.ID = 0;
                    objid--;
                    link_tail((Object *)p, (Object **)&face->view_list);
                }
            }

            p = point_newp(last_point);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&face->view_list);
            break;
        }
    }

    face->view_valid = TRUE;

    // calculate the normal vector.  Store a new refpt here too, in case something has moved.
    polygon_normal(face->view_list, &face->normal);
    face->normal.refpt = *face->edges[0]->endpoints[0];
}

// Generate the view list for an arc edge.
void
gen_view_list_arc(ArcEdge *ae)
{
    Edge *edge = (Edge *)ae;
    Plane n = ae->normal;
    Point *p;
    float rad = length(ae->centre, edge->endpoints[0]);
    float t, theta, step;
    float matrix[16];
    float v[4];
    float res[4];
    int i;

    if (edge->view_valid)
        return;

    free_view_list_edge(edge);

    // transform arc to XY plane, centre at origin, endpoint 0 on x axis
    look_at_centre(*ae->centre, *edge->endpoints[0], n, matrix);

    // angle between two vectors c-p0 and c-p1. If the points are the same, we are
    // drawing a full circle.
    if (edge->endpoints[0] == edge->endpoints[1])
        theta = ae->clockwise ? -2 * PI : 2 * PI;
    else
        theta = angle3(edge->endpoints[0], ae->centre, edge->endpoints[1], &n);
    
    // step for angle. This may be fixed in advance.
    if (edge->stepsize != 0)
        step = edge->stepsize;
    else
        step = 2.0f * acosf(1.0f - tolerance / rad);
    i = 0;

    if (ae->clockwise)  // Clockwise angles go negative
    {
#ifdef DEBUG_VIEW_LIST_ARC
        Log("Clockwise arc:");
#endif
        if (theta > 0)
            theta -= 2 * PI;  

        // draw arc from p1 (on x axis) to p2. 
        for (t = 0, i = 0; t > theta; t -= step, i++)
        {
            v[0] = rad * cosf(t);
            v[1] = rad * sinf(t);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            p = point_new(res[0], res[1], res[2]);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&edge->view_list);
#ifdef DEBUG_VIEW_LIST_ARC
            {
                char buf[64];
                sprintf_s(buf, 64, "%f %f %f\r\n", res[0], res[1], res[2]);
                Log(buf);
            }
#endif
        }
    }
    else
    {
#ifdef DEBUG_VIEW_LIST_ARC
        Log("Anticlockwise arc:");
#endif
        if (theta < 0)
            theta += 2 * PI;

        for (t = 0, i = 0; t < theta; t += step, i++)  
        {
            v[0] = rad * cosf(t);
            v[1] = rad * sinf(t);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            p = point_new(res[0], res[1], res[2]);
            p->hdr.ID = 0;
            objid--;
            link_tail((Object *)p, (Object **)&edge->view_list);
#ifdef DEBUG_VIEW_LIST_ARC
            {
                char buf[64];
                sprintf_s(buf, 64, "%f %f %f\r\n", res[0], res[1], res[2]);
                Log(buf);
            }
#endif
        }
    }

    edge->nsteps = i;

    // Make sure the last point is in the view list
    p = point_newp(edge->endpoints[1]);
    p->hdr.ID = 0;
    objid--;
    link_tail((Object *)p, (Object **)&edge->view_list);

    edge->view_valid = TRUE;
}

// Length squared shortcut
#define LENSQ(x1, y1, z1, x2, y2, z2) ((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) + (z2-z1)*(z2-z1))

// Recursive bezier edge drawing.
void
recurse_bez
(
    BezierEdge *be, 
    float x1, float y1, float z1, 
    float x2, float y2, float z2, 
    float x3, float y3, float z3, 
    float x4, float y4, float z4
)
{
    Point *p;
    Edge *e = (Edge *)be;

    // Calculate all the mid-points of the line segments
    float x12 = (x1 + x2) / 2;
    float y12 = (y1 + y2) / 2;
    float z12 = (z1 + z2) / 2;

    float x23 = (x2 + x3) / 2;
    float y23 = (y2 + y3) / 2;
    float z23 = (z2 + z3) / 2;

    float x34 = (x3 + x4) / 2;
    float y34 = (y3 + y4) / 2;
    float z34 = (z3 + z4) / 2;

    float x123 = (x12 + x23) / 2;
    float y123 = (y12 + y23) / 2;
    float z123 = (z12 + z23) / 2;

    float x234 = (x23 + x34) / 2;
    float y234 = (y23 + y34) / 2;
    float z234 = (z23 + z34) / 2;

    float x1234 = (x123 + x234) / 2;
    float y1234 = (y123 + y234) / 2;
    float z1234 = (z123 + z234) / 2;

    float x14 = (x1 + x4) / 2;
    float y14 = (y1 + y4) / 2;
    float z14 = (z1 + z4) / 2;

    // Do a length test < the grid unit, and a curve flatness test < the tolerance.
    // Test length squared (to save the sqrts)
    if 
    (
        LENSQ(x1, y1, z1, x4, y4, z4) < grid_snap * grid_snap 
        || 
        LENSQ(x1234, y1234, z1234, x14, y14, z14) < tolerance * tolerance
    )
    {
        // Add (x4, y4, z4) as a point to the view list
        p = point_new(x4, y4, z4);
        p->hdr.ID = 0;
        objid--;
        link_tail((Object *)p, (Object **)&e->view_list);
        e->nsteps++;
    }
    else
    {
        // Continue subdivision
        recurse_bez(be, x1, y1, z1, x12, y12, z12, x123, y123, z123, x1234, y1234, z1234);
        recurse_bez(be, x1234, y1234, z1234, x234, y234, z234, x34, y34, z34, x4, y4, z4);
    }
}

// Generate the view list for a bezier edge.
void
gen_view_list_bez(BezierEdge *be)
{
    Edge *e = (Edge *)be;
    Point *p;

    if (e->view_valid)
        return;

    free_view_list_edge(e);

    // Put the first endpoint on the view list
    p = point_newp(e->endpoints[0]);
    p->hdr.ID = 0;
    objid--;
    link_tail((Object *)p, (Object **)&e->view_list);
    e->nsteps = 0;

    // TODO: fixed step division if stepsize > 0

    // Subdivide the bezier
    recurse_bez
    (
        be,
        e->endpoints[0]->x, e->endpoints[0]->y, e->endpoints[0]->z,
        be->ctrlpoints[0]->x, be->ctrlpoints[0]->y, be->ctrlpoints[0]->z,
        be->ctrlpoints[1]->x, be->ctrlpoints[1]->y, be->ctrlpoints[1]->z,
        e->endpoints[1]->x, e->endpoints[1]->y, e->endpoints[1]->z
    );

    e->view_valid = TRUE;
}

// Purge an object. Points are put in the free list.
void
purge_obj(Object *obj)
{
    int i;
    Edge *e;
    ArcEdge *ae;
    BezierEdge *be;
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
        e = (Edge *)obj;
        purge_obj((Object *)e->endpoints[0]);
        purge_obj((Object *)e->endpoints[1]);
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            purge_obj((Object *)ae->centre);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            purge_obj((Object *)be->ctrlpoints[0]);
            purge_obj((Object *)be->ctrlpoints[1]);
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

