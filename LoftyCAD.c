// LoftyCAD.c: Defines the entry point for the application.
//

#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

#define INIT_ZTRANS -2

GLint wWidth = 800, wHeight = 800;
float xTrans = 0;
float yTrans = 0;
float zTrans = INIT_ZTRANS;
int zoom_delta = 0;

int	left_mouseX, left_mouseY;
int	right_mouseX, right_mouseY;
BOOL	left_mouse = FALSE;
BOOL	right_mouse = FALSE;

// Toolbars
HWND hWndToolbar;
HWND hWndDebug;
BOOL view_tools = TRUE;
BOOL view_debug = FALSE;

// State the app is in.
STATE app_state = STATE_NONE;

// A list of objects that are currently selected. The "prev" pointer points to 
// the actual object (it's a singly linked list)
Object *selection = NULL;

// List of objects to be drawn
Object *object_tree = NULL;

// Set TRUE whenever something is changed and the tree needs to be saved
BOOL drawing_changed = FALSE;

// The current object(s) being drawn (and not yet added to the object tree)
Object *curr_obj = NULL;

// The object that was picked when an action, such as drawing, was first
// started. Also the point where the pick occurred (in (x,y,z) coordinates)
Object *picked_obj = NULL;
Point picked_point;

// The plane that picked_obj lies in; or else the facing plane if there is none.
Plane *picked_plane = NULL;

// Standard planes
Plane plane_XY = { 0, };
Plane plane_XZ = { 0, };
Plane plane_YZ = { 0, };
Plane plane_mXY = { 0, };
Plane plane_mXZ = { 0, };
Plane plane_mYZ = { 0, };

float quat_XY[4] = { 0, 0, 0, 1 };
float quat_YZ[4] = { 0, -0.707, 0, 0.707 };
float quat_XZ[4] = { -0.707, 0, 0, -0.707 };
float quat_mXY[4] = { 0, 1, 0, 0 };
float quat_mYZ[4] = { -0.5, 0.5, 0.5, -0.5 };
float quat_mXZ[4] = { 0, -0.707, -0.707, 0 };

Plane *facing_plane = &plane_XY;
PLANE facing_index = PLANE_XY;

// Viewing model (ortho or perspective)
BOOL view_ortho = FALSE;

// Current filename
char curr_filename[256] = { 0, };

