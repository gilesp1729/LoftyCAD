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
    Object *obj = malloc(sizeof(Object));

    obj->type = OBJ_NONE;
    obj->ID = 0;
    obj->next = NULL;
    obj->prev = NULL;
    obj->save_count = 0;
    obj->copied_to = NULL;
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
    }
    else
    {
        pt = malloc(sizeof(Point));
    }

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = objid++;
    pt->hdr.next = NULL;
    pt->hdr.prev = NULL;
    pt->x = x;
    pt->y = y;
    pt->z = z;
    pt->hdr.copied_to = NULL;
    pt->hdr.save_count = 0;
    pt->moved = FALSE;
    return pt;
}

// Copy just the coordinates from the given point.
Point *point_newp(Point *p)
{
    Point   *pt = malloc(sizeof(Point));

    pt->hdr.type = OBJ_POINT;
    pt->hdr.ID = objid++;
    pt->hdr.next = NULL;
    pt->hdr.prev = NULL;
    pt->x = p->x;
    pt->y = p->y;
    pt->z = p->z;
    pt->hdr.copied_to = NULL;
    pt->hdr.save_count = 0;
    return pt;
}

// Edges. 
Edge *edge_new(EDGE edge_type)
{
    StraightEdge *se;
    CircleEdge *ce;
    ArcEdge *ae;
    BezierEdge *be;

    switch (edge_type & ~EDGE_CONSTRUCTION)
    {
    case EDGE_STRAIGHT:
    default:  // just to shut compiler up
        se = malloc(sizeof(StraightEdge));
        se->edge.hdr.type = OBJ_EDGE;
        se->edge.hdr.ID = objid++;
        se->edge.hdr.next = NULL;
        se->edge.hdr.prev = NULL;
        se->edge.hdr.copied_to = NULL;
        se->edge.hdr.save_count = 0;
        se->edge.type = edge_type;
        return (Edge *)se;

    case EDGE_CIRCLE:
        ce = malloc(sizeof(CircleEdge));
        ce->edge.hdr.type = OBJ_EDGE;
        ce->edge.hdr.ID = objid++;
        ce->edge.hdr.next = NULL;
        ce->edge.hdr.prev = NULL;
        ce->edge.hdr.copied_to = NULL;
        ce->edge.hdr.save_count = 0;
        ce->edge.type = edge_type;
        return (Edge *)ce;

    case EDGE_ARC:
        ae = malloc(sizeof(ArcEdge));
        ae->edge.hdr.type = OBJ_EDGE;
        ae->edge.hdr.ID = objid++;
        ae->edge.hdr.next = NULL;
        ae->edge.hdr.prev = NULL;
        ae->edge.hdr.copied_to = NULL;
        ae->edge.hdr.save_count = 0;
        ae->edge.type = edge_type;
        return (Edge *)ae;

    case EDGE_BEZIER:
        be = malloc(sizeof(BezierEdge));
        be->edge.hdr.type = OBJ_EDGE;
        be->edge.hdr.ID = objid++;
        be->edge.hdr.next = NULL;
        be->edge.hdr.prev = NULL;
        be->edge.hdr.copied_to = NULL;
        be->edge.hdr.save_count = 0;
        be->edge.type = edge_type;
        return (Edge *)be;
    }
}

Face *face_new(Plane norm)
{
    Face *face = malloc(sizeof(Face));

    face->hdr.type = OBJ_FACE;
    face->hdr.ID = objid++;
    face->hdr.next = NULL;
    face->hdr.prev = NULL;
    face->hdr.copied_to = NULL;
    face->hdr.save_count = 0;
    face->normal = norm;
    face->vol = NULL;
    face->view_list = NULL;
    face->edges = NULL;
    face->view_valid = FALSE;
    return face;
}

Volume *vol_new(Face *attached_to)
{
    Volume *vol = malloc(sizeof(Volume));

    vol->hdr.type = OBJ_VOLUME;
    vol->hdr.ID = objid++;
    vol->hdr.next = NULL;
    vol->hdr.prev = NULL;
    vol->hdr.copied_to = NULL;
    vol->hdr.save_count = 0;
    vol->faces = NULL;
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
    Point *p;
    EDGE type;
    StraightEdge *se;
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
        switch (type)
        {
        case EDGE_STRAIGHT:
            se = (StraightEdge *)obj;
            clear_move_copy_flags((Object *)se->endpoints[0]);
            clear_move_copy_flags((Object *)se->endpoints[1]);
            obj->copied_to = NULL;
            break;

        case EDGE_CIRCLE:     // TODO others
        case EDGE_ARC:
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }

        break;

    case OBJ_FACE:
        face = (Face *)obj;
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
            clear_move_copy_flags((Object *)edge);
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
    Object *new_obj = NULL;
    Point *p;
    EDGE type;
    StraightEdge *se, *ne;
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
            new_obj = (Object *)edge_new(((Edge *)obj)->type);
            obj->copied_to = new_obj;

            type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
            switch (type)
            {
            case EDGE_STRAIGHT:
                se = (StraightEdge *)obj;
                ne = (StraightEdge *)new_obj;
                ne->endpoints[0] = (Point *)copy_obj((Object *)se->endpoints[0], xoffset, yoffset, zoffset);
                ne->endpoints[1] = (Point *)copy_obj((Object *)se->endpoints[1], xoffset, yoffset, zoffset);
                break;

            case EDGE_CIRCLE:     // TODO others
            case EDGE_ARC:
            case EDGE_BEZIER:
                ASSERT(FALSE, "Not implemented");
                break;
            }
        }

        break;

    case OBJ_FACE:
        face = (Face *)obj;
        new_face = face_new(face->normal);
        new_obj = (Object *)new_face;
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
        {
            new_edge = (Edge *)copy_obj((Object *)edge, xoffset, yoffset, zoffset);
            link_tail((Object *)new_edge, (Object **)&new_face->edges);
        }
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

