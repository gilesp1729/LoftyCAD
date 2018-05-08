// Drawing functions

#ifndef __DRAW3D_H__
#define __DRAW3D_H__

// Presentation for draw3d. OR bits together as needed.
typedef enum
{
    DRAW_NONE = 0,
    DRAW_SELECTED = 1,
    DRAW_HIGHLIGHT = 2,
    DRAW_TOP_LEVEL_ONLY = 4,
    DRAW_HIGHLIGHT_LOCKED = 8
} PRESENTATION;

void color(OBJECT obj_type, BOOL selected, BOOL highlighted, BOOL locked);
void draw_object(Object *obj, PRESENTATION pres, LOCK parent_lock);

#endif // __DRAW3D_H__