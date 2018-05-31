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


GLint wWidth = 800, wHeight = 800;

// Mouse movements recorded here
int	left_mouseX, left_mouseY;
int	orig_left_mouseX, orig_left_mouseY;
int	right_mouseX, right_mouseY;
int key_status;
BOOL	left_mouse = FALSE;
BOOL	right_mouse = FALSE;

// Toolbars
HWND hWndToolbar;
HWND hWndDebug;
HWND hWndHelp;
HWND hWndTree;
HWND hWndDims;
BOOL view_tools = TRUE;
BOOL view_debug = FALSE;
BOOL view_help = TRUE;
BOOL view_tree = FALSE;

// State the app is in.
STATE app_state = STATE_NONE;

// TRUE when drawing a construction edge or other construction object.
BOOL construction = FALSE;

// A list of objects that are currently selected. The "prev" pointer points to 
// the actual object (it's a singly linked list). There is also a clipboard,
// which is arranged similarly.
Object *selection = NULL;
Object *clipboard = NULL;

// Top level group of objects to be drawn
Group object_tree = { 0, };

// Set TRUE whenever something is changed and the tree needs to be saved
BOOL drawing_changed = FALSE;

// The current object(s) being drawn (and not yet added to the object tree)
Object *curr_obj = NULL;

// The object that was picked when an action, such as drawing, was first
// started. Also the point where the pick occurred (in (x,y,z) coordinates)
Object *picked_obj = NULL;
Point picked_point;

// Starts at picked_point, but is updated throughout a move/drag.
Point last_point;

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
float quat_YZ[4] = { -0.5f, -0.5f, -0.5f, -0.5f };
float quat_XZ[4] = { -0.707f, 0, 0, -0.707f };
float quat_mXY[4] = { 0, 1, 0, 0 };
float quat_mYZ[4] = { -0.5f, 0.5f, 0.5f, -0.5f };
float quat_mXZ[4] = { 0, -0.707f, -0.707f, 0 };

Plane *facing_plane = &plane_XY;
PLANE facing_index = PLANE_XY;

// Viewing model (ortho or perspective)
BOOL view_ortho = FALSE;

// TRUE if viewing rendered representation
BOOL view_rendered = FALSE;

// TRUE to display construction edges
BOOL view_constr = TRUE;

// TRUE to display clipping of faces to volumes
BOOL view_clipped_faces = FALSE;

// Current filename
char curr_filename[256] = { 0, };

// Grid (for snapping points) and unit tolerance (for display of dims)
// When grid snapping is turned off, points are still snapped to the tolerance.
// grid_snap must be a power of 10; tolerance must be a power of 10, and less
// than or equal to the grid scale. (e.g. 1, 0.1)
float grid_snap = 1.0f;
float tolerance = 0.1f;

// Snapping tolerance, a bit more relaxed than the flatness tolerance
float snap_tol = 0.3f;

// log10(1.0 / tolerance)
int tol_log = 1;

// TRUE if snapping to grid (FALSE will snap to the tolerance)
BOOL snapping_to_grid = TRUE;

// Angular snap in degrees
int angle_snap = 15;

// TRUE if snapping to angle
BOOL snapping_to_angle = FALSE;

// Half-size of drawing volume, nominally in mm (although units are arbitrary)
float half_size = 100.0f;

// Initial values of translation components
float xTrans = 0;
float yTrans = 0;
float zTrans = -200;    // twice the initial half_size
int zoom_delta = 0;

// Clipboard paste offsets
float clip_xoffset, clip_yoffset, clip_zoffset;

// Undo (checkpoint) generation, the latest generation to be written, and the highest to be written
int generation = 0;
int latest_generation = 0;
int max_generation = 0;


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

    // Enable alpha blending, so we can have transparency
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);      // alpha blending
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);     // multiply blending. 
    glEnable(GL_BLEND);

    // For text annotations, horizontal characters start at 1000
    wglUseFontBitmaps(auxGetHDC(), 0, 256, 1000);

    plane_XY.C = 1.0;           // set up planes
    plane_XZ.B = 1.0;
    plane_YZ.A = 1.0;
    plane_mXY.C = -1.0;
    plane_mXZ.B = -1.0;
    plane_mYZ.A = -1.0;

    glEnable(GL_CULL_FACE);    // don't show back facing faces

    object_tree.hdr.type = OBJ_GROUP;  // set up object tree group
}

