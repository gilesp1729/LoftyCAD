#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>


// handle context menu when right-clicking on a highlightable object.
void CALLBACK
right_click(AUX_EVENTREC *event)
{
    POINT pt;

    if (view_rendered || view_printer)
        return;

    picked_obj = Pick(event->data[0], event->data[1], FALSE);

    // If we don't pick anything, attempt a forced pick (we might have clicked a locked volume)
    if (picked_obj == NULL)
        picked_obj = Pick(event->data[0], event->data[1], TRUE);
    if (picked_obj == NULL)
        return;

    pt.x = event->data[AUX_MOUSEX];
    pt.y = event->data[AUX_MOUSEY];
    ClientToScreen(auxGetHWND(), &pt);

    contextmenu(picked_obj, pt);
}

// Display a context menu for an object at a screen location.
void
contextmenu(Object *picked_obj, POINT pt)
{
    HMENU hMenu;
    int rc;
    Object *parent, *sel_obj, *o, *o_next;
    char buf[128], buf2[128];
    LOCK old_parent_lock;
    BOOL group_changed = FALSE;
    BOOL dims_changed = FALSE;
    BOOL sel_changed = FALSE;
    BOOL xform_changed = FALSE;
    BOOL material_changed = FALSE;
    BOOL inserted = FALSE;
    BOOL hole;
    OPERATION op, old_op;
    Group *group;
    Volume* vol;
    Face *face;
    Point *p, *nextp;
    int i;
    OPENFILENAME ofn;
    CHOOSEFONT cf;
    LOGFONT lf;
    char group_filename[256];
    float xc, yc, zc;
    Bbox box;

    // Display the object ID and parent group (optionally) at the top of the menu
    brief_description(picked_obj, buf, 128);
    if (picked_obj->parent_group != NULL && picked_obj->parent_group->hdr.ID != 0)
    {
        strcat_s(buf, 128, " (");
        brief_description((Object*)picked_obj->parent_group, buf2, 128);
        strcat_s(buf, 128, buf2);
        strcat_s(buf, 128, ")");
    }


    // Find the parent object. 
    // Select a menu, and disable irrelevant menu items based on the parent.
    // If we are on a selected object, selections get a special menu.
    parent = find_parent_object(&object_tree, picked_obj, TRUE);
    if (is_selected_direct(picked_obj, &o))
    {
        hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_SELECTION));
        hMenu = GetSubMenu(hMenu, 0);
    }
    else
    {
        switch (parent->type)
        {
        case OBJ_EDGE:
            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_EDGE));
            hMenu = GetSubMenu(hMenu, 0);
            ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, buf);

            EnableMenuItem(hMenu, ID_LOCKING_FACES, MF_GRAYED);
            EnableMenuItem(hMenu, ID_LOCKING_VOLUME, MF_GRAYED);
            break;

        case OBJ_FACE:
            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_FACE));
            hMenu = GetSubMenu(hMenu, 0);
            ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, buf);

            EnableMenuItem(hMenu, ID_LOCKING_VOLUME, MF_GRAYED);
            EnableMenuItem(hMenu, ID_OBJ_MAKEEDGEGROUP, ((Face*)parent)->vol == NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hMenu, ID_OBJ_CLIP_FACE, IS_FLAT((Face *)parent) ? MF_ENABLED : MF_GRAYED);
            break;

        case OBJ_VOLUME:
        default:  // note: never used, but shuts up compiler
            hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_VOL));
            hMenu = GetSubMenu(hMenu, 0);
            ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, buf);
            face = (Face *)(((Volume*)parent)->faces.tail);
            hole = face->extrude_height < 0;
            EnableMenuItem(hMenu, ID_OPERATION_UNION, hole ? MF_GRAYED : MF_ENABLED);
            EnableMenuItem(hMenu, ID_OPERATION_DIFFERENCE, hole ? MF_GRAYED : MF_ENABLED);

            // Don't allow unlocking of some components to keep view lists integrity.
            switch (((Volume*)parent)->max_facetype)
            {
            case FACE_BARREL:
            case FACE_BEZIER:
                EnableMenuItem(hMenu, ID_LOCKING_EDGES, MF_GRAYED);
                // fall through
            case FACE_CYLINDRICAL:
                EnableMenuItem(hMenu, ID_LOCKING_POINTS, MF_GRAYED);
                EnableMenuItem(hMenu, ID_LOCKING_UNLOCKED, MF_GRAYED);
            }
            break;

        case OBJ_GROUP:
            if (is_edge_group((Group *)parent))     // Special treatment for connected edge groups
            {
                BOOL closed;

                hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_EDGEGROUP));
                hMenu = GetSubMenu(hMenu, 0);
                ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, buf);

                // The edge group must be closed to make a face, but open to make a path.
                closed = is_closed_edge_group((Group *)parent);
                EnableMenuItem(hMenu, ID_OBJ_MAKEFACE, closed ? MF_ENABLED : MF_GRAYED);
                EnableMenuItem(hMenu, ID_OBJ_MAKEPATH, !closed ? MF_ENABLED : MF_GRAYED);
                EnableMenuItem(hMenu, ID_OBJ_MAKEBODYREV, curr_path != NULL ? MF_ENABLED : MF_GRAYED);
            }
            else
            {
                hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_GROUP));
                hMenu = GetSubMenu(hMenu, 0);
                ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_GRAYED | MF_STRING, 0, buf);
            }
            break;
        }
    }

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

    // Text editing only for faces that are at top level and retain the text info
    EnableMenuItem(hMenu, ID_OBJ_EDITTEXT,
                   (picked_obj->type == OBJ_FACE && ((Face *)picked_obj)->text != NULL) ? MF_ENABLED : MF_GRAYED);

    // Rounding and chamfering depend on whether the object picked is a point or a face.
    // The parent has to be at least a face (checked above)
    switch (picked_obj->type)
    {
    case OBJ_EDGE:
    case OBJ_VOLUME:
    case OBJ_GROUP:
        EnableMenuItem(hMenu, ID_OBJ_CHAMFERCORNER, MF_GRAYED);
        EnableMenuItem(hMenu, ID_OBJ_ROUNDCORNER, MF_GRAYED);
        break;
    }

    // We can only do render operations on a volume or a group.
    op = OP_MAX;
    switch (picked_obj->type)
    {
    default:
    case OBJ_POINT:
    case OBJ_EDGE:
    case OBJ_FACE:
        break;
    case OBJ_VOLUME:
        op = ((Volume *)picked_obj)->op;
        break;
    case OBJ_GROUP:
        op = ((Group *)picked_obj)->op;
        break;
    }

    old_op = op;
    switch (op)
    {
    case OP_UNION:
        CheckMenuItem(hMenu, ID_OPERATION_UNION, MF_CHECKED);
        break;
    case OP_INTERSECTION:
        CheckMenuItem(hMenu, ID_OPERATION_INTERSECTION, MF_CHECKED);
        break;
    case OP_DIFFERENCE:
        CheckMenuItem(hMenu, ID_OPERATION_DIFFERENCE, MF_CHECKED);
        break;
    case OP_NONE:
        CheckMenuItem(hMenu, ID_OPERATION_NONE, MF_CHECKED);
        break;
    }

    // Check the right lock state for the parent
    old_parent_lock = parent->lock;
    switch (parent->lock)
    {
    case LOCK_GROUP:
        CheckMenuItem(hMenu, ID_LOCKING_GROUP, MF_CHECKED);
        break;
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

    if (picked_obj->type == OBJ_VOLUME)
        load_materials_menu(GetSubMenu(hMenu, 10), FALSE, ((Volume*)picked_obj)->material);
    else
        EnableMenuItem(hMenu, ID_MATERIALS_NEW, MF_GRAYED);

    display_help("Context menu");

    // Display and track the menu
    suppress_drawing = TRUE;
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

    suppress_drawing = FALSE;
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
    case ID_LOCKING_GROUP:
        parent->lock = LOCK_GROUP;
        if (parent->lock == picked_obj->type)
            remove_from_selection(picked_obj);
        break;

    case ID_OPERATION_UNION:
        op = OP_UNION;
        break;
    case ID_OPERATION_INTERSECTION:
        op = OP_INTERSECTION;
        break;
    case ID_OPERATION_DIFFERENCE:
        op = OP_DIFFERENCE;
        break;
    case ID_OPERATION_NONE:
        op = OP_NONE;
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
        group = group_connected_edges((Edge*)picked_obj);
        if (group != NULL)
        {
            link_group((Object*)group, &object_tree);
            clear_selection(&selection);
            group_changed = TRUE;
        }
        break;

    case ID_OBJ_MAKEFACE:
        face = make_face((Group*)picked_obj);
        if (face != NULL)
        {
            link_group((Object*)face, &object_tree);
            clear_selection(&selection);
            group_changed = TRUE;
        }
        break;

    case ID_OBJ_MAKEPATH:
        curr_path = picked_obj;
        group_changed = TRUE;
        break;

    case ID_OBJ_MAKEBODYREV:
        // TODO: Some sort of UI to support negative BOR (holes)
        vol = make_body_of_revolution((Group*)picked_obj, FALSE);
        if (vol != NULL)
        {
            link_group((Object*)vol, &object_tree);
            clear_selection(&selection);
            group_changed = TRUE;
        }
        break;

    case ID_OBJ_MAKEEDGEGROUP:
        delink_group(picked_obj, &object_tree);         // TODO this and other places - don't assume we're in the object tree!
        face = (Face*)picked_obj;
        group = group_new();
        for (i = 0; i < face->n_edges; i++)
            link_tail_group((Object *)face->edges[i], group);   // TODO watch circles - the normal comes out wrong
        group->hdr.lock = LOCK_POINTS;
        link_group((Object *)group, &object_tree);
        group_changed = TRUE;
        break;

    case ID_OBJ_CLIP_FACE:
        face = (Face*)picked_obj;
#if 0
        // Get face normal (we know it is flat) and put it into the trackball as a quaternion
        z.A = 0;
        z.B = 0;
        z.C = 1;
        plcross(&z, &face->normal, &axis);
        if (nz(axis.A) && nz(axis.B) && nz(axis.C))
        {
            trackball_InitQuat(quat_XY);
        }
        else
        {
            pz.x = 0;
            pz.y = 0;
            pz.z = 1;
            origin.x = 0;
            origin.y = 0;
            origin.z = 0;
            pn.x = face->normal.A;
            pn.y = face->normal.B;
            pn.z = face->normal.C;
            phi = angle3(&pz, &origin, &pn, &axis);
            trackball_axis_to_quat(&axis.A, phi, quat);
            trackball_InitQuat(quat);
        }
#endif //0 

        // Set up clip plane in GL
        // Solve plane equation for its 4th component 
        clip_plane[0] = face->normal.A;
        clip_plane[1] = face->normal.B;
        clip_plane[2] = face->normal.C;
        clip_plane[3] = -(
            face->normal.A * face->normal.refpt.x
            +
            face->normal.B * face->normal.refpt.y
            +
            face->normal.C * face->normal.refpt.z
            );
        glEnable(GL_CLIP_PLANE0);
        view_clipped = TRUE;
        invalidate_dl();
        break;

    case ID_OBJ_GROUPSELECTED:
        group = group_new();
        link_group((Object*)group, &object_tree);
        for (sel_obj = selection.head; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            // TODO - bug here if a component is selected. You should only ever be able to 
            // put the parent in the group.
            delink_group(sel_obj->prev, &object_tree);
            link_tail_group(sel_obj->prev, group);
        }
        clear_selection(&selection);
        group_changed = TRUE;
        break;

    case ID_OBJ_UNGROUP:
        group = (Group*)picked_obj;
        delink_group(picked_obj, &object_tree);         // TODO this and other places - don't assume we're in the object tree!
        if (is_edge_group(group))
            disconnect_edges_in_group(group);
        for (o = group->obj_list.head; o != NULL; o = o_next)
        {
            o_next = o->next;
            delink_group(o, group);
            link_tail_group(o, &object_tree);
        }
        purge_obj(picked_obj);
        clear_selection(&selection);
        group_changed = TRUE;
        break;

    case ID_OBJ_SAVEGROUP:
        memset(&ofn, 0, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = auxGetHWND();
        ofn.lpstrFilter = "LoftyCAD Files (*.LCD)\0*.LCD\0All Files\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = "lcd";
        strcpy_s(group_filename, 256, "GROUP");
        ofn.lpstrFile = group_filename;
        ofn.nMaxFile = 256;
        ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
        if (GetSaveFileName(&ofn))
        {
            group = (Group*)picked_obj;
            serialise_tree(group, group_filename);
        }

        break;

        // The following operations work on a point, or on all points in a face.
    // The parent must be a face.
    case ID_OBJ_CHAMFERCORNER:
        switch (picked_obj->type)
        {
        case OBJ_POINT:
            face = (Face*)parent;
            insert_chamfer_round((Point*)picked_obj, face, chamfer_rad, EDGE_STRAIGHT, FALSE);
            face->type = FACE_FLAT;
            face->view_valid = FALSE;
            break;
        case OBJ_FACE:
            face = (Face*)picked_obj;
            p = face->initial_point;
            for (i = 0; i < face->n_edges - 1; i++)
            {
                // find the next point before the edge is modified
                if (p == face->edges[i]->endpoints[0])
                    nextp = face->edges[i]->endpoints[1];
                else
                {
                    ASSERT(p == face->edges[i]->endpoints[1], "Points not connected properly");
                    nextp = face->edges[i]->endpoints[0];
                }
                // insert the extra edge
                insert_chamfer_round(p, face, chamfer_rad, EDGE_STRAIGHT, TRUE);
                p = nextp;
                if (i > 0)
                    i++;
            }
            face->type = FACE_FLAT;
            face->view_valid = FALSE;
        }
        inserted = TRUE;
        break;

    case ID_OBJ_ROUNDCORNER:
        switch (picked_obj->type)
        {
        case OBJ_POINT:
            face = (Face*)parent;
            insert_chamfer_round((Point*)picked_obj, face, round_rad, EDGE_ARC, FALSE);
            face->type = FACE_FLAT;
            face->view_valid = FALSE;
            break;
        case OBJ_FACE:
            face = (Face*)picked_obj;

            // TODO - find shortest edge and ensure round radius <= (shortest_len / 2 + tol)
            // to avoid distorting the face



            p = face->initial_point;
            for (i = 0; i < face->n_edges - 1; i++)
            {
                if (p == face->edges[i]->endpoints[0])
                    nextp = face->edges[i]->endpoints[1];
                else
                {
                    ASSERT(p == face->edges[i]->endpoints[1], "Points not connected properly");
                    nextp = face->edges[i]->endpoints[0];
                }
                insert_chamfer_round(p, face, round_rad, EDGE_ARC, TRUE);
                p = nextp;
                if (i > 0)
                    i++;
            }
            face->type = FACE_FLAT;
            face->view_valid = FALSE;
        }
        inserted = TRUE;
        break;

    case ID_OBJ_ROTATE90:
        find_obj_pivot(parent, &xc, &yc, &zc);
        rotate_obj_90_facing(parent, xc, yc, zc);
        clear_move_copy_flags(parent);
        xform_changed = TRUE;
        break;

    case ID_OBJ_ROTATESELECTED90:
        // find the centre of all the pivots of the selected objects
        clear_bbox(&box);
        for (sel_obj = selection.head; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            find_obj_pivot(sel_obj->prev, &xc, &yc, &zc);
            expand_bbox_coords(&box, xc, yc, zc);
        }
        xc = (box.xmin + box.xmax) / 2;
        yc = (box.ymin + box.xmax) / 2;
        zc = (box.zmin + box.zmax) / 2;
        for (sel_obj = selection.head; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            rotate_obj_90_facing(sel_obj->prev, xc, yc, zc);
            clear_move_copy_flags(sel_obj->prev);
        }
        xform_changed = TRUE;
        break;

    case ID_OBJ_REFLECT:
        find_obj_pivot(parent, &xc, &yc, &zc);
        reflect_obj_facing(parent, xc, yc, zc);
        clear_move_copy_flags(parent);
        xform_changed = TRUE;
        break;

    case ID_OBJ_REFLECTSELECTED:
        clear_bbox(&box);
        for (sel_obj = selection.head; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            find_obj_pivot(sel_obj->prev, &xc, &yc, &zc);
            expand_bbox_coords(&box, xc, yc, zc);
        }
        xc = (box.xmin + box.xmax) / 2;
        yc = (box.ymin + box.xmax) / 2;
        zc = (box.zmin + box.zmax) / 2;
        for (sel_obj = selection.head; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            reflect_obj_facing(sel_obj->prev, xc, yc, zc);
            clear_move_copy_flags(sel_obj->prev);
        }
        xform_changed = TRUE;
        break;

    case ID_OBJ_EDITTEXT:
        face = (Face*)picked_obj;
        curr_text = face->text;
        if (curr_text == NULL)
            break;

        // Put up choosefont and preload, delete old face and replace at same point(s)
        memset(&cf, 0, sizeof(CHOOSEFONT));
        cf.lStructSize = sizeof(CHOOSEFONT);
        cf.Flags = CF_NOSIZESEL | CF_TTONLY | CF_ENABLETEMPLATE | CF_ENABLEHOOK | CF_INITTOLOGFONTSTRUCT;
        cf.lpTemplateName = MAKEINTRESOURCE(1543);
        cf.lpfnHook = font_hook;
        cf.lCustData = (LPARAM)curr_text;
        memset(&lf, 0, sizeof(LOGFONT));
        lf.lfWeight = curr_text->bold ? FW_BOLD : FW_NORMAL;
        lf.lfItalic = curr_text->italic;
        strcpy_s(lf.lfFaceName, 32, curr_text->font);
        cf.lpLogFont = &lf;
        if (!ChooseFont(&cf))
            break;

        curr_text->bold = lf.lfWeight > FW_NORMAL;
        curr_text->italic = lf.lfItalic;
        strcpy_s(curr_text->font, 32, lf.lfFaceName);

        // Replace the face with a new one, having the same lock state
        face = text_face(curr_text, face);
        if (face == NULL)
            break;
        inserted = TRUE;
        break;

    case ID_MATERIALS_NEW:
        // Dialog box to edit or add a material, then return the one selected
        i = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_MATERIALS), auxGetHWND(), materials_dialog, (LPARAM)picked_obj);
        if (i >= 0)
        {
            ((Volume*)picked_obj)->material = i;
            material_changed = TRUE;
        }
        break;

        // test here for a possibly variable number of ID_MATERIALS_BASE + n (for n in [0,MAX_MATERIAL])
    default:
        if (rc < ID_MATERIAL_BASE || rc >= ID_MATERIAL_BASE + MAX_MATERIAL)
            break;

        hMenu = GetSubMenu(hMenu, 3);

        // Find which material index this menu item corresponds to. This number is at the
        // front of the menu string.
        GetMenuString(hMenu, rc, buf, 64, 0);
        i = atoi(buf);
        ((Volume*)picked_obj)->material = i;
        material_changed = TRUE;
        break;
    }

    if (op != old_op)
    {
        switch (picked_obj->type)
        {
        case OBJ_VOLUME:
            ((Volume *)picked_obj)->op = op;
            break;
        case OBJ_GROUP:
            ((Group *)picked_obj)->op = op;
            break;
        }

        invalidate_all_view_lists(parent, picked_obj, 0, 0, 0);
    }

    if (material_changed || xform_changed)
        invalidate_all_view_lists(parent, picked_obj, 0, 0, 0);

    if (parent->lock != old_parent_lock || op != old_op || group_changed || dims_changed || sel_changed || xform_changed || inserted || material_changed)
    {
        // we have changed the drawing - write an undo checkpoint
        update_drawing();
    }
}


