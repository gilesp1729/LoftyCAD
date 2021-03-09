#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <shellapi.h>

// Put the icon in the button (if not zero) and set up a tooltip for the button.
void
LoadAndDisplayIcon(HWND hWnd, int icon, int button, int toolstring)
{
    char buffer[256];
    HWND hWndButton = GetDlgItem(hWnd, button);
    HICON hIcon;

    if (icon != 0)
    {
        hIcon = LoadIcon(hInst, MAKEINTRESOURCE(icon));
        SendDlgItemMessage(hWnd, button, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);
    }

    // Create a tooltip.
    HWND hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
                                 WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                 hWndButton, NULL, hInst, NULL);

    SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessage(hwndTT, TTM_SETMAXTIPWIDTH, 0, 200);

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = hWndButton;
    ti.hinst = hInst;
    LoadString(hInst, toolstring, buffer, 256);
    ti.lpszText = buffer;

    GetClientRect(hWndButton, &ti.rect);

    // Associate the tooltip with the "tool" window.
    SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&ti);
}

// Hook proc for the ChooseFont dialog.
int WINAPI
font_hook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static Text *text;
    static CHOOSEFONT *cf;

    switch (msg)
    {
    case WM_INITDIALOG:
        cf = (CHOOSEFONT *)lParam;
        text = (Text *)cf->lCustData;
        SetDlgItemText(hWnd, IDC_FONT_STRING, text->string);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            GetDlgItemText(hWnd, IDC_FONT_STRING, text->string, 80);
            break;
        }

    }
    return 0;
}

