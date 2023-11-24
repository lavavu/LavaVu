#!/usr/bin/env python

#sudo apt install emscripten
#make clean
#make emscripten

#Creates amalgamated js and css for emscripten web viewer
filenames = ["dat.gui.min.js", "OK-min.js", "baseviewer.js", "menu.js", "emscripten.js", "LavaVu.js"]
with open("html/LavaVu-amalgamated.js", "w") as text_file:
    text_file.write('\n'.join(open('html/' + f).read() for f in filenames))

filenames = ["emscripten.css", "control.css", "styles.css", "gui.css"]
with open("html/LavaVu-amalgamated.css", "w") as text_file:
    text_file.write('\n'.join(open('html/' + f).read() for f in filenames))

