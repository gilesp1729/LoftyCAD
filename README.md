# LoftyCAD
Simple CAD for 3D printers. It came out of dissatisfactions with SketchUp, and with free 3D CAD programs in general.

# LoftyCAD goals
# General
It is open-source, actively maintained, and free to all.
Simple to use like Sketchup, but with some improvements:
In particular, it will:
- Retain the identity of 3D shapes, do not merge irreversibly with others.
- Not allow silly extrusions
- Produce a single triangle mesh from merged objects
- NEVER produce non-manifold triangle meshes
- Write STL files directly

# Interface
Sketch-up-like (draw a 2D face followed by extrusion)
Allows rects, polygons, circles, arcs, beziers and the extruded right prisms from them
Volume-edge-face hierarchy
Show dimensions when selected and when drawing/moving/scaling
Allow dimensions to snap points and also sensible other places (perp to lines, etc)
Allow units 0.1mm, mm, etc

# Files handled
Export triangle meshes STL, DAE, 3DS?
Import same, and some other features (progressively)
Always output full normals

# Stretch goals
Bezier surfaces (compound curves)