// Move any object by an offset on all its point coordinates.
void
move_obj(Object *obj, float xoffset, float yoffset, float zoffset)
{
    Point *p;
    EDGE type;
    StraightEdge *se;
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
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_STRAIGHT:
            se = (StraightEdge *)obj;
            move_obj((Object *)se->endpoints[0], xoffset, yoffset, zoffset);
            move_obj((Object *)se->endpoints[1], xoffset, yoffset, zoffset);
            break;

        case EDGE_CIRCLE:     // TODO others
        case EDGE_ARC:
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }

        break;

    case OBJ_FACE:
        face = (Face *)obj;
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
            move_obj((Object *)edge, xoffset, yoffset, zoffset);
        for (p = face->view_list; p != NULL; p = (Point *)p->hdr.next)
            move_obj((Object *)p, xoffset, yoffset, zoffset);
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
    EDGE type;
    StraightEdge *se;
    Edge *edge;
    Face *face;
    Volume *vol;

    switch (parent->type)
    {
    case OBJ_POINT:
        ASSERT(FALSE, "A Point cannot be a parent");
        return FALSE;

    case OBJ_EDGE:
        type = ((Edge *)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_STRAIGHT:
            se = (StraightEdge *)obj;
            if ((Object *)se->endpoints[0] == obj)
                return TRUE;
            if ((Object *)se->endpoints[1] == obj)
                return TRUE;
            break;

        case EDGE_CIRCLE:     // TODO others
        case EDGE_ARC:
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }

        break;

    case OBJ_FACE:
        face = (Face *)parent;
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
        {
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

// Clean out the view list for a face, by putting all the points on the free list.
// The points already have ID's of 0.
void
free_view_list(Face *face)
{
    Point *p;

    if (free_list == NULL)
    {
        free_list = face->view_list;
    }
    else
    {
        for (p = free_list; p->hdr.next != NULL; p = (Point *)p->hdr.next)
            ;   // run down to the last free element
        p->hdr.next = (Object *)face->view_list;
    }
    face->view_list = NULL;
    face->view_valid = FALSE;
}

// Regenerate the view list for a face. While here, also calculate the outward
// normal for the face.
void
gen_view_list(Face *face)
{
    Edge *e;
    BOOL start_point;
    Point *p;
    StraightEdge *se;

    if (face->view_valid)
        return;

    free_view_list(face);

    // Add points at tail of list, to preserve order
    start_point = FALSE;
    for (e = face->edges; e != NULL; e = (Edge *)e->hdr.next)
    {
        if (!start_point)
        {
            // First the start point
            switch (e->type)
            {
            case EDGE_STRAIGHT:
                se = (StraightEdge *)e;
                p = point_newp(se->endpoints[0]);
                p->hdr.ID = 0;
                link_tail((Object *)p, (Object **)&face->view_list);
                break;

            case EDGE_CIRCLE:     // TODO others
            case EDGE_ARC:
            case EDGE_BEZIER:
                ASSERT(FALSE, "Not implemented");
                break;
            }
            start_point = TRUE;
        }

        // Then the subsequent points, assuming the edges follow on
        switch (e->type)
        {
        case EDGE_STRAIGHT:
            se = (StraightEdge *)e;
            p = point_newp(se->endpoints[1]);
            p->hdr.ID = 0;
            link_tail((Object *)p, (Object **)&face->view_list);
            break;

        case EDGE_CIRCLE:     // TODO others
        case EDGE_ARC:
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
    }

    face->view_valid = TRUE;

    // calculate the normal vector
    normal(face->view_list, &face->normal);
}

// Purge an object. Points are put in the free list.
void
purge_obj(Object *obj)
{
    StraightEdge *se;
    EDGE type;
    Face *face;
    Edge *edge;
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
            se = (StraightEdge *)obj;
            purge_obj((Object *)se->endpoints[0]);
            purge_obj((Object *)se->endpoints[1]);
            break;

        case EDGE_CIRCLE:     // TODO others. Be very careful if Points are in a list, as freeing them will cause problems...
        case EDGE_ARC:
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        free_view_list(face);
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
            purge_obj((Object *)edge);
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            purge_obj((Object *)face);
        break;
    }
}

// Purge a tree, freeing everything in it, except for Points, which are
// placed in the free list.
void
purge_tree(Object *tree)
{
    Object *obj;

    for (obj = tree; obj != NULL; obj = obj->next)
        purge_obj(obj);
}

// names of things that make the serialised format a little easier to read
char *objname[] = { "POINT", "EDGE", "FACE", "VOLUME" };
char *edgetypes[] = { "STRAIGHT", "CIRCLE", "ARC", "BEZIER" };
char *facetypes[] = { "RECT", "CIRCLE", "FLAT", "CYLINDRICAL", "GENERAL" };

// Serialise an object. Children go out before their parents, in general.
void
serialise_obj(Object *obj, FILE *f)
{
    StraightEdge *se;
    EDGE type, constr;
    Face *face;
    Edge *edge;
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
        switch (type)
        {
        case EDGE_STRAIGHT:
            fprintf_s(f, "BEGIN %d\n", obj->ID);
            se = (StraightEdge *)obj;
            serialise_obj((Object *)se->endpoints[0], f);
            serialise_obj((Object *)se->endpoints[1], f);
            break;

        case EDGE_CIRCLE:     // TODO others
        case EDGE_ARC:
        case EDGE_BEZIER:
            ASSERT(FALSE, "Not implemented");
            break;
        }
        break;

    case OBJ_FACE:
        fprintf_s(f, "BEGIN %d\n", obj->ID);
        face = (Face *)obj;
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
            serialise_obj((Object *)edge, f);
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            serialise_obj((Object *)face, f);
        break;
    }

    fprintf_s(f, "%s %d ", objname[obj->type], obj->ID);
    switch (obj->type)
    {
    case OBJ_POINT:
        fprintf_s(f, "%f %f %f\n", ((Point *)obj)->x, ((Point *)obj)->y, ((Point *)obj)->z);
        break;

    case OBJ_EDGE:
        fprintf_s(f, "%s%s ", edgetypes[type], constr ? "(C)" : "");
        switch (type)
        {
        case EDGE_STRAIGHT:
            se = (StraightEdge *)obj;
            fprintf_s(f, "%d %d\n", se->endpoints[0]->hdr.ID, se->endpoints[1]->hdr.ID);
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        fprintf_s(f, "%s %f %f %f %f %f %f ",
            facetypes[face->type],
            face->normal.refpt.x, face->normal.refpt.y, face->normal.refpt.z,
            face->normal.A, face->normal.B, face->normal.C);
        for (edge = face->edges; edge != NULL; edge = (Edge *)edge->hdr.next)
            fprintf_s(f, "%d ", edge->hdr.ID);
        fprintf_s(f, "\n");
        break;

    case OBJ_VOLUME:

        // TODO
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

        if (fgets(buf, 512, f) == NULL)
            break;

        tok = strtok_s(buf, " \t\n", &nexttok);
        if (strcmp(tok, "BEGIN") == 0)
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

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strcmp(tok, "STRAIGHT") == 0)
            {
                StraightEdge *se;

                edge = edge_new(EDGE_STRAIGHT);
                se = (StraightEdge *)edge;
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok);
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok);
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                se->endpoints[0] = (Point *)object[end0];
                se->endpoints[1] = (Point *)object[end1];
            }
            else
            {
                // TODO other edge types


            }
            edge->hdr.ID = id;
            object[id] = (Object *)edge;
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed edge record");
            stkptr--;
            if (stkptr == 0)
                link_tail((Object *)edge, tree);
        }
        else if (strcmp(tok, "FACE") == 0)
        {
            Face *face;
            Plane norm;
            FACE type;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strcmp(tok, "RECT") == 0)
            {
                type = FACE_RECT;
            }
            else
            {
                // TODO other types


            }

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

            face = face_new(norm);
            face->type = type;

            while(TRUE)
            {
                int eid;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                eid = atoi(tok);
                ASSERT(eid > 0 && object[eid] != NULL, "Bad edge ID");
                link_tail(object[eid], (Object **)&face->edges);
            }

            face->hdr.ID = id;
            object[id] = (Object *)face;
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed face record");
            stkptr--;
            if (stkptr == 0)
                link_tail((Object *)face, tree);
        }
        else if (strcmp(tok, "VOLUME") == 0)
        {
#if 0 // TODO vol record
            Volume *vol;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            check_and_grow(id, &object, &objsize);


            vol->hdr.ID = id;
            object[id] = (Object *)vol;
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed volume record");
            stkptr--;
            ASSERT(stkptr == 0, "ID stack not empty");
            link_tail((Object *)vol, tree);
#endif
        }
    }

    objid = maxobjid + 1;
    save_count = 1;
    free(object);
    fclose(f);

    return TRUE;
}

