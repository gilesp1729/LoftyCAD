#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Anything to do with moving and copying objects.

// Clear the moved and copied_to flags on all points referenced by the object.
// Call this after move_obj, copy_obj or the rotate/reflect functions.
void
clear_move_copy_flags(Object* obj)
{
    int i;
    Point* p;
    EDGE type;
    Edge* edge;
    ArcEdge* ae;
    BezierEdge* be;
    Face* face;
    Volume* vol;
    Group* grp;
    Object* o;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        p->moved = FALSE;
        obj->copied_to = NULL;
        break;

    case OBJ_EDGE:
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        edge = (Edge*)obj;
        clear_move_copy_flags((Object*)edge->endpoints[0]);
        clear_move_copy_flags((Object*)edge->endpoints[1]);
        obj->copied_to = NULL;

        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            clear_move_copy_flags((Object*)ae->centre);
            clear_move_copy_flags((Object*)&ae->normal.refpt);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            clear_move_copy_flags((Object*)be->ctrlpoints[0]);
            clear_move_copy_flags((Object*)be->ctrlpoints[1]);
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            clear_move_copy_flags((Object*)edge);
        }
        for (p = (Point*)face->view_list.head; p != NULL; p = (Point*)p->hdr.next)
            clear_move_copy_flags((Object*)p);
        if (face->text != NULL)             // and the text positions
        {
            clear_move_copy_flags((Object*)&face->text->origin);
            clear_move_copy_flags((Object*)&face->text->endpt);
        }

        obj->copied_to = NULL;
        break;

    case OBJ_VOLUME:
        vol = (Volume*)obj;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            clear_move_copy_flags((Object*)face);
        obj->copied_to = NULL;
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            clear_move_copy_flags(o);
        obj->copied_to = NULL;
        break;
    }
}

// Copy any object, with an offset on all its point coordinates. Optionally if cloning,
// fix any arc/bez stepsize and number of steps on both source and dest edges
// (like clone_face_reverse does).
// Make sure to call clear_move_copy_flags afterwards.
Object*
copy_obj(Object* obj, float xoffset, float yoffset, float zoffset, BOOL cloning)
{
    int i;
    Object* new_obj = NULL;
    Point* p;
    EDGE type;
    Edge* edge, * new_edge;
    ArcEdge* ae, * nae;
    BezierEdge* be, * nbe;
    Face* face, * new_face;
    Volume* vol, * new_vol;
    Object* o;
    Group* grp, * new_grp;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        if (obj->copied_to != NULL)
        {
            new_obj = obj->copied_to;
        }
        else
        {
            new_obj = (Object*)point_new(p->x + xoffset, p->y + yoffset, p->z + zoffset);
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
            new_obj = (Object*)edge_new(((Edge*)obj)->type);
            new_obj->lock = obj->lock;
            obj->copied_to = new_obj;

            // Copy the points
            edge = (Edge*)obj;
            new_edge = (Edge*)new_obj;
            new_edge->endpoints[0] = (Point*)copy_obj((Object*)edge->endpoints[0], xoffset, yoffset, zoffset, cloning);
            new_edge->endpoints[1] = (Point*)copy_obj((Object*)edge->endpoints[1], xoffset, yoffset, zoffset, cloning);
            type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
            switch (type)
            {
            case EDGE_ARC:
                ae = (ArcEdge*)edge;
                nae = (ArcEdge*)new_edge;
                nae->centre = (Point*)copy_obj((Object*)ae->centre, xoffset, yoffset, zoffset, cloning);
                nae->clockwise = ae->clockwise;
                nae->normal = ae->normal;
                move_obj((Object *)&nae->normal.refpt, xoffset, yoffset, zoffset);
                new_edge->nsteps = edge->nsteps;
                new_edge->stepsize = edge->stepsize;
                new_edge->stepping = edge->stepping;
                if (cloning)
                {
                    edge->stepping = 1;
                    new_edge->stepping = 1;
                    edge->view_valid = FALSE;
                }
                new_edge->view_valid = FALSE;
                break;

            case EDGE_BEZIER:
                be = (BezierEdge*)edge;
                nbe = (BezierEdge*)new_edge;
                nbe->ctrlpoints[0] = (Point*)copy_obj((Object*)be->ctrlpoints[0], xoffset, yoffset, zoffset, cloning);
                nbe->ctrlpoints[1] = (Point*)copy_obj((Object*)be->ctrlpoints[1], xoffset, yoffset, zoffset, cloning);
                new_edge->nsteps = edge->nsteps;
                new_edge->stepsize = edge->stepsize;
                new_edge->stepping = edge->stepping;
                if (cloning)
                {
                    edge->stepping = 1;
                    new_edge->stepping = 1;
                    edge->view_valid = FALSE;
                }
                new_edge->view_valid = FALSE;
                break;
            }
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        new_face = face_new(face->type, face->normal);
        new_obj = (Object*)new_face;
        new_obj->lock = obj->lock;

        // Realloc the edge array if we need a big one
        if (face->n_edges >= new_face->max_edges)
        {
            new_face->max_edges = face->max_edges;
            new_face->edges = realloc(new_face->edges, new_face->max_edges * sizeof(Edge*));
        }
        new_face->n_edges = face->n_edges;
        new_face->paired = face->paired;
        new_face->extrude_height = face->extrude_height;


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
            new_edge = (Edge*)copy_obj((Object*)edge, xoffset, yoffset, zoffset, cloning);
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
        vol = (Volume*)obj;
        new_vol = vol_new();
        new_obj = (Object*)new_vol;
        new_obj->lock = obj->lock;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
        {
            new_face = (Face*)copy_obj((Object*)face, xoffset, yoffset, zoffset, cloning);
            new_face->vol = new_vol;
            link_tail((Object*)new_face, &new_vol->faces);
        }
        new_vol->op = vol->op;
        new_vol->max_facetype = vol->max_facetype;
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        new_grp = group_new();
        for (o = grp->obj_list.head; o != NULL; o = o->next)
        {
            new_obj = copy_obj(o, xoffset, yoffset, zoffset, cloning);
            link_tail_group(new_obj, new_grp);
        }
        new_obj = (Object*)new_grp;
        new_obj->lock = obj->lock;
        new_grp->op = grp->op;
        break;
    }

    return new_obj;
}

