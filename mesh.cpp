// C interface to a little bit of CGAL's polygon mesh processing library.

#include "mesh.h"
typedef void(*FaceCB)(void *arg, float x[3], float y[3], float z[3]);

extern "C"
{
    void
        mesh_destroy(Mesh *mesh)
    {
        delete mesh;
    }

    Mesh *
        mesh_new(void)
    {
        Mesh *mesh = new Mesh;
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

    void 
        mesh_foreach_face(Mesh *mesh, FaceCB callback, void *callback_arg)
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
                //ASSERT(i < 3, "Too many points in mesh face");
            }
            (*callback)(callback_arg, x, y, z);
        }
    }

}

