#!/usr/bin/env python
import lavavu

debug = False

def second_viewer():
    #return
    global debug
    lv2 = lavavu.Viewer(verbose=debug, hidden=False) #Interactive - require visible window
    lines2 = lv2.add("lines", {"colour" : "blue", "link" : True, "geometry" : "lines"});
    lines2.vertices([[1, -1, 1], [0, 0, 0], [1, -1, -1]]);
    lv2.test()
    lv2.init()
    #lv2.interactive()
    return lv2

lv = lavavu.Viewer(verbose=debug, hidden=False) #Interactive - require visible window
#lv = lavavu.load()

lv2 = second_viewer()

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
#lv.init()

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

#from threading import Thread
#thread = Thread(target = second_viewer)
#thread.start()

#Enter interative mode
lv.interactive()

#thread.join()
lv2.interactive()

#print lv
print lv.image("final")

#lv("export")
imagestr = lv2.image()

#lv = None