// Copy a face, but reverse all the edges so the normal points the opposite
// way. Make sure the edges containing the initial points still line up.
Face
* clone_face_reverse(Face* face)
{
    Face* clone = face_new(face->type, face->normal);
    Object* e;
    Object* ne = NULL;
    Edge* edge;
    ArcEdge* ae, * nae;
    Point* last_point;
    int i, idx, c;
    //char buf[256];

    // Swap the normal around
    clone->normal.A = -clone->normal.A;
    clone->normal.B = -clone->normal.B;
    clone->normal.C = -clone->normal.C;
    clone->n_edges = face->n_edges;
    if (face->max_edges > clone->max_edges)   // in case it has been grown before being cloned
        clone->edges = realloc(clone->edges, face->max_edges * sizeof(Edge*));
    clone->max_edges = face->max_edges;

    // associate the face with its clone by setting the extrude flag
    clone->paired = TRUE;
    face->paired = TRUE;

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
            e = (Object*)face->edges[ei + i];
            ne = copy_obj(e, 0, 0, 0, TRUE);
            if (i == 0)
                idx = 0;
            else
                idx = face->contours[c].n_edges - i;
            clone->edges[ei + idx] = (Edge*)ne;

            // Follow the chain of points from e->initial_point
            edge = (Edge*)e;
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
                ae = (ArcEdge*)e;
                nae = (ArcEdge*)ne;

                // Reverse the arc
                nae->clockwise = !ae->clockwise;
                nae->normal.A = -ae->normal.A;
                nae->normal.B = -ae->normal.B;
                nae->normal.C = -ae->normal.C;
                break;
            }
        }
    }

    clone->initial_point = clone->edges[clone->contours[0].edge_index]->endpoints[clone->contours[0].ip_index];

#ifdef DEBUG_REVERSE_RECT_FACE
    sprintf_s(buf, 256, "Clone %d IP %d\r\n", clone->hdr.ID, clone->initial_point->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge*)clone->edges[0])->endpoints[0]->hdr.ID, ((StraightEdge*)clone->edges[0])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge*)clone->edges[1])->endpoints[0]->hdr.ID, ((StraightEdge*)clone->edges[1])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge*)clone->edges[2])->endpoints[0]->hdr.ID, ((StraightEdge*)clone->edges[2])->endpoints[1]->hdr.ID);
    Log(buf);
    sprintf_s(buf, 256, "%d %d\r\n", ((StraightEdge*)clone->edges[3])->endpoints[0]->hdr.ID, ((StraightEdge*)clone->edges[3])->endpoints[1]->hdr.ID);
    Log(buf);
#endif

    return clone;
}

// Move any object by an offset on all its point coordinates.
// Make sure to call clear_move_copy_flags afterwards.
void
move_obj(Object* obj, float xoffset, float yoffset, float zoffset)
{
    int i;
    Point* p;
    EDGE type;
    Edge* edge;
    ArcEdge* ae;
    BezierEdge* be;
    Face* face;
    Volume* vol;
    Group* grp;
    Object* o;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        if (!p->moved)
        {
            p->x += xoffset;
            p->y += yoffset;
            p->z += zoffset;
            p->moved = TRUE;
        }
        break;

    case OBJ_EDGE:
        edge = (Edge*)obj;
        move_obj((Object*)edge->endpoints[0], xoffset, yoffset, zoffset);
        move_obj((Object*)edge->endpoints[1], xoffset, yoffset, zoffset);
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            move_obj((Object*)ae->centre, xoffset, yoffset, zoffset);
            move_obj((Object*)&ae->normal.refpt, xoffset, yoffset, zoffset);
            edge->view_valid = FALSE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            move_obj((Object*)be->ctrlpoints[0], xoffset, yoffset, zoffset);
            move_obj((Object*)be->ctrlpoints[1], xoffset, yoffset, zoffset);
            edge->view_valid = FALSE;
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            move_obj((Object*)edge, xoffset, yoffset, zoffset);
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
        vol = (Volume*)obj;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            move_obj((Object*)face, xoffset, yoffset, zoffset);
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            move_obj(o, xoffset, yoffset, zoffset);
        break;
    }
}

