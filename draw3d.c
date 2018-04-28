#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// A good-sized array of mouse move coordinates. Used to draw out arcs and beziers.
#define MAX_MOVES   400
static Point move_points[MAX_MOVES];
static int num_moves = 0;

// Show a dimension or other hint in the dims window.
void
show_hint_at(POINT pt, char *buf)
{
    POINT winpt;  // copy the POINT so it doesn't change the caller

    SendDlgItemMessage(hWndDims, IDC_DIMENSIONS, WM_SETTEXT, 0, (LPARAM)buf);
    winpt.x = pt.x;
    winpt.y = pt.y;
    ClientToScreen(auxGetHWND(), &winpt);
    SetWindowPos(hWndDims, HWND_TOPMOST, winpt.x + 10, winpt.y + 20, 0, 0, SWP_NOSIZE);
    ShowWindow(hWndDims, SW_SHOW);
}

void 
hide_hint()
{
    ShowWindow(hWndDims, SW_HIDE);
}

// Some standard colors sent to GL.
void
color(OBJECT obj_type, BOOL selected, BOOL highlighted, BOOL locked)
{
    float r, g, b;

    switch (obj_type)
    {
    case OBJ_VOLUME:
        return;  // no action here

    case OBJ_POINT:
    case OBJ_EDGE:
        r = g = b = 0.3f;
        if (selected)
            r += 0.4f;
        if (highlighted)
            g += 0.4f;
        if (highlighted && locked)
            r = g = b = 0.1f;
        break;

    case OBJ_FACE:
        r = g = b = 0.8f;
        if (selected)
            r += 0.2f;
        if (highlighted)
            g += 0.2f;
        if (highlighted && locked)
            r = g = b = 0.6f;
        break;
    }
    glColor3f(r, g, b);
}

// Shade in a face.
void
face_shade(Face *face, BOOL selected, BOOL highlighted, BOOL locked)
{
    Point   *v;

    gen_view_list_face(face);
    glBegin(GL_POLYGON);
    color(OBJ_FACE, selected, highlighted, locked);
    glNormal3f(face->normal.A, face->normal.B, face->normal.C);
    for (v = face->view_list; v != NULL; v = (Point *)v->hdr.next)
        glVertex3f(v->x, v->y, v->z);
    glEnd();
}

