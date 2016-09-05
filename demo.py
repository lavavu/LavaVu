#!/usr/bin/env python
import LavaVu

lv = LavaVu.load(hidden=False, verbose=False) #Interactive - require visible window
#lv = LavaVu.load()

print lv.image("initial")

#Points
points = lv.add("points", {"colour" : "red", "pointsize" : 10, "opacity" : 0.75, "static" : True, "geometry" : "points"})
points.vertices([[-1, -1, -1], [-1, 1, -1], [1, 1, -1], [1, -1, -1]]);
points.vertices([[-1, -1, 1], [-1, 1, 1], [1, 1, 1], [1, -1, 1]]);
points.values([1, 2, 3, 4, 5, 6, 7, 8])

#Lines
lines = lv.add("lines", {"colour" : "blue", "link" : True, "geometry" : "lines"});
lines.vertices([[1, -1, 1], [0, 0, 0], [1, -1, -1]]);

lv.select()
lv("add vec vectors")
lv("vertices=[[0, 0, 0], [0, 0, 0], [0, 0, 0], [1, 1, 1], [-1, -1, -1]]")
lv("vectors=[1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1]")
lv("colours=red green blue white white")
lv("read vertices")
lv("read vectors")
lv("read colours")

#This will trigger bounding box update so we can use min/max macros
lv.open()
#LavaVu.display()

lv.select()
lv("add sealevel quads")
lv("colour=[0,204,255]")
lv("opacity=0.5")
lv("static=true")
lv("dims=[2,2]")
lv("cullface=false")
lv("vertex minX 0 minZ")
lv("vertex maxX 0 minZ")
lv("vertex minX 0 maxZ")
lv("vertex maxX 0 maxZ")

lv.rotateX(45)
lv.fit()

print lv.image("rotated")

#Enter interative mode
lv.interactive()

print lv.image("final")

#lv("export")
imagestr = lv.image()

lv = None