// Move a face by local normals (each boundary point is assumed to have a local normal
// in the face's PlaneRef array)
void
extrude_local(Face* face, float length)
{
    int i;

    for (i = 0; i < face->n_local; i++)
    {
        PlaneRef* n = &face->local_norm[i];

        move_obj((Object *)n->refpt, n->A * length, n->B * length, n->C * length);
    }

    // If the face contains arcs, their normals need to be recalculated.




}

// Find any adjacent round/chamfer corner edges to the given edge or face
// that need moving along with it. If a face is being picked, return faces that
// need moving (not just edges) since they will be used for highlighting.
// Return TRUE if some corners were found and added to the list.
BOOL
find_corner_edges(Object* obj, Object* parent, ListHead* halo)
{
    int i;
    Face* face, *f;
    Volume* vol;
    BOOL rc = FALSE;

    if (parent == NULL) // no parent, nothing to do
        return FALSE;

    switch (parent->type)
    {
    case OBJ_FACE:
        if (obj->type == OBJ_EDGE)
        {
            // Picked edge, with parent face. Put adjacent corner edges in the halo list.
            face = (Face*)parent;
            for (i = 0; i < face->n_edges; i++)
            {
                if ((Object *)face->edges[i] == obj)
                {
                    int next = (i < face->n_edges - 1) ? i + 1 : 0;
                    int prev = (i > 0) ? i - 1 : face->n_edges - 1;

                    if (face->edges[prev]->corner)
                    {
                        rc = TRUE;
                        link_single((Object*)face->edges[prev], halo);
                    }
                    if (face->edges[next]->corner)
                    {
                        rc = TRUE;
                        link_single((Object*)face->edges[next], halo);
                    }
                    break;
                }
            }
        }
        break;

    case OBJ_VOLUME:
        vol = (Volume*)parent;
        if (obj->type == OBJ_EDGE)
        {
            // Picked edge, with parent volume. Find corner edges adjacent to the
            // picked edge in all the faces in the volume's face list. Stop when
            // they are found (the picked edge will only occur once with adjacent corners)
            for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            {
                for (i = 0; i < face->n_edges; i++)
                {
                    if ((Object*)face->edges[i] == obj)
                    {
                        rc = find_corner_edges(obj, (Object *)face, halo);
                        if (rc)
                            goto finished;
                    }
                }
            }
        }
        else if (obj->type == OBJ_FACE)
        {
            // Picked face, with parent volume. Find corner _faces_ adjacent to the
            // picked face in the volume's face list. Note that:
            // - if the picked face has corner edges, there will be no corner faces edge-adjacent to it.
            // - that leaves the picked face being a side face, and any corner faces will be adjacent 
            // to it in the volume's face list.
            face = (Face*)obj;
            if (face->has_corners)
                return FALSE;
            for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
            {
                if (f == face)
                {
                    Face* fnext, * fprev;

                    fprev = (Face*)f->hdr.prev;
                    if (fprev == NULL)
                        fprev = (Face*)vol->faces.tail;
                    fnext = (Face*)f->hdr.next;
                    if (fnext == NULL)
                        fnext = (Face*)vol->faces.head;

                    if (fprev->corner)
                    {
                        rc = TRUE;
                        link_single((Object*)fprev, halo);
                    }
                    if (fnext->corner)
                    {
                        rc = TRUE;
                        link_single((Object*)fnext, halo);
                    }
                    break;
                }
            }
        }
        break;
    }
finished:
    return rc;
}

// Move any edges or faces that have been put in the halo list by find_corner_edges.
// Ignore any smooth factors.
void
move_corner_edges(ListHead *halo, float xoffset, float yoffset, float zoffset)
{
    Object* obj;

    for (obj = halo->head; obj != NULL; obj = obj->next)
        move_obj(obj->prev, xoffset, yoffset, zoffset);
}

