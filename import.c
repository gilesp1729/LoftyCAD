#include "stdafx.h"
#include "LoftyCAD.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <stdio.h>

// Globals for readers (shared by several routines)
char buf[512];

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

// Find a material in the list by name, and return its index, or zero if not found.
int
find_material(char* mat_name)
{
    int mat;

    for (mat = 0; mat < MAX_MATERIAL; mat++)
    {
        if (materials[mat].valid)
        {
            if (strcmp(mat_name, materials[mat].name) == 0)
                return mat;
        }
    }
    return 0;
}

// Read a Wavefront OBJ file
BOOL
read_obj_to_group(Group* group, char* filename)
{
    FILE *f;
    char* tok;
    char* nexttok = NULL;
    int npoints, npoints_alloced;
    Point** points;
    Volume* vol;
    int mat, mat_offset;

    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    npoints = 0;
    npoints_alloced = 64;   // start with a small power of 2
    points = malloc(npoints_alloced * sizeof(Point*));     // point array

    mat = 0;
    while (1)
    {
        if (fgets(buf, 512, f) == NULL)     // skip any comments and blank lines
            goto error;
        tok = strtok_s(buf, " \t\n", &nexttok);
        if (tok == NULL)
            continue;
        if (strcmp(tok, "usemtl") == 0)     // set the material for the first volume
        {
            tok = strtok_s(NULL, "\n", &nexttok);
            mat = find_material(tok);
            continue;
        }
        if (tok[0] == 'f')                  // beginning of the faces
            break;
        if (strcmp(tok, "mtllib") == 0)     // material library filename
        {
            FILE* mtl;
            char mtlfile[256];

            // If the filename does not have a directory, assume it's alongside the .obj file
            tok = strtok_s(NULL, " \t\n", &nexttok);
            if (strchr(tok, '\\') == NULL)
            {
                char* slosh;

                strcpy_s(mtlfile, 256, filename);
                slosh = strrchr(mtlfile, '\\');
                *(slosh + 1) = '\0';
                strcat_s(mtlfile, 256, tok);
            }
            else
            {
                strcpy_s(mtlfile, 256, tok);
            }

            fopen_s(&mtl, mtlfile, "rt");
            if (mtl != NULL);
            {
                char* nexttok2 = NULL;

                // add materials to the existing material collection (if any)
                mat_offset = 0;
                for (mat = 0; mat < MAX_MATERIAL; mat++)
                {
                    if (materials[mat].valid)
                        mat_offset = mat;
                }
                mat = mat_offset + 1;
                while (1)
                {
                    if (fgets(buf, 512, mtl) == NULL)
                        break;
                    tok = strtok_s(buf, " \t\n", &nexttok2);
                    if (tok == NULL)
                        continue;
                    if (strcmp(tok, "newmtl") == 0)
                    {
                        tok = strtok_s(NULL, "\n", &nexttok2);
                        strcpy_s(materials[mat].name, 64, tok);
                        while (1)
                        {
                            if (fgets(buf, 512, mtl) == NULL)
                                break;
                            tok = strtok_s(buf, " \t\n", &nexttok2);
                            if (strcmp(tok, "Kd") == 0)
                            {
                                tok = strtok_s(NULL, " \t\n", &nexttok2);
                                materials[mat].color[0] = (float)atof(tok);
                                tok = strtok_s(NULL, " \t\n", &nexttok2);
                                materials[mat].color[1] = (float)atof(tok);
                                tok = strtok_s(NULL, " \t\n", &nexttok2);
                                materials[mat].color[2] = (float)atof(tok);

                                materials[mat].hidden = FALSE;
                                materials[mat].valid = TRUE;
                                materials[mat].shiny = 30;
                                mat++;
                                break;
                            }
                        }
                    }
                }
                fclose(mtl);
                mat = 0;
            }
        }
        else if (tok[0] == 'v')             // read a vertex
        {
            Point* p = point_new(0, 0, 0);

            tok = strtok_s(NULL, " \t\n", &nexttok);
            p->x = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            p->y = (float)atof(tok);
            tok = strtok_s(NULL, " \t\n", &nexttok);
            p->z = (float)atof(tok);
            if (npoints >= npoints_alloced)
            {
                npoints_alloced *= 2;
                points = realloc(points, npoints_alloced * sizeof(Point*));
            }
            points[npoints++] = p;
        }
    }

    // Start the first volume
    vol = vol_new();
    vol->material = mat;
    while (1)
    {
        Plane dummy = { 0, };
        Face* tf = face_new(FACE_FLAT, dummy);
        int p1, p2, p3;

        // We already have 'f' on entry to this loop
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p1 = atoi(tok) - 1;
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p2 = atoi(tok) - 1;
        tok = strtok_s(NULL, " \t\n", &nexttok);
        p3 = atoi(tok) - 1;

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
        link((Object*)tf, &vol->faces);

        // Read a new line
        while (1)
        {
            if (fgets(buf, 512, f) == NULL)     // skip any comments and blank lines
                goto eof;
            tok = strtok_s(buf, " \t\n", &nexttok);
            if (tok == NULL)
                continue;
            if (strcmp(tok, "usemtl") == 0)     // set the material for subsequent volume
            {
                tok = strtok_s(NULL, "\n", &nexttok);
                mat = find_material(tok);

                // switch to new volume for new material
                link_group((Object*)vol, group);
                vol = vol_new();
                vol->material = mat;
                continue;
            }
            if (tok[0] == 'f')
                break;                          // ready to read next face
        }
    }

eof:
    link_group((Object*)vol, group);
    fclose(f);
    free(points);
    return TRUE;

error:
    fclose(f);
    free(points);
    return FALSE;
}

