#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Status bar and progress bar routines.

// Show status in the status bar. Set to blank strings to clear it.
void show_status(char* heading, char* string)
{
    char buf[128];

    strcpy_s(buf, 128, heading);
    strcat_s(buf, 128, string);
    SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)buf);
}

// Set the progress bar range (0-n) and its step to 1.
void set_progress_range(int n)
{
    SendMessage(hwndProg, PBM_SETRANGE, 0, MAKELPARAM(0, n));
    SendMessage(hwndProg, PBM_SETSTEP, 1, 0);
}

// Set the progress bar value
void set_progress(int n)
{
    SendMessage(hwndProg, PBM_SETPOS, n, 0);
}

// Increment the progress bar value
void bump_progress(void)
{
    SendMessage(hwndProg, PBM_STEPIT, 0, 0);
}

// Blank everything in the status bar
void clear_status_and_progress(void)
{
    SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)"");
    SendMessage(hwndProg, PBM_SETPOS, 0, 0);
}

// Accumulate the volume count for the given group for rendering, including those 
// below it (but leave out counts for groups with an operation not NONE and having
// valid meshes).
int accum_render_count(Group* tree)
{
    int count = 0;
    Object* obj;
    Volume* vol;
    Group* group;

    for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
    {
        switch (obj->type)
        {
        case OBJ_VOLUME:
            vol = (Volume*)obj;
            if (materials[vol->material].hidden)
                break;
            count++;
            break;

        case OBJ_GROUP:
            group = (Group*)obj;
            if (group->op != OP_NONE && group->mesh_valid)
                break;
            count += accum_render_count(group);
            break;
        }
    }

    return count;
}
