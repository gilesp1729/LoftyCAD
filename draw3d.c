#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Some standard colors sent to GL.
void
color(OBJECT obj_type, BOOL selected, BOOL highlighted)
{
    float r, g, b;

    switch (obj_type)
    {
    case OBJ_VOLUME:
    case OBJ_POINT:
        return;  // no action here

    case OBJ_EDGE:
        r = g = b = 0.3f;
        if (selected)
            r += 0.4f;
        if (highlighted)
            g += 0.4f;
        break;

    case OBJ_FACE:
        r = g = b = 0.8f;
        if (selected)
            r += 0.2f;
        if (highlighted)
            g += 0.2f;
        break;
    }
    glColor3f(r, g, b);
}

// Shade in a face.
void
face_shade(Face *face, BOOL selected, BOOL highlighted)
{
    Point   *v;

    gen_view_list(face);
    glBegin(GL_POLYGON);
    color(OBJ_FACE, selected, highlighted);
    glNormal3f(face->normal.A, face->normal.B, face->normal.C);
    for (v = face->view_list; v != NULL; v = (Point *)v->hdr.next)
        glVertex3f(v->x, v->y, v->z);
    glEnd();
}

// Draw any object.
void
draw_object(Object *obj, BOOL selected, BOOL highlighted)
{
    int i;
    Face *face;
    Edge *edge;
    StraightEdge *se;

    switch (obj->type)
    {
    case OBJ_POINT:
        // Only if selected/highlighted.
        if (selected)
        {
            // glPushName((GLuint)obj);
            // TODO

            // glPopName();

        }
        break;

    case OBJ_EDGE:
        glPushName((GLuint)obj);
        edge = (Edge *)obj;
        switch (edge->type)
        {
        case EDGE_STRAIGHT:
            se = (StraightEdge *)edge;
            glBegin(GL_LINES);
            color(OBJ_EDGE, selected, highlighted);
            glVertex3f(se->endpoints[0]->x, se->endpoints[0]->y, se->endpoints[0]->z);
            glVertex3f(se->endpoints[1]->x, se->endpoints[1]->y, se->endpoints[1]->z);
            glEnd();
            glPopName();
            // TODO draw points after popping
            break;

        case EDGE_CIRCLE:
        case EDGE_ARC:
        case EDGE_BEZIER:
            break;
        }
        break;

    case OBJ_FACE:
        glPushName((GLuint)obj);
        face = (Face *)obj;
        face_shade(face, selected, highlighted);
        glPopName();
        for (i = 0; i < face->n_edges; i++)
            draw_object((Object *)face->edges[i], selected, highlighted);
        break;

    case OBJ_VOLUME:
        for (face = ((Volume *)obj)->faces; face != NULL; face = (Face *)face->hdr.next)
            draw_object((Object *)face, selected, highlighted);
        break;
    }
}

