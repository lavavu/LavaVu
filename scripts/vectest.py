import sys
import re
import lvdb

dbPath = "testvectors.gldb"

#Create vis object
vectors = lvdb.Vectors('vectors', None)
#Create vis database for output
db = lvdb.Database(dbPath)

#Create a new timestep entry in database
db.timestep()

#Write vertex position
vectors.addVertex(0.0, 0.0, 0.0)
vectors.addVector(0.5, 0.5, 0.5)
vectors.addColour([255, 0, 0, 255])

vectors.addVertex(0.0, -0.5, -0.5)
vectors.addVector(1.0, 0.0, 0.0)
vectors.addColour([0, 255, 0, 255])

vectors.addVertex(-0.5, 0.0, -0.5)
vectors.addVector(0.0, 1.0, 0.0)
vectors.addColour([0, 0, 255, 255])

vectors.write(db)

db.addWindow("", [-0.5, -0.5, -0.5], [0.5, 0.5, 0.5], "")
#Close and write database to disk
db.close()


