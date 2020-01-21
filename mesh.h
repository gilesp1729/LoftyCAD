// (Don't include this in any C source files)
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <boost/container/flat_map.hpp>
#include <CGAL/Polygon_mesh_processing/corefinement.h>


typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Surface_mesh<K::Point_3> Mesh;
typedef boost::graph_traits<Mesh>::vertex_descriptor vertex_descriptor;
typedef Mesh::Property_map<vertex_descriptor, EK::Point_3> Exact_point_map;
typedef Mesh::Property_map<vertex_descriptor, bool> Exact_point_computed;
typedef Mesh::Face_index Face_index;
typedef Mesh::Vertex_index Vertex_index;

namespace PMP = CGAL::Polygon_mesh_processing;
namespace params = PMP::parameters;

struct Coref_point_map
{
    // typedef for the property map
    typedef boost::property_traits<Exact_point_map>::value_type value_type;
    typedef boost::property_traits<Exact_point_map>::reference reference;
    typedef boost::property_traits<Exact_point_map>::category category;
    typedef boost::property_traits<Exact_point_map>::key_type key_type;

    // exterior references
    Exact_point_computed* exact_point_computed_ptr;
    Exact_point_map* exact_point_ptr;
    Mesh* mesh_ptr;

    Exact_point_computed& exact_point_computed() const
    {
        CGAL_assertion(exact_point_computed_ptr != NULL);
        return *exact_point_computed_ptr;
    }

    Exact_point_map& exact_point() const
    {
        CGAL_assertion(exact_point_ptr != NULL);
        return *exact_point_ptr;
    }

    Mesh& mesh() const
    {
        CGAL_assertion(mesh_ptr != NULL);
        return *mesh_ptr;
    }

    // Converters
    CGAL::Cartesian_converter<K, EK> to_exact;
    CGAL::Cartesian_converter<EK, K> to_input;

    Coref_point_map()
        : exact_point_computed_ptr(NULL)
        , exact_point_ptr(NULL)
        , mesh_ptr(NULL)
    {}

    Coref_point_map(Exact_point_map& ep,
                    Exact_point_computed& epc,
                    Mesh& m)
                    : exact_point_computed_ptr(&epc)
                    , exact_point_ptr(&ep)
                    , mesh_ptr(&m)
    {}

    friend
        reference get(const Coref_point_map& map, key_type k)
    {
        // create exact point if it does not exist
        if (!map.exact_point_computed()[k])
        {
            map.exact_point()[k] = map.to_exact(map.mesh().point(k));
            map.exact_point_computed()[k] = true;
        }
        return map.exact_point()[k];
    }

    friend
        void put(const Coref_point_map& map, key_type k, const EK::Point_3& p)
    {
        map.exact_point_computed()[k] = true;
        map.exact_point()[k] = p;
        // create the input point from the exact one
        map.mesh().point(k) = map.to_input(p);
    }
};

struct Visitor :
    public PMP::Corefinement::Default_visitor<Mesh>
{
    typedef Mesh::Face_index face_descriptor;

    boost::container::flat_map<const Mesh*, Mesh::Property_map<Mesh::Face_index, int> > properties;
    int face_id;

    Visitor()
    {
        properties.reserve(3);
        face_id = -1;
    }

    // visitor API overloaded
    void before_subface_creations(face_descriptor f_split, Mesh& tm)
    {
        face_id = properties[&tm][f_split];
    }

    void after_subface_created(face_descriptor f_new, Mesh& tm)
    {
        properties[&tm][f_new] = face_id;
    }

    void after_face_copy(face_descriptor f_src, Mesh& tm_src,
        face_descriptor f_tgt, Mesh& tm_tgt)
    {
        properties[&tm_tgt][f_tgt] = properties[&tm_src][f_src];
    }
};



