#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>


// Search for a closed chain of connected edges (having coincident endpoints within tol)
// and if found, make a group out of them.
Group *
group_connected_edges(Edge *edge)
{
    Object *obj, *nextobj;
    Edge *e;
    Group *group;
    int n_edges = 0;
    int pass;
    typedef struct End          // an end of the growing chain
    {
        int which_end;          // endpoint 0 or 1
        Edge *edge;             // which edge it is on
    } End;
    End end0, end1;
    BOOL legit = FALSE;
    int max_passes = 0;

    if (((Object *)edge)->type != OBJ_EDGE)
        return NULL;

    // Put the first edge in the list, removing it from the object tree.
    // It had better be in the object tree to start with! Check to be sure,
    // and while here, find out how many top-level edges there are as an
    // upper bound on passes later on.
    for (obj = object_tree.obj_list; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_EDGE)
        {
            max_passes++;
            if (obj == (Object *)edge)
                legit = TRUE;
        }
    }
    if (!legit)
        return NULL;

    // make a group for the edges
    group = group_new();
    delink_group((Object *)edge, &object_tree);
    link_group((Object *)edge, group);
    end0.edge = edge;
    end0.which_end = 0;
    end1.edge = edge;
    end1.which_end = 1;

    // Make passes over the tree, greedy-grouping edges as they are found to connect.
    // If the chain is closed, we should add at least 2 edges per pass, and could add
    // a lot more than that with favourable ordering.
    //
    // While here, link the points themselves into another list, 
    // so a normal can be found early (we may need to reverse
    // the order if the edges were drawn the wrong way round).
    // We can do this with edge endpoints because their next/prev
    // pointers are not used for anything else.
    for (pass = 0; pass < max_passes; pass++)
    {
        BOOL advanced = FALSE;

        for (obj = object_tree.obj_list; obj != NULL; obj = nextobj)
        {
            nextobj = obj->next;

            if (obj->type != OBJ_EDGE)
                continue;

            e = (Edge *)obj;
            if (near_pt(e->endpoints[0], end0.edge->endpoints[end0.which_end]))
            {
                // endpoint 0 of obj connects to end0. Put obj in the list.
                delink_group(obj, &object_tree);
                link_group(obj, group);

                // Update end0 to point to the other end of the new edge we just added
                end0.edge = e;
                end0.which_end = 1;
                n_edges++;
                advanced = TRUE;
            }

            // Check for endpoint 1 connecting to end0 similarly
            else if (near_pt(e->endpoints[1], end0.edge->endpoints[end0.which_end]))
            {
                delink_group(obj, &object_tree);
                link_group(obj, group);
                end0.edge = e;
                end0.which_end = 0;
                n_edges++;
                advanced = TRUE;
            }

            // And the same for end1. New edges are linked at the tail. 
            else if (near_pt(e->endpoints[0], end1.edge->endpoints[end1.which_end]))
            {
                delink_group(obj, &object_tree);
                link_tail_group(obj, group);
                end1.edge = e;
                end1.which_end = 1;
                n_edges++;
                advanced = TRUE;
            }

            else if (near_pt(e->endpoints[1], end1.edge->endpoints[end1.which_end]))
            {
                delink_group(obj, &object_tree);
                link_tail_group(obj, group);
                end1.edge = e;
                end1.which_end = 0;
                n_edges++;
                advanced = TRUE;
            }

            if (near_pt(end0.edge->endpoints[end0.which_end], end1.edge->endpoints[end1.which_end]))
            {
                // We have closed the chain. Mark the group as a closed edge group by
                // setting its lock to Edges (it's hacky, but the lock is written to the
                // file, and it's not used for anything else)
                group->hdr.lock = LOCK_EDGES;
                return group;
            }
        }

        // Every pass should advance at least one of the ends. If we can't close,
        // return the edges unchanged to the object tree and return NULL.
        if (!advanced)
            break;
    }

    // We have not been able to close the chain. Ungroup everything we've grouped
    // so far and return.
    for (obj = group->obj_list; obj != NULL; obj = nextobj)
    {
        nextobj = obj->next;
        delink_group(obj, group);
        link_tail_group(obj, &object_tree);
    }
    purge_obj((Object *)group);

    return NULL;
}

