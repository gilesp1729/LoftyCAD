#pragma once

#include <GL/gl.h>
#include <GL/glu.h>
#include "glaux/glaux.h"
#include <math.h>
#include <stdio.h>
#include "resource.h"
#include "trackbal.h"
#include "objtree.h"
#include "geometry.h"
#include "draw3d.h"
#include "dimensions.h"
#include "registry.h"
#include "triangulate.h"

// States the app can be in.
typedef enum STATE
{
    STATE_NONE,                 // Default state when orbiting with the trackball, panning or zooming
    STATE_MOVING,               // Moving an object, or the selection
    STATE_DRAGGING_SELECT,      // Dragging out a selection window

    // Starting states are entered when a toolbar button is pressed.
    // In starting states, snapping targets are displayed at the mouse position.
    // (TODO - snapping things while moving, this is not at the mouse pos. so very different)
    STATE_STARTING_EDGE,        // Starting to draw (toolbar button pressed, but mouse not yet down)
    STATE_STARTING_RECT,
    STATE_STARTING_CIRCLE,
    STATE_STARTING_BEZIER,
    STATE_STARTING_ARC,
    STATE_STARTING_MEASURE,
    STATE_STARTING_EXTRUDE,

    STATE_DRAWING_EDGE,         // Actually drawing something (left mouse down and dragging)
    STATE_DRAWING_RECT,         // NOTE: These must be in the same order as the starting states
    STATE_DRAWING_CIRCLE,
    STATE_DRAWING_BEZIER,
    STATE_DRAWING_ARC,
    STATE_DRAWING_MEASURE,
    STATE_DRAWING_EXTRUDE,

    // The offset between members of the above two sets of possible states.
    STATE_DRAWING_OFFSET = STATE_DRAWING_EDGE - STATE_STARTING_EDGE
} STATE;

// Externs
extern HINSTANCE hInst;

extern GLint wWidth, wHeight;
extern float xTrans, yTrans, zTrans;
extern int zoom_delta;

extern int	left_mouseX, left_mouseY;
extern int	orig_left_mouseX, orig_left_mouseY;
extern int	right_mouseX, right_mouseY;
extern int key_status;
extern BOOL	left_mouse;
extern BOOL	right_mouse;

extern HWND hWndToolbar;
extern HWND hWndDebug;
extern HWND hWndDims;
extern BOOL view_tools;
extern BOOL view_debug;
extern BOOL view_rendered;

extern STATE app_state;
extern Object *selection;
extern Object *clipboard;
extern Object *object_tree;
extern BOOL drawing_changed;
extern Object *curr_obj;
extern Object *picked_obj;
extern Point picked_point;
extern Point last_point;
extern Plane *picked_plane;
extern Plane *facing_plane;
extern PLANE facing_index;

extern char curr_title[];
extern float grid_snap;
extern float tolerance;
extern int tol_log;
extern int angle_snap;
extern BOOL snapping_to_grid;
extern BOOL snapping_to_angle;
extern float half_size;
extern float clip_xoffset, clip_yoffset, clip_zoffset;
extern int generation;
extern int latest_generation;
extern int max_generation;


// Debug stuff
#define Log(msg)            SendDlgItemMessage(hWndDebug, IDC_DEBUG, EM_REPLACESEL, 0, (LPARAM)(msg));

#define LogShow(msg)        \
    {                       \
        ShowWindow(hWndDebug, SW_SHOW);                                                     \
        CheckMenuItem(GetSubMenu(GetMenu(auxGetHWND()), 2), ID_VIEW_DEBUGLOG, MF_CHECKED);  \
        view_debug = TRUE;                                                                  \
        SendDlgItemMessage(hWndDebug, IDC_DEBUG, EM_REPLACESEL, 0, (LPARAM)(msg));          \
    }

#define ASSERT(exp, msg)    { if (!(exp)) LogShow(msg) }

// Debugging defines
#undef DEBUG_SHADE_CYL_FACE
#undef DEBUG_DRAW_RECT_NORMAL
#undef DEBUG_POSITION_ZOOM
#define DEBUG_PICK
#define DEBUG_PICK_ALL
#undef DEBUG_LEFT_UP_FACING
#undef DEBUG_LEFT_UP_MODELVIEW
#undef DEBUG_COMMAND_FACING
#undef DEBUG_TOOLBAR_FACING
#undef DEBUG_REVERSE_RECT_FACE
#undef DEBUG_VIEW_LIST_ARC

// Some forwards
Object * Pick(GLint x_pick, GLint y_pick, OBJECT obj_type);
Object * Pick_all_in_rect(GLint x_pick, GLint y_pick, GLint width, GLint height);
void CALLBACK Draw(BOOL picking, GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick);
void CALLBACK Position(BOOL picking, GLint x_pick, GLint y_pick, GLint w_pick, GLint h_pick);
void display_help(char *key);
void change_state(STATE new_state);
BOOL is_selected_parent(Object *obj);
void clear_selection(Object **sel_list);

// Forwards for window procedures
int WINAPI debug_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI toolbar_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI dimensions_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI prefs_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