// Set material and lighting up
void
SetMaterial(bBlack)
{
    static float front_mat_shininess[] = { 30.0f };
    static float front_mat_specular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    static float front_mat_diffuse[] = { 0.5f, 0.28f, 0.38f, 1.0f };

    static float back_mat_shininess[] = { 50.0f };
    static float back_mat_specular[] = { 0.5f, 0.5f, 0.2f, 1.0f };
    static float back_mat_diffuse[] = { 1.0f, 1.0f, 0.2f, 1.0f };

    static float black_mat_shininess[] = { 0.0f };
    static float black_mat_specular[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static float black_mat_diffuse[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    static float ambient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    static float no_ambient[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static float lmodel_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static float lmodel_no_ambient[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (!bBlack) {
        glMaterialfv(GL_FRONT, GL_SHININESS, front_mat_shininess);
        glMaterialfv(GL_FRONT, GL_SPECULAR, front_mat_specular);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, front_mat_diffuse);
        glMaterialfv(GL_BACK, GL_SHININESS, back_mat_shininess);
        glMaterialfv(GL_BACK, GL_SPECULAR, back_mat_specular);
        glMaterialfv(GL_BACK, GL_DIFFUSE, back_mat_diffuse);
        glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    }
    else {
        glMaterialfv(GL_FRONT, GL_SHININESS, black_mat_shininess);
        glMaterialfv(GL_FRONT, GL_SPECULAR, black_mat_specular);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, black_mat_diffuse);
        glMaterialfv(GL_BACK, GL_SHININESS, black_mat_shininess);
        glMaterialfv(GL_BACK, GL_SPECULAR, black_mat_specular);
        glMaterialfv(GL_BACK, GL_DIFFUSE, black_mat_diffuse);
        glLightfv(GL_LIGHT0, GL_AMBIENT, no_ambient);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_no_ambient);
    }
}

void
Init(void)
{
    static GLint colorIndexes[3] = { 0, 200, 255 };
    static float ambient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    static float diffuse[] = { 0.5f, 1.0f, 1.0f, 1.0f };
    static float position[] = { 90.0f, 90.0f, 150.0f, 0.0f };
    static float lmodel_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static float lmodel_twoside[] = { GL_TRUE };
    static float decal[] = { GL_DECAL };
    static float modulate[] = { GL_MODULATE };
    static float repeat[] = { GL_REPEAT };
    static float nearest[] = { GL_NEAREST };

    glClearColor(1.0, 1.0, 1.0, 1.0);

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    SetMaterial(FALSE);

    plane_XY.C = 1.0;           // set up planes
    plane_XZ.B = 1.0;
    plane_YZ.A = 1.0;
    plane_mXY.C = -1.0;
    plane_mXZ.B = -1.0;
    plane_mYZ.A = -1.0;

    glEnable(GL_CULL_FACE);    // don't show back facing faces
}

// Set up frustum and possibly picking matrix
void CALLBACK
Position(BOOL picking, GLint x_pick, GLint y_pick)
{
    GLint viewport[4], width, height;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glGetIntegerv(GL_VIEWPORT, viewport);
    width = viewport[2];
    height = viewport[3];
    if (picking)
        gluPickMatrix(x_pick, height - y_pick, 3, 3, viewport);  // Y window coords need inverting for GL

    if (view_ortho)
    {
        if (width > height)
        {
            glOrtho(-(double)width / height, (double)width / height, -1, 1, 0.5, 50);
        }
        else
        {
            glOrtho(-1, 1, -(double)height / width, (double)height / width, 0.5, 50);
        }
    }
    else
    {
        if (width > height)
        {
            glFrustum(-(double)width / height, (double)width / height, -1, 1, 0.5, 50);
        }
        else
        {
            glFrustum(-1, 1, -(double)height / width, (double)height / width, 0.5, 50);
        }
    }

    glTranslated(xTrans, yTrans, zTrans);
}

// Change the proportions of the viewport when window is resized
void CALLBACK
Reshape(int width, int height)
{
    trackball_Resize(width, height);

    glViewport(0, 0, (GLint)width, (GLint)height);

    Position(FALSE, 0, 0);
}

// Pick an object: find the frontmost object under the cursor, or NULL if nothing is there.
// Pick objects from "min_priority" down in the hierarchy; e.g. faces/edges/points, or just edges/points.
Object *
Pick(GLint x_pick, GLint y_pick, OBJECT min_priority)
{
    GLuint buffer[512];
    GLint viewport[4];
    GLint num_hits;
    GLuint min_obj = 0;
    OBJECT priority = OBJ_NONE;

    // Find the object under the cursor, if any
    glSelectBuffer(512, buffer);
    glRenderMode(GL_SELECT);
    glGetIntegerv(GL_VIEWPORT, viewport);
    Draw(TRUE, x_pick, y_pick);
    num_hits = glRenderMode(GL_RENDER);

    if (num_hits > 0)
    {
        int n = 0;
        int i, j, len;
        char buf[512];
        size_t size = 512;
        char *p = buf;
        GLuint min_depth = 0xFFFFFFFF;
        char *obj_prefix[] = { "P", "E", "F", "V" };
        Object *obj;

        for (i = 0; i < num_hits; i++)
        {
            int num_objs = buffer[n];

            if (num_objs == 0)
            {
                n += 3;  // skip count, min and max
                break;
            }

            // buffer = {{num objs, min depth, max depth, obj name, ...}, ...}

            // find top-of-stack and what kind of object it is
            obj = (Object *)buffer[n + num_objs + 2];
            if (obj == NULL)
                priority = OBJ_NONE;
            else
                priority = obj->type;

            if (priority < min_priority || buffer[n + 1] < min_depth)
            {
                min_depth = buffer[n + 1];
                min_priority = priority;
                min_obj = buffer[n + num_objs + 2];  // top of stack is last element in buffer
            }

            if (view_debug)
            {
                len = sprintf_s(p, size, "objs %d min %x max %x: ", buffer[n], buffer[n + 1], buffer[n + 2]);
                p += len;
                size -= len;
                n += 3;
                for (j = 0; j < num_objs; j++)
                {
                    Object *obj = (Object *)buffer[n];
 
                    if (obj == NULL)
                        len = sprintf_s(p, size, "NULL ");
                    else
                        len = sprintf_s(p, size, "%s%d ", obj_prefix[obj->type], obj->ID);
                    p += len;
                    size -= len;
                    n++;
                }
                len = sprintf_s(p, size, "\r\n");
                p += len;
                size -= len;
            }
            else  // not logging, still need to skip the data
            {
                n += num_objs + 3;
            }
        }

        if (view_debug)
        {
            obj = (Object *)min_obj;
            if (obj == NULL)
                len = sprintf_s(p, size, "Frontmost: NULL\r\n");
            else
                len = sprintf_s(p, size, "Frontmost: %s%d\r\n", obj_prefix[obj->type], obj->ID);

            Log(buf);
        }
    }

    return (Object *)min_obj;
}

// callback version of Draw for AUX/TK
void CALLBACK
DrawCB(void)
{
    Draw(FALSE, 0, 0);
}

// Find if an object is in the selection, returning TRUE if found, and
// a pointer to the _previous_ element (to aid deletion) if not the first.
BOOL
is_selected(Object *obj, Object **prev_in_list)
{
    Object *sel;
    BOOL present = FALSE;

    *prev_in_list = NULL;
    for (sel = selection; sel != NULL; sel = sel->next)
    {
        if (sel->prev == obj)
        {
            present = TRUE;
            break;
        }
        *prev_in_list = sel;
    }

    return present;
}

// Clear the selection
void
clear_selection(void)
{
    Object *sel_obj, *next_obj;

    sel_obj = selection;
    do
    {
        next_obj = sel_obj->next;
        free(sel_obj);
        sel_obj = next_obj;
    } while (next_obj != NULL);

    selection = NULL;
}

// Mouse handlers
void CALLBACK
left_down(AUX_EVENTREC *event)
{
    Object *dummy;

    // In any case, find if there is an object under the cursor, and
    // also find if it is in the selection.
    picked_obj = Pick(event->data[0], event->data[1], OBJ_FACE);

    switch (app_state)
    {
    case STATE_NONE:
        if (!is_selected(picked_obj, &dummy))
        {
            trackball_MouseDown(event);
        }
        else
        {
            SetCapture(auxGetHWND());
            left_mouseX = event->data[AUX_MOUSEX];
            left_mouseY = event->data[AUX_MOUSEY];
            left_mouse = TRUE;
            app_state = STATE_MOVING;

            // Set the picked point on the facing plane for later subtraction
            intersect_ray_plane(left_mouseX, left_mouseY, facing_plane, &picked_point);
        }
        break;

    case STATE_STARTING_EDGE:
    case STATE_STARTING_RECT:
    case STATE_STARTING_CIRCLE:
    case STATE_STARTING_BEZIER:
    case STATE_STARTING_ARC:
    case STATE_STARTING_MEASURE:
    case STATE_STARTING_EXTRUDE:
        // We can't always determine plane in which the new edge will be drawn.
        // This only works with a mouse move within a face, and we will often have
        // started on an edge or snapped to a point. 
        // But if we do have a plane, we can set it here.
        SetCapture(auxGetHWND());

        // Remember the initial mouse click so we can test for gross movement later
        left_mouseX = event->data[AUX_MOUSEX];
        left_mouseY = event->data[AUX_MOUSEY];
        left_mouse = TRUE;

        // Move into the corresponding drawing state
        app_state += STATE_DRAWING_OFFSET;

        if (picked_obj == NULL)
        {
            picked_plane = facing_plane;
            intersect_ray_plane(left_mouseX, left_mouseY, picked_plane, &picked_point);
        }
        else
        {
            picked_plane = NULL;

            // TODO: snap to any valid snap target and remember the (x,y,z) position of the
            // first point.
            // - find intersection of projection ray with picked_obj

            switch (picked_obj->type)
            {
            case OBJ_FACE:
                // We're on a face, so we can proceed.
                picked_plane = &((Face *)picked_obj)->normal;
                intersect_ray_plane(left_mouseX, left_mouseY, picked_plane, &picked_point);
                break;

            case OBJ_EDGE:
                // Find a picked point in the edge (we don't have a plane yet)
                snap_ray_edge(left_mouseX, left_mouseY, (Edge *)picked_obj, &picked_point);
                break;

            case OBJ_POINT:
                // Snap to the point.
                picked_point = *(Point *)picked_obj;
                break;
            }
        }
        // Don't add the object yet until we have moved the mouse, as a click will
        // need to be handled harmlessly and silently.
        break;

    case STATE_DRAWING_RECT:
    case STATE_DRAWING_CIRCLE:
    case STATE_DRAWING_ARC:
    case STATE_DRAWING_BEZIER:
    case STATE_DRAWING_MEASURE:
    case STATE_DRAWING_EXTRUDE:
        ASSERT(FALSE, "Mouse down in drawing state");
        break;
    }
}

void CALLBACK
left_up(AUX_EVENTREC *event)
{
    Face *rf;
    StraightEdge *se;
    Object *sel_obj;
    float mv[16], vec[4], nvec[4];
    float eye[4] = { 0, 0, 1, 0 };
    char buf[256];

    switch (app_state)
    {
    case STATE_NONE:
        // We have orbited - calculate the facing plane again
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
#if 0
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[0], mv[1], mv[2], mv[3]);
        Log(buf);
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[4], mv[5], mv[6], mv[7]);
        Log(buf);
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[8], mv[9], mv[10], mv[11]);
        Log(buf);
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[12], mv[13], mv[14], mv[15]);
        Log(buf);
