#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// Routines for drawing text.

// 1MB buffer
#define TEXTBUFSIZE     (1024 * 1024)
GLfloat *textbuf = NULL;

// Make a face out of text.
Face *
text_face(Text *text)
{
    HFONT hFont, hFontOld;
    GLdouble modelMatrix[16], projMatrix[16];
    GLint viewport[4];
    int i, bufsize, n_edges, new_edges, maxc;
    Face *f;
    Edge *e;
    Point *last_point = NULL, *first_point = NULL;
    ListHead edge_list = { NULL, NULL };
    double matrix[16];
    float scale;
    BOOL closed = FALSE;

    // Map picked_point to origin, new_point to X axis, and attempt to scale the font.
    look_at_centre_d(curr_text->origin, curr_text->endpt, curr_text->plane, matrix);

    // Scale given that most fonts are proportional (2 *)
    scale = 2 * length(&curr_text->origin, &curr_text->endpt) / strlen(text->string);
    if (nz(scale))
        return NULL;

    // Make some display lists in the desired font and size
    hFont = CreateFont(12, 0, 0, 0, 
                       text->bold ? FW_BOLD : FW_NORMAL, text->italic, 
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, text->font);
    hFontOld = SelectObject(auxGetHDC(), hFont);
    wglUseFontOutlines(auxGetHDC(), 0, 256, 2000, 0, 0, WGL_FONT_LINES, NULL);

    // Draw text as line segments
    if (textbuf == NULL)
        textbuf = malloc(TEXTBUFSIZE * sizeof(GLfloat));
    glFeedbackBuffer(TEXTBUFSIZE, GL_3D, textbuf);
    glRenderMode(GL_FEEDBACK);
    glPushMatrix();
    glMultMatrixd(matrix);
    glScalef(scale, scale, scale);

    // draw the text
    glListBase(2000);
    glCallLists(strlen(text->string), GL_UNSIGNED_BYTE, text->string);

    glPopMatrix();
    bufsize = glRenderMode(GL_RENDER); 
    SelectObject(auxGetHDC(), hFontOld);
    DeleteObject(hFont);
    if (bufsize <= 0)
        return NULL;

    // get matrices
    glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Make edges out of the line segments in the buffer, and put them into a face.
    // Since TT expresses line segments clockwise, build each contour backwards.
    // Assume there will be more than one contour, and allocate a contour array.
    f = face_new(FACE_FLAT, curr_text->plane);
    maxc = 16;
    f->contours = calloc(maxc, sizeof(Contour));
    f->n_contours = 0;

    // Copy and store the text structure with the face. 
    f->text = calloc(1, sizeof(Text));
    memcpy(f->text, text, sizeof(Text));

    for (i = 0, n_edges = 0; i < bufsize; i++)
    {
        GLfloat tok = textbuf[i];
        GLdouble p[3];

        // Starting a new contour:
        // If we have a previous contour in the list, reverse it out into the face's edge array.
        if (tok == GL_LINE_RESET_TOKEN && n_edges != 0)
        {
            f->contours[f->n_contours].edge_index = f->n_edges;
            f->contours[f->n_contours].ip_index = 0;
            f->contours[f->n_contours].n_edges = n_edges;
            f->n_contours++;
            if (f->n_contours == maxc)
            {
                maxc <<= 1;
                f->contours = realloc(f->contours, maxc * sizeof(Contour));
            }

            new_edges = n_edges + f->n_edges;
            if (new_edges >= f->max_edges)
            {
                while (f->max_edges <= new_edges)
                    f->max_edges <<= 1;
                f->edges = realloc(f->edges, f->max_edges * sizeof(Edge *));
            }
            for (e = (Edge *)edge_list.head; e != NULL; e = (Edge *)e->hdr.next)
                f->edges[f->n_edges++] = e;
            ASSERT(f->n_edges == new_edges, "Edge count mismatch");

            n_edges = 0;
            edge_list.head = NULL;
            edge_list.tail = NULL;
        }
        else if (tok == GL_LINE_TOKEN && closed)
        {
            // We have already closed this contour. Skip any zero-length crap at the end.
            i += 6;
            continue;
        }

        // Start of new edge. Put it at the head of a list.
        e = edge_new(EDGE_STRAIGHT);
        link((Object *)e, &edge_list);
        n_edges++;

        // Get model coords for the window point returned in buffer
        if (tok == GL_LINE_RESET_TOKEN)
        {
            // New contour starts with this point
            gluUnProject(textbuf[i + 1], textbuf[i + 2], textbuf[i + 3], modelMatrix, projMatrix, viewport, &p[0], &p[1], &p[2]);
            e->endpoints[1] = point_new(p[0], p[1], p[2]);
            first_point = e->endpoints[1];
            closed = FALSE;
        }
        else if (tok == GL_LINE_TOKEN)
        {
            // Join on from last line segment
            ASSERT(last_point != NULL, "Continuing with no last point");
            e->endpoints[1] = last_point;
        }
        else
        {
            ASSERT(FALSE, "Unexpected token in feedback buffer");
            return NULL;
        }
        gluUnProject(textbuf[i + 4], textbuf[i + 5], textbuf[i + 6], modelMatrix, projMatrix, viewport, &p[0], &p[1], &p[2]);
        
        // Join back to start if we have arrived there (provided we have been somewhere in the meantime)
        if (near_pt_xyz(first_point, p[0], p[1], p[2], SMALL_COORD * 5) && tok == GL_LINE_TOKEN)
        {
            e->endpoints[0] = first_point;
            closed = TRUE;
            first_point = NULL;
        }
        else
        {
            e->endpoints[0] = point_new(p[0], p[1], p[2]);
        }
        last_point = e->endpoints[0];
        i += 6;
    }

    // Empty out the last contour. Don't bother creating a contour structure if it's the only one.
    if (n_edges != 0)
    {
        if (f->n_contours != 0)
        {
            f->contours[f->n_contours].edge_index = f->n_edges;
            f->contours[f->n_contours].ip_index = 0;
            f->contours[f->n_contours].n_edges = n_edges;
            f->n_contours++;
            if (f->n_contours == maxc)
            {
                maxc <<= 1;
                f->contours = realloc(f->contours, maxc * sizeof(Contour));
            }
        }

        new_edges = n_edges + f->n_edges;
        if (new_edges >= f->max_edges)
        {
            while (f->max_edges <= new_edges)
                f->max_edges <<= 1;
            f->edges = realloc(f->edges, f->max_edges * sizeof(Edge *));
        }
        for (e = (Edge *)edge_list.head; e != NULL; e = (Edge *)e->hdr.next)
            f->edges[f->n_edges++] = e;
        ASSERT(f->n_edges == new_edges, "Edge count mismatch");
    }

    f->initial_point = f->edges[0]->endpoints[0];

    // If we haven't used the contour struct, we can throw it away.
    if (f->n_contours == 0)
    {
        free(f->contours);
        f->contours = NULL;
    }

    return f;
}