// Find a suitable (x,y,z) point about which to rotate any object.
void
find_obj_pivot(Object* obj, float* x, float* y, float* z)
{
    Point* p;
    Edge* edge;
    Face* face;
    Volume* vol;
    Group* grp;

    switch (obj->type)
    {
    case OBJ_POINT:         // will never be used, but here for completeness
        p = (Point*)obj;
        *x = p->x;
        *y = p->y;
        *z = p->z;
        break;

    case OBJ_EDGE:
        edge = (Edge*)obj;
        *x = edge->endpoints[0]->x;
        *y = edge->endpoints[0]->y;
        *z = edge->endpoints[0]->z;
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        *x = face->normal.refpt.x;
        *y = face->normal.refpt.y;
        *z = face->normal.refpt.z;
        break;

    case OBJ_VOLUME:
        vol = (Volume*)obj;
        *x = vol->bbox.xc;
        *y = vol->bbox.yc;
        *z = vol->bbox.zc;
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        *x = grp->bbox.xc;
        *y = grp->bbox.yc;
        *z = grp->bbox.zc;
        break;
    }
}

// Rotate a coordinate, in the facing plane, by 90 degrees in the positive direction.
void
rotate_coord_90_facing(float* x, float* y, float* z, float xc, float yc, float zc)
{
    float x0 = *x - xc;
    float y0 = *y - yc;
    float z0 = *z - zc;

    switch (facing_index)
    {
    case PLANE_XY:
    case PLANE_MINUS_XY:
        *x = xc - y0;
        *y = yc + x0;
        break;

    case PLANE_XZ:
    case PLANE_MINUS_XZ:
        *x = xc - z0;
        *z = zc + x0;
        break;

    case PLANE_YZ:
    case PLANE_MINUS_YZ:
        *y = yc - z0;
        *z = zc + y0;
        break;
    }
}

// Rotate the direction of a plane, in the facing plane, by 90 degrees in the positive direction.
void
rotate_plane_90_facing(Plane* pl)
{
    float A0 = pl->A;
    float B0 = pl->B;
    float C0 = pl->C;

    switch (facing_index)
    {
    case PLANE_XY:
    case PLANE_MINUS_XY:
        pl->A = -B0;
        pl->B = A0;
        break;

    case PLANE_XZ:
    case PLANE_MINUS_XZ:
        pl->A = -C0;
        pl->C = A0;
        break;

    case PLANE_YZ:
    case PLANE_MINUS_YZ:
        pl->B = -C0;
        pl->C = B0;
        break;
    }
}

// Rotate any object, in the facing plane, by 90 degrees in the positive direction.
// Make sure to call clear_move_copy_flags afterwards.
void
rotate_obj_90_facing(Object* obj, float xc, float yc, float zc)
{
    int i;
    Point* p;
    EDGE type;
    Edge* edge;
    ArcEdge* ae;
    BezierEdge* be;
    Face* face;
    Volume* vol;
    Group* grp;
    Object* o;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        if (!p->moved)
        {
            rotate_coord_90_facing(&p->x, &p->y, &p->z, xc, yc, zc);
            p->moved = TRUE;
        }
        break;

    case OBJ_EDGE:
        edge = (Edge*)obj;
        rotate_obj_90_facing((Object*)edge->endpoints[0], xc, yc, zc);
        rotate_obj_90_facing((Object*)edge->endpoints[1], xc, yc, zc);
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            if (!ae->centre->moved)     // don't do it twice
                rotate_plane_90_facing(&ae->normal);
            rotate_obj_90_facing((Object*)ae->centre, xc, yc, zc);
            rotate_obj_90_facing((Object*)&ae->normal.refpt, xc, yc, zc);
            edge->view_valid = FALSE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            rotate_obj_90_facing((Object*)be->ctrlpoints[0], xc, yc, zc);
            rotate_obj_90_facing((Object*)be->ctrlpoints[1], xc, yc, zc);
            edge->view_valid = FALSE;
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            rotate_obj_90_facing((Object*)edge, xc, yc, zc);
        }
        // don't forget to rotate the normal refpt too
        rotate_coord_90_facing(&face->normal.refpt.x, &face->normal.refpt.y, &face->normal.refpt.z, xc, yc, zc);
        if (face->text != NULL)             // and the text positions
        {
            rotate_coord_90_facing(&face->text->origin.x, &face->text->origin.y, &face->text->origin.z, xc, yc, zc);
            rotate_coord_90_facing(&face->text->endpt.x, &face->text->endpt.y, &face->text->endpt.z, xc, yc, zc);
        }
        face->view_valid = FALSE;
        break;

    case OBJ_VOLUME:
        vol = (Volume*)obj;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            rotate_obj_90_facing((Object*)face, xc, yc, zc);
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            rotate_obj_90_facing(o, xc, yc, zc);
        break;
    }
}

// 2x2 rotation helper
static void
mat_mult_2x2_xy(double m[4], float x0, float y0, float* x, float* y)
{
    *x = (float)(m[0] * x0 + m[1] * y0);
    *y = (float)(m[2] * x0 + m[3] * y0);
}

