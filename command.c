#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <shellapi.h>

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char buf[64];

    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        strcpy_s(buf, 64, "Version ");
        strcat_s(buf, 64, LOFTYCAD_VERSION);
        strcat_s(buf, 64, " (Build ");
        strcat_s(buf, 64, __DATE__);
        strcat_s(buf, 64, ") ");
        strcat_s(buf, 64, LOFTYCAD_BRANCH);
        SendDlgItemMessage(hDlg, IDC_STATIC_BUILD, WM_SETTEXT, 0, (LPARAM)buf);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void CALLBACK
check_file_changed(HWND hWnd)
{
    if (drawing_changed)
    {
        int rc = MessageBox(hWnd, "File modified. Save it?", curr_filename, MB_YESNOCANCEL | MB_ICONWARNING);

        if (rc == IDCANCEL)
        {
            return;
        }
        else if (rc == IDYES)
        {
            if (curr_filename[0] == '\0')
                SendMessage(hWnd, WM_COMMAND, ID_FILE_SAVEAS, 0);
            else
                serialise_tree(&object_tree, curr_filename);
        }
    }

    clean_checkpoints(curr_filename);
    DestroyWindow(hWnd);
    close_comms();
}

// Load material names up to menu, with all non-hidden materials checked, or a single
// selected one checked.
void
load_materials_menu(HMENU hMenu, BOOL show_all_checks, int which_check)
{
    int i;

    for (i = 0; i < MAX_MATERIAL; i++)
        DeleteMenu(hMenu, 2, MF_BYPOSITION);  // clear them all out (leave the first slot for New..)

    for (i = 0; i < MAX_MATERIAL; i++)
    {
        char mat_string[64];

        if (!materials[i].valid)
            continue;
        sprintf_s(mat_string, 64, "%d %s", i, materials[i].name);
        AppendMenu(hMenu, 0, ID_MATERIAL_BASE + i, mat_string);
        if (show_all_checks)
            CheckMenuItem(hMenu, ID_MATERIAL_BASE + i, materials[i].hidden ? MF_UNCHECKED : MF_CHECKED);
    }
    if (!show_all_checks)
        CheckMenuItem(hMenu, ID_MATERIAL_BASE + which_check, MF_CHECKED);
}

// Set/reset rendered view blanking of toolbar buttons.
void
enable_rendered_view_items(void)
{
    EnableWindow(GetDlgItem(hWndToolbar, IDB_EDGE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_RECT), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_CIRCLE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_ARC_EDGE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_BEZIER_EDGE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_EXTRUDE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_EXTRUDE_LOCAL), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_TEXT), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_SCALE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_ROTATE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_EDGE), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_RECT), !view_rendered);
    EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_CIRCLE), !view_rendered);

}

