import sys
import re
import lavavu

if len(sys.argv) < 2:
  print "Convert text xyz to LavaVu database"
  print "(basic comma/space/tab delimited X,Y,Z[,R,G,B] only)"
  print "  Usage: "
  print "    xyz2gldb.py input.txt output.gldb"
  print ""
  exit()

filePath = sys.argv[1] #cmds.fileDialog()
dbPath = sys.argv[2]

#Create vis object (points)
points = lavavu.Points('points', None, size=5, pointtype=lavavu.Points.ShinySphere, props="colour=white")
#points = lavavu.Points('points', None, size=5, pointtype=lavavu.Points.ShinySphere, props="colour=white\ntexturefile=shiny.png")

#Create vis database for output
db = lavavu.Database(dbPath)
#Write the colourmaps to the database
#colours.write(db)

#For storing bounding box
bbmin = [float("inf"),float("inf"),float("inf")]
bbmax = [float("-inf"),float("-inf"),float("-inf")]

#Create a new timestep entry in database
db.timestep()

#Loop over lines in input file
with open(filePath, 'r') as file:
  for line in file:
    #Read particle position
    data = re.split(r'[,;\s]+', line.rstrip())
    #print data
    if len(data) < 3: continue
    x = float(data[0])
    y = float(data[1])
    z = float(data[2])
    
    #Write vertex position, Points...
    points.addVertex(x, y, z)

    #R,G,B[,A] colour if provided
    if len(data) >= 7:
      rgba = [int(data[3]), int(data[4]), int(data[5]), int(data[6])]
      points.addColour(rgba)
    elif len(data) == 6:
      rgba = [int(data[3]), int(data[4]), int(data[5]), 255]
      points.addColour(rgba)

  #Write saved data
  points.write(db)

  #Close and write database to disk
  db.close()