// Set up frustum and possibly picking matrix. If picking, pass the centre of the
// picking region and its width and height.
void CALLBACK
Position(BOOL picking, GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick)
{
    GLint viewport[4], width, height;
    float h, w, znear, zfar, zoom_factor;
    // char buf[64];

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glGetIntegerv(GL_VIEWPORT, viewport);
    width = viewport[2];
    height = viewport[3];
    if (picking)
        gluPickMatrix(x_pick, height - y_pick, w_pick, h_pick, viewport);  // Y window coords need inverting for GL

    znear = 0.5f * half_size;
    zfar = 50 * half_size;
    if (view_ortho)
    {
        zoom_factor = -0.5f * zTrans / half_size;
#ifdef DEBUG_POSITION_ZOOM
        sprintf_s(buf, 64, "Ortho Ztrans %f zoomf %f\r\n", zTrans, zoom_factor);
        Log(buf);
#endif
        if (width > height)
        {
            w = half_size * zoom_factor * (float)width / height;
            h = half_size * zoom_factor;
            glOrtho(-w, w, -h, h, znear, zfar);
        }
        else
        {
            w = half_size * zoom_factor;
            h = half_size * zoom_factor * (float)height / width;
            glOrtho(-w, w, -h, h, znear, zfar);
        }
        glTranslated(xTrans, yTrans, zTrans);
    }
    else
    {
        // In perspective mode, zooming is done more by narrowing the frustum
        // and less by moving back (zTrans)
        // TODO: make the near clipping plane less obvious in use
        zoom_factor = ((-0.5f * zTrans / half_size) - 1) * 0.5f + 0.4f;
#ifdef DEBUG_POSITION_ZOOM
        sprintf_s(buf, 64, "Persp Ztrans %f zoomf %f\r\n", zTrans, zoom_factor);
        Log(buf);
#endif
        if (width > height)
        {
            w = half_size * zoom_factor * (float)width / height;
            h = half_size * zoom_factor;
            glFrustum(-w, w, -h, h, znear, zfar);
        }
        else
        {
            w = half_size * zoom_factor;
            h = half_size * zoom_factor * (float)height / width;
            glFrustum(-w, w, -h, h, znear, zfar);
        }
        glTranslated(xTrans, yTrans, zTrans * 0.5f);
    }
}

// Change the proportions of the viewport when window is resized
void CALLBACK
Reshape(int width, int height)
{
    trackball_Resize(width, height);

    glViewport(0, 0, (GLint)width, (GLint)height);

    Position(FALSE, 0, 0, 0, 0);
}

// Pick an object: find the frontmost object under the cursor, or NULL if nothing is there.
// Pick objects from "min_priority" down in the hierarchy; e.g. faces/edges/points, or just edges/points.
// Only execption is that picking a face in a locked volume will pick the parent volume instead.
Object *
Pick(GLint x_pick, GLint y_pick, OBJECT min_priority)
{
    GLuint buffer[512];
    GLint num_hits;
    GLuint min_obj = 0;
    OBJECT priority = OBJ_MAX;
    Object *obj = NULL;

    // Find the object under the cursor, if any
    glSelectBuffer(512, buffer);
    glRenderMode(GL_SELECT);
    Draw(TRUE, x_pick, y_pick, 3, 3);
    num_hits = glRenderMode(GL_RENDER);

    if (num_hits > 0)
    {
        int n = 0;
        int i;
#ifdef DEBUG_PICK
        int j, len;
        char buf[512];
        size_t size = 512;
        char *p = buf;
        char *obj_prefix[] = { "N", "P", "E", "F", "V", "G" };
#endif
        GLuint min_depth = 0xFFFFFFFF;

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
                priority = OBJ_MAX;
            else
                priority = obj->type;

            if (priority < min_priority || buffer[n + 1] < min_depth)
            {
                min_depth = buffer[n + 1];
                min_priority = priority;
                min_obj = buffer[n + num_objs + 2];  // top of stack is last element in buffer
            }

#ifdef DEBUG_PICK_ALL
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
                Log(buf);
                p = buf;
                size = 512;
            }
            else  // not logging, still need to skip the data
#endif 
            {
                n += num_objs + 3;
            }
        }

        // Some special cases:
        // If the object is in a group, then return the group.
        // If the object is a face, but it belongs to a volume that is locked at the
        // face or volume level, then return the parent volume instead.
        // Do the face test first, as if we have its volume we can find any parent
        // group quickly without a full search.
        obj = (Object *)min_obj;
        if (obj != NULL)
        {
            if (obj->type == OBJ_FACE && ((Face *)obj)->vol != NULL)
            {
                Face *face = (Face *)obj;

                if (face->vol->hdr.lock >= LOCK_FACES)
                    obj = (Object *)face->vol;

                if (face->vol->hdr.parent_group->hdr.parent_group != NULL)
                    obj = find_top_level_parent(&object_tree, (Object *)face->vol->hdr.parent_group);  // this is fast
            }
            else
            {
                Object *parent = find_parent_object(&object_tree, obj, TRUE);               // this is not so fast
                if (parent != NULL && parent->type == OBJ_GROUP)
                    obj = parent;
            }
        }