// Process WM_COMMAND, INITMENUPOPUP and the like, from TK window proc
int CALLBACK
Command(int message, int wParam, int lParam)
{
    HMENU hMenu;
    OPENFILENAME ofn;
    char window_title[256];
    char button_title[256];
    char new_filename[256];
    Object* obj;
    Group* group;
    char* pdot;
    char buf[64];
    int i;
    BOOL rc;
    char* filetypes[6] = {"lcd", "stl", "amf", "obj", "off", "gcode"};

    // Check for micro moves
    if (micro_moved)
    {
        update_drawing();
        micro_moved = FALSE;
    }

    switch (message)
    {
    case WM_SETCURSOR:
        display_cursor(app_state);
        return TRUE;

    case WM_DROPFILES:
        DragQueryFile((HDROP)wParam, 0, new_filename, 256);

        // If an LCD file, open it. If one of the recognised import formats, import it to a group.
        pdot = strrchr(new_filename, '.');
        for (i = 0; i < 6; i++)
        {
            if (_stricmp(pdot + 1, filetypes[i]) == 0)
                break;
        }
        if (i == 6)
            break;   // not recognised, just forget it

        if (i == 0)
        {
            HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_OPENORIMPORT));
            int rc;
            POINT pt;

            // allow option to open LCD or import to group
            DragQueryPoint((HDROP)wParam, &pt);
            hMenu = GetSubMenu(hMenu, 0);
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

            if (rc == 0)
                break;
            if (rc == ID_FILE_OPENFILE)
            {
                // check if needs saving, close, and open new file
                if (drawing_changed)
                {
                    int rc = MessageBox(auxGetHWND(), "File modified. Save it?", curr_filename, MB_YESNOCANCEL | MB_ICONWARNING);

                    if (rc == IDCANCEL)
                        break;

                    if (rc == IDYES)
                    {
                        if (curr_filename[0] == '\0')
                            SendMessage(auxGetHWND(), WM_COMMAND, ID_FILE_SAVEAS, 0);
                        else
                            serialise_tree(&object_tree, curr_filename);
                    }
                }

                clear_selection(&selection);
                purge_tree(&object_tree, clipboard.head != NULL, &saved_list);
                drawing_changed = FALSE;
                view_rendered = FALSE;
                enable_rendered_view_items();
                clean_checkpoints(curr_filename);
                curr_filename[0] = '\0';
                object_tree.title[0] = '\0';
                SetWindowText(auxGetHWND(), "LoftyCAD");
                SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)"Slice Current Model");
                EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), FALSE);

                if (deserialise_tree(&object_tree, new_filename, FALSE))
                {
                    strcpy_s(curr_filename, 256, new_filename);
                    drawing_changed = FALSE;
                    strcpy_s(window_title, 256, curr_filename);
                    strcat_s(window_title, 256, " - ");
                    strcat_s(window_title, 256, object_tree.title);
                    SetWindowText(auxGetHWND(), window_title);
                    strcpy_s(button_title, 256, "Export and Slice ");
                    strcat_s(button_title, 256, curr_filename);
                    SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)button_title);
                    EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), TRUE);
                    hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
                    hMenu = GetSubMenu(hMenu, 9);
                    insert_filename_to_MRU(hMenu, curr_filename);
                    xform_list.head = NULL;
                    xform_list.tail = NULL;
                    gen_view_list_tree_volumes(&object_tree);
                    populate_treeview();
                }

                break;
            }
        }

        // Import the file to a group
        group = group_new();
        rc = FALSE;

        switch (i)
        {
        case 0:
            rc = deserialise_tree(group, new_filename, TRUE);
            break;
        case 1:
            rc = read_stl_to_group(group, new_filename);
            break;
        case 2:
            rc = read_amf_to_group(group, new_filename);
            break;
        case 3:
            rc = read_obj_to_group(group, new_filename);
            break;
        case 4:
            rc = read_off_to_group(group, new_filename);
            break;
        case 5:
            // These don't go to the object tree, but to the gcode tree. Only one at a time.
            purge_zpoly_edges(&gcode_tree);
            rc = read_gcode_to_group(&gcode_tree, new_filename);
            invalidate_dl();
            SendMessage(hWndPropSheet, PSM_SETCURSEL, TAB_PREVIEW, 0);  // select print preview tab
            SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FILENAME, WM_SETTEXT, 0, (LPARAM)new_filename);
            SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FIL_USED, WM_SETTEXT, 0, (LPARAM)gcode_tree.fil_used);
            SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_EST_PRINT, WM_SETTEXT, 0, (LPARAM)gcode_tree.est_print);
            EnableWindow(GetDlgItem(hWndPrintPreview, IDB_PRINTER_PRINT), TRUE);
            break;
        }
        if (i < 5)
        {
            if (rc)
            {
                link_group((Object*)group, &object_tree);
                update_drawing();
            }
        }
        else
        {
            purge_obj((Object*)group);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_EXIT:
            check_file_changed(auxGetHWND());
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), auxGetHWND(), About);
            break;

        case ID_PREFERENCES_SNAPTOGRID:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
            if (snapping_to_grid)
            {
                snapping_to_grid = FALSE;
                CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOGRID, MF_UNCHECKED);
            }
            else
            {
                snapping_to_grid = TRUE;
                CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOGRID, MF_CHECKED);
            }
            break;

        case ID_PREFERENCES_SNAPTOANGLE:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
            if (snapping_to_angle)
            {
                snapping_to_angle = FALSE;
                CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOANGLE, MF_UNCHECKED);
            }
            else
            {
                snapping_to_angle = TRUE;
                CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOANGLE, MF_CHECKED);
            }
            break;

        case ID_VIEW_TOOLS:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_tools)
            {
                ShowWindow(hWndPropSheet, SW_HIDE);
                view_tools = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_TOOLS, MF_UNCHECKED);
            }
            else
            {
                ShowWindow(hWndPropSheet, SW_SHOW);
                view_tools = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_TOOLS, MF_CHECKED);
            }
            break;

        case ID_VIEW_DEBUGLOG:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_debug)
            {
                ShowWindow(hWndDebug, SW_HIDE);
                view_debug = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_DEBUGLOG, MF_UNCHECKED);
            }
            else
            {
                ShowWindow(hWndDebug, SW_SHOW);
                view_debug = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_DEBUGLOG, MF_CHECKED);
            }
            break;

        case ID_VIEW_HELP:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_help)
            {
                ShowWindow(hWndHelp, SW_HIDE);
                view_help = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_HELP, MF_UNCHECKED);
            }
            else
            {
                ShowWindow(hWndHelp, SW_SHOW);
                view_help = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_HELP, MF_CHECKED);
            }
            break;

        case ID_VIEW_TREE:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_tree)
            {
                ShowWindow(hWndTree, SW_HIDE);
                view_tree = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_TREE, MF_UNCHECKED);
            }
            else
            {
                ShowWindow(hWndTree, SW_SHOW);
                view_tree = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_TREE, MF_CHECKED);
            }
            break;

        case ID_VIEW_CONSTRUCTIONEDGES:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_constr)
            {
                view_constr = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_CONSTRUCTIONEDGES, MF_UNCHECKED);
            }
            else
            {
                view_constr = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_CONSTRUCTIONEDGES, MF_CHECKED);
            }
            EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_EDGE), view_constr);
            EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_RECT), view_constr);
            EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_CIRCLE), view_constr);
            break;

        case ID_VIEW_RENDEREDVIEW:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_rendered)
            {
                view_rendered = FALSE;
                glEnable(GL_BLEND);
                CheckMenuItem(hMenu, ID_VIEW_RENDEREDVIEW, MF_UNCHECKED);
                invalidate_dl();
            }
            else
            {
                xform_list.head = NULL;
                xform_list.tail = NULL;
                gen_view_list_tree_volumes(&object_tree);
                if (!gen_view_list_tree_surfaces(&object_tree, &object_tree))
                    break;

                view_rendered = TRUE;
                glDisable(GL_BLEND);
                CheckMenuItem(hMenu, ID_VIEW_RENDEREDVIEW, MF_CHECKED);
            }
            enable_rendered_view_items();
            break;

        case ID_VIEW_PRINTBED:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_printbed)
            {
                view_printbed = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_PRINTBED, MF_UNCHECKED);
            }
            else
            {
                view_printbed = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_PRINTBED, MF_CHECKED);
            }
            invalidate_dl();
            break;

