#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// A good-sized array of mouse move coordinates. Used to draw out arcs and beziers.
#define MAX_MOVES   400
static Point move_points[MAX_MOVES];
static int num_moves = 0;

static Plane temp_plane;

// Current drawn number increments for every invalidate and draw.
// (rollover after 2^32 invalidates - unlikely ever)
static unsigned int curr_drawn_no = 0;

// The halo list. Initialise to NULL here so rogue pointers don't escape into free lists.
ListHead halo = { NULL, NULL };

// Material array
Material materials[MAX_MATERIAL];

// Flag to turn drawing off while building display lists.
BOOL suppress_drawing = FALSE;

// Display lists for commonly drawn collections of objects.
// Flags to indicate each DL is valid and can be replayed. 
#define DRAW_DL 3000
#define HL_DL   3001
#define SEL_DL  3002    // Problem: the selction is drawn differently depending on what is highlighted...
BOOL draw_dl_valid = FALSE;         // The object tree
BOOL hl_dl_valid = FALSE;           // The highlighted object
BOOL sel_dl_valid = FALSE;          // The selected object(s)

// Object ID for the highlight DL, so it can be reused if the object is revisited unchanged
Object* hl_dl_obj = NULL;

#ifdef TIME_DRAWING
LARGE_INTEGER draw_clock_start, draw_clock_end, draw_clock, clock_freq;
int num_draws = 0;
#endif

// Set material and lighting up for the rendered view
void
SetMaterial(int mat, BOOL force_set)
{
    static float front_mat_shininess[] = { 30.0f };
    static float front_mat_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    static float front_mat_diffuse[] = { 0.5f, 0.28f, 0.38f, 1.0f };
    static float front_mat_specular[] = { 0.5f, 0.5f, 0.5f, 1.0f };

    static float back_mat_shininess[] = { 50.0f };
    static float back_mat_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    static float back_mat_diffuse[] = { 1.0f, 1.0f, 0.2f, 1.0f };
    static float back_mat_specular[] = { 0.5f, 0.5f, 0.2f, 1.0f };

    static int curr_mat = -1;

    if (mat != curr_mat || force_set)
    {
        if (mat == 0)
        {
            glMaterialfv(GL_FRONT, GL_SHININESS, front_mat_shininess);
            glMaterialfv(GL_FRONT, GL_AMBIENT, front_mat_ambient);
            glMaterialfv(GL_FRONT, GL_DIFFUSE, front_mat_diffuse);
            glMaterialfv(GL_FRONT, GL_SPECULAR, front_mat_specular);

            glMaterialfv(GL_BACK, GL_SHININESS, back_mat_shininess);
            glMaterialfv(GL_BACK, GL_AMBIENT, back_mat_ambient);
            glMaterialfv(GL_BACK, GL_DIFFUSE, back_mat_diffuse);
            glMaterialfv(GL_BACK, GL_SPECULAR, back_mat_specular);
        }
        else
        {
            float col[4];

            col[3] = 1.0f;

            // Multiply material color by amb/diff separately
            col[0] = materials[mat].color[0] * front_mat_ambient[0] * 2.0f;
            col[1] = materials[mat].color[1] * front_mat_ambient[1] * 2.0f;
            col[2] = materials[mat].color[2] * front_mat_ambient[2] * 2.0f;
            glMaterialfv(GL_FRONT, GL_AMBIENT, col);

            col[0] = materials[mat].color[0] * front_mat_diffuse[0] * 2.0f;
            col[1] = materials[mat].color[1] * front_mat_diffuse[1] * 2.0f;
            col[2] = materials[mat].color[2] * front_mat_diffuse[2] * 2.0f;
            glMaterialfv(GL_FRONT, GL_DIFFUSE, col);

            glMaterialf(GL_FRONT, GL_SHININESS, materials[mat].shiny);
        }
        curr_mat = mat;
    }
}

// Some standard colors sent to GL.
// Color as an object of the given type.
void
color_as(OBJECT obj_type, float color_decay, BOOL construction, PRESENTATION pres, BOOL locked)
{
    float r, g, b, a;

    // This object is selected. Color it and all its components.
    BOOL selected = pres & DRAW_SELECTED;
    // This object is highlighted because the mouse is hovering on it.
    BOOL highlighted = pres & DRAW_HIGHLIGHT;
    // This object is highlighted because it is in the halo.
    BOOL in_halo = pres & DRAW_HIGHLIGHT_HALO;

    switch (obj_type)
    {
    case OBJ_GROUP:
    case OBJ_VOLUME:
        return;  // no action here

    case OBJ_POINT:
    case OBJ_EDGE:
        if (construction)
        {
            r = 0.3f;
            g = 0.3f;
            b = 0.5f;
        }
        else
        {
            r = g = b = 0.5f;
        }
        a = 1.0f;

        if (selected)
            r += 0.4f;
        if (highlighted)
            g += 0.4f;
        // Halo treatment varies with the blend mode. Remember that halo edges are drawn (at least) twice.
        if (in_halo)
        {
            switch (view_blend)
            {
            case BLEND_ALPHA:
                g += 0.2f;
                a = 0.5f * color_decay + 0.1f;
                break;
            case BLEND_MULTIPLY:
                r = 0.9f - 0.2f * color_decay;
                g = 1.0f;
                b = 0.9f - 0.2f * color_decay;
                break;
            default:  // opaque
                g += 0.15f * color_decay + 0.05f;
                break;
            }
        }
        if (locked)
            r = g = b = 0.8f;
        break;

    case OBJ_FACE:
        if (construction)
        {
            r = 0.9f;
            g = 0.9f;
            b = 1.0f;
        }
        else
        {
            r = g = b = 0.75f;
        }
        a = 0.6f;

        if (selected)
            r += 0.2f;
        if (highlighted)
            g += 0.2f;
        // Halo treatment varies with the blend mode. Remember that halo faces are drawn twice.
        if (in_halo)
        {
            switch (view_blend)
            {
            case BLEND_ALPHA:
                g += 0.2f;
                a = 0.5f * color_decay + 0.1f;
                break;
            case BLEND_MULTIPLY:
                r = 0.9f - 0.2f * color_decay;
                g = 1.0f;
                b = 0.9f - 0.2f * color_decay;
                break;
            default:  // opaque
                g += 0.15f * color_decay + 0.05f;
                break;
            }
        }
        if (locked)
            r = g = b = 1.0f;
        break;
    }

    glColor4f(r, g, b, a);
}

// Color a passed object.
void
color(Object* obj, BOOL construction, PRESENTATION pres, BOOL locked)
{
    float color_decay = 1.0f;

    if (pres & DRAW_HIGHLIGHT_HALO)
        color_decay = 0;
    if (obj->type == OBJ_FACE)
        color_decay *= ((Face*)obj)->color_decay;

    color_as(obj->type, color_decay, construction, pres, locked);
}

// Draw a single mesh triangle with normal and material index.
void
draw_triangle(void *arg, int mat, float x[3], float y[3], float z[3])
{
    int i;
    float A, B, C, length;

    SetMaterial(mat, FALSE);
    cross(x[1] - x[0], y[1] - y[0], z[1] - z[0], x[2] - x[0], y[2] - y[0], z[2] - z[0], &A, &B, &C);
    length = (float)sqrt(A * A + B * B + C * C);
    if (!nz(length))
    {
        A /= length;
        B /= length;
        C /= length;
        glNormal3d(A, B, C);
    }
    for (i = 0; i < 3; i++)
        glVertex3d(x[i], y[i], z[i]);
}

// Helpers for all clipping tests: determine if a point is clipped out by any
// clipping plane that is in effect. One or two sided test depending on whether
// we're restricting picking to the clipping plane.
BOOL
clipped(Point* p)
{
    double f;

    if (!view_clipped)
        return FALSE;

    f = clip_plane.A * p->x + clip_plane.B * p->y + clip_plane.C * p->z + clip_plane.D;

    if (draw_on_clip_plane)
        return f < -tolerance || f > tolerance;
    else
        return f < tolerance;
}

// Determine if a point is clipped out by its coordinates. One sided test.
BOOL
clippedv(float x, float y, float z)
{
    double f;

    if (!view_clipped)
        return FALSE;

    f = clip_plane.A * x + clip_plane.B * y + clip_plane.C * z + clip_plane.D;

    return f < 0;
}

// Determine if a bbox is partially clipped (lies across the clipping plane)
BOOL
is_bbox_clipped(Bbox* box)
{
    int count = 0;

    if (clippedv(box->xmin, box->ymin, box->zmin))
        count++;
    if (clippedv(box->xmax, box->ymin, box->zmin))
        count++;
    if (clippedv(box->xmin, box->ymax, box->zmin))
        count++;
    if (clippedv(box->xmax, box->ymax, box->zmin))
        count++;
    if (clippedv(box->xmin, box->ymin, box->zmax))
        count++;
    if (clippedv(box->xmax, box->ymin, box->zmax))
        count++;
    if (clippedv(box->xmin, box->ymax, box->zmax))
        count++;
    if (clippedv(box->xmax, box->ymax, box->zmax))
        count++;

    if (count == 0 || count == 8)
        return FALSE;

    return TRUE;
}

