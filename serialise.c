#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

#define IS_GROUP(obj) ((obj) != NULL && (obj)->type == OBJ_GROUP)
#define MAXLINE 1024

// Version of output file
double file_version = 0.3;

// A constantly incrementing ID. 
// Should not need to worry about it overflowing 32-bits (4G objects!)
// Start at 1 as an ID of zero is used to check for an unreferenced object.
unsigned int objid = 1;

// The highest object ID encountered when reading in a file.
static unsigned int maxobjid = 0;

// The save count for the tree. Used to protect against multiple writing 
// of shared objects.
static unsigned int save_count = 1;

// Marks whetther a material has been written out.
static BOOL mat_written[MAX_MATERIAL] = { 0, };

// names of things that make the serialised format a little easier to read
char *objname[] = { "(none)", "POINT", "EDGE", "FACE", "VOLUME", "ENDGROUP" };
char *locktypes[] = { "N", "P", "E", "F", "V" };
char *edgetypes[] = { "STRAIGHT", "ARC", "BEZIER" };
char *facetypes[] = { "RECT", "CIRCLE", "FLAT", "CYLINDRICAL", "GENERAL" };
char *optypes[] = { "UNION", "INTER", "DIFF", "NONE" };

// Serialise an object. Children go out before their parents, in general.
void
serialise_obj(Object *obj, FILE *f)
{
    int i, n;
    Edge *e;
    EDGE type, constr;
    ArcEdge *ae;
    BezierEdge *be;
    Face *face;
    Volume *vol;
    Group *group;
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
        for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
            serialise_obj((Object *)face, f);
        break;

    case OBJ_GROUP:
        group = (Group *)obj;
        fprintf_s(f, "BEGINGROUP %d %s\n", obj->ID, group->title);
        for (o = group->obj_list.head; o != NULL; o = o->next)
            serialise_obj(o, f);
        break;
    }

    // Now write the object itself
    n = fprintf_s(f, "%s %d %s ", objname[obj->type], obj->ID, locktypes[obj->lock]);
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
        n += fprintf_s(f, "%s%s %d %f %f %f %f %f %f ",
                  facetypes[face->type & ~FACE_CONSTRUCTION],
                  (face->type & FACE_CONSTRUCTION) ? "(C)" : (obj->show_dims ? "(D)" : ""),
                  face->initial_point->hdr.ID,
                  face->normal.refpt.x, face->normal.refpt.y, face->normal.refpt.z,
                  face->normal.A, face->normal.B, face->normal.C);
        for (i = 0; i < face->n_edges; i++)
        {
            if (n >= MAXLINE - 10)          // not enough room for one more ID
            {
                if (i < face->n_edges - 1)
                    fprintf_s(f, "+");      // write continuation char if more to come
                fprintf_s(f, "\n");         // and start a new line
                n = 0;
            }
            n += fprintf_s(f, "%d ", face->edges[i]->hdr.ID);
        }
        fprintf_s(f, "\n");
        if (face->n_contours != 0)
        {
            fprintf_s(f, "CONTOUR %d ", obj->ID);
            for (i = 0; i < face->n_contours; i++)  // TODO: Handle lines longer than 1024 (approx. 80 contours)
                fprintf_s(f, "%d %d %d ", 
                    face->contours[i].edge_index, 
                    face->contours[i].ip_index, 
                    face->contours[i].n_edges);
            fprintf_s(f, "\n");
        }
        if (face->text != NULL)
        {
            Text *text = face->text;
            fprintf_s(f, "TEXT %d %f %f %f %f %f %f %f %f %f %s\n",
                      obj->ID,
                      text->origin.x, text->origin.y, text->origin.z,
                      text->endpt.x, text->endpt.y, text->endpt.z,
                      text->plane.A, text->plane.B, text->plane.C,
                      text->string);
            fprintf_s(f, "FONT %d %d %d %s\n",
                      obj->ID, text->bold, text->italic, text->font);
        }
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        fprintf_s(f, "%s ", optypes[vol->op]);
        for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
        {
            if (n >= MAXLINE - 10)
            {
                if (face->hdr.next != NULL)
                    fprintf_s(f, "+");
                fprintf_s(f, "\n");
                n = 0;
            }
            n += fprintf_s(f, "%d ", face->hdr.ID);
        }
        fprintf_s(f, "\n");
        if (vol->xform != NULL)
        {
            Transform *xform = vol->xform;

            fprintf_s(f, "TRANSFORM %d %f %f %f %d %f %f %f %d %f %f %f\n",
                      obj->ID,
                      xform->xc, xform->yc, xform->zc,
                      xform->enable_scale,
                      xform->sx, xform->sy, xform->sz,
                      xform->enable_rotation,
                      xform->rx, xform->ry, xform->rz
                      );
        }
        if (vol->material != 0)
        {
            if (mat_written[vol->material])
            {
                fprintf_s(f, "MATERIAL %d %d\n",
                    vol->material,
                    obj->ID);
            }
            else
            {
                fprintf_s(f, "MATERIAL %d %d %f %f %f %f %s\n",
                    vol->material,
                    obj->ID,
                    materials[vol->material].color[0],
                    materials[vol->material].color[1],
                    materials[vol->material].color[2],
                    materials[vol->material].shiny,
                    materials[vol->material].name);
                mat_written[vol->material] = TRUE;
            }
        }
        break;

    case OBJ_GROUP:
        group = (Group *)obj;
        if (group->op != OP_NONE)
            fprintf_s(f, "%s ", optypes[group->op]);
        fprintf_s(f, "\n");
        if (group->xform != NULL)
        {
            Transform *xform = group->xform;

            fprintf_s(f, "TRANSFORM %d %f %f %f %d %f %f %f %d %f %f %f\n",
                      obj->ID,
                      xform->xc, xform->yc, xform->zc,
                      xform->enable_scale,
                      xform->sx, xform->sy, xform->sz,
                      xform->enable_rotation,
                      xform->rx, xform->ry, xform->rz
                      );
        }
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
    int i, n;

    // Write header
    fopen_s(&f, filename, "wt");
    fprintf_s(f, "LOFTYCAD %.1f\n", file_version);
    fprintf_s(f, "TITLE %s\n", tree->title);
    fprintf_s(f, "SCALE %f %f %f %d %f\n", half_size, grid_snap, tolerance, angle_snap, round_rad);

    // Write materials here, in case some are not used by any volumes. Mark them as written.
    for (i = 0; i < MAX_MATERIAL; i++)
        mat_written[i] = FALSE;
    for (i = 1; i < MAX_MATERIAL; i++)      // don't write the default [0] material
    {
        if (materials[i].valid)
        {
            fprintf_s(f, "MATERIAL %d %d %f %f %f %f %s\n",
                i,
                0,
                materials[i].color[0],
                materials[i].color[1],
                materials[i].color[2],
                materials[i].shiny,
                materials[i].name);
            mat_written[i] = TRUE;
        }
    }

    // Write object tree
    save_count++;
    for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
        serialise_obj(obj, f);

    // Write selection out.
    if (selection.head != NULL)
    {
        fprintf_s(f, "SELECTION ");
        n = 10;
        for (obj = selection.head; obj != NULL; obj = obj->next)
        {
            if (n >= MAXLINE - 10)
            {
                if (obj->next != NULL)
                    fprintf_s(f, "+");
                fprintf_s(f, "\n");
                n = 0;
            }
            n += fprintf_s(f, "%d ", obj->prev->ID);
        }
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
        int orig_size, new_size;
        unsigned int new_objsize = *objsize * 2;

        while (new_objsize < id)
            new_objsize *= 2;

        // make sure the new bit of the object array is zeroed out after the realloc
        // it's usually just the second half, but sometimes it jumps by more than *2
        *object = (Object **)realloc(*object, sizeof(Object *) * new_objsize);
        orig_size = sizeof(Object *) * (*objsize);
        new_size = sizeof(Object *) * new_objsize;
        memset((char *)(*object) + orig_size, 0, new_size - orig_size);
        *objsize = new_objsize;
    }
}

