# LoftyCAD
Simple CAD for 3D printers for Windows. It came out of dissatisfactions with SketchUp, and with free 3D CAD programs in general.

# LoftyCAD goals
# General
It is open-source, actively maintained, and free to all.
Simple to use like Sketchup, but with some improvements:
In particular, it will:
- Retain the identity of 3D shapes, do not merge irreversibly with others
- Not allow silly extrusions
- Produce a single triangle mesh from merged objects, CSG fashion
- Not produce non-manifold triangle meshes requiring repairs (OK, some apps complain, but I've never seen on fail to slice)
- Write STL files directly

# Interface
- Sketch-up-like (draw a 2D face followed by extrusion)
- Allows rects, polygons, circles, arcs, beziers and the extruded right prisms from them
- Volume-face-edge-point hierarchy with controlled locking
- Group objects and transform (scale and rotate)
- Show dimensions when selected and when drawing/moving/scaling
- Allow dimensions to snap points and also sensible other places (perp to lines, etc)
- Allow units 0.1mm, mm, etc. and snapping tolerances to distance and angle

# Files handled
- Export triangle meshes as STL
- Import STL, GTS, OFF, and some other features (progressively)
- Always output full normals

# Stretch goals
- Editing faces by breaking edges and adding points
- Bezier surfaces (compound curves)
- Lofting (a nod to the original Lofty, which is LoftyCAD's spiritual ancestor)

# Installing LoftyCAD from the prebuilt installer
Unzip the zip in the Installer directory, to any directory on the Windows system (c:\Program Files (x86) is the standard place for 32-bit programs). The install.bat file will do this (run as administrator!), register the file associations, and create a desktop link.

# Building LoftyCAD
LoftyCAD uses the CGAL computational geometry library to merge and intersect triangle meshes.
Build CGAL from its build instructions, noting that:
- The 32-bit version must be used, since LoftyCAD is a 32-bit program for now.
- The prerequisites are cmake, Boost, GMP and MPFR. They are precompiled.
- Qt and libQGLViewer are not required, nor are any of the examples or demos.
- CGAL and Boost should be checked out to directories alongside LoftyCAD, and their environment variables set up accordingly. Make sure CGAL_DIR points to the CGAL install directory.