// Rotate a coordinate, in the facing plane, by angle alpha in the positive direction.
void
rotate_coord_free_facing(float* x, float* y, float* z, float alpha, float xc, float yc, float zc)
{
    float x0 = *x - xc;
    float y0 = *y - yc;
    float z0 = *z - zc;
    double co = cos(alpha / RAD);
    double si = sin(alpha / RAD);
    double m[4] = { co, si, -si, co };

    switch (facing_index)
    {
    case PLANE_XY:
    case PLANE_MINUS_XY:
        mat_mult_2x2_xy(m, x0, y0, x, y);
        *x += xc;
        *y += yc;
        break;

    case PLANE_XZ:
    case PLANE_MINUS_XZ:
        mat_mult_2x2_xy(m, x0, z0, x, z);
        *x += xc;
        *z += zc;
        break;

    case PLANE_YZ:
    case PLANE_MINUS_YZ:
        mat_mult_2x2_xy(m, y0, z0, y, z);
        *y += yc;
        *z += zc;
        break;
    }
}

// Rotate the direction of a plane, in the facing plane, by angle alpha in the positive direction.
void
rotate_plane_free_facing(Plane* pl, float alpha)
{
    float A0 = pl->A;
    float B0 = pl->B;
    float C0 = pl->C;
    double co = cos(alpha / RAD);
    double si = sin(alpha / RAD);
    double m[4] = { co, si, -si, co };

    switch (facing_index)
    {
    case PLANE_XY:
    case PLANE_MINUS_XY:
        mat_mult_2x2_xy(m, A0, B0, &pl->A, &pl->B);
        break;

    case PLANE_XZ:
    case PLANE_MINUS_XZ:
        mat_mult_2x2_xy(m, A0, C0, &pl->A, &pl->C);
break;

    case PLANE_YZ:
    case PLANE_MINUS_YZ:
        mat_mult_2x2_xy(m, B0, C0, &pl->B, &pl->C);
        break;
    }
}

// Rotate any object, in the facing plane, by angle alpha in the positive direction.
// Make sure to call clear_move_copy_flags afterwards.
void
rotate_obj_free_facing(Object* obj, float alpha, float xc, float yc, float zc)
{
    int i;
    Point* p;
    EDGE type;
    Edge* edge;
    ArcEdge* ae;
    BezierEdge* be;
    Face* face;
    Volume* vol;
    Group* grp;
    Object* o;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        if (!p->moved)
        {
            rotate_coord_free_facing(&p->x, &p->y, &p->z, alpha, xc, yc, zc);
            p->moved = TRUE;
        }
        break;

    case OBJ_EDGE:
        edge = (Edge*)obj;
        rotate_obj_free_facing((Object*)edge->endpoints[0], alpha, xc, yc, zc);
        rotate_obj_free_facing((Object*)edge->endpoints[1], alpha, xc, yc, zc);
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            if (!ae->centre->moved)     // don't do it twice
                rotate_plane_free_facing(&ae->normal, alpha);
            rotate_obj_free_facing((Object*)ae->centre, alpha, xc, yc, zc);
            rotate_obj_free_facing((Object*)&ae->normal.refpt, alpha, xc, yc, zc);
            edge->view_valid = FALSE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            rotate_obj_free_facing((Object*)be->ctrlpoints[0], alpha, xc, yc, zc);
            rotate_obj_free_facing((Object*)be->ctrlpoints[1], alpha, xc, yc, zc);
            edge->view_valid = FALSE;
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            rotate_obj_free_facing((Object*)edge, alpha, xc, yc, zc);
        }
        // don't forget to rotate the normal refpt too
        rotate_coord_free_facing(&face->normal.refpt.x, &face->normal.refpt.y, &face->normal.refpt.z, alpha, xc, yc, zc);
        if (face->text != NULL)             // and the text positions
        {
            rotate_coord_free_facing(&face->text->origin.x, &face->text->origin.y, &face->text->origin.z, alpha, xc, yc, zc);
            rotate_coord_free_facing(&face->text->endpt.x, &face->text->endpt.y, &face->text->endpt.z, alpha, xc, yc, zc);
        }
        face->view_valid = FALSE;
        break;

#if 1  // these are handled by xforms. TODO: do we want to give the choice?
    case OBJ_VOLUME:
        vol = (Volume*)obj;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            rotate_obj_free_facing((Object*)face, alpha, xc, yc, zc);
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            rotate_obj_free_facing(o, alpha, xc, yc, zc);
        break;
#endif
    }
}

// Scale a coordinate by (sx, sy, sz) about (xc, yc, zc). Unused scales are expected to be 1.
// Zero or negative scales are ignored.
void
scale_coord_free(float* x, float* y, float* z, float sx, float sy, float sz, float xc, float yc, float zc)
{
    if (sx < SMALL_COORD)
        sx = 1;
    if (sy < SMALL_COORD)
        sy = 1;
    if (sz < SMALL_COORD)
        sz = 1;

    *x = (*x - xc) * sx + xc;
    *y = (*y - yc) * sy + yc;
    *z = (*z - zc) * sz + zc;
}

