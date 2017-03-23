#!/usr/bin/env python
#Parse LavaVu.cpp for property documentation comments
#Generate markdown output to docs/Property-Reference.md
import os
import subprocess
import re

def readFile(filename):
  f = open(filename,"r")
  contents = f.read()
  f.close()
  return contents

src = readFile("src/DrawState.h").split("\n")
gotComment = False
lastScope = ""
tokens = []
content = ""
TOC = ""
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

        #Some tweaks to default values
        default = default.replace('json::array()', '[]')
        default = default.replace('FLT_MAX', 'Infinity')
        default = default.replace('{', '[')
        default = default.replace('}', ']')

        if scope != lastScope:
            TOC += " * [" + scope + "](#" + re.sub(r'\W+', '', scope) + ")\n"
            content += "\n### " + scope + "\n\n"
            content += "| Property         | Type       | Default        | Description                               |\n"
            content += "| ---------------- | ---------- | -------------- | ----------------------------------------- |\n"
            lastScope = scope

        content += "|" + name.ljust(16)
        content += "|" + typename.ljust(10) + ""
        content += "|" + default.ljust(14) + ""
        content += "|" + desc + "|\n"

    elif line.startswith("    // |"):
        gotComment = True
        tokens = line.split(' | ')

f = open("docs/Property-Reference.md", "w")
f.write("\n# Property reference\n\n")
f.write("\n" + TOC + "\n")
f.write(content)
f.close()
