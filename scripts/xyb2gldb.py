import sys
import lavavu
import os
import struct

if len(sys.argv) < 2:
  print "Convert binary IEEE 32-bit float xyz to LavaVu database"
  print "  Usage: "
  print "    xyz2gldb.py input.xyb output.gldb"
  print ""
  exit()

filePath = sys.argv[1] #cmds.fileDialog()
dbPath = sys.argv[2]
inbytes = os.path.getsize(filePath)
num = inbytes / (4 * 6) # x y z r g b

#Create vis object (points)
points = lavavu.Points('points', None, size=1, props="colour=white")

#Create vis database for output
db = lavavu.Database(dbPath)

bbmin = [float("inf"),float("inf"),float("inf")]
bbmax = [float("-inf"),float("-inf"),float("-inf")]

#Create a new timestep entry in database
db.timestep()

#Loop over lines in input file
with open(filePath, 'rb') as infile:
  #Read & Write saved data
  points.write(db) #Write the object only
  print "Importing " + str(num) + " points"
  for p in range(num):
    if p % 10000 == 0:
      print str(p) + " / " + str(num)
      sys.stdout.flush()
    vert = struct.unpack('f'*3, infile.read(3*4))
    rgb = struct.unpack('f'*3, infile.read(3*4))
    rgba = [int(rgb[0]*255), int(rgb[1]*255), int(rgb[2]*255), 0xff]
    points.addVertex(vert[0], vert[1], vert[2])
    points.addColour(rgba)

  print "Writing " + str(num) + " points to database"
  sys.stdout.flush()
  #Close and write database to disk
  #points.write(db)
  points.write(db, compress=True)
  db.close()

  infile.close()

