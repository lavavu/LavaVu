#!/usr/bin/env python
import sys
import lavavu
import os
import struct

if len(sys.argv) < 3:
  print "Convert binary IEEE 32-bit float xyz to LavaVu database"
  print "  Usage: "
  print ""
  print "    xyz2gldb.py input.xyb output.gldb [subsample]"
  print ""
  print "      subsample: N, use every N'th point, skip others"
  print ""
  exit()

filePath = sys.argv[1] #cmds.fileDialog()
dbPath = sys.argv[2]
#Optional subsample arg
subsample = 1
if len(sys.argv) > 3:
  subsample = int(sys.argv[3])

inbytes = os.path.getsize(filePath)
#All floats
num = inbytes / (4 * 6) # x y z r g b
#All doubles, rgba
#num = inbytes / (8 * 7) # x y z r g b a
#Float vert, byte colour
#num = inbytes / (4 * 3 + 4) # x y z r g b a

#Create vis object (points)
points = lavavu.Points('points', None, size=1, props="colour=white")

#Create vis database for output
db = lavavu.Database(dbPath)

bbmin = [float("inf"),float("inf"),float("inf")]
bbmax = [float("-inf"),float("-inf"),float("-inf")]

#Create a new timestep entry in database
db.timestep()

#Loop over lines in input file
rgba = None
with open(filePath, 'rb') as infile:
  #Read & Write saved data
  points.write(db) #Write the object only
  print "Importing " + str(num) + " points"
  for p in range(num):
    if p % 10000 == 0:
      print str(p) + " / " + str(num)
      sys.stdout.flush()
    vert = struct.unpack('f'*3, infile.read(3*4))
    #vert = struct.unpack('d'*3, infile.read(3*8))
    #RBGA byte * 4
    #rgba = struct.unpack('B'*4, infile.read(4))
    #RGBA float * 4
    #rgb = struct.unpack('f'*4, infile.read(4*4))
    #RGBA double * 4
    #rgb = struct.unpack('d'*4, infile.read(4*8))
    #rgba = [int(rgb[0]*255), int(rgb[1]*255), int(rgb[2]*255), int(rgb[3]*255)]
    #RGB float * 3
    rgb = struct.unpack('f'*3, infile.read(3*4))
    rgba = [int(rgb[0]*255), int(rgb[1]*255), int(rgb[2]*255), 0xff]
    #Subsample?
    if subsample > 1 and p % subsample != 0: continue
    points.addVertex(vert[0], vert[1], vert[2])
    points.addColour(rgba)

    #Write every 10 million
    if p%10000000 == 0:
        print "Writing points to database"
        points.write(db, compress=True)

print "Writing " + str(int(num/subsample)) + " points to database"
sys.stdout.flush()
#Close and write database to disk
#points.write(db)
  points.write(db, compress=True)
  db.close()

  infile.close()