// Draw any object.
void
draw_object(Object *obj, BOOL selected, BOOL highlighted, LOCK parent_lock)
{
    int i;
    Face *face;
    Edge *edge;
    ArcEdge *ae;
    BezierEdge *be;
    Point *p;
    BOOL push_name, locked;

    switch (obj->type)
    {
    case OBJ_POINT:
        push_name = parent_lock <= obj->type;
        locked = parent_lock == obj->type;
        glPushName(push_name ? (GLuint)obj : 0);
        if ((selected || highlighted) && !push_name)
            return;

        p = (Point *)obj;
        if (selected || highlighted)
        {
            float dx, dy, dz;

            // Draw a square blob in the facing plane, so it's more easily seen
            glDisable(GL_CULL_FACE);
            glBegin(GL_POLYGON);
            color(OBJ_POINT, selected, highlighted, locked);
            switch (facing_index)
            {
            case PLANE_XY:
            case PLANE_MINUS_XY:
                dx = 1;
                dy = 1;
                glVertex3f(p->x - dx, p->y - dy, p->z);
                glVertex3f(p->x + dx, p->y - dy, p->z);
                glVertex3f(p->x + dx, p->y + dy, p->z);
                glVertex3f(p->x - dx, p->y + dy, p->z);
                break;
            case PLANE_YZ:
            case PLANE_MINUS_YZ:
                dx = 0;
                dy = 1;
                dz = 1;
                glVertex3f(p->x, p->y - dy, p->z - dz);
                glVertex3f(p->x, p->y + dy, p->z - dz);
                glVertex3f(p->x, p->y + dy, p->z + dz);
                glVertex3f(p->x, p->y - dy, p->z + dz);
                break;
            case PLANE_XZ:
            case PLANE_MINUS_XZ:
                dx = 1;
                dy = 0;
                dz = 1;
                glVertex3f(p->x - dx, p->y, p->z - dz);
                glVertex3f(p->x + dx, p->y, p->z - dz);
                glVertex3f(p->x + dx, p->y, p->z + dz);
                glVertex3f(p->x - dx, p->y, p->z + dz);
            }
            glEnd();
            glEnable(GL_CULL_FACE);
        }
        else
        {
            // Just draw the point, the picking will still work
            glBegin(GL_POINTS);
            color(OBJ_POINT, selected, highlighted, locked);
            glVertex3f(p->x, p->y, p->z);
            glEnd();
        }
        glPopName();
        break;

    case OBJ_EDGE:
        push_name = parent_lock <= obj->type;
        locked = parent_lock == obj->type;
        glPushName(push_name ? (GLuint)obj : 0);
        if ((selected || highlighted) && !push_name)
            return;

        edge = (Edge *)obj;
        switch (edge->type)
        {
        case EDGE_STRAIGHT:
            glBegin(GL_LINES);
            color(OBJ_EDGE, selected, highlighted, locked);
            glVertex3f(edge->endpoints[0]->x, edge->endpoints[0]->y, edge->endpoints[0]->z);
            glVertex3f(edge->endpoints[1]->x, edge->endpoints[1]->y, edge->endpoints[1]->z);
            glEnd();
            glPopName();
            draw_object((Object *)edge->endpoints[0], selected, highlighted, parent_lock);
            draw_object((Object *)edge->endpoints[1], selected, highlighted, parent_lock);
            break;

        case EDGE_ARC:
            ae = (ArcEdge *)edge;
            gen_view_list_arc(ae);
            glBegin(GL_LINE_STRIP);
            color(OBJ_EDGE, selected, highlighted, locked);
            for (p = edge->view_list; p != NULL; p = (Point *)p->hdr.next)
                glVertex3f(p->x, p->y, p->z);

            glEnd();
            glPopName();
            draw_object((Object *)ae->centre, selected, highlighted, parent_lock);
            draw_object((Object *)edge->endpoints[0], selected, highlighted, parent_lock);
            draw_object((Object *)edge->endpoints[1], selected, highlighted, parent_lock);
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)edge;
            gen_view_list_bez(be);
            glBegin(GL_LINE_STRIP);
            color(OBJ_EDGE, selected, highlighted, locked);
            for (p = edge->view_list; p != NULL; p = (Point *)p->hdr.next)
                glVertex3f(p->x, p->y, p->z);

            glEnd();
            glPopName();
            draw_object((Object *)edge->endpoints[0], selected, highlighted, parent_lock);
            draw_object((Object *)edge->endpoints[1], selected, highlighted, parent_lock);
            draw_object((Object *)be->ctrlpoints[0], selected, highlighted, parent_lock);
            draw_object((Object *)be->ctrlpoints[1], selected, highlighted, parent_lock);
            break;
        }
        break;

    case OBJ_FACE:
        locked = parent_lock > obj->type;
        glPushName((GLuint)obj);
        face = (Face *)obj;
        face_shade(face, selected, highlighted, locked);
        glPopName();
        for (i = 0; i < face->n_edges; i++)
            draw_object((Object *)face->edges[i], selected, highlighted, parent_lock);
        break;

    case OBJ_VOLUME:
        for (face = ((Volume *)obj)->faces; face != NULL; face = (Face *)face->hdr.next)
            draw_object((Object *)face, selected, highlighted, parent_lock);
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
                Edge *e;
                ArcEdge *ae;
                BezierEdge *be;
                Plane grad0, grad1;
                float dist;
                Face *rf;
                Plane norm;
                char buf[64], buf2[64];
                Object *parent;

                switch (app_state)
                {
                case STATE_MOVING:
                    // Move the selection, or part thereof, by a delta in XYZ within the facing plane
                    intersect_ray_plane(pt.x, pt.y, facing_plane, &new_point);
                    if (key_status & AUX_SHIFT)
                        snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                    else if (snapping_to_angle)
                        snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                    snap_to_grid(facing_plane, &new_point);

                    // TODO: If picked_obj is a part of the selection, just manipulate that.
                    // Only when a top level object is picked do we want to move the whole
                    // selection.
                    parent = find_top_level_parent(object_tree, picked_obj);
                    if (picked_obj != parent)
                    {
                        move_obj
                            (
                            picked_obj,
                            new_point.x - last_point.x,
                            new_point.y - last_point.y,
                            new_point.z - last_point.z
                            );
                        clear_move_copy_flags(picked_obj);

                        // If we have moved some part of another object containing view lists:
                        // Invalidate them, as any of them may have changed.
                        invalidate_all_view_lists
                            (
                            parent, 
                            picked_obj,
                            new_point.x - last_point.x,
                            new_point.y - last_point.y,
                            new_point.z - last_point.z
                            );
                    }
                    else // Move the whole selection en bloc
                    {
                        for (obj = selection; obj != NULL; obj = obj->next)
                        {
                            move_obj
                                (
                                obj->prev,
                                new_point.x - last_point.x,
                                new_point.y - last_point.y,
                                new_point.z - last_point.z
                                );
                            clear_move_copy_flags(obj->prev);

                            parent = find_top_level_parent(object_tree, obj->prev);
                            invalidate_all_view_lists
                                (
                                parent,
                                obj->prev,
                                new_point.x - last_point.x,
                                new_point.y - last_point.y,
                                new_point.z - last_point.z
                                );
                        }
                    }

                    last_point = new_point;

                    break;

                case STATE_DRAWING_EDGE:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that. 


                    }

                    // Move the end point of the current edge
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                    if (key_status & AUX_SHIFT)
                        snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                    else if (snapping_to_angle)
                        snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                    snap_to_grid(picked_plane, &new_point);

                    // If first move, create the edge here.
                    if (curr_obj == NULL)
                    {
                        curr_obj = (Object *)edge_new(EDGE_STRAIGHT);
                        e = (Edge *)curr_obj;
                        // TODO: share points if snapped onto an existing edge endpoint,
                        // and the edge is not referenced by a face. For now, just create points...
                        e->endpoints[0] = point_newp(&picked_point);
                        e->endpoints[1] = point_newp(&new_point);
                    }
                    else
                    {
                        e = (Edge *)curr_obj;
                        e->endpoints[1]->x = new_point.x;
                        e->endpoints[1]->y = new_point.y;
                        e->endpoints[1]->z = new_point.z;
                    }

                    // Show the dimensions (length) of the edge.
                    sprintf_s(buf, 64, "%s mm", display_rounded(buf2, length(&picked_point, &new_point)));
                    show_hint_at(pt, buf);

                    break;

                case STATE_DRAWING_ARC:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that. 


                    }

                    // Move the end point of the current edge
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                    snap_to_grid(picked_plane, &new_point);

                    // If first move, create the edge here.
                    ae = (ArcEdge *)curr_obj;
                    if (curr_obj == NULL)
                    {
                        curr_obj = (Object *)edge_new(EDGE_ARC);
                        e = (Edge *)curr_obj;
                        // TODO: share points if snapped onto an existing edge endpoint,
                        // and the edge is not referenced by a face. For now, just create points...
                        e->endpoints[0] = point_newp(&picked_point);
                        e->endpoints[1] = point_newp(&new_point);
                        ae = (ArcEdge *)curr_obj;
                        ae->centre = point_new(0, 0, 0);    // will be updated later
                        num_moves = 0;

                        // Masquerade as a straight edge for the moment, until we get a centre
                        e->type = EDGE_STRAIGHT;
                    }
                    else
                    {
                        e = (Edge *)curr_obj;
                        e->endpoints[1]->x = new_point.x;
                        e->endpoints[1]->y = new_point.y;
                        e->endpoints[1]->z = new_point.z;
                    }

                    // Arc edges: remember all mouse moves, and use the midpoint of them as
                    // the 3rd point to define the circular arc.
                    if (num_moves < MAX_MOVES - 1)
                        move_points[num_moves++] = new_point;

                    ae->normal = *picked_plane;

                    if (num_moves > 5)   // set a reasonable number before trying to find the midpoint
                    {
                        Point centre;
                        BOOL clockwise;

                        if
                        (
                            centre_3pt_circle
                            (
                            &picked_point,
                            &move_points[num_moves / 2],
                            &new_point,
                            picked_plane,
                            &centre,
                            &clockwise
                            )
                        )
                        {
                            e->type = EDGE_ARC;
                            ae->centre->x = centre.x;
                            ae->centre->y = centre.y;
                            ae->centre->z = centre.z;
                            ae->clockwise = clockwise;
                            e->view_valid = FALSE;
                        }
                    }

                    break;

                case STATE_DRAWING_BEZIER:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that. 


                    }

                    // Move the end point of the current edge
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                    snap_to_grid(picked_plane, &new_point);

                    // If first move, create the edge here.
                    be = (BezierEdge *)curr_obj;
                    if (curr_obj == NULL)
                    {
                        curr_obj = (Object *)edge_new(EDGE_BEZIER);
                        e = (Edge *)curr_obj;
                        be = (BezierEdge *)curr_obj;
                        // TODO: share points if snapped onto an existing edge endpoint,
                        // and the edge is not referenced by a face. For now, just create points...
                        e->endpoints[0] = point_newp(&picked_point);
                        e->endpoints[1] = point_newp(&new_point);

                        // Create control points. they will be updated shortly
                        be->ctrlpoints[0] = point_newp(&picked_point);
                        be->ctrlpoints[1] = point_newp(&new_point);

                        num_moves = 0;
                    }
                    else
                    {
                        e = (Edge *)curr_obj;
                        e->endpoints[1]->x = new_point.x;
                        e->endpoints[1]->y = new_point.y;
                        e->endpoints[1]->z = new_point.z;
                        be->ctrlpoints[1]->x = new_point.x;
                        be->ctrlpoints[1]->y = new_point.y;
                        be->ctrlpoints[1]->z = new_point.z;
                    }

                    // Bezier edges: remember the first and last couple of moves, and use their 
                    // directions to position the control points.
                    if (num_moves < MAX_MOVES - 1)
                        move_points[num_moves++] = new_point;

                    if (num_moves > 3)
                    {
                        dist = length(&picked_point, &new_point) / 3;

                        // These normalising operations might try to divide by zero. Break if so.
                        grad0.A = move_points[2].x - move_points[0].x;
                        grad0.B = move_points[2].y - move_points[0].y;
                        grad0.C = move_points[2].z - move_points[0].z;
                        if (!normalise_plane(&grad0))
                            break;

                        // Thank Zarquon for optimising compilers...
                        grad1.A = move_points[num_moves - 3].x - move_points[num_moves - 1].x;
                        grad1.B = move_points[num_moves - 3].y - move_points[num_moves - 1].y;
                        grad1.C = move_points[num_moves - 3].z - move_points[num_moves - 1].z;
                        if (!normalise_plane(&grad1))
                            break;

                        be->ctrlpoints[0]->x = e->endpoints[0]->x + grad0.A * dist;
                        be->ctrlpoints[0]->y = e->endpoints[0]->y + grad0.B * dist;
                        be->ctrlpoints[0]->z = e->endpoints[0]->z + grad0.C * dist;

                        be->ctrlpoints[1]->x = e->endpoints[1]->x + grad1.A * dist;
                        be->ctrlpoints[1]->y = e->endpoints[1]->y + grad1.B * dist;
                        be->ctrlpoints[1]->z = e->endpoints[1]->z + grad1.C * dist;

                        e->view_valid = FALSE;
                    }
                    break;

                case STATE_DRAWING_RECT:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that.


                    }

                    // Move the opposite corner point
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                    snap_to_grid(picked_plane, &new_point);

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
                        ASSERT(FALSE, "Draw PLANE_GENERAL Not implemented");
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

                    // If first move, create the rect here.
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
                        p00 = (Point *)rf->view_list;
                        p01 = (Point *)p00->hdr.next;
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

                    // Show the dimensions of the rect.
                    sprintf_s(buf, 64, "%s,%s mm", display_rounded(buf, length(p00, p01)), display_rounded(buf2, length(p00, p03)));
                    show_hint_at(pt, buf);
                    break;

                case STATE_DRAWING_CIRCLE:
                    if (picked_plane == NULL)
                    {
                        // Uhoh. We don't have a plane yet. TODO: Check if the mouse has moved into
                        // a face object, and use that.


                    }

                    // Move the circumference point
                    intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                    snap_to_grid(picked_plane, &new_point);

                    // First move create an arc edge and a circle face
                    if (curr_obj == NULL)
                    {
                        ae = (ArcEdge *)edge_new(EDGE_ARC);
                        ae->normal = *picked_plane;
                        ae->centre = point_newp(&picked_point);
                        
                        // Endpoints are coincident
                        p01 = point_newp(&new_point);
                        ((Edge *)ae)->endpoints[0] = ((Edge *)ae)->endpoints[1] = p01;

                        rf = face_new(FACE_CIRCLE, *picked_plane);
                        rf->edges[0] = (Edge *)ae;
                        rf->n_edges = 1;
                        rf->initial_point = p01;

                        curr_obj = (Object *)rf;
                    }
                    else
                    {
                        rf = (Face *)curr_obj;
                        ae = (ArcEdge *)rf->edges[0];
                        p01 = ((Edge *)ae)->endpoints[0];
                        p01->x = new_point.x;
                        p01->y = new_point.y;
                        p01->z = new_point.z;
                        ((Edge *)ae)->view_valid = FALSE;
                        rf->view_valid = FALSE;
                    }

                    // Show the dimensions of the circle.
                    sprintf_s(buf, 64, "%s mm rad", display_rounded(buf, length(ae->centre, p01)));
                    show_hint_at(pt, buf);
                    break;

                case STATE_DRAWING_MEASURE:
                    ASSERT(FALSE, "Draw Measure Not implemented");
                    break;

                case STATE_DRAWING_EXTRUDE:
                    if (picked_obj != NULL && picked_obj->type == OBJ_FACE)
                    {
                        Plane proj_plane = *facing_plane;
                        Face *face = (Face *)picked_obj;
                        float length, height;

                        // Can we extrude this face?
                        if (face->type == FACE_CYLINDRICAL || face->type == FACE_GENERAL)
                            break;

                        // See if we need to create a volume first, otherwise just move the face
                        if (face->vol == NULL)
                        {
                            int i;
                            Face *opposite, *side;
                            Edge *e, *o, *ne;
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
                                if (oip == o->endpoints[0])
                                    oip = o->endpoints[1];
                                else
                                    oip = o->endpoints[0];

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

                                    ne = edge_new(EDGE_STRAIGHT);
                                    ne->endpoints[0] = eip;
                                    ne->endpoints[1] = oip;
                                    side->edges[0] = ne;
                                    side->edges[1] = o;

                                    // Move to the next pair of points
                                    if (eip == e->endpoints[0])
                                    {
                                        eip = e->endpoints[1];
                                    }
                                    else
                                    {
                                        ASSERT(eip == e->endpoints[1], "Edges don't join up");
                                        eip = e->endpoints[0];
                                    }

                                    if (oip == o->endpoints[0])
                                    {
                                        oip = o->endpoints[1];
                                    }
                                    else
                                    {
                                        ASSERT(oip == o->endpoints[1], "Edges don't join up");
                                        oip = o->endpoints[0];
                                    }

                                    ne = edge_new(EDGE_STRAIGHT);
                                    ne->endpoints[0] = oip;
                                    ne->endpoints[1] = eip;
                                    side->edges[2] = ne;
                                    side->edges[3] = e;
                                    side->n_edges = 4;

                                    link((Object *)side, (Object **)&vol->faces);
                                }
                                break;

                            case FACE_FLAT:
                            case FACE_CIRCLE:
                                ASSERT(FALSE, "Draw Face Flat/Circle Not implemented yet");
                                break;
                            }

                            height = 0;

                            // Link the volume into the object tree. Set its lock to FACES
                            // (default locking is one level down)
                            link((Object *)vol, &object_tree);
                            ((Object *)vol)->lock = LOCK_FACES;
                        }
                        else
                        {
                            // TODO - Find the height of the existing volume
                            // TODO - make height persist between calls!
                            height = 0;
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
                        snap_to_scale(&length);
                        if (length == 0)
                            break;          // come back in when you've got a length

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
                        height += length;

                        // Show the height of the extrusion.
                        sprintf_s(buf, 64, "%s mm", display_rounded(buf2, height));
                        show_hint_at(pt, buf);
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
            zTrans += 0.01f * half_size * zoom_delta;
            if (zTrans > -0.8f * half_size)
                zTrans = -0.8f * half_size;
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

    // traverse object tree. Send their own locks, as they are always at top level.
    glInitNames();
    for (obj = object_tree; obj != NULL; obj = obj->next)
        draw_object(obj, FALSE, FALSE, obj->lock);

    // draw selection. Watch for highlighted objects appearing in the selection list.
    // Pass lock state of top-level parent to determine what is shown.
    highlit = FALSE;
    for (obj = selection; obj != NULL; obj = obj->next)
    {
        Object *parent = find_top_level_parent(object_tree, obj->prev);

        if (obj->prev == curr_obj)
        {
            draw_object(obj->prev, TRUE, TRUE, parent->lock);  
            highlit = TRUE;
        }
        else
        {
            draw_object(obj->prev, TRUE, FALSE, parent->lock); 
        }
    }

    // draw any current object not yet added to the object tree,
    // or any under highlighting. Handle the case where it doesn't have a
    // parent yet.
    if (curr_obj != NULL && !highlit)
    {
        Object *parent = find_top_level_parent(object_tree, curr_obj);

        draw_object(curr_obj, FALSE, TRUE, parent != NULL ? parent->lock : LOCK_NONE);
    }

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

    glFlush();
    if (!picking)
        auxSwapBuffers();
}
