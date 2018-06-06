#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Version of output file
double file_version = 0.2;

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
char *objname[] = { "(none)", "POINT", "EDGE", "FACE", "VOLUME", "ENDGROUP" };
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
    Object *o;

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
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            serialise_obj((Object *)face, f);
        break;

    case OBJ_GROUP:
        fprintf_s(f, "BEGINGROUP %d %s\n", obj->ID, ((Group *)obj)->title);
        for (o = ((Group *)obj)->obj_list; o != NULL; o = o->next)
            serialise_obj(o, f);
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
        fprintf_s(f, "%s%s ", edgetypes[type], constr ? "(C)" : (obj->show_dims ? "(D)" : ""));
        e = (Edge *)obj;
        fprintf_s(f, "%d %d ", e->endpoints[0]->hdr.ID, e->endpoints[1]->hdr.ID);
        switch (type)
        {
        case EDGE_STRAIGHT:
            fprintf_s(f, "\n");
            break;

        case EDGE_ARC:
            ae = (ArcEdge *)obj;
            fprintf_s(f, "%s %d %f %f %f %d %f %d\n",
                      ae->clockwise ? "C" : "AC",
                      ae->centre->hdr.ID,
                      ae->normal.A, ae->normal.B, ae->normal.C,
                      e->stepping, e->stepsize, e->nsteps);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)obj;
            fprintf_s(f, "%d %d %d %f %d\n", 
                      be->ctrlpoints[0]->hdr.ID, be->ctrlpoints[1]->hdr.ID,
                      e->stepping, e->stepsize, e->nsteps);
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        fprintf_s(f, "%s%s %d %f %f %f %f %f %f ",
                  facetypes[face->type & ~FACE_CONSTRUCTION],
                  (face->type & FACE_CONSTRUCTION) ? "(C)" : (obj->show_dims ? "(D)" : ""),
                  face->initial_point->hdr.ID,
                  face->normal.refpt.x, face->normal.refpt.y, face->normal.refpt.z,
                  face->normal.A, face->normal.B, face->normal.C);
        for (i = 0; i < face->n_edges; i++)
            fprintf_s(f, "%d ", face->edges[i]->hdr.ID);
        fprintf_s(f, "\n");
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        for (face = vol->faces; face != NULL; face = (Face *)face->hdr.next)
            fprintf_s(f, "%d ", face->hdr.ID);
        fprintf_s(f, "\n");
        break;

    case OBJ_GROUP:
        fprintf_s(f, "\n");
        break;
    }

    obj->save_count = save_count;
}

