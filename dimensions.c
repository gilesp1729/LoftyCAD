#include "stdafx.h"
#include "LoftyCAD.h"

// Quick and dirty find first arc edge in a circle face. It is [0] for normal faces,
// but [1] if it has been reflected.
ArcEdge *
first_arc_edge(Face* f)
{
    ASSERT(f->type == FACE_CIRCLE, "Face must be a circle");
    if (f->edges[0]->type == EDGE_ARC)
        return (ArcEdge*)f->edges[0];

    ASSERT(f->edges[1]->type == EDGE_ARC, "Something is wrong with the circle face");
    return (ArcEdge*)f->edges[1];
}

// Return TRUE if an object has dimensions that can be displayed and changed.
BOOL
has_dims(Object *obj)
{
    Face *f;
    Volume *v;

    if (obj == NULL)
        return FALSE;

    if (app_state == STATE_MOVING)
        return TRUE;

    switch (obj->type)
    {
    case OBJ_POINT:
        return FALSE;

    case OBJ_EDGE:
        switch (((Edge *)obj)->type & ~EDGE_CONSTRUCTION)
        {
        case EDGE_BEZIER:
            return FALSE;
        }
        break;

    case OBJ_FACE:
        switch (((Face *)obj)->type & ~FACE_CONSTRUCTION)
        {
        case FACE_RECT:
        case FACE_CIRCLE:
            return TRUE;

        case FACE_FLAT:
            // If we are extruding, then we show a height on the dims.
            // Otherwise flat faces don't have anything.
            // TODO local extrude test here too
            f = (Face *)obj;
            if ((app_state == STATE_STARTING_EXTRUDE || app_state == STATE_DRAWING_EXTRUDE) && f->paired)
                return TRUE;
            else
                return FALSE;

        case FACE_CYLINDRICAL:
        case FACE_BARREL:
        case FACE_BEZIER:
            return FALSE;
        }
        break;

    case OBJ_VOLUME:
        v = (Volume *)obj;
        if (app_state == STATE_STARTING_SCALE || app_state == STATE_DRAWING_SCALE)
            return TRUE;
        if (app_state == STATE_STARTING_ROTATE || app_state == STATE_DRAWING_ROTATE)
            return TRUE;
        if (((Face *)v->faces.tail)->type == FACE_CIRCLE)
            return TRUE;
        if (v->measured)
            return TRUE;
        return FALSE;

    case OBJ_GROUP:
        if (app_state == STATE_STARTING_SCALE || app_state == STATE_DRAWING_SCALE)
            return TRUE;
        if (app_state == STATE_STARTING_ROTATE || app_state == STATE_DRAWING_ROTATE)
            return TRUE;
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
    EnableWindow(GetDlgItem(hWndDims, IDCANCEL), accept_input);
    SendDlgItemMessage(hWndDims, IDC_DIMENSIONS, EM_SETREADONLY, !accept_input, 0);
    winpt.x = pt.x;
    winpt.y = pt.y;
    ClientToScreen(auxGetHWND(), &winpt);
    SetWindowPos(hWndDims, HWND_TOPMOST, winpt.x + 10, winpt.y + 20, 0, 0, SWP_NOSIZE);
    ShowWindow(hWndDims, SW_SHOW);
#if 1
    Log(buf);
    Log("\r\n");
#endif
}

// Hide the dimensions window.
void
hide_hint()
{
    ShowWindow(hWndDims, SW_HIDE);
}

// Process any messages in the message queue - use to display status during long operations
void
process_messages(void)
{
    MSG Message;

    while (PeekMessage(&Message, NULL, 0, 0, PM_NOREMOVE) == TRUE)
    {
        if (GetMessage(&Message, NULL, 0, 0))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }
}

// Update the dimension of an object, depending on what has been returned by the dialog box.
void
update_dims(Object *obj, char *buf)
{
    Edge *e, *e0, *e1, *e2, *e3;
    ArcEdge *ae, *ae1, *ae2;
    Face *f, *c1, *c2;
    Volume *vol;
    double angle;
    float len, len2, rad;
    char *nexttok = NULL;
    char *tok;
    Object *parent;
    double matrix[16], v[4], res[4];

    // parse the changed string
    switch (obj->type)
    {
    case OBJ_EDGE:
        e = (Edge *)obj;
        switch (e->type & ~EDGE_CONSTRUCTION)
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
            angle = atof(tok);
            if (angle == 0)
                break;
            if (ae->clockwise)
                angle = -angle / RAD;
            else
                angle = angle / RAD;

            // transform arc to XY plane, centre at origin, endpoint 0 on x axis
            look_at_centre_d(*ae->centre, *e->endpoints[0], ae->normal, matrix);
            v[0] = len * cos(angle);
            v[1] = len * sin(angle);
            v[2] = 0;
            v[3] = 1;
            mat_mult_by_col_d(matrix, v, res);
            e->endpoints[1]->x = (float)res[0];
            e->endpoints[1]->y = (float)res[1];
            e->endpoints[1]->z = (float)res[2];
            break;
        }
        break;

    case OBJ_FACE:
        f = (Face *)obj;
        switch (f->type & ~FACE_CONSTRUCTION)
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
            // TODO1  - this will be wrong for side faces after extrusion. Need to follow the rect around and
            // change the right sides at the right time! Use initial point and chase round in the right order.
            new_length(e0->endpoints[0], e0->endpoints[1], len);
            new_length(e2->endpoints[1], e2->endpoints[0], len);
            new_length(e3->endpoints[1], e3->endpoints[0], len2);
            new_length(e1->endpoints[0], e1->endpoints[1], len2);
            break;

        case FACE_CIRCLE:
            e = f->edges[0];
            ae = (ArcEdge *)e;
            rad = (float)atof(buf);
            if (rad == 0)
                break;
            new_length(ae->centre, e->endpoints[0], rad);
            new_length(ae->centre, e->endpoints[1], rad);
            break;
        }
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        if (((Face *)vol->faces.tail)->type == FACE_CIRCLE)    // Cylinders
        {
            c1 = (Face *)vol->faces.tail;
            c2 = (Face *)c1->hdr.prev;
            ASSERT(c1->type == FACE_CIRCLE, "Face 1 must be a circle");
            ASSERT(c2->type == FACE_CIRCLE, "Face 2 must be a circle");

            ae1 = first_arc_edge(c1);
            ae2 = first_arc_edge(c2);
            e1 = (Edge *)ae1;
            e2 = (Edge *)ae2;
            tok = strtok_s(buf, " ,\t\n", &nexttok);
            rad = (float)atof(tok);
            if (rad == 0)
                break;
            tok = strtok_s(NULL, " ,\t\n", &nexttok);
            len = (float)atof(tok);
            if (len == 0)
                break;
            new_length(ae1->centre, e1->endpoints[0], rad);
            new_length(ae1->centre, e1->endpoints[1], rad);
            new_length(ae2->centre, e2->endpoints[0], rad);
            new_length(ae2->centre, e2->endpoints[1], rad);

            new_length(ae2->centre, ae1->centre, len);
            new_length(e2->endpoints[0], e1->endpoints[0], len);
            new_length(e2->endpoints[1], e1->endpoints[1], len);
        }
        else if (vol->measured)
        {
            float l, w, h;

            tok = strtok_s(buf, " ,\t\n", &nexttok);
            l = (float)atof(tok);
            tok = strtok_s(NULL, " ,\t\n", &nexttok);
            w = (float)atof(tok);
            tok = strtok_s(NULL, " ,\t\n", &nexttok);
            h = (float)atof(tok);

            // Determine principal directions from paired faces with positive axis-aligned normals
            for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
            {
                float x, y, z;

                if (!f->paired)
                    continue;
                x = dot(f->normal.A, f->normal.B, f->normal.C, 1, 0, 0);
                y = dot(f->normal.A, f->normal.B, f->normal.C, 0, 1, 0);
                z = dot(f->normal.A, f->normal.B, f->normal.C, 0, 0, 1);
                if (x > y && x > z)
                {
                    move_obj((Object*)f, l - f->extrude_height, 0, 0);
                    clear_move_copy_flags((Object*)f);
                }
                else if (y > x && y > z)
                {
                    move_obj((Object*)f, 0, w - f->extrude_height, 0);
                    clear_move_copy_flags((Object*)f);
                }
                else if (z > x && z > y)
                {
                    move_obj((Object*)f, 0, 0, h - f->extrude_height);
                    clear_move_copy_flags((Object *)f);
                }
            }
            calc_extrude_heights(vol);
        }

        break;
    }

    // If we have changed anything, invalidate all view lists
    parent = find_parent_object(&object_tree, obj, TRUE);
    invalidate_all_view_lists(parent, obj, 0, 0, 0);
    update_drawing();
}

