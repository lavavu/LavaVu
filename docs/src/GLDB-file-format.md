## GLDB file format

LavaVu supports reading and writing it's own **.gldb** format files for storing visualisations. 
These are simply SQLite3 databases with the bulk of the vis data stored as binary blobs. So they can be opened with the sqlite3 command line tool to inspect or modify the data.

The "export" command (Viewer.export() in python) can be used to store the current visualisation in this compact single file format which can then be loaded rapidly for later viewing.

The full schema will not be described here but the most important tables are:

* "object" : Definitions of objects to draw and all their properties and settings.
* "colourmap" : Colour maps available for use when drawing field data with objects.
* "geometry" : The actual geometry and field datasets. Along with a few metadata fields the actual data is stored in 32-bit floating point blocks in the "data" field. This is usually zlib compressed.