// Window proc for the toolbar tab.
int WINAPI
toolbar_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    CHOOSEFONT cf;
    LOGFONT lf;
    PSHNOTIFY *notify;

    switch (msg)
    {
    case WM_INITDIALOG:
        hWndToolbar = hWnd;
        LoadAndDisplayIcon(hWnd, IDI_EDGE, IDB_EDGE, IDS_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_RECT, IDB_RECT, IDS_RECT);
        LoadAndDisplayIcon(hWnd, IDI_HEX, IDB_HEX, IDS_HEX);
        LoadAndDisplayIcon(hWnd, IDI_CIRCLE, IDB_CIRCLE, IDS_CIRCLE);
        LoadAndDisplayIcon(hWnd, IDI_CONST_EDGE, IDB_CONST_EDGE, IDS_CONST_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_CONST_RECT, IDB_CONST_RECT, IDS_CONST_RECT);
        LoadAndDisplayIcon(hWnd, IDI_CONST_HEX, IDB_CONST_HEX, IDS_CONST_HEX);
        LoadAndDisplayIcon(hWnd, IDI_CONST_CIRCLE, IDB_CONST_CIRCLE, IDS_CONST_CIRCLE);
        LoadAndDisplayIcon(hWnd, IDI_BEZIER_EDGE, IDB_BEZIER_EDGE, IDS_BEZIER_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_ARC_EDGE, IDB_ARC_EDGE, IDS_ARC_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_EXTRUDE, IDB_EXTRUDE, IDS_EXTRUDE);
        LoadAndDisplayIcon(hWnd, IDI_EXTRUDE_LOCAL, IDB_EXTRUDE_LOCAL, IDS_EXTRUDE_LOCAL);
        LoadAndDisplayIcon(hWnd, IDI_TEXT, IDB_TEXT, IDS_TEXT);
        LoadAndDisplayIcon(hWnd, IDI_SCALE, IDB_SCALE, IDS_SCALE);
        LoadAndDisplayIcon(hWnd, IDI_ROTATE, IDB_ROTATE, IDS_ROTATE);
        LoadAndDisplayIcon(hWnd, IDI_RENDERED, IDB_RENDERED, IDS_RENDERED);

        LoadAndDisplayIcon(hWnd, IDI_TOP, IDB_XY, IDS_XY);
        LoadAndDisplayIcon(hWnd, IDI_FRONT, IDB_YZ, IDS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_LEFT, IDB_XZ, IDS_XZ);
        LoadAndDisplayIcon(hWnd, IDI_BOTTOM, IDB_MINUS_XY, IDS_MINUS_XY);
        LoadAndDisplayIcon(hWnd, IDI_BACK, IDB_MINUS_YZ, IDS_MINUS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_RIGHT, IDB_MINUS_XZ, IDS_MINUS_XZ);

        // Tools are disabled when in render view
        enable_rendered_view_items();
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            construction = FALSE;
            switch (LOWORD(wParam))
            {
            case IDB_CONST_EDGE:
                construction = TRUE;
            case IDB_EDGE:
                change_state(STATE_STARTING_EDGE);
                break;

            case IDB_ARC_EDGE:
                change_state(STATE_STARTING_ARC);
                break;

            case IDB_BEZIER_EDGE:
                change_state(STATE_STARTING_BEZIER);
                break;

            case IDB_CONST_RECT:
            case IDB_CONST_HEX:
                construction = TRUE;
            case IDB_RECT:
            case IDB_HEX:
                change_state(STATE_STARTING_RECT);
                break;

            case IDB_CONST_CIRCLE:
                construction = TRUE;
            case IDB_CIRCLE:
                change_state(STATE_STARTING_CIRCLE);
                break;

            case IDB_EXTRUDE:
                change_state(STATE_STARTING_EXTRUDE);
                break;

            case IDB_EXTRUDE_LOCAL:
                change_state(STATE_STARTING_EXTRUDE_LOCAL);
                break;

            case IDB_TEXT:
                display_help_state(STATE_STARTING_TEXT);    // get the prompt up
                curr_text = calloc(1, sizeof(Text));
                // Choose font and input string here
                memset(&cf, 0, sizeof(CHOOSEFONT));
                cf.lStructSize = sizeof(CHOOSEFONT);
                cf.Flags = CF_NOSIZESEL | CF_TTONLY | CF_ENABLETEMPLATE | CF_ENABLEHOOK | CF_INITTOLOGFONTSTRUCT;
                cf.lpTemplateName = MAKEINTRESOURCE(1543);
                cf.lpfnHook = font_hook;
                cf.lCustData = (LPARAM)curr_text;
                memset(&lf, 0, sizeof(LOGFONT));
                cf.lpLogFont = &lf;
                if (!ChooseFont(&cf))
                {
                    free(curr_text);
                    display_help_state(STATE_NONE);
                    break;
                }

                curr_text->bold = lf.lfWeight > FW_NORMAL;
                curr_text->italic = lf.lfItalic;
                strcpy_s(curr_text->font, 32, lf.lfFaceName);
                // TODO check string and valid font here

                change_state(STATE_STARTING_TEXT);
                break;

            case IDB_SCALE:
                change_state(STATE_STARTING_SCALE);
                break;

            case IDB_ROTATE:
                change_state(STATE_STARTING_ROTATE);
                break;

            case IDB_XY:
                facing_plane = &plane_XY;
                facing_index = PLANE_XY;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane XY\r\n");
#endif
                trackball_InitQuat(quat_XY);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_YZ:
                facing_plane = &plane_YZ;
                facing_index = PLANE_YZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane YZ\r\n");
#endif
                trackball_InitQuat(quat_YZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_XZ:
                facing_plane = &plane_XZ;
                facing_index = PLANE_XZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane XZ\r\n");
#endif
                trackball_InitQuat(quat_XZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_XY:
                facing_plane = &plane_mXY;
                facing_index = PLANE_MINUS_XY;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -XY\r\n");
#endif
                trackball_InitQuat(quat_mXY);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_YZ:
                facing_plane = &plane_mYZ;
                facing_index = PLANE_MINUS_YZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -YZ\r\n");
#endif
                trackball_InitQuat(quat_mYZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_XZ:
                facing_plane = &plane_mXZ;
                facing_index = PLANE_MINUS_XZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -XZ\r\n");
#endif
                trackball_InitQuat(quat_mXZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_RENDERED:
                hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
                if (view_rendered)
                {
                    view_rendered = FALSE;
                    glEnable(GL_BLEND);
                    CheckMenuItem(hMenu, ID_VIEW_RENDEREDVIEW, MF_UNCHECKED);
                }
                else
                {
                    gen_view_list_tree_volumes(&object_tree);
                    if (!gen_view_list_tree_surfaces(&object_tree, &object_tree))
                        break;

                    view_rendered = TRUE;
                    glDisable(GL_BLEND);
                    CheckMenuItem(hMenu, ID_VIEW_RENDEREDVIEW, MF_CHECKED);
                }
                enable_rendered_view_items();
                invalidate_dl();
                break;
            }
        }

        break;

    case WM_NOTIFY:
        notify = (PSHNOTIFY*)lParam;
        switch (notify->hdr.code)
        {
        case PSN_SETACTIVE:
            view_printer = FALSE;
            glEnable(GL_BLEND);
            invalidate_dl();
            break;

        case PSN_RESET:
            view_tools = FALSE;
            ShowWindow(hWndPropSheet, SW_HIDE);
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
            CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        break;
    }

    return 0;
}

// Window proc for the slicer tab.
int WINAPI
slicer_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    static HFONT hFontStd, hFontBold;
    LOGFONT lf;
    PSHNOTIFY* notify;
    char printer[64], print[64], filament[64];
    char cmd[1024], filename[MAX_PATH], dir[MAX_PATH], gcode_filename[MAX_PATH], button_title[256];
    OPENFILENAME ofn;
    int indx, len;
    char* slosh, *pdot;
    char inifile[MAX_PATH];
    char buf[16];
    char option[80];
    static BOOL text_changed = FALSE;
    static BOOL first_focus = FALSE;

    switch (msg)
    {
    case WM_INITDIALOG:
        hWndSlicer = hWnd;
        read_slic3r_config("printer", IDC_SLICER_PRINTER, printer);
        read_slic3r_config("print", IDC_SLICER_PRINTSETTINGS, printer);
        read_slic3r_config("filament", IDC_SLICER_FILAMENT, printer);

        indx = SendDlgItemMessage(hWnd, IDC_SLICER_PRINTSETTINGS, CB_GETCURSEL, 0, 0);
        SendDlgItemMessage(hWnd, IDC_SLICER_PRINTSETTINGS, CB_GETLBTEXT, indx, (LPARAM)print);
        read_string_and_load_dlgitem(print, "support_material", IDB_SLICER_SUPPORT, TRUE);
        read_string_and_load_dlgitem(print, "layer_height", IDC_SLICER_LAYERHEIGHT, FALSE);
        read_string_and_load_dlgitem(print, "perimeters", IDC_SLICER_WALLTHICK, FALSE);
        read_string_and_load_dlgitem(print, "top_solid_layers", IDC_SLICER_ROOFTHICK, FALSE);
        read_string_and_load_dlgitem(print, "bottom_solid_layers", IDC_SLICER_FLOORTHICK, FALSE);
        read_string_and_load_dlgitem(print, "fill_density", IDC_SLICER_INFILL, FALSE);

        // Get the font used by the edit boes, and make a bold version of it
        hFontStd = (HFONT)SendDlgItemMessage(hWnd, IDC_SLICER_LAYERHEIGHT, WM_GETFONT, 0, 0);
        GetObject(hFontStd, sizeof(LOGFONT), &lf);
        lf.lfWeight = FW_BOLD;
        hFontBold = CreateFontIndirect(&lf);

        if (curr_filename[0] == '\0')
        {
            SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)"Slice Current Model");
            EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), FALSE);
        }
        else
        {
            strcpy_s(button_title, 256, "Export and Slice ");
            strcat_s(button_title, 256, curr_filename);
            SendDlgItemMessage(hWndSlicer, IDB_SLICER_SLICE, WM_SETTEXT, 0, (LPARAM)button_title);
            EnableWindow(GetDlgItem(hWndSlicer, IDB_SLICER_SLICE), TRUE);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SLICER_PRINTER:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                indx = SendDlgItemMessage(hWnd, IDC_SLICER_PRINTER, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hWnd, IDC_SLICER_PRINTER, CB_GETLBTEXT, indx, (LPARAM)printer);
                read_slic3r_config("print", IDC_SLICER_PRINTSETTINGS, printer);
                read_slic3r_config("filament", IDC_SLICER_FILAMENT, printer);

                // Pull out bed shape from cached printer section
                set_bed_shape(printer);
                break;
            }
            break;

        case IDC_SLICER_PRINTSETTINGS:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:
                indx = SendDlgItemMessage(hWnd, IDC_SLICER_PRINTSETTINGS, CB_GETCURSEL, 0, 0);
                SendDlgItemMessage(hWnd, IDC_SLICER_PRINTSETTINGS, CB_GETLBTEXT, indx, (LPARAM)print);
                read_string_and_load_dlgitem(print, "support_material", IDB_SLICER_SUPPORT, TRUE);
                read_string_and_load_dlgitem(print, "layer_height", IDC_SLICER_LAYERHEIGHT, FALSE);
                read_string_and_load_dlgitem(print, "perimeters", IDC_SLICER_WALLTHICK, FALSE);
                read_string_and_load_dlgitem(print, "top_solid_layers", IDC_SLICER_ROOFTHICK, FALSE);
                read_string_and_load_dlgitem(print, "bottom_solid_layers", IDC_SLICER_FLOORTHICK, FALSE);
                read_string_and_load_dlgitem(print, "fill_density", IDC_SLICER_INFILL, FALSE);
                SendDlgItemMessage(hWnd, IDC_SLICER_LAYERHEIGHT, WM_SETFONT, (WPARAM)hFontStd, TRUE);
                SendDlgItemMessage(hWnd, IDC_SLICER_WALLTHICK, WM_SETFONT, (WPARAM)hFontStd, TRUE);
                SendDlgItemMessage(hWnd, IDC_SLICER_ROOFTHICK, WM_SETFONT, (WPARAM)hFontStd, TRUE);
                SendDlgItemMessage(hWnd, IDC_SLICER_FLOORTHICK, WM_SETFONT, (WPARAM)hFontStd, TRUE);
                SendDlgItemMessage(hWnd, IDC_SLICER_INFILL, WM_SETFONT, (WPARAM)hFontStd, TRUE);
                text_changed = FALSE;
                break;
            }
            break;

        case IDB_SLICER_SUPPORT:
            switch (HIWORD(wParam))
            {
            case BN_CLICKED:
                SetWindowLong(GetDlgItem(hWndSlicer, LOWORD(wParam)), GWL_USERDATA, 1);
                break;
            }
            break;

        case IDC_SLICER_LAYERHEIGHT:
        case IDC_SLICER_WALLTHICK:
        case IDC_SLICER_ROOFTHICK:
        case IDC_SLICER_FLOORTHICK:
        case IDC_SLICER_INFILL:
            // Set changed overrides to bold face
            switch (HIWORD(wParam))
            {
            case EN_SETFOCUS:
                // Catch the first time it gets the focus, so we don't count
                // the EN_CHANGE when the edit box is first written to
                first_focus = TRUE;
                break;

            case EN_CHANGE:
                text_changed = first_focus;
                break;

            case EN_KILLFOCUS:
                if (text_changed)
                {
                    SendDlgItemMessage(hWnd, LOWORD(wParam), WM_SETFONT, (WPARAM)hFontBold, TRUE);
                    SetWindowLong(GetDlgItem(hWndSlicer, LOWORD(wParam)), GWL_USERDATA, 1);
                }
                text_changed = FALSE;
            }
            break;

        case IDB_SLICER_GUI:
            ShellExecute(hWndSlicer, "open", slicer_exe[slicer_index].exe, NULL, NULL, SW_SHOW);
            break;

        case IDB_SLICER_SLICE_EXISTING:
            // Open an existing triangle mesh (STL, AMF, ...) and slice it
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = auxGetHWND();
            ofn.lpstrFilter =
                "STL Meshes (*.STL)\0*.STL\0"
                "AMF Files (*.AMF)\0*.AMF\0"
                "OBJ Files (*.OBJ)\0*.OBJ\0"
                "All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "lcd";
            filename[0] = '\0';
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
            if (!GetOpenFileName(&ofn))
                break;

            goto slice_it;

        case IDB_SLICER_SLICE:
            // Export the object tree as STL (note: this may change with multi-materials)
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = auxGetHWND();
            ofn.lpstrFilter =
                "STL Meshes (*.STL)\0*.STL\0"
                "STL Meshes for each material (*_1.STL)\0*.STL\0"
                "All Files\0*.*\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrDefExt = "stl";
            strcpy_s(filename, MAX_PATH, curr_filename);
            pdot = strrchr(filename, '.');
            if (pdot != NULL)
                *pdot = '\0';           // cut off any ".lcd"
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
            if (!GetSaveFileName(&ofn))
                break;

            // Export the model
            gen_view_list_tree_volumes(&object_tree);
            if (!gen_view_list_tree_surfaces(&object_tree, &object_tree))
                break;
            export_object_tree(&object_tree, filename, ofn.nFilterIndex);

        slice_it:
            // Get the directory with its trailing '\\'
            strcpy_s(dir, MAX_PATH, filename);
            slosh = strrchr(dir, '\\');
            *(slosh + 1) = '\0';

            // Open the ini files for writing and fill them from the selected presets.
            // Separate ini files are used as there may be duplicates between sections
            strcpy_s(inifile, MAX_PATH, dir);
            strcat_s(inifile, MAX_PATH, "slicer_settings_printer.ini");
            indx = SendDlgItemMessage(hWnd, IDC_SLICER_PRINTER, CB_GETCURSEL, 0, 0);
            SendDlgItemMessage(hWnd, IDC_SLICER_PRINTER, CB_GETLBTEXT, indx, (LPARAM)printer);
            get_slic3r_config_section("printer", printer, inifile);
            
            // Add load options to slic3r cmd line for settings
            strcpy_s(cmd, 1024, " --load ");
            strcat_s(cmd, 1024, inifile);

            strcpy_s(inifile, MAX_PATH, dir);
            strcat_s(inifile, MAX_PATH, "slicer_settings_print.ini");
            indx = SendDlgItemMessage(hWnd, IDC_SLICER_PRINTSETTINGS, CB_GETCURSEL, 0, 0);
            SendDlgItemMessage(hWnd, IDC_SLICER_PRINTSETTINGS, CB_GETLBTEXT, indx, (LPARAM)print);
            get_slic3r_config_section("print", print, inifile);
            strcat_s(cmd, 1024, " --load ");
            strcat_s(cmd, 1024, inifile);

            strcpy_s(inifile, MAX_PATH, dir);
            strcat_s(inifile, MAX_PATH, "slicer_settings_filament.ini");
            indx = SendDlgItemMessage(hWnd, IDC_SLICER_FILAMENT, CB_GETCURSEL, 0, 0);
            SendDlgItemMessage(hWnd, IDC_SLICER_FILAMENT, CB_GETLBTEXT, indx, (LPARAM)filament);
            get_slic3r_config_section("filament", filament, inifile);
            strcat_s(cmd, 1024, " --load ");
            strcat_s(cmd, 1024, inifile);

            // Build slic3r options. Centre model on the current print bed dimensions.
            sprintf_s(option, 80, slicer_cmd[slicer_config[config_index].type],
                (int)(bed_xmax - bed_xmin) / 2,
                (int)(bed_ymax - bed_ymin) / 2);
            strcat_s(cmd, 1024, option);

            // Put in the output file option if we are specifying it explicitly.
            // (otherwise it will be generated according to output_filename_format,
            // but beware of PrusaSlicer issue #4872 messing this up)
            if (explicit_gcode)
            {
                memset(&ofn, 0, sizeof(OPENFILENAME));
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = auxGetHWND();
                ofn.lpstrFilter =
                    "G-code (*.GCODE)\0*.GCODE\0"
                    "All Files\0*.*\0\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrDefExt = "gcode";
                strcpy_s(gcode_filename, MAX_PATH, filename);
                pdot = strrchr(gcode_filename, '.');
                if (pdot != NULL)
                    *pdot = '\0';           // cut off any ".stl" and add extra stuff
                ofn.lpstrFile = gcode_filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
                if (!GetSaveFileName(&ofn))
                    break;

                sprintf_s(option, 80, " -o %s ", gcode_filename);
                strcat_s(cmd, 1024, option);
            }

            // Put in any changed overrrides as options. Apart from the infill density (below)
            // NO checking is done.
            if (GetWindowLong(GetDlgItem(hWnd, IDB_SLICER_SUPPORT), GWL_USERDATA) != 0)
            {
                if (IsDlgButtonChecked(hWnd, IDB_SLICER_SUPPORT))
                    strcat_s(cmd, 1024, " --support-material ");
                else
                    strcat_s(cmd, 1024, " --no-support-material ");
            }
            if (GetWindowLong(GetDlgItem(hWnd, IDC_SLICER_LAYERHEIGHT), GWL_USERDATA) != 0)
            {
                SendDlgItemMessage(hWnd, IDC_SLICER_LAYERHEIGHT, WM_GETTEXT, 16, (LPARAM)buf);
                sprintf_s(option, 80, " --layer-height %s ", buf);
                strcat_s(cmd, 1024, option);
            }
            if (GetWindowLong(GetDlgItem(hWnd, IDC_SLICER_WALLTHICK), GWL_USERDATA) != 0)
            {
                SendDlgItemMessage(hWnd, IDC_SLICER_WALLTHICK, WM_GETTEXT, 16, (LPARAM)buf);
                sprintf_s(option, 80, " --perimeters %s ", buf);
                strcat_s(cmd, 1024, option);
            }
            if (GetWindowLong(GetDlgItem(hWnd, IDC_SLICER_ROOFTHICK), GWL_USERDATA) != 0)
            {
                SendDlgItemMessage(hWnd, IDC_SLICER_ROOFTHICK, WM_GETTEXT, 16, (LPARAM)buf);
                sprintf_s(option, 80, " --top-solid-layers %s ", buf);
                strcat_s(cmd, 1024, option);
            }
            if (GetWindowLong(GetDlgItem(hWnd, IDC_SLICER_FLOORTHICK), GWL_USERDATA) != 0)
            {
                SendDlgItemMessage(hWnd, IDC_SLICER_FLOORTHICK, WM_GETTEXT, 16, (LPARAM)buf);
                sprintf_s(option, 80, " --bottom-solid-layers %s ", buf);
                strcat_s(cmd, 1024, option);
            }
            if (GetWindowLong(GetDlgItem(hWnd, IDC_SLICER_INFILL), GWL_USERDATA) != 0)
            {
                SendDlgItemMessage(hWnd, IDC_SLICER_INFILL, WM_GETTEXT, 16, (LPARAM)buf);

                // Make sure the "%" is present, otherwise PS runs away in an infinite loop!
                // Raised issue #4887 on PS.
                len = strlen(buf);
                if (buf[len - 1] != '%')
                    strcat_s(buf, 16, "%");

                sprintf_s(option, 80, " --fill-density %s ", buf);
                strcat_s(cmd, 1024, option);
            }

            // Add the input file and run it
            strcat_s(cmd, 1024, filename);
            run_slicer(slicer_exe[slicer_index].exe, cmd, dir, gcode_filename);

            break;
        }
        break;

    case WM_NOTIFY:
        notify = (PSHNOTIFY*)lParam;
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        switch (notify->hdr.code)
        {
        case PSN_SETACTIVE:
            break;

        case PSN_RESET:
            view_tools = FALSE;
            ShowWindow(hWndPropSheet, SW_HIDE);
            CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        break;
    }

    return 0;
}

// Window proc for the print preview tab.
int WINAPI
preview_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    PSHNOTIFY* notify;
    char buf[16], print_button[128], gcode_filename[MAX_PATH];

    switch (msg)
    {
    case WM_INITDIALOG:
        hWndPrintPreview = hWnd;
        LoadAndDisplayIcon(hWnd, IDI_TOP, IDB_XY, IDS_XY);
        LoadAndDisplayIcon(hWnd, IDI_FRONT, IDB_YZ, IDS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_LEFT, IDB_XZ, IDS_XZ);
        LoadAndDisplayIcon(hWnd, IDI_BOTTOM, IDB_MINUS_XY, IDS_MINUS_XY);
        LoadAndDisplayIcon(hWnd, IDI_BACK, IDB_MINUS_YZ, IDS_MINUS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_RIGHT, IDB_MINUS_XZ, IDS_MINUS_XZ);

        sprintf_s(buf, 16, "%.2f", print_zmin);
        SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", print_zmax);
        SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_SETTEXT, 0, (LPARAM)buf);

        if (print_zmin <= 0 && print_zmax >= 9999)
        {
            EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZFROM), FALSE);
            EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZTO), FALSE);
            SendDlgItemMessage(hWnd, IDC_PRINTER_FULL, BM_CLICK, 0, 0);
        }
        else if (print_zmin == print_zmax)
        {
            EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZFROM), TRUE);
            EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZTO), FALSE);
            SendDlgItemMessage(hWnd, IDC_PRINTER_LAYER, BM_CLICK, 0, 0);
        }
        else
        {
            EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZFROM), TRUE);
            EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZTO), TRUE);
            SendDlgItemMessage(hWnd, IDC_PRINTER_UPTO, BM_CLICK, 0, 0);
        }

        if (print_octo)
            sprintf_s(print_button, 128, "Upload G-code to %s", octoprint_server);
        else
            sprintf_s(print_button, 128, "Print G-code to %s", printer_port);
        SendDlgItemMessage(hWndPrintPreview, IDB_PRINTER_PRINT, WM_SETTEXT, 0, (LPARAM)print_button);
        EnableWindow(GetDlgItem(hWnd, IDB_PRINTER_PRINT), FALSE);
        SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FILENAME, WM_SETTEXT, 0, (LPARAM)"");
        SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_FIL_USED, WM_SETTEXT, 0, (LPARAM)"");
        SendDlgItemMessage(hWndPrintPreview, IDC_PRINT_EST_PRINT, WM_SETTEXT, 0, (LPARAM)"");
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_PRINTER_FULL:
                EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZFROM), FALSE);
                EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZTO), FALSE);
                print_zmin = 0;
                print_zmax = 9999;
                invalidate_dl();
                break;

            case IDC_PRINTER_LAYER:
                EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZFROM), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZTO), FALSE);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmin = print_zmax = (float)atof(buf);
                invalidate_dl();
                break;

            case IDC_PRINTER_UPTO:
                EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZFROM), TRUE);
                EnableWindow(GetDlgItem(hWnd, IDC_PRINTER_ZTO), TRUE);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmin = (float)atof(buf);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmax = (float)atof(buf);
                invalidate_dl();
                break;

            case IDB_XY:
                facing_plane = &plane_XY;
                facing_index = PLANE_XY;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane XY\r\n");