#ifdef DEBUG_HIGHLIGHTING_ENABLED
        case ID_DEBUG_BBOXES:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (debug_view_bbox)
            {
                debug_view_bbox = FALSE;
                CheckMenuItem(hMenu, ID_DEBUG_BBOXES, MF_UNCHECKED);
            }
            else
            {
                debug_view_bbox = TRUE;
                CheckMenuItem(hMenu, ID_DEBUG_BBOXES, MF_CHECKED);
            }
            break;

        case ID_DEBUG_NORMALS:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (debug_view_normals)
            {
                debug_view_normals = FALSE;
                CheckMenuItem(hMenu, ID_DEBUG_NORMALS, MF_UNCHECKED);
            }
            else
            {
                debug_view_normals = TRUE;
                CheckMenuItem(hMenu, ID_DEBUG_NORMALS, MF_CHECKED);
            }
            break;
        case ID_DEBUG_VIEWLIST:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (debug_view_viewlist)
            {
                debug_view_viewlist = FALSE;
                CheckMenuItem(hMenu, ID_DEBUG_VIEWLIST, MF_UNCHECKED);
            }
            else
            {
                debug_view_viewlist = TRUE;
                CheckMenuItem(hMenu, ID_DEBUG_VIEWLIST, MF_CHECKED);
            }
            break;
#endif

        case ID_VIEW_TOP:
            facing_plane = &plane_XY;
            facing_index = PLANE_XY;
