#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Import other types of files. Only triangle meshes are supported at the moment. 
// They are assumed to contain one contiguous body, which will become a volume
// with lots of triangular faces.

// Helpers for STL reading; find existing points and edges
Point *
find_point(Point *pt, Point **points)
{
    Point *p;

    for (p = *points; p != NULL; p = (Point *)p->hdr.next)
    {
        if (near_pt(pt, p))
            break;
    }
    if (p == NULL)
    {
        p = point_newp(pt);
        link((Object *)p, (Object **)points);
    }

    return p;
}

Edge *
find_edge(Point *p0, Point *p1, Edge **edges)
{
    Edge *e;

    for (e = *edges; e != NULL; e = (Edge *)e->hdr.next)
    {
        if (near_pt(e->endpoints[0], p0) && near_pt(e->endpoints[1], p1))
            break;
        if (near_pt(e->endpoints[0], p1) && near_pt(e->endpoints[1], p0))
            break;
    }
    if (e == NULL)
    {
        e = edge_new(EDGE_STRAIGHT);
        e->endpoints[0] = p0;
        e->endpoints[1] = p1;
        link((Object *)e, (Object **)edges);
    }

    return e;
}

// Read an STL mesh
BOOL
read_stl_to_group(Group *group, char *filename)
{
    FILE *f;
    char buf[512];
    char *tok;
    char *nexttok = NULL;
    int i;
    Point *points = NULL;
    Edge *edges = NULL;
    Face *tf;
    Volume *vol = NULL;
    Plane norm;
    Point pt[3];
    Point *p0, *p1, *p2;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    while (TRUE)
    {
        if (fgets(buf, 512, f) == NULL)
            break;

        tok = strtok_s(buf, " \t\n", &nexttok);
        if (strcmp(tok, "solid") == 0)
        {
            tok = strtok_s(NULL, "\n", &nexttok);  // rest of line till \n
            if (tok != NULL)
                strcpy_s(group->title, 256, tok);
            vol = vol_new();
        }
        else if (strcmp(tok, "facet") == 0)
        {
            tok = strtok_s(NULL, " \t\n", &nexttok);  // absorb "normal"
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.A = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.B = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            norm.C = (float)atof(tok);
            i = 0;
        }
        else if (strcmp(tok, "vertex") == 0)
        {
            tok = strtok_s(NULL, " \t\n", &nexttok);
            pt[i].x = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            pt[i].y = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            pt[i].z = (float)atof(tok);
            i++;
        }
        else if (strcmp(tok, "endfacet") == 0)
        {
            if (i != 3)
                goto error_return;

            p0 = find_point(&pt[0], &points);
            p1 = find_point(&pt[1], &points);
            p2 = find_point(&pt[2], &points);

            tf = face_new(FACE_FLAT, norm);
            tf->edges[0] = find_edge(p0, p1, &edges);
            tf->edges[1] = find_edge(p1, p2, &edges);
            tf->edges[2] = find_edge(p2, p0, &edges);
            tf->n_edges = 3;
            if
                (
                tf->edges[0]->endpoints[1] == tf->edges[1]->endpoints[0]
                ||
                tf->edges[0]->endpoints[1] == tf->edges[1]->endpoints[1]
                )
                tf->initial_point = tf->edges[0]->endpoints[0];
            else
                tf->initial_point = tf->edges[0]->endpoints[1];

            if (vol == NULL)
                goto error_return;      // have not seen "solid" at the beginning

            tf->vol = vol;
            link((Object *)tf, (Object **)&vol->faces);
        }
        else if (strcmp(tok, "endsolid") == 0)
        {
            break;
        }
    }

    link_group((Object *)vol, group);
    fclose(f);
    return TRUE;

error_return:
    fclose(f);
    return FALSE;
}