#ifdef DEBUG_PICK
        if (view_debug)
        {
            if (obj == NULL)
                len = sprintf_s(p, size, "Picked: NULL\r\n");
            else
                len = sprintf_s(p, size, "Picked: %s%d\r\n", obj_prefix[obj->type], obj->ID);

            Log(buf);
        }
#endif
    }

    return obj;
}

// Pick all top-level objects intersecting the given rect. Return a list
// suitable for assigning to the selection.
Object * 
Pick_all_in_rect(GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick)
{
    GLuint buffer[4096];
    GLint num_hits;
    Object *obj = NULL;
    Object *list = NULL;
    Object *parent, *sel_obj;

    // Find the objects within the rect
    glSelectBuffer(4096, buffer);           // TODO: This may be too small. There are a lot of null hits.
    glRenderMode(GL_SELECT);
    Draw(TRUE, x_pick, y_pick, w_pick, h_pick);
    num_hits = glRenderMode(GL_RENDER);

    if (num_hits > 0)
    {
        int n = 0;
        int i;
#ifdef DEBUG_PICK
        int j, len;
        char buf[512];
        size_t size = 512;
        char *p = buf;
        char *obj_prefix[] = { "N", "P", "E", "F", "V" };
#endif

        for (i = 0; i < num_hits; i++)
        {
            int num_objs = buffer[n];

            if (num_objs == 0)
            {
                n += 3;  // skip count, min and max
                break;
            }

            // buffer = {{num objs, min depth, max depth, obj name, ...}, ...}

            // find top-of-stack and its parent
            obj = (Object *)buffer[n + num_objs + 2];
            if (obj != NULL)
            {
                parent = find_parent_object(&object_tree, obj, TRUE);  // Must be deep
                if (parent == NULL)
                {
                    n += num_objs + 3;
                    break;
                }

                // If parent is not already in the list, add it
                for (obj = list; obj != NULL; obj = obj->next)
                {
                    if (obj->prev == parent)
                        break;
                }

                if (obj == NULL)
                {
                    sel_obj = obj_new();
                    sel_obj->next = list;
                    list = sel_obj;
                    sel_obj->prev = parent;
                }

#ifdef DEBUG_PICK_ALL
                if (view_debug)
                {
                    len = sprintf_s(p, size, "(%d) objs %d min %x max %x: ", n, buffer[n], buffer[n + 1], buffer[n + 2]);
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
                    Log(buf);
                    p = buf;
                    size = 512;
                }
                else  // not logging, still need to skip the data
#endif
                {
                    n += num_objs + 3;
                }
            }
            else
            {
                n += num_objs + 3;
            }
        }

#ifdef DEBUG_PICK
        if (view_debug)
        {
            len = sprintf_s(p, size, "Select buffer used: %d\r\n", n);
            Log(buf);
        }
#endif
        return list;
    }

    return NULL;
}

// callback version of Draw for AUX/TK
void CALLBACK
DrawCB(void)
{
    Draw(FALSE, 0, 0, 0, 0);
}

