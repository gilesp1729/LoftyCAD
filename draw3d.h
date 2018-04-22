// Drawing functions

#ifndef __DRAW3D_H__
#define __DRAW3D_H__

void show_hint_at(POINT pt, char *buf);
void hide_hint();
void color(OBJECT obj_type, BOOL selected, BOOL highlighted, BOOL locked);
void draw_object(Object *obj, BOOL selected, BOOL highlighted, LOCK parent_lock);

#endif // __DRAW3D_H__