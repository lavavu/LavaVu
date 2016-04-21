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

f.write("\n##Setting properties\n\n")
f.write("\nProperties can be changed by entering the property name followed by = then the desired value\n");
f.write("\nIf an object is selected the property is applied to that object\n");
f.write("\n    select myobj\n");
f.write("\n    opacity=0.5\n");
f.write("\nOtherwise, if property is applicable to the current view/camera it is applied to it, if not then the global property is set.\n");
f.write("\n    border=0    #Applies to view, disabling border\n");
f.write("\n    opacity=0.7 #Applies global default opacity\n");
f.write("\nNumerical properties can be incremented or decremented as follows:\n");
f.write("\n    pointsize+=1\n");
f.write("\n    opacity-=0.1\n");
f.write("\nBoolean properties can be set to 1/0 or true/false\n");
f.write("\nColour properties can be specified by html style strings, [X11 colour names](https://en.wikipedia.org/wiki/X11_color_names#Color_name_chart) or json RGBA arrays\n");

f.write("\n    colour=#ff0000\n");
f.write("\n    colour=red\n");
f.write("\n    colour=rgba(255,0,0,1)\n");
f.write("\n    colour=[1,0,0,1]\n");

f.write("\n##Property reference\n\n")

src = readFile("src/LavaVu.cpp").split("\n")
gotComment = False
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
        name = line[pos1:pos2]
        default = line[pos3+1:pos4]
        scope = tokens[1]
        typename = tokens[2]
        desc = tokens[3]

        f.write("**" + name + "**\n")
        f.write("*Applies to: " + scope + "* :  \n")
        f.write("type: " + typename + ", default:  " + default + "  \n")
        f.write(desc + "  \n\n")

    elif line.startswith("  // |"):
        gotComment = True
        tokens = line.split(' | ')

f.close()