#ifdef DEBUG_COMMAND_FACING
            Log("Facing plane XY\r\n");
#endif
            trackball_InitQuat(quat_XY);
            break;

        case ID_VIEW_FRONT:
            facing_plane = &plane_YZ;
            facing_index = PLANE_YZ;
#ifdef DEBUG_COMMAND_FACING
            Log("Facing plane YZ\r\n");
#endif
            trackball_InitQuat(quat_YZ);
            break;

        case ID_VIEW_LEFT:
            facing_plane = &plane_XZ;
            facing_index = PLANE_XZ;
#ifdef DEBUG_COMMAND_FACING
            Log("Facing plane XZ\r\n");
#endif
            trackball_InitQuat(quat_XZ);
            break;

        case ID_VIEW_BOTTOM:
            facing_plane = &plane_mXY;
            facing_index = PLANE_MINUS_XY;
#ifdef DEBUG_COMMAND_FACING
            Log("Facing plane -XY\r\n");
#endif
            trackball_InitQuat(quat_mXY);
            break;

        case ID_VIEW_BACK:
            facing_plane = &plane_mYZ;
            facing_index = PLANE_MINUS_YZ;
#ifdef DEBUG_COMMAND_FACING
            Log("Facing plane -YZ\r\n");
#endif
            trackball_InitQuat(quat_mYZ);
            break;

        case ID_VIEW_RIGHT:
            facing_plane = &plane_mXZ;
            facing_index = PLANE_MINUS_XZ;
#ifdef DEBUG_COMMAND_FACING
            Log("Facing plane -XZ\r\n");