#endif
                trackball_InitQuat(quat_XY);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_YZ:
                facing_plane = &plane_YZ;
                facing_index = PLANE_YZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane YZ\r\n");
#endif
                trackball_InitQuat(quat_YZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_XZ:
                facing_plane = &plane_XZ;
                facing_index = PLANE_XZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane XZ\r\n");
#endif
                trackball_InitQuat(quat_XZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_XY:
                facing_plane = &plane_mXY;
                facing_index = PLANE_MINUS_XY;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -XY\r\n");
#endif
                trackball_InitQuat(quat_mXY);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_YZ:
                facing_plane = &plane_mYZ;
                facing_index = PLANE_MINUS_YZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -YZ\r\n");
#endif
                trackball_InitQuat(quat_mYZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_XZ:
                facing_plane = &plane_mXZ;
                facing_index = PLANE_MINUS_XZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -XZ\r\n");
#endif
                trackball_InitQuat(quat_mXZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_PRINTER_PRINT:
                if (SendDlgItemMessage(hWnd, IDC_PRINT_FILENAME, WM_GETTEXT, MAX_PATH, (LPARAM)gcode_filename) == 0)
                    break;
                if (print_octo)
                    send_to_octoprint(gcode_filename, "local");
                else
                    send_to_serial(gcode_filename);

                break;
            }
        }
        else if (HIWORD(wParam) == EN_KILLFOCUS)
        {
            if (LOWORD(wParam) == IDC_PRINTER_ZFROM)
            {
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmin = (float)atof(buf);
                if (print_zmax < print_zmin)
                {
                    print_zmax = print_zmin;
                    sprintf_s(buf, 16, "%.2f", print_zmax);
                    SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_SETTEXT, 0, (LPARAM)buf);
                }
                invalidate_dl();
            }
            else if (LOWORD(wParam) == IDC_PRINTER_ZTO)
            {
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmax = (float)atof(buf);
                if (print_zmin > print_zmax)
                {
                    print_zmin = print_zmax;
                    sprintf_s(buf, 16, "%.2f", print_zmin);
                    SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_SETTEXT, 0, (LPARAM)buf);
                }
                invalidate_dl();
            }
        }
        break;

    case WM_NOTIFY:
        notify = (PSHNOTIFY*)lParam;
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        switch (notify->hdr.code)
        {
        case PSN_SETACTIVE:
            view_printer = TRUE;
            view_rendered = FALSE;
            glDisable(GL_BLEND);
            invalidate_dl();
            view_printbed = TRUE;
            CheckMenuItem(hMenu, ID_VIEW_PRINTBED, MF_CHECKED);
            break;

        case PSN_RESET:
            view_tools = FALSE;
            ShowWindow(hWndPropSheet, SW_HIDE);
            CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        break;
    }

    return 0;
}