// Find if an object is in the selection, returning TRUE if found, and
// a pointer to the _previous_ element (to aid deletion) if not the first.
BOOL
is_selected_direct(Object *obj, Object **prev_in_list)
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

#if 0 // not used any more
// Find if an object (or any of its parents) is in the selection.
BOOL
is_selected_parent(Object *obj)
{
    Object *sel, *parent;
    BOOL present = FALSE;

    for (sel = selection; sel != NULL; sel = sel->next)
    {
        if (sel->prev == obj)
        {
            present = TRUE;
            break;
        }

        // Make sure the object is not locked at the level of the thing being picked
        parent = find_parent_object(&object_tree, sel->prev, FALSE);
        if (find_obj(sel->prev, obj) && parent->lock < obj->type)
        {
            present = TRUE;
            break;
        }
    }

    return present;
}
#endif

// Clear the selection, or the clipboard.
void
clear_selection(Object **sel_list)
{
    Object *sel_obj, *next_obj;

    sel_obj = *sel_list;
    if (sel_obj == NULL)
        return;

    do
    {
        next_obj = sel_obj->next;
        free(sel_obj);              // TODO use a free list - these are tiny
        sel_obj = next_obj;
    } while (next_obj != NULL);

    *sel_list = NULL;
}

// Mouse handlers
void CALLBACK
left_down(AUX_EVENTREC *event)
{
    // If rendered, don't do anything here except orbit.
    if (view_rendered)
    {
        trackball_MouseDown(event);
        return;
    }

    // Find if there is an object under the cursor, and
    // also find if it is in the selection.
    picked_obj = Pick(event->data[0], event->data[1], OBJ_FACE);

    switch (app_state)
    {
    case STATE_NONE:
        if (event->data[AUX_MOUSESTATUS] & AUX_SHIFT)
        {
            // Starting a shift-drag. 
            SetCapture(auxGetHWND());
            orig_left_mouseX = left_mouseX = event->data[AUX_MOUSEX];
            orig_left_mouseY = left_mouseY = event->data[AUX_MOUSEY];
            left_mouse = TRUE;
            key_status = event->data[AUX_MOUSESTATUS];
            change_state(STATE_DRAGGING_SELECT);
        }
        else if (picked_obj == NULL)
        {
            // Orbiting the view
            trackball_MouseDown(event);
        }
        else
        {
            // Starting a move on an object, or on the selection
            SetCapture(auxGetHWND());
            left_mouseX = event->data[AUX_MOUSEX];
            left_mouseY = event->data[AUX_MOUSEY];
            left_mouse = TRUE;
            key_status = event->data[AUX_MOUSESTATUS];
            change_state(STATE_MOVING);

            // Set the picked point on the facing plane for later subtraction
            intersect_ray_plane(left_mouseX, left_mouseY, facing_plane, &picked_point);
            snap_to_grid(facing_plane, &picked_point);
            last_point = picked_point;
        }
        break;

    case STATE_STARTING_EDGE:
    case STATE_STARTING_RECT:
    case STATE_STARTING_CIRCLE:
    case STATE_STARTING_BEZIER:
    case STATE_STARTING_ARC:
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
        key_status = event->data[AUX_MOUSESTATUS];

        // Move into the corresponding drawing state
        change_state(app_state + STATE_DRAWING_OFFSET);

        if (picked_obj == NULL)
        {
            // Drawing on the standard axis.
            picked_plane = facing_plane;
            intersect_ray_plane(left_mouseX, left_mouseY, picked_plane, &picked_point);
            snap_to_grid(picked_plane, &picked_point);
        }
        else
        {
            picked_plane = NULL;

            // snap to a valid snap target and remember the (x,y,z) position of the
            // first point.
            switch (picked_obj->type)
            {
            case OBJ_FACE:
                // We're on a face, so we can proceed.
                picked_plane = &((Face *)picked_obj)->normal;
                intersect_ray_plane(left_mouseX, left_mouseY, picked_plane, &picked_point);
                snap_to_grid(picked_plane, &picked_point);
                break;

            case OBJ_EDGE:
                // Find a picked point in the edge (we don't have a plane yet) and snap
                // to the edge. If for some reason we can't, abandon the drawing operation.
                if (!snap_ray_edge(left_mouseX, left_mouseY, (Edge *)picked_obj, &picked_point))
                {
                    ReleaseCapture();
                    left_mouse = FALSE;
                    change_state(STATE_NONE);
                    trackball_MouseDown(event);
                }
                break;

            case OBJ_POINT:
                // Snap to the point. 
                picked_point = *(Point *)picked_obj;
                break;
            }
        }
        // Don't add the object yet until we have moved the mouse, as a click will
        // need to be handled harmlessly and silently.
        curr_obj = NULL;
        break;

    case STATE_DRAWING_RECT:
    case STATE_DRAWING_CIRCLE:
    case STATE_DRAWING_ARC:
    case STATE_DRAWING_BEZIER:
    case STATE_DRAWING_EXTRUDE:
        ASSERT(FALSE, "Mouse down in drawing state");
        break;
    }
}