// Get the dims into a string, if they are available for an object.
// Otherwise return a blank string (not NULL) so it can be used in a printf.
char *
get_dims_string(Object *obj, char buf[64])
{
    char buf2[64], buf3[64];
    Point *p0, *p1, *p2;
    Edge *e, *e1;
    ArcEdge *ae, *ae1, *ae2;
    Face *f, *c1, *c2;
    Volume *v;
    Group* g;
    double angle;

    buf[0] = '\0';

#if 0  // this doesn't work for objects with parents (like groups) so take it out.
    // If moving, return the distance moved.
    // TODO - this happens for all objects! Only do it on the one(s) being moved..
    if (app_state == STATE_MOVING && obj == picked_obj)
    {
        sprintf_s(buf, 64, "Moved %s mm", display_rounded(buf2, length(&picked_point, &new_point)));
        return buf;
    }
#endif


    switch (obj->type)
    {
    case OBJ_EDGE:
        e = (Edge *)obj;
        switch (e->type & ~EDGE_CONSTRUCTION)
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
                      display_rounded(buf2, (float)angle)
                      );
            break;
        }
        break;

    case OBJ_FACE:
        f = (Face *)obj;
        // TODO local extrude test here too
        if ((app_state == STATE_STARTING_EXTRUDE || app_state == STATE_DRAWING_EXTRUDE) && f->paired)
        {
            float x = fabsf(dot(f->normal.A, f->normal.B, f->normal.C, 1, 0, 0));
            float y = fabsf(dot(f->normal.A, f->normal.B, f->normal.C, 0, 1, 0));
            float z = fabsf(dot(f->normal.A, f->normal.B, f->normal.C, 0, 0, 1));

            // We are extruding, so show the length/width/height.
            if (x > y && x > z)
                sprintf_s(buf, 64, "%s mm long", display_rounded(buf, f->extrude_height));
            else if (y > x && y > z)
                sprintf_s(buf, 64, "%s mm wide", display_rounded(buf, f->extrude_height));
            else if (z > x && z > y)
                sprintf_s(buf, 64, "%s mm high", display_rounded(buf, f->extrude_height));
        }
        else
        {
            switch (f->type & ~FACE_CONSTRUCTION)
            {
            case FACE_RECT:
                // use view list here, then it works for drawing rects
                // protect against NULL view list (transitory)
                p0 = (Point *)f->view_list.head;
                if (p0 == NULL)
                    break;
                p1 = (Point *)p0->hdr.next;
                p2 = (Point *)p1->hdr.next;
                sprintf_s(buf, 64, "%s,%s mm",
                          display_rounded(buf, length(p0, p1)),
                          display_rounded(buf2, length(p1, p2)));
                break;

            case FACE_CIRCLE:
                ae = first_arc_edge(f);
                e = (Edge*)ae;
                sprintf_s(buf, 64, "%s mmR", display_rounded(buf2, length(ae->centre, e->endpoints[0])));
                break;
            }
        }
        break;

    case OBJ_VOLUME:
        v = (Volume *)obj;
        if (app_state == STATE_STARTING_SCALE || app_state == STATE_DRAWING_SCALE)
        {
            if (v->xform != NULL)
            {
                switch (scaled)
                {
                case DIRN_X:
                    sprintf_s(buf, 64, "%sx", display_rounded(buf, v->xform->sx));
                    break;
                case DIRN_Y:
                    sprintf_s(buf, 64, "%sy", display_rounded(buf, v->xform->sy));
                    break;
                case DIRN_Z:
                    sprintf_s(buf, 64, "%sz", display_rounded(buf, v->xform->sz));
                    break;
                case DIRN_X | DIRN_Y:
                    sprintf_s(buf, 64, "%s,%sxy",
                              display_rounded(buf, v->xform->sx),
                              display_rounded(buf2, v->xform->sy));
                    break;
                case DIRN_X | DIRN_Z:
                    sprintf_s(buf, 64, "%s,%sxz",
                              display_rounded(buf, v->xform->sx),
                              display_rounded(buf2, v->xform->sz));
                    break;
                case DIRN_Y | DIRN_Z:
                    sprintf_s(buf, 64, "%s,%syz",
                              display_rounded(buf, v->xform->sy),
                              display_rounded(buf2, v->xform->sz));
                    break;
                }
            }
        }
        else if (app_state == STATE_STARTING_ROTATE || app_state == STATE_DRAWING_ROTATE)
        {
            sprintf_s(buf, 64, "%sdeg", 
                      display_rounded(buf, cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT)));
        }
        else if (((Face *)v->faces.tail)->type == FACE_CIRCLE)    // Cylinders
        {
            c1 = (Face *)v->faces.tail;
            c2 = (Face *)c1->hdr.prev;
            ae1 = first_arc_edge(c1);
            ae2 = first_arc_edge(c2);
            e1 = (Edge*)ae1;
            sprintf_s
            (
                buf, 64, "%s,%s mmR/h", 
                display_rounded(buf, length(ae1->centre, e1->endpoints[0])),
                display_rounded(buf2, length(ae1->centre, ae2->centre))
            );
        }
        else if (v->measured)   // Rect prisms with all 3 dims known from parallel faces
        {
            float x, y, z, l, w, h;

            // Determine principal directions from paired faces with positive axis-aligned normals
            for (f = (Face*)v->faces.head; f != NULL; f = (Face*)f->hdr.next)
            {
                if (!f->paired)
                    continue;
                x = dot(f->normal.A, f->normal.B, f->normal.C, 1, 0, 0);
                y = dot(f->normal.A, f->normal.B, f->normal.C, 0, 1, 0);
                z = dot(f->normal.A, f->normal.B, f->normal.C, 0, 0, 1);
                if (x > y && x > z)
                    l = f->extrude_height;
                else if (y > x && y > z)
                    w = f->extrude_height;
                else if (z > x && z > y)
                    h = f->extrude_height;
            }
            sprintf_s
            (
                buf, 64, "%s,%s,%s mm",
                display_rounded(buf, l),
                display_rounded(buf2, w),
                display_rounded(buf3, h)
            );
        }

        break;

    case OBJ_GROUP:
        g = (Group*)obj;
        if (app_state == STATE_STARTING_SCALE || app_state == STATE_DRAWING_SCALE)
        {
            if (g->xform != NULL)
            {
                switch (scaled)
                {
                case DIRN_X:
                    sprintf_s(buf, 64, "%sx", display_rounded(buf, g->xform->sx));
                    break;
                case DIRN_Y:
                    sprintf_s(buf, 64, "%sy", display_rounded(buf, g->xform->sy));
                    break;
                case DIRN_Z:
                    sprintf_s(buf, 64, "%sz", display_rounded(buf, g->xform->sz));
                    break;
                case DIRN_X | DIRN_Y:
                    sprintf_s(buf, 64, "%s,%sxy",
                        display_rounded(buf, g->xform->sx),
                        display_rounded(buf2, g->xform->sy));
                    break;
                case DIRN_X | DIRN_Z:
                    sprintf_s(buf, 64, "%s,%sxz",
                        display_rounded(buf, g->xform->sx),
                        display_rounded(buf2, g->xform->sz));
                    break;
                case DIRN_Y | DIRN_Z:
                    sprintf_s(buf, 64, "%s,%syz",
                        display_rounded(buf, g->xform->sy),
                        display_rounded(buf2, g->xform->sz));
                    break;
                }
            }
        }
        else if (app_state == STATE_STARTING_ROTATE || app_state == STATE_DRAWING_ROTATE)
        {
            sprintf_s(buf, 64, "%sdeg",
                display_rounded(buf, cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT)));
        }

        break;
    }

    return buf;
}