// Find a lock type or an operation type from its first letter.
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

OPERATION
optype_of(char *tok)
{
    int i;

    for (i = 0; i < OP_MAX; i++)
    {
        if (tok[0] == optypes[i][0])
            return i;
    }

    return OP_MAX;
}


// Deserialise a tree from file. 
BOOL
deserialise_tree(Group *tree, char *filename, BOOL importing)
{
    FILE *f;
    char buf[MAXLINE];
    int stack[10], stkptr;
    int objsize = 1000;
    int id_offset, mat_offset, mat;
    double version = 0.1;
    Object **object;
    Group *grp;
    Transform *xform;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    // initialise the object array
    object = (Object **)calloc(objsize, sizeof(Object *));
    stkptr = 0;

    // If we're importing to a group, we need to avoid ID conflicts on objects and materials
    if (importing)
    {
        id_offset = maxobjid + 1;
        mat_offset = 0;
        for (mat = 0; mat < MAX_MATERIAL; mat++)
        {
            if (materials[mat].valid)
                mat_offset = mat;
        }
    }
    else
    {
        maxobjid = 0;
        id_offset = 0;
        mat_offset = 0;
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
            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (tok != NULL)
                round_rad = (float)atof(tok);
        }
        else if (strcmp(tok, "BEGIN") == 0)
        {
            // Stack the object ID being constructed
            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            check_and_grow(id, &object, &objsize);
            stack[stkptr++] = id;
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
                {
                    edge->type |= EDGE_CONSTRUCTION;
                    ((Object *)edge)->show_dims = TRUE;
                }
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
                {
                    edge->type |= EDGE_CONSTRUCTION;
                    ((Object *)edge)->show_dims = TRUE;
                }
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
            Face *face;
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
                dims = TRUE;
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

                if (tok[0] == '+')    // handle continuation character '+' at end of line
                {
                    if (fgets(buf, MAXLINE, f) == NULL)
                        break;
                    tok = strtok_s(buf, " \t\n", &nexttok);
                }

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
        else if (strcmp(tok, "CONTOUR") == 0)
        {
            Face *face;
            int maxc;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            face = (Face *)object[id];
            maxc = 16;
            face->contours = calloc(maxc, sizeof(Contour));
            while (TRUE)
            {
                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;
                face->contours[face->n_contours].edge_index = atoi(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                face->contours[face->n_contours].ip_index = atoi(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                face->contours[face->n_contours].n_edges = atoi(tok);
                face->n_contours++;
                if (face->n_contours == maxc)
                {
                    maxc <<= 1;
                    face->contours = realloc(face->contours, maxc * sizeof(Contour));
                }
            }
        }
        else if (strcmp(tok, "TEXT") == 0)
        {
            Face *face;
        
            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            face = (Face *)object[id];
            if (face->text == NULL)
                face->text = calloc(1, sizeof(Text));
            face->text->origin.hdr.type = OBJ_POINT;
            face->text->endpt.hdr.type = OBJ_POINT;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->origin.x = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->origin.y = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->origin.z = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->endpt.x = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->endpt.y = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->endpt.z = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->plane.A = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->plane.B = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->plane.C = (float)atof(tok);
            tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
            if (tok != NULL)
                strcpy_s(face->text->string, 80, tok);
        }
        else if (strcmp(tok, "FONT") == 0)
        {
            Face *face;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            face = (Face *)object[id];
            if (face->text == NULL)
                face->text = calloc(1, sizeof(Text));

            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->bold = atoi(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            face->text->italic = atoi(tok);
            tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
            if (tok != NULL)
                strcpy_s(face->text->font, 32, tok);
        }
        else if (strcmp(tok, "VOLUME") == 0)
        {
            Volume *vol;
            Face *last_face = NULL;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            check_and_grow(id, &object, &objsize);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            lock = locktype_of(tok);

            vol = vol_new();
            vol->hdr.ID = id;
            vol->hdr.lock = lock;
            object[id] = (Object *)vol;
            vol->op = OP_MAX;

            // Read the list of faces that make up the volume.
            while (TRUE)
            {
                int fid;

                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;

                if (tok[0] == '+')    // handle continuation character '+' at end of line
                {
                    if (fgets(buf, MAXLINE, f) == NULL)
                        break;
                    tok = strtok_s(buf, " \t\n", &nexttok);
                }

                if (isalpha(tok[0]))    // handle operator before any face ID's
                {
                    vol->op = optype_of(tok);
                    tok = strtok_s(NULL, " \t\n", &nexttok);
                }

                fid = atoi(tok) + id_offset;
                ASSERT(fid > 0 && object[fid] != NULL, "Bad face ID");

                ((Face *)object[fid])->vol = vol;
                link_tail(object[fid], &vol->faces);
                last_face = (Face *)object[fid];
            }

            // The last two faces are always the extruded ones - mark them as such,
            // and calculate the current extruded height
            last_face->extruded = TRUE;
            ((Face *)last_face->hdr.prev)->extruded = TRUE;
            vol->extrude_height = 
                -distance_point_plane(&last_face->normal, &((Face *)last_face->hdr.prev)->normal.refpt);

            // Default operation code if not specified
            // TODO handle default-extruded and explicitly set ops differently
            // TODO store extruded flag on faces explicitly?
            if (vol->op == OP_MAX)
                vol->op = vol->extrude_height < 0 ? OP_INTERSECTION : OP_UNION;

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
            grp = (Group *)object[id];
            grp->op = OP_NONE;    // default for groups
            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (tok != NULL && isalpha(tok[0]))    // handle operator if present
                grp->op = optype_of(tok);

            stkptr--;
            if (stkptr == 0)
                link_tail_group(object[id], tree);
            else if (IS_GROUP(object[stack[stkptr - 1]]))
                link_tail_group(object[id], (Group *)object[stack[stkptr - 1]]);
        }
        else if (strcmp(tok, "TRANSFORM") == 0)
        {
            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok) + id_offset;
            ASSERT(object[id]->type == OBJ_VOLUME || object[id]->type == OBJ_GROUP, "Transform must be on volume or group");
            xform = xform_new();
            switch (object[id]->type)
            {
            case OBJ_VOLUME:
                ((Volume *)object[id])->xform = xform;
                break;
            case OBJ_GROUP:
                ((Group *)object[id])->xform = xform;
                break;
            }
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->xc = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->yc = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->zc = (float)atof(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->enable_scale = atoi(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->sx = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->sy = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->sz = (float)atof(tok);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->enable_rotation = atoi(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->rx = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->ry = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            xform->rz = (float)atof(tok);
            evaluate_transform(xform);
        }
        else if (strcmp(tok, "MATERIAL") == 0)
        {
            Volume* vol;

            tok = strtok_s(NULL, " \t\n", &nexttok);
            mat = atoi(tok) + mat_offset;
            tok = strtok_s(NULL, " \t\n", &nexttok);
            id = atoi(tok);
            if (id != 0)            // if there's an ID, it must be a volume
            {
                id += id_offset;
                ASSERT(object[id]->type == OBJ_VOLUME, "Material must be on volume");
                vol = (Volume*)object[id];
                vol->material = mat;
            }
            if (!materials[mat].valid)
            {
                tok = strtok_s(NULL, " \t\n", &nexttok);
                ASSERT(tok != NULL, "New material must have colours, etc");
                materials[mat].color[0] = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                materials[mat].color[1] = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                materials[mat].color[2] = (float)atof(tok);
                tok = strtok_s(NULL, " \t\n", &nexttok);
                materials[mat].shiny = (float)atof(tok);
                tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
                if (tok != NULL)
                    strcpy_s(materials[mat].name, 64, tok);
                materials[mat].valid = TRUE;
            }
        }
        else if (strcmp(tok, "SELECTION") == 0)
        {
            clear_selection(&selection);
            while (TRUE)
            {
                tok = strtok_s(NULL, " \t\n", &nexttok);
                if (tok == NULL)
                    break;

                if (tok[0] == '+')    // handle continuation character '+' at end of line
                {
                    if (fgets(buf, MAXLINE, f) == NULL)
                        break;
                    tok = strtok_s(buf, " \t\n", &nexttok);
                }

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
    char basename[256], check[256], tmpdir[256];
    int baselen;
    char *pdot;

    pdot = strrchr(filename, '\\');
    if (pdot != NULL)
        strcpy_s(basename, 256, pdot+1);          // cut off any directory in file
    else
        strcpy_s(basename, 256, filename);
    baselen = strlen(basename);
    if (baselen > 4 && (pdot = strrchr(basename, '.')) != NULL)
        *pdot = '\0';                                            // cut off ".lcd" 
    GetTempPath(256, tmpdir);
    sprintf_s(check, 256, "%s%s_%04d.lcd", tmpdir, basename, ++generation);
    serialise_tree(tree, check);
    latest_generation = generation;
    if (generation > max_generation)
        max_generation = generation;
}

// Read back a given generation of checkpoint. Generation zero is the
// original file (or an empty tree). Clean out the existing tree first.
BOOL
read_checkpoint(Group *tree, char *filename, int generation)
{
    char basename[256], check[256], tmpdir[256];
    int baselen;
    BOOL rc = FALSE;
    char *pdot;

    clear_selection(&selection);
    clear_selection(&saved_list);
    clear_selection(&clipboard);
    purge_tree(tree, FALSE, NULL);

    if (generation > 0)
    {
        pdot = strrchr(filename, '\\');
        if (pdot != NULL)
            strcpy_s(basename, 256, pdot+1);          // cut off any directory in file
        else
            strcpy_s(basename, 256, filename);
        baselen = strlen(basename);
        if (baselen > 4 && (pdot = strrchr(basename, '.')) != NULL)
            *pdot = '\0';                                            // cut off ".lcd" 
        GetTempPath(256, tmpdir);
        sprintf_s(check, 256, "%s%s_%04d.lcd", tmpdir, basename, generation);
        rc = deserialise_tree(tree, check, FALSE);
    }
    else if (filename[0] != '\0')
    {
        drawing_changed = FALSE;
        rc = deserialise_tree(tree, filename, FALSE);
    }

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
