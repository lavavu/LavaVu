import sys
import re
import lavavu

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
colours = lavavu.RandomColourMap(colours=15, seed=34)
#Create vis object, points and lines
points = lavavu.Points('molecules', colours, size=10, pointtype=lavavu.Points.ShinySphere)
links = lavavu.Lines('links', colours, width=1, link=True, flat=True)
#Create vis database for output
db = lavavu.Database(dbPath)
#Write the colourmap to the database
colours.write(db)

#For storing bounding box over full simulation
bbmin = [float("inf"),float("inf"),float("inf")]
bbmax = [float("-inf"),float("-inf"),float("-inf")]

#Function to write a stored chain of molecules to database
def writeChain():
  global db, points, links, bbmin, bbmax
  #Set value min/max to number of chains
  points.vmin = links.vmin = 0
  points.vmax = links.vmax = 15
  #Save min/max bounding box dims
  if fixBB:
    for i in range(3):
      if bbmin[i] > points.bmin[i]: bbmin[i] = points.bmin[i]
      if bbmax[i] < points.bmax[i]: bbmax[i] = points.bmax[i]
  points.write(db)
  links.write(db)

#Loop over lines in input file
with open(filePath, 'r') as file:
  lastnum = None

  for line in file:
    if re.match(r'\AAtoms', line):
      #Found a new timestep
      if lastnum: 
        #Write saved data from previous chain
        writeChain();
      #Create a new timestep entry in database
      db.timestep()
      lastnum = None
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
      num = int(data[0])

      if lastnum and lastnum != num:
        #Save and start a new chain when id number changes
        writeChain()

      #Write vertex position, Points...
      points.addVertex(x, y, z)
      #... and Linking lines
      links.addVertex(x, y, z)

      #R,G,B colour if provided
      if len(data) >= 6:
        rgba = [int(data[4]), int(data[5]), int(data[6]), 255]
        points.addColour(rgba)
        lines.addColour(rgba)

      #Write chain id as value for colouring
      #if lastnum != num:
      #if lastnum: print "old %d new %d" % (lastnum, num)
      links.addValue(num)
      points.addValue(num)

      lastnum = num

  #Write remaining saved data
  if lastnum: writeChain()

  #Write window (sets global bounding box dims)
  if fixBB:
    db.addWindow("", bbmin, bbmax, "")

  #Close and write database to disk
  db.close()