// Show the dimensions of an existing object, and optionally accept changes to them. 
// If accepting input, display them in a modal dialog.
// Note that objects under construction don't always use this, as the object may not be
// fully valid yet.
void
show_dims_at(POINT pt, Object *obj, BOOL accept_input)
{
    char buf[64];

    if (!has_dims(obj))
        return;

    get_dims_string(obj, buf);
    SetWindowLongPtr(hWndDims, DWL_USER, (LONG_PTR)obj);
    show_hint_at(pt, buf, accept_input);
}

#if 0
// show a string on an object.
void
show_hint_on(Object *obj, char buf[64])
{
}
#endif

// Show dimensions on an object during drawing.
void
show_dims_on(Object *obj, PRESENTATION pres, LOCK parent_lock)
{
    HDC hdc = auxGetHDC();
    Volume *v;
    Face *f;
    Edge *e;
    Point *p0, *p1, *p2;
    int n;
    float x, y, z;
    BOOL selected = pres & DRAW_SELECTED;
    BOOL highlighted = pres & DRAW_HIGHLIGHT;

    char buf[64];

    if (!has_dims(obj))
        return;

    get_dims_string(obj, buf);

    switch (obj->type & ~EDGE_CONSTRUCTION)
    {
    case OBJ_EDGE:
        e = (Edge *)obj;
        if ((e->type & EDGE_CONSTRUCTION) && !view_constr)
            return;

        color_as(OBJ_EDGE, 1.0f, e->type & EDGE_CONSTRUCTION, 0, FALSE);
        glRasterPos3f
        (
            (e->endpoints[0]->x + e->endpoints[1]->x) / 2,
            (e->endpoints[0]->y + e->endpoints[1]->y) / 2,
            (e->endpoints[0]->z + e->endpoints[1]->z) / 2
        );
        break;

    case OBJ_FACE:
        f = (Face *)obj;
        if ((f->type & FACE_CONSTRUCTION) && !view_constr)
            return;

        // color face dims in the edge color, so they can be easily read
        color_as(OBJ_EDGE, 1.0f, f->type & FACE_CONSTRUCTION, 0, FALSE);

        switch (f->type)
        {
        case FACE_CIRCLE:
            if (f->edges[0]->type == EDGE_ARC)
                p0 = ((ArcEdge*)f->edges[0])->centre;
            else if (f->edges[1]->type == EDGE_ARC)
                p0 = ((ArcEdge*)f->edges[1])->centre;
            else
            {
                ASSERT(FALSE, "Where's the arc edge?");
                p0 = f->edges[0]->endpoints[0];
            }
            glRasterPos3f(p0->x, p0->y, p0->z);
            break;

        default:
            // use view list here if there are no edges yet, then it works for rects being drawn in.
            if (f->n_edges == 0)
            {
                p0 = (Point *)f->view_list.head;
                p1 = (Point *)p0->hdr.next;
                p2 = (Point *)p1->hdr.next;
                glRasterPos3f
                (
                    (p0->x + p2->x) / 2,
                    (p0->y + p2->y) / 2,
                    (p0->z + p2->z) / 2
                );
            }
            else
            {
                x = y = z = 0;
                for (n = 0, p0 = (Point*)f->view_list.head->next; p0 != NULL; p0 = (Point*)p0->hdr.next, n++)
                {
                    x += p0->x;
                    y += p0->y;
                    z += p0->z;
                }
                x /= n;
                y /= n;
                z /= n;
                glRasterPos3f(x, y, z);
            }
            break;
        }
        break;

    case OBJ_VOLUME:
    case OBJ_GROUP:
        v = (Volume *)obj;
        color_as(OBJ_EDGE, 1.0f, FALSE, 0, FALSE);
        glRasterPos3f(v->bbox.xc, v->bbox.yc, v->bbox.zc);
        break;
    }

    glListBase(1000);
    ASSERT(strlen(buf) < 64, "String too long!");
    glCallLists(strlen(buf), GL_UNSIGNED_BYTE, buf);
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

