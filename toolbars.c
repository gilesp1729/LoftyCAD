#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>

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
        LoadAndDisplayIcon(hWnd, IDI_CIRCLE, IDB_CIRCLE, IDS_CIRCLE);
        LoadAndDisplayIcon(hWnd, IDI_CONST_EDGE, IDB_CONST_EDGE, IDS_CONST_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_CONST_RECT, IDB_CONST_RECT, IDS_CONST_RECT);
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
                construction = TRUE;
            case IDB_RECT:
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

// Window proc for the printer tab.
int WINAPI
printer_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    PSHNOTIFY* notify;
    char buf[16];

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
            SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_ENABLE, 0, 0);
            SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_ENABLE, 0, 0);
            SendDlgItemMessage(hWnd, IDC_PRINTER_FULL, BM_CLICK, 0, 0);
        }
        else if (print_zmin == print_zmax)
        {
            SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_ENABLE, 1, 0);
            SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_ENABLE, 0, 0);
            SendDlgItemMessage(hWnd, IDC_PRINTER_LAYER, BM_CLICK, 0, 0);
        }
        else
        {
            SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_ENABLE, 1, 0);
            SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_ENABLE, 1, 0);
            SendDlgItemMessage(hWnd, IDC_PRINTER_UPTO, BM_CLICK, 0, 0);
        }
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_PRINTER_FULL:
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_ENABLE, 0, 0);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_ENABLE, 0, 0);
                print_zmin = 0;
                print_zmax = 9999;
                invalidate_dl();
                break;

            case IDC_PRINTER_LAYER:
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_ENABLE, 1, 0);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_ENABLE, 0, 0);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmin = print_zmax = (float)atof(buf);
                invalidate_dl();
                break;

            case IDC_PRINTER_UPTO:
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_ENABLE, 1, 0);
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_ENABLE, 1, 0);
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
            }
        }
        else if (HIWORD(wParam) == EN_KILLFOCUS)
        {
            if (LOWORD(wParam) == IDC_PRINTER_ZFROM)
            {
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZFROM, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmin = (float)atof(buf);
                invalidate_dl();
            }
            else if (LOWORD(wParam) == IDC_PRINTER_ZTO)
            {
                SendDlgItemMessage(hWnd, IDC_PRINTER_ZTO, WM_GETTEXT, 16, (LPARAM)buf);
                print_zmax = (float)atof(buf);
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

// Window proc for the slicer tab.
int WINAPI
slicer_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    PSHNOTIFY* notify;
    //char buf[16];

    switch (msg)
    {
    case WM_INITDIALOG:
        hWndSlicer = hWnd;


        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {


        }
        else if (HIWORD(wParam) == EN_KILLFOCUS)
        {

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

