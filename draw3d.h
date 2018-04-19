// Drawing functions

#ifndef __DRAW3D_H__
#define __DRAW3D_H__

void show_hint_at(POINT pt, char *buf);
void hide_hint();
void color(OBJECT obj_type, BOOL selected, BOOL highlighted);
void draw_object(Object *obj, BOOL selected, BOOL highlighted);

#endif // __DRAW3D_H__