// Window proc for the printer tab.
int WINAPI
printer_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    PSHNOTIFY* notify;

    switch (msg)
    {
    case WM_INITDIALOG:
        hWndPrinter = hWnd;
        LoadAndDisplayIcon(hWnd, IDI_TOP, IDB_XY, IDS_XY);
        LoadAndDisplayIcon(hWnd, IDI_FRONT, IDB_YZ, IDS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_LEFT, IDB_XZ, IDS_XZ);
        LoadAndDisplayIcon(hWnd, IDI_BOTTOM, IDB_MINUS_XY, IDS_MINUS_XY);
        LoadAndDisplayIcon(hWnd, IDI_BACK, IDB_MINUS_YZ, IDS_MINUS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_RIGHT, IDB_MINUS_XZ, IDS_MINUS_XZ);
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            //case IDC_PRINTER_FULL:
            //    break;

            case IDB_XY:
                facing_plane = &plane_XY;
                facing_index = PLANE_XY;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane XY\r\n");
#endif
                trackball_InitQuat(quat_XY);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_YZ:
                facing_plane = &plane_YZ;
                facing_index = PLANE_YZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane YZ\r\n");
#endif
                trackball_InitQuat(quat_YZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_XZ:
                facing_plane = &plane_XZ;
                facing_index = PLANE_XZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane XZ\r\n");
#endif
                trackball_InitQuat(quat_XZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_XY:
                facing_plane = &plane_mXY;
                facing_index = PLANE_MINUS_XY;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -XY\r\n");
#endif
                trackball_InitQuat(quat_mXY);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_YZ:
                facing_plane = &plane_mYZ;
                facing_index = PLANE_MINUS_YZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -YZ\r\n");
#endif
                trackball_InitQuat(quat_mYZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;

            case IDB_MINUS_XZ:
                facing_plane = &plane_mXZ;
                facing_index = PLANE_MINUS_XZ;
#ifdef DEBUG_TOOLBAR_FACING
                Log("Facing plane -XZ\r\n");
#endif
                trackball_InitQuat(quat_mXZ);
                SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
                break;
            }
        }
        else if (HIWORD(wParam) == EN_KILLFOCUS)
        {
        //    if (LOWORD(wParam) == IDC_PRINTER_ZFROM)
        //    {
        //    }
        }
        break;

    case WM_NOTIFY:
        notify = (PSHNOTIFY*)lParam;
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        switch (notify->hdr.code)
        {
        case PSN_SETACTIVE:
            view_printer = TRUE;
            view_rendered = FALSE;
            glDisable(GL_BLEND);
            invalidate_dl();
            view_printbed = TRUE;
            CheckMenuItem(hMenu, ID_VIEW_PRINTBED, MF_CHECKED);
            break;

        case PSN_RESET:
            view_tools = FALSE;
            ShowWindow(hWndPropSheet, SW_HIDE);
            CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        break;
    }

    return 0;
}

// Wndproc for debug log dialog. Contains one large edit box.
int WINAPI
debug_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;

    switch (msg)
    {
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDB_CLEARDEBUG:
                SendDlgItemMessage(hWnd, IDC_DEBUG, EM_SETSEL, 0, -1);
                SendDlgItemMessage(hWnd, IDC_DEBUG, EM_REPLACESEL, 0, (LPARAM)"");
                break;
            }
        }

        break;

    case WM_CLOSE:
        view_debug = FALSE;
        ShowWindow(hWnd, SW_HIDE);
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_DEBUGLOG, view_debug ? MF_CHECKED : MF_UNCHECKED);
        break;
    }

    return 0;
}

