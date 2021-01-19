# LoftyCAD: Sketch, Render, Slice and Print.
Free CSG CAD for 3D printers for Windows. It came out of dissatisfactions with SketchUp, and with free 3D CAD programs in general. However, it has since grown into something a lot bigger.

# LoftyCAD philosophy and goals
# General
LoftyCAD is a hierarchical CSG 3D-modelling program. It is open-source, actively maintained, and free to all.
Simple to use like Sketchup, but with some improvements:
In particular, it will:
- Retain the identity of 3D shapes, and not merge shapes irreversibly with others
- Produce a single triangle mesh from merged objects, using CSG operations (union, intersection, difference)
- Not produce non-manifold triangle meshes requiring repairs 
- Write STL and other triangle mesh files directly
- Not be subscription or web-based; your data is yours forever on your local PC

# Interface
- Sketch-up-like (draw a 2D-2.5D face followed by extrusion into a 3D volume)
- Volume-face-edge-point hierarchy with controlled locking
- Allows rect edges, polygons, circles, arcs, Beziers and the extruded right prisms from them
- Allows curved surfaces (cylinders, barrel arcs, and Bezier surfaces)
- Allows extrusion of curved surfaces, either parallel or along local normals
- Allows extrusion of text and fonts
- Allows axially symmetric objects (bodies of revolution)
- Copy and paste objects
- Reflect and rotate objects
- Group objects and transform (scale and rotate)
- Show dimensions when selected and when drawing/moving/scaling
- Allow dimensions to snap points and also sensible other places (perp to lines, etc)
- Allow units 0.1mm, mm, etc. and snapping tolerances to distance and angle
- Allow multiple materials

# Files handled
- Native (LCD) format is human-readable
- Export triangle meshes in STL, AMF, OBJ, or OFF formats
- Export multi-material model to separate STL meshes
- Export multi-material model to AMF and OBJ along with material definitions
- Always output full normals
- Import STL, AMF, OBJ, and OFF to groups containing meshes as volumes
- Import G-code files and visualise G-code

# Slicer and printer integration
- Integrates with PrusaSlicer 2.x, Slic3rPE 1.41 and vanilla Slic3r
- Slice to G-code and preview layers
- Directly upload G-code to an OctoPrint server

# Works in progress
- Multi-material slicing
- Directly control, and print to, a USB or serial connected printer
- Repairing imported triangle meshes
- Allow manipulation of triangle meshes: smooth extrusion, refinement, and more
- Tubing and lofting (a nod to the original Lofty, LoftyCAD's spiritual ancestor)

# Installing LoftyCAD from the prebuilt installer
Unzip the zip in the Installer directory, to any directory on the Windows system (c:\Program Files (x86) is the standard place for 32-bit programs). The install.bat file will do this (run as administrator!), register the file associations, and create a desktop link.

# Building LoftyCAD
LoftyCAD uses the excellent CGAL computational geometry library to merge and intersect triangle meshes.

# Material Girl branch:
# This branch is the live development branch and will become the master in due course. Features described above refer to this branch and may not be present in the deprecated master branch.
- This branch requires CGAL 5.0 (at least) and VS2019.
- The 32-bit version must be used, since LoftyCAD is a 32-bit program for now.
- CGAL from 5.0 is a header-only library and requires no build steps (other than cmake to make all the .slns and vsprojs).
- Some environment variables still need to be set up to build:
	CGAL_DIR --> CGAL checkout location
	BOOST_INCLUDEDIR --> Boost checkout location
	BOOST_LIBRARYDIR --> Boost checkout location
	PATH --> <CGAL_DIR>\\auxiliary\gmp\lib added to path

# Master branch:
# No further development will be done on the master branch.
Requires 32-bit CGAL 4.12 checked out to directories alongside, and built from its build instructions.