// Scale an object.
void
scale_obj_free(Object* obj, float sx, float sy, float sz, float xc, float yc, float zc)
{
    int i;
    Point* p;
    EDGE type;
    Edge* edge;
    ArcEdge* ae;
    BezierEdge* be;
    Face* face;
    Volume* vol;
    Group* grp;
    Object* o;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        if (!p->moved)
        {
            scale_coord_free(&p->x, &p->y, &p->z, sx, sy, sz, xc, yc, zc);
            p->moved = TRUE;
        }
        break;

    case OBJ_EDGE:
        edge = (Edge*)obj;
        scale_obj_free((Object*)edge->endpoints[0], sx, sy, sz, xc, yc, zc);
        scale_obj_free((Object*)edge->endpoints[1], sx, sy, sz, xc, yc, zc);
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            scale_obj_free((Object*)ae->centre, sx, sy, sz, xc, yc, zc);
            scale_obj_free((Object*)&ae->normal.refpt, sx, sy, sz, xc, yc, zc);
            edge->view_valid = FALSE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            scale_obj_free((Object*)be->ctrlpoints[0], sx, sy, sz, xc, yc, zc);
            scale_obj_free((Object*)be->ctrlpoints[1], sx, sy, sz, xc, yc, zc);
            edge->view_valid = FALSE;
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            scale_obj_free((Object*)edge, sx, sy, sz, xc, yc, zc);
        }
        // don't forget to rotate the normal refpt too
        scale_coord_free(&face->normal.refpt.x, &face->normal.refpt.y, &face->normal.refpt.z, sx, sy, sz, xc, yc, zc);
        if (face->text != NULL)             // and the text positions
        {
            scale_coord_free(&face->text->origin.x, &face->text->origin.y, &face->text->origin.z, sx, sy, sz, xc, yc, zc);
            scale_coord_free(&face->text->endpt.x, &face->text->endpt.y, &face->text->endpt.z, sx, sy, sz, xc, yc, zc);
        }
        face->view_valid = FALSE;
        break;

    case OBJ_VOLUME:
        vol = (Volume*)obj;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            scale_obj_free((Object*)face, sx, sy, sz, xc, yc, zc);
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            scale_obj_free(o, sx, sy, sz, xc, yc, zc);
        break;
    }
}

// Reflect a coordinate in the facing plane.
void
reflect_coord_facing(float* x, float* y, float* z, float xc, float yc, float zc)
{
    float x0 = *x - xc;
    float y0 = *y - yc;
    float z0 = *z - zc;

    switch (facing_index)
    {
    case PLANE_XY:
    case PLANE_MINUS_XY:
        *x = xc - x0;
        break;

    case PLANE_XZ:
    case PLANE_MINUS_XZ:
        *x = xc - x0;
        break;

    case PLANE_YZ:
    case PLANE_MINUS_YZ:
        *y = yc - y0;
        break;
    }
}

// Reflect the direction of a plane.
void
reflect_plane_facing(Plane* pl)
{
    float A0 = pl->A;
    float B0 = pl->B;
    float C0 = pl->C;

    switch (facing_index)
    {
    case PLANE_XY:
    case PLANE_MINUS_XY:
        pl->A = -A0;
        break;

    case PLANE_XZ:
    case PLANE_MINUS_XZ:
        pl->A = -A0;
        break;

    case PLANE_YZ:
    case PLANE_MINUS_YZ:
        pl->B = -B0;
        break;
    }
}

