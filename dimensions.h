// Dimensions displaying functions

#ifndef __DIMS_H__
#define __DIMS_H__

typedef struct InitDims
{
    POINT pt;
    char *buf;
} InitDims;

void show_hint_at(POINT pt, char *buf, BOOL accept_input);
void hide_hint();
void process_messages(void);
BOOL has_dims(Object *obj);
void show_dims_on(Object *obj, PRESENTATION pres, LOCK parent_lock);
void show_dims_at(POINT pt, Object *obj, BOOL accept_input);

#endif // __DIMS_H__