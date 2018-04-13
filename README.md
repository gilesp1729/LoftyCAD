# LoftyCAD
Simple CAD for 3D printers

# LoftyCAD goals
# General
Simple to use like Sketchup, but not as buggy and a bit more functional.

# Interface
Sketch-up-like (draw 2D, extrusion)
Allows rects, polygons, circles, ellipses and the extruded right prisms from them
Volume-edge-face hierarchy
Retain original 3D shape after modification, allow move/scale whole thing
Don't allow silly extrusions (holes going further than the thickness, etc)
Show dimensions when selected and when drawing/moving/scaling
Allow dimensions to snap points and also sensible other places (perp to lines, etc)
Allow units 0.1mm, mm, etc

# Files handled
Export triangle meshes STL, DAE, 3DS?
Import same, and some other features (progressively)
NEVER produce non-manifold triangle meshes
Always output full normals

# Stretch goals
Bezier edges
Bezier surfaces (how to do interface not sure)

# 3d Printing stretch goals
Slic3r interface
USB printer interface for one-step operation