// Reflect any object about the upward pointing axis in the facing plane.
// take care to get normals and face ordering right!
// Make sure to call clear_move_copy_flags afterwards.
void
reflect_obj_facing(Object* obj, float xc, float yc, float zc)
{
    int i, n;
    Point* p;
    EDGE type;
    Edge* edge;
    ArcEdge* ae;
    BezierEdge* be;
    Face* face;
    Volume* vol;
    Group* grp;
    Object* o;
    Point** ipts = NULL;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point*)obj;
        if (!p->moved)
        {
            reflect_coord_facing(&p->x, &p->y, &p->z, xc, yc, zc);
            p->moved = TRUE;
        }
        break;

    case OBJ_EDGE:
        edge = (Edge*)obj;
        reflect_obj_facing((Object*)edge->endpoints[0], xc, yc, zc);
        reflect_obj_facing((Object*)edge->endpoints[1], xc, yc, zc);
        type = ((Edge*)obj)->type & ~EDGE_CONSTRUCTION;
        switch (type)
        {
        case EDGE_ARC:
            ae = (ArcEdge*)obj;
            if (!ae->centre->moved)             // don't do these things twice
            {
                reflect_plane_facing(&ae->normal);
                ae->clockwise = !ae->clockwise;     // keep the sense of the arc when reflected
            }
            reflect_obj_facing((Object*)ae->centre, xc, yc, zc);
            reflect_obj_facing((Object*)&ae->normal.refpt, xc, yc, zc);
            edge->view_valid = FALSE;
            break;

        case EDGE_BEZIER:
            be = (BezierEdge*)obj;
            reflect_obj_facing((Object*)be->ctrlpoints[0], xc, yc, zc);
            reflect_obj_facing((Object*)be->ctrlpoints[1], xc, yc, zc);
            edge->view_valid = FALSE;
            break;
        }
        break;

    case OBJ_FACE:
        face = (Face*)obj;
        for (i = 0; i < face->n_edges; i++)
        {
            edge = face->edges[i];
            reflect_obj_facing((Object*)edge, xc, yc, zc);
        }
        // don't forget to reflect the normal refpt too
        reflect_coord_facing(&face->normal.refpt.x, &face->normal.refpt.y, &face->normal.refpt.z, xc, yc, zc);
        if (face->text != NULL) 
        {
            //reflect_coord_facing(&face->text->origin.x, &face->text->origin.y, &face->text->origin.z, xc, yc, zc);
            //reflect_coord_facing(&face->text->endpt.x, &face->text->endpt.y, &face->text->endpt.z, xc, yc, zc);

            // Lose the text struct as it can no longer be regenerated
            free(face->text);
            face->text = NULL;
        }

        // If there are contours, remember all the initial points by their actual pointers
        // The indices will be updated to point to the same point struct after the edges are reversed
        if (face->n_contours > 0)
        {
            ipts = malloc(face->n_contours * sizeof(Point*));
            for (i = 0, n = 0; i < face->n_contours; i++)
            {
                int ni = face->n_contours - i - 1;
                // Build this array reversed, since we will be reversing the edges
                ipts[ni] = face->edges[n]->endpoints[face->contours[i].ip_index];
                n += face->contours[i].n_edges;
            }
        }

        // Faces get their edge order reversed. Don't touch the initial point yet. 
        for (i = 0; i < face->n_edges / 2; i++)
        {
            Edge* temp = face->edges[i];
            int ni = face->n_edges - i - 1;

            face->edges[i] = face->edges[ni];
            face->edges[ni] = temp;
        }

        // Reverse contour lists for multi-contour faces (such as text)
        if (face->n_contours > 0)
        {
            for (i = 0; i < face->n_contours / 2; i++)
            {
                Contour temp = face->contours[i];
                int ni = face->n_contours - i - 1;

                face->contours[i] = face->contours[ni];
                face->contours[ni] = temp;
            }

            // Fix up the edge indicies and IP indicies
            for (i = 0, n = 0; i < face->n_contours; i++)
            {
                face->contours[i].edge_index = n;
                if (ipts[i] == face->edges[n]->endpoints[0])
                    face->contours[i].ip_index = 0;
                else
                {
                    ASSERT(ipts[i] == face->edges[n]->endpoints[1], "Edges in contour don't join up");
                    face->contours[i].ip_index = 1;
                }
                n += face->contours[i].n_edges;
            }

            // Set the face IP to the first contour's IP
            face->initial_point = ipts[0];
            free(ipts);
        }

        face->view_valid = FALSE;
        break;

    case OBJ_VOLUME:
        vol = (Volume*)obj;
        for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            reflect_obj_facing((Object*)face, xc, yc, zc);
        break;

    case OBJ_GROUP:
        grp = (Group*)obj;
        for (o = grp->obj_list.head; o != NULL; o = o->next)
            reflect_obj_facing(o, xc, yc, zc);
        break;
    }
}

// Calculate the smoothed point radius decay.
// - if outside the smooth radius, it's zero
// - if inside, it's a gaussian based on 3 std devs = radius
void
smooth_decay(Point* p, Point* cent)
{
    float two_stds = 2 * halo_rad * halo_rad / 9;  // Temp 2 * (R/3)**2
    float d2 = length_squared(p, cent);

    if (p->decay > 0)       // only set this once
        return;
    if (d2 > halo_rad * halo_rad) // outside the radius
        return;

    p->decay = expf(-d2 / two_stds);
}

// Move a point by the smoothed offsets
void
move_smoothed_point(Point* p, float xoffset, float yoffset, float zoffset)
{
    float smooth = p->cosine * p->decay;

    move_obj((Object*)p, xoffset * smooth, yoffset * smooth, zoffset * smooth);
}

// Calculate the centroid of the central face (just do a quick simple average for now)
void
centroid_face(Face *face, Point *cent)
{
    int i;

    cent->x = cent->y = cent->z = 0;
    for (i = 0; i < face->n_edges; i++)
    {
        Edge* e = face->edges[i];

        cent->x += e->endpoints[0]->x;
        cent->y += e->endpoints[0]->y;
        cent->z += e->endpoints[0]->z;
    }
    cent->x /= face->n_edges;
    cent->y /= face->n_edges;
    cent->z /= face->n_edges;
}