// Determine if a triangle is partially clipped (by its coordinates)
BOOL
is_tri_clipped(float x[3], float y[3], float z[3])
{
    int count = 0;

    if (clippedv(x[0], y[0], z[0]))
        count++;
    if (clippedv(x[1], y[1], z[1]))
        count++;
    if (clippedv(x[2], y[2], z[2]))
        count++;
    
    if (count == 0 || count == 3)
        return FALSE;

    return TRUE;
}

// Draw the clip plane intersection with a triangle
void
clip_triangle(void* arg, int mat, float x[3], float y[3], float z[3])
{
    int rc, count = 0;
    Point pt, points[6];
    Plane* plane = (Plane*)arg;

    if (!is_tri_clipped(x, y, z))
        return;
    
    rc = intersect_segment_plane(x[0], y[0], z[0], x[1], y[1], z[1], plane, &pt);
    if (rc == 1)
        points[count++] = pt;
    rc = intersect_segment_plane(x[1], y[1], z[1], x[2], y[2], z[2], plane, &pt);
    if (rc == 1)
        points[count++] = pt;
    rc = intersect_segment_plane(x[2], y[2], z[2], x[0], y[0], z[0], plane, &pt);
    if (rc == 1)
        points[count++] = pt;

    if (count == 2)
    {
        glVertex3d(points[0].x, points[0].y, points[0].z);
        glVertex3d(points[1].x, points[1].y, points[1].z);
    }
}

// For volumes in the object tree, if triangles in their surface mesh intersect the 
// clipping plane, draw line segments across them to represent the sliced surface.
void
draw_clip_intersection(Group *tree)
{
    Object* obj;
    Volume* vol;
    Face* f;

#if 0
    // Make a Plane out of the 4-component clipping plane
    plane.A = (float)clip_plane[0];
    plane.B = (float)clip_plane[1];
    plane.C = (float)clip_plane[2];
    plane.refpt.x = (float)(-clip_plane[0] * clip_plane[3]);
    plane.refpt.y = (float)(-clip_plane[1] * clip_plane[3]);
    plane.refpt.z = (float)(-clip_plane[2] * clip_plane[3]);
#endif

    // Go through the volumes in the tree and intersect each one
    for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
    {
        switch (obj->type)
        {
        case OBJ_VOLUME:
            vol = (Volume*)obj;
            if (!is_bbox_clipped(&vol->bbox))
                break;

            // Make sure there's a triangle mesh
            if (!vol->mesh_valid)
            {
                for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
                    gen_view_list_surface(f);
                vol->mesh_valid = TRUE;
            }

            color_as(OBJ_EDGE, 1.0f, TRUE, DRAW_PATH, FALSE);
            glBegin(GL_LINES);
            mesh_foreach_face_coords_mat(vol->mesh, clip_triangle, &clip_plane);
            glEnd();
            break;

        case OBJ_GROUP:
            draw_clip_intersection((Group*)obj);
            break;
        }
    }
}

