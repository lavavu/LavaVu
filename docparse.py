#!/usr/bin/env python
#Parse LavaVu.cpp for property documentation comments
#Generate markdown output to docs/Property-Reference.md
import os
import subprocess

def readFile(filename):
  f = open(filename,"r")
  contents = f.read()
  f.close()
  return contents

f = open("docs/Property-Reference.md", "w")
f.write("\n##Property reference\n\n")

src = readFile("src/LavaVu.cpp").split("\n")
gotComment = False
lastScope = ""
tokens = []
for line in src:
    #Already read a comment descriptor, read the name and default value
    if gotComment:
        gotComment = False
        pos1 = line.find('"')+1
        pos2 = line.find('"', pos1)
        pos3 = line.find('=')+1
        pos4 = line.find(';', pos3)

        #Extract the information
        name = '*' + line[pos1:pos2] + '*'
        default = line[pos3+1:pos4]
        scope = tokens[1]
        typename = tokens[2]
        desc = tokens[3]

        if scope != lastScope:
            f.write("\n###" + scope + "\n")
            f.write("| **Property**     | Type       | Default        | Description                               |\n")
            f.write("| ---------------- | ---------- | -------------- | ----------------------------------------- |\n")
            lastScope = scope

        f.write("|" + name.ljust(16))
        f.write("|" + typename.ljust(10) + "")
        f.write("|" + default.ljust(14) + "")
        f.write("|" + desc + "|\n")

    elif line.startswith("  // |"):
        gotComment = True
        tokens = line.split(' | ')

f.close()