#endif
        mat_mult_by_row(mv, eye, nvec);
        sprintf_s(buf, 256, "Eye: %f %f %f\r\n", nvec[0], nvec[1], nvec[2]);
        Log(buf);
        vec[0] = fabsf(nvec[0]);
        vec[1] = fabsf(nvec[1]);
        vec[2] = fabsf(nvec[2]);
        if (vec[0] > vec[1] && vec[0] > vec[2])
        {
            if (nvec[0] > 0)
            {
                facing_plane = &plane_YZ;
                facing_index = PLANE_YZ;
                Log("Facing plane YZ\r\n");
            }
            else
            {
                facing_plane = &plane_mYZ;
                facing_index = PLANE_MINUS_YZ;
                Log("Facing plane -YZ\r\n");
            }
        }
        else if (vec[1] > vec[0] && vec[1] > vec[2])
        {
            if (nvec[1] > 0)
            {
                facing_plane = &plane_XZ;
                facing_index = PLANE_XZ;
                Log("Facing plane XZ\r\n");
            }
            else
            {
                facing_plane = &plane_mXZ;
                facing_index = PLANE_MINUS_XZ;
                Log("Facing plane -XZ\r\n");
            }
        }
        else
        {
            if (nvec[2] > 0)
            {
                facing_plane = &plane_XY;
                facing_index = PLANE_XY;
                Log("Facing plane XY\r\n");
            }
            else
            {
                facing_plane = &plane_mXY;
                facing_index = PLANE_MINUS_XY;
                Log("Facing plane -XY\r\n");
            }
        }

        trackball_MouseUp(event);
        break;

    case STATE_MOVING:
        ReleaseCapture();
        left_mouse = FALSE;
        app_state = STATE_NONE;

        // regenerate the view lists for all moved faces to get rid of
        // any little errors that have crept in
        for (sel_obj = selection; sel_obj != NULL; sel_obj = sel_obj->next)
        {
            Object *parent = find_top_level_parent(object_tree, sel_obj->prev);
            Face *face;

            if (parent->type == OBJ_VOLUME)
            {
                for (face = ((Volume *)parent)->faces; face != NULL; face = (Face *)face->hdr.next)
                    face->view_valid = FALSE;
            }
            else if (parent->type == OBJ_FACE)
            {
                ((Face *)parent)->view_valid = FALSE;
            }
        }

        drawing_changed = TRUE;
        break;

    case STATE_DRAWING_RECT:
        if (curr_obj != NULL)
        {
            Point *p00, *p01, *p02, *p03;

            // Create the edges for the rect here as a special case, as the order
            // is not known till the mouse is released.
            // generate four edges, and put them on the face's edge list
            // (in opposite order, as they are being added at the list head)
            rf = (Face *)curr_obj;
            p00 = rf->view_list;
            p01 = (Point *)p00->hdr.next;
            p02 = (Point *)p01->hdr.next;
            p03 = (Point *)p02->hdr.next;

            se = (StraightEdge *)edge_new(EDGE_STRAIGHT);
            se->endpoints[0] = p03;
            se->endpoints[1] = p00;
            link((Object *)se, (Object **)&rf->edges);
            se = (StraightEdge *)edge_new(EDGE_STRAIGHT);
            se->endpoints[0] = p02;
            se->endpoints[1] = p03;
            link((Object *)se, (Object **)&rf->edges);
            se = (StraightEdge *)edge_new(EDGE_STRAIGHT);
            se->endpoints[0] = p01;
            se->endpoints[1] = p02;
            link((Object *)se, (Object **)&rf->edges);
            se = (StraightEdge *)edge_new(EDGE_STRAIGHT);
            se->endpoints[0] = p00;
            se->endpoints[1] = p01;
            link((Object *)se, (Object **)&rf->edges);

            // the face now has its edges. Generate its view list and the normal
            rf->view_valid = FALSE;
            gen_view_list(rf);
        }
        // fallthrough
    case STATE_DRAWING_EDGE:
    case STATE_DRAWING_CIRCLE:
    case STATE_DRAWING_BEZIER:
    case STATE_DRAWING_ARC:
    case STATE_DRAWING_MEASURE:
        // add new object to the object tree
        if (curr_obj != NULL)
        {
            link(curr_obj, &object_tree);
            drawing_changed = TRUE;
            curr_obj = NULL;
            // TODO push tree to undo stack


        }

        ReleaseCapture();
        left_mouse = FALSE;
        app_state = STATE_NONE;
        break;

    case STATE_STARTING_RECT:
    case STATE_STARTING_CIRCLE:
    case STATE_STARTING_ARC:
    case STATE_STARTING_BEZIER:
    case STATE_STARTING_MEASURE:
    case STATE_STARTING_EXTRUDE:
        ASSERT(FALSE, "Mouse up in starting state");
        curr_obj = NULL;
        ReleaseCapture();
        left_mouse = FALSE;
        app_state = STATE_NONE;
        break;
    }
}