// Load and save material props to/from dialog.
void
load_material(HWND hDlg, int mat)
{
    char buf[16];

    SetDlgItemText(hDlg, IDC_MATERIAL_NAME, materials[mat].name);
    sprintf_s(buf, 16, "%.2f", materials[mat].color[0]);
    SetDlgItemText(hDlg, IDC_MATERIAL_RED, buf);
    sprintf_s(buf, 16, "%.2f", materials[mat].color[1]);
    SetDlgItemText(hDlg, IDC_MATERIAL_GREEN, buf);
    sprintf_s(buf, 16, "%.2f", materials[mat].color[2]);
    SetDlgItemText(hDlg, IDC_MATERIAL_BLUE, buf);
    sprintf_s(buf, 16, "%.0f", materials[mat].shiny);
    SetDlgItemText(hDlg, IDC_MATERIAL_SHINY, buf);

    // disable changes to default material
    EnableWindow(GetDlgItem(hDlg, IDC_MATERIAL_NAME), mat > 0);
    EnableWindow(GetDlgItem(hDlg, IDC_MATERIAL_RED), mat > 0);
    EnableWindow(GetDlgItem(hDlg, IDC_MATERIAL_GREEN), mat > 0);
    EnableWindow(GetDlgItem(hDlg, IDC_MATERIAL_BLUE), mat > 0);
    EnableWindow(GetDlgItem(hDlg, IDC_MATERIAL_SHINY), mat > 0);
}

