// Drawing functions

#ifndef __DRAW3D_H__
#define __DRAW3D_H__

// Presentation for draw3d. OR bits together as needed.
typedef enum
{
    DRAW_NONE = 0,
    DRAW_SELECTED = 1,          // This object is selected. Color it and all its components.
    DRAW_HIGHLIGHT = 2,         // This object is highlighted because the mouse is hovering on it.
    DRAW_TOP_LEVEL_ONLY = 4,    // unused
    DRAW_HIGHLIGHT_LOCKED = 8,  // We're drawing, so we want to see the snap targets even if they are locked. But only under the mouse.
    DRAW_HIGHLIGHT_HALO = 16,   // This object is highlighted because it is in the halo.
    DRAW_WITH_DIMENSIONS = 32,  // Whether to show dimensions.
    DRAW_PATH = 64              // These edge(s) are part of the current path.
} PRESENTATION;

void SetMaterial(int mat, BOOL force_set);
void color(Object* obj, BOOL construction, PRESENTATION pres, BOOL locked);
void color_as(OBJECT obj_type, float color_decay, BOOL construction, PRESENTATION pres, BOOL locked);
void draw_object(Object *obj, PRESENTATION pres, LOCK parent_lock);
Face *text_face(Text *text, Face *f);
void CALLBACK Draw(void);
void invalidate_dl(void);
BOOL clipped(Point* p);
BOOL clippedv(float x, float y, float z);
BOOL is_bbox_clipped(Bbox* box);
BOOL is_tri_clipped(float x[3], float y[3], float z[3]); 
void draw_clip_intersection(Group* tree);

#endif // __DRAW3D_H__