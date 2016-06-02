#!/usr/bin/env python
import sys
sys.path.append('bin')
import LavaVu

lv = LavaVu.load(["-a", "-S", "-v"])

print lv.image("initial")

#Points
points = lv.add("points", {"colour" : "red", "pointsize" : 10, "opacity" : 0.75, "static" : True, "geometry" : "points"})
lv.vertices([[-1, -1, -1], [-1, 1, -1], [1, 1, -1], [1, -1, -1]]);
lv.vertices([[-1, -1, 1], [-1, 1, 1], [1, 1, 1], [1, -1, 1]]);
lv.values([1, 2, 3, 4, 5, 6, 7, 8])

#Lines
lines = lv.add("lines", {"colour" : "blue", "link" : True, "geometry" : "lines"});
lv.vertices([[1, -1, 1], [0, 0, 0], [1, -1, -1]]);

lv.commands("add vec vectors")
lv.commands("vertices=[[0, 0, 0], [0, 0, 0], [0, 0, 0], [1, 1, 1], [-1, -1, -1]]")
lv.commands("vectors=[1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1]")
lv.commands("colours=red green blue white white")
lv.commands("read vertices")
lv.commands("read vectors")
lv.commands("read colours")

#This will trigger bounding box update so we can use min/max macros
lv.commands("open")
#LavaVu.display()

lv.commands("add sealevel quads")
lv.commands("colour=[0,204,255]")
lv.commands("opacity=0.5")
lv.commands("static=true")
lv.commands("geomwidth=2")
lv.commands("geomheight=2")
lv.commands("cullface=false")
lv.commands("vertex minX 0 minZ")
lv.commands("vertex maxX 0 minZ")
lv.commands("vertex minX 0 maxZ")
lv.commands("vertex maxX 0 maxZ")

lv.commands("rotate x 45")
lv.commands("fit")

print lv.image("rotated")

#Enter interative mode
lv.commands("interactive")

print lv.image("final")

#lv.commands("export")
imagestr = lv.image()