void CALLBACK
left_click(AUX_EVENTREC *event)
{
    Object *prev_in_list;
    Object *sel_obj;

    if (picked_obj == NULL)
        return;

    // Pick object (already in picked_obj) and select. If Shift key down, add it to the selection.
    // If it is already selected, remove it from selection.
    if (picked_obj != NULL)
    {
        if (is_selected(curr_obj, &prev_in_list))
        {
            if (prev_in_list == NULL)
            {
                sel_obj = selection;
                selection = sel_obj->next;
            }
            else
            {
                sel_obj = prev_in_list->next;
                prev_in_list->next = sel_obj->next;
            }

            ASSERT(sel_obj->prev == curr_obj, "Selection list broken");
            free(sel_obj);

            if (selection != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection();
        }
        else
        {
            if (selection != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection();

            // select it
            sel_obj = obj_new();
            sel_obj->next = selection;
            selection = sel_obj;
            sel_obj->prev = curr_obj;
        }
    }
}

void CALLBACK
right_down(AUX_EVENTREC *event)
{
    SetCapture(auxGetHWND());
    right_mouseX = event->data[AUX_MOUSEX];
    right_mouseY = event->data[AUX_MOUSEY];
    right_mouse = TRUE;
}


void CALLBACK
right_up(AUX_EVENTREC *event)
{
    ReleaseCapture();
    right_mouse = FALSE;
}

void CALLBACK
right_click(AUX_EVENTREC *event)
{
}

void CALLBACK
mouse_wheel(AUX_EVENTREC *event)
{
    zoom_delta = event->data[AUX_MOUSESTATUS];
}

void CALLBACK
mouse_move(AUX_EVENTREC *event)
{
    // when moving the mouse, we may be highlighting objects.
    // We do this when we are about to add something to the tree.
    // Just having the function causes force redraws (TODO: TK/AUX may need to 
    // handle a return value to control this, if the drawing is complex)
}

// Process WM_COMMAND from TK window proc
int CALLBACK
Command(int wParam, int lParam)
{
    HMENU hMenu;
    OPENFILENAME ofn;

    switch (LOWORD(wParam))
    {
    case IDM_EXIT:
        if (drawing_changed)
        {
            int rc = MessageBox(auxGetHWND(), "File modified. Save it?", curr_filename, MB_YESNOCANCEL | MB_ICONWARNING);

            if (rc == IDCANCEL)
                break;
            else if (rc == IDYES)
                serialise_tree(object_tree, curr_filename);
        }

        purge_tree(object_tree);
        DestroyWindow(auxGetHWND());
        break;

    case ID_VIEW_TOOLS:
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        if (view_tools)
        {
            ShowWindow(hWndToolbar, SW_HIDE);
            view_tools = FALSE;
            CheckMenuItem(hMenu, ID_VIEW_TOOLS, MF_UNCHECKED);
        }
        else
        {
            ShowWindow(hWndToolbar, SW_SHOW);
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

    case ID_VIEW_TOP:
        facing_plane = &plane_XY;
        facing_index = PLANE_XY;
        Log("Facing plane XY\r\n");
        trackball_InitQuat(quat_XY);
        break;

    case ID_VIEW_FRONT:
        facing_plane = &plane_YZ;
        facing_index = PLANE_YZ;
        Log("Facing plane YZ\r\n");
        trackball_InitQuat(quat_YZ);
        break;

    case ID_VIEW_LEFT:
        facing_plane = &plane_XZ;
        facing_index = PLANE_XZ;
        Log("Facing plane XZ\r\n");
        trackball_InitQuat(quat_XZ);
        break;

    case ID_VIEW_BOTTOM:
        facing_plane = &plane_mXY;
        facing_index = PLANE_MINUS_XY;
        Log("Facing plane -XY\r\n");
        trackball_InitQuat(quat_mXY);
        break;

    case ID_VIEW_BACK:
        facing_plane = &plane_mYZ;
        facing_index = PLANE_MINUS_YZ;
        Log("Facing plane -YZ\r\n");
        trackball_InitQuat(quat_mYZ);
        break;

    case ID_VIEW_RIGHT:
        facing_plane = &plane_mXZ;
        facing_index = PLANE_MINUS_XZ;
        Log("Facing plane -XZ\r\n");
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

    case ID_FILE_NEW:
    case ID_FILE_OPEN:
        if (drawing_changed)
        {
            int rc = MessageBox(auxGetHWND(), "File modified. Save it?", curr_filename, MB_YESNOCANCEL | MB_ICONWARNING);

            if (rc == IDCANCEL)
                break;
            else if (rc == IDYES)
                serialise_tree(object_tree, curr_filename);
        }

        purge_tree(object_tree);
        object_tree = NULL;
        drawing_changed = FALSE;

        if (LOWORD(wParam) == ID_FILE_NEW)
            break;

        memset(&ofn, 0, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = auxGetHWND();
        ofn.lpstrFilter = "LoftyCAD Files\0*.LCD\0All Files\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFile = curr_filename;
        ofn.nMaxFile = 256;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
        if (GetOpenFileName(&ofn))
        {
            deserialise_tree(&object_tree, curr_filename);
            drawing_changed = FALSE;
            SetWindowText(auxGetHWND(), curr_filename);
        }

        break;

    case ID_FILE_SAVE:
        if (curr_filename[0] != '\0')
        {
            serialise_tree(object_tree, curr_filename);
            drawing_changed = FALSE;
            break;
        }
        // If no filename, fall through to save as...
    case ID_FILE_SAVEAS:
        memset(&ofn, 0, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = auxGetHWND();
        ofn.lpstrFilter = "LoftyCAD Files\0*.LCD\0All Files\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFile = curr_filename;
        ofn.nMaxFile = 256;
        ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
        if (GetSaveFileName(&ofn))
        {
            serialise_tree(object_tree, curr_filename);
            drawing_changed = FALSE;
            SetWindowText(auxGetHWND(), curr_filename);
        }

        break;
    }
    return 0;
}

// Put the icon in the button, and set up a tooltip for the button.
void
LoadAndDisplayIcon(HWND hWnd, int icon, int button, int toolstring)
{
    char buffer[256];
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(icon));
    SendDlgItemMessage(hWnd, button, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);
    HWND hWndButton = GetDlgItem(hWnd, button);

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
        //  LoadAndDisplayIcon(hWnd, IDI_BEZIER, IDB_BEZIER, IDS_BEZIER);
        //  LoadAndDisplayIcon(hWnd, IDI_ARC, IDB_ARC, IDS_ARC);
        //  LoadAndDisplayIcon(hWnd, IDI_MEASURE, IDB_MEASURE, IDS_MEASURE);

        // For now grey out unimplemented ones
        EnableWindow(GetDlgItem(hWnd, IDB_POINT), FALSE);
        EnableWindow(GetDlgItem(hWnd, IDB_CIRCLE), FALSE);
        EnableWindow(GetDlgItem(hWnd, IDB_CONST_EDGE), FALSE);
        EnableWindow(GetDlgItem(hWnd, IDB_CONST_RECT), FALSE);
        EnableWindow(GetDlgItem(hWnd, IDB_CONST_CIRCLE), FALSE);
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDB_EDGE:
                app_state = STATE_STARTING_EDGE;
                break;

            case IDB_RECT:
                app_state = STATE_STARTING_RECT;
                break;

            case IDB_CIRCLE:
                app_state = STATE_STARTING_CIRCLE;
                break;

            case IDB_XY:
                facing_plane = &plane_XY;
                facing_index = PLANE_XY;
                Log("Facing plane XY\r\n");
                trackball_InitQuat(quat_XY);
                break;   // TODO set focus back to main window so trackball starts working

            case IDB_YZ:
                facing_plane = &plane_YZ;
                facing_index = PLANE_YZ;
                Log("Facing plane YZ\r\n");
                trackball_InitQuat(quat_YZ);
                break;

            }
        }

        break;

    case WM_CLOSE:
        view_tools = FALSE;
        break;
    }

    return 0;
}

// Wndproc for debug log dialog. Contains one large edit box.
int WINAPI
debug_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDB_CLEARDEBUG:
                SendDlgItemMessage(hWndDebug, IDC_DEBUG, EM_SETSEL, 0, -1);
                SendDlgItemMessage(hWndDebug, IDC_DEBUG, EM_REPLACESEL, 0, (LPARAM)"");
                break;
            }
        }

        break;
    }

    return 0;
}