// Make a face object out of a group of connected edges, sharing points as we go.
// The original group is deleted and the face is put into the object tree.
Face *
make_face(Group *group)
{
    Face *face = NULL;
    Edge *e, *next_edge, *prev_edge;
    Plane norm;
    BOOL reverse = FALSE;
    int initial, final, n_edges;
    Point *plist = NULL;
    Point *pt;
    int i;

    // Check that the group is locked at Edges (made by group-connected_edges)
    if (group->hdr.lock != LOCK_EDGES)
        return NULL;

    // Determine normal of points gathered up so far. From this we decide
    // which order to build the final edge array on the face. Join the
    // endpoints up into a list (the next/prev pointers aren't used for
    // anything else)
    e = (Edge *)group->obj_list;
    next_edge = (Edge *)group->obj_list->next;
    if (near_pt(e->endpoints[0], next_edge->endpoints[0]))
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (near_pt(e->endpoints[0], next_edge->endpoints[1]))
    {
        initial = 1;
        pt = e->endpoints[0];
    }
    else if (near_pt(e->endpoints[1], next_edge->endpoints[0]))
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else if (near_pt(e->endpoints[1], next_edge->endpoints[1]))
    {
        initial = 0;
        pt = e->endpoints[1];
    }
    else
    {
        ASSERT(FALSE, "The edges aren't connected");
        return NULL;
    }

    link_tail((Object *)e->endpoints[initial], (Object **)&plist);
    n_edges = 1;

    for (; e->hdr.next != NULL; e = next_edge)
    {
        next_edge = (Edge *)e->hdr.next;

        if (e->hdr.next->type != OBJ_EDGE)
            return NULL;

        if (near_pt(next_edge->endpoints[0], pt))
        {
            next_edge->endpoints[0] = pt;       // share the point
            link_tail((Object *)next_edge->endpoints[0], (Object **)&plist);
            final = 1;
        }
        else if (near_pt(next_edge->endpoints[1], pt))
        {
            next_edge->endpoints[1] = pt;
            link_tail((Object *)next_edge->endpoints[1], (Object **)&plist);
            final = 0;
        }
        else
        {
            return FALSE;
        }
        pt = next_edge->endpoints[final];
        n_edges++;
    }

    // Share the last point back to the beginning
    ASSERT(near_pt(((Edge *)group->obj_list)->endpoints[initial], pt), "The edges don't close at the starting point");
    next_edge->endpoints[final] = ((Edge *)group->obj_list)->endpoints[initial];

    // Get the normal and see if we need to reverse
    polygon_normal((Point *)plist, &norm);
    if (dot(norm.A, norm.B, norm.C, facing_plane->A, facing_plane->B, facing_plane->C) < 0)
    {
        reverse = TRUE;
        norm.A = -norm.A;
        norm.B = -norm.B;
        norm.C = -norm.C;
    }

    // make the face
    face = face_new(FACE_FLAT, norm);
    face->n_edges = n_edges;
    face->max_edges = 1;
    while (face->max_edges <= n_edges)
        face->max_edges <<= 1;          // round up to a power of 2
    if (face->max_edges > 16)           // does it need any more than the default 16?
        face->edges = realloc(face->edges, face->max_edges * sizeof(Edge *));

    // Populate the edge list, sharing points along the way. 
    if (reverse)
    {
        face->initial_point = next_edge->endpoints[final];
        for (i = 0, e = next_edge; e != NULL; e = prev_edge, i++)
        {
            prev_edge = (Edge *)e->hdr.prev;
            face->edges[i] = e;
            delink_group((Object *)e, group);
        }
    }
    else
    {
        face->initial_point = ((Edge *)group->obj_list)->endpoints[initial];
        for (i = 0, e = (Edge *)group->obj_list; e != NULL; e = next_edge, i++)
        {
            next_edge = (Edge *)e->hdr.next;
            face->edges[i] = e;
            delink_group((Object *)e, group);
        }
    }

    // Delete the group
    ASSERT(group->obj_list == NULL, "Edge group is not empty");
    delink_group((Object *)group, &object_tree);
    purge_obj((Object *)group);

    // Finally, update the view list for the face
    gen_view_list_face(face);

    return face;
}

