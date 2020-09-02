// LoftyCAD.c: Defines the entry point for the application.
//

#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>
#include <shellapi.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);


GLint wWidth = 800, wHeight = 800;
int toolbar_bottom;

// Mouse movements recorded here
int	left_mouseX, left_mouseY;
int	orig_left_mouseX, orig_left_mouseY;
int	right_mouseX, right_mouseY;
int key_status;
BOOL	left_mouse = FALSE;
BOOL	right_mouse = FALSE;

// Toolbars
HWND hWndPropSheet;
HWND hWndToolbar;
HWND hWndPrintPreview;
HWND hWndDebug;
HWND hWndHelp;
HWND hWndTree;
HWND hWndDims;
HWND hwndStatus;
HWND hwndProg;
BOOL view_tools = TRUE;
BOOL view_debug = FALSE;
BOOL view_help = TRUE;
BOOL view_tree = FALSE;

// Micro moving (with arrow keys)
BOOL micro_moved = FALSE;

// State the app is in.
STATE app_state = STATE_NONE;

// TRUE when drawing a construction edge or other construction object.
BOOL construction = FALSE;

// A list of objects that are currently selected. The "prev" pointer points to 
// the actual object (it's a singly linked list). There is also a clipboard,
// which is arranged similarly.
ListHead selection = { NULL, NULL };
ListHead clipboard = { NULL, NULL };

// A list head that saves an old object tree when the clipboard still has 
// references to it
ListHead saved_list = { NULL, NULL };

// Top level group of objects to be drawn
Group object_tree = { 0, };

// Top level group of objects to be drawn in the printer (G-code) view.
Group gcode_tree = { 0, };

// Set TRUE whenever something is changed and the tree needs to be saved
BOOL drawing_changed = FALSE;

// The current object(s) being drawn (and not yet added to the object tree)
Object *curr_obj = NULL;

// The object that was picked when an action, such as drawing, was first
// started. Also the point where the pick occurred (in (x,y,z) coordinates) and where it was dragged to.
Object *picked_obj = NULL;
Point picked_point;
Point new_point;

// When picked_obj is a volume or group due to locking, this is the underlying face.
Object *raw_picked_obj = NULL;

// Starts at picked_point, but is updated throughout a move/drag.
Point last_point;

// The plane that picked_obj lies in; or else the facing plane if there is none.
Plane *picked_plane = NULL;

// The plane through the centre of an object's bbox, parallel to the facing plane.
Plane centre_facing_plane;

// The current text structure, either a new one about to be assigned to a face, or on a face being edited.
Text *curr_text;

// The current path (an edge or an edge group) used for rotating bodies of revolution, cloning, or lofting.
Object* curr_path = NULL;

// Dominant direction for a scaling operation
SCALED scaled_dirn;
SCALED scaled;

// Total angle and effective angle accumulator for a rotation operation
float total_angle;
float effective_angle;


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

// TRUE if viewing printer preview (sliced objects)
BOOL view_printer = FALSE;

// TRUE to display the print bed dimensions with the axes (in any view)
BOOL view_printbed = FALSE;

// TRUE to display construction edges
BOOL view_constr = TRUE;

// TRUE to display halo faces when highlighting
BOOL view_halo = FALSE;

// Halo radius
float halo_rad = 50;

// Current filename
char curr_filename[256] = { 0, };

// Print bed dimensions defaults
float bed_xmin = 0;
float bed_ymin = 0;
float bed_xmax = 200;
float bed_ymax = 200;
float layer_height = 0.3f;

// Printer view Z min and max for G-code visualisation
float print_zmin = 0;
float print_zmax = 9999;

// Grid (for snapping points) and unit tolerance (for display of dims)
// When grid snapping is turned off, points are still snapped to the tolerance.
// grid_snap should be a power of 10; tolerance must be less
// than or equal to the grid scale. (e.g. 1, 0.1)
#define INITIAL_GRID 1.0f
#define INITIAL_TOL 0.1f
#define INITIAL_HALFSIZE 100.0f
float grid_snap = INITIAL_GRID;
float tolerance = INITIAL_TOL;

// Perspective zTrans adjustment
#define ZOOM_Z_SCALE 0.25f

// Snapping tolerance, a bit more relaxed than the flatness tolerance
float snap_tol = 3 * INITIAL_TOL;

// log10(1.0 / tolerance)
int tol_log = 1;

// TRUE if snapping to grid (FALSE will snap to the tolerance)
BOOL snapping_to_grid = TRUE;

// Angular snap in degrees
int angle_snap = 15;

// TRUE if snapping to angle
BOOL snapping_to_angle = FALSE;