// The good old WinMain.
int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;
    HMENU hMenu;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_LOFTYCAD, szWindowClass, MAX_LOADSTRING);

        wWidth = wHeight = GetSystemMetrics(SM_CYFULLSCREEN);
        auxInitPosition(0, 0, wWidth, wHeight);
        auxInitDisplayMode(AUX_DEPTH16 | AUX_RGB | AUX_DOUBLE);
        auxInitWindow("LoftyCAD", TRUE, (HMENU)MAKEINTRESOURCE(IDC_LOFTYCAD), TRUE);
        Init();

        hInst = GetModuleHandle(NULL);

        auxExposeFunc((AUXEXPOSEPROC)Reshape);
        auxReshapeFunc((AUXRESHAPEPROC)Reshape);
        auxCommandFunc((AUXCOMMANDPROC)Command);

#if 0
        auxKeyFunc(AUX_z, Key_z); // zero viewing distance
        auxKeyFunc(AUX_r, Key_r); // results view
        auxKeyFunc(AUX_e, Key_e); // elements
        auxKeyFunc(AUX_n, Key_n); // normals
        auxKeyFunc(AUX_m, Key_m); // vel. magnitudes
        auxKeyFunc(AUX_v, Key_v); // vel. vectors
        auxKeyFunc(AUX_p, Key_p); // pressures
        auxKeyFunc(AUX_l, Key_l); // zlines
        auxKeyFunc(AUX_t, Key_t); // zones of turb/separation
        auxKeyFunc(AUX_UP, Key_up);	    // up zline
        auxKeyFunc(AUX_DOWN, Key_down);   // down zline
