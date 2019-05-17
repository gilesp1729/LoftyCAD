#include "htmlbrowser.h"
#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// This is indexed by the app state (LoftyCAD.h)
char *state_key[] =
{
    "Exploring",
    "Moving",
    "Dragging_Selection",

    "Drawing_Edge",
    "Drawing_Face",
    "Drawing_Face",
    "Drawing_Edge",
    "Drawing_Edge",
    "Drawing_Extrude",
    "Drawing_Scale",
    "Drawing_Rotate",

    "Drawing_Edge",
    "Drawing_Face",
    "Drawing_Face",
    "Drawing_Edge",
    "Drawing_Edge",
    "Drawing_Extrude",
    "Drawing_Scale",
    "Drawing_Rotate"
};

HWND init_help_window(void)
{
    HWND hWnd;

    OleInitialize(NULL);
    hWnd = CreateDialog
        (
        hInst,
        MAKEINTRESOURCE(IDD_HELP),
        auxGetHWND(),
        help_dialog
        );

    SetWindowPos(hWnd, HWND_NOTOPMOST, wWidth, wHeight / 2, 0, 0, SWP_NOSIZE);
    if (view_help)
        ShowWindow(hWnd, SW_SHOW);

    return hWnd;
}

// Display text or an HTML page in the help window relevant to the current action.
void
display_help(char *key)
{
    char fname[256]; 
    char *slosh;

    GetModuleFileName(NULL, fname, 256);
    slosh = strrchr(fname, '\\');
    if (slosh != NULL)
        *slosh = '\0';
    strcat_s(fname, 256, "\\html\\");
    strcat_s(fname, 256, key);
    strcat_s(fname, 256, ".htm");
    DisplayHTMLPage(hWndHelp, fname);
}

void
display_help_state(STATE state)
{
    display_help(state_key[app_state]);
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
    }

    // NOTE: If you want to resize the area that the browser object occupies when you
    // resize the window, then handle WM_SIZE and use the IWebBrowser2's put_Width()
    // and put_Height() to give it the new dimensions.

    return 0;
}

