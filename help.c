#include "htmlbrowser.h"
#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Directory containing help pages
char help_dir[256];

// These are indexed by the app state (LoftyCAD.h)
char *state_key[STATE_MAX] =
{
    "Exploring",
    "Moving",
    "Dragging_Selection",

    "Drawing_Edge",
    "Drawing_Face",
    "Drawing_Face",
    "Drawing_Face",
    "Drawing_Edge",
    "Drawing_Edge",
    "Drawing_Extrude",
    "Drawing_Text",
    "Drawing_Extrude",
    "Drawing_Scale",
    "Drawing_Rotate",

    "Drawing_Edge",
    "Drawing_Face",
    "Drawing_Face",
    "Drawing_Face",
    "Drawing_Edge",
    "Drawing_Edge",
    "Drawing_Extrude",
    "Drawing_Text",
    "Drawing_Extrude",
    "Drawing_Scale",
    "Drawing_Rotate"
};

struct
{
    HINSTANCE   instance;
    LPSTR       idc;
    HCURSOR     cursor;
} cursors[STATE_MAX] =
{
    { NULL, IDC_ARROW, NULL },
    { NULL, IDC_ARROW, NULL },
    { NULL, IDC_ARROW, NULL },

    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_EDGE1), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE1), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE6), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE2), NULL }, 
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_EDGE3), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_EDGE2), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE3), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE4), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE5), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_RESIZE1), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_RESIZE2), NULL },

    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_EDGE1), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE1), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE6), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE2), NULL },  
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_EDGE3), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_EDGE2), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE3), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE4), NULL }, 
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_FACE5), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_RESIZE1), NULL },
    { (HINSTANCE)1, MAKEINTRESOURCE(IDC_RESIZE2), NULL },
};

HWND init_help_window(void)
{
    HWND hWnd;
    int i;
    char *slosh;

    OleInitialize(NULL);
    hWnd = CreateDialog
        (
        hInst,
        MAKEINTRESOURCE(IDD_HELP),
        auxGetHWND(),
        help_dialog
        );

    SetWindowPos(hWnd, HWND_NOTOPMOST, wWidth, toolbar_bottom, 0, 0, SWP_NOSIZE);
    if (view_help)
        ShowWindow(hWnd, SW_SHOW);

    // Load the cursors (both homemade and system-supplied ones)
    for (i = 0; i < STATE_MAX; i++)
    {
        if (cursors[i].instance != NULL)
            cursors[i].instance = hInst;
        cursors[i].cursor = LoadCursor(cursors[i].instance, (LPCTSTR)cursors[i].idc);
    }

    // Find the directory containing the help files
    GetModuleFileName(NULL, help_dir, 256);
    slosh = strrchr(help_dir, '\\');
    if (slosh != NULL)
        *slosh = '\0';

    // If we're running from a Debug directory, go up one level
    if (_stricmp(slosh - 6, "\\Debug") == 0)
        *(slosh - 6) = '\0';

    strcat_s(help_dir, 256, "\\html\\");

    return hWnd;
}

// Show the help window if it currently isn't shown.
void 
display_help_window()
{
    HMENU hMenu;

    if (!view_help)
    {
        ShowWindow(hWndHelp, SW_SHOW);
        view_help = TRUE;
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_HELP, MF_CHECKED);
    }
}

// Display text or an HTML page in the help window relevant to the current action.
void
display_help(char *key)
{
    char fname[256]; 

    strcpy_s(fname, 256, help_dir);
    strcat_s(fname, 256, key);
    strcat_s(fname, 256, ".htm");
    DisplayHTMLPage(hWndHelp, fname);
}

void
display_help_state(STATE state)
{
    display_help(state_key[app_state]);
}

// Change app state, displaying any help for the new state. Clear any error messages.
void
change_state(STATE new_state)
{
    app_state = new_state;
    display_help_state(app_state);
    clear_status_and_progress();
}

// Display the cursor for the new state
void
display_cursor(STATE new_state)
{
    SetCursor(cursors[new_state].cursor);
}

// Wndproc for help dialog. Comments from the original code (see htmlbrowser.[ch])
int WINAPI
help_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;

    switch (msg)
    {
    case WM_INITDIALOG:
        // Embed the browser object into our host window. We need do this only
        // once. Note that the browser object will start calling some of our
        // IOleInPlaceFrame and IOleClientSite functions as soon as we start
        // calling browser object functions in EmbedBrowserObject().
        if (EmbedBrowserObject(hWnd)) 
            return(-1);

        // Another window created with an embedded browser object
        ++WindowCount;
        break;

    case WM_DESTROY:
        // Detach the browser object from this window, and free resources.
        UnEmbedBrowserObject(hWnd);

        // One less window
        if (--WindowCount == 0)
            OleUninitialize();

        break;

    case WM_CLOSE:
        // Closing just hides the window, it can be brought back (like the other toolbars)
        view_help = FALSE;
        ShowWindow(hWnd, SW_HIDE);
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_HELP, view_help ? MF_CHECKED : MF_UNCHECKED);
        break;

        // NOTE: If you want to resize the area that the browser object occupies when you
        // resize the window, then handle WM_SIZE and use the IWebBrowser2's put_Width()
        // and put_Height() to give it the new dimensions.
        // TODO: This doesn't seem to work at all. Why?
    case WM_SIZE:
        SetBrowserSize(hWnd, LOWORD(lParam), HIWORD(lParam));
        break;
    }

    return 0;
}