void CALLBACK
left_up(AUX_EVENTREC *event)
{
    Face *rf;
    Edge *e;
    float mv[16], vec[4], nvec[4];
    float eye[4] = { 0, 0, 1, 0 };
#ifdef DEBUG_LEFT_UP_FACING
    char buf[256];
#endif

    switch (app_state)
    {
    case STATE_NONE:
        // We have orbited - calculate the facing plane again
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
#ifdef DEBUG_LEFT_UP_FACING
#ifdef DEBUG_LEFT_UP_MODELVIEW
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[0], mv[1], mv[2], mv[3]);
        Log(buf);
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[4], mv[5], mv[6], mv[7]);
        Log(buf);
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[8], mv[9], mv[10], mv[11]);
        Log(buf);
        sprintf_s(buf, 256, "%f %f %f %f\r\n", mv[12], mv[13], mv[14], mv[15]);
        Log(buf);
#endif
#endif
        mat_mult_by_row(mv, eye, nvec);
#ifdef DEBUG_LEFT_UP_FACING
        sprintf_s(buf, 256, "Eye: %f %f %f\r\n", nvec[0], nvec[1], nvec[2]);
        Log(buf);
#endif
        vec[0] = fabsf(nvec[0]);
        vec[1] = fabsf(nvec[1]);
        vec[2] = fabsf(nvec[2]);
        if (vec[0] > vec[1] && vec[0] > vec[2])
        {
            if (nvec[0] > 0)
            {
                facing_plane = &plane_YZ;
                facing_index = PLANE_YZ;
#ifdef DEBUG_LEFT_UP_FACING
                Log("Facing plane YZ\r\n");
#endif
            }
            else
            {
                facing_plane = &plane_mYZ;
                facing_index = PLANE_MINUS_YZ;
#ifdef DEBUG_LEFT_UP_FACING
                Log("Facing plane -YZ\r\n");
#endif
            }
        }
        else if (vec[1] > vec[0] && vec[1] > vec[2])
        {
            if (nvec[1] > 0)
            {
                facing_plane = &plane_XZ;
                facing_index = PLANE_XZ;
#ifdef DEBUG_LEFT_UP_FACING
                Log("Facing plane XZ\r\n");
#endif
            }
            else
            {
                facing_plane = &plane_mXZ;
                facing_index = PLANE_MINUS_XZ;
#ifdef DEBUG_LEFT_UP_FACING
                Log("Facing plane -XZ\r\n");
#endif
            }
        }
        else
        {
            if (nvec[2] > 0)
            {
                facing_plane = &plane_XY;
                facing_index = PLANE_XY;
#ifdef DEBUG_LEFT_UP_FACING
                Log("Facing plane XY\r\n");
#endif
            }
            else
            {
                facing_plane = &plane_mXY;
                facing_index = PLANE_MINUS_XY;
#ifdef DEBUG_LEFT_UP_FACING
                Log("Facing plane -XY\r\n");
#endif
            }
        }

        trackball_MouseUp(event);
        break;

    case STATE_DRAGGING_SELECT:
        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        break;

    case STATE_MOVING:
        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        drawing_changed = TRUE;  // TODO test for a real change (poss just a click)
        write_checkpoint(&object_tree, curr_filename);
        break;

    case STATE_DRAWING_RECT:
        if (curr_obj != NULL)
        {
            Point *p00, *p01, *p02, *p03;
            EDGE type;

            // Create the edges for the rect here as a special case, as the order
            // is not known till the mouse is released.
            // Generate four edges, and put them on the face's edge list.
            rf = (Face *)curr_obj;
            p00 = rf->view_list;
            p01 = (Point *)p00->hdr.next;
            p02 = (Point *)p01->hdr.next;
            p03 = (Point *)p02->hdr.next;

            type = EDGE_STRAIGHT | (construction ? EDGE_CONSTRUCTION : 0);
            e = (Edge *)edge_new(type);
            e->endpoints[0] = p00;
            e->endpoints[1] = p01;
            rf->edges[0] = e;
            e = (Edge *)edge_new(type);
            e->endpoints[0] = p01;
            e->endpoints[1] = p02;
            rf->edges[1] = e;
            e = (Edge *)edge_new(type);
            e->endpoints[0] = p02;
            e->endpoints[1] = p03;
            rf->edges[2] = e;
            e = (Edge *)edge_new(type);
            e->endpoints[0] = p03;
            e->endpoints[1] = p00;
            rf->edges[3] = e;

            // Take the points out of the face's view list, as they are about
            // to be freed when the view list is regenerated.
            rf->view_list = NULL;

            // the face now has its edges. Generate its view list and the normal
            rf->n_edges = 4;
            rf->view_valid = FALSE;
            gen_view_list_face(rf);
        }
        // fallthrough
    case STATE_DRAWING_EDGE:
    case STATE_DRAWING_CIRCLE:
    case STATE_DRAWING_BEZIER:
    case STATE_DRAWING_ARC:
        // add new object to the object tree
        if (curr_obj != NULL)
        {
            link_group(curr_obj, &object_tree);

            // Set its lock to EDGES for rect/circle faces, but no lock for edges (we will
            // very likely need to move the points soon)
            //if (!construction)
            {
                curr_obj->lock =
                    (app_state == STATE_DRAWING_RECT || app_state == STATE_DRAWING_CIRCLE) ? LOCK_EDGES : LOCK_NONE;
            }

            drawing_changed = TRUE;
            write_checkpoint(&object_tree, curr_filename);
            curr_obj = NULL;
        }

        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        construction = FALSE;
        hide_hint();
        break;

    case STATE_DRAWING_EXTRUDE:
        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        drawing_changed = TRUE;
        write_checkpoint(&object_tree, curr_filename);
        hide_hint();
        break;

    case STATE_STARTING_RECT:
    case STATE_STARTING_CIRCLE:
    case STATE_STARTING_ARC:
    case STATE_STARTING_BEZIER:
    case STATE_STARTING_EXTRUDE:
        ASSERT(FALSE, "Mouse up in starting state");
        curr_obj = NULL;
        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        break;
    }

    key_status = 0;
}