// Calculate the movement factors for the points in a halo around a face.
// The points are moved according to a gaussian decay of distance as well as the
// difference of the normals of faces containing the points. The faces with 
// nonzero movement (decay) factors are placed in a list.
void
calc_halo_params(Face* face, ListHead* halo)
{
    Volume* vol = face->vol;
    Face* f;
    Point cent;
    ArcEdge* ae;
    BezierEdge* be;
    float cosine;
    int i;

    // The face must be part of a volume
    if (vol == NULL)
        return;

    // Calculate the centroid of the central face (just do a quick simple average for now)
    centroid_face(face, &cent);

    // Set all points' decay factors to zero.
    for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
    {
        f->color_decay = 0;
        for (i = 0; i < f->n_edges; i++)
        {
            Edge* e = f->edges[i];

            e->endpoints[0]->decay = 0;
            e->endpoints[1]->decay = 0;
            switch (e->type & ~EDGE_CONSTRUCTION)
            {
            case EDGE_ARC:
                ae = (ArcEdge*)e;
                ae->centre->decay = 0;
                break;

            case EDGE_BEZIER:
                be = (BezierEdge*)e;
                be->ctrlpoints[0]->decay = 0;
                be->ctrlpoints[1]->decay = 0;
                break;
            }
        }
    }

    // Go through the faces on the volume and find the largest cosine(diff of normals) for
    // each unmoved point (the faces that share the point will have different normals). 
    // While here, calculate the distance decay factor as well. 
    for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
    {
        BOOL in_halo = FALSE;
        float face_decay = 0;

        if (f == face)
            continue;   // don't touch the central face

        cosine = pldot(&f->normal, &face->normal);
        if (cosine > 1)
            cosine = 1;
        else if (cosine < 0)
            cosine = 0;

        for (i = 0; i < f->n_edges; i++)
        {
            Edge* e = f->edges[i];

            // Accumulate the maximum cosine over points shared between edges/faces.
            if (cosine > e->endpoints[0]->cosine)
                e->endpoints[0]->cosine = cosine;
            if (cosine > e->endpoints[1]->cosine)
                e->endpoints[1]->cosine = cosine;

            // Decay factor for each point is only set once
            smooth_decay(e->endpoints[0], &cent);
            smooth_decay(e->endpoints[1], &cent);

            // If face is in the halo, set its decay factor to the largest of the points
            if (e->endpoints[0]->decay > 0 && cosine > 0)
                in_halo = TRUE;
            if (e->endpoints[1]->decay > 0 && cosine > 0)
                in_halo = TRUE;
            if (e->endpoints[0]->decay * cosine > face_decay)
                face_decay = e->endpoints[0]->decay * cosine;
            if (e->endpoints[1]->decay * cosine > face_decay)
                face_decay = e->endpoints[1]->decay * cosine;

            // Things like arc centre, Bezier control points etc. also get these set.
            // TODO: not sure what to do about decay for these, they need to be kept in plane.
            // For now just smile and do the dumb thing..
            switch (e->type & ~EDGE_CONSTRUCTION)
            {
            case EDGE_ARC:
                ae = (ArcEdge*)e;
                if (cosine > ae->centre->cosine)
                    ae->centre->cosine = cosine;
                smooth_decay(ae->centre, &cent);
                break;

            case EDGE_BEZIER:
                be = (BezierEdge*)e;
                if (cosine > be->ctrlpoints[0]->cosine)
                    be->ctrlpoints[0]->cosine = cosine;
                if (cosine > be->ctrlpoints[1]->cosine)
                    be->ctrlpoints[1]->cosine = cosine;
                smooth_decay(be->ctrlpoints[0], &cent);
                smooth_decay(be->ctrlpoints[1], &cent);
                break;
            }
        }

        if (in_halo)
        {
            f->color_decay = face_decay;
            link_single((Object*)f, halo);
        }
    }
}

// Move the halo around a face. The given face is assumed to 
// already have been moved by (xoffset,yoffset,zoffset) and to have had its moved flags set.
void
move_halo_around_face(Face* face, float xoffset, float yoffset, float zoffset)
{
    Volume* vol = face->vol;
    Face* f;
    ArcEdge* ae;
    BezierEdge* be;
    int i;

    // The face must be part of a volume
    if (vol == NULL)
        return;

    // Apply the cosine and decay offsets to each point not already moved
    for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
    {
        if (f == face)
            continue;   // don't touch the central face

        for (i = 0; i < f->n_edges; i++)
        {
            Edge* e = f->edges[i];

            move_smoothed_point(e->endpoints[0], xoffset, yoffset, zoffset);
            move_smoothed_point(e->endpoints[1], xoffset, yoffset, zoffset);
            switch (e->type & ~EDGE_CONSTRUCTION)
            {
            case EDGE_ARC:
                ae = (ArcEdge*)e;
                move_smoothed_point(ae->centre, xoffset, yoffset, zoffset);
                break;

            case EDGE_BEZIER:
                be = (BezierEdge*)e;
                move_smoothed_point(be->ctrlpoints[0], xoffset, yoffset, zoffset);
                move_smoothed_point(be->ctrlpoints[1], xoffset, yoffset, zoffset);
                break;
            }
        }
    }
}