// handle context menu when right-clicking on a highlightable object.
void CALLBACK
right_click(AUX_EVENTREC *event)
{
    HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT));
    int rc;
    POINT pt;
    Object *parent, *sel_obj, *o, *o_next;
    char buf[32];
    LOCK old_parent_lock;
    BOOL group_changed = FALSE;
    BOOL dims_changed = FALSE;
    BOOL sel_changed = FALSE;
    Group *group;
    Face *face;

    if (view_rendered)
        return;

    picked_obj = Pick(event->data[0], event->data[1], OBJ_FACE);
    if (picked_obj == NULL)
        return;

    pt.x = event->data[AUX_MOUSEX];
    pt.y = event->data[AUX_MOUSEY];
    ClientToScreen(auxGetHWND(), &pt);

    // Display the object ID at the top of the menu
    hMenu = GetSubMenu(hMenu, 0);
    switch (picked_obj->type)
    {
    case OBJ_POINT:
        sprintf_s(buf, 32, "Point %d", picked_obj->ID);
        break;
    case OBJ_EDGE:
        sprintf_s(buf, 32, "Edge %d", picked_obj->ID);
        break;
    case OBJ_FACE:
        sprintf_s(buf, 32, "Face %d", picked_obj->ID);
        break;
    case OBJ_VOLUME:
        sprintf_s(buf, 32, "Volume %d", picked_obj->ID);
        break;
    case OBJ_GROUP:
        sprintf_s(buf, 32, "Group %d %s", picked_obj->ID, ((Group *)picked_obj)->title);
        break;
    }
    ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, buf);

    // Find the top-level parent. Disable irrelevant menu items
    parent = find_parent_object(&object_tree, picked_obj, FALSE);
    switch (parent->type)
    {
    case OBJ_EDGE:
        EnableMenuItem(hMenu, ID_LOCKING_FACES, MF_GRAYED);
        EnableMenuItem(hMenu, ID_LOCKING_VOLUME, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_SELECTPARENTVOLUME, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_UNGROUP, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_MAKEFACE, MF_GRAYED);
        break;

    case OBJ_FACE:
        EnableMenuItem(hMenu, ID_LOCKING_VOLUME, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_SELECTPARENTVOLUME, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_GROUPEDGES, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_UNGROUP, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_MAKEFACE, MF_GRAYED);
        break;

    case OBJ_VOLUME:
        EnableMenuItem(hMenu, ID_OBJ_GROUPEDGES, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_UNGROUP, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_MAKEFACE, MF_GRAYED);
        break;

    case OBJ_GROUP:
        EnableMenuItem(hMenu, ID_LOCKING_VOLUME, MF_GRAYED);
        EnableMenuItem(hMenu, ID_LOCKING_FACES, MF_GRAYED);
        EnableMenuItem(hMenu, ID_LOCKING_EDGES, MF_GRAYED);
        EnableMenuItem(hMenu, ID_LOCKING_POINTS, MF_GRAYED);
        EnableMenuItem(hMenu, ID_LOCKING_UNLOCKED, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_SELECTPARENTVOLUME, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_GROUPEDGES, MF_GRAYED);
        break;
    }

    // TODO - bug here when groups are selected, it doesn't show up under is_selected_direct, so item is grayed
    EnableMenuItem(hMenu, ID_OBJ_GROUPSELECTED, is_selected_direct(picked_obj, &o) ? MF_ENABLED : MF_GRAYED);

    // Disable "enter dimensions" for objects that have no dimensions that can be easily changed
    if (!has_dims(picked_obj))
    {
        EnableMenuItem(hMenu, ID_OBJ_ALWAYSSHOWDIMS, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_ENTERDIMENSIONS, MF_GRAYED);
    }
    else
    {
        CheckMenuItem(hMenu, ID_OBJ_ALWAYSSHOWDIMS, picked_obj->show_dims ? MF_CHECKED : MF_UNCHECKED);
    }

    // Check the right lock state for the parent
    old_parent_lock = parent->lock;
    switch (parent->lock)
    {
    case LOCK_VOLUME:
        CheckMenuItem(hMenu, ID_LOCKING_VOLUME, MF_CHECKED);
        break;
    case LOCK_FACES:
        CheckMenuItem(hMenu, ID_LOCKING_FACES, MF_CHECKED);
        break;
    case LOCK_EDGES:
        CheckMenuItem(hMenu, ID_LOCKING_EDGES, MF_CHECKED);
        break;
    case LOCK_POINTS:
        CheckMenuItem(hMenu, ID_LOCKING_POINTS, MF_CHECKED);
        break;
    case LOCK_NONE:
        CheckMenuItem(hMenu, ID_LOCKING_UNLOCKED, MF_CHECKED);
        break;
    }

    display_help("Context menu");

    // Display and track the menu
    rc = TrackPopupMenu
        (
        hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pt.x,
        pt.y,
        0,
        auxGetHWND(),
        NULL
        );

    change_state(app_state);  // back to displaying usual state text
    switch (rc)
    {
    case 0:             // no item chosen
        break;

        // Locking options. If an object is selected, and we have locked it at
        // its own level, we must remove it from selection (it cannot be selected)
    case ID_LOCKING_UNLOCKED:
        parent->lock = LOCK_NONE;
        if (parent->lock == picked_obj->type)
            remove_from_selection(picked_obj);
        break;
    case ID_LOCKING_POINTS:
        parent->lock = LOCK_POINTS;
        if (parent->lock == picked_obj->type)
            remove_from_selection(picked_obj);
        break;
    case ID_LOCKING_EDGES:
        parent->lock = LOCK_EDGES;
        if (parent->lock == picked_obj->type)
            remove_from_selection(picked_obj);
        break;
    case ID_LOCKING_FACES:
        parent->lock = LOCK_FACES;
        if (parent->lock == picked_obj->type)
            remove_from_selection(picked_obj);
        break;
    case ID_LOCKING_VOLUME:
        parent->lock = LOCK_VOLUME;
        if (parent->lock == picked_obj->type)
            remove_from_selection(picked_obj);
        break;

    case ID_OBJ_SELECTPARENTVOLUME:
        sel_changed = TRUE;
        clear_selection(&selection);
        link_single(parent, &selection);
        break;

    case ID_OBJ_ENTERDIMENSIONS:
        show_dims_at(pt, picked_obj, TRUE);
        break;

    case ID_OBJ_ALWAYSSHOWDIMS:
        dims_changed = TRUE;
        if (picked_obj->show_dims)
        {
            picked_obj->show_dims = FALSE;
            CheckMenuItem(hMenu, ID_OBJ_ALWAYSSHOWDIMS, MF_UNCHECKED);
        }
        else
        {
            picked_obj->show_dims = TRUE;
            CheckMenuItem(hMenu, ID_OBJ_ALWAYSSHOWDIMS, MF_CHECKED);
        }
        break;

    case ID_OBJ_GROUPEDGES:
        group = group_connected_edges((Edge *)picked_obj);
        if (group != NULL)
        {
            link_group((Object *)group, &object_tree);
            group_changed = TRUE;
        }
        break;

    case ID_OBJ_MAKEFACE:
        face = make_face((Group *)picked_obj);
        if (face != NULL)
        {
            link_group((Object *)face, &object_tree);
            group_changed = TRUE;
        }
        break;

    case ID_OBJ_GROUPSELECTED:
        group = group_new();
        link_group((Object *)group, &object_tree);
        for (sel_obj = selection; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            // TODO - bug here if a component is selected. You should onnly ever be able to 
            // put the parent in the group.
            delink_group(sel_obj->prev, &object_tree);
            link_tail_group(sel_obj->prev, group);
        }
        clear_selection(&selection);
        group_changed = TRUE;
        break;

    case ID_OBJ_UNGROUP:
        group = (Group *)picked_obj;
        delink_group(picked_obj, &object_tree);
        for (o = group->obj_list; o != NULL; o = o_next)
        {
            o_next = o->next;
            delink_group(o, group);
            link_tail_group(o, &object_tree);
        }
        purge_obj(picked_obj);
        clear_selection(&selection);
        group_changed = TRUE;
        break;
    }

    if (parent->lock != old_parent_lock || group_changed || dims_changed || sel_changed)
    {
        // we have changed the drawing - write an undo checkpoint
        drawing_changed = TRUE;
        write_checkpoint(&object_tree, curr_filename);
    }
}
