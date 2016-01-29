import sys
import re
import sqlite3
import struct

dbPath = sys.argv[1]

#Bounding box
bmin = [0,0,0]
bmax = [501,501,501]

db = sqlite3.connect(dbPath)

query = "update geometry set minX = %g, minY = %g, minZ = %g, maxX = %g, maxY = %g, maxZ = %g" % (bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2])
print query
db.execute(query);

db.commit()

db.close()