void
save_material(HWND hDlg, int mat)
{
    char buf[16];

    GetDlgItemText(hDlg, IDC_MATERIAL_NAME, materials[mat].name, 64);
    GetDlgItemText(hDlg, IDC_MATERIAL_RED, buf, 16);
    materials[mat].color[0] = (float)atof(buf);
    GetDlgItemText(hDlg, IDC_MATERIAL_GREEN, buf, 16);
    materials[mat].color[1] = (float)atof(buf);
    GetDlgItemText(hDlg, IDC_MATERIAL_BLUE, buf, 16);
    materials[mat].color[2] = (float)atof(buf);
    GetDlgItemText(hDlg, IDC_MATERIAL_SHINY, buf, 16);
    materials[mat].shiny = (float)atof(buf);
}

// Materials dialog procedure. Edit of add new materials, then return the index of
// the material selected, or -1 if cancelled.
int WINAPI
materials_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char buf[64];
    static Object* obj;
    int i, k;
    static int mat_index[MAX_MATERIAL];
    static int mat;
    static BOOL text_changed;

    switch (msg)
    {
    case WM_INITDIALOG:
        obj = (Object*)lParam;
        ASSERT(obj == NULL || obj->type == OBJ_VOLUME, "This has to be a volume");

        // Preload the combo with existing materials and select the current object's material
        mat = (obj != NULL) ? ((Volume*)obj)->material : 0;

        for (i = 0, k = 0; i < MAX_MATERIAL; i++)
        {
            if (materials[i].valid)
            {
                mat_index[k] = i;
                SendDlgItemMessage(hWnd, IDC_COMBO_MATERIAL, CB_INSERTSTRING, k, (LPARAM)materials[i].name);
                if (i == mat)
                    SendDlgItemMessage(hWnd, IDC_COMBO_MATERIAL, CB_SETCURSEL, k, 0);
                k++;
            }
        }
        SetDlgItemInt(hWnd, IDC_STATIC_MAT_INDEX, mat, FALSE);
        load_material(hWnd, mat);
        text_changed = FALSE;
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            k = SendDlgItemMessage(hWnd, IDC_COMBO_MATERIAL, CB_GETCURSEL, 0, 0);
            mat = mat_index[k];
            save_material(hWnd, mat);
            EndDialog(hWnd, mat);
            break;

        case IDCANCEL:
            EndDialog(hWnd, -1);
            break;

        case IDC_COMBO_MATERIAL:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                save_material(hWnd, mat);
                k = SendDlgItemMessage(hWnd, IDC_COMBO_MATERIAL, CB_GETCURSEL, 0, 0);
                mat = mat_index[k];
                SetDlgItemInt(hWnd, IDC_STATIC_MAT_INDEX, mat, FALSE);
                load_material(hWnd, mat);
                break;

            case CBN_EDITCHANGE:
                text_changed = TRUE;
                break;

            case CBN_KILLFOCUS:
                if (text_changed)
                {
                    text_changed = FALSE;
                    SendDlgItemMessage(hWnd, IDC_COMBO_MATERIAL, WM_GETTEXT, 64, (LPARAM)buf);
                    for (i = 0; i < MAX_MATERIAL; i++)
                    {
                        if (materials[i].valid)
                            mat = i;
                    }
                    mat++;
                    if (mat < MAX_MATERIAL)
                    {
                        strcpy_s(materials[mat].name, 64, buf);
                        k = SendDlgItemMessage(hWnd, IDC_COMBO_MATERIAL, CB_ADDSTRING, 0, (LPARAM)materials[mat].name);
                        mat_index[k] = mat;
                        materials[mat].hidden = FALSE;
                        materials[mat].valid = TRUE;
                        SetDlgItemInt(hWnd, IDC_STATIC_MAT_INDEX, mat, FALSE);
                        load_material(hWnd, mat);
                    }
                }
                break;
            }
            break;
        }
        break;
    }
    return 0;
}
