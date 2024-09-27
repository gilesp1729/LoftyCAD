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
export_triangle_stl_d(void* arg, double x[3], double y[3], double z[3])
{
    int i;
    double A, B, C, length;

    cross(x[1] - x[0], y[1] - y[0], z[1] - z[0], x[2] - x[0], y[2] - y[0], z[2] - z[0], &A, &B, &C);
    length = sqrt(A * A + B * B + C * C);
    if (!nz(length))
    {
        A /= length;
        B /= length;
        C /= length;
    }

    fprintf_s(stl, "facet normal %.15g %.15g %.15g\n", A, B, C);
    fprintf_s(stl, "  outer loop\n");
    for (i = 0; i < 3; i++)
        fprintf_s(stl, "    vertex %.15g %.15g %.15g\n", x[i], y[i], z[i]);
    fprintf_s(stl, "  endloop\n");
    fprintf_s(stl, "endfacet\n");
    num_exported_tri++;
}

// Write a vertex out to an OFF file, counting is zero-based position along the way
void
export_vertex_off_d(void* arg, Vertex_index* v, double x, double y, double z)
{
    fprintf_s(off, "%.15g %.15g %.15g\n", x, y, z);
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
export_vertex_amf_d(void* arg, Vertex_index* v, double x, double y, double z)
{
    fprintf_s(amf, "        <vertex><coordinates><x>%.15g</x><y>%.15g</y><z>%.15g</z></coordinates></vertex>\n", x, y, z);
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
export_vertex_obj(void* arg, Vertex_index* v, double x, double y, double z)
{
    fprintf_s(objf, "v %f %f %f\n", x, y, z);
    reindex[*(int*)v] = num_exported_vertices++;
}

void
export_vertex_obj_d(void* arg, Vertex_index* v, double x, double y, double z)
{
    fprintf_s(objf, "v %.15g %.15g %.15g\n", x, y, z);
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
            mesh_foreach_face_coords_d(((Volume *)obj)->mesh, export_triangle_stl_d, NULL);
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
    single_stl_output:
        fopen_s(&stl, filename, "wt");
        if (stl == NULL)
            return;
        show_status("Exporting ", filename);
        fprintf_s(stl, "solid %s\n", tree->title);

        num_exported_tri = 0;
        if (tree->mesh != NULL && tree->mesh_valid && !tree->mesh_merged)
            mesh_foreach_face_coords_d(tree->mesh, export_triangle_stl_d, NULL);

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
        clear_status_and_progress();
        break;

    case 2: // export each visible material to separate STL files
        // build a list of all the non-hidden material indices
        for (i = k = 0; i < MAX_MATERIAL; i++)
        {
            if (materials[i].valid && !materials[i].hidden)
                candidates[k++] = i;
        }
        
        // If there's only one material, use the existing mesh and go write it out
        if (k == 1)
            goto single_stl_output;

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
        show_status("Exporting ", filename);

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

            if (k > 1)                      // don't bother re-rendering, if there's only one material
            {
                if (object_tree.mesh != NULL)
                    mesh_destroy(object_tree.mesh);
                object_tree.mesh = NULL;
                object_tree.mesh_valid = FALSE;
                gen_view_list_tree_surfaces(&object_tree, &object_tree);
            }
            if (!object_tree.mesh_valid)    // nothing for this material
                continue;

            // vertices for the mesh for this material
            mesh_foreach_vertex_d(tree->mesh, export_vertex_amf_d, NULL);

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

        // write out any materials beyond material 0
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
        clear_status_and_progress();

        if (k > 1)
        {
            // reinstate all the non-hidden materials and mark the surface mesh for regeneration
            for (i = 0; i < k; i++)
                materials[candidates[i]].hidden = FALSE;

            if (object_tree.mesh != NULL)
                mesh_destroy(object_tree.mesh);
            object_tree.mesh = NULL;
            object_tree.mesh_valid = FALSE;
        }
        break;

    case 4: // export to an OBJ file
        fopen_s(&objf, filename, "wt");
        if (objf == NULL)
            return;
        show_status("Exporting ", filename);

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

            if (k > 1)
            {
                if (object_tree.mesh != NULL)
                    mesh_destroy(object_tree.mesh);
                object_tree.mesh = NULL;
                object_tree.mesh_valid = FALSE;
                gen_view_list_tree_surfaces(&object_tree, &object_tree);
            }
            if (!object_tree.mesh_valid)    // nothing for this material
                continue;

            // vertices for the mesh for this material
            mesh_foreach_vertex_d(tree->mesh, export_vertex_obj_d, NULL);

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
        clear_status_and_progress();
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
        show_status("Exporting ", filename);

        fprintf_s(off, "OFF\n");

        num_exported_tri = 0;
        num_exported_vertices = 0;
        if (tree->mesh != NULL && tree->mesh_valid && tree->mesh_complete)
        {
            int n_vertices = mesh_num_vertices(tree->mesh);
            int n_faces = mesh_num_faces(tree->mesh);

            fprintf_s(off, "%d %d %d\n", n_vertices, n_faces, 0);
            reindex = (int *)calloc(n_vertices, sizeof(int));

            mesh_foreach_vertex_d(tree->mesh, export_vertex_off_d, NULL);
            mesh_foreach_face_vertices(tree->mesh, export_triangle_off, NULL);

            free(reindex);
        }

        sprintf_s(buf, 64, "Mesh: %d triangles\r\n", num_exported_tri);
        Log(buf);
        fclose(off);
        clear_status_and_progress();
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

        mesh_foreach_vertex_d(mesh, export_vertex_off_d, NULL);
        mesh_foreach_face_vertices(mesh, export_triangle_off, NULL);
        free(reindex);
    }
    fclose(off);
}

#endif // DEBUG_WRITE_VOL_MESH

