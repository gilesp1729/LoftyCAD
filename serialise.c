#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// A constantly incrementing ID. 
// Should not need to worry about it overflowing 32-bits (4G objects!)
// Start at 1 as an ID of zero is used to check for an unreferenced object.
unsigned int objid = 1;

// The highest object ID encountered when reading in a file.
static unsigned int maxobjid = 0;

// The save count for the tree. Used to protect against multiple writing 
// of shared objects.
static unsigned int save_count = 1;

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
    BezierEdge *be;
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
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            serialise_obj((Object *)ae->centre, f);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            serialise_obj((Object *)be->ctrlpoints[0], f);
            serialise_obj((Object *)be->ctrlpoints[1], f);
            break;
        }
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
            fprintf_s(f, "%s %d %f %f %f\n",
                      ae->clockwise ? "C" : "AC",
                      ae->centre->hdr.ID,
                      ae->normal.A, ae->normal.B, ae->normal.C);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            fprintf_s(f, "%d %d\n", be->ctrlpoints[0]->hdr.ID, be->ctrlpoints[1]->hdr.ID);
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
            int end0, end1, ctrl0, ctrl1, ctr;
            Edge *edge;
            ArcEdge *ae;
            BezierEdge *be;

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
                ctr = atoi(tok);
                ASSERT(ctr > 0 && object[ctr] != NULL, "Bad centre point ID");
                ae->centre = (Point *)object[ctr];

                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.A = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.B = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.C = (float)atof(tok);
            }
            else if (strcmp(tok, "BEZIER") == 0)
            {
                edge = edge_new(EDGE_BEZIER);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok);
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok);
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                edge->endpoints[0] = (Point *)object[end0];
                edge->endpoints[1] = (Point *)object[end1];

                tok = strtok_s(NULL, " \t\n", &nexttok);
                ctrl0 = atoi(tok);
                ASSERT(ctrl0 > 0 && object[ctrl0] != NULL, "Bad control point ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ctrl1 = atoi(tok);
                ASSERT(ctrl1 > 0 && object[ctrl1] != NULL, "Bad control point ID");
                be = (BezierEdge *)edge;
                be->ctrlpoints[0] = (Point *)object[ctrl0];
                be->ctrlpoints[1] = (Point *)object[ctrl1];
            }
            else
            {
                ASSERT(FALSE, "Bad edge type");
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
            else if (strcmp(tok, "CIRCLE") == 0)
            {
                type = FACE_CIRCLE;
            }
            else if (strcmp(tok, "FLAT") == 0)
            {
                type = FACE_FLAT;
            }
            else if (strcmp(tok, "CYLINDRICAL") == 0)
            {
                type = FACE_CYLINDRICAL;
            }
            else
            {
                // TODO other types
                ASSERT(FALSE, "Deserialise Face (general) Not implemented");
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

            while (TRUE)
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