// Read a GTS file
BOOL
read_gts_to_group(Group *group, char *filename)
{
    FILE *f;
    char buf[512];
    char *tok;
    char *nexttok = NULL;
    int i, npoints, nedges, nfaces;
    Point **points;
    Edge **edges;
    Volume *vol;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    if (fgets(buf, 512, f) == NULL)     // npoints, nedges, nfaces
        return FALSE;
    tok = strtok_s(buf, " \t\n", &nexttok);
    npoints = atoi(tok);
    tok = strtok_s(NULL, " \t\n", &nexttok);
    nedges = atoi(tok);
    tok = strtok_s(NULL, " \t\n", &nexttok);
    nfaces = atoi(tok);

    points = malloc((npoints + 1) * sizeof(Point *));   // point array (note indexed from 1)
    edges = malloc((nedges + 1) * sizeof(Edge *));      // edge array similarly

    for (i = 0; i < npoints; i++)
    {
        Point *p = point_new(0, 0, 0);

        nexttok = NULL;
        if (fgets(buf, 512, f) == NULL)
            goto error;
        tok = strtok_s(buf, " \t\n", &nexttok);
        p->x = (float)atof(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p->y = (float)atof(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p->z = (float)atof(tok);
        points[i + 1] = p;
    }

    for (i = 0; i < nedges; i++)
    {
        Edge *e = edge_new(EDGE_STRAIGHT);
        int p1, p2;

        nexttok = NULL;
        if (fgets(buf, 512, f) == NULL)
            goto error;
        tok = strtok_s(buf, " \t\n", &nexttok);
        p1 = atoi(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p2 = atoi(tok);
        e->endpoints[0] = points[p1];
        e->endpoints[1] = points[p2];
        edges[i + 1] = e;
    }

    vol = vol_new();
    for (i = 0; i < nfaces; i++)
    {
        Plane dummy = { 0, };
        Face *tf = face_new(FACE_FLAT, dummy);
        int e1, e2, e3;

        nexttok = NULL;
        if (fgets(buf, 512, f) == NULL)
            goto error;

        tok = strtok_s(buf, " \t\n", &nexttok);
        e1 = atoi(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        e2 = atoi(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        e3 = atoi(tok);

        tf->edges[0] = edges[e1];
        tf->edges[1] = edges[e2];
        tf->edges[2] = edges[e3];
        tf->n_edges = 3;
        if 
        (
            tf->edges[0]->endpoints[1] == tf->edges[1]->endpoints[0] 
            || 
            tf->edges[0]->endpoints[1] == tf->edges[1]->endpoints[1]
        )
            tf->initial_point = tf->edges[0]->endpoints[0];
        else
            tf->initial_point = tf->edges[0]->endpoints[1];
        tf->vol = vol;
        link((Object *)tf, (Object **)&vol->faces);
    }

    link_group((Object *)vol, group);
    fclose(f);
    free(points);
    free(edges);
    return TRUE;

error:
    fclose(f);
    free(points);
    free(edges);
    return FALSE;
}

// Read a Geomview (OOGL) OFF file
BOOL
read_off_to_group(Group *group, char *filename)
{
    FILE *f;
    char buf[512];
    char *tok;
    char *nexttok = NULL;
    int i, npoints, nedges, nfaces;
    Point **points;
    Volume *vol;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    if (fgets(buf, 512, f) == NULL)     // skip "OFF"
        return FALSE;
    if (fgets(buf, 512, f) == NULL)     // npoints, faces, nedges (note order different from GTS)
        return FALSE;
    tok = strtok_s(buf, " \t\n", &nexttok);
    npoints = atoi(tok);
    tok = strtok_s(NULL, " \t\n", &nexttok);
    nfaces = atoi(tok);
    tok = strtok_s(NULL, " \t\n", &nexttok);
    nedges = atoi(tok);

    points = malloc(npoints * sizeof(Point *));     // point array

    for (i = 0; i < npoints; i++)
    {
        Point *p = point_new(0, 0, 0);

        nexttok = NULL;
        if (fgets(buf, 512, f) == NULL)
            goto error;
        tok = strtok_s(buf, " \t\n", &nexttok);
        p->x = (float)atof(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p->y = (float)atof(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p->z = (float)atof(tok);
        points[i] = p;
    }

    vol = vol_new();
    for (i = 0; i < nfaces; i++)
    {
        Plane dummy = { 0, };
        Face *tf = face_new(FACE_FLAT, dummy);
        int np, p1, p2, p3;

        nexttok = NULL;
        if (fgets(buf, 512, f) == NULL)
            goto error;

        tok = strtok_s(buf, " \t\n", &nexttok);
        np = atoi(tok);
        ASSERT(np == 3, "faces must be triangles");

        tok = strtok_s(NULL, " \t\n", &nexttok);
        p1 = atoi(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p2 = atoi(tok);
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p3 = atoi(tok);

        tf->edges[0] = edge_new(EDGE_STRAIGHT);         // TODO - this is a HUGE waste of edges. Share 'em.
        tf->edges[0]->endpoints[0] = points[p1];
        tf->edges[0]->endpoints[1] = points[p2];
        tf->edges[1] = edge_new(EDGE_STRAIGHT);
        tf->edges[1]->endpoints[0] = points[p2];
        tf->edges[1]->endpoints[1] = points[p3];
        tf->edges[2] = edge_new(EDGE_STRAIGHT);
        tf->edges[2]->endpoints[0] = points[p3];
        tf->edges[2]->endpoints[1] = points[p1];
        tf->n_edges = 3;
        tf->initial_point = tf->edges[0]->endpoints[0];
        tf->vol = vol;
        link((Object *)tf, (Object **)&vol->faces);
    }

    link_group((Object *)vol, group);
    fclose(f);
    free(points);
    return TRUE;

error:
    fclose(f);
    free(points);
    return FALSE;
}