// Size ("radius") of chamfer. It must be slightly larger than snap_tol.
float chamfer_rad = 3.5f * INITIAL_TOL;

// Radius of rounded corners.
float round_rad = 2 * INITIAL_GRID;

// Half-size of drawing volume, nominally in mm (although units are arbitrary)
float half_size = INITIAL_HALFSIZE;

// Initial values of translation components
float xTrans = 0;
float yTrans = 0;
float zTrans = -2.0f * INITIAL_HALFSIZE;
int zoom_delta = 0;

// Clipboard paste offsets
float clip_xoffset, clip_yoffset, clip_zoffset;

// Undo (checkpoint) generation, the latest generation to be written, and the highest to be written
int generation = 0;
int latest_generation = 0;
int max_generation = 0;

// Debugging options
BOOL debug_view_bbox = FALSE;
BOOL debug_view_normals = FALSE;
BOOL debug_view_viewlist = FALSE;

// Size and span of point-searching buckets used in coordinate matching.
// Chosen so that there are 40 buckets covering +/-halfsize, in X and Y (we don't bucket on Z)
// If these change, n_buckets must be even.
float bucket_size = INITIAL_GRID * 5;
int n_buckets = 40;

BlendMode view_blend = BLEND_MULTIPLY;

void
Init(void)
{
    static GLint colorIndexes[3] = { 0, 200, 255 };
    static float ambient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    static float diffuse[] = { 0.5f, 1.0f, 1.0f, 1.0f };
    static float position[] = { 90.0f, 90.0f, 150.0f, 0.0f };
    static float lmodel_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static float lmodel_twoside[] = { GL_TRUE };
    // HFONT hFont;

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

    materials[0].valid = TRUE;
    strcpy_s(materials[0].name, 64, "(default)");
    SetMaterial(0);

    // Enable alpha blending, so we can have transparency
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);      // alpha blending
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);     // multiply blending. 
    glEnable(GL_BLEND);

    // For text annotations, horizontal characters start at 1000, outlines at 2000
    wglUseFontBitmaps(auxGetHDC(), 0, 256, 1000);
#if 0
    hFont = CreateFont(48, 0, 0, 0, FW_DONTCARE, FALSE, TRUE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Arial"));
    SelectObject(auxGetHDC(), hFont);
    wglUseFontOutlines(auxGetHDC(), 0, 256, 2000, 0, 0, WGL_FONT_POLYGONS, NULL);
#endif

    plane_XY.C = 1.0;           // set up planes
    plane_XZ.B = 1.0;
    plane_YZ.A = 1.0;
    plane_mXY.C = -1.0;
    plane_mXZ.B = -1.0;
    plane_mYZ.A = -1.0;

    picked_point.hdr.type = OBJ_POINT;
    new_point.hdr.type = OBJ_POINT;

    glEnable(GL_CULL_FACE);    // don't show back facing faces

    object_tree.hdr.type = OBJ_GROUP;  // set up object tree groups
    gcode_tree.hdr.type = OBJ_GROUP;
    gcode_tree.hdr.lock = LOCK_GROUP;
}

// Set up frustum and possibly picking matrix. If picking, pass the centre of the
// picking region and its width and height.
void CALLBACK
Position(BOOL picking, GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick)
{
    GLint viewport[4], width, height;
    float h, w, znear, zfar, zoom_factor;
#ifdef DEBUG_POSITION_ZOOM
    char buf[64];
#endif

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
        glTranslated(xTrans, yTrans, -2.0f * half_size);
    }
    else
    {
        // In perspective mode, zooming is done more by narrowing the frustum.
        // Don't back off to zTrans, as you always hit the near clipping plane
        zoom_factor = (-0.5f * zTrans / half_size) * ZOOM_Z_SCALE;
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
        glTranslated(xTrans, yTrans, -2.0f * half_size);
    }
}

// Change the proportions of the viewport when window is resized
void CALLBACK
Reshape(int width, int height)
{
    SendMessage(hwndStatus, WM_SIZE, 0, 0);     // redraw the status bar
    trackball_Resize(width, height);
    glViewport(0, 0, (GLint)width, (GLint)height);
    Position(FALSE, 0, 0, 0, 0);
}