// Read a Geomview (OOGL) OFF file
BOOL
read_off_to_group(Group *group, char *filename)
{
    FILE *f;
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

// AMF globals
float scale;
char* tok;
char* nexttok = NULL;
int npoints, npoints_alloced;
Point** points;
int mat;
Volume* vol;
Plane dummy = { 0, };
int mat_offset;

// AMF file readers to deal with XML prolixity.

// Read next token from file to tok. Return FALSE on eof.
BOOL
next_token(FILE* f)
{
    if (tok != NULL)
        tok = strtok_s(NULL, " >\n", &nexttok);
    while (tok == NULL)
    {
        if (fgets(buf, 512, f) == NULL)
            return FALSE;
        tok = strtok_s(buf, " >\n", &nexttok);
    }
    return TRUE;
}

// Read till a given string (delimited by spaces or '>') is encountered, skipping white space and newlines.
// Return FALSE if string is never found.
BOOL
read_till(char *string, FILE* f)
{
    while (1)
    {
        if (!next_token(f))
            return FALSE;
        if (strcmp(tok, string) == 0)
            return TRUE;
    }
}

// Read one vertex. Return FALSE if there are none left (</vertices> trailer is read)
BOOL
read_amf_vertex(Group* group, FILE* f)
{
    float x, y, z;

    if (!next_token(f))
        return FALSE;
    if (strcmp(tok, "</vertices") == 0)
        return FALSE;

    read_till("<coordinates", f);
    read_till("<x", f);

    tok = strtok_s(NULL, "<", &nexttok);
    x = (float)atof(tok);
    read_till("<y", f);
    tok = strtok_s(NULL, "<", &nexttok);
    y = (float)atof(tok);
    read_till("<z", f);
    tok = strtok_s(NULL, "<", &nexttok);
    z = (float)atof(tok);
    read_till("</vertex", f);

    if (npoints >= npoints_alloced)
    {
        npoints_alloced *= 2;
        points = realloc(points, npoints_alloced * sizeof(Point*));
    }
    points[npoints++] = point_new(x, y, z);

    return TRUE;
}

// Read a vertices list. Return FALSE on error/eof.
BOOL
read_amf_vertices(Group* group, FILE* f)
{
    // skip till we read vertices header
    if (!read_till("<vertices", f))
        return FALSE;

    npoints = 0;
    npoints_alloced = 64;   // start with a small power of 2
    points = malloc(npoints_alloced * sizeof(Point*));     // point array

    // read each vertex until end of vertices encountered
    while (read_amf_vertex(group, f))
        ;
    return TRUE;
}

// Read a triangle. Return FALSE if there are none left (</volume> is read)
BOOL
read_amf_triangle(Group* group, FILE* f)
{
    int p1, p2, p3;
    Face* tf;

    if (!next_token(f))
        return FALSE;
    if (strcmp(tok, "</volume") == 0)
        return FALSE;
    if (!read_till("<v1", f))
        return FALSE;
    if (!next_token(f))
        return FALSE;
    p1 = atoi(tok);
    if (!read_till("<v2", f))
        return FALSE;
    if (!next_token(f))
        return FALSE;
    p2 = atoi(tok);
    if (!read_till("<v3", f))
        return FALSE;
    if (!next_token(f))
        return FALSE;
    p3 = atoi(tok);

    read_till("</triangle", f);

    tf = face_new(FACE_FLAT, dummy);
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
    link((Object*)tf, &vol->faces);
    return TRUE;
}

// Read a volume. Return FALSE if there are none left (</mesh > trailer is read)
BOOL
read_amf_volume(Group* group, FILE* f)
{
    mat = 0;
    if (!next_token(f))
        return FALSE;
    if (strcmp(tok, "</mesh") == 0)
        return FALSE;
    else if (strcmp(tok, "<volume") == 0)
    {
        tok = strtok_s(NULL, " =\"\n", &nexttok);
        if (tok != NULL && strcmp(tok, "materialid") == 0)
        {
            tok = strtok_s(NULL, "\"", &nexttok);
            mat = atoi(tok) + mat_offset;
        }
    }

    vol = vol_new();
    vol->material = mat;

    // read each triangle until end of volume encountered
    while (read_amf_triangle(group, f))
        ;
    link_group((Object*)vol, group);

    return TRUE;
}

// Read an object section. Return FALSE on error/eof.
BOOL
read_amf_object(Group* group, FILE* f)
{
    // skip metadata and object headers
    if (!read_till("<object", f))
        return FALSE;

    // read vertices
    if (!read_amf_vertices(group, f))
        return FALSE;

    // read volumes until end of object encountered
    while (read_amf_volume(group, f))
        ;

    // read the end of the object section so we're ready for materials
    read_till("</object", f);
    return TRUE;
}

// Read a material. Return FALSE if there are none left (</amf> trailer is read, and that's your lot)
BOOL
read_amf_material(Group* group, FILE* f)
{
    if (!next_token(f))
        return FALSE;
    if (strcmp(tok, "</amf") == 0)
        return FALSE;

    if (strcmp(tok, "<material") == 0)
    {
        tok = strtok_s(NULL, " =\"\n", &nexttok);
        if (tok != NULL && strcmp(tok, "id") == 0)
        {
            tok = strtok_s(NULL, "\"", &nexttok);
            mat = atoi(tok) + mat_offset;
        }
    }

    while (1)
    {
        if (!next_token(f))
            return FALSE;
        if (strcmp(tok, "</material") == 0)
            break;

        if (strcmp(tok, "<metadata") == 0)
        {
            tok = strtok_s(NULL, ">\n", &nexttok);
            if (tok != NULL && strcmp(tok, "type=\"name\"") == 0)
            {
                tok = strtok_s(NULL, "<", &nexttok);
                strcpy_s(materials[mat].name, 64, tok);
                next_token(f);              // swallow "/metadata>\n"
            }
            else
                read_till("<metadata", f);  // it's some other kind of metadata
        }
        else if (strcmp(tok, "<color") == 0)
        {
            if (!read_till("<r", f))
                return FALSE;
            if (!next_token(f))
                return FALSE;
            materials[mat].color[0] = (float)atof(tok);
            if (!read_till("<g", f))
                return FALSE;
            if (!next_token(f))
                return FALSE;
            materials[mat].color[1] = (float)atof(tok);
            if (!read_till("<b", f))
                return FALSE;
            if (!next_token(f))
                return FALSE;
            materials[mat].color[2] = (float)atof(tok);
            
            materials[mat].hidden = FALSE;
            materials[mat].valid = TRUE;
            materials[mat].shiny = 30;

            read_till("</color", f);
        }
    }

    return TRUE;
}

// Read an AMF file to a group. Currently only one object section is supported, and constellations
// are not read.
BOOL
read_amf_to_group(Group* group, char* filename)
{
    FILE *f;
    
    scale = 1.0f;
    nexttok = NULL;
    fopen_s(&f, filename, "rt");
    if (f == NULL)
        return FALSE;

    // Find last valid existing material, and use that to offset material ID's in file.
    // Material ID's in AMF are always numbered from 1 on (never 0)
    mat_offset = 0;
    for (mat = 0; mat < MAX_MATERIAL; mat++)
    {
        if (materials[mat].valid)
            mat_offset = mat;
    }

    // read unit from AMF header
    tok = NULL;
    if (!read_till("<amf", f))
        goto eof_error;

    // look for unit="inch" or unit="millimeter" or "mm" (the default)
    tok = strtok_s(NULL, " ", &nexttok);
    if (strcmp(tok, "unit=\"inch\"") == 0)
        scale = 25.4f;

    // read object description
    if (!read_amf_object(group, f))
        goto eof_error;

    free(points);

    // read materials until trailer encountered
    while (read_amf_material(group, f))
        ;

    fclose(f);
    return TRUE;

eof_error:
    fclose(f);
    return FALSE;
}

#if 0
// Read a file of Bezier points to a group.
// Flexible format. Lines are assumed to contain one or more XYZ points (each of 3 floats).
// Any line that is blank, has non-numeric text, or contains fewer than 3 numbers is discarded.
// Groups of 16 are gathered into Bezier faces. The resulting volume may or may not be
// manifold (mainly used for renditions of the famous Utah Teapot)
BOOL
read_bezpts_to_group(Group* group, char* filename)
{
    FILE* f;
    char* tok;
    char* nexttok = NULL;
    int i;
    Point* points = NULL;
    Face* tf;
    Volume* vol = NULL;
    Plane norm;
    Point pt[3];
    Point* p0, * p1, * p2;
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






    // Read in the rest of the file.
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
            link((Object*)tf, &vol->faces);
            n_tri++;
        }
        else if (strcmp(tok, "endsolid") == 0)
        {
            break;
        }
    }

    link_group((Object*)vol, group);
    empty_bucket(vol->point_bucket);
    fclose(f);
    return TRUE;

error_return:
    fclose(f);
    return FALSE;
}

#endif // 0