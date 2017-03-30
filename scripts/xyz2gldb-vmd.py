import sys
import re
import lvdb

if len(sys.argv) < 2:
  print "Convert text xyz to LavaVu database"
  print "  Usage: "
  print "    xyz2gldb.py input.txt output.gldb"
  print "      Convert with varying bounding box"
  print "    xyz2gldb.py input.txt output.gldb 1" 
  print "      Convert with bounding box fixed to maximum over full simulation"
  print ""
  exit()

filePath = sys.argv[1] #cmds.fileDialog()
dbPath = sys.argv[2]
#Optional: fix bounding box to min/max over entire dataset
fixBB = 0
if len(sys.argv) > 3:
    fixBB = sys.argv[3]

#Create a colourmap with 15 random colours
#colours = lvdb.RandomColourMap(count=15, seed=34)
#Print the colour list, this can be used as the starting point for a custom colour map
#print colours.colours
#User defined colour map (R,G,B [0,1])
#colours = lvdb.ColourMap([[1,0,0], [1,1,0], [0,1,0], [0,1,1], [0,0,1], [1,0,1], [1,1,1]])
#Create vis object (points)
#points = lvdb.Points('points', None, size=5, pointtype=lvdb.Points.ShinySphere, props="colour=white")
points = lvdb.Points('points', None, size=5, pointtype=lvdb.Points.ShinySphere, props="colour=white\ntexturefile=shiny.png")
#points = lvdb.Points('points', colours, size=10, pointtype=lvdb.Points.ShinySphere)
#points = lvdb.Shapes('points', colours, size=1.0, shape=lvdb.Shapes.Sphere)
#points = lvdb.Points('points', None, size=10, pointtype=lvdb.Points.ShinySphere, props="colour=[1,0,0]")

#Create vis database for output
db = lvdb.Database(dbPath)
#Write the colourmaps to the database
#colours.write(db)

#For storing bounding box over full simulation
bbmin = [float("inf"),float("inf"),float("inf")]
bbmax = [float("-inf"),float("-inf"),float("-inf")]

#Write a particle timestep to database
def writeTimeStep():
  global db, points, bbmin, bbmax
  #Set value min/max for colouring
  #points.vmin = 0
  #points.vmax = 15

  #Save min/max bounding box dims
  if fixBB:
    for i in range(3):
      if bbmin[i] > points.bmin[i]: bbmin[i] = points.bmin[i]
      if bbmax[i] < points.bmax[i]: bbmax[i] = points.bmax[i]

  points.write(db)

#Loop over lines in input file
with open(filePath, 'r') as file:
  verts = 0

  for line in file:
    if re.match(r'\AAtoms', line):
      #Found a new timestep
      if verts: 
        #Write saved data from previous
        writeTimeStep();
      #Create a new timestep entry in database
      db.timestep()
      verts = 0
      pass
    elif len(line) <= 2:
      #Skip blank lines
      pass
    elif (line.count(' ') >= 3):
      #Read particle position
      data = line.split()
      x = float(data[1])
      y = float(data[2])
      z = float(data[3])
      
      #Write vertex position, Points...
      points.addVertex(x, y, z)
      verts += 1

      #R,G,B colour if provided
      #if len(data) >= 6:
      #  rgba = [int(data[4]), int(data[5]), int(data[6]), 255]
      #  points.addColour(rgba)

      #Write chain id as value for colouring
      #points.addValue(int(data[0]))
      
  #Write remaining saved data
  if verts > 0: writeTimeStep()

  #Write window (sets global bounding box dims)
  if fixBB:
    db.addWindow("", bbmin, bbmax, "")

  #Close and write database to disk
  db.close()


