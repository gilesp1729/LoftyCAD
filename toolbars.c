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

// Window proc for the toolbar dialog box.
int WINAPI
toolbar_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;

    switch (msg)
    {
    case WM_INITDIALOG:
        LoadAndDisplayIcon(hWnd, IDI_POINT, IDB_POINT, IDS_POINT);
        LoadAndDisplayIcon(hWnd, IDI_EDGE, IDB_EDGE, IDS_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_RECT, IDB_RECT, IDS_RECT);
        LoadAndDisplayIcon(hWnd, IDI_CIRCLE, IDB_CIRCLE, IDS_CIRCLE);
        LoadAndDisplayIcon(hWnd, IDI_CONST_EDGE, IDB_CONST_EDGE, IDS_CONST_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_CONST_RECT, IDB_CONST_RECT, IDS_CONST_RECT);
        LoadAndDisplayIcon(hWnd, IDI_CONST_CIRCLE, IDB_CONST_CIRCLE, IDS_CONST_CIRCLE);
        LoadAndDisplayIcon(hWnd, IDI_BEZIER_EDGE, IDB_BEZIER_EDGE, IDS_BEZIER_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_ARC_EDGE, IDB_ARC_EDGE, IDS_ARC_EDGE);
        LoadAndDisplayIcon(hWnd, IDI_EXTRUDE, IDB_EXTRUDE, IDS_EXTRUDE);
        LoadAndDisplayIcon(hWnd, IDI_TOP, IDB_XY, IDS_XY);
        LoadAndDisplayIcon(hWnd, IDI_FRONT, IDB_YZ, IDS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_LEFT, IDB_XZ, IDS_XZ);
        LoadAndDisplayIcon(hWnd, IDI_BOTTOM, IDB_MINUS_XY, IDS_MINUS_XY);
        LoadAndDisplayIcon(hWnd, IDI_BACK, IDB_MINUS_YZ, IDS_MINUS_YZ);
        LoadAndDisplayIcon(hWnd, IDI_RIGHT, IDB_MINUS_XZ, IDS_MINUS_XZ);
        LoadAndDisplayIcon(hWnd, IDI_RENDERED, IDB_RENDERED, IDS_RENDERED);

        // Tools are disabled when in render view
        EnableWindow(GetDlgItem(hWnd, IDB_EDGE), !view_rendered);
        EnableWindow(GetDlgItem(hWnd, IDB_RECT), !view_rendered);
        EnableWindow(GetDlgItem(hWnd, IDB_CIRCLE), !view_rendered);
        EnableWindow(GetDlgItem(hWnd, IDB_ARC_EDGE), !view_rendered);
        EnableWindow(GetDlgItem(hWnd, IDB_BEZIER_EDGE), !view_rendered);
        EnableWindow(GetDlgItem(hWnd, IDB_EXTRUDE), !view_rendered);

        // For now grey out unimplemented ones
        EnableWindow(GetDlgItem(hWnd, IDB_POINT), FALSE);
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
                    xform_list.head = NULL;
                    xform_list.tail = NULL;
                    gen_view_list_tree_volumes(&object_tree);
                    gen_view_list_tree_surfaces(&object_tree, &object_tree);
                    view_rendered = TRUE;
                    glDisable(GL_BLEND);
                    CheckMenuItem(hMenu, ID_VIEW_RENDEREDVIEW, MF_CHECKED);
                }
                EnableWindow(GetDlgItem(hWndToolbar, IDB_EDGE), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_RECT), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_CIRCLE), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_ARC_EDGE), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_BEZIER_EDGE), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_EXTRUDE), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_EDGE), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_RECT), !view_rendered);
                EnableWindow(GetDlgItem(hWndToolbar, IDB_CONST_CIRCLE), !view_rendered);
                break;
            }
        }

        break;

    case WM_CLOSE:
        view_tools = FALSE;
        ShowWindow(hWnd, SW_HIDE);
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
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