// Pick an object: find the frontmost object under the cursor, or NULL if nothing is there.
// Pick objects from "min_priority" down in the hierarchy; e.g. faces/edges/points, or just edges/points.
// Only execption is that picking a face in a locked volume will pick the parent volume instead.
// Setting force_pick ensures that even locked objects can be picked (use for the context menu)
Object *
Pick(GLint x_pick, GLint y_pick, BOOL force_pick)
{
    GLuint buffer[512];
    GLint num_hits;
    GLuint min_obj = 0;
    OBJECT priority = OBJ_MAX;
    Object *obj = NULL;
    OBJECT min_priority = OBJ_MAX;
    BOOL edit_in_groups = FALSE;        // This is very problematical wrt. moving objects or sub-components.

    // Find the object under the cursor, if any
    glSelectBuffer(512, buffer);
    glRenderMode(GL_SELECT);
    Draw(TRUE, x_pick, y_pick, 3, 3);
    num_hits = glRenderMode(GL_RENDER);
    raw_picked_obj = NULL;

   // if (num_hits < 0)
   //     DebugBreak();
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
        GLuint depth = 0xFFFFFFFF;

        for (i = 0; i < num_hits; i++)
        {
            int num_objs = buffer[n];

            if (num_objs == 0)
            {
                n += 3;         // skip count, min and max
                continue;       // see if the next hit has anything for us
            }

            // buffer = {{num objs, min depth, max depth, obj name, ...}, ...}

            // find top-of-stack and what kind of object it is
            obj = (Object *)buffer[n + num_objs + 2];
            if (obj == NULL)
            {
                // no object worth picking
                priority = OBJ_MAX;
            }
            else
            {
                priority = obj->type;
                depth = buffer[n + 1];

                // special case: if we are in STATE_NONE and we have a face on a fully locked volume,
                // just look straight through it so we can pick things behind it. However, we must still
                // be able to right-click things, so don't do it if force_pick is set TRUE.
                if 
                (
                    !force_pick 
                    &&
                    obj->type == OBJ_FACE 
                    && 
                    ((Face *)obj)->vol != NULL 
                    && 
                    ((Face *)obj)->vol->hdr.lock >= LOCK_VOLUME
                )
                {
                    raw_picked_obj = obj;
                    obj = NULL;
                    priority = OBJ_MAX;
                    depth = 0xFFFFFFFF;
                }
            }

            // store the lowest priority object, closest to the viewer
            if (priority < min_priority || depth < min_depth)
            {
                min_depth = buffer[n + 1];
                min_priority = priority;
                min_obj = (GLuint)obj; //  buffer[n + num_objs + 2];  // top of stack is last element in buffer
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

                raw_picked_obj = obj;
                if (face->vol->hdr.lock >= LOCK_FACES)
                    obj = (Object *)face->vol;

                if (!edit_in_groups && face->vol->hdr.parent_group->hdr.parent_group != NULL)
                    obj = find_top_level_parent(&object_tree, (Object *)face->vol->hdr.parent_group);  // this is fast
                if (obj->lock >= obj->type && !force_pick)
                    obj = NULL; // this object is locked
            }
            else
            {
                Object *parent = find_top_level_parent(&object_tree, obj);               // this is not so fast
                if (!edit_in_groups && parent != NULL && parent->type == OBJ_GROUP)
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

// Pick all top-level objects intersecting the given rect and select.
void 
Pick_all_in_rect(GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick)
{
    GLuint buffer[4096];
    GLint num_hits;
    Object *obj = NULL;
    Object *list = NULL;
    Object *parent;

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
                continue;
            }

            // buffer = {{num objs, min depth, max depth, obj name, ...}, ...}

            // find top-of-stack and its parent
            obj = (Object *)buffer[n + num_objs + 2];
            if (obj != NULL)
            {
                parent = find_top_level_parent(&object_tree, obj);
                if (parent == NULL)
                {
                    n += num_objs + 3;
                    continue;
                }

                // If parent is not already in the list, add it
                if (obj != NULL)
                    link_single_checked(parent, &selection);

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
    }
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
    for (sel = selection.head; sel != NULL; sel = sel->next)
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


// Clear the selection, or the clipboard.   TODO - replace this with a plain old call to free_obj_list.
void
clear_selection(ListHead *sel_list)
{
    free_obj_list(sel_list);
}

// When something has changed: mark drawing as changed, write a checkpoint, and update the clipped surface.
void
update_drawing(void)
{
    drawing_changed = TRUE;
    invalidate_dl();
    write_checkpoint(&object_tree, curr_filename);
    xform_list.head = NULL;
    xform_list.tail = NULL;
    gen_view_list_tree_volumes(&object_tree);
    populate_treeview();
}

// Mouse handlers
void CALLBACK
left_down(AUX_EVENTREC *event)
{
    Point d1;
    Volume *vol = NULL;

    // If viewing rendered or the printer, don't do anything here except orbit.
    if (view_rendered || view_printer)
    {
        trackball_MouseDown(event);
        return;
    }

    // Terminate a series of micro moves
    if (micro_moved)
    {
        update_drawing();
        micro_moved = FALSE;
    }

    // Find if there is an object under the cursor, and
    // also find if it is in the selection.
    picked_obj = Pick(event->data[0], event->data[1], FALSE);

    switch (app_state)
    {
    case STATE_NONE:
        if ((event->data[AUX_MOUSESTATUS] & AUX_SHIFT) && picked_obj == NULL)
        {
            // Starting a shift-drag. You must start outside any object (otherwise it is a shift-move)
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
            snap_to_grid(facing_plane, &picked_point, FALSE);
            last_point = picked_point;
        }
        break;

    case STATE_STARTING_EDGE:
    case STATE_STARTING_RECT:
    case STATE_STARTING_CIRCLE:
    case STATE_STARTING_BEZIER:
    case STATE_STARTING_ARC:
    case STATE_STARTING_EXTRUDE:
    case STATE_STARTING_EXTRUDE_LOCAL:
    case STATE_STARTING_TEXT:
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
            snap_to_grid(picked_plane, &picked_point, FALSE);
        }
        else
        {
            picked_plane = NULL;

            // snap to a valid snap target and remember the (x,y,z) position of the
            // first point.
            switch (picked_obj->type)
            {
            case OBJ_GROUP:
            case OBJ_VOLUME:
                // We are on a volume or group, and the faces are locked; 
                // access the underlying face if we have one.
                if (raw_picked_obj != NULL && raw_picked_obj->type == OBJ_FACE)
                {
                    picked_plane = &((Face *)raw_picked_obj)->normal;
                    intersect_ray_plane(left_mouseX, left_mouseY, picked_plane, &picked_point);
                    snap_to_grid(picked_plane, &picked_point, FALSE);
                    picked_obj = raw_picked_obj;
                }
                break;

            case OBJ_FACE:
                // We're on a face, so we can proceed.
                picked_plane = &((Face *)picked_obj)->normal;
                intersect_ray_plane(left_mouseX, left_mouseY, picked_plane, &picked_point);
                snap_to_grid(picked_plane, &picked_point, FALSE);
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

    case STATE_STARTING_SCALE:
    case STATE_STARTING_ROTATE:
        // Determine the facing plane through the centroid of the object.
        // This is used as picked_plane. Project the mouse down position
        // back to this plane and use that as picked_point. If the object is 
        // not a volume or group, we want the parent.
        SetCapture(auxGetHWND());

        // Remember the initial mouse click so we can test for gross movement later
        left_mouseX = event->data[AUX_MOUSEX];
        left_mouseY = event->data[AUX_MOUSEY];
        left_mouse = TRUE;
        key_status = event->data[AUX_MOUSESTATUS];

        if (picked_obj == NULL)
        {
            ReleaseCapture();
            left_mouse = FALSE;
            change_state(STATE_NONE);
            trackball_MouseDown(event);
            break;
        }

        // Find the object (or its parent) 
        switch (picked_obj->type)
        {
        case OBJ_FACE:
        case OBJ_EDGE:
        case OBJ_POINT:
            // Find the parent
            picked_obj = find_top_level_parent(&object_tree, picked_obj);
            if (picked_obj->type == OBJ_FACE)
            {
                // We're rotating or scaling a face in-place.
                vol = NULL;
                centre_facing_plane = *facing_plane;
                centroid_face((Face*)picked_obj, &centre_facing_plane.refpt);
                intersect_ray_plane(left_mouseX, left_mouseY, &centre_facing_plane, &picked_point);
                break;
            }
            else if (picked_obj->type < OBJ_FACE)
            {
                // Edges not supported for in-place
                ReleaseCapture();
                left_mouse = FALSE;
                change_state(STATE_NONE);
                trackball_MouseDown(event);
                break;
            }
            // fall through for volumes/groups
        case OBJ_GROUP:  
        case OBJ_VOLUME:
            // Note group and volume have box immediately following header (mandatory)
            vol = (Volume *)picked_obj;
            centre_facing_plane = *facing_plane;
            centre_facing_plane.refpt.x = vol->bbox.xc;
            centre_facing_plane.refpt.y = vol->bbox.yc;
            centre_facing_plane.refpt.z = vol->bbox.zc;
            intersect_ray_plane(left_mouseX, left_mouseY, &centre_facing_plane, &picked_point);
            break;
        }

        // For scaling, determine the dominant direction (it doesn't change during the
        // scaling operation). Drop out if we have coincident points and can't get an
        // angle or a dominant direction.
        if (nz(length(&picked_point, &centre_facing_plane.refpt)))
        {
            ReleaseCapture();
            left_mouse = FALSE;
            change_state(STATE_NONE);
            trackball_MouseDown(event);
            break;
        }

        // Initialise some stuff used by both scaling and rotation
        switch (facing_index)
        {
        case PLANE_XY:
        case PLANE_MINUS_XY:
            total_angle = 0;
            if (vol != NULL && vol->xform != NULL)
                total_angle = vol->xform->rz;
            effective_angle = total_angle;
            d1.x = fabsf(picked_point.x - centre_facing_plane.refpt.x);
            d1.y = fabsf(picked_point.y - centre_facing_plane.refpt.y);
            if (d1.x > d1.y)
                scaled_dirn = DIRN_X;
            else
                scaled_dirn = DIRN_Y;
            break;

        case PLANE_XZ:
        case PLANE_MINUS_XZ:
            if (vol != NULL && vol->xform != NULL)
                total_angle = vol->xform->ry;
            effective_angle = total_angle;
            d1.x = fabsf(picked_point.x - centre_facing_plane.refpt.x);
            d1.z = fabsf(picked_point.z - centre_facing_plane.refpt.z);
            if (d1.x > d1.z)
                scaled_dirn = DIRN_X;
            else
                scaled_dirn = DIRN_Z;
            break;

        case PLANE_YZ:
        case PLANE_MINUS_YZ:
            if (vol != NULL && vol->xform != NULL)
                total_angle = vol->xform->rx;
            effective_angle = total_angle;
            d1.y = fabsf(picked_point.y - centre_facing_plane.refpt.y);
            d1.z = fabsf(picked_point.z - centre_facing_plane.refpt.z);
            if (d1.y > d1.z)
                scaled_dirn = DIRN_Y;
            else
                scaled_dirn = DIRN_Z;
            break;
        }

        // Move into the corresponding drawing state
        change_state(app_state + STATE_DRAWING_OFFSET);

        break;

    case STATE_DRAWING_RECT:
    case STATE_DRAWING_CIRCLE:
    case STATE_DRAWING_ARC:
    case STATE_DRAWING_BEZIER:
    case STATE_DRAWING_EXTRUDE:
    case STATE_DRAWING_EXTRUDE_LOCAL:
    case STATE_DRAWING_TEXT:
    case STATE_DRAWING_SCALE:
    case STATE_DRAWING_ROTATE:
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
        // Treat this as a row matrix, to get the inverse of any rotation in it
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
        update_drawing();
        break;

    case STATE_MOVING:
        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        update_drawing();
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
            p00 = (Point *)rf->view_list.head;
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
            rf->view_list.head = NULL;
            rf->view_list.tail = NULL;

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
    case STATE_DRAWING_TEXT:
        // add new object to the object tree
        if (curr_obj != NULL)
        {
            link_group(curr_obj, &object_tree);

            // Set its lock to EDGES for closed (rect/circle/text) faces, but no lock for edges (we will
            // very likely need to move the points soon)
            //if (!construction)
            {
                curr_obj->lock =
                    (app_state == STATE_DRAWING_RECT || app_state == STATE_DRAWING_CIRCLE || app_state == STATE_DRAWING_TEXT) ? LOCK_EDGES : LOCK_NONE;
            }

            update_drawing();
            curr_obj = NULL;
        }

        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        construction = FALSE;
        hide_hint();
        break;

    case STATE_DRAWING_EXTRUDE:
    case STATE_DRAWING_EXTRUDE_LOCAL:
    case STATE_DRAWING_SCALE:
    case STATE_DRAWING_ROTATE:
        ReleaseCapture();
        left_mouse = FALSE;
        change_state(STATE_NONE);
        update_drawing();
        hide_hint();
        curr_obj = NULL;
        break;

    case STATE_STARTING_RECT:
    case STATE_STARTING_CIRCLE:
    case STATE_STARTING_ARC:
    case STATE_STARTING_BEZIER:
    case STATE_STARTING_EXTRUDE_LOCAL:
    case STATE_STARTING_TEXT:
    case STATE_STARTING_SCALE:
    case STATE_STARTING_ROTATE:
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
            sel_obj = selection.head;
            selection.head = sel_obj->next;
            if (selection.head == NULL)
                selection.tail = NULL;
        }
        else
        {
            sel_obj = prev_in_list->next;
            prev_in_list->next = sel_obj->next;
            if (prev_in_list->next == NULL)
                selection.tail = prev_in_list;
        }

        ASSERT(sel_obj->prev == picked_obj, "Selection list broken");
        sel_obj->next = free_list_obj.head;  // put it in the free list
        if (free_list_obj.head == NULL)
            free_list_obj.tail = sel_obj;
        free_list_obj.head = sel_obj;

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
    Object *parent;

    hide_hint();

    // If rendering, don't do any of this
    if (view_rendered || view_printer)
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
            if (selection.head != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection(&selection);

            // test for this first, as we get called twice and don't want to unselect it
            // before the double click comes through
            parent = find_top_level_parent(&object_tree, picked_obj);
            if (parent != NULL)
            {
                picked_obj = parent;
                link_single(picked_obj, &selection);
            }
        }
        else if (remove_from_selection(picked_obj))
        {
            if (selection.head != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection(&selection);
        }
        else
        {
            if (selection.head != NULL && (event->data[AUX_MOUSESTATUS] & AUX_SHIFT) == 0)
                clear_selection(&selection);

            // select it
            link_single(picked_obj, &selection);
        }
        update_drawing();
    }
}

// move the selection by a small amount
void
micro_move_selection(float x, float y, BOOL inhibit_snap)
{
    float dx, dy, dz;
    Object *obj;

    if (snapping_to_grid && !inhibit_snap)
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

    for (obj = selection.head; obj != NULL; obj = obj->next)
    {
        Object *parent;

        move_obj(obj->prev, dx, dy, dz);

        // Invalidate all the view lists for the volume, as any of them may have changed
        parent = find_parent_object(&object_tree, obj->prev, FALSE);
        invalidate_all_view_lists(parent, obj->prev, dx, dy, dz);
    }

    // clear the flags separately, to stop double-moving of shared points
    for (obj = selection.head; obj != NULL; obj = obj->next)
        clear_move_copy_flags(obj->prev);

    micro_moved = TRUE;
    invalidate_dl();
}

// U/D/L/R arrow keys move selection by one unit in the facing plane,
// where one unit is a multiple of grid snap, or of tolerance if snapping is
// turmed off.
void CALLBACK
left_arrow_key(int status)
{
    micro_move_selection(-1, 0, status & AUX_CONTROL);
}

void CALLBACK
right_arrow_key(int status)
{
    micro_move_selection(1, 0, status & AUX_CONTROL);
}

void CALLBACK
up_arrow_key(int status)
{
    micro_move_selection(0, 1, status & AUX_CONTROL);
}

void CALLBACK
down_arrow_key(int status)
{
    micro_move_selection(0, -1, status & AUX_CONTROL);
}

void CALLBACK
right_down(AUX_EVENTREC *event)
{
    if (micro_moved)
    {
        update_drawing();
        micro_moved = FALSE;
    }

    SetCapture(auxGetHWND());
    right_mouseX = event->data[AUX_MOUSEX];
    right_mouseY = event->data[AUX_MOUSEY];
    right_mouse = TRUE;
    suppress_drawing = TRUE;        // prepare for menu to be displayed, hold highlight
}


void CALLBACK
right_up(AUX_EVENTREC *event)
{
    ReleaseCapture();
    right_mouse = FALSE;
    suppress_drawing = FALSE;
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

    // get rid of any highlights from the tree view
    treeview_highlight = NULL;

    // allow drawing if panning
    if (right_mouse)
        suppress_drawing = FALSE;
}


// Preferences dialog.
int WINAPI
prefs_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char buf[16];
    float new_val;

    switch (msg)
    {
    case WM_INITDIALOG:
        SendDlgItemMessage(hWnd, IDC_PREFS_TITLE, WM_SETTEXT, 0, (LPARAM)object_tree.title);
        sprintf_s(buf, 16, "%.0f", half_size);
        SendDlgItemMessage(hWnd, IDC_PREFS_HALFSIZE, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", grid_snap);
        SendDlgItemMessage(hWnd, IDC_PREFS_GRID, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", tolerance);
        SendDlgItemMessage(hWnd, IDC_PREFS_TOL, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%d", angle_snap);
        SendDlgItemMessage(hWnd, IDC_PREFS_ANGLE, WM_SETTEXT, 0, (LPARAM)buf);
        sprintf_s(buf, 16, "%.2f", round_rad);
        SendDlgItemMessage(hWnd, IDC_PREFS_ROUNDRAD, WM_SETTEXT, 0, (LPARAM)buf);
        SetFocus(GetDlgItem(hWnd, IDC_PREFS_TITLE));
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            SendDlgItemMessage(hWnd, IDC_PREFS_TITLE, WM_GETTEXT, 256, (LPARAM)object_tree.title);

            SendDlgItemMessage(hWnd, IDC_PREFS_HALFSIZE, WM_GETTEXT, 16, (LPARAM)buf);
            half_size = (float)atof(buf);
            zTrans = -2.0f * half_size;
            Position(FALSE, 0, 0, 0, 0);

            SendDlgItemMessage(hWnd, IDC_PREFS_TOL, WM_GETTEXT, 16, (LPARAM)buf);
            new_val = (float)atof(buf);
            if(!nz(new_val - tolerance))
            {
                // The snapping tol and chamfer rad are fixed fractions of the tolerance. Don't change them.
                snap_tol = 3 * tolerance;
                chamfer_rad = 3.5f * tolerance;
                tol_log = (int)ceilf(log10f(1.0f / tolerance));

                // Fix up all the flattened curves.
                adjust_stepsizes((Object *)&object_tree, new_val);
                clear_move_copy_flags((Object*)&object_tree);
                invalidate_all_view_lists((Object *)&object_tree, (Object *)&object_tree, 0, 0, 0);

                tolerance = new_val;
                drawing_changed = TRUE;

                if (view_rendered)
                {
                    // regenerate surface mesh, in case we're viewing rendered
                    xform_list.head = NULL;
                    xform_list.tail = NULL;
                    if (object_tree.mesh != NULL)
                        mesh_destroy(object_tree.mesh);
                    object_tree.mesh = NULL;
                    object_tree.mesh_valid = FALSE;
                    gen_view_list_tree_volumes(&object_tree);
                    gen_view_list_tree_surfaces(&object_tree, &object_tree);
                }
            }

            // These don't change the drawing until something else is added.
            SendDlgItemMessage(hWnd, IDC_PREFS_GRID, WM_GETTEXT, 16, (LPARAM)buf);
            grid_snap = (float)atof(buf);
            SendDlgItemMessage(hWnd, IDC_PREFS_ANGLE, WM_GETTEXT, 16, (LPARAM)buf);
            angle_snap = atoi(buf);
            SendDlgItemMessage(hWnd, IDC_PREFS_ROUNDRAD, WM_GETTEXT, 16, (LPARAM)buf);
            round_rad = (float)atof(buf);

            EndDialog(hWnd, drawing_changed);
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

	MSG msg;
	HACCEL hAccelTable;
    HMENU hMenu;
    POINT pt = { 0, 0 };
    RECT rect;
    int parts[3];
    PROPSHEETPAGE psp[2];
    PROPSHEETHEADER psh;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_LOFTYCAD, szWindowClass, MAX_LOADSTRING);

        wWidth = GetSystemMetrics(SM_CYFULLSCREEN);
        wHeight = wWidth - GetSystemMetrics(SM_CYMENU);  // leave a little room for the status bar
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
#if 0
        // Toolbar, and get the bottom to position other windows against
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
        GetWindowRect(hWndToolbar, &rect);
        toolbar_bottom = rect.bottom;

        // Print bed view toolbar
        hWndPrintPreview = CreateDialog
        (
            hInst,
            MAKEINTRESOURCE(IDD_PRINT_PREVIEW),
            auxGetHWND(),
            printer_dialog
        );

        SetWindowPos(hWndPrintPreview, HWND_NOTOPMOST, wWidth, 0, 0, 0, SWP_NOSIZE);
        ShowWindow(hWndPrintPreview, SW_HIDE);
#else
        // Property sheet for the tools and preview tools
        psp[0].dwSize = sizeof(PROPSHEETPAGE);
        psp[0].dwFlags = PSP_USETITLE;
        psp[0].hInstance = hInst;
        psp[0].pszTemplate = MAKEINTRESOURCE(IDD_TOOLBAR);
        psp[0].pszIcon = NULL;
        psp[0].pfnDlgProc = toolbar_dialog;
        psp[0].pszTitle = "Drawing Tools";
        psp[0].lParam = 0;
        psp[0].pfnCallback = NULL;
        
        psp[1].dwSize = sizeof(PROPSHEETPAGE);
        psp[1].dwFlags = PSP_USETITLE;
        psp[1].hInstance = hInst;
        psp[1].pszTemplate = MAKEINTRESOURCE(IDD_PRINT_PREVIEW);
        psp[1].pszIcon = NULL;
        psp[1].pfnDlgProc = printer_dialog;
        psp[1].pszTitle = "Printer Preview";
        psp[1].lParam = 0;
        psp[1].pfnCallback = NULL;

        psh.dwSize = sizeof(PROPSHEETHEADER);
        psh.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
        psh.hwndParent = auxGetHWND();
        psh.hInstance = hInst;
        psh.pszIcon = NULL;
        psh.pszCaption = "Tools";
        psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
        psh.nStartPage = 0;
        psh.ppsp = (LPCPROPSHEETPAGE)&psp;
        psh.pfnCallback = NULL;

        hWndPropSheet = (HWND)PropertySheet(&psh);
        SetWindowPos(hWndPropSheet, HWND_NOTOPMOST, wWidth, 0, 0, 0, SWP_NOSIZE);
        ShowWindow(GetDlgItem(hWndPropSheet, IDOK), SW_HIDE);
        ShowWindow(GetDlgItem(hWndPropSheet, IDCANCEL), SW_HIDE);
        if (view_tools)
            ShowWindow(hWndPropSheet, SW_SHOW);
        GetWindowRect(hWndPropSheet, &rect);
        toolbar_bottom = rect.bottom;
#endif

        // Debug log
        hWndDebug = CreateDialog
        (
            hInst,
            MAKEINTRESOURCE(IDD_DEBUG),
            auxGetHWND(),
            debug_dialog
        );

        SetWindowPos(hWndDebug, HWND_NOTOPMOST, wWidth, toolbar_bottom, 0, 0, SWP_NOSIZE);
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

        SetWindowPos(hWndTree, HWND_NOTOPMOST, wWidth + 150, 0, 0, 0, SWP_NOSIZE);
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

        // Status bar goes at the bottom (and moves with the window - WM_SIZE with 0,0 redraws)
        hwndStatus = CreateWindow
        (
            STATUSCLASSNAME, 
            NULL, 
            WS_CHILD | WS_VISIBLE, 
            0, wHeight - 10, wWidth, 10, 
            auxGetHWND(), 
            NULL, 
            hInst, 
            0
        );
        parts[0] = 250;
        parts[1] = 450;
        parts[2] = -1;
        SendMessage(hwndStatus, SB_SETPARTS, 3, (LPARAM)parts);
        ShowWindow(hwndStatus, SW_SHOW);

        // Progress bar: a child of the status bar, so it stays together and on top.
        // Put it in the second part of the status bar
        GetClientRect(hwndStatus, &rect);
        rect.top += 2;
        hwndProg = CreateWindow
        (
            PROGRESS_CLASS, 
            NULL,
            WS_CHILD | WS_VISIBLE,
            250, rect.top, 200, rect.bottom - rect.top,
            hwndStatus,
            NULL,
            hInst,
            0
        );
        ShowWindow(hwndProg, SW_SHOW);

        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
        CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOGRID, snapping_to_grid ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_PREFERENCES_SNAPTOANGLE, snapping_to_angle ? MF_CHECKED : MF_UNCHECKED);

        // recent files list
        hMenu = GetSubMenu(hMenu, 9);  // zero-based item position, separators count
        //AppendMenu(hMenu, MF_STRING, 0, "New file here"); 
        load_MRU_to_menu(hMenu);

        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_TOOLS, view_tools ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_HELP, view_help ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_TREE, view_tree ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_ORTHO, view_ortho ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_PERSPECTIVE, !view_ortho ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_BLEND_MULTIPLY, view_blend == BLEND_MULTIPLY ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_BLEND_ALPHA, view_blend == BLEND_ALPHA ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_BLEND_OPAQUE, view_blend == BLEND_OPAQUE ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_CONSTRUCTIONEDGES, view_constr ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_VIEW_DEBUGLOG, view_debug ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_DEBUG_BBOXES, debug_view_bbox ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_DEBUG_NORMALS, debug_view_normals ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(hMenu, ID_DEBUG_VIEWLIST, debug_view_viewlist ? MF_CHECKED : MF_UNCHECKED);
#ifndef DEBUG_HIGHLIGHTING_ENABLED
        EnableMenuItem(hMenu, ID_DEBUG_BBOXES, MF_GRAYED);
        EnableMenuItem(hMenu, ID_DEBUG_NORMALS, MF_GRAYED);
#endif

        // Display help for the resting state
        change_state(STATE_NONE);

        // Open any file passed on the cmd line
        if (lpCmdLine[0] != '\0')
        {
            char window_title[256];
            int lens, start;

            // Strip quotes.
            start = 0;
            if (lpCmdLine[0] == '"')
                start = 1;
            strcpy_s(curr_filename, 256, &lpCmdLine[start]);
            lens = strlen(curr_filename) - 1;
            if (curr_filename[lens] == '"')
                curr_filename[lens] = '\0';

            deserialise_tree(&object_tree, curr_filename, FALSE);
            strcpy_s(window_title, 256, curr_filename);
            strcat_s(window_title, 256, " - ");
            strcat_s(window_title, 256, object_tree.title);
            SetWindowText(auxGetHWND(), window_title);
            hMenu = GetSubMenu(GetMenu(auxGetHWND()), 0);
            hMenu = GetSubMenu(hMenu, 9);
            insert_filename_to_MRU(hMenu, curr_filename);
            xform_list.head = NULL;
            xform_list.tail = NULL;
            gen_view_list_tree_volumes(&object_tree);
            populate_treeview();
        }

        DragAcceptFiles(auxGetHWND(), TRUE);

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

