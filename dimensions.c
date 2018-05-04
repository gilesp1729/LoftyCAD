#include "stdafx.h"
#include "LoftyCAD.h"

// Return TRUE if an object has dimensions that can be displayed and changed.
BOOL
has_dims(Object *obj)
{
    switch (obj->type)
    {
    case OBJ_POINT:
        return FALSE;

    case OBJ_EDGE:
        switch (((Edge *)obj)->type)
        {
        case EDGE_BEZIER:
            return FALSE;
        }
        break;

    case OBJ_FACE:
        switch (((Face *)obj)->type)
        {
        case FACE_FLAT:
        case FACE_CYLINDRICAL:
        case FACE_GENERAL:
            return FALSE;
        }
        break;

    case OBJ_VOLUME:
        return FALSE;
    }

    return TRUE;
}

// Show a dimension or other hint in the dims window. The modeless dialog has
// already been set up and is hidden. The text can optionally be changed.
void
show_hint_at(POINT pt, char *buf, BOOL accept_input)
{
    POINT winpt;  // copy the POINT so it doesn't change the caller

    SendDlgItemMessage(hWndDims, IDC_DIMENSIONS, WM_SETTEXT, 0, (LPARAM)buf);
    EnableWindow(GetDlgItem(hWndDims, IDOK), accept_input);
    SendDlgItemMessage(hWndDims, IDC_DIMENSIONS, EM_SETREADONLY, !accept_input, 0);
    winpt.x = pt.x;
    winpt.y = pt.y;
    ClientToScreen(auxGetHWND(), &winpt);
    SetWindowPos(hWndDims, HWND_TOPMOST, winpt.x + 10, winpt.y + 20, 0, 0, SWP_NOSIZE);
    ShowWindow(hWndDims, SW_SHOW);
}

// Hide the dimensions window.
void
hide_hint()
{
    ShowWindow(hWndDims, SW_HIDE);
}

// Update the dimension of an object, delending on what has been returned by the dialog box.
void
update_dims(Object *obj, char *buf)
{
    Edge *e, *e0, *e1, *e2, *e3;
    ArcEdge *ae;
    Face *f;
    float angle;
    float len, len2;
    char *nexttok = NULL;
    char *tok;
    Object *parent;
    float matrix[16], v[4], res[4];

    // parse the changed string
    switch (obj->type)
    {
    case OBJ_EDGE:
        e = (Edge *)obj;
        switch (e->type)
        {
        case EDGE_STRAIGHT:
            len = (float)atof(buf);
            if (len == 0)
                break;
            new_length(e->endpoints[0], e->endpoints[1], len);
            break;

        case EDGE_ARC:
            ae = (ArcEdge *)e;
            tok = strtok_s(buf, " ,\t\n", &nexttok);
            len = (float)atof(tok);
            if (len == 0)
                break;
            new_length(ae->centre, e->endpoints[0], len);  // just typing the radius still works
            new_length(ae->centre, e->endpoints[1], len);

            tok = strtok_s(NULL, " ,\t\n", &nexttok);
            angle = (float)atof(tok);
            if (angle == 0)
                break;
            if (ae->clockwise)
                angle = -angle / RAD;
            else
                angle = angle / RAD;

            // transform arc to XY plane, centre at origin, endpoint 0 on x axis
            look_at_centre(*ae->centre, *e->endpoints[0], ae->normal, matrix);
            v[0] = len * cosf(angle);
            v[1] = len * sinf(angle);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col(matrix, v, res);
            e->endpoints[1]->x = res[0];
            e->endpoints[1]->y = res[1];
            e->endpoints[1]->z = res[2];
            break;
        }
        break;

    case OBJ_FACE:
        f = (Face *)obj;
        switch (f->type)
        {
        case FACE_RECT:
            e0 = f->edges[0];
            e1 = f->edges[1];
            e2 = f->edges[2];
            e3 = f->edges[3];
            tok = strtok_s(buf, " ,\t\n", &nexttok);
            len = (float)atof(tok);
            if (len == 0)
                break;
            tok = strtok_s(NULL, " ,\t\n", &nexttok);
            len2 = (float)atof(tok);
            if (len2 == 0)
                break;

            // Change the edges' lengths. be careful to do it in the right order.
            // TODO - this will be wrong for side faces after extrusion. Need to follow the rect around and
            // change the right sides at the right time!
            new_length(e0->endpoints[0], e0->endpoints[1], len);
            new_length(e2->endpoints[1], e2->endpoints[0], len);
            new_length(e3->endpoints[1], e3->endpoints[0], len2);
            new_length(e1->endpoints[0], e1->endpoints[1], len2);
            break;

        case FACE_CIRCLE:
            e = f->edges[0];
            ae = (ArcEdge *)e;
            len = (float)atof(buf);
            if (len == 0)
                break;
            new_length(ae->centre, e->endpoints[0], len);  // endpt 1 is the same point
            break;
        }
        break;
    }

    // If we have changed anything, invalidate all view lists
    parent = find_top_level_parent(object_tree, obj);
    invalidate_all_view_lists(parent, obj, 0, 0, 0);
}

