#include "stdafx.h"
#include "LoftyCAD.h"
#include <stdio.h>

// globals for STL/AMF/OBJ/OFF file writing
// File to export STL/AMF/OBJ/OFF to (global so callbacks can see it)
FILE *stl, *off, *amf, *amfv, *objf, *objv, *mtl;

// number of triangles exported
int num_exported_tri;

// number of vertices for an OFF file
int num_exported_vertices;

// reindex array to count vertex indices in an OFF file
int *reindex;


// Write a single mesh triangle with normal out to an STL file
void
export_triangle_stl(void *arg, float x[3], float y[3], float z[3])
{
    int i;
    float A, B, C, length;

    cross(x[1] - x[0], y[1] - y[0], z[1] - z[0], x[2] - x[0], y[2] - y[0], z[2] - z[0], &A, &B, &C);
    length = (float)sqrt(A * A + B * B + C * C);
    if (!nz(length))
    {
        A /= length;
        B /= length;
        C /= length;
    }

    fprintf_s(stl, "facet normal %f %f %f\n", A, B, C);
    fprintf_s(stl, "  outer loop\n");
    for (i = 0; i < 3; i++)
        fprintf_s(stl, "    vertex %f %f %f\n", x[i], y[i], z[i]);
    fprintf_s(stl, "  endloop\n");
    fprintf_s(stl, "endfacet\n");
    num_exported_tri++;
}

// Write a vertex out to an OFF file, counting is zero-based position along the way
void
export_vertex_off(void* arg, Vertex_index* v, float x, float y, float z)
{
    fprintf_s(off, "%f %f %f\n", x, y, z);
    reindex[*(int*)v] = num_exported_vertices++;
}

void
export_vertex_off_d(void* arg, Vertex_index* v, double x, double y, double z)
{
    fprintf_s(off, "%.15f %.15f %.15f\n", x, y, z);
    reindex[*(int*)v] = num_exported_vertices++;
}

// Write a single mesh triangle out to an OFF file
void
export_triangle_off(void* arg, int nv, Vertex_index* vi)
{
    int* ivi = (int*)vi;
    int i;

    fprintf_s(off, "%d", nv);
    for (i = 0; i < nv; i++)
        fprintf_s(off, " %d", reindex[ivi[i]]);
    fprintf_s(off, "\n");
}

// Write a vertex out to an AMF file
void
export_vertex_amf(void* arg, Vertex_index* v, float x, float y, float z)
{
    fprintf_s(amf, "        <vertex><coordinates><x>%f</x><y>%f</y><z>%f</z></coordinates></vertex>\n", x, y, z);
    reindex[*(int*)v] = num_exported_vertices++;
}

// Write a single mesh triangle out to an AMF file (actually, to the AMF temp volume file)
void
export_triangle_amf(void* arg, int nv, Vertex_index* vi)
{
    int* ivi = (int*)vi;
    int i;

    fprintf_s(amfv, "        <triangle>");
    for (i = 0; i < nv; i++)
        fprintf_s(amfv, "<v%d>%d</v%d>", i+1, reindex[ivi[i]], i+1);
    fprintf_s(amfv, "</triangle>\n");
}

// Write a vertex out to an OBJ file
void
export_vertex_obj(void* arg, Vertex_index* v, float x, float y, float z)
{
    fprintf_s(objf, "v %f %f %f\n", x, y, z);
    reindex[*(int*)v] = num_exported_vertices++;
}

// Write a single mesh triangle out to an OBJ file (actually, to the OBJ temp volume file)
void
export_triangle_obj(void* arg, int nv, Vertex_index* vi)
{
    int* ivi = (int*)vi;
    int i;

    fprintf_s(objv, "f ");
    for (i = 0; i < nv; i++)
        fprintf_s(objv, "%d ", reindex[ivi[i]] + 1);  // vertices start from 1
    fprintf_s(objv, "\n");
}

// Render an un-merged volume or group to triangles and export it to an STL file
void
export_unmerged_object_stl(Object *obj)
{
    Object *o;
    Volume *vol;

    switch (obj->type)
    {
    case OBJ_VOLUME:
        vol = (Volume *)obj;
        if (!vol->mesh_merged)
            mesh_foreach_face_coords(((Volume *)obj)->mesh, export_triangle_stl, NULL);
        break;

    case OBJ_GROUP:
        for (o = ((Group *)obj)->obj_list.head; o != NULL; o = o->next)
            export_unmerged_object_stl(o);
        break;
    }
}

