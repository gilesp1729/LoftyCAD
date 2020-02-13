// C interface to a little bit of CGAL's polygon mesh processing library.

#include "mesh.h"
typedef void(*FaceCoordCB)(void* arg, float x[3], float y[3], float z[3]);
typedef void(*FaceCoordMaterialCB)(void* arg, int mat_index, float x[3], float y[3], float z[3]);
typedef void(*FaceVertexCB)(void *arg, int nv, Vertex_index *vi);
typedef void(*VertexCB)(void* arg, Vertex_index* v, float x, float y, float z);
typedef void(*VertexCB_D)(void* arg, Vertex_index* v, double x, double y, double z);

extern "C"
{
    void
        mesh_destroy(Mesh *mesh)
    {
        delete mesh;
    }

    // Create a mesh and add its material as a property to faces.
    Mesh *
        mesh_new(int material)
    {
        Mesh *mesh = new Mesh;
        Mesh::Property_map<Mesh::Face_index, int> mesh_id =
            mesh->add_property_map<Mesh::Face_index, int>("f:id", material).first;
        return mesh;
    }

    Mesh *
        mesh_copy(Mesh *from)
    {
        Mesh *mesh = new Mesh(*from);
        return mesh;
    }

    void
        mesh_add_vertex(Mesh *mesh, float x, float y, float z, Vertex_index *vi)
    {
        K::Point_3 p = { x, y, z };
        *vi = mesh->add_vertex(p);
    }

    void
        mesh_add_face(Mesh *mesh, Vertex_index *v1, Vertex_index *v2, Vertex_index *v3, Face_index *fi)
    {
        *fi = mesh->add_face(*v1, *v2, *v3);
    }

#define NON_INPLACE_ISSUE_4522
// Non-in-place operations to work around CGAL issue #4522.
#ifdef NON_INPLACE_ISSUE_4522
    int // no BOOL here
        mesh_union(Mesh **mesh1_ptr, Mesh *mesh2)
    {
        Mesh* mesh1 = *mesh1_ptr;
        Mesh *out = new Mesh;
        bool rc;

        // Create new (or reference existing) property maps
        Exact_point_map mesh1_exact_points =
            mesh1->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh1_exact_points_computed =
            mesh1->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map mesh2_exact_points =
            mesh2->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh2_exact_points_computed =
            mesh2->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map out_exact_points =
            out->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed out_exact_points_computed =
            out->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Coref_point_map mesh1_pm(mesh1_exact_points, mesh1_exact_points_computed, *mesh1);
        Coref_point_map mesh2_pm(mesh2_exact_points, mesh2_exact_points_computed, *mesh2);
        Coref_point_map out_pm(out_exact_points, out_exact_points_computed, *out);

        Mesh::Property_map<Mesh::Face_index, int> mesh1_id =
            mesh1->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> mesh2_id =
            mesh2->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> out_id =
            out->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        Visitor visitor;
        visitor.properties[mesh1] = mesh1_id;
        visitor.properties[mesh2] = mesh2_id;
        visitor.properties[out] = out_id;

        rc = (PMP::corefine_and_compute_union(*mesh1,
            *mesh2,
            *out,
            params::vertex_point_map(mesh1_pm).visitor(visitor),
            params::vertex_point_map(mesh2_pm),
            params::vertex_point_map(out_pm)));

        if (rc)
        {
            delete mesh1;
            *mesh1_ptr = out;
        }

        return rc;
    }

    int // no BOOL here
        mesh_intersection(Mesh** mesh1_ptr, Mesh* mesh2)
    {
        Mesh* mesh1 = *mesh1_ptr;
        Mesh* out = new Mesh;
        bool rc;

        // Create new (or reference existing) property maps
        Exact_point_map mesh1_exact_points =
            mesh1->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh1_exact_points_computed =
            mesh1->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map mesh2_exact_points =
            mesh2->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh2_exact_points_computed =
            mesh2->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map out_exact_points =
            out->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed out_exact_points_computed =
            out->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Coref_point_map mesh1_pm(mesh1_exact_points, mesh1_exact_points_computed, *mesh1);
        Coref_point_map mesh2_pm(mesh2_exact_points, mesh2_exact_points_computed, *mesh2);
        Coref_point_map out_pm(out_exact_points, out_exact_points_computed, *out);

        Mesh::Property_map<Mesh::Face_index, int> mesh1_id =
            mesh1->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> mesh2_id =
            mesh2->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> out_id =
            out->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        Visitor visitor;
        visitor.properties[mesh1] = mesh1_id;
        visitor.properties[mesh2] = mesh2_id;
        visitor.properties[out] = out_id;

        rc = (PMP::corefine_and_compute_intersection(*mesh1,
            *mesh2,
            *out,
            params::vertex_point_map(mesh1_pm).visitor(visitor),
            params::vertex_point_map(mesh2_pm),
            params::vertex_point_map(out_pm)));

        if (rc)
        {
            delete mesh1;
            *mesh1_ptr = out;
        }

        return rc;
    }

    int // no BOOL here
        mesh_difference(Mesh** mesh1_ptr, Mesh* mesh2)
    {
        Mesh* mesh1 = *mesh1_ptr;
        Mesh* out = new Mesh;
        bool rc;

        // Create new (or reference existing) property maps
        Exact_point_map mesh1_exact_points =
            mesh1->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh1_exact_points_computed =
            mesh1->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map mesh2_exact_points =
            mesh2->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh2_exact_points_computed =
            mesh2->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map out_exact_points =
            out->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed out_exact_points_computed =
            out->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Coref_point_map mesh1_pm(mesh1_exact_points, mesh1_exact_points_computed, *mesh1);
        Coref_point_map mesh2_pm(mesh2_exact_points, mesh2_exact_points_computed, *mesh2);
        Coref_point_map out_pm(out_exact_points, out_exact_points_computed, *out);

        Mesh::Property_map<Mesh::Face_index, int> mesh1_id =
            mesh1->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> mesh2_id =
            mesh2->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> out_id =
            out->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        Visitor visitor;
        visitor.properties[mesh1] = mesh1_id;
        visitor.properties[mesh2] = mesh2_id;
        visitor.properties[out] = out_id;

        rc = (PMP::corefine_and_compute_difference(*mesh1,
            *mesh2,
            *out,
            params::vertex_point_map(mesh1_pm).visitor(visitor),
            params::vertex_point_map(mesh2_pm),
            params::vertex_point_map(out_pm)));

        if (rc)
        {
            delete mesh1;
            *mesh1_ptr = out;
        }

        return rc;
    }
#else // old in-place code, but with the ** pointer to maintain compatibility with caller
    int // no BOOL here
        mesh_union(Mesh** mesh1_ptr, Mesh* mesh2)
    {
        Mesh* mesh1 = *mesh1_ptr;

        // Create new (or reference existing) property maps
        Exact_point_map mesh1_exact_points =
            mesh1->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh1_exact_points_computed =
            mesh1->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map mesh2_exact_points =
            mesh2->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh2_exact_points_computed =
            mesh2->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Coref_point_map mesh1_pm(mesh1_exact_points, mesh1_exact_points_computed, *mesh1);
        Coref_point_map mesh2_pm(mesh2_exact_points, mesh2_exact_points_computed, *mesh2);

        Mesh::Property_map<Mesh::Face_index, int> mesh1_id =
            mesh1->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> mesh2_id =
            mesh2->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        Visitor visitor;
        visitor.properties[mesh1] = mesh1_id;
        visitor.properties[mesh2] = mesh2_id;

        return (PMP::corefine_and_compute_union(*mesh1,
            *mesh2,
            *mesh1,
            params::vertex_point_map(mesh1_pm).visitor(visitor),
            params::vertex_point_map(mesh2_pm),
            params::vertex_point_map(mesh1_pm)));
    }

    int // no BOOL here
        mesh_intersection(Mesh **mesh1_ptr, Mesh *mesh2)
    {
        Mesh* mesh1 = *mesh1_ptr;

        Exact_point_map mesh1_exact_points =
            mesh1->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh1_exact_points_computed =
            mesh1->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map mesh2_exact_points =
            mesh2->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh2_exact_points_computed =
            mesh2->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Coref_point_map mesh1_pm(mesh1_exact_points, mesh1_exact_points_computed, *mesh1);
        Coref_point_map mesh2_pm(mesh2_exact_points, mesh2_exact_points_computed, *mesh2);

        Mesh::Property_map<Mesh::Face_index, int> mesh1_id =
            mesh1->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> mesh2_id =
            mesh2->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        Visitor visitor;
        visitor.properties[mesh1] = mesh1_id;
        visitor.properties[mesh2] = mesh2_id;

        return (PMP::corefine_and_compute_intersection(*mesh1,
            *mesh2,
            *mesh1,
            params::vertex_point_map(mesh1_pm).visitor(visitor),
            params::vertex_point_map(mesh2_pm),
            params::vertex_point_map(mesh1_pm)));
    }

    int // no BOOL here
        mesh_difference(Mesh **mesh1_ptr, Mesh *mesh2)
    {
        Mesh* mesh1 = *mesh1_ptr;

        Exact_point_map mesh1_exact_points =
            mesh1->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh1_exact_points_computed =
            mesh1->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Exact_point_map mesh2_exact_points =
            mesh2->add_property_map<vertex_descriptor, EK::Point_3>("e:exact_point").first;
        Exact_point_computed mesh2_exact_points_computed =
            mesh2->add_property_map<vertex_descriptor, bool>("e:exact_points_computed").first;

        Coref_point_map mesh1_pm(mesh1_exact_points, mesh1_exact_points_computed, *mesh1);
        Coref_point_map mesh2_pm(mesh2_exact_points, mesh2_exact_points_computed, *mesh2);

        Mesh::Property_map<Mesh::Face_index, int> mesh1_id =
            mesh1->add_property_map<Mesh::Face_index, int>("f:id", 0).first;
        Mesh::Property_map<Mesh::Face_index, int> mesh2_id =
            mesh2->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        Visitor visitor;
        visitor.properties[mesh1] = mesh1_id;
        visitor.properties[mesh2] = mesh2_id;

        return (PMP::corefine_and_compute_difference(*mesh1,
            *mesh2,
            *mesh1,
            params::vertex_point_map(mesh1_pm).visitor(visitor),
            params::vertex_point_map(mesh2_pm),
            params::vertex_point_map(mesh1_pm)));
    }

#endif // NON_INPLACE_ISSUE_4522

    void
        mesh_foreach_vertex(Mesh* mesh, VertexCB callback, void* callback_arg)
    {
        BOOST_FOREACH(Vertex_index v, mesh->vertices())
        {
            (*callback)(callback_arg, &v, mesh->point(v).x(), mesh->point(v).y(), mesh->point(v).z());
        }
    }

    void
        mesh_foreach_vertex_d(Mesh* mesh, VertexCB_D callback, void* callback_arg)
    {
        BOOST_FOREACH(Vertex_index v, mesh->vertices())
        {
            (*callback)(callback_arg, &v, mesh->point(v).x(), mesh->point(v).y(), mesh->point(v).z());
        }
    }

    void
        mesh_foreach_face_vertices(Mesh *mesh, FaceVertexCB callback, void *callback_arg)
    {
        Vertex_index vi[3];
        int i;

        BOOST_FOREACH(Face_index f, mesh->faces())
        {
            i = 0;
            BOOST_FOREACH(Vertex_index v, CGAL::vertices_around_face(mesh->halfedge(f), *mesh))
            {
                vi[i] = v;
                i++;
            }
            (*callback)(callback_arg, i, vi);
        }
    }

    void
        mesh_foreach_face_coords(Mesh *mesh, FaceCoordCB callback, void *callback_arg)
    {
        float x[3], y[3], z[3];
        int i;

        BOOST_FOREACH(Face_index f, mesh->faces())
        {
            i = 0;
            BOOST_FOREACH(Vertex_index v, CGAL::vertices_around_face(mesh->halfedge(f), *mesh))
            {
                x[i] = mesh->point(v).x();
                y[i] = mesh->point(v).y();
                z[i] = mesh->point(v).z();
                i++;
            }
            (*callback)(callback_arg, x, y, z);
        }
    }

    void
        mesh_foreach_face_coords_mat(Mesh* mesh, FaceCoordMaterialCB callback, void* callback_arg)
    {
        float x[3], y[3], z[3];
        int i;
        int mat;
        Mesh::Property_map<Mesh::Face_index, int> mesh_id =
            mesh->add_property_map<Mesh::Face_index, int>("f:id", 0).first;

        BOOST_FOREACH(Face_index f, mesh->faces())
        {
            i = 0;
            BOOST_FOREACH(Vertex_index v, CGAL::vertices_around_face(mesh->halfedge(f), *mesh))
            {
                x[i] = mesh->point(v).x();
                y[i] = mesh->point(v).y();
                z[i] = mesh->point(v).z();
                i++;
            }
            mat = mesh_id[f];
            (*callback)(callback_arg, mat, x, y, z);
        }
    }

    int
        mesh_num_vertices(Mesh *mesh)
    {
        return mesh->number_of_vertices();
    }
    int
        mesh_num_faces(Mesh *mesh)
    {
        return mesh->number_of_faces();
    }
}