// Remove an object from the selection and return TRUE. If the object was
// not selected, return FALSE.
BOOL
remove_from_selection(Object *obj)
{
    Object *prev_in_list;
    Object *sel_obj;

    if (is_selected_direct(obj, &prev_in_list))
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

        ASSERT(sel_obj->prev == picked_obj, "Selection list broken");
        free(sel_obj);
        hide_hint();    // in case the dims was displayed
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void CALLBACK
left_click(AUX_EVENTREC *event)
{
    Object *sel_obj;
    Object *parent;

    hide_hint();

    // If rendering, don't do any of this
    if (view_rendered)
        return;

    // If we have just clicked on an arc centre, do nothing here
    if (app_state == STATE_STARTING_ARC)
        return;

    // Nothing picked.
    if (picked_obj == NULL)
        return;

    // We cannot select objects that are locked at their own level
    parent = find_parent_object(&object_tree, picked_obj, FALSE);
    if (parent->lock >= picked_obj->type)
        return;

    display_help("Selection");

    // Pick object (already in picked_obj) and select. 
    // If a double click, select its parent volume. 
    // If Shift key down, add it to the selection.
    // If it is already selected, remove it from selection.
    if (picked_obj != NULL)
    {
        if (event->data[AUX_MOUSESTATUS] & AUX_DBLCLK)
        {
            if (selection != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection(&selection);

            // test for this first, as we get called twice and don't want to unselect it
            // before the double click comes through
            parent = find_top_level_parent(&object_tree, picked_obj);
            if (parent != NULL)
            {
                picked_obj = parent;
                sel_obj = obj_new();
                sel_obj->next = selection;
                selection = sel_obj;
                sel_obj->prev = picked_obj;
            }
        }
        else if (remove_from_selection(picked_obj))
        {
            if (selection != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection(&selection);
        }
        else
        {
            if (selection != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection(&selection);

            // select it
            sel_obj = obj_new();
            sel_obj->next = selection;
            selection = sel_obj;
            sel_obj->prev = picked_obj;
        }
    }
}

// move the selection by a small amount
void
micro_move_selection(float x, float y)
{
    float dx, dy, dz;
    Object *obj;

    if (snapping_to_grid)
    {
        x *= grid_snap;
        y *= grid_snap;
    }
    else
    {
        x *= tolerance;
        y *= tolerance;
    }

    switch (facing_index)
    {
    case PLANE_XY:
        dx = x;
        dy = y;
        dz = 0;
        break;

    case PLANE_YZ:
        dx = 0;
        dy = x;
        dz = y;
        break;

    case PLANE_XZ:
        dx = -x;
        dy = 0;
        dz = y;
        break;

    case PLANE_MINUS_XY:
        dx = -x;
        dy = y;
        dz = 0;
        break;

    case PLANE_MINUS_YZ:
        dx = 0;
        dy = -x;
        dz = y;
        break;

    case PLANE_MINUS_XZ:
        dx = x;
        dy = 0;
        dz = y;
        break;
    }

    for (obj = selection; obj != NULL; obj = obj->next)
    {
        Object *parent;

        move_obj(obj->prev, dx, dy, dz);
        clear_move_copy_flags(obj->prev);

        // Invalidate all the view lists for the volume, as any of them may have changed
        parent = find_parent_object(&object_tree, obj->prev, FALSE);
        invalidate_all_view_lists(parent, obj->prev, dx, dy, dz);
    }
}

// U/D/L/R arrow keys move selection by one unit in the facing plane,
// where one unit is a multiple of grid snap, or of tolerance if snapping is
// turmed off.
void CALLBACK
left_arrow_key(void)
{
    micro_move_selection(-1, 0);
}

void CALLBACK
right_arrow_key(void)
{
    micro_move_selection(1, 0);
}

void CALLBACK
up_arrow_key(void)
{
    micro_move_selection(0, 1);
}

void CALLBACK
down_arrow_key(void)
{
    micro_move_selection(0, -1);
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

    // while here, store away the state of the shift key
    key_status = event->data[AUX_MOUSESTATUS];
}


// Change app state, displaying any help for the new state
void
change_state(STATE new_state)
{
    app_state = new_state;
    display_help_state(app_state);
}


// Preferences dialog.
int WINAPI
prefs_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char buf[16];

    switch (msg)
    {
    case WM_INITDIALOG:
        SendDlgItemMessage(hWnd, IDC_PREFS_TITLE, WM_SETTEXT, 0, (LPARAM)object_tree.title);
        sprintf_s(buf, 16, "%.0f", half_size);
        SendDlgItemMessage(hWnd, IDC_PREFS_HALFSIZE, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.1f", grid_snap);
        SendDlgItemMessage(hWnd, IDC_PREFS_GRID, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", tolerance);
        SendDlgItemMessage(hWnd, IDC_PREFS_TOL, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%d", angle_snap);
        SendDlgItemMessage(hWnd, IDC_PREFS_ANGLE, WM_SETTEXT, 0, (LPARAM)buf);
        SetFocus(GetDlgItem(hWnd, IDC_PREFS_TITLE));
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            SendDlgItemMessage(hWnd, IDC_PREFS_TITLE, WM_GETTEXT, 256, (LPARAM)object_tree.title);
            SendDlgItemMessage(hWnd, IDC_PREFS_HALFSIZE, WM_GETTEXT, 16, (LPARAM)buf);
            half_size = (float)atof(buf);
            SendDlgItemMessage(hWnd, IDC_PREFS_GRID, WM_GETTEXT, 16, (LPARAM)buf);
            grid_snap = (float)atof(buf);
            // TODO1 check grid scale and tolerance are powers of 10
            SendDlgItemMessage(hWnd, IDC_PREFS_TOL, WM_GETTEXT, 16, (LPARAM)buf);
            tolerance = (float)atof(buf);
            tol_log = (int)log10f(1.0f / tolerance);
            // TODO1 check angle snap divides 360
            SendDlgItemMessage(hWnd, IDC_PREFS_ANGLE, WM_GETTEXT, 16, (LPARAM)buf);
            angle_snap = atoi(buf);

            drawing_changed = TRUE;   // TODO test for a real change
            // Note: we can't undo this.

            EndDialog(hWnd, 1);
            break;

        case IDCANCEL:
            EndDialog(hWnd, 0);
        }
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
    POINT pt = { 0, 0 };

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_LOFTYCAD, szWindowClass, MAX_LOADSTRING);

        wWidth = wHeight = GetSystemMetrics(SM_CYFULLSCREEN);
        auxInitPosition(0, 0, wWidth, wHeight);
        auxInitDisplayMode(AUX_DEPTH16 | AUX_RGB | AUX_DOUBLE);
        auxInitWindow("LoftyCAD", TRUE, (HMENU)MAKEINTRESOURCE(IDC_LOFTYCAD), TRUE);
        Init();
        init_triangulator();

        hInst = GetModuleHandle(NULL);

        auxExposeFunc((AUXEXPOSEPROC)Reshape);
        auxReshapeFunc((AUXRESHAPEPROC)Reshape);
        auxCommandFunc((AUXCOMMANDPROC)Command);
        auxDestroyFunc((AUXDESTROYPROC)check_file_changed);

#if 0
        auxKeyFunc(AUX_ESCAPE, escape_key);
#endif
        auxKeyFunc(AUX_LEFT, left_arrow_key);
        auxKeyFunc(AUX_RIGHT, right_arrow_key);
        auxKeyFunc(AUX_UP, up_arrow_key);
        auxKeyFunc(AUX_DOWN, down_arrow_key);

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

        // help window
        hWndHelp = init_help_window();

        // Tree view of object tree
        hWndTree = CreateDialog
            (
            hInst,
            MAKEINTRESOURCE(IDD_TREEVIEW),
            auxGetHWND(),
            treeview_dialog
            );

        SetWindowPos(hWndTree, HWND_NOTOPMOST, wWidth, wHeight / 2, 0, 0, SWP_NOSIZE);
        if (view_tree)
            ShowWindow(hWndTree, SW_SHOW);

        // Dimensions window. It looks like a tooltip, but displays and allows input of dimensions.
        // It is used both modeless (as here) and also modal (when typing in dimensions)
        hWndDims = CreateDialogParam
            (
            hInst,
            MAKEINTRESOURCE(IDD_DIMENSIONS),
            auxGetHWND(),
            dimensions_dialog,
            (LPARAM)NULL
            );

        SetWindowPos(hWndDims, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE);
        ShowWindow(hWndDims, SW_HIDE);

        // Bring main window back to the front
        SetWindowPos(auxGetHWND(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);

        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
        CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOGRID, snapping_to_grid ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOANGLE, snapping_to_angle ? MF_CHECKED : MF_UNCHECKED);

        // recent files list
        hMenu = GetSubMenu(hMenu, 9);  // zero-based item position, separators count
        //AppendMenu(hMenu, MF_STRING, 0, "New file here"); 
        load_MRU_to_menu(hMenu);

        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_DEBUGLOG, view_debug ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_HELP, view_help ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_TREE, view_tree ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_ORTHO, view_ortho ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_PERSPECTIVE, !view_ortho ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_CONSTRUCTIONEDGES, view_constr ? MF_CHECKED : MF_UNCHECKED);

        // Display help for the resting state
        change_state(STATE_NONE);


        hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LOFTYCAD));

        // Main message loop:
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (!IsDialogMessage(hWndToolbar, &msg) && !IsDialogMessage(hWndDims, &msg))
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