#endif
            trackball_InitQuat(quat_mXZ);
            break;

        case ID_VIEW_ORTHO:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (!view_ortho)
            {
                view_ortho = TRUE;
                CheckMenuItem(hMenu, ID_VIEW_PERSPECTIVE, MF_UNCHECKED);
                CheckMenuItem(hMenu, ID_VIEW_ORTHO, MF_CHECKED);
            }
            break;

        case ID_VIEW_PERSPECTIVE:
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            if (view_ortho)
            {
                view_ortho = FALSE;
                CheckMenuItem(hMenu, ID_VIEW_ORTHO, MF_UNCHECKED);
                CheckMenuItem(hMenu, ID_VIEW_PERSPECTIVE, MF_CHECKED);
            }
            break;

        case ID_VIEW_BLEND_OPAQUE:
            view_blend = BLEND_OPAQUE;
            glBlendFunc(GL_ONE, GL_ZERO);                           // no blending
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_OPAQUE, MF_CHECKED);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_MULTIPLY, MF_UNCHECKED);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_ALPHA, MF_UNCHECKED);
            break;

        case ID_VIEW_BLEND_MULTIPLY:
            view_blend = BLEND_MULTIPLY;
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);                     // multiply blending
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_OPAQUE, MF_UNCHECKED);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_MULTIPLY, MF_CHECKED);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_ALPHA, MF_UNCHECKED);
            break;

        case ID_VIEW_BLEND_ALPHA:
            view_blend = BLEND_ALPHA;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);      // alpha blending
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_OPAQUE, MF_UNCHECKED);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_MULTIPLY, MF_UNCHECKED);
            CheckMenuItem(hMenu, ID_VIEW_BLEND_ALPHA, MF_CHECKED);
            break;

        case ID_FILE_NEW:
        case ID_FILE_OPEN:
            if (drawing_changed)
            {
                int rc = MessageBox(auxGetHWND(), "File modified. Save it?", curr_filename, MB_YESNOCANCEL | MB_ICONWARNING);

                if (rc == IDCANCEL)
                    break;
                
                if (rc == IDYES)
                {
                    if (curr_filename[0] == '\0')
                        SendMessage(auxGetHWND(), WM_COMMAND, ID_FILE_SAVEAS, 0);
                    else
                        serialise_tree(&object_tree, curr_filename);
                }
            }

            clear_selection(&selection);
            purge_tree(&object_tree, clipboard.head != NULL, &saved_list);
            drawing_changed = FALSE;
            view_rendered = FALSE;
            enable_rendered_view_items();
            clean_checkpoints(curr_filename);
            curr_filename[0] = '\0';
            object_tree.title[0] = '\0';
            SetWindowText(auxGetHWND(), "LoftyCAD");
            SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)"Slice Current Model");
            EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), FALSE);

            if (LOWORD(wParam) == ID_FILE_NEW)
            {
                invalidate_dl();
                break;
            }

            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = auxGetHWND();
            ofn.lpstrFilter = "LoftyCAD Files (*.LCD)\0*.LCD\0All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "lcd";
            ofn.lpstrFile = curr_filename;
            ofn.nMaxFile = 256;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
            if (GetOpenFileName(&ofn))
            {
                deserialise_tree(&object_tree, curr_filename, FALSE);
                drawing_changed = FALSE;
                strcpy_s(window_title, 256, curr_filename);
                strcat_s(window_title, 256, " - ");
                strcat_s(window_title, 256, object_tree.title);
                SetWindowText(auxGetHWND(), window_title);
                strcpy_s(button_title, 256, "Export and Slice ");
                strcat_s(button_title, 256, curr_filename);
                SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)button_title);
                EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), TRUE);

                hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
                hMenu = GetSubMenu(hMenu, 9);
                insert_filename_to_MRU(hMenu, curr_filename);
                xform_list.head = NULL;
                xform_list.tail = NULL;
                gen_view_list_tree_volumes(&object_tree);
                populate_treeview();
            }

            break;

        case ID_FILE_SAVE:
            if (curr_filename[0] != '\0')
            {
                serialise_tree(&object_tree, curr_filename);
                drawing_changed = FALSE;
                break;
            }
            // If no filename, fall through to save as...
        case ID_FILE_SAVEAS:
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = auxGetHWND();
            ofn.lpstrFilter = "LoftyCAD Files (*.LCD)\0*.LCD\0All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "lcd";
            ofn.lpstrFile = curr_filename;
            ofn.nMaxFile = 256;
            ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
            if (GetSaveFileName(&ofn))
            {
                serialise_tree(&object_tree, curr_filename);
                drawing_changed = FALSE;
                strcpy_s(window_title, 256, curr_filename);
                strcat_s(window_title, 256, " - ");
                strcat_s(window_title, 256, object_tree.title);
                SetWindowText(auxGetHWND(), window_title);
                strcpy_s(button_title, 256, "Export and Slice ");
                strcat_s(button_title, 256, curr_filename);
                SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)button_title);
                EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), TRUE);
                hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
                hMenu = GetSubMenu(hMenu, 9);
                insert_filename_to_MRU(hMenu, curr_filename);
            }

            break;

        case ID_FILE_EXPORT:
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = auxGetHWND();
            ofn.lpstrFilter =
                "STL Meshes (*.STL)\0*.STL\0"
                "STL Meshes for each material (*_1.STL)\0*.STL\0"
                "AMF Files (*.AMF)\0*.AMF\0"
                "OBJ Files (*.OBJ)\0*.OBJ\0"
                "Geomview Object File Format Files (*.OFF)\0*.OFF\0"
                "All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "stl";
            strcpy_s(new_filename, 256, curr_filename);
            pdot = strrchr(new_filename, '.');
            if (pdot != NULL)
                *pdot = '\0';           // cut off ".lcd"
            ofn.lpstrFile = new_filename;
            ofn.nMaxFile = 256;
            ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
            if (GetSaveFileName(&ofn))
            {
                xform_list.head = NULL;
                xform_list.tail = NULL;
                gen_view_list_tree_volumes(&object_tree);
                if (!gen_view_list_tree_surfaces(&object_tree, &object_tree))
                    break;

                export_object_tree(&object_tree, new_filename, ofn.nFilterIndex);
            }

            break;

        case ID_FILE_IMPORTTOGROUP:
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = auxGetHWND();
            ofn.lpstrFilter = 
                "LoftyCAD Files (*.LCD)\0*.LCD\0"
                "STL Meshes (*.STL)\0*.STL\0"
                "AMF Files (*.AMF)\0*.AMF\0"
                "OBJ Files (*.OBJ)\0*.OBJ\0"
                "Geomview Object File Format Files (*.OFF)\0*.OFF\0"
                "G-code Files (*.GCODE)\0*.GCODE\0"
                "All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "lcd";
            new_filename[0] = '\0';
            ofn.lpstrFile = new_filename;
            ofn.nMaxFile = 256;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
            if (GetOpenFileName(&ofn))
            {
                Group *group = group_new();
                BOOL rc = FALSE;

                switch (ofn.nFilterIndex)
                {
                case 1:
                    rc = deserialise_tree(group, new_filename, TRUE);
                    break;
                case 2:
                    rc = read_stl_to_group(group, new_filename);
                    break;
                case 3:
                    rc = read_amf_to_group(group, new_filename);
                    break;
                case 4:
                    rc = read_obj_to_group(group, new_filename);
                    break;
                case 5:
                    rc = read_off_to_group(group, new_filename);
                    break;
                case 6:
                    // These don't go to the object tree, but to the gcode tree. Only one at a time.
                    purge_zpoly_edges(&gcode_tree);
                    rc = read_gcode_to_group(&gcode_tree, new_filename);
                    invalidate_dl();
                    SendMessage(hWndPropSheet, PSM_SETCURSEL, TAB_PREVIEW, 0);  // select print preview tab
                    SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FILENAME, WM_SETTEXT, 0, (LPARAM)new_filename);
                    SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FIL_USED, WM_SETTEXT, 0, (LPARAM)gcode_tree.fil_used);
                    SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_EST_PRINT, WM_SETTEXT, 0, (LPARAM)gcode_tree.est_print);
                    EnableWindow(GetDlgItem(hWndPrintPreview, IDB_PRINTER_PRINT), TRUE);
                    break;
                }
                if (ofn.nFilterIndex < 6)
                {
                    if (rc)
                    {
                        link_group((Object*)group, &object_tree);
                        update_drawing();
                    }
                    else
                    {
                        purge_obj((Object*)group);
                    }
                }
            }

            break;

        case ID_PREFERENCES_SETTINGS:
            display_help("Preferences");
            DialogBox(hInst, MAKEINTRESOURCE(IDD_PREFS), auxGetHWND(), prefs_dialog);
            strcpy_s(window_title, 256, curr_filename);
            strcat_s(window_title, 256, " - ");
            strcat_s(window_title, 256, object_tree.title);
            SetWindowText(auxGetHWND(), window_title);
            break;

        case ID_MRU_FILE1:
        case ID_MRU_FILE2:
        case ID_MRU_FILE3:
        case ID_MRU_FILE4:
            if (get_filename_from_MRU(LOWORD(wParam) - ID_MRU_BASE, new_filename))
            {
                if (drawing_changed)
                {
                    int rc = MessageBox(auxGetHWND(), "File modified. Save it?", curr_filename, MB_YESNOCANCEL | MB_ICONWARNING);

                    if (rc == IDCANCEL)
                        break;
                    else if (rc == IDYES)
                        serialise_tree(&object_tree, curr_filename);
                }

                clear_selection(&selection);
                purge_tree(&object_tree, clipboard.head != NULL, &saved_list);
                drawing_changed = FALSE;
                view_rendered = FALSE;
                enable_rendered_view_items();
                clean_checkpoints(curr_filename);
                curr_filename[0] = '\0';
                object_tree.title[0] = '\0';
                SetWindowText(auxGetHWND(), "LoftyCAD");
                SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)"Slice Current Model");
                EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), FALSE);

                if (!deserialise_tree(&object_tree, new_filename, FALSE))
                {
                    MessageBox(auxGetHWND(), "File not found. Removing from recently opened list.", new_filename, MB_OK | MB_ICONWARNING);
                    hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
                    hMenu = GetSubMenu(hMenu, 9);
                    remove_filename_from_MRU(hMenu, LOWORD(wParam) - ID_MRU_BASE);
                }
                else
                {
                    strcpy_s(curr_filename, 256, new_filename);
                    strcpy_s(window_title, 256, curr_filename);
                    strcat_s(window_title, 256, " - ");
                    strcat_s(window_title, 256, object_tree.title);
                    SetWindowText(auxGetHWND(), window_title);
                    strcpy_s(button_title, 256, "Export and Slice ");
                    strcat_s(button_title, 256, curr_filename);
                    SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)button_title);
                    EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), TRUE);
                    xform_list.head = NULL;
                    xform_list.tail = NULL;
                    gen_view_list_tree_volumes(&object_tree);
                    populate_treeview();
                }
            }
            break;

        case ID_EDIT_CUT:
            // You can't cut a component, only a top-level object (in the object tree).
            // Check that they are (all) in fact top-level before unlinking them.
            for (obj = selection.head; obj != NULL; obj = obj->next)
            {
                if (!is_top_level_object(obj->prev, &object_tree))
                    goto skip_cut;
            }

            for (obj = selection.head; obj != NULL; obj = obj->next)
                delink_group(obj->prev, &object_tree);

            clear_selection(&saved_list);
            clear_selection(&clipboard);
            clipboard = selection;
            selection.head = NULL;
            selection.tail = NULL;
            clip_xoffset = 0;
            clip_yoffset = 0;
            clip_zoffset = 0;
            update_drawing();
        skip_cut:
            break;

        case ID_EDIT_COPY:
            // As above, but don't unlink the objects. The clipboard is a reference to 
            // existing objects, not a copy of them. 
            // Excel does it like this, and it drives me nuts sometimes.
            clear_selection(&saved_list);
            clear_selection(&clipboard);
            clipboard = selection;
            selection.head = NULL;
            selection.tail = NULL;
            if (nz(facing_plane->A))
                clip_xoffset = 10;          // TODO - scale this so it is not too large when zoomed
            else
                clip_xoffset = 0;
            if (nz(facing_plane->B))
                clip_yoffset = 10;
            else
                clip_yoffset = 0;
            if (nz(facing_plane->C))
                clip_zoffset = 10;
            else
                clip_zoffset = 0;
            break;

        case ID_EDIT_PASTE:
            clear_selection(&selection);

            // Link the objects in the clipboard to the object tree. Do this by making a 
            // copy of them, as we might want to paste the same thing multiple times.
            // Each successive copy will go in at an increasing offset in the facing plane.
            // Don't offset if pasting from an old obj tree into a new one (at least the first time)
            if (saved_list.head != NULL)
            {
                clip_xoffset = 0;
                clip_yoffset = 0;
                clip_zoffset = 0;
            }

            for (obj = clipboard.head; obj != NULL; obj = obj->next)
            {
                Object * new_obj = copy_obj(obj->prev, clip_xoffset, clip_yoffset, clip_zoffset, FALSE);

                clear_move_copy_flags(obj->prev);
                link_tail_group(new_obj, &object_tree);
                link_single(new_obj, &selection);
            }

            if (nz(facing_plane->A))
                clip_xoffset += 10;
            if (nz(facing_plane->B))
                clip_yoffset += 10;
            if (nz(facing_plane->C))
                clip_zoffset += 10;

            update_drawing();
            break;

        case ID_EDIT_DELETE:
            // You can't delete a component, only a top-level object (in the object tree).
            // Check that they are in fact top-level before deleting them.
            for (obj = selection.head; obj != NULL; obj = obj->next)
            {
                if (is_top_level_object(obj->prev, &object_tree))
                {
                    delink_group(obj->prev, &object_tree);
                    purge_obj(obj->prev);
                }
            }
            clear_selection(&selection);
            if (object_tree.mesh != NULL)
                mesh_destroy(object_tree.mesh);
            object_tree.mesh = NULL;
            object_tree.mesh_valid = FALSE;
            update_drawing();
            break;

        case ID_EDIT_SELECTALL:
            // Put all top-level objects on the selection list.
            clear_selection(&selection);
            for (obj = object_tree.obj_list.head; obj != NULL; obj = obj->next)
                link_single(obj, &selection);
            update_drawing();
            break;

        case ID_EDIT_SELECTNONE:
            clear_selection(&selection);
            curr_path = NULL;
            update_drawing();
            break;

        case ID_EDIT_UNDO:
            generation--;
            read_checkpoint(&object_tree, curr_filename, generation);
            xform_list.head = NULL;
            xform_list.tail = NULL;
            gen_view_list_tree_volumes(&object_tree);
            populate_treeview();
            break;

        case ID_EDIT_REDO:
            generation++;
            read_checkpoint(&object_tree, curr_filename, generation);
            xform_list.head = NULL;
            xform_list.tail = NULL;
            gen_view_list_tree_volumes(&object_tree);
            populate_treeview();
            break;

        case ID_HELP_GETTINGSTARTED:
            display_help_window();
            display_help("Exploring");
            break;

        case ID_HELP_DRAWINGFACES:
            display_help_window();
            display_help("Drawing_Face");
            break;

        case ID_HELP_DRAWINGTEXT:
            display_help_window();
            display_help("Drawing_Text");
            break;

        case ID_HELP_EXTRUDING:
            display_help_window();
            display_help("Drawing_Extrude");
            break;

        case ID_HELP_LOCKINGANDGROUPING:
            display_help_window();
            display_help("Locking_Grouping");
            break;

        case ID_HELP_CSGOPERATIONS:
            display_help_window();
            display_help("CSG_Operations");
            break;

        case ID_HELP_IMPORTANDEXPORT:
            display_help_window();
            display_help("Import_Export");
            break;

        case ID_HELP_SCALING:
            display_help_window();
            display_help("Drawing_Scale");
            break;

        case ID_HELP_ROTATING:
            display_help_window();
            display_help("Drawing_Rotate");
            break;

        case ID_HELP_DIMENSIONS:
            display_help_window();
            display_help("Dimensions");
            break;

        case ID_MATERIALS_NEW:
            // Dialog box to edit or add a material
            if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_MATERIALS), auxGetHWND(), materials_dialog, 0) >= 0)
                update_drawing();
            break;

            // test here for a possibly variable number of ID_MATERIALS_BASE + n (for n in [0,MAX_MATERIAL])
            // don't use LOWORD as it's bigger than 65k
        default:
            if (wParam < ID_MATERIAL_BASE || wParam >= ID_MATERIAL_BASE + MAX_MATERIAL)
                break;

            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            hMenu = GetSubMenu(hMenu, 3);

            // Find which material index this menu item corresponds to. This number is at the
            // front of the menu string.
            GetMenuString(hMenu, wParam, buf, 64, 0);
            i = atoi(buf);
            if (materials[i].hidden)
            {
                materials[i].hidden = FALSE;
                CheckMenuItem(hMenu, wParam, MF_CHECKED);
            }
            else
            {
                materials[i].hidden = TRUE;
                CheckMenuItem(hMenu, wParam, MF_UNCHECKED);
            }

            update_drawing();

            // regenerate surface mesh, in case we're viewing rendered
            xform_list.head = NULL;
            xform_list.tail = NULL;
            if (object_tree.mesh != NULL)
                mesh_destroy(object_tree.mesh);
            object_tree.mesh = NULL;
            object_tree.mesh_valid = FALSE;
            gen_view_list_tree_surfaces(&object_tree, &object_tree);
            break;
        }
        break;

    case WM_INITMENUPOPUP:
        if ((HMENU)wParam == GetSubMenu(GetMenu(auxGetHWND()), 0))
        {
            // File menu
            EnableMenuItem((HMENU)wParam, ID_FILE_SAVE, drawing_changed ? MF_ENABLED : MF_GRAYED);
        }
        else if ((HMENU)wParam == GetSubMenu(GetMenu(auxGetHWND()), 1))
        {
            // Edit menu
            EnableMenuItem((HMENU)wParam, ID_EDIT_UNDO, generation > 0 ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem((HMENU)wParam, ID_EDIT_REDO, generation < latest_generation ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem((HMENU)wParam, ID_EDIT_CUT, selection.head != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem((HMENU)wParam, ID_EDIT_COPY, selection.head != NULL ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem((HMENU)wParam, ID_EDIT_DELETE, selection.head != NULL ? MF_ENABLED : MF_GRAYED);
        }
        else if ((HMENU)wParam == GetSubMenu(GetMenu(auxGetHWND()), 2))
        {
            // View menu
            EnableMenuItem((HMENU)wParam, ID_VIEW_CONSTRUCTIONEDGES, (view_rendered || view_printer) ? MF_GRAYED : MF_ENABLED);
            EnableMenuItem((HMENU)wParam, ID_VIEW_RENDEREDVIEW, view_printer ? MF_GRAYED : MF_ENABLED);

            hMenu = GetSubMenu((HMENU)wParam, 3);   // Materials pop-out
            load_materials_menu(hMenu, TRUE, 0);         // display materials menu with all check marks
        }

        break;

    }
    return 0;
}