#endif
        auxMouseFunc(AUX_LEFTBUTTON, AUX_MOUSEDOWN, left_down);
        auxMouseFunc(AUX_LEFTBUTTON, AUX_MOUSEUP, left_up);
        auxMouseFunc(AUX_LEFTBUTTON, AUX_MOUSECLICK, left_click);

        auxMouseFunc(AUX_RIGHTBUTTON, AUX_MOUSEDOWN, right_down);
        auxMouseFunc(AUX_RIGHTBUTTON, AUX_MOUSEUP, right_up);
        auxMouseFunc(AUX_RIGHTBUTTON, AUX_MOUSECLICK, right_click);

        auxMouseFunc(AUX_MOUSEWHEEL, AUX_MOUSEWHEEL, mouse_wheel);
        auxMouseFunc(0, AUX_MOUSELOC, mouse_move);

        trackball_Init(wWidth, wHeight);

        auxIdleFunc(DrawCB);
        auxMainLoop(DrawCB);

        // Toolbar
        hWndToolbar = CreateDialog
            (
            hInst,
            MAKEINTRESOURCE(IDD_TOOLBAR),
            auxGetHWND(),
            toolbar_dialog
            );

        SetWindowPos(hWndToolbar, HWND_NOTOPMOST, wWidth, 0, 0, 0, SWP_NOSIZE);
        if (view_tools)
            ShowWindow(hWndToolbar, SW_SHOW);

        // Debug log
        hWndDebug = CreateDialog
            (
            hInst,
            MAKEINTRESOURCE(IDD_DEBUG),
            auxGetHWND(),
            debug_dialog
            );

        SetWindowPos(hWndDebug, HWND_NOTOPMOST, wWidth, wHeight/2, 0, 0, SWP_NOSIZE);
        if (view_debug)
            ShowWindow(hWndDebug, SW_SHOW);

        SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);

        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_DEBUGLOG, view_debug ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_ORTHO, view_ortho ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_PERSPECTIVE, !view_ortho ? MF_CHECKED : MF_UNCHECKED);

        hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LOFTYCAD));

        // Main message loop:
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!IsDialogMessage(hWndToolbar, &msg))
            {
                /*
                * Send accelerator keys straight to the 3D window, if it is in front.
                */
                if (msg.hwnd == auxGetHWND() || !TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
                {
                    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
        }
        return (int)msg.wParam;
}



// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
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