// export every volume to various kinds of files
void
export_object_tree(Group *tree, char *filename, int file_index)
{
    Object *obj;
    char buf[64], tmpdir[256], basename[256], tmp[256];
    char* dot;
    int i, k, baselen;
    int candidates[MAX_MATERIAL];

    ASSERT(tree->mesh != NULL, "Tree mesh NULL");
    ASSERT(tree->mesh_valid, "Tree mesh not valid");
    if (tree->mesh == NULL || !tree->mesh_valid)
        return;

    // TODO: Send this to a new status bar down the bottom, as well as the debug log
    if (!tree->mesh_complete)
        Log("Mesh incomplete - writing unmerged objects\r\n");

    switch (file_index)
    {
    case 1: // Export to an STL file
        fopen_s(&stl, filename, "wt");
        if (stl == NULL)
            return;
        fprintf_s(stl, "solid %s\n", tree->title);

        num_exported_tri = 0;
        if (tree->mesh != NULL && tree->mesh_valid && !tree->mesh_merged)
            mesh_foreach_face_coords(tree->mesh, export_triangle_stl, NULL);

        sprintf_s(buf, 64, "Mesh: %d triangles\r\n", num_exported_tri);
        Log(buf);

        if (!tree->mesh_complete)
        {
            for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
            {
                if (obj->type == OBJ_VOLUME || obj->type == OBJ_GROUP)
                    export_unmerged_object_stl(obj);
            }
            sprintf_s(buf, 64, "Unmerged: %d triangles total\r\n", num_exported_tri);
            Log(buf);
        }

        fprintf_s(stl, "endsolid %s\n", tree->title);
        fclose(stl);
        break;

    case 2: // export each visible material to separate STL files
        // build a list of all the non-hidden material indices
        for (i = k = 0; i < MAX_MATERIAL; i++)
        {
            if (materials[i].valid && !materials[i].hidden)
                candidates[k++] = i;
        }

        // Remove ".STL" and append the material number to the base filename
        dot = strrchr(filename, '.');
        *dot = '\0';

        // for each one of these, hide all the others, generate the surface and export it
        for (i = 0; i < k; i++)
        {
            char name[256];
            int j;

            for (j = 0; j < k; j++)
                materials[candidates[j]].hidden = TRUE;
            materials[candidates[i]].hidden = FALSE;

            xform_list.head = NULL;
            xform_list.tail = NULL;
            if (object_tree.mesh != NULL)
                mesh_destroy(object_tree.mesh);
            object_tree.mesh = NULL;
            object_tree.mesh_valid = FALSE;
            gen_view_list_tree_surfaces(&object_tree, &object_tree);

            sprintf_s(name, 256, "%s_%d.STL", filename, candidates[i]);
            export_object_tree(&object_tree, name, 1);
        }

        // reinstate all the non-hidden materials and mark the surface mesh for regeneration
        for (i = 0; i < k; i++)
            materials[candidates[i]].hidden = FALSE;

        if (object_tree.mesh != NULL)
            mesh_destroy(object_tree.mesh);
        object_tree.mesh = NULL;
        object_tree.mesh_valid = FALSE;
        break;

    case 3: // export to an AMF file
        fopen_s(&amf, filename, "wt");
        if (amf == NULL)
            return;

        // make a temp filename for the AMF volumes
        dot = strrchr(filename, '\\');
        if (dot != NULL)
            strcpy_s(basename, 256, dot + 1);          // cut off any directory in file
        else
            strcpy_s(basename, 256, filename);
        baselen = strlen(basename);
        if (baselen > 4 && (dot = strrchr(basename, '.')) != NULL)
            *dot = '\0';                               // cut off ".amf" 
        GetTempPath(256, tmpdir);
        sprintf_s(tmp, 256, "%s%s.amfv", tmpdir, basename);
        fopen_s(&amfv, tmp, "wt");
        if (amfv == NULL)
            return;

        // put out the header
        num_exported_tri = 0;
        num_exported_vertices = 0;
        fprintf_s(amf, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
        fprintf_s(amf, "<amf unit=\"mm\" version=\"1.1\">\n");
        fprintf_s(amf, "  <object id=\"1\">\n");
        fprintf_s(amf, "  <metadata type=\"slic3r.extruder\">0</metadata>\n");
        fprintf_s(amf, "    <mesh>\n");
        fprintf_s(amf, "      <vertices>\n");

        // Node renumbering array (assumes full mesh is built beforehand..)
        reindex = (int*)calloc(mesh_num_vertices(tree->mesh), sizeof(int));

        // build a list of all the non-hidden material indices
        for (i = k = 0; i < MAX_MATERIAL; i++)
        {
            if (materials[i].valid && !materials[i].hidden)
                candidates[k++] = i;
        }

        // for each one of these, hide all the others, generate the surface and export it
        for (i = 0; i < k; i++)
        {
            int j;

            for (j = 0; j < k; j++)
                materials[candidates[j]].hidden = TRUE;
            materials[candidates[i]].hidden = FALSE;

            xform_list.head = NULL;
            xform_list.tail = NULL;
            if (object_tree.mesh != NULL)
                mesh_destroy(object_tree.mesh);
            object_tree.mesh = NULL;
            object_tree.mesh_valid = FALSE;
            gen_view_list_tree_surfaces(&object_tree, &object_tree);

            if (!object_tree.mesh_valid)    // nothing for this material
                continue;

            // vertices for the mesh for this material
            mesh_foreach_vertex(tree->mesh, export_vertex_amf, NULL);

            // AMF volume for this material (write it to a temp file and append it at the end)
            if (candidates[i] != 0)
                fprintf_s(amfv, "      <volume materialid=\"%d\">\n", candidates[i]);
            else
                fprintf_s(amfv, "      <volume>\n");
            fprintf_s(amfv, "        <metadata type=\"slic3r.extruder\">%d</metadata>\n", candidates[i]);
            mesh_foreach_face_vertices(tree->mesh, export_triangle_amf, NULL);
            fprintf_s(amfv, "      </volume>\n");
        }

        free(reindex);
        fprintf_s(amf, "      </vertices>\n");

        // Append the temp file to the AMF file and write the trailer
        fclose(amfv);
        fopen_s(&amfv, tmp, "rt");
        while (1)
        {
            fgets(basename, 256, amfv);
            if (feof(amfv))
                break;

            fputs(basename, amf);
        }

        fprintf_s(amf, "    </mesh>\n");
        fprintf_s(amf, "  </object>\n");

        // write the materials (leave out material 0)
        for (i = 1; i < k; i++)
        {
            fprintf(amf, "  <material id=\"%d\">\n", candidates[i]);
            fprintf(amf, "    <metadata type=\"name\">%s</metadata>\n", materials[candidates[i]].name);
            fprintf(amf, "    <color><r>%f</r><g>%f</g><b>%f</b></color>\n",
                materials[candidates[i]].color[0],
                materials[candidates[i]].color[1],
                materials[candidates[i]].color[2]);
            fprintf(amf, "  </material>\n");
        }

        fprintf_s(amf, "</amf>\n");
        fclose(amf);
        fclose(amfv);
        DeleteFile(tmp);

        // reinstate all the non-hidden materials and mark the surface mesh for regeneration
        for (i = 0; i < k; i++)
            materials[candidates[i]].hidden = FALSE;

        if (object_tree.mesh != NULL)
            mesh_destroy(object_tree.mesh);
        object_tree.mesh = NULL;
        object_tree.mesh_valid = FALSE;
        break;

    case 4: // export to an OBJ file
        fopen_s(&objf, filename, "wt");
        if (objf == NULL)
            return;

        // make a temp filename for the OBJ volumes
        dot = strrchr(filename, '\\');
        if (dot != NULL)
            strcpy_s(basename, 256, dot + 1);          // cut off any directory in file
        else
            strcpy_s(basename, 256, filename);
        baselen = strlen(basename);
        if (baselen > 4 && (dot = strrchr(basename, '.')) != NULL)
            *dot = '\0';                               // cut off ".obj" 
        GetTempPath(256, tmpdir);
        sprintf_s(tmp, 256, "%s%s.objv", tmpdir, basename);
        fopen_s(&objv, tmp, "wt");
        if (objv == NULL)
            return;

        // put out the header
        num_exported_tri = 0;
        num_exported_vertices = 0;
        fprintf_s(objf, "# Exported by LoftyCAD\n");

        // build a list of all the non-hidden material indices
        for (i = k = 0; i < MAX_MATERIAL; i++)
        {
            if (materials[i].valid && !materials[i].hidden)
                candidates[k++] = i;
        }

        // material library file, if there are materials
        if (k > 1)
        {
            char mtlname[256];

            strcpy_s(basename, 256, filename);
            baselen = strlen(basename);
            if (baselen > 4 && (dot = strrchr(basename, '.')) != NULL)
                *dot = '\0';                               // cut off ".obj" 
            sprintf_s(mtlname, 256, "%s.mtl", basename);
            fopen_s(&mtl, mtlname, "wt");
            if (mtl == NULL)
                return;

            dot = strrchr(mtlname, '\\');
            if (dot != NULL)
                fprintf(objf, "mtllib %s\n", dot + 1);      // cut off directory
        }
        fprintf_s(objf, "o obj_0\n");

        // Node renumbering array (assumes full mesh is built beforehand..)
        reindex = (int*)calloc(mesh_num_vertices(tree->mesh), sizeof(int));

        // for each material index, hide all the others, generate the surface and export it
        for (i = 0; i < k; i++)
        {
            int j;

            for (j = 0; j < k; j++)
                materials[candidates[j]].hidden = TRUE;
            materials[candidates[i]].hidden = FALSE;

            xform_list.head = NULL;
            xform_list.tail = NULL;
            if (object_tree.mesh != NULL)
                mesh_destroy(object_tree.mesh);
            object_tree.mesh = NULL;
            object_tree.mesh_valid = FALSE;
            gen_view_list_tree_surfaces(&object_tree, &object_tree);

            if (!object_tree.mesh_valid)    // nothing for this material
                continue;

            // vertices for the mesh for this material
            mesh_foreach_vertex(tree->mesh, export_vertex_obj, NULL);

            // OBJ volume for this material (write it to a temp file and append it at the end)
            if (candidates[i] != 0)
                fprintf_s(objv, "usemtl %s\n", materials[candidates[i]].name);
            mesh_foreach_face_vertices(tree->mesh, export_triangle_obj, NULL);
        }

        free(reindex);

        // Append the temp file to the OBJ file
        fclose(objv);
        fopen_s(&objv, tmp, "rt");
        while (1)
        {
            fgets(basename, 256, objv);
            if (feof(objv))
                break;

            fputs(basename, objf);
        }

        fclose(objf);
        fclose(objv);
        DeleteFile(tmp);

        if (k == 1)
            break;          // no materials other than the default (0)

        // write the materials to the corresponding MTL file (leave out material 0)
        for (i = 1; i < k; i++)
        {
            fprintf(mtl, "newmtl %s\n", materials[candidates[i]].name);
            fprintf(mtl, "Kd %f %f %f\n",
                materials[candidates[i]].color[0],
                materials[candidates[i]].color[1],
                materials[candidates[i]].color[2]);
        }

        fclose(mtl);

        // reinstate all the non-hidden materials and mark the surface mesh for regeneration
        for (i = 0; i < k; i++)
            materials[candidates[i]].hidden = FALSE;

        if (object_tree.mesh != NULL)
            mesh_destroy(object_tree.mesh);
        object_tree.mesh = NULL;
        object_tree.mesh_valid = FALSE;
        break;

    case 5: // export to an OFF File
        fopen_s(&off, filename, "wt");
        if (off == NULL)
            return;
        fprintf_s(off, "OFF\n");

        num_exported_tri = 0;
        num_exported_vertices = 0;
        if (tree->mesh != NULL && tree->mesh_valid && tree->mesh_complete)
        {
            int n_vertices = mesh_num_vertices(tree->mesh);
            int n_faces = mesh_num_faces(tree->mesh);

            fprintf_s(off, "%d %d %d\n", n_vertices, n_faces, 0);
            reindex = (int *)calloc(n_vertices, sizeof(int));

            //mesh_foreach_vertex(tree->mesh, export_vertex_off, NULL);
            mesh_foreach_vertex_d(tree->mesh, export_vertex_off_d, NULL);
            mesh_foreach_face_vertices(tree->mesh, export_triangle_off, NULL);

            free(reindex);
        }

        sprintf_s(buf, 64, "Mesh: %d triangles\r\n", num_exported_tri);
        Log(buf);
        fclose(off);
        break;
    }
}

#ifdef DEBUG_WRITE_VOL_MESH

// Write a mesh out to OFF. Used for debugging (sending meshes to CGAL for bug reporting)
void
mesh_write_off(char *prefix, int id, Mesh* mesh)
{
    char  filename[128];

    sprintf_s(filename, 128, "mesh_%s_%d.off", prefix, id);
    Log(filename);
    Log("\r\n");
    fopen_s(&off, filename, "wt");
    if (off == NULL)
        return;
    fprintf_s(off, "OFF\n");
    num_exported_tri = 0;
    num_exported_vertices = 0;
    {
        int n_vertices = mesh_num_vertices(mesh);
        int n_faces = mesh_num_faces(mesh);

        fprintf_s(off, "%d %d %d\n", n_vertices, n_faces, 0);
        reindex = (int*)calloc(n_vertices, sizeof(int));

        //mesh_foreach_vertex(mesh, export_vertex_off, NULL);
        mesh_foreach_vertex_d(mesh, export_vertex_off_d, NULL);
        mesh_foreach_face_vertices(mesh, export_triangle_off, NULL);
        free(reindex);
    }
    fclose(off);
}

#endif // DEBUG_WRITE_VOL_MESH


#ifdef OLD_EXPORT_CODE  // Old code to export direct from GL tessellator

// count of vertices received so far in the polygon
int stl_count;

// count of triangles output so far in the polygon
int stl_tri_count;

// Points stored for the next triangle
Point stl_points[3];

// Normal for the current polygon
Plane stl_normal;

// What kind of triangle sequence is being output (GL_TRIANGLES, TRIANGLE_STRIP or TRIANGLE_FAN)
GLenum stl_sequence;

// Write a single triangle out to the STL file
void
stl_write(void)
{
    int i;

    fprintf_s(stl, "facet normal %f %f %f\n", stl_normal.A, stl_normal.B, stl_normal.C);
    fprintf_s(stl, "  outer loop\n");
    for (i = 0; i < 3; i++)
        fprintf_s(stl, "    vertex %f %f %f\n", stl_points[i].x, stl_points[i].y, stl_points[i].z);
    fprintf_s(stl, "  endloop\n");
    fprintf_s(stl, "endfacet\n");
    stl_tri_count++;
}

// callbacks for exporting tessellated stuff to an STL file
void
export_beginData(GLenum type, void * polygon_data)
{
    Plane *norm = (Plane *)polygon_data;

    stl_sequence = type;
    stl_normal = *norm;
    stl_count = 0;
    stl_tri_count = 0;
}

void
export_vertexData(void * vertex_data, void * polygon_data)
{
    Point *v = (Point *)vertex_data;

    if (stl_count < 3)
    {
        stl_points[stl_count++] = *v;
    }
    else
    {
        switch (stl_sequence)
        {
        case GL_TRIANGLES:
            stl_write();
            stl_count = 0;
            stl_points[stl_count++] = *v;
            break;

        case GL_TRIANGLE_FAN:
            stl_write();
            stl_points[1] = stl_points[2];
            stl_points[2] = *v;
            break;

        case GL_TRIANGLE_STRIP:
            stl_write();
            if (stl_tri_count & 1)
                stl_points[0] = stl_points[2];
            else
                stl_points[1] = stl_points[2];
            stl_points[2] = *v;
            break;
        }
    }
}

void
export_endData(void * polygon_data)
{
    // write out the last triangle
    if (stl_count == 3)
        stl_write();
}

void
export_combineData(GLdouble coords[3], void *vertex_data[4], GLfloat weight[4], void **outData, void * polygon_data)
{
    // Allocate a new Point for the new vertex, and (TODO:) hang it off the face's spare vertices list.
    // It will be freed when the view list is regenerated.
    Point *p = point_new((float)coords[0], (float)coords[1], (float)coords[2]);
    p->hdr.ID = 0;
    objid--;

    *outData = p;
}

void export_errorData(GLenum errno, void * polygon_data)
{
    ASSERT(FALSE, "tesselator error");
}


// Render an volume or face object to triangles
void
export_object(GLUtesselator *tess, Object *obj)
{
    Face *face;
    Object *o;

    switch (obj->type)
    {
    case OBJ_FACE:
        face = (Face *)obj;
        gen_view_list_face(face);
        face_shade(tess, face, FALSE, FALSE, LOCK_NONE);
        break;

    case OBJ_VOLUME:
        for (face = ((Volume *)obj)->faces; face != NULL; face = (Face *)face->hdr.next)
            export_object(tess, (Object *)face);
        break;

    case OBJ_GROUP:
        for (o = ((Group *)obj)->obj_list; o != NULL; o = o->next)
            export_object(tess, o);
        break;
    }
}

// Tessellate every solid object in the tree to triangles and export to STL
void
export_object_tree(Group *tree, char *filename, int file_index)
{
    Object *obj;
    GLUtesselator *tess = gluNewTess();

    gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (void(__stdcall *)(void))export_beginData);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void(__stdcall *)(void))export_vertexData);
    gluTessCallback(tess, GLU_TESS_END_DATA, (void(__stdcall *)(void))export_endData);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void(__stdcall *)(void))export_combineData);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void(__stdcall *)(void))export_errorData);

    fopen_s(&stl, filename, "wt");
    if (stl == NULL)
        return;
    fprintf_s(stl, "solid %s\n", tree->title);

    for (obj = tree->obj_list; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_VOLUME || obj->type == OBJ_GROUP)
            export_object(tess, obj);
    }

    fprintf_s(stl, "endsolid %s\n", tree->title);
    fclose(stl);

    gluDeleteTess(tess);
}

#endif


