#!/usr/bin/env python
import sys
import lavavu
import os
import numpy

if len(sys.argv) < 2:
  print "Load binary IEEE xyz in LavaVu"
  print "  Usage: "
  print ""
  print "    xyz2gldb.py input.xyb [subsample]"
  print ""
  print "      subsample: N, use every N'th point, skip others"
  print ""
  exit()

filePath = sys.argv[1] #cmds.fileDialog()
#Optional subsample arg
subsample = 1
if len(sys.argv) > 2:
  subsample = int(sys.argv[2])

inbytes = os.path.getsize(filePath)
#All floats
#num = inbytes / (4 * 6) # x y z r g b
#All floats, rgba
#num = inbytes / (4 * 7) # x y z r g b a
#All doubles, rgba
num = inbytes / (8 * 7) # x y z r g b a
#Float vert, byte colour
#num = inbytes / (4 * 3 + 4) # x y z r g b a

lv = lavavu.Viewer()
#Create vis object (points)
points = lv.points(pointsize=1, colour="white")

bbmin = [float("inf"),float("inf"),float("inf")]
bbmax = [float("-inf"),float("-inf"),float("-inf")]

with open(filePath, 'rb') as infile:
    print "Importing " + str(num) + " points"
    arr = numpy.fromfile(infile, dtype='float64')
    infile.close()

    arr = arr.astype('float32').reshape(len(arr)/7, 7)
    if subsample > 1:
        arr = arr[::subsample]


    #Convert float rgba to uint32 colour
    colours = arr[:,3:7]
    colours *= 255
    colours = colours.astype('uint8').ravel()
    colours = colours.view(dtype='uint32')

    points.vertices(arr[:,0:3])
    points.colours(colours)

print "Rendering " + str(len(arr)) + " points"

lv.interactive()