// Draw the contents of the main window. Everything happens in here.
void CALLBACK
Draw(BOOL picking, GLint x_pick, GLint y_pick)
{
    float matRot[4][4];
    POINT   pt;
    Object  *obj;
    BOOL highlit;

    if (!picking)
    {
        // handle mouse movement actions.
        // Highlight snap targets (use curr_obj for this)
        if (!left_mouse && !right_mouse)
        {
            auxGetMouseLoc(&pt.x, &pt.y);
            curr_obj = Pick(pt.x, pt.y, OBJ_FACE);
        }

        // handle left mouse dragging actions. We must be moving or drawing,
        // otherwise the trackball would have it and we wouldn't be here.
        if (left_mouse)
        {
            auxGetMouseLoc(&pt.x, &pt.y);

            // Use XYZ coordinates rather than mouse position deltas, as we may
            // have snapped and we want to preserve the accuracy. Just use mouse
            // position to check for gross movement.
            if (pt.x != left_mouseX || pt.y != left_mouseY)
            {
                Point   new_point, p1, p3;
                Point   *p00, *p01, *p02, *p03;
                StraightEdge *se;
                Face *rf;
                Plane norm;
                char buf[64];
                POINT winpt;

                switch (app_state)
                {
                case STATE_MOVING:
                    // Move the selection by a delta in XYZ within the facing plane
                    // TODO: We need some locking/constraints, otherwise we can mess things up.
                    intersect_ray_plane(pt.x, pt.y, facing_plane, &new_point);
                    for (obj = selection; obj != NULL; obj = obj->next)
                    {
                        move_obj
                            (
                            obj->prev,
                            new_point.x - picked_point.x,
                            new_point.y - picked_point.y,
                            new_point.z - picked_point.z
                            );
                        clear_move_copy_flags(obj->prev);

                        // If we have moved a face:
                        // Invalidate all the view lists for the volume, as any of them may have changed
                        // TODO - do this by finding the ultimate parent, so it works for points, edges, etc.
                        if (obj->prev->type == OBJ_FACE)
                        {
                            Face *face = (Face *)obj->prev;

                            if (face->vol != NULL)
                            {
                                Face *f;

                                for (f = face->vol->faces; f != NULL; f = (Face *)f->hdr.next)
                                    f->view_valid = FALSE;
                            }
                        }
                    }

                    picked_point = new_point;

                    break;

                case STATE_DRAWING_EDGE:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that.


                    }

                    // Move the end point of the current edge
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);

                    // If first move, create the edge here.
                    if (curr_obj == NULL)
                    {
                        curr_obj = (Object *)edge_new(EDGE_STRAIGHT);
                        se = (StraightEdge *)curr_obj;
                        // TODO: share points if snapped onto an existing edge endpoint,
                        // and the edge is not referenced by a face. For now, just create points...
                        se->endpoints[0] = point_newp(&picked_point);
                        se->endpoints[1] = point_newp(&new_point);
                    }
                    else
                    {
                        se = (StraightEdge *)curr_obj;
                        se->endpoints[1]->x = new_point.x;
                        se->endpoints[1]->y = new_point.y;
                        se->endpoints[1]->z = new_point.z;
                    }

                    // Show the dimensions (length) of the edge.
                    sprintf_s(buf, 64, "%f mm", length(picked_point.x, picked_point.y, picked_point.z, new_point.x, new_point.y, new_point.z));
                    SendDlgItemMessage(hWndDims, IDC_DIMENSIONS, WM_SETTEXT, 0, (LPARAM)buf);
                    winpt.x = pt.x;
                    winpt.y = pt.y;
                    ClientToScreen(auxGetHWND(), &winpt);
                    SetWindowPos(hWndDims, HWND_TOPMOST, winpt.x + 10, winpt.y + 20, 0, 0, SWP_NOSIZE);
                    ShowWindow(hWndDims, SW_SHOW);

                    break;

                case STATE_DRAWING_RECT:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that.


                    }

                    // Move the opposite corner point
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);

                    // generate the other corners. The rect goes in the 
                    // order picked-p1-new-p3.
                    switch (facing_index)
                    {
                    case PLANE_XY:
                    case PLANE_MINUS_XY:
                        p1.x = new_point.x;
                        p1.y = picked_point.y;
                        p1.z = picked_point.z;
                        p3.x = picked_point.x;
                        p3.y = new_point.y;
                        p3.z = picked_point.z;
                        break;

                    case PLANE_YZ:
                    case PLANE_MINUS_YZ:
                        p1.x = picked_point.x;
                        p1.y = new_point.y;
                        p1.z = picked_point.z;
                        p3.x = picked_point.x;
                        p3.y = picked_point.y;
                        p3.z = new_point.z;
                        break;

                    case PLANE_XZ:
                    case PLANE_MINUS_XZ:
                        p1.x = new_point.x;
                        p1.y = picked_point.y;
                        p1.z = picked_point.z;
                        p3.x = picked_point.x;
                        p3.y = picked_point.y;
                        p3.z = new_point.z;
                        break;

                    case PLANE_GENERAL:
                        ASSERT(FALSE, "Not implemented yet");
                        break;
                    }

                    // Make sure the normal vector is pointing towards the eye,
                    // swapping p1 and p3 if necessary.
                    normal3(&p1, &picked_point, &p3, &norm);
                    {
                        char buf[256];
                        sprintf_s(buf, 256, "%f %f %f\r\n", norm.A, norm.B, norm.C);
                        Log(buf);
                    }
                    if (dot(norm.A, norm.B, norm.C, facing_plane->A, facing_plane->B, facing_plane->C) < 0)
                    {
                        Point swap = p1;
                        p1 = p3;
                        p3 = swap;
                    }

                    // If first move, create the rect and its edges here.
                    // Create a special rect with no edges but a 4-point view list.
                    // Only create the edges when completed, as we have to keep the anticlockwise
                    // order of the points no matter how the mouse is dragged around.
                    if (curr_obj == NULL)
                    {
                        rf = face_new(FACE_RECT, *picked_plane);

                        // generate four points for the view list
                        p00 = point_newp(&picked_point);
                        p01 = point_newp(&p1);
                        p02 = point_newp(&new_point);
                        p03 = point_newp(&p3);

                        // put the points into the view list
                        rf->view_list = rf->initial_point = p00;
                        p00->hdr.next = (Object *)p01;
                        p01->hdr.next = (Object *)p02;
                        p02->hdr.next = (Object *)p03;

                        // set it valid, so Draw doesn't try to overwrite it
                        rf->view_valid = TRUE;

                        curr_obj = (Object *)rf;
                    }
                    else
                    {
                        rf = (Face *)curr_obj;

                        // Dig out the points that need updating, and update them
                        p01 = (Point *)rf->view_list->hdr.next;
                        p02 = (Point *)p01->hdr.next;
                        p03 = (Point *)p02->hdr.next;

                        // Update the points
                        p01->x = p1.x;
                        p01->y = p1.y;
                        p01->z = p1.z;
                        p02->x = new_point.x;
                        p02->y = new_point.y;
                        p02->z = new_point.z;
                        p03->x = p3.x;
                        p03->y = p3.y;
                        p03->z = p3.z;
                    }

                    break;

                case STATE_DRAWING_CIRCLE:
                case STATE_DRAWING_ARC:
                case STATE_DRAWING_BEZIER:
                case STATE_DRAWING_MEASURE:
                    ASSERT(FALSE, "Not implemented");
                    break;

                case STATE_DRAWING_EXTRUDE:
                    if (picked_obj != NULL && picked_obj->type == OBJ_FACE)
                    {
                        Plane proj_plane = *facing_plane;
                        Face *face = (Face *)picked_obj;
                        float length;

                        // Can we extrude this face?
                        if (face->type == FACE_CYLINDRICAL || face->type == FACE_GENERAL)
                            break;

                        // See if we need to create a volume first, otherwise just move the face
                        if (face->vol == NULL)
                        {
                            int i;
                            Face *opposite, *side;
                            Edge *e, *o;
                            StraightEdge *se;
                            Point *eip, *oip;
                            Volume *vol;

                            delink((Object *)face, &object_tree);
                            vol = vol_new(NULL);
                            link((Object *)face, (Object **)&vol->faces);
                            face->vol = vol;
                            face->view_valid = FALSE;

                            switch (face->type)
                            {
                            case FACE_RECT:
                                // Clone the face with coincident edges/points, but in the
                                // opposite sense (and with an opposite normal)
                                opposite = clone_face_reverse(face);
                                clear_move_copy_flags(picked_obj);
                                link((Object *)opposite, (Object **)&vol->faces);
                                opposite->vol = vol;

                                // Create faces that link the picked face to its clone
                                // Take care to traverse the opposite edges backwards
                                eip = face->initial_point;
                                oip = opposite->initial_point;
                                o = opposite->edges[0];
                                if (oip == ((StraightEdge *)o)->endpoints[0])    // TODO this only works for EDGE_STRAIGHT
                                    oip = ((StraightEdge *)o)->endpoints[1];
                                else
                                    oip = ((StraightEdge *)o)->endpoints[0];

                                for (i = 0; i < face->n_edges; i++)
                                {
                                    e = face->edges[i];
                                    if (i == 0)
                                        o = opposite->edges[0];
                                    else
                                        o = opposite->edges[face->n_edges - i];
                                   
                                    side = face_new(FACE_RECT, norm);  // Any old norm will do, it will be recalculated with the view list
                                    side->initial_point = eip;
                                    side->vol = vol;

                                    se = (StraightEdge *)edge_new(EDGE_STRAIGHT);
                                    se->endpoints[0] = eip;
                                    se->endpoints[1] = oip;
                                    side->edges[0] = (Edge *)se;
                                    side->edges[1] = o;

                                    // Move to the next pair of points
                                    // TODO this only works for EDGE_STRAIGHT - break this out into a routine that works for all types
                                    if (eip == ((StraightEdge *)e)->endpoints[0])
                                    {
                                        eip = ((StraightEdge *)e)->endpoints[1];
                                    }
                                    else
                                    {
                                        ASSERT(eip == ((StraightEdge *)e)->endpoints[1], "Edges don't join up");
                                        eip = ((StraightEdge *)e)->endpoints[0];
                                    }

                                    if (oip == ((StraightEdge *)o)->endpoints[0])
                                    {
                                        oip = ((StraightEdge *)o)->endpoints[1];
                                    }
                                    else
                                    {
                                        ASSERT(oip == ((StraightEdge *)o)->endpoints[1], "Edges don't join up");
                                        oip = ((StraightEdge *)o)->endpoints[0];
                                    }

                                    se = (StraightEdge *)edge_new(EDGE_STRAIGHT);
                                    se->endpoints[0] = oip;
                                    se->endpoints[1] = eip;
                                    side->edges[2] = (Edge *)se;
                                    side->edges[3] = e;
                                    side->n_edges = 4;

                                    link((Object *)side, (Object **)&vol->faces);
                                }
                                break;

                            case FACE_FLAT:
                            case FACE_CIRCLE:
                                ASSERT(FALSE, "Not implemented yet");
                                break;
                            }

                            link((Object *)vol, &object_tree);
                        }

                        // Project new_point back to the face's normal wrt. picked_point,
                        // as seen from the eye position. First project onto the facing plane
                        // through the picked point
                        proj_plane.refpt = picked_point;
                        intersect_ray_plane(pt.x, pt.y, &proj_plane, &new_point);

                        // Project picked-new onto the face's normal
                        length = dot
                            (
                            new_point.x - picked_point.x,
                            new_point.y - picked_point.y,
                            new_point.z - picked_point.z,
                            face->normal.A,
                            face->normal.B,
                            face->normal.C
                            );

                        // Move the picked face by a delta in XYZ up its own normal
                        move_obj
                            (
                            picked_obj,
                            face->normal.A * length,
                            face->normal.B * length,
                            face->normal.C * length
                            );
                        clear_move_copy_flags(picked_obj);

                        // Invalidate all the view lists for the volume, as any of them may have changed
                        if (face->vol != NULL)
                        {
                            Face *f;

                            for (f = face->vol->faces; f != NULL; f = (Face *)f->hdr.next)
                                f->view_valid = FALSE;
                        }

                        picked_point = new_point;
                    }

                    break;

                default:
                    ASSERT(FALSE, "Mouse drag in starting state");
                    break;
                }

                left_mouseX = pt.x;
                left_mouseY = pt.y;
            }

        }

        // handle panning with right mouse drag. 
        if (right_mouse)
        {
            auxGetMouseLoc(&pt.x, &pt.y);
            if (pt.y != right_mouseY)
            {
                GLint viewport[4], width, height;

                glGetIntegerv(GL_VIEWPORT, viewport);
                width = viewport[2];
                height = viewport[3];
                if (width > height)
                {
                    // Y window coords need inverting for GL
                    xTrans += 2 * zTrans * (float)(right_mouseX - pt.x) / height;
                    yTrans += 2 * zTrans * (float)(pt.y - right_mouseY) / height;
                }
                else
                {
                    xTrans += 2 * zTrans * (float)(right_mouseX - pt.x) / width;
                    yTrans += 2 * zTrans * (float)(pt.y - right_mouseY) / width;
                }

                Position(FALSE, 0, 0);
                right_mouseX = pt.x;
                right_mouseY = pt.y;
            }
            else
            {
                // right mouse click - handle context menu

            }
        }

        // handle zooming. No state change here.
        if (zoom_delta != 0)
        {
            zTrans += 0.01f * zoom_delta;
            Position(FALSE, 0, 0);
            zoom_delta = 0;
        }

        // Only clear pixel buffer stuff if not picking (reduces flashing)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (view_rendered)
            glEnable(GL_LIGHTING);
        else
            glDisable(GL_LIGHTING);
    }

    // handle picking, or just position for viewing
    Position(picking, x_pick, y_pick);

    // draw contents
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    trackball_CalcRotMatrix(matRot);
    glMultMatrixf(&(matRot[0][0]));

    // traverse object tree
    glInitNames();
    for (obj = object_tree; obj != NULL; obj = obj->next)
        draw_object(obj, FALSE, FALSE);

    // draw selection. Watch for highlighted objects appearing in the selection list
    highlit = FALSE;
    for (obj = selection; obj != NULL; obj = obj->next)
    {
        if (obj->prev == curr_obj)
        {
            draw_object(obj->prev, TRUE, TRUE);
            highlit = TRUE;
        }
        else
        {
            draw_object(obj->prev, TRUE, FALSE);
        }
    }

    // draw any current object not yet added to the object tree,
    // or any under highlighting
    if (curr_obj != NULL && !highlit)
        draw_object(curr_obj, FALSE, TRUE);

    // Draw axes XYZ in RGB. 
    glPushName(0);
    glBegin(GL_LINES);
    glColor3d(1.0, 0.4, 0.4);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(100.0, 0.0, 0.0);
    glEnd();
    glBegin(GL_LINES);
    glColor3d(0.4, 1.0, 0.4);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 100.0, 0.0);
    glEnd();
    glBegin(GL_LINES);
    glColor3d(0.4, 0.4, 1.0);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 0.0, 100.0);
    glEnd();

#if 0
    // TEST draw a polygon
    glBegin(GL_POLYGON);
    glColor3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(1.0, 0.0, 0.0);
    glVertex3d(1.0, 0.5, 0.0);
    glVertex3d(0.5, 0.5, 0.0);
    glVertex3d(0.5, 1.0, 0.0);
    glVertex3d(0.0, 1.0, 0.0);
    glEnd();
#endif

    glFlush();
    if (!picking)
        auxSwapBuffers();
}
