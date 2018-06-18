# LoftyCAD
Simple CAD for 3D printers. It came out of dissatisfactions with SketchUp, and with free 3D CAD programs in general.

# LoftyCAD goals
# General
It is open-source, actively maintained, and free to all.
Simple to use like Sketchup, but with some improvements:
In particular, it will:
- Retain the identity of 3D shapes, do not merge irreversibly with others
- Not allow silly extrusions
- Produce a single triangle mesh from merged objects
- NEVER produce non-manifold triangle meshes requiring repairs
- Write STL files directly

# Interface
- Sketch-up-like (draw a 2D face followed by extrusion)
- Allows rects, polygons, circles, arcs, beziers and the extruded right prisms from them
- Volume-edge-face hierarchy
- Show dimensions when selected and when drawing/moving/scaling
- Allow dimensions to snap points and also sensible other places (perp to lines, etc)
- Allow units 0.1mm, mm, etc. and snapping tolerances to distance and angle

# Files handled
- Export triangle meshes STL, DAE?, 3DS?
- Import STL, GTS, OFF, and some other features (progressively)
- Always output full normals

# Stretch goals
- Bezier surfaces (compound curves)
- Lofting (a nod to the original Lofty)

# Building LoftyCAD
LoftyCAD uses the CGAL computational geometry library to merge and intersect triangle meshes.
Build CGAL from its build instructions, noting that:
- The 32-bit version must be used, since LoftyCAD is a 32-bit program for now
- The prerequisites are cmake, Boost, GMP and MPFR. They are precompiled.
- Qt and libQGLViewer are not required, nor are any of the examples or demos.
- CGAL and Boost should be checked out to directories alongside LoftyCAD, and their environment variables set up accordingly. Make sure CGAL_DIR points to the CGAL install directory.


