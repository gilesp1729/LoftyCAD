// Drawing functions

#ifndef __DRAW3D_H__
#define __DRAW3D_H__

// Presentation for draw3d. OR bits together as needed.
typedef enum
{
    DRAW_NONE = 0,
    DRAW_SELECTED = 1,
    DRAW_HIGHLIGHT = 2,
    DRAW_TOP_LEVEL_ONLY = 4,    // unused
    DRAW_HIGHLIGHT_LOCKED = 8,
    DRAW_HIGHLIGHT_HALO = 16,
    DRAW_WITH_DIMENSIONS = 32,
    DRAW_PATH = 64,
    DRAW_PICKING = 128          // unused
} PRESENTATION;

void SetMaterial(int mat, BOOL force_set);
void color(Object* obj, BOOL construction, PRESENTATION pres, BOOL locked);
void color_as(OBJECT obj_type, float color_decay, BOOL construction, PRESENTATION pres, BOOL locked);
void draw_object(Object *obj, PRESENTATION pres, LOCK parent_lock);
Face *text_face(Text *text, Face *f);
void CALLBACK Draw(void);
void invalidate_dl(void);

#endif // __DRAW3D_H__