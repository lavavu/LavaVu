#!/usr/bin/env python
import sys
sys.path.append('bin')
import LavaVu

LavaVu.execute(["LavaVu", "-a", "-S", "-v"])
#LavaVu.execute(["LavaVu", "-a", "-S"])

print LavaVu.image("initial")

#Points
points = LavaVu.addObject("points", "colour=red\npointsize=10\nopacity=0.75\nstatic=1\ngeometry=points\n");
LavaVu.loadVertices([[10, 2, 3], [0, 0, 0], [1, -5, -5]]);
LavaVu.loadValues([1, 2, 3, 4, 5])

#Lines
lines = LavaVu.addObject("lines", "colour=blue\nlink=true\ngeometry=lines\n");
LavaVu.loadVertices([[10, 2, 3], [0, 0, 0], [1, -5, -5]]);

LavaVu.command("add vec vectors")
LavaVu.command("colour=orange")
LavaVu.command("vertices=[[2, 3, 0], [0, 0, 1], [-5, -5, 0.5], [0.5, 0.5, 5], [5, 5, 10]]")
LavaVu.command("vectors=[0.5, 0.0, 0.5, 0.0, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.0, 0.5, 0.5]")
LavaVu.command("read vertices")
LavaVu.command("read vectors")

#This will trigger bounding box update so we can use min/max macros
LavaVu.command("open")
#LavaVu.display()

LavaVu.command("add sealevel quads")
LavaVu.command("colour=[0,204,255]")
LavaVu.command("opacity=0.5")
LavaVu.command("static=true")
LavaVu.command("geomwidth=2")
LavaVu.command("geomheight=2")
LavaVu.command("cullface=false")
LavaVu.command("vertex minX 0 minZ")
LavaVu.command("vertex maxX 0 minZ")
LavaVu.command("vertex minX 0 maxZ")
LavaVu.command("vertex maxX 0 maxZ")

LavaVu.command("rotate x 45")
LavaVu.command("fit")

print LavaVu.image("rotated")

#Enter interative mode
LavaVu.command("interactive")

print LavaVu.image("final")

#LavaVu.command("export")
imagestr = LavaVu.image()
