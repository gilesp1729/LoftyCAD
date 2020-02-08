#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>


// Free lists for Points and Objects. Only singly linked.
ListHead free_list_pt = { NULL, NULL };
ListHead free_list_obj = { NULL, NULL };

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
    }

    obj->type = OBJ_NONE;
    obj->ID = 0;
    return obj;
}

Point *point_new(float x, float y, float z)
{
    Point   *pt;
    
    // Try and obtain a point from the free list first
    if (free_list_pt.head != NULL)
    {
        pt = (Point *)free_list_pt.head;
        free_list_pt.head = free_list_pt.head->next;
        if (free_list_pt.head == NULL)
            free_list_pt.tail = NULL;
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

    if (free_list_pt.head != NULL)
    {
        pt = (Point *)free_list_pt.head;
        free_list_pt.head = free_list_pt.head->next;
        if (free_list_pt.head == NULL)
            free_list_pt.tail = NULL;
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
        se->edge.hdr.show_dims = edge_type & EDGE_CONSTRUCTION;
        se->edge.type = edge_type;
        return (Edge *)se;

    case EDGE_ARC:
        ae = calloc(1, sizeof(ArcEdge));
        ae->edge.hdr.type = OBJ_EDGE;
        ae->edge.hdr.ID = objid++;
        ae->edge.hdr.show_dims = edge_type & EDGE_CONSTRUCTION;
        ae->edge.type = edge_type;
        return (Edge *)ae;

    case EDGE_BEZIER:
        be = calloc(1, sizeof(BezierEdge));
        be->edge.hdr.type = OBJ_EDGE;
        be->edge.hdr.ID = objid++;
        be->edge.hdr.show_dims = edge_type & EDGE_CONSTRUCTION;
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

    // Have a stab at allocating the edge array
    switch (face_type & ~FACE_CONSTRUCTION)
    {
    case FACE_RECT:
    case FACE_CIRCLE:
        face->hdr.show_dims = face_type & FACE_CONSTRUCTION;
        // fallthrough
    case FACE_CYLINDRICAL:
        face->max_edges = 4;
        break;

    default:  // general and flat faces may have many edges
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
    grp->hdr.lock = LOCK_FACES;
    grp->op = OP_NONE;
    clear_bbox(&grp->bbox);
    return grp;
}

Transform *xform_new(void)
{
    Transform *xform = calloc(1, sizeof(Transform));

    xform->hdr.type = OBJ_TRANSFORM;
    xform->hdr.ID = 0;  // these do not use object ID's
    xform->sx = 1.0f;   // set up a unity transform with no rotation. Nothing is enabled.
    xform->sy = 1.0f;
    xform->sz = 1.0f;
    return xform;
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
    Group *grp;
    Object *o;

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
        for (p = (Point *)face->view_list.head; p != NULL; p = (Point *)p->hdr.next)
            clear_move_copy_flags((Object *)p);
        if (face->text != NULL)             // and the text positions
        {
            clear_move_copy_flags((Object *)&face->text->origin);
            clear_move_copy_flags((Object *)&face->text->endpt);
        }

        obj->copied_to = NULL;
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
            clear_move_copy_flags((Object *)face);
        obj->copied_to = NULL;
        break;

    case OBJ_GROUP:
        grp = (Group *)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            clear_move_copy_flags(o);
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
    BezierEdge *be, *nbe;
    Face *face, *new_face;
    Volume *vol, *new_vol;
    Object *o;
    Group *grp, *new_grp;

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

            case EDGE_BEZIER:
                be = (BezierEdge *)edge;
                nbe = (BezierEdge *)new_edge;
                nbe->ctrlpoints[0] = (Point *)copy_obj((Object *)be->ctrlpoints[0], xoffset, yoffset, zoffset);
                nbe->ctrlpoints[1] = (Point *)copy_obj((Object *)be->ctrlpoints[1], xoffset, yoffset, zoffset);
                break;
            }
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        new_face = face_new(face->type, face->normal);
        new_obj = (Object *)new_face;
        new_obj->lock = obj->lock;

        // Realloc the edge array if we need a big one
        if (face->n_edges >= new_face->max_edges)
        {
            new_face->max_edges = face->max_edges;
            new_face->edges = realloc(new_face->edges, new_face->max_edges * sizeof(Edge *));
        }
        new_face->n_edges = face->n_edges;

        // Alloc and copy any contour array. Don't worry about the power of 2 thing as it will
        // not be extended again.
        if (face->n_contours != 0)
        {
            new_face->n_contours = face->n_contours;
            new_face->contours = calloc(new_face->n_contours, sizeof(Contour));
            memcpy(new_face->contours, face->contours, face->n_contours * sizeof(Contour));
        }

        // Copy the edges
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            new_edge = (Edge *)copy_obj((Object *)edge, xoffset, yoffset, zoffset);
            new_face->edges[i] = new_edge;
        }

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
        for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
        {
            new_face = (Face *)copy_obj((Object *)face, xoffset, yoffset, zoffset);
            new_face->vol = new_vol;
            link_tail((Object *)new_face, &new_vol->faces);
        }
        if (vol->xform != NULL)
        {
            new_vol->xform = xform_new();
            *new_vol->xform = *vol->xform;
            new_vol->xform->xc += xoffset;
            new_vol->xform->yc += yoffset;
            new_vol->xform->zc += zoffset;
        }
        new_vol->extrude_height = vol->extrude_height;
        new_vol->op = vol->op;
        break;

    case OBJ_GROUP:
        grp = (Group *)obj;
        new_grp = group_new();
        for (o = grp->obj_list.head; o != NULL; o = o->next)
        {
            new_obj = copy_obj(o, xoffset, yoffset, zoffset);
            link_tail_group(new_obj, new_grp);
        }
        new_obj = (Object *)new_grp;
        if (grp->xform != NULL)
        {
            new_grp->xform = xform_new();
            *new_grp->xform = *grp->xform;
            new_grp->xform->xc += xoffset;
            new_grp->xform->yc += yoffset;
            new_grp->xform->zc += zoffset;
        }
        new_grp->op = grp->op;
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
    int i, idx, c;
    //char buf[256];

    // Swap the normal around
    clone->normal.A = -clone->normal.A;
    clone->normal.B = -clone->normal.B;
    clone->normal.C = -clone->normal.C;
    clone->n_edges = face->n_edges;
    if (face->max_edges > clone->max_edges)   // in case it has been grown before being cloned
        clone->edges = realloc(clone->edges, face->max_edges * sizeof(Edge *));
    clone->max_edges = face->max_edges;
    
    // associate the face with its clone by setting the extrude flag
    clone->extruded = TRUE;
    face->extruded = TRUE;

    // If the face does not have a contour array, create one here for the face and its clone.
    // The extrusion code needs to use it later, and it makes everything simpler here as well.
    if (face->contours == NULL)
    {
        face->contours = calloc(1, sizeof(Contour));
        face->n_contours = 1;
        face->contours[0].edge_index = 0;
        if (face->initial_point == face->edges[0]->endpoints[0])
            face->contours[0].ip_index = 0;
        else
            face->contours[0].ip_index = 1;
        face->contours[0].n_edges = face->n_edges;
    }

    // Copy the contour array to the clone. Its IP's will be swapped later.
    clone->n_contours = face->n_contours;
    clone->contours = calloc(clone->n_contours, sizeof(Contour));
    memcpy(clone->contours, face->contours, face->n_contours * sizeof(Contour));

    // Clone and reverse each contour separately
    for (c = 0; c < face->n_contours; c++)
    {
        int ei = face->contours[c].edge_index;

        // Set the initial point for the contour. 
        last_point = face->edges[ei]->endpoints[face->contours[c].ip_index];

        // Copy the edges, reversing the order
        for (i = 0; i < face->contours[c].n_edges; i++)
        {
            e = (Object *)face->edges[ei + i];
            ne = copy_obj(e, 0, 0, 0);
            if (i == 0)
                idx = 0;
            else
                idx = face->contours[c].n_edges - i;
            clone->edges[ei + idx] = (Edge *)ne;

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
                clone->contours[c].ip_index = idx;
                //clone->initial_point = ((Edge *)ne)->endpoints[idx];

            switch (face->edges[ei + i]->type)
            {
            case EDGE_ARC:
                ae = (ArcEdge *)e;
                nae = (ArcEdge *)ne;

                // Reverse the arc
                nae->clockwise = !ae->clockwise;
                nae->normal.A = -ae->normal.A;
                nae->normal.B = -ae->normal.B;
                nae->normal.C = -ae->normal.C;
                // fall through
            case EDGE_BEZIER:
                // Since the arc/bezier and its clone now belong to extruded faces,
                // fix their stepsize
                ((Edge *)ne)->stepsize = ((Edge *)e)->stepsize;
                ((Edge *)ne)->nsteps = ((Edge *)e)->nsteps;
                ((Edge *)e)->stepping = TRUE;
                ((Edge *)ne)->stepping = TRUE;
                break;
            }
        }
    }
    
    clone->initial_point = clone->edges[clone->contours[0].edge_index]->endpoints[clone->contours[0].ip_index];

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
    Group *grp;
    Object *o;

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
        if (face->text != NULL)             // and the text positions
        {
            face->text->origin.x += xoffset;
            face->text->origin.y += yoffset;
            face->text->origin.z += zoffset;
            face->text->endpt.x += xoffset;
            face->text->endpt.y += yoffset;
            face->text->endpt.z += zoffset;
        }
        face->view_valid = FALSE;
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
            move_obj((Object *)face, xoffset, yoffset, zoffset);
        if (vol->xform != NULL)
        {
            vol->xform->xc += xoffset;
            vol->xform->yc += yoffset;
            vol->xform->zc += zoffset;
        }
        break;

    case OBJ_GROUP:
        grp = (Group *)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            move_obj(o, xoffset, yoffset, zoffset);
        if (grp->xform != NULL)
        {
            grp->xform->xc += xoffset;
            grp->xform->yc += yoffset;
            grp->xform->zc += zoffset;
        }
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
// The parent object maintains the lock on all its components. Will not return a group.
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

    for (top_level = tree->obj_list.head; top_level != NULL; top_level = top_level->next)
    {
        if (top_level->type == OBJ_GROUP)
        {
            if (deep_search)
            {
                Object *o = find_parent_object((Group *)top_level, obj, deep_search);
                if (o != NULL)
                    return top_level;
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

// Build xform list for volume, and groups leading to the volume
void
build_parent_xform_list(Object *obj, Object *parent, ListHead *xform_list)
{
    Object *top_parent = parent;

    xform_list->head = NULL;
    xform_list->tail = NULL;
    if (parent == NULL)
        return;

    if (obj->type < OBJ_VOLUME && parent->type == OBJ_VOLUME)
    {
        if (((Volume *)parent)->xform != NULL)
            link((Object *)((Volume *)parent)->xform, xform_list);
        for (; top_parent->parent_group->hdr.parent_group != NULL; top_parent = (Object *)top_parent->parent_group)
        {
            if (((Group *)top_parent)->xform != NULL)
                link((Object *)((Group *)top_parent)->xform, xform_list);
        }
    }
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
        obj->next = (Object *)free_list_pt.head;
        if (free_list_pt.head == NULL)
            free_list_pt.tail = obj;
        free_list_pt.head = obj;
        obj->ID = 0;
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
        if (top_type <= OBJ_FACE)       // If this edge is not part of a volume, we can safely delete it
            free(obj);
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
        if (vol->xform != NULL)
            free(vol->xform);
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
        if (group->xform != NULL)
            free(group->xform);
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