// Serialise an object tree to a file.
void
serialise_tree(Group *tree, char *filename)
{
    FILE *f;
    Object *obj;

    fopen_s(&f, filename, "wt");
    fprintf_s(f, "LOFTYCAD %.1f\n", file_version);
    fprintf_s(f, "TITLE %s\n", tree->title);
    fprintf_s(f, "SCALE %f %f %f %d\n", half_size, grid_snap, tolerance, angle_snap);

    save_count++;
    for (obj = tree->obj_list; obj != NULL; obj = obj->next)
        serialise_obj(obj, f);

    // Write selection out.
    if (selection != NULL)
    {
        fprintf_s(f, "SELECTION ");
        for (obj = selection; obj != NULL; obj = obj->next)
            fprintf_s(f, "%d ", obj->prev->ID);
        fprintf_s(f, "\n");
    }

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
        int halfsize;

        // make sure the new bit of the object array is zeroed out after the realloc
        *object = (Object **)realloc(*object, sizeof(Object *) * (*objsize * 2));
        halfsize = sizeof(Object *) * (*objsize);
        memset((char *)(*object) + halfsize, 0, halfsize);
        *objsize *= 2;
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

#define IS_GROUP(obj) ((obj) != NULL && (obj)->type == OBJ_GROUP)

// TODO - this might be exceeded for vols consisting of large numbers of faces. make it dynamic somehow?
#define MAXLINE 1024

// Deserialise a tree from file. 
BOOL
deserialise_tree(Group *tree, char *filename, BOOL importing)
{
    FILE *f;
    char buf[MAXLINE];
    int stack[10], stkptr;
    int objsize = 1000;
    int id_offset;
    double version = 0.1;
    Object **object;
    Group *grp;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    // initialise the object array
    object = (Object **)calloc(objsize, sizeof(Object *));
    stkptr = 0;

    // If we're importing to a group, we need to avoid ID conflicts
    if (importing)
    {
        id_offset = maxobjid + 1;
    }
    else
    {
        maxobjid = 0;
        id_offset = 0;
    }

    // read the file line by line
    while (TRUE)
    {
        char *nexttok = NULL;
        char *tok;
        int id;
        LOCK lock;

        if (fgets(buf, MAXLINE, f) == NULL)
            break;

        tok = strtok_s(buf, " \t\n", &nexttok);
        if (strcmp(tok, "LOFTYCAD") == 0)
        {
            tok = strtok_s(NULL, " \t\n", &nexttok);
            version = (float)atof(tok);
        }
        else if (strcmp(tok, "TITLE") == 0)
        {
            tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
            if (tok != NULL)
                strcpy_s(tree->title, 256, tok);
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
            stack[stkptr++] = atoi(tok) + id_offset;
        }
        else if (strcmp(tok, "BEGINGROUP") == 0)
        {
            // Stack the object ID being constructed. 
            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            check_and_grow(id, &object, &objsize);
            stack[stkptr++] = id;

            // Create the group now, so we can link to it
            grp = group_new();
            grp->hdr.ID = id;
            object[id] = (Object *)grp;

            tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
            if (tok != NULL)
                strcpy_s(grp->title, 256, tok);
        }
        else if (strcmp(tok, "POINT") == 0)
        {
            Point *p;
            float x, y, z;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
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
                link_tail_group((Object *)p, tree);
            else if (IS_GROUP(object[stack[stkptr - 1]]))
                link_tail_group((Object *)p, (Group *)object[stack[stkptr - 1]]);
        }
        else if (strcmp(tok, "EDGE") == 0)
        {
            int end0, end1, ctrl0, ctrl1, ctr;
            Edge *edge = NULL;
            ArcEdge *ae;
            BezierEdge *be;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strcmp(tok, "STRAIGHT") == 0 || strcmp(tok, "STRAIGHT(C)") == 0 || strcmp(tok, "STRAIGHT(D)") == 0)
            {
                edge = edge_new(EDGE_STRAIGHT);
                if (strcmp(tok, "STRAIGHT(C)") == 0)
                    edge->type |= EDGE_CONSTRUCTION;
                if (strcmp(tok, "STRAIGHT(D)") == 0)
                    ((Object *)edge)->show_dims = TRUE;
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok) + id_offset;
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok) + id_offset;
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                edge->endpoints[0] = (Point *)object[end0];
                edge->endpoints[1] = (Point *)object[end1];
            }
            else if (strcmp(tok, "ARC") == 0 || strcmp(tok, "ARC(C)") == 0 || strcmp(tok, "ARC(D)") == 0)
            {
                edge = edge_new(EDGE_ARC);
                if (strcmp(tok, "ARC(C)") == 0)
                    edge->type |= EDGE_CONSTRUCTION;
                if (strcmp(tok, "ARC(D)") == 0)
                    ((Object *)edge)->show_dims = TRUE;
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok) + id_offset;
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok) + id_offset;
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                edge->endpoints[0] = (Point *)object[end0];
                edge->endpoints[1] = (Point *)object[end1];

                ae = (ArcEdge *)edge;
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->clockwise = strcmp(tok, "C") == 0;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                ctr = atoi(tok) + id_offset;
                ASSERT(ctr > 0 && object[ctr] != NULL, "Bad centre point ID");
                ae->centre = (Point *)object[ctr];

                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.A = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.B = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ae->normal.C = (float)atof(tok);

                if (version >= 0.2)
                {
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                    edge->stepping = atoi(tok);
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                    edge->stepsize = (float)atof(tok);
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                    edge->nsteps = atoi(tok);
                }
            }
            else if (strcmp(tok, "BEZIER") == 0)
            {
                edge = edge_new(EDGE_BEZIER);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end0 = atoi(tok) + id_offset;
                ASSERT(end0 > 0 && object[end0] != NULL, "Bad endpoint ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                end1 = atoi(tok) + id_offset;
                ASSERT(end1 > 0 && object[end1] != NULL, "Bad endpoint ID");
                edge->endpoints[0] = (Point *)object[end0];
                edge->endpoints[1] = (Point *)object[end1];

                tok = strtok_s(NULL, " \t\n", &nexttok);
                ctrl0 = atoi(tok) + id_offset;
                ASSERT(ctrl0 > 0 && object[ctrl0] != NULL, "Bad control point ID");
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ctrl1 = atoi(tok) + id_offset;
                ASSERT(ctrl1 > 0 && object[ctrl1] != NULL, "Bad control point ID");
                be = (BezierEdge *)edge;
                be->ctrlpoints[0] = (Point *)object[ctrl0];
                be->ctrlpoints[1] = (Point *)object[ctrl1];

                if (version >= 0.2)
                {
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                    edge->stepping = atoi(tok);
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                    edge->stepsize = (float)atof(tok);
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                    edge->nsteps = atoi(tok);
                }
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
                link_tail_group((Object *)edge, tree);
            else if (IS_GROUP(object[stack[stkptr - 1]]))
                link_tail_group((Object *)edge, (Group *)object[stack[stkptr - 1]]);
        }
        else if (strcmp(tok, "FACE") == 0)
        {
            int pid;
            Face *face, *pair;
            Plane norm;
            FACE type;
            Point *init_pt;
            BOOL dims = FALSE;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strcmp(tok, "RECT") == 0)
            {
                type = FACE_RECT;
            }
            else if (strcmp(tok, "RECT(C)") == 0)
            {
                type = FACE_RECT | FACE_CONSTRUCTION;
            }
            else if (strcmp(tok, "RECT(D)") == 0)
            {
                type = FACE_RECT;
                dims = TRUE;
            }
            else if (strcmp(tok, "CIRCLE") == 0)
            {
                type = FACE_CIRCLE;
            }
            else if (strcmp(tok, "CIRCLE(C)") == 0)
            {
                type = FACE_CIRCLE | FACE_CONSTRUCTION;
            }
            else if (strcmp(tok, "CIRCLE(D)") == 0)
            {
                type = FACE_CIRCLE;
                dims = TRUE;
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
                ASSERT(FALSE, "Deserialise Face (general) Not implemented");
            }

            tok = strtok_s(NULL, " \t\n", &nexttok);
            pid = atoi(tok) + id_offset;
            ASSERT(pid != 0 && object[pid] != NULL && object[pid]->type == OBJ_POINT, "Bad initial point ID");
            init_pt = (Point *)object[pid];

            pair = NULL;
#if 0
            if (version >= 0.2)         // read ???? for face (can't do 2-way pairing!)
            {
                tok = strtok_s(NULL, " \t\n", &nexttok);
                pid = atoi(tok);
                ASSERT(pid != 0 && object[pid] != NULL && object[pid]->type == OBJ_FACE, "Bad pair face ID");
                pair = (Face *)object[pid];
            }
#endif
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
            face->pair = pair;

            while (TRUE)
            {
                int eid;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                eid = atoi(tok) + id_offset;
                ASSERT(eid > 0 && object[eid] != NULL, "Bad edge ID");

                if (face->n_edges >= face->max_edges)
                {
                    face->max_edges *= 2;
                    face->edges = realloc(face->edges, face->max_edges * sizeof(Edge *));
                }

                face->edges[face->n_edges++] = (Edge *)object[eid];
            }

            face->hdr.ID = id;
            face->hdr.lock = lock;
            face->hdr.show_dims = dims;
            object[id] = (Object *)face;
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed face record");
            stkptr--;
            if (stkptr == 0)
                link_tail_group((Object *)face, tree);
            else if (IS_GROUP(object[stack[stkptr - 1]]))
                link_tail_group((Object *)face, (Group *)object[stack[stkptr - 1]]);
        }
        else if (strcmp(tok, "VOLUME") == 0)
        {
            Volume *vol;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            vol = vol_new();
            vol->hdr.ID = id;
            vol->hdr.lock = lock;
            object[id] = (Object *)vol;

            while (TRUE)
            {
                int fid;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                fid = atoi(tok) + id_offset;
                ASSERT(fid > 0 && object[fid] != NULL, "Bad face ID");

                ((Face *)object[fid])->vol = vol;
                link_tail(object[fid], (Object **)&vol->faces);
            }

            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed volume record");
            stkptr--;
            if (stkptr == 0)
                link_tail_group((Object *)vol, tree);
            else if (IS_GROUP(object[stack[stkptr - 1]]))
                link_tail_group((Object *)vol, (Group *)object[stack[stkptr - 1]]);
        }
        else if (strcmp(tok, "ENDGROUP") == 0)
        {
            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);
            ASSERT(stkptr > 0 && id == stack[stkptr - 1], "Badly formed group");
            ASSERT(object[id]->type == OBJ_GROUP, "ENDGROUP is not a group");
            object[id]->lock = lock;
            stkptr--;
            if (stkptr == 0)
                link_tail_group(object[id], tree);
            else if (IS_GROUP(object[stack[stkptr - 1]]))
                link_tail_group(object[id], (Group *)object[stack[stkptr - 1]]);
        }
        else if (strcmp(tok, "SELECTION") == 0)
        {
            clear_selection(&selection);
            while (TRUE)
            {
                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                id = atoi(tok) + id_offset;
                ASSERT(id > 0 && object[id] != NULL, "Bad selection ID");
                link_single(object[id], &selection);
            }
        }
    }

    objid = maxobjid + 1;
    if (!importing)
        save_count = 1;
    free(object);
    fclose(f);

    return TRUE;
}