// Show the dimensions of an existing object, and optionally accept changes to them. 
// If accepting input, display them in a modal dialog.
// Note that objects under construction don't always use this, as the object may not be
// fully valid yet.
void
show_dims_at(POINT pt, Object *obj, BOOL accept_input)
{
    char buf[64], buf2[64];
    Edge *e, *e0, *e1;
    ArcEdge *ae;
    Face *f;
    float angle;

    if (!has_dims(obj))
        return;

    // get the dimension(s) into a string, depending on the type of object we have picked.
    switch (obj->type)
    {
    case OBJ_EDGE:
        e = (Edge *)obj;
        switch (e->type)
        {
        case EDGE_STRAIGHT:
            sprintf_s(buf, 64, "%s mm", display_rounded(buf2, length(e->endpoints[0], e->endpoints[1])));
            break;
        case EDGE_ARC:
            ae = (ArcEdge *)e;
            if (ae->clockwise)
                angle = RAD * angle3(e->endpoints[1], ae->centre, e->endpoints[0], &ae->normal);
            else
                angle = RAD * angle3(e->endpoints[0], ae->centre, e->endpoints[1], &ae->normal);
            if (angle < 0)
                angle += 360;
            sprintf_s(buf, 64, "%s,%s mmR/deg",
                      display_rounded(buf, length(ae->centre, e->endpoints[0])),
                      display_rounded(buf2, angle)
                      );
            break;
        }
        break;

    case OBJ_FACE:
        f = (Face *)obj;
        switch (f->type)
        {
        case FACE_RECT:
            e0 = f->edges[0];
            e1 = f->edges[1];
            sprintf_s(buf, 64, "%s,%s mm", 
                      display_rounded(buf, length(e0->endpoints[0], e0->endpoints[1])), 
                      display_rounded(buf2, length(e1->endpoints[0], e1->endpoints[1])));
            break;
        case FACE_CIRCLE:
            e = f->edges[0];
            ae = (ArcEdge *)e;
            sprintf_s(buf, 64, "%s mmR", display_rounded(buf2, length(ae->centre, e->endpoints[0])));
            break;
        }
        break;
    }

    SetWindowLongPtr(hWndDims, DWL_USER, (LONG_PTR)obj);
    show_hint_at(pt, buf, accept_input);
}

// Wndproc for the dimensions dialog. Normally it's only for display while dragging
// but we can also type dimensions into it.
int WINAPI
dimensions_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char buf[64];
    static BOOL changed = FALSE;

    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_DIMENSIONS:
            if (HIWORD(wParam) == EN_CHANGE)
                changed = TRUE;
            break;
        case IDOK:
            SendDlgItemMessage(hWnd, IDC_DIMENSIONS, WM_GETTEXT, 64, (LPARAM)buf);
            if (changed)
            {
                Object *obj = (Object *)GetWindowLongPtr(hWnd, DWL_USER);
                update_dims(obj, buf);
            }
            // fall through
        case IDCANCEL:
            ShowWindow(hWnd, SW_HIDE);
            break;
        }
        break;
    }

    return 0;
}
