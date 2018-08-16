#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Import other types of files. Only triangle meshes are supported at the moment. 
// They are assumed to contain one contiguous body, which will become a volume
// with lots of triangular faces.

// Helpers for STL reading; find existing points by coordinate
Point *
find_point_coord(Point *pt, Point ***bucket)
{
    Point *p;
    Point **b = find_bucket(pt, bucket);

    for (p = *b; p != NULL; p = p->bucket_next)
    {
        if (near_pt(pt, p, SMALL_COORD))
            break;
    }

    if (p == NULL)
    {
        p = point_newp(pt);
        p->bucket_next = *b;
        *b = p;
    }

    return p;
}

// Find edges by endpoints, using the start_list.
Edge *
find_edge(Point *p0, Point *p1)
{
    Edge *e = NULL;

    // See if the edge is in p0's start list
    for (e = p0->start_list; e != NULL; e = e->start_next) 
    {
        if (e->endpoints[1] == p1)
            break;
    }

    if (e == NULL)
    {
        // See if the edge is in p1's start list
        for (e = p1->start_list; e != NULL; e = e->start_next)
        {
            if (e->endpoints[1] == p0)
                break;
        }
    }

    if (e == NULL)
    {
        // Create a new edge, and put it in p0's start list
        e = edge_new(EDGE_STRAIGHT);
        e->endpoints[0] = p0;
        e->endpoints[1] = p1;
        e->start_next = p0->start_list;
        p0->start_list = e;
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
    Face *tf;
    Volume *vol = NULL;
    Plane norm;
    Point pt[3];
    Point *p0, *p1, *p2;
    int n_tri = 0;
    short attrib;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    // Search for "solid ". If not found, assume it's a binary STL file.
    if (fread_s(buf, 512, 1, 6, f) != 6)
        goto error_return;
    if (strncmp(buf, "solid ", 6) == 0)
    {
        if (fgets(buf, 512, f) == NULL)
            goto error_return;
        tok = strtok_s(buf, "\n", &nexttok);  // rest of line till \n
        if (tok != NULL)
            strcpy_s(group->title, 256, tok);
        vol = vol_new();
        vol->hdr.lock = LOCK_FACES;
    }
    else
    {
        fclose(f);
        fopen_s(&f, filename, "rb");        // make sure it's binary!
        if (fread_s(buf, 512, 1, 80, f) != 80)
            goto error_return;
        strncpy_s(group->title, 80, buf, _TRUNCATE);
        group->title[79] = '\0';    // in case there's no NULL char
        if (fread_s(&n_tri, 4, 1, 4, f) != 4)
            goto error_return;
        vol = vol_new();
        vol->hdr.lock = LOCK_FACES;

        goto binary_stl;
    }

    // Read in the rest of the ASCII STL file.
    while (TRUE)
    {
        if (fgets(buf, 512, f) == NULL)
            break;

        tok = strtok_s(buf, " \t\n", &nexttok);
        if (strcmp(tok, "facet") == 0)
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

            p0 = find_point_coord(&pt[0], vol->point_bucket);
            p1 = find_point_coord(&pt[1], vol->point_bucket);
            p2 = find_point_coord(&pt[2], vol->point_bucket);
            if (near_pt(&pt[0], &pt[1], SMALL_COORD))
                continue;
            if (near_pt(&pt[1], &pt[2], SMALL_COORD))
                continue;
            if (near_pt(&pt[2], &pt[0], SMALL_COORD))
                continue;

            tf = face_new(FACE_FLAT, norm);
            tf->edges[0] = find_edge(p0, p1);
            tf->edges[1] = find_edge(p1, p2);
            tf->edges[2] = find_edge(p2, p0);
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
            link((Object *)tf, &vol->faces);
            n_tri++;
        }
        else if (strcmp(tok, "endsolid") == 0)
        {
            break;
        }
    }

    link_group((Object *)vol, group);
    empty_bucket(vol->point_bucket);
    fclose(f);
    return TRUE;

error_return:
    fclose(f);
    return FALSE;

binary_stl:
    i = 0;
    while (TRUE)
    {
        if (fread_s(&norm.A, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&norm.B, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&norm.C, 4, 1, 4, f) != 4)
            goto binary_eof;

        if (fread_s(&pt[0].x, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&pt[0].y, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&pt[0].z, 4, 1, 4, f) != 4)
            goto binary_eof;

        if (fread_s(&pt[1].x, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&pt[1].y, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&pt[1].z, 4, 1, 4, f) != 4)
            goto binary_eof;

        if (fread_s(&pt[2].x, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&pt[2].y, 4, 1, 4, f) != 4)
            goto binary_eof;
        if (fread_s(&pt[2].z, 4, 1, 4, f) != 4)
            goto binary_eof;

        if (fread_s(&attrib, 2, 1, 2, f) != 2)  // ignore this
            goto binary_eof;

        if (near_pt(&pt[0], &pt[1], SMALL_COORD))
            continue;
        if (near_pt(&pt[1], &pt[2], SMALL_COORD))
            continue;
        if (near_pt(&pt[2], &pt[0], SMALL_COORD))
            continue;

        p0 = find_point_coord(&pt[0], vol->point_bucket);
        p1 = find_point_coord(&pt[1], vol->point_bucket);
        p2 = find_point_coord(&pt[2], vol->point_bucket);

        tf = face_new(FACE_FLAT, norm);
        tf->edges[0] = find_edge(p0, p1);
        tf->edges[1] = find_edge(p1, p2);
        tf->edges[2] = find_edge(p2, p0);
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
        link((Object *)tf, &vol->faces);
        i++;
    }

binary_eof:
    link_group((Object *)vol, group);
    empty_bucket(vol->point_bucket);
    fclose(f);
    return TRUE;
}

#if 0
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
        link((Object *)tf, &vol->faces);
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
#endif // 0

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

        if (p1 == p2 || p2 == p3 || p1 == p3)
            ASSERT(FALSE, "Degenerate triangles");
        if (near_pt(points[p1], points[p2], SMALL_COORD))
            continue;
        if (near_pt(points[p2], points[p3], SMALL_COORD))
            continue;
        if (near_pt(points[p3], points[p1], SMALL_COORD))
            continue;
        tf->edges[0] = find_edge(points[p1], points[p2]);
        tf->edges[1] = find_edge(points[p2], points[p3]);
        tf->edges[2] = find_edge(points[p3], points[p1]);
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
        link((Object *)tf, &vol->faces);
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