// Write a checkpoint (a file in the undo stack).
void
write_checkpoint(Group *tree, char *filename)
{
    char basename[256], check[256];
    int baselen;
    char *pdot;

    strcpy_s(basename, 256, filename);
    baselen = strlen(basename);
    if (baselen > 4 && (pdot = strrchr(basename, '.')) != NULL)
        *pdot = '\0';                                            // cut off ".lcd" 
    sprintf_s(check, 256, "%s_%04d.lcd", basename, ++generation);
    serialise_tree(tree, check);
    latest_generation = generation;
    if (generation > max_generation)
        max_generation = generation;
    populate_treeview();
}

// Read back a given generation of checkpoint. Generation zero is the
// original file (or an empty tree). Clean out the existing tree first.
BOOL
read_checkpoint(Group *tree, char *filename, int generation)
{
    char basename[256], check[256];
    int baselen;
    BOOL rc;
    char *pdot;

    clear_selection(&selection);
    clear_selection(&clipboard);
    purge_tree(tree->obj_list);
    tree->obj_list = NULL;

    if (generation > 0)
    {
        strcpy_s(basename, 256, filename);
        baselen = strlen(basename);
        if (baselen > 4 && (pdot = strrchr(basename, '.')) != NULL)
            *pdot = '\0';                                            // cut off ".lcd" 
        sprintf_s(check, 256, "%s_%04d.lcd", basename, generation);
        rc = deserialise_tree(tree, check, FALSE);
    }
    else if (filename[0] != '\0')
    {
        drawing_changed = FALSE;
        rc = deserialise_tree(tree, filename, FALSE);
    }
    populate_treeview();
    return rc;
}

// Clean out all the checkpoint files for a filename.
void
clean_checkpoints(char *filename)
{
    char basename[256], check[256];
    int baselen, gen;
    char *pdot;

    strcpy_s(basename, 256, filename);
    baselen = strlen(basename);
    if (baselen > 4 && (pdot = strrchr(basename, '.')) != NULL)
        *pdot = '\0';                                            // cut off ".lcd" 

    for (gen = 1; gen <= max_generation; gen++)
    {
        sprintf_s(check, 256, "%s_%04d.lcd", basename, gen);
        DeleteFile(check);
    }
    
    generation = 0;
    latest_generation = 0;
    max_generation = 0;
}