// Draw any object. Control select/highlight colors per object type, how the parent is locked,
// and whether to draw components or just the top-level object, among other things.
void
draw_object(Object *obj, PRESENTATION pres, LOCK parent_lock)
{
    int i;
    Face *face;
    Edge *edge;
    ZPolyEdge* zedge;
    ArcEdge *ae;
    BezierEdge *be;
    Point *p;
    Object *o;
    Volume *vol;
    Group *group;
    float dx, dy, dz;
    BOOL locked, constr_edge, re_enable;
    // This object is selected. Color it and all its components.
    BOOL selected = pres & DRAW_SELECTED;
    // This object is highlighted because the mouse is hovering on it.
    BOOL highlighted = pres & DRAW_HIGHLIGHT;
    // This object is highlighted because it is in the halo.
    BOOL in_halo = pres & DRAW_HIGHLIGHT_HALO;
    // We're drawing, so we want to see the snap targets even if they are locked. But only under the mouse.
    BOOL snapping = pres & DRAW_HIGHLIGHT_LOCKED;
    // Whether to show dimensions (all the time if highlighted or selected, but don't pass to components)
    BOOL show_dims = obj->show_dims || (pres & DRAW_WITH_DIMENSIONS);

    BOOL draw_components = !highlighted || !snapping;

    switch (obj->type)
    {
    case OBJ_POINT:
        locked = !(snapping || parent_lock < obj->type);
        p = (Point *)obj;
        if ((selected || highlighted) && locked)
            return;

        if (selected || highlighted)
        {
            // Draw a square blob in the facing plane, so it's more easily seen
            glDisable(GL_CULL_FACE);
            glBegin(GL_POLYGON);
            color(obj, FALSE, pres, locked);
            switch (facing_index)
            {
            case PLANE_XY:
            case PLANE_MINUS_XY:
                dx = 1;         // TODO - scale this unit so it is not too large when zoomed in
                dy = 1;
                dz = 0;
                glVertex3d(p->x - dx, p->y - dy, p->z);
                glVertex3d(p->x + dx, p->y - dy, p->z);
                glVertex3d(p->x + dx, p->y + dy, p->z);
                glVertex3d(p->x - dx, p->y + dy, p->z);
                break;
            case PLANE_YZ:
            case PLANE_MINUS_YZ:
                dx = 0;
                dy = 1;
                dz = 1;
                glVertex3d(p->x, p->y - dy, p->z - dz);
                glVertex3d(p->x, p->y + dy, p->z - dz);
                glVertex3d(p->x, p->y + dy, p->z + dz);
                glVertex3d(p->x, p->y - dy, p->z + dz);
                break;
            case PLANE_XZ:
            case PLANE_MINUS_XZ:
                dx = 1;
                dy = 0;
                dz = 1;
                glVertex3d(p->x - dx, p->y, p->z - dz);
                glVertex3d(p->x + dx, p->y, p->z - dz);
                glVertex3d(p->x + dx, p->y, p->z + dz);
                glVertex3d(p->x - dx, p->y, p->z + dz);
            }
            glEnd();
            glEnable(GL_CULL_FACE);
        }
        else
        {
            // Just draw the point (if not locked), the picking will still work
            if (!locked)
            {
                if (!(selected || highlighted))
                {
                    if (p->drawn == curr_drawn_no)
                        return;
                    p->drawn = curr_drawn_no;
                }
                glBegin(GL_POINTS);
                color(obj, FALSE, pres, locked);
                glVertex3d(p->x, p->y, p->z);
                glEnd();
            }
        }

        break;

    case OBJ_EDGE:
        locked = !(snapping || parent_lock < obj->type);
        edge = (Edge *)obj;
        if ((edge->type & EDGE_CONSTRUCTION) && !view_constr)
            return;
        if ((selected || highlighted) && locked)
            return;
        constr_edge = (pres & DRAW_PATH) != 0 || (edge->type & EDGE_CONSTRUCTION) != 0;

        // Disable blending here so highlight shows up with multiply-blending
        re_enable = FALSE;
        if (selected || highlighted /*|| in_halo*/ || (pres & DRAW_PATH) != 0)
        {
            re_enable = glIsEnabled(GL_BLEND);
            glDisable(GL_BLEND);
        }
        else  // normal drawing, check the drawn no. and don't draw shared edges twice
        {
            if (edge->drawn == curr_drawn_no /*&& !in_halo*/)
                return;
            edge->drawn = curr_drawn_no;
        }

        switch (edge->type & ~EDGE_CONSTRUCTION)
        {
        case EDGE_STRAIGHT:
            glBegin(GL_LINES);
            color(obj, constr_edge, pres, locked);
            glVertex3d(edge->endpoints[0]->x, edge->endpoints[0]->y, edge->endpoints[0]->z);
            glVertex3d(edge->endpoints[1]->x, edge->endpoints[1]->y, edge->endpoints[1]->z);
            glEnd();
            if (draw_components)
            {
                draw_object((Object *)edge->endpoints[0], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
                draw_object((Object *)edge->endpoints[1], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
            }
            break;

        case EDGE_ARC:
            ae = (ArcEdge *)edge;
            gen_view_list_arc(ae);
            glBegin(GL_LINE_STRIP);
            color(obj, constr_edge, pres, locked);
            for (p = (Point *)edge->view_list.head; p != NULL; p = (Point *)p->hdr.next)
                glVertex3d(p->x, p->y, p->z);

            glEnd();
            if (draw_components)
            {
                draw_object((Object *)ae->centre, (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
                draw_object((Object *)edge->endpoints[0], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
                draw_object((Object *)edge->endpoints[1], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
            }
            break;

        case EDGE_BEZIER:
            be = (BezierEdge *)edge;
            gen_view_list_bez(be);
            glBegin(GL_LINE_STRIP);
            color(obj, constr_edge, pres, locked);
            for (p = (Point *)edge->view_list.head; p != NULL; p = (Point *)p->hdr.next)
                glVertex3d(p->x, p->y, p->z);

            glEnd();
            if (draw_components)
            {
                draw_object((Object *)edge->endpoints[0], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
                draw_object((Object *)edge->endpoints[1], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
                draw_object((Object *)be->ctrlpoints[0], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
                draw_object((Object *)be->ctrlpoints[1], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
            }
            break;

        case EDGE_ZPOLY:
            zedge = (ZPolyEdge*)edge;
            color(obj, constr_edge, pres, locked);
            spaghetti(zedge, print_zmin, print_zmax);
            break;
        }

        if (re_enable)
            glEnable(GL_BLEND);
        
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        if (face->vol != NULL)
            locked = parent_lock > obj->type;  // allow picking faces to get to the volume
        else
            locked = parent_lock >= obj->type;
        if ((face->type & FACE_CONSTRUCTION) && !view_constr)
            return;

        if (face->vol == NULL || !materials[face->vol->material].hidden)
        {
            gen_view_list_face(face);
            face_shade(rtess, face, pres, locked);
        }

        // Don't pass draw with dims down to sub-components, to minimise clutter
        if (draw_components)
        {
            for (i = 0; i < face->n_edges; i++)
                draw_object((Object *)face->edges[i], (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
        }
        if (face->text != NULL)
        {
            // Draw the origin and endpoint so it can be picked and moved (unless face is locked at face level)
            draw_object((Object *)&face->text->origin, pres, parent_lock < LOCK_FACES ? LOCK_NONE : parent_lock);
            draw_object((Object *)&face->text->endpt, pres, parent_lock < LOCK_FACES ? LOCK_NONE : parent_lock);
        }
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        if (view_rendered)
        {
            // Draw from the triangulated mesh for the volume. (only for incomplete meshes)
            ASSERT(vol->mesh_valid, "Mesh is not up to date");
            color(obj, FALSE, pres, FALSE);
            glBegin(GL_TRIANGLES);
            mesh_foreach_face_coords_mat(vol->mesh, draw_triangle, NULL);
            glEnd();
        }
        else if (vol->max_facetype == FACE_TRI) // special cases for speed
        {
            ListHead elist = { NULL, NULL };

            gen_view_list_vol(vol);

            // Output triangle faces, putting their edges in a queue to do later.
            // There will be no construction faces in any volume, so just color it once.
            locked = parent_lock > OBJ_FACE;
            color_as(OBJ_FACE, 1.0f, FALSE, pres, locked);
            glBegin(GL_TRIANGLES);
            for (face = (Face*)vol->faces.head; face != NULL; face = (Face*)face->hdr.next)
            {
                for (i = 0, p = (Point*)face->view_list.head; i < 3 && p != NULL; i++, p = (Point*)p->hdr.next)
                    glVertex3d(p->x, p->y, p->z);

                if (draw_components)
                {
                    for (i = 0; i < face->n_edges; i++)
                    {
                        edge = face->edges[i];
                        if (edge->drawn == curr_drawn_no)
                            continue;
                        edge->drawn = curr_drawn_no;
                        link_tail((Object*)edge, &elist);
                    }
                }
            }
            glEnd();

            // Draw all the edges. There will be no construction edges, so just color it once.
            locked = parent_lock >= OBJ_EDGE;
            color_as(OBJ_EDGE, 1.0f, FALSE, pres, locked);
            glBegin(GL_LINES);
            for (edge = (Edge*)elist.head; edge != NULL; edge = (Edge*)edge->hdr.next)
            {
                glVertex3d(edge->endpoints[0]->x, edge->endpoints[0]->y, edge->endpoints[0]->z);
                glVertex3d(edge->endpoints[1]->x, edge->endpoints[1]->y, edge->endpoints[1]->z);
            }
            glEnd();
        }
        else
        {
            // Draw individual faces 
            gen_view_list_vol(vol);
            for (face = (Face *)vol->faces.head; face != NULL; face = (Face *)face->hdr.next)
                draw_object((Object *)face, (pres & ~DRAW_WITH_DIMENSIONS), parent_lock);
        }
        break;

    case OBJ_GROUP:
        // Draw from the triangulated mesh, and then draw any remaining
        // volume meshes that were not completely merged.
        group = (Group *)obj;
        if (view_rendered)
        {
            // The object tree is a group, but it is never merged.
            if (group->mesh != NULL && group->mesh_valid && !group->mesh_merged)
            {
                glBegin(GL_TRIANGLES);
                mesh_foreach_face_coords_mat(group->mesh, draw_triangle, NULL);
                glEnd();
            }
            if (!group->mesh_complete)
            {
                for (o = group->obj_list.head; o != NULL; o = o->next)
                {
                    BOOL merged = FALSE;

                    if (o->type == OBJ_GROUP)
                        merged = ((Group*)o)->mesh_merged;
                    else if (o->type == OBJ_VOLUME)
                        merged = ((Volume*)o)->mesh_merged;
                    else
                        continue;   // can't render edges, points, etc.

                    if (!merged)
                        draw_object(o, (pres & ~DRAW_WITH_DIMENSIONS), o->lock);
                }
            }
        }
        else
        {
            // Not a rendered view - just draw the thing no matter what. Take account of a locked group.
            for (o = group->obj_list.head; o != NULL; o = o->next)
            {
                draw_object
                (
                    o,
                    (pres & ~DRAW_WITH_DIMENSIONS),
                    parent_lock == LOCK_GROUP ? LOCK_GROUP : o->lock
                );
            }
        }
        break;
    }

    if (show_dims)
        show_dims_on(obj, pres, parent_lock);
}

// When about to draw on an object:
// Assign a picked_plane, based on where the mouse has moved to. We still might
// not have a picked_plane after calling this, but we will be back.
void
assign_picked_plane(POINT pt)
{
    Object *obj = Pick(pt.x, pt.y, FALSE);

    if (obj == NULL)
    {
        // We may have started on an edge or at a point, which is not necessarily on the
        // facing plane. Make a plane through the first point picked.
        temp_plane = *facing_plane;
        temp_plane.refpt = picked_point;
        picked_plane = &temp_plane;
    }
    else if (obj->type == OBJ_FACE)
    {
        // Moved onto face - stay on its plane
        picked_plane = &((Face *)obj)->normal;
        picked_obj = obj;
    }
    else if (obj->type == OBJ_VOLUME || obj->type == OBJ_GROUP)
    {
        // go back to the raw picked obj (underlying face) to get a plane
        if (raw_picked_obj != NULL && raw_picked_obj->type == OBJ_FACE)
        {
            picked_plane = &((Face *)raw_picked_obj)->normal;
            picked_obj = raw_picked_obj;
        }
    }
    // other picked obj types just wait till the mouse moves off them
}

// Draw the contents of the main window. Everything happens in here.
void CALLBACK
Draw(void)
{
    float matRot[4][4];
    POINT   pt;
    Object  *obj;
    PRESENTATION pres;
    Object *highlight_obj = NULL;

    if (suppress_drawing)
        return;

    if (app_state != STATE_NONE)
        invalidate_dl();

    // handle mouse movement actions.
    // Highlight pick targets (use highlight_obj for this)
    // If rendering, don't do any picks in here (enables smooth orbiting and spinning)
    if (!left_mouse && !right_mouse && !view_rendered && !view_printer)
    {
        auxGetMouseLoc(&pt.x, &pt.y);
        highlight_obj = Pick(pt.x, pt.y, FALSE);

        // See if we are in the treeview window and have something to highlight from there
        if (highlight_obj != NULL)
            treeview_highlight = NULL;
        else
            highlight_obj = treeview_highlight;

        // stop stale scaling directions from showing on hover
        scaled = 0;

        // Tailor feedback to the action (e.g. extruding faces). Some other faces or edges
        // may be put into the halo list. Some types of objects will have their highlighting
        // suppressed if the action cannot be performed upon them.
        free_obj_list(&halo);
        if (highlight_obj != NULL)
        {
            if (highlight_obj->type == OBJ_GROUP)
            {
                if (app_state == STATE_STARTING_EXTRUDE || app_state == STATE_STARTING_EXTRUDE_LOCAL)
                    highlight_obj = NULL;
            }
            else if (highlight_obj->type == OBJ_VOLUME && raw_picked_obj != NULL)
            {
                find_corner_edges
                (
                    raw_picked_obj,     // get the face
                    highlight_obj,
                    &halo
                );

                // If volume locked at face level, highlight the face if extruding
                // TODO: only highlight faces that are legal to extrude
                if (app_state == STATE_STARTING_EXTRUDE || app_state == STATE_STARTING_EXTRUDE_LOCAL)
                {
                    if (extrudible(raw_picked_obj))
                        highlight_obj = raw_picked_obj;
                    else
                        highlight_obj = NULL;
                }
            }
            else if (highlight_obj->type == OBJ_FACE)
            {
                find_corner_edges
                (
                    highlight_obj,
                    find_parent_object(&object_tree, highlight_obj, FALSE),
                    &halo
                );
                if (app_state == STATE_STARTING_EXTRUDE || app_state == STATE_STARTING_EXTRUDE_LOCAL)
                {
                    if (!extrudible(highlight_obj))
                        highlight_obj = NULL;
                }
            }
            else if (highlight_obj->type == OBJ_EDGE)
            {
                // Mark out adjacent corner edges.
                find_corner_edges
                (
                    highlight_obj,
                    find_parent_object(&object_tree, highlight_obj, FALSE),
                    &halo
                );
                if (app_state == STATE_STARTING_EXTRUDE || app_state == STATE_STARTING_EXTRUDE_LOCAL)
                {
                    if (!extrudible(highlight_obj))
                        highlight_obj = NULL;
                }
                else if (app_state == STATE_STARTING_SCALE || app_state == STATE_STARTING_ROTATE)
                {
                    highlight_obj = NULL;
                }
            }

            // If we're editing any object in a group, highlight the whole group softly
            if (highlight_obj != NULL && parent_picked != NULL)
                link_single(parent_picked, &halo);
        }

        // Set up the halo if we are doing halo highlighting for smooth extrusions, etc.
        // view_halo controls this (but corner edges etc. are still put in the halo list for highlighting)
        if 
        (
            view_halo 
            && 
            highlight_obj != NULL 
            && 
            treeview_highlight == NULL 
            && 
            highlight_obj->type == OBJ_FACE
        )
            calc_halo_params((Face*)highlight_obj, &halo);
    }
    else if (left_mouse && app_state != STATE_DRAGGING_SELECT)
    {
        // If we're performing a left mouse action, we are dragging an object
        // and so picking is unreliable (there are always self-picks). Also, we
        // need a full XYZ coordinate to perform snapping properly, and we're
        // not necessarily snapping things at the mouse position. So we can't use
        // picking here.
        if (app_state == STATE_MOVING)
            highlight_obj = find_in_neighbourhood(picked_obj, &object_tree);
        else
            highlight_obj = find_in_neighbourhood(curr_obj, &object_tree);
    }

    // Handle left mouse dragging actions. We must be moving or drawing,
    // otherwise the trackball would have it and we wouldn't be here.
    if (left_mouse)
    {
        auxGetMouseLoc(&pt.x, &pt.y);

        // Use XYZ coordinates rather than mouse position deltas, as we may
        // have snapped and we want to preserve the accuracy. Just use mouse
        // position to check for gross movement.
        if (pt.x != left_mouseX || pt.y != left_mouseY)
        {
            Point   p1, p2, p3, p4;
            Point   *p00, *p01, *p02, *p03, *p04, *p05;
            Point d1, d3, dn, da;
            Edge *e;
            ArcEdge *ae;
            BezierEdge *be;
            Plane grad0, grad1;
            float dist;
            Face *rf;
            Plane norm;
            Object *parent, *dummy;

            // Flag that the mouse has been moved.
            mouse_moved = TRUE;

            switch (app_state)
            {
            case STATE_DRAGGING_SELECT:
                // Each move, clear the selection, then pick all objects lying within
                // the selection rectangle, adding their top-level parents to the selection.
                clear_selection(&selection);
                Pick_all_in_rect
                (
                    (orig_left_mouseX + pt.x) / 2, 
                    (orig_left_mouseY + pt.y) / 2, 
                    abs(pt.x - orig_left_mouseX), 
                    abs(pt.y - orig_left_mouseY)
                );

                break;

            case STATE_MOVING:
                // Move the selection, or an object, by a delta in XYZ within the facing plane
                intersect_ray_plane(pt.x, pt.y, facing_plane, &new_point);

                // NOTE: shift-dragging will drag a selection, unless you hold shift after mouse down.
                if (key_status & AUX_SHIFT)
                    snap_to_angle(facing_plane, &picked_point, &new_point, 45);
                else if (snapping_to_angle)
                    snap_to_angle(facing_plane, &picked_point, &new_point, angle_snap);
                snap_to_grid(facing_plane, &new_point, key_status & AUX_CONTROL);
                parent = find_top_level_parent(&object_tree, picked_obj);

                // moving a single object (or group) under the cursor
                // allow moving handles even if selected
                if 
                (
                    !is_selected_direct(picked_obj, &dummy) 
                    && 
                    (parent->type == OBJ_GROUP || parent->lock < parent->type)
                )
                {
                    move_obj
                        (
                        picked_obj,
                        new_point.x - last_point.x,
                        new_point.y - last_point.y,
                        new_point.z - last_point.z
                        );

                    // Move any corner edges/faces adjacent to edges of this face
                    move_corner_edges
                    (
                        &halo,
                        new_point.x - last_point.x,
                        new_point.y - last_point.y,
                        new_point.z - last_point.z
                    );

                    clear_move_copy_flags(parent);  // do whole parent in case halo moves other things

                    // If the point is in a text struct, special case here to regenerate the text.
                    // Only if position has actually changed
                    if 
                    (
                        picked_obj->type == OBJ_POINT 
                        && 
                        parent->type == OBJ_FACE 
                        && 
                        ((Face *)parent)->text != NULL
                        &&
                        !near_pt(&new_point, &last_point, SMALL_COORD)
                    )
                    {
                        Face *face = (Face *)parent;

                        // Replace the face with a new one, having the same lock state
                        face = text_face(face->text, face);
                        // TODo what happens if face comes back NULL?
                    }

                    // calculate the extruded heights
                    if (parent->type == OBJ_VOLUME)
                        calc_extrude_heights((Volume *)parent);

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
                    for (obj = selection.head; obj != NULL; obj = obj->next)
                    {
                        move_obj
                            (
                            obj->prev,
                            new_point.x - last_point.x,
                            new_point.y - last_point.y,
                            new_point.z - last_point.z
                            );

                        parent = find_parent_object(&object_tree, obj->prev, FALSE);
                        invalidate_all_view_lists
                            (
                            parent,
                            obj->prev,
                            new_point.x - last_point.x,
                            new_point.y - last_point.y,
                            new_point.z - last_point.z
                            );
                    }

                    // clear the flags separately, to stop double-moving of shared points
                    for (obj = selection.head; obj != NULL; obj = obj->next)
                        clear_move_copy_flags(obj->prev);
                }

                last_point = new_point;

                break;

            case STATE_DRAWING_EDGE:
                if (picked_plane == NULL)
                    // Uhoh. We don't have a plane yet. Check if the mouse has moved into
                    // a face object, and use that. Otherwise, just come back and try with the
                    // next move.
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Move the end point of the current edge
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                if (key_status & AUX_SHIFT)
                    snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                else if (snapping_to_angle)
                    snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                snap_to_grid(picked_plane, &new_point, key_status & AUX_CONTROL);

                // If first move, create the edge here.
                if (curr_obj == NULL)
                {
                    curr_obj = (Object *)edge_new(EDGE_STRAIGHT | (construction ? EDGE_CONSTRUCTION : 0));
                    e = (Edge *)curr_obj;
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
                break;

            case STATE_DRAWING_ARC:
                if (picked_plane == NULL)
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Move the end point of the current edge
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                snap_to_grid(picked_plane, &new_point, key_status& AUX_CONTROL);

                // If first move, create the edge here.
                ae = (ArcEdge *)curr_obj;
                if (curr_obj == NULL)
                {
                    curr_obj = (Object *)edge_new(EDGE_ARC | (construction ? EDGE_CONSTRUCTION : 0));
                    e = (Edge *)curr_obj;
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
                        if (construction)
                        {
                            e->type |= EDGE_CONSTRUCTION;
                            curr_obj->show_dims = TRUE;
                        }
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
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Move the end point of the current edge
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                snap_to_grid(picked_plane, &new_point, key_status & AUX_CONTROL);

                // If first move, create the edge here.
                be = (BezierEdge *)curr_obj;
                if (curr_obj == NULL)
                {
                    curr_obj = (Object *)edge_new(EDGE_BEZIER);
                    e = (Edge *)curr_obj;
                    be = (BezierEdge *)curr_obj;
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
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Move the opposite corner point
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                if (key_status & AUX_SHIFT)
                    snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                else if (snapping_to_angle)
                    snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                snap_to_grid(picked_plane, &new_point, key_status & AUX_CONTROL);

                // generate the other corners. The rect goes in the 
                // order picked-p1-new-p3. 
                if (picked_obj == NULL || picked_obj->type != OBJ_FACE)  
                    //TODO: when starting on a volume locked at face level, the picked plane
                    // must agree with the d1/d3 calculated below (i.e. it must be the facing plane)
                    // Otherwise things get wierd.
                {
                    // Drawing on a facing plane derived from standard axes
                    switch (facing_index)
                    {
                    case PLANE_XY:
                    case PLANE_MINUS_XY:
                        d1.x = 1;
                        d1.y = 0;
                        d1.z = 0;
                        d3.x = 0;
                        d3.y = 1;
                        d3.z = 0;
                        break;

                    case PLANE_YZ:
                    case PLANE_MINUS_YZ:
                        d1.x = 0;
                        d1.y = 1;
                        d1.z = 0;
                        d3.x = 0;
                        d3.y = 0;
                        d3.z = 1;
                        break;

                    case PLANE_XZ:
                    case PLANE_MINUS_XZ:
                        d1.x = 1;
                        d1.y = 0;
                        d1.z = 0;
                        d3.x = 0;
                        d3.y = 0;
                        d3.z = 1;
                        break;

                    case PLANE_GENERAL:
                        ASSERT(FALSE, "Facing index must be axis aligned");
                        break;
                    }
                }
                else
                {
                    float ax, ay, az;

                    // Drawing on a face on an existing object. Derive rect directions
                    // from it if it is sensible to do so.
                    rf = (Face *)picked_obj;
                    dn.x = rf->normal.A;
                    dn.y = rf->normal.B;
                    dn.z = rf->normal.C;
                    switch (rf->type & ~FACE_CONSTRUCTION)
                    {
                    case FACE_RECT:
                        // We're drawing on an existing rect. Make the new rect parallel to its
                        // principal directions just so it looks nice.
                        e = (Edge *)rf->edges[0];
                        d1.x = e->endpoints[1]->x - e->endpoints[0]->x;
                        d1.y = e->endpoints[1]->y - e->endpoints[0]->y;
                        d1.z = e->endpoints[1]->z - e->endpoints[0]->z;
                        normalise_point(&d1);
                        break;

                    default:
                        // Some other kind of face. Find a principal direction by looking
                        // for the smallest component in the normal. If it is axis-aligned
                        // one or two components of the normal will be zero. Any will do.
                        ax = fabsf(dn.x);
                        ay = fabsf(dn.y);
                        az = fabsf(dn.z);
                        if (ax <= ay && ax <= az)
                        {
                            da.x = 1;
                            da.y = 0;
                            da.z = 0;
                        }
                        else if (ay <= ax && ay <= az)
                        {
                            da.x = 0;
                            da.y = 1;
                            da.z = 0;
                        }
                        else
                        {
                            da.x = 0;
                            da.y = 0;
                            da.z = 1;
                        }
                        pcross(&da, &dn, &d1);
                        break;
                    }
                    // calculate the other principal direction in the plane of the face
                    pcross(&d1, &dn, &d3);
                }

                // Produce p1 and p3 by projecting picked-new onto the principal
                // directions derived above
                dn.x = new_point.x - picked_point.x;
                dn.y = new_point.y - picked_point.y;
                dn.z = new_point.z - picked_point.z;
                dist = pdot(&dn, &d1);
                p1.x = picked_point.x + d1.x * dist;
                p1.y = picked_point.y + d1.y * dist;
                p1.z = picked_point.z + d1.z * dist;
                dist = pdot(&dn, &d3);
                p3.x = picked_point.x + d3.x * dist;
                p3.y = picked_point.y + d3.y * dist;
                p3.z = picked_point.z + d3.z * dist;

                // Make sure the normal vector is pointing towards the eye,
                // swapping p1 and p3 if necessary.
                if (normal3(&p1, &picked_point, &p3, &norm))
                {
#ifdef DEBUG_DRAW_RECT_NORMAL
                    {
                        char buf[256];
                        sprintf_s(buf, 256, "%f %f %f\r\n", norm.A, norm.B, norm.C);
                        Log(buf);
                    }
#endif
                    if (dot(norm.A, norm.B, norm.C, picked_plane->A, picked_plane->B, picked_plane->C) < 0)
                    {
                        Point swap = p1;
                        p1 = p3;
                        p3 = swap;
                    }
                }

                // If first move, create the rect here.
                // Create a special rect with no edges but a 4-point view list.
                // Only create the edges when completed, as we have to keep the anticlockwise
                // order of the points no matter how the mouse is dragged around.
                if (curr_obj == NULL)
                {
                    rf = face_new(FACE_RECT | (construction ? FACE_CONSTRUCTION : 0), *picked_plane);

                    // generate four points for the view list
                    p00 = point_newp(&picked_point);
                    p01 = point_newp(&p1);
                    p02 = point_newp(&new_point);
                    p03 = point_newp(&p3);

                    // put the points into the view list. Only the head/next used for now
                    rf->view_list.head = (Object *)p00;
                    rf->initial_point = p00;
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
                    p00 = (Point *)rf->view_list.head;
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

                    update_view_list_2D(rf);
                }

                break;

            case STATE_DRAWING_HEX:
                if (picked_plane == NULL)
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Move the opposite corner point
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                if (key_status & AUX_SHIFT)
                    snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                else if (snapping_to_angle)
                    snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                snap_to_grid(picked_plane, &new_point, key_status & AUX_CONTROL);

                // Generate the other corners. The hex is in the picked plane,
                // and goes in the order picked-p1-p2-new-p3-p4.
                dn.x = new_point.x - picked_point.x;
                dn.y = new_point.y - picked_point.y;
                dn.z = new_point.z - picked_point.z;
                dist = length(&new_point, &picked_point);

                grad0.A = dn.x;
                grad0.B = dn.y;
                grad0.C = dn.z;
                normalise_plane(&grad0);
                plcross(&grad0, picked_plane, &grad1);

                p1.x = picked_point.x + dist * (0.25f * grad0.A + 0.433f * grad1.A);
                p1.y = picked_point.y + dist * (0.25f * grad0.B + 0.433f * grad1.B);
                p1.z = picked_point.z + dist * (0.25f * grad0.C + 0.433f * grad1.C);
                p2.x = picked_point.x + dist * (0.75f * grad0.A + 0.433f * grad1.A);
                p2.y = picked_point.y + dist * (0.75f * grad0.B + 0.433f * grad1.B);
                p2.z = picked_point.z + dist * (0.75f * grad0.C + 0.433f * grad1.C);
                p3.x = picked_point.x + dist * (0.75f * grad0.A - 0.433f * grad1.A);
                p3.y = picked_point.y + dist * (0.75f * grad0.B - 0.433f * grad1.B);
                p3.z = picked_point.z + dist * (0.75f * grad0.C - 0.433f * grad1.C);
                p4.x = picked_point.x + dist * (0.25f * grad0.A - 0.433f * grad1.A);
                p4.y = picked_point.y + dist * (0.25f * grad0.B - 0.433f * grad1.B);
                p4.z = picked_point.z + dist * (0.25f * grad0.C - 0.433f * grad1.C);

                // Make sure the normal vector is pointing towards the eye,
                // swapping points if necessary.
                if (normal3(&p1, &picked_point, &p4, &norm))
                {
#ifdef DEBUG_DRAW_RECT_NORMAL
                    {
                        char buf[256];
                        sprintf_s(buf, 256, "%f %f %f\r\n", norm.A, norm.B, norm.C);
                        Log(buf);
                    }
#endif
                    if (dot(norm.A, norm.B, norm.C, picked_plane->A, picked_plane->B, picked_plane->C) < 0)
                    {
                        Point swap = p1;
                        p1 = p4;
                        p4 = swap;
                        swap = p2;
                        p2 = p3;
                        p3 = swap;
                    }
                }

                // If first move, create the hex here.
                // Create a special face with no edges but a 4-point view list.
                // Only create the edges when completed, as we have to keep the anticlockwise
                // order of the points no matter how the mouse is dragged around.
                if (curr_obj == NULL)
                {
                    rf = face_new(FACE_HEX | (construction ? FACE_CONSTRUCTION : 0), *picked_plane);

                    // generate six points for the view list
                    p00 = point_newp(&picked_point);
                    p01 = point_newp(&p1);
                    p02 = point_newp(&p2);
                    p03 = point_newp(&new_point);
                    p04 = point_newp(&p3);
                    p05 = point_newp(&p4);

                    // put the points into the view list. Only the head/next used for now
                    rf->view_list.head = (Object*)p00;
                    rf->initial_point = p00;
                    p00->hdr.next = (Object*)p01;
                    p01->hdr.next = (Object*)p02;
                    p02->hdr.next = (Object*)p03;
                    p03->hdr.next = (Object*)p04;
                    p04->hdr.next = (Object*)p05;

                    // set it valid, so Draw doesn't try to overwrite it
                    rf->view_valid = TRUE;

                    curr_obj = (Object*)rf;
                }
                else
                {
                    rf = (Face*)curr_obj;

                    // Dig out the points that need updating, and update them
                    p00 = (Point*)rf->view_list.head;
                    p01 = (Point*)p00->hdr.next;
                    p02 = (Point*)p01->hdr.next;
                    p03 = (Point*)p02->hdr.next;
                    p04 = (Point*)p03->hdr.next;
                    p05 = (Point*)p04->hdr.next;

                    // Update the points
                    p01->x = p1.x;
                    p01->y = p1.y;
                    p01->z = p1.z;
                    p02->x = p2.x;
                    p02->y = p2.y;
                    p02->z = p2.z;
                    p03->x = new_point.x;
                    p03->y = new_point.y;
                    p03->z = new_point.z;
                    p04->x = p3.x;
                    p04->y = p3.y;
                    p04->z = p3.z;
                    p05->x = p4.x;
                    p05->y = p4.y;
                    p05->z = p4.z;

                    update_view_list_2D(rf);
                }

                break;

            case STATE_DRAWING_CIRCLE:
                if (picked_plane == NULL)
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Move the circumference point. Allow angle snapping so user can 
                // select sensible radius value if desired.
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                if (key_status & AUX_SHIFT)
                    snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                else if (snapping_to_angle)
                    snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                snap_to_grid(picked_plane, &new_point, key_status& AUX_CONTROL);

                // First move create an arc edge and a circle face
                if (curr_obj == NULL)
                {
                    ae = (ArcEdge *)edge_new(EDGE_ARC | (construction ? EDGE_CONSTRUCTION : 0));
                    ae->normal = *picked_plane;
                    ae->centre = point_newp(&picked_point);
                        
                    // Endpoints are coincident, but they have to be separate Point structs,
                    // to allow edge to be threaded into a view list in either direction
                    p00 = point_newp(&new_point);
                    ((Edge *)ae)->endpoints[0] = p00;
                    p01 = point_newp(&new_point);
                    ((Edge *)ae)->endpoints[1] = p01;

                    rf = face_new(FACE_CIRCLE | (construction ? FACE_CONSTRUCTION : 0), *picked_plane);
                    rf->edges[0] = (Edge *)ae;

                    // Add a straight edge linking p01 - p00, in case these points ever
                    // get pulled apart by a mischievous user. 
                    e = edge_new(EDGE_STRAIGHT);
                    e->endpoints[0] = p01;
                    e->endpoints[1] = p00;
                    rf->edges[1] = e;

                    rf->n_edges = 2;
                    rf->initial_point = p00;
                    curr_obj = (Object *)rf;
                }
                else
                {
                    rf = (Face *)curr_obj;
                    ae = (ArcEdge *)rf->edges[0];
                    p00 = ((Edge *)ae)->endpoints[0];
                    p00->x = new_point.x;
                    p00->y = new_point.y;
                    p00->z = new_point.z;
                    p01 = ((Edge *)ae)->endpoints[1];
                    p01->x = new_point.x;
                    p01->y = new_point.y;
                    p01->z = new_point.z;
                    ((Edge *)ae)->view_valid = FALSE;
                    rf->view_valid = FALSE;
                }

                break;

            case STATE_DRAWING_EXTRUDE:
            case STATE_DRAWING_EXTRUDE_LOCAL:
                if (picked_obj != NULL && picked_obj->type == OBJ_FACE)
                {
                    Plane proj_plane;
                    Face *face = (Face *)picked_obj;
                    float length;

                    // Can we extrude this face? (does it have a normal?)
                    if (!extrudible((Object *)face))
                        break;

                    curr_obj = picked_obj;  // for highlighting

                    // See if we need to create a volume first, otherwise just move the face
                    if (face->vol == NULL)
                    {
                        int i, c;
                        Face *opposite, *side;
                        Edge *e, *o, *ne;
                        Point *eip, *oip;
                        Volume *vol;

                        // Put face into the volume's face list, and remove any text structure attached.
                        delink_group((Object *)face, &object_tree);
                        vol = vol_new();
                        link((Object *)face, &vol->faces);
                        face->vol = vol;
                        if (face->text != NULL)
                        {
                            free(face->text);
                            face->text = NULL;
                        }
                        face->view_valid = FALSE;
                        vol->max_facetype = face->type;

                        // Clone the face with coincident edges/points, but in the
                        // opposite sense (and with an opposite normal)
                        opposite = clone_face_reverse(face);
                        clear_move_copy_flags(picked_obj);
                        link((Object *)opposite, &vol->faces);
                        opposite->vol = vol;

                        // After cloning, both the face and its clone have contour arrays.
                        // The edge indexes and edge counts of each contour will line up.
                        for (c = 0; c < face->n_contours; c++)
                        {
                            int ei = face->contours[c].edge_index;

                            // Create faces that link the picked face to its clone
                            // Take care to traverse the opposite edges backwards
                            // Start with the initial points of the countours
                            //eip = face->initial_point;
                            eip = face->edges[ei]->endpoints[face->contours[c].ip_index];
                            //oip = opposite->initial_point;
                            oip = opposite->edges[ei]->endpoints[opposite->contours[c].ip_index];
                            o = opposite->edges[ei];
                            if (oip == o->endpoints[0])
                                oip = o->endpoints[1];
                            else
                                oip = o->endpoints[0];

                            for (i = 0; i < face->contours[c].n_edges; i++)
                            {
                                FACE side_type;

                                e = face->edges[ei + i];
                                if (i == 0)
                                    o = opposite->edges[ei];
                                else
                                    o = opposite->edges[ei + face->contours[c].n_edges - i];

                                side_type = FACE_RECT;
                                if (e->type == EDGE_ARC || e->type == EDGE_BEZIER)
                                    side_type = FACE_CYLINDRICAL;
                                if (side_type > vol->max_facetype)
                                    vol->max_facetype = side_type;
                                ASSERT(e->type == o->type, "Opposite edge types don't match");
                                norm.A = norm.B = norm.C = 0; 
                                side = face_new(side_type, norm);
                                side->normal.refpt = *eip;      // give it a valid refpt but a denormal ABC
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
                                if (e->corner)
                                    side->corner = TRUE;    // if extruding a corner edge, mark face too
                                link((Object *)side, &vol->faces);
                            }
                        }

                        // Link the volume into the object tree. Set its lock to FACES
                        // (default locking is one level down)
                        link_group((Object *)vol, &object_tree);
                        ((Object *)vol)->lock = LOCK_FACES;
                    }

                    // Project new_point back to the face's normal wrt. picked_point,
                    // as seen from the eye position. First project onto a plane
                    // through the picked point, perp to the ray from the eye position.
                    ray_from_eye(pt.x, pt.y, &proj_plane);
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
                    snap_to_scale(&length, key_status& AUX_CONTROL);
                    if (length != 0)
                    {
                        if (IS_FLAT(face) || app_state == STATE_DRAWING_EXTRUDE)
                        {
                            // Move the picked face by a delta in XYZ up its own normal
                            move_obj
                            (
                                picked_obj,
                                face->normal.A * length,
                                face->normal.B * length,
                                face->normal.C * length
                            );

                            // Move any corner edges/faces adjacent to edges of this face
                            move_corner_edges
                            (
                                &halo,
                                face->normal.A * length,
                                face->normal.B * length,
                                face->normal.C * length
                            );

                            // Move any halo faces if called for
                            if (view_halo)
                            {
                                move_halo_around_face
                                (
                                    face,
                                    face->normal.A * length,
                                    face->normal.B * length,
                                    face->normal.C * length
                                );
                            }
                        }
                        else    // Curved faces get their points extruded by their local normals
                        {
                            extrude_local(face, length);
                        }
                        clear_move_copy_flags((Object*)face->vol); // need to do on whole volume if moving halo.
                        picked_point = new_point;
                    }

                    // calculate the extruded heights
                    calc_extrude_heights(face->vol);

                    // Invalidate all the view lists for the volume, as any of them may have changed
                    invalidate_all_view_lists((Object *)face->vol, (Object *)face->vol, 0, 0, 0);
                }

                break;

            case STATE_DRAWING_TEXT:
                if (picked_plane == NULL)
                    assign_picked_plane(pt);
                if (picked_plane == NULL)
                    break;

                // Proceed as for drawing an edge, so user gets feedback about text direction.
                intersect_ray_plane(pt.x, pt.y, picked_plane, &new_point);
                if (key_status & AUX_SHIFT)
                    snap_to_angle(picked_plane, &picked_point, &new_point, 45);
                else if (snapping_to_angle)
                    snap_to_angle(picked_plane, &picked_point, &new_point, angle_snap);
                snap_to_grid(picked_plane, &new_point, key_status& AUX_CONTROL);

                curr_text->origin = picked_point;
                curr_text->endpt = new_point;
                curr_text->plane = *picked_plane;
                curr_obj = (Object *)text_face(curr_text, (Face *)curr_obj);
                break;

            case STATE_DRAWING_SCALE:
                if (picked_obj != NULL)
                {
                    float sx, sy, sz;

                    intersect_ray_plane(pt.x, pt.y, &centre_facing_plane, &new_point);
                    // Find dominant direction in plane, or do both if SHIFT key down
                    d1.x = 0;  // shhh compiler
                    d1.y = 0;
                    d1.z = 0;  
                    scaled = scaled_dirn;
                    switch (facing_index)
                    {
                    case PLANE_XY:
                    case PLANE_MINUS_XY:
                        d1.x = fabsf(picked_point.x - centre_facing_plane.refpt.x);
                        d1.y = fabsf(picked_point.y - centre_facing_plane.refpt.y);
                        if (key_status & AUX_SHIFT)
                            scaled = DIRN_X | DIRN_Y;
                        break;

                    case PLANE_XZ:
                    case PLANE_MINUS_XZ:
                        d1.x = fabsf(picked_point.x - centre_facing_plane.refpt.x);
                        d1.z = fabsf(picked_point.z - centre_facing_plane.refpt.z);
                        if (key_status & AUX_SHIFT)
                            scaled = DIRN_X | DIRN_Z;
                        break;

                    case PLANE_YZ:
                    case PLANE_MINUS_YZ:
                        d1.y = fabsf(picked_point.y - centre_facing_plane.refpt.y);
                        d1.z = fabsf(picked_point.z - centre_facing_plane.refpt.z);
                        if (key_status & AUX_SHIFT)
                            scaled = DIRN_Y | DIRN_Z;
                        break;
                    }

                    sx = sy = sz = 1;
                    if ((scaled & DIRN_X) && !nz(d1.x))
                    {
                        sx = fabsf(new_point.x - centre_facing_plane.refpt.x) / d1.x;
                        eff_sx *= sx;
                    }
                    if ((scaled & DIRN_Y) && !nz(d1.y))
                    {
                        sy = fabsf(new_point.y - centre_facing_plane.refpt.y) / d1.y;
                        eff_sy *= sy;
                    }
                    if ((scaled & DIRN_Z) && !nz(d1.z))
                    {
                        sz = fabsf(new_point.z - centre_facing_plane.refpt.z) / d1.z;
                        eff_sz *= sz;
                    }
                    scale_obj_free
                    (
                        picked_obj,
                        sx,
                        ((scaled & DIRN_Y) && !nz(d1.y)) ? fabsf(new_point.y - centre_facing_plane.refpt.y) / d1.y : 1,
                        ((scaled & DIRN_Z) && !nz(d1.z)) ? fabsf(new_point.z - centre_facing_plane.refpt.z) / d1.z : 1,
                        centre_facing_plane.refpt.x,
                        centre_facing_plane.refpt.y,
                        centre_facing_plane.refpt.z
                    );
                    clear_move_copy_flags(picked_obj);

                    curr_obj = picked_obj;  // for highlighting
                    picked_point = new_point;
                    invalidate_all_view_lists(picked_obj, picked_obj, 0, 0, 0);
                }

                break;

            case STATE_DRAWING_ROTATE:
                if (picked_obj == NULL)
                    break;
                {
                    float da;
                    float alpha;

                    intersect_ray_plane(pt.x, pt.y, &centre_facing_plane, &new_point);
                    da = RADF * angle3(&picked_point, &centre_facing_plane.refpt, &new_point, &centre_facing_plane);

                    switch (facing_index)       // this matches the centre facing plane
                    {
                    case PLANE_XY:
                        total_angle += da;
                        alpha = cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT);
                        break;
                    case PLANE_MINUS_XY:
                        total_angle -= da;
                        alpha = cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT);
                        break;
                    case PLANE_XZ:
                        total_angle += da;
                        alpha = cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT);
                        break;
                    case PLANE_MINUS_XZ:
                        total_angle -= da;
                        alpha = cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT);
                        break;
                    case PLANE_YZ:
                        total_angle += da;
                        alpha = cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT);
                        break;
                    case PLANE_MINUS_YZ:
                        total_angle -= da;
                        alpha = cleanup_angle_and_snap(total_angle, key_status & AUX_SHIFT);
                        break;
                    }
                    {
                        char buf[64];
                        sprintf_s(buf, 64, "DA %f TA %f alpha %f", da, total_angle, alpha);
                        Log(buf);
                        Log("\r\n");
                    }

                    if (alpha != effective_angle)
                    {
                        // When alpha changes, rotate the object in-place
                        rotate_obj_free_facing
                        (
                            picked_obj,
                            effective_angle - alpha,
                            centre_facing_plane.refpt.x,
                            centre_facing_plane.refpt.y,
                            centre_facing_plane.refpt.z
                        );
                        clear_move_copy_flags(picked_obj);

                        effective_angle = alpha;
                    }
                    curr_obj = picked_obj;  // for highlighting
                    picked_point = new_point;
                }
                invalidate_all_view_lists(picked_obj, picked_obj, 0, 0, 0);

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

            Position();
            right_mouseX = pt.x;
            right_mouseY = pt.y;
        }
    }

    // handle zooming. No state change here.
    if (zoom_delta != 0)
    {
        zTrans += 0.002f * half_size * zoom_delta;
        // Don't go too far forward, or we'll hit the near clipping plane
        if (zTrans > -0.1f * half_size)
            zTrans = -0.1f * half_size;
        Position();
        zoom_delta = 0;
    }

    // Only clear pixel buffer stuff if not picking (reduces flashing)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (view_rendered || view_printer)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
    }
    else
    {
        if (view_blend == BLEND_OPAQUE)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
    }

    // handle picking, or just position for viewing
    Position();

    // draw contents
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    trackball_CalcRotMatrix(matRot);
    glMultMatrixf(&(matRot[0][0]));

    // Clipping if enabled (do this every time as it gets translated to eye coords)
    if (view_clipped)
    {
        double clip[4];

        clip[0] = clip_plane.A;
        clip[1] = clip_plane.B;
        clip[2] = clip_plane.C;
        clip[3] = clip_plane.D + tolerance;
        glClipPlane(GL_CLIP_PLANE0, clip);
    }

    // Draw the object tree. 
#ifdef TIME_DRAWING
    if (num_draws * num_picks == 0)  // only once
    {
        QueryPerformanceFrequency(&clock_freq);
        clock_freq.QuadPart /= 1000000;   // bring to usecs
    }
    QueryPerformanceCounter(&draw_clock_start);
#endif
    if (draw_dl_valid)
    {
        // Object tree has not changed, just redraw from the display list
        glCallList(DRAW_DL);
    }
    else
    {
        glNewList(DRAW_DL, GL_COMPILE_AND_EXECUTE);
        pres = 0;
        if (app_state >= STATE_STARTING_EDGE)
            pres |= DRAW_HIGHLIGHT_LOCKED;
        curr_drawn_no++;
        if (view_printer)
            draw_object((Object*)&gcode_tree, pres, LOCK_NONE);  // locks come from objects
        else
            draw_object((Object*)&object_tree, pres, LOCK_NONE);  // locks come from objects

        // If clipping, and tracing is enabled, draw the intersection path of
        // any volumes with the clipping plane.
        if (view_clipped && draw_on_clip_plane)
        {
            draw_clip_intersection(&object_tree);
        }

        glEndList();
        draw_dl_valid = TRUE;
    }

#ifdef TIME_DRAWING
    QueryPerformanceCounter(&draw_clock_end);
    num_draws++;
    draw_clock.QuadPart += draw_clock_end.QuadPart - draw_clock_start.QuadPart;
    if (num_draws == 100)
    {
        char buf[64];

        sprintf_s(buf, 64, "100 draws: %lld us\r\n", draw_clock.QuadPart / clock_freq.QuadPart);
        Log(buf);
        draw_clock.QuadPart = 0;
        num_draws = 0;
    }
#endif

    if (!view_rendered && !view_printer)
    {
        // Draw selection. Watch for highlighted objects appearing in the selection list.
        // Pass lock state of parent to determine what is shown.
        for (obj = selection.head; obj != NULL; obj = obj->next)
        {
            Object *parent = find_parent_object(&object_tree, obj->prev, TRUE);

            if (obj->prev != curr_obj && obj->prev != highlight_obj)
            {
                pres = DRAW_SELECTED | DRAW_WITH_DIMENSIONS;
                draw_object(obj->prev, pres, parent != NULL ? parent->lock : LOCK_NONE);
            }
        }

        // Draw any current object not yet added to the object tree,
        // or any under highlighting. Handle the case where it doesn't have a
        // parent yet. 
        if (curr_obj != NULL)
        {
            Object *parent = find_parent_object(&object_tree, curr_obj, TRUE);

            pres = DRAW_HIGHLIGHT | DRAW_WITH_DIMENSIONS;
            if (app_state >= STATE_STARTING_EDGE)
                pres |= DRAW_HIGHLIGHT_LOCKED;
            draw_object(curr_obj, pres, parent != NULL ? parent->lock : LOCK_NONE);
        }

        // Draw edges in current path.
        if (curr_path != NULL)
        {
            pres = DRAW_PATH;
            draw_object(curr_path, pres, curr_path->lock);
        }

        // Draw highlighted object(s).
        if (highlight_obj != NULL)
        {
            Object* parent = find_parent_object(&object_tree, highlight_obj, TRUE);

            pres = DRAW_HIGHLIGHT | DRAW_WITH_DIMENSIONS;
            //if (app_state >= STATE_STARTING_EDGE)         // allow locked edges to be highlighted
                pres |= DRAW_HIGHLIGHT_LOCKED;
            draw_object(highlight_obj, pres, parent != NULL ? parent->lock : LOCK_NONE);

            // Draw parent object so its dimensions are updated and shown 
            // (faces only; volumes are handled by extrusion)
            if (parent != NULL && parent != highlight_obj && parent->type == OBJ_FACE)
            {
                pres = DRAW_HIGHLIGHT | DRAW_WITH_DIMENSIONS;
                if (app_state >= STATE_STARTING_EDGE)
                    pres |= DRAW_HIGHLIGHT_LOCKED;
                draw_object(parent, pres, parent->lock);
            }

            // Draw halo objects, if any have been picked out for highlighting.
            for (obj = halo.head; obj != NULL; obj = obj->next)
            {
                Object* parent = find_parent_object(&object_tree, obj->prev, TRUE);

                if (obj->prev != curr_obj && obj->prev != highlight_obj)
                {
                    pres = DRAW_HIGHLIGHT_HALO;
                    draw_object(obj->prev, pres, parent != NULL ? parent->lock : LOCK_NONE);
                }
            }

#ifdef DEBUG_HIGHLIGHTING_ENABLED
            // The bounding box for a volume, or the parent volume of any highlighted component.
            // TODO: this will not work for groups, as find_parent_object won't return them.
            if (app_state == STATE_NONE && parent != NULL && parent->type == OBJ_VOLUME)
            {
                Volume* vol = (Volume*)parent;

                if (debug_view_bbox)
                {
                    Bbox box = vol->bbox;

                    glColor3d(1.0, 0.4, 0.4);
                    glBegin(GL_LINE_LOOP);
                    glVertex3f(box.xmin, box.ymin, box.zmin);
                    glVertex3f(box.xmax, box.ymin, box.zmin);
                    glVertex3f(box.xmax, box.ymax, box.zmin);
                    glVertex3f(box.xmin, box.ymax, box.zmin);
                    glEnd();
                    glBegin(GL_LINE_LOOP);
                    glVertex3f(box.xmin, box.ymin, box.zmax);
                    glVertex3f(box.xmax, box.ymin, box.zmax);
                    glVertex3f(box.xmax, box.ymax, box.zmax);
                    glVertex3f(box.xmin, box.ymax, box.zmax);
                    glEnd();
                    glBegin(GL_LINES);
                    glVertex3f(box.xmin, box.ymin, box.zmin);
                    glVertex3f(box.xmin, box.ymin, box.zmax);
                    glEnd();
                    glBegin(GL_LINES);
                    glVertex3f(box.xmax, box.ymin, box.zmin);
                    glVertex3f(box.xmax, box.ymin, box.zmax);
                    glEnd();
                    glBegin(GL_LINES);
                    glVertex3f(box.xmin, box.ymax, box.zmin);
                    glVertex3f(box.xmin, box.ymax, box.zmax);
                    glEnd();
                    glBegin(GL_LINES);
                    glVertex3f(box.xmax, box.ymax, box.zmin);
                    glVertex3f(box.xmax, box.ymax, box.zmax);
                    glEnd();
                }
            }

            // Normals (red) and local normals (blue) for all the faces in a volume.
            if (app_state == STATE_NONE && highlight_obj != NULL && highlight_obj->type == OBJ_VOLUME)
            {
                Volume* vol = (Volume*)highlight_obj;

                if (debug_view_normals)
                {
                    Face* f;
                    int i;

                    for (f = (Face*)vol->faces.head; f != NULL; f = (Face*)f->hdr.next)
                    {
                        if (extrudible((Object *)f))
                        {
                            Plane* n = &f->normal;

                            glColor3d(1.0, 0.4, 0.4);
                            glBegin(GL_LINES);
                            glVertex3f(n->refpt.x, n->refpt.y, n->refpt.z);
                            glVertex3f(n->refpt.x + 3 * n->A, n->refpt.y + 3 * n->B, n->refpt.z + 3 * n->C);
                            glEnd();
                        }

                        for (i = 0; i < f->n_local; i++)
                        {
                            PlaneRef* n = &f->local_norm[i];

                            glColor3d(0.4, 0.4, 1.0);
                            glBegin(GL_LINES);
                            glVertex3f(n->refpt->x, n->refpt->y, n->refpt->z);
                            glVertex3f(n->refpt->x + 2 * n->A, n->refpt->y + 2 * n->B, n->refpt->z + 2 * n->C);
                            glEnd();
                        }
                    }
                }
            }

            // The normal and any local normals for a single highlighted face.
            if (app_state == STATE_NONE && highlight_obj != NULL && highlight_obj->type == OBJ_FACE)
            {
                Face* f = (Face*)highlight_obj;
                int i;

                if (debug_view_normals)
                {
                    if (extrudible((Object*)f))
                    {
                        Plane* n = &f->normal;

                        glColor3d(1.0, 0.4, 0.4);
                        glBegin(GL_LINES);
                        glVertex3f(n->refpt.x, n->refpt.y, n->refpt.z);
                        glVertex3f(n->refpt.x + 3 * n->A, n->refpt.y + 3 * n->B, n->refpt.z + 3 * n->C);
                        glEnd();
                    }
                    for (i = 0; i < f->n_local; i++)
                    {
                        PlaneRef* n = &f->local_norm[i];

                        glColor3d(0.4, 0.4, 1.0);
                        glBegin(GL_LINES);
                        glVertex3f(n->refpt->x, n->refpt->y, n->refpt->z);
                        glVertex3f(n->refpt->x + 2 * n->A, n->refpt->y + 2 * n->B, n->refpt->z + 2 * n->C);
                        glEnd();
                    }
                }

                // The view list for a single highlighted face.
                if (debug_view_viewlist)
                {
                    Point* v;

                    for (v = (Point*)f->view_list.head; v->hdr.next != NULL; v = (Point*)v->hdr.next)
                    {
                        if (v->flags == FLAG_NONE)
                            draw_object((Object*)v, DRAW_SELECTED, LOCK_NONE);
                    }
                }
            }
    #endif
        }
    }
    
    // Draw axes and optional print bed view on 10mm grid.
    {
        double axis = 100;

        SetMaterial(0, TRUE);

        if (view_printbed)
        {
            double xoffset = (bed_xmax - bed_xmin) / 2;
            double yoffset = (bed_ymax - bed_ymin) / 2;
            double xmin = bed_xmin - xoffset;
            double ymin = bed_ymin - yoffset;
            double xmax = bed_xmax - xoffset;
            double ymax = bed_ymax - yoffset;
            double xinc = 10;
            double yinc = 10;
            double x, y;

            // Draw print bed. By default, just a 200x200mm square centred at the origin.
            glBegin(GL_LINE_LOOP);
            glColor3d(0.8, 0.8, 0.8);
            glVertex3d(xmin, ymin, 0.0);
            glVertex3d(xmin, ymax, 0.0);
            glVertex3d(xmax, ymax, 0.0);
            glVertex3d(xmax, ymin, 0.0);
            glEnd();

            glBegin(GL_LINES);
            glColor3d(0.8, 0.8, 0.8);
            for (x = xmin + xinc; x < xmax - 0.001; x += xinc)
            {
                glVertex3d(x, ymin, 0.0);
                glVertex3d(x, ymax, 0.0);
            }
            for (y = ymin + yinc; y < ymax - 0.001; y += yinc)
            {
                glVertex3d(xmin, y, 0.0);
                glVertex3d(xmax, y, 0.0);
            }
            glEnd();
            axis = 15;
        }

        // Draw axes XYZ in RGB (large for tool view, small for print bed view)
        glBegin(GL_LINES);
        glColor3d(1.0, 0.4, 0.4);
        glVertex3d(0.0, 0.0, 0.0);
        glVertex3d(axis, 0.0, 0.0);
        glEnd();
        glBegin(GL_LINES);
        glColor3d(0.4, 1.0, 0.4);
        glVertex3d(0.0, 0.0, 0.0);
        glVertex3d(0.0, axis, 0.0);
        glEnd();
        glBegin(GL_LINES);
        glColor3d(0.4, 0.4, 1.0);
        glVertex3d(0.0, 0.0, 0.0);
        glVertex3d(0.0, 0.0, axis);
        glEnd();
    }

    // handle shift-drag for selection by drawing 2D rect. 
    if (app_state == STATE_DRAGGING_SELECT)
    {
        GLint vp[4];

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glGetIntegerv(GL_VIEWPORT, vp);
        glOrtho(vp[0], vp[0] + vp[2], vp[1], vp[1] + vp[3], 0.1, 10);
        glTranslatef(0, 0, -1);
        glColor3f(0, 0.5, 0);
        glBegin(GL_LINE_LOOP);
        glVertex2f((float)pt.x, (float)vp[3] - pt.y);
        glVertex2f((float)orig_left_mouseX, (float)vp[3] - pt.y);
        glVertex2f((float)orig_left_mouseX, (float)vp[3] - orig_left_mouseY);
        glVertex2f((float)pt.x, (float)vp[3] - orig_left_mouseY);
        glEnd();
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

#if 0  // This is done in show_dims now (on the object, not under the cursor)
    // echo highlighting of snap targets at cursor
    if (highlight_obj != NULL)
    {
        HDC hdc = auxGetHDC();
        GLint vp[4];
        char buf[64];

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glGetIntegerv(GL_VIEWPORT, vp);
        glOrtho(vp[0], vp[0] + vp[2], vp[1], vp[1] + vp[3], 0.1, 10);
        glTranslatef(0, 0, -1);

        glListBase(1000);
        glColor3f(0.4f, 0.4f, 0.4f);
        glRasterPos2f((float)pt.x + 10, (float)vp[3] - pt.y - 12);

        brief_description(highlight_obj, buf, 64);
        glCallLists(strlen(buf), GL_UNSIGNED_BYTE, buf);

        // Echo the parent group if there is one.
        if (highlight_obj->parent_group != NULL && highlight_obj->parent_group->hdr.ID != 0)
        {
            glCallLists(2, GL_UNSIGNED_BYTE, " (");
            brief_description((Object *)highlight_obj->parent_group, buf, 64);
            glCallLists(strlen(buf), GL_UNSIGNED_BYTE, buf);
            glCallLists(1, GL_UNSIGNED_BYTE, ")");
        }

        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
#endif // 0

    glFlush();
    auxSwapBuffers();
}

// Mark DL's as invalid. The next draw will regenerate them.
void invalidate_dl(void)
{
    draw_dl_valid = FALSE;
    sel_dl_valid = FALSE;
    hl_dl_valid = FALSE;
    hl_dl_obj = NULL;
}
