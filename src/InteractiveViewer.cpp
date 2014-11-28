/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
** Copyright (c) 2010, Monash University
** All rights reserved.
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
**       * Redistributions of source code must retain the above copyright notice,
**          this list of conditions and the following disclaimer.
**       * Redistributions in binary form must reproduce the above copyright
**         notice, this list of conditions and the following disclaimer in the
**         documentation and/or other materials provided with the distribution.
**       * Neither the name of the Monash University nor the names of its contributors
**         may be used to endorse or promote products derived from this software
**         without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
** THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
** OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** Contact:
*%  Owen Kaluza - Owen.Kaluza(at)monash.edu
*%
*% Development Team :
*%  http://www.underworldproject.org/aboutus.html
**
**~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "GLuciferViewer.h"
#include "ColourMap.h"

/////////////////////////////////////////////////////////////////////////////////
// Event handling
/////////////////////////////////////////////////////////////////////////////////
bool GLuciferViewer::mousePress(MouseButton btn, bool down, int x, int y) 
{
   //Only process on mouse release
   static bool translated = false;
   bool redraw = false;
   int scroll = 0;
   viewer->notIdle(); //Reset idle timer
   if (down)
   {
      translated = false;
      if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
      viewer->button = btn;
      viewer->last_x = x;
      viewer->last_y = y;
   }
   else
   {
      aview->inertia(false); //Clear inertia

      //switch (viewer->button)
      switch (btn)
      {
      case WheelUp:
         scroll = 1;
         redraw = true;
         break;
      case WheelDown:
         scroll = -1;
         redraw = true;
         break;
      case LeftButton:
         if (!viewer->keyState.alt && !viewer->keyState.shift)
            aview->rotated = true;
         redraw = true;
         break;
      case MiddleButton:
         aview->rotated = true;
         break;
      case RightButton:
         if (!viewer->keyState.alt && !viewer->keyState.shift)
            translated = true;
      default:
         break;
      }

      if (scroll) mouseScroll(scroll);

      //Update cam move in history
      if (translated) record(true, aview->translateString());
      if (aview->rotated) record(true, aview->rotateString());

      viewer->button = NoButton;
   }
   return redraw;
}

bool GLuciferViewer::mouseMove(int x, int y)
{
   float adjust;
   int dx = x - viewer->last_x;
   int dy = y - viewer->last_y;
   viewer->last_x = x;
   viewer->last_y = y;
   viewer->notIdle(); //Reset idle timer

   //For mice with no right button, ctrl+left 
   if (viewer->keyState.ctrl && viewer->button == LeftButton)
      viewer->button = RightButton;

   switch (viewer->button) {
   case LeftButton:
      if (viewer->keyState.alt || viewer->keyState.shift)
         //Mac glut scroll-wheel alternative
         record(true, aview->zoom(-dy * 0.01));
      else
      {
         // left = rotate
         aview->rotate(dy / 5.0f, dx / 5.0, 0.0f);
      }
      break;
   case RightButton:
      if (viewer->keyState.alt || viewer->keyState.shift)
         //Mac glut scroll-wheel alternative
         record(true, aview->zoomClip(-dy * 0.001));
      else
      {
         // right = translate
         adjust = aview->model_size / 1000.0;   //1/1000th of size
         aview->translate(dx * adjust, -dy * adjust, 0);
      }
      break;
   case MiddleButton:
      // middle = rotate z (roll)
      aview->rotate(0.0f, 0.0f, dx / 5.0f);
      break;
   default: 
      return false;
   }

   return true;
}

bool GLuciferViewer::mouseScroll(int scroll) 
{
   //Only process on mouse release
   //Process wheel scrolling
   //CTRL+ALT+SHIFT = eye-separation adjust
   if (viewer->keyState.alt && viewer->keyState.shift && viewer->keyState.ctrl)
      record(true, aview->adjustStereo(0, 0, scroll / 500.0));
   //ALT+SHIFT = focal-length adjust
   if (viewer->keyState.alt && viewer->keyState.shift)
      record(true, aview->adjustStereo(0, scroll * aview->model_size / 100.0, 0));
   //CTRL+SHIFT - fine adjust near clip-plane
   if (viewer->keyState.shift && viewer->keyState.ctrl)
      record(true, aview->zoomClip(scroll * 0.001));
   //SHIFT = move near clip-plane
   else if (viewer->keyState.shift)
      record(true, aview->zoomClip(scroll * 0.01));
   //ALT = adjust field of view (aperture)
   else if (viewer->keyState.alt)
      record(true, aview->adjustStereo(scroll, 0, 0));
   else if (viewer->keyState.ctrl)
      //Fast zoom
      record(true, aview->zoom(scroll * 0.1));
   //Default = slow zoom
   else
      record(true, aview->zoom(scroll * 0.01));

   return true;
}

bool GLuciferViewer::keyPress(unsigned char key, int x, int y)
{
   viewer->notIdle(); //Reset idle timer
   if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
   return parseChar(key);
}

void GLuciferViewer::record(bool mouse, std::string command)
{
   command.erase(command.find_last_not_of("\n\r")+1);

   if (!recording) return;
   //This is to allow capture of history from stdout
   if (output) std::cout << command << std::endl;
   //std::cerr << command << std::endl;
   history.push_back(command);
   //Add to linehistory only if not a duplicate
   if (!mouse)
   {
      int lend = linehistory.size()-1;
      if (lend < 0 || command != linehistory[lend])
         linehistory.push_back(command);
   }
}

bool GLuciferViewer::parseChar(unsigned char key)
{
   static int historyline = -1;
   int hline = historyline;
   if (hline < 0) hline = linehistory.size();
   bool response = true;
   bool msg = false;
   historyline = -1;

   //ALT commands
   if (viewer->keyState.alt)
   {
      switch(key) 
      {
      case KEY_UP:    return parseCommands("model up");
      case KEY_DOWN:  return parseCommands("model down");
      case '*':       return parseCommands("autozoom");
      case '/':       return parseCommands("stereo");
      case '\\':      return parseCommands("coordsystem");
      case ',':       return parseCommands("pointtype all");
      case '-':       return parseCommands("pointsample down");
      case '+':       return parseCommands("pointsample up");
      case '=':       return parseCommands("pointsample up");
      case '|':       return parseCommands("rulers");
      case 'a':       return parseCommands("axis");
      case 'b':       return parseCommands("background invert");
      case 'c':       return parseCommands("camera");
      case 'd':       return parseCommands("trianglestrips");
      case 'e':       return parseCommands("list elements");
      case 'f':       return parseCommands("border");
      case 'g':       return parseCommands("logscales");
      case 'h':       return parseCommands("history");
      case 'i':       return parseCommands("image");
      case 'j':       return parseCommands("localise");
      case 'k':       return parseCommands("lockscale");
      case 'u':       return parseCommands("cullface");
      case 'w':       return parseCommands("wireframe");
      case 'J':       return parseCommands("json");
      case 'o':       return parseCommands("list objects");
      case 'q':       return parseCommands("quit");
      case 'r':       return parseCommands("reset");
      case 'p':       return parseCommands("scale points up");
      case 's':       return parseCommands("scale shapes up");
      case 't':       return parseCommands("scale tracers up");
      case 'v':       return parseCommands("scale vectors up");
      case 'l':       return parseCommands("scale lines up");
      case 'R':       return parseCommands("reload");
      case 'B':       return parseCommands("background");
      case 'S':       return parseCommands("scale shapes down");
      case 'V':       return parseCommands("scale vectors down");
      case 'T':       return parseCommands("scale tracers down");
      case 'P':       return parseCommands("scale points down");
      case 'L':       return parseCommands("scale lines down");
      case ' ':    
         if (loop)
            return parseCommands("stop");
         else
            return parseCommands("play");
      default:        return false;
      }
   }

   //Direct commands and verbose command entry
   switch (key) 
   {
   case KEY_RIGHT:    return parseCommands("timestep down");
   case KEY_LEFT:     return parseCommands("timestep up");
   case KEY_ESC:      return parseCommands("quit");
   case KEY_ENTER:
      if (entry.length() == 0)
         return parseCommands("repeat");
      if (entry.length() == 1)
      {
         char ck = entry.at(0);
         //Digit? (9 == ascii 57)
         if (ck < 57)
         {
           response = parseCommands(entry);
           entry = "";
           return response;
         }
         switch (ck)
         {
            case 'p':   //Points
               if (points->allhidden)
                 response = parseCommands("show points");
               else
                 response = parseCommands("hide points");
               break;
            case 'v':   //Vectors
               if (vectors->allhidden)
                 response = parseCommands("show vectors");
               else
                 response = parseCommands("hide vectors");
               break;
            case 't':   //Tracers
               if (tracers->allhidden)
                 response = parseCommands("show tracers");
               else
                 response = parseCommands("hide tracers");
               break;
            case 'u':   //TriSurfaces
               if (triSurfaces->allhidden)
                 response = parseCommands("show triangles");
               else
                 response = parseCommands("hide triangles");
               break;
            case 'q':   //QuadSurfaces
               if (quadSurfaces->allhidden)
                 response = parseCommands("show quads");
               else
                 response = parseCommands("hide quads");
               break;
            case 's':   //Shapes
               if (shapes->allhidden)
                 response = parseCommands("show shapes");
               else
                 response = parseCommands("hide shapes");
               break;
            case 'l':   //Lines
               if (lines->allhidden)
                 response = parseCommands("show lines");
               else
                 response = parseCommands("hide lines");
               break;
            default:
               //Parse as if entered with ALT
               viewer->keyState.alt = true;
               response = parseChar(ck);
         }
      }
      else
        response = parseCommands(entry);
      entry = "";
      break;
   case KEY_DELETE:
   case KEY_BACKSPACE:  //Backspace
      if (entry.size() > 0)
      {
         std::string::iterator it;
         it = entry.end() - 1;
         entry.erase(it);
      }
      msg = true;
      break;
   case KEY_UP:
      historyline = hline-1;
      if (linehistory.size())
      {
         if (historyline < 0) historyline = 0;
         entry = linehistory[historyline];
      }
      msg = true;
      break;
   case KEY_DOWN:
      historyline = hline+1;
      if (historyline >= linehistory.size())
      {
         historyline = -1;
         entry = "";
      }
      else
         entry = linehistory[historyline];
      msg = true;
      break;
   case KEY_PAGEUP:
      //Previous viewport
      if (viewAll) 
      {
         viewAll = false;
         redrawObjects();
      }
      viewSelect(view-1);
      printMessage("Set viewport to %d", view);
      if (!viewPorts) redrawObjects();
      break;
   case KEY_PAGEDOWN:
      //Next viewport
      if (viewAll) 
      {
         viewAll = false;
         redrawObjects();
      }
      viewSelect(view+1);
      printMessage("Set viewport to %d", view);
      if (!viewPorts) redrawObjects();
      break;
   case KEY_HOME:
      //All viewports
      viewAll = !viewAll;
      if (viewAll) viewPorts = false; //Invalid in viewAll mode
      redrawObjects();
      printMessage("View All set to %s", (viewAll ? "ON" : "OFF"));
      break;
   case KEY_END:
      //No viewports (use first view but full window and all objects)
      viewPorts = !viewPorts;
      if (viewPorts) viewAll = false;  //Invalid in viewPorts mode
      redrawObjects();
      printMessage("ViewPorts set to %s", (viewPorts ? "ON" : "OFF"));
      break;
   case '`':    return parseCommands("fullscreen");
   case KEY_F1: return parseCommands("help");
   case KEY_F2: return parseCommands("antialias");
   default:
      //Only add printable characters
      if (key > 31 && key < 127)
        entry += key;
      msg = true;
      break;
   }
   //if (entry.length() > 0) printMessage(": %s", entry.c_str());
   if (entry.length() > 0 || msg)
   {
     printMessage(": %s", entry.c_str());
     response = false;
     if (!response) 
        viewer->postdisplay = true;
   }

   return response;
}

Geometry* GLuciferViewer::getGeometryType(std::string what)
{
   if (what == "points")
      return points;
   if (what == "labels")
      return labels;
   if (what == "vectors")
      return vectors;
   if (what == "tracers")
      return tracers;
   if (what == "triangles")
      return triSurfaces;
   if (what == "quads")
      return quadSurfaces;
   if (what == "shapes")
      return shapes;
   if (what == "lines")
      return lines;
   return NULL;
}

DrawingObject* GLuciferViewer::findObject(std::string what, int id)
{
   //Find by name/ID match in all drawing objects
   std::transform(what.begin(), what.end(), what.begin(), ::tolower);
   for (unsigned int i=0; i<amodel->objects.size(); i++)
   {
      if (!amodel->objects[i]) continue;
      std::string namekey = amodel->objects[i]->name;
      std::transform(namekey.begin(), namekey.end(), namekey.begin(), ::tolower);
      if (namekey == what || id > 0 && id == amodel->objects[i]->id)
      {
         return amodel->objects[i];
      }
   }
   return NULL;
}

ColourMap* GLuciferViewer::findColourMap(std::string what, int id)
{
   //Find by name/ID match in all colour maps
   std::transform(what.begin(), what.end(), what.begin(), ::tolower);
   for (unsigned int i=0; i<amodel->colourMaps.size(); i++)
   {
      if (!amodel->colourMaps[i]) continue;
      std::string namekey = amodel->colourMaps[i]->name;
      std::transform(namekey.begin(), namekey.end(), namekey.begin(), ::tolower);
      if (namekey == what || id > 0 && id == amodel->colourMaps[i]->id)
      {
         return amodel->colourMaps[i];
      }
   }
   return NULL;
}

bool GLuciferViewer::parseCommands(std::string cmd)
{
   bool redisplay = true;
   static std::string last_cmd;
   std::stringstream ss(cmd);
   PropertyParser parsed = PropertyParser(ss);

   //Verbose command processor
   float fval;
   int ival;
   if (parsed.exists("rotation"))
   {
      std::cerr << cmd << std::endl;
      float x = 0, y = 0, z = 0, w = 0;
      if (parsed.has(x, "rotation", 0) &&
          parsed.has(y, "rotation", 1) &&
          parsed.has(z, "rotation", 2) &&
          parsed.has(w, "rotation", 3))
      {
        aview->setRotation(Quaternion(x, y, z, w));
        aview->rotated = true;  //Flag rotation finished
      }
   }
   else if (parsed.exists("translation"))
   {
      float x = 0, y = 0, z = 0;
      if (parsed.has(x, "translation", 0) &&
          parsed.has(y, "translation", 1) &&
          parsed.has(z, "translation", 2))
      {
        aview->setTranslation(x, y, z);
      }
   }
   else if (parsed.exists("rotate"))
   {
      float xr = 0, yr = 0, zr = 0;
      std::string axis = parsed["rotate"];
      if (axis == "x")
         parsed.has(xr, "rotate", 1);
      else if (axis == "y")
         parsed.has(yr, "rotate", 1);
      else if (axis == "z")
         parsed.has(zr, "rotate", 1);
      else 
      {
         parsed.has(xr, "rotate", 0);
         parsed.has(yr, "rotate", 1);
         parsed.has(zr, "rotate", 2);
      }
      aview->rotate(xr, yr, zr);
      aview->rotated = true;  //Flag rotation finished
   }
   else if (parsed.has(fval, "rotatex"))
   {
      aview->rotate(fval, 0, 0);
      aview->rotated = true;  //Flag rotation finished
   }
   else if (parsed.has(fval, "rotatey"))
   {
      aview->rotate(0, fval, 0);
      aview->rotated = true;  //Flag rotation finished
   }
   else if (parsed.has(fval, "rotatez"))
   {
      aview->rotate(0, 0, fval);
      aview->rotated = true;  //Flag rotation finished
   }
   else if (parsed.has(fval, "zoom"))
      aview->zoom(fval);
   else if (parsed.has(fval, "translatex"))
      aview->translate(fval, 0, 0);
   else if (parsed.has(fval, "translatey"))
      aview->translate(0, fval, 0);
   else if (parsed.has(fval, "translatez"))
      aview->translate(0, 0, fval);
   else if (parsed.exists("translate"))
   {
      float xt = 0, yt = 0, zt = 0;
      std::string axis = parsed["translate"];
      if (axis == "x")
         parsed.has(xt, "translate", 1);
      else if (axis == "y")
         parsed.has(yt, "translate", 1);
      else if (axis == "z")
         parsed.has(zt, "translate", 1);
      else 
      {
         parsed.has(xt, "translate", 0);
         parsed.has(yt, "translate", 1);
         parsed.has(zt, "translate", 2);
      }
      aview->translate(xt, yt, zt);
   }
   else if (parsed.exists("focus"))
   {
      float xf = 0, yf = 0, zf = 0;
      if (parsed.has(xf, "focus", 0) &&
          parsed.has(yf, "focus", 1) &&
          parsed.has(zf, "focus", 2))
      {
         aview->focus(xf, yf, zf);
      }
   }
   else if (parsed.has(fval, "aperture"))
      aview->adjustStereo(fval, 0, 0);
   else if (parsed.has(fval, "focallength"))
      aview->adjustStereo(0, fval, 0);
   else if (parsed.has(fval, "eyeseparation"))
      aview->adjustStereo(0, 0, fval);
   else if (parsed.has(fval, "zoomclip"))
      aview->zoomClip(fval);
   else if (parsed.has(fval, "nearclip"))
      aview->near_clip = fval;
   else if (parsed.has(fval, "farclip"))
      aview->far_clip = fval;
   else if (parsed.exists("timestep"))  //Absolute
   {
      if (!parsed.has(ival, "timestep"))
      {
         if (parsed["timestep"] == "up")
            ival = timestep-1;
         else if (parsed["timestep"] == "down")
            ival = timestep+1;
         else
            ival = timestep;
      }
      if (setTimeStep(ival) >= 0)
      {
         printMessage("Go to timestep %d", timestep);
         resetViews(); //Update the viewports
      }
      else
         return false;  //Invalid
   }
   else if (parsed.has(ival, "jump"))      //Relative
   {
      //Relative jump
      if (setTimeStep(timestep+ival) >= 0)
      {
         printMessage("Jump to timestep %d", timestep);
         resetViews(); //Update the viewports
      }
      else
         return false;  //Invalid
   }
   else if (parsed.exists("model"))      //Model switch
   {
      if (!parsed.has(ival, "model"))
      {
         if (parsed["model"] == "up")
            ival = window-1;
         else if (parsed["model"] == "down")
            ival = window+1;
         else
            ival = window;
      }
      if (ival < 0) ival = windows.size()-1;
      if (ival >= (int)windows.size()) ival = 0;
      if (!loadWindow(ival, timestep)) return false;  //Invalid
      printMessage("Load model %d", window);
   }
   else if (parsed.exists("hide") || parsed.exists("show"))
   {
      std::string action = parsed.exists("hide") ? std::string("hide") : std::string("show");
      std::string what = parsed[action];
      Geometry* active = getGeometryType(what);

      //Have selected a geometry type?
      if (active)
      {
         int id;
         if (parsed.has(id, action, 1))
         {
            parsed.has(id, action, 1);
            if (action == "hide")
            {
               if (!active->hide(id)) return false; //Invalid
               printMessage("Hide %s %d", what.c_str(), id);
            }
            else
            {
               if (!active->show(id)) return false; //Invalid
               printMessage("Show %s %d", what.c_str(), id);
            }
         }
         else
         {
            if (action == "hide")
            {
               active->hideAll();
               printMessage("Hide all %s", what.c_str());
            }
            else
            {
               active->showAll();
               printMessage("Show all %s", what.c_str());
            }
         }
         redrawViewports();
      }
      else
      {
         //Hide by name/ID match in all drawing objects
         //std::string which = parsed.get(action, 1);
         for (int c=0; c<10; c++) //Allow multiple id/name specs on line
         {
            int id = parsed.Int(action, 0, c);
            DrawingObject* obj = findObject(what, id);
            if (obj)
            {
               if (obj->skip)
               {
                  std::ostringstream ss;
                  ss << "load " << obj->id;
                  return parseCommands(ss.str());
               }
               else
               {
                  showById(obj->id, action == "show");
                  //printMessage("%s object %d", (amodel->objects[i]->visible ? "Show" : "Hide"), num);
                  printMessage("%s object %s", action.c_str(), obj->name.c_str());
                  redrawViewports();
               }
            }
         }
      }
   }
   else if (parsed.has(ival, "dump"))
   {
      //Export drawing object by name/ID match 
      for (int c=0; c<10; c++) //Allow multiple id/name specs on line
      {
         DrawingObject* obj = findObject(parsed["dump"], parsed.Int("dump", 0, c));
         if (obj)
         {
            dumpById(obj->id);
            printMessage("Dumped object %s to CSV", obj->name.c_str());
         }
      }
   }
   else if (parsed.has(ival, "tracersteps"))
   {
      tracers->steps = ival;
      if (tracers->steps > timestep) tracers->steps = timestep;
      printMessage("Set tracer steps limit to %d", tracers->steps);
      tracers->redraw = true;
   }
   else if (parsed.has(fval, "alpha"))
   {
      if (fval > 1.0)
         GeomData::opacity = fval / 255.0;
      else
         GeomData::opacity = fval;
      printMessage("Set global alpha to %.2f", GeomData::opacity);
      quadSurfaces->redraw = true;
      triSurfaces->redraw = true;
      redrawViewports();
   }
   else if (parsed.exists("opacity"))
   {
      std::string what = parsed["opacity"];
      //Alpha by name/ID match in all drawing objects
      int id = parsed.Int("opacity", 0);
      DrawingObject* obj = findObject(what, id);
      if (obj)
      {
         if (parsed.has(fval, "opacity", 1))
            setOpacity(obj->id, fval);
         printMessage("%s opacity set to %f", obj->name.c_str(), obj->opacity);
         redrawViewports();
      }
   }
   else if (parsed.has(ival, "movie"))
   {
      encodeVideo("gLucifer.mp4", timestep, ival);
   }
   else if (parsed.has(ival, "play"))
   {
      writeSteps(false, false, timestep, ival, NULL);
   }
   else if (parsed.exists("play"))
   {
      //Play loop
      loop = true;
      OpenGLViewer::commands.push_back(std::string("next"));
   }
   else if (parsed.exists("next"))
   {
      int old = timestep;
      setTimeStep(timestep+1);
      //Allow loop back to start when using next command
      if (timestep > 0 && timestep == old)
         setTimeStep(0);

      if (loop)
         OpenGLViewer::commands.push_back(std::string("next"));
   }
   else if (parsed.exists("stop"))
   {
      loop = false;
   }
   else if (parsed.has(ival, "images"))
   {
      writeImages(timestep, ival);
   }
   else if (parsed.exists("repeat"))
   {
      bool state = recording;
      if (parsed["repeat"] == "history" && parsed.has(ival, "repeat", 1))
      {
         recording = false;
         bool animate = (parsed.get("repeat", 2) == "animate");
         for (int r=0; r<ival; r++)
         {
            for (unsigned int l=0; l<history.size(); l++)
               parseCommands(history[l]);
            if (animate) viewer->display();
         }
         recording = state;
         return true; //Skip record
      }
      else if (parsed.has(ival, "repeat"))
      {
         recording = false;
         bool animate = (parsed.get("repeat", 1) == "animate");
         for (int r=0; r<ival; r++)
         {
            parseCommands(last_cmd);
            if (animate) viewer->display();
         }
         recording = state;
         return true;  //Skip record
      }
      else
         return parseCommands(last_cmd);
   }
   else if (parsed.exists("quit"))
   {
      viewer->quitProgram = true;
   }
   else if (parsed.exists("axis"))
   {
      std::string axis = parsed["axis"];
      if (parsed["axis"] == "on")
        aview->axis = true;
      else if (parsed["axis"] == "off")
        aview->axis = false;
      else
        aview->axis = !aview->axis;
      printMessage("Axis %s", aview->axis ? "ON" : "OFF");
   }
   else if (parsed.exists("cullface"))
   {
      quadSurfaces->cullface = !quadSurfaces->cullface;
      triSurfaces->cullface = !triSurfaces->cullface;
      printMessage("Back face culling for surfaces is %s", quadSurfaces->cullface ? "ON":"OFF");
      quadSurfaces->redraw = true;
      triSurfaces->redraw = true;
   }
   else if (parsed.exists("wireframe"))
   {
      for (int type=lucMinType; type<lucMaxType; type++)
      {
         geometry[type]->wireframe = !geometry[type]->wireframe;
         geometry[type]->redraw = true;
      }
      printMessage("Wireframe %s", geometry[lucMinType]->wireframe ? "ON":"OFF");
   }
   else if (parsed.exists("trianglestrips"))
   {
      quadSurfaces->triangles = !quadSurfaces->triangles;
      printMessage("Triangle strips for quad surfaces %s", quadSurfaces->triangles ? "ON":"OFF");
      quadSurfaces->redraw = true;
   }
   else if (parsed.exists("redraw"))
   {
      for (int type=lucMinType; type<lucMaxType; type++)
      {
         geometry[type]->redraw = true;
      }
      printMessage("Redrawing all objects");
   }
   else if (parsed.exists("fullscreen"))
   {
      viewer->fullScreen();
      printMessage("Full-screen is %s", viewer->fullscreen ? "ON":"OFF");
   }
   else if (parsed.exists("scaling"))
   {
      printMessage("Scaling is %s", aview->scaleSwitch() ? "ON":"OFF");
   }
   else if (parsed.exists("fit"))
   {
      aview->zoomToFit();
   }
   else if (parsed.exists("autozoom"))
   {
      aview->autozoom = !aview->autozoom;
      printMessage("AutoZoom is %s", aview->autozoom ? "ON":"OFF");
   }
   else if (parsed.exists("stereo"))
   {
      aview->stereo = !aview->stereo;
      printMessage("Stereo is %s", aview->stereo ? "ON":"OFF");
   }
   else if (parsed.exists("coordsystem"))
   {
      printMessage("%s coordinate system", (aview->switchCoordSystem() == RIGHT_HANDED ? "Right-handed":"Left-handed"));
   }
   else if (parsed.exists("title"))
   {
      //Show/hide title
      aview->heading = !aview->heading;
      printMessage("Title %s", aview->heading ? "ON" : "OFF");
   }
   else if (parsed.exists("rulers"))
   {
      //Show/hide rulers
      aview->rulers = !aview->rulers;
      printMessage("Rulers %s", aview->rulers ? "ON" : "OFF");
   }
   else if (parsed.exists("log"))
   {
      ColourMap::logscales = (ColourMap::logscales + 1) % 3;
      bool state = ColourMap::lock;
      ColourMap::lock = false;
      for (unsigned int i=0; i<amodel->colourMaps.size(); i++)
         amodel->colourMaps[i]->calibrate();
      ColourMap::lock = state;  //restore setting
      printMessage("Log scales are %s", ColourMap::logscales  == 0 ? "DEFAULT": ( ColourMap::logscales  == 1 ? "ON" : "OFF"));
      redrawObjects();
   }
   else if (parsed.exists("help"))
   {
      viewer->display();  //Ensures any previous help text wiped
      std::string cmd = parsed["help"];
      if (cmd == "manual")
      {
         std::cout << HELP_MESSAGE;
         std::cout << "\nMiscellanious commands:\n\n";
         std::cout << helpCommand("quit") << helpCommand("repeat") << helpCommand("history");
         std::cout << helpCommand("clearhistory") << helpCommand("pause") << helpCommand("list");
         std::cout << helpCommand("timestep") << helpCommand("jump") << helpCommand("model") << helpCommand("reload");
         std::cout << helpCommand("next") << helpCommand("play") << helpCommand("stop") << helpCommand("script");
         std::cout << "\nView/camera commands:\n\n";
         std::cout << helpCommand("rotate") << helpCommand("rotatex") << helpCommand("rotatey");
         std::cout << helpCommand("rotatez") << helpCommand("rotation") << helpCommand("zoom") << helpCommand("translate");
         std::cout << helpCommand("translatex") << helpCommand("translatey") << helpCommand("translatez");
         std::cout << helpCommand("focus") << helpCommand("aperture") << helpCommand("focallength");
         std::cout << helpCommand("eyeseparation") << helpCommand("nearclip") << helpCommand("farclip");
         std::cout << helpCommand("zerocam") << helpCommand("reset") << helpCommand("camera");
         std::cout << helpCommand("resize") << helpCommand("fullscreen") << helpCommand("fit");
         std::cout << helpCommand("autozoom") << helpCommand("stereo") << helpCommand("coordsystem");
         std::cout << helpCommand("sort") << helpCommand("rotation") << helpCommand("translation");
         std::cout << "\nOutput commands:\n\n";
         std::cout << helpCommand("image") << helpCommand("images") << helpCommand("outwidth");
         std::cout << helpCommand("movie") << helpCommand("dump") << helpCommand("json");
         std::cout << "\nObject commands:\n\n";
         std::cout << helpCommand("hide") << helpCommand("show") << helpCommand("delete") << helpCommand("load");
         std::cout << "\nDisplay commands:\n\n";
         std::cout << helpCommand("background") << helpCommand("alpha") << helpCommand("opacity");
         std::cout << helpCommand("axis") << helpCommand("cullface") << helpCommand("wireframe");
         std::cout << helpCommand("trianglestrips") << helpCommand("redraw") << helpCommand("scaling") << helpCommand("rulers") << helpCommand("log");
         std::cout << helpCommand("antialias") << helpCommand("localise") << helpCommand("lockscale");
         std::cout << helpCommand("lighting") << helpCommand("colourmap") << helpCommand("pointtype");
         std::cout << helpCommand("vectorquality") << helpCommand("tracerflat");
         std::cout << helpCommand("tracerscale") << helpCommand("tracersteps") << helpCommand("pointsample");
         std::cout << helpCommand("border") << helpCommand("title") << helpCommand("scale");
      }
      else if (cmd.length() > 0)
      {
         displayText(helpCommand(cmd));
         std::cout << helpCommand(cmd);
      }
      else
      {
         displayText(helpCommand("help"));
         std::cout << HELP_MESSAGE;
         std::cout << helpCommand("help");
      }
      viewer->swap();  //Immediate display
      redisplay = false;
   }
   else if (parsed.exists("antialias"))
   {
      aview->antialias = !aview->antialias;
      printMessage("Anti-aliasing %s", aview->antialias ? "ON":"OFF");
   }
   else if (parsed.exists("localise"))
   {
      //Find colour value min/max local to each geom element
      points->localiseColourValues();
      vectors->localiseColourValues();
      tracers->localiseColourValues();
      quadSurfaces->localiseColourValues();
      triSurfaces->localiseColourValues();
      printMessage("ColourMap scales localised");
      redrawObjects();
   }
   else if (parsed.exists("json"))
   {
      //Export drawing object by name/ID match 
      for (int c=0; c<10; c++) //Allow multiple id/name specs on line
      {
         DrawingObject* obj = findObject(parsed["json"], parsed.Int("json", 0, c));
         if (obj)
         {
            jsonWriteFile(obj->id);
            printMessage("Dumped object %s to json", obj->name.c_str());
         }
         else
         {
            jsonWriteFile();
            printMessage("Dumped all objects to json", ival);
            break;
         }
      }
   }
   else if (parsed.exists("lockscale"))
   {
      ColourMap::lock = !ColourMap::lock;
      printMessage("ColourMap scale locking %s", ColourMap::lock ? "ON":"OFF");
      redrawObjects();
   }
   else if (parsed.exists("lighting"))
   {
      //Lighting ON/OFF
      for (int type=lucMinType; type<lucMaxType; type++)
      {
         geometry[type]->lit = !geometry[type]->lit;
         geometry[type]->redraw = true;
      }
      printMessage("Lighting is %s", geometry[lucMinType]->lit ? "ON":"OFF");
   }
   else if (parsed.exists("list"))
   {
      if (parsed["list"] == "objects")
      {
         objectlist = !objectlist;
         displayObjectList(true);
      }
      if (parsed["list"] == "elements")
      {
         //Print available elements by id
         for (unsigned int i=0; i < geometry.size(); i++)
            geometry[i]->print();
      }
      if (parsed["list"] == "colourmaps")
      {
         int offset = 0;
         for (unsigned int i=0; i < amodel->colourMaps.size(); i++)
         {
            if (amodel->colourMaps[i])
            {
               std::ostringstream ss;
               ss << std::setw(5) << amodel->colourMaps[i]->id << " : " << amodel->colourMaps[i]->name;

               displayText(ss.str(), ++offset);
               std::cerr << ss.str() << std::endl;
            }
         }
         viewer->swap();  //Immediate display
         record(false, cmd);
         return false;
      }
   }
   else if (parsed.exists("reset"))
   {
      aview->reset();     //Reset camera
      aview->init(true);  //Reset camera to default view of model
      printMessage("View reset");
   }
   else if (parsed.exists("reload"))
   {
      //Restore original window data
      if (!loadWindow(window, timestep)) return false;
   }
   else if (parsed.exists("zerocam"))
   {
      aview->reset();     //Zero camera
   }
   else if (parsed.exists("colourmap"))
   {
      std::string what = parsed["colourmap"];

      //Set colourmap on object by name/ID match
      int id = parsed.Int("colourmap", 0);
      DrawingObject* obj = findObject(what, id);
      if (obj)
      {
         std::string cname = parsed.get("colourmap", 1);
         //int cid = parsed.Int("colourmap", 2);
         parsed.has(ival, "colourmap", 1);
         ColourMap* cmap = findColourMap(cname, ival);
         //Only able to set the value colourmap now
         obj->addColourMap(cmap, lucColourValueData);
         if (cmap)
            printMessage("%s colourmap set to %s (%d)", obj->name.c_str(), cmap->name.c_str(), cmap->id);
         else
            printMessage("%s colourmap set to none", obj->name.c_str());
         redraw(id);
         redrawViewports();
      }
   }
   //TODO: document colourvalue,colourset,colour
   else if (parsed.exists("colourvalue"))
   {
      std::string what = parsed["colourvalue"];
      //Set value on colourmap (id idx val)
      int id = parsed.Int("colourvalue", 1);
      int idx = parsed.Int("colourvalue", 2);
      float val = parsed.Float("colourvalue", 3);
      ColourMap* cmap = findColourMap(what, id);
      cmap->colours[idx].value = val;
      cmap->calibrate();
            printMessage("%s colourmap idx (%d) value set to (%f)", cmap->name.c_str(), idx, val);
      //Will need to redraw all objects using this colourmap...
   }
   else if (parsed.exists("colourset"))
   {
      std::string what = parsed["colourset"];
      //Set colourmap colour (id idx val-hex-rgba)
      int id = parsed.Int("colourset", 1);
      int idx = parsed.Int("colourset", 2);
      unsigned int colour = parsed.Colour("colourset", 3);
      ColourMap* cmap = findColourMap(what, id);
      cmap->colours[idx].colour.value = colour;
      cmap->calibrate();
            printMessage("%s colourmap idx (%d) colour set to (%x)", cmap->name.c_str(), idx, colour);
      //Will need to redraw all objects using this colourmap...
   }
   else if (parsed.exists("colour"))
   {
      //Set object colour by name/ID match in all drawing objects
      std::string what = parsed["colour"];
      int id = parsed.Int("colour", 0);
      DrawingObject* obj = findObject(what, id);
      if (obj)
      {
         unsigned int colour = parsed.Colour("colour", 1);
         obj->colour.value = colour;
         printMessage("%s colour set to %x", obj->name.c_str(), colour);
         redraw(id);
         redrawViewports();
      }
   }
   else if (parsed.exists("pointtype"))
   {
      std::string what = parsed["pointtype"];
      if (what == "all")
      {
         //Should use a set method here...
         if (parsed.has(ival, "pointtype", 1))
            Points::pointType = ival;
         else
            Points::pointType++;
         if (Points::pointType > 4) Points::pointType = 0;
         if (Points::pointType < 0) Points::pointType = 4;
         printMessage("Point type %d", Points::pointType);
      }
      else
      {
         //Point type by name/ID match in all drawing objects
         int id = parsed.Int("pointtype", 0);
         DrawingObject* obj = findObject(what, id);
         if (obj)
         {
            if (parsed.has(ival, "pointtype", 1))
               obj->pointType = ival;
            else if (parsed.get("pointtype", 1) == "up")
               obj->pointType = (obj->pointType-1) % 5;
            else if (parsed.get("pointtype", 1) == "down")
               obj->pointType = (obj->pointType+1) % 5;
            printMessage("%s point type set to %d", obj->name.c_str(), obj->pointType);
            geometry[lucPointType]->redraw = true;
            redraw(id);
            redrawViewports();
         }
      }
   }
   else if (parsed.has(ival, "vectorquality"))
   {
      if (ival <0 || ival > 10) return false;
      vectors->redraw = true;
      vectors->glyphs = ival;
      if (ival == 0) vectors->flat = true;
      printMessage("Vector quality set to %d", vectors->glyphs);
   }
   else if (parsed.exists("tracerflat"))
   {
      tracers->redraw = true;
      tracers->flat = tracers->flat ? false : true;
      printMessage("Flat tracer rendering is %s", tracers->flat ? "ON":"OFF");
   }
   else if (parsed.exists("tracerscale"))
   {
      tracers->redraw = true;
      tracers->scaling = tracers->scaling ? false : true;
      printMessage("Scaled tracer rendering is %s", tracers->scaling ? "ON":"OFF");
   }
   else if (parsed.exists("pointsample"))
   {
      if (parsed.has(ival, "pointsample"))
         Points::subSample = ival;
      else if (parsed["pointsample"] == "up")
         Points::subSample /= 2;
      else if (parsed["pointsample"] == "down")
         Points::subSample *= 2;
      if (Points::subSample < 1) Points::subSample = 1;
      points->redraw = true;
      printMessage("Point sampling %d", Points::subSample);
   }
   else if (parsed.exists("image"))
   {
      viewer->snapshot(awin->name.c_str());
      if (viewer->outwidth > 0)
         printMessage("Saved image %d x %d", viewer->outwidth, viewer->outheight);
      else
         printMessage("Saved image %d x %d", viewer->width, viewer->height);
   }
   else if (parsed.has(ival, "outwidth"))
   {
      if (ival < 10) return false;
      viewer->outwidth = ival;
      printMessage("Output image width set to %d", viewer->outwidth);
   }
   else if (parsed.exists("background"))
   {
      if (parsed["background"] == "white")
         awin->background.value = 0xffffffff;
      else if (parsed["background"] == "black")
         awin->background.value = 0xff000000;
      else if (parsed["background"] == "grey")
         awin->background.value = 0xff666666;
      else if (parsed["background"] == "invert")
         Colour_Invert(awin->background);
      else if (parsed.has(fval, "background"))
      {
         awin->background.a = 255;
         awin->background.r = awin->background.g = awin->background.b = fval * 255;
      }
      else
      {
         if (awin->background.r == 255 || awin->background.r == 0)
            awin->background.value = 0xff666666;
         else
            awin->background.value = 0xff000000;
      }
      viewer->setBackground(awin->background.value);
      printMessage("Background colour set");
   }
   else if (parsed.exists("border"))
   {
      //Frame off/on/filled
      if (parsed["border"] == "on")
         aview->borderType = 1;
      else if (parsed["border"] == "off")
         aview->borderType = 0;
      else if (parsed["border"] == "filled")
         aview->borderType = 2;
      else 
         aview->borderType++;
      aview->borderType = aview->borderType % 3;
      printMessage("Frame set to %s", aview->borderType == 0 ? "OFF": (aview->borderType == 1 ? "ON" : "FILLED"));
   }
   else if (parsed.exists("camera"))
   {
      //Output camera view xml
      aview->print();
   }
   else if (parsed.exists("scale"))
   {
      std::string what = parsed["scale"];
      Geometry* active = getGeometryType(what);
      if (active) 
      {
         if (parsed.has(fval, "scale", 1))
           active->scale = fval;
         else if (parsed.get("scale", 1) == "up")
           active->scale *= 1.5;
         else if (parsed.get("scale", 1) == "down")
           active->scale /= 1.5;
         active->redraw = true;
         printMessage("%s scaling set to %f", what.c_str(), active->scale);
      }
      else
      {
         //X,Y,Z: geometry scaling
         if (what == "x")
         {
            if (parsed.has(fval, "scale", 1))
               aview->setScale(fval, 1, 1);
         }
         else if (what == "y")
         {
            if (parsed.has(fval, "scale", 1))
               aview->setScale(1, fval, 1);
         }
         else if (what == "z")
         {
            if (parsed.has(fval, "scale", 1))
               aview->setScale(1, 1, fval);
         }
         else
         {
            //Scale by name/ID match in all drawing objects
            int id = parsed.Int("scale", 0);
            DrawingObject* obj = findObject(what, id);
            if (obj)
            {
               if (parsed.has(fval, "scale", 1))
                  obj->scaling = fval;
               else if (parsed.get("scale", 1) == "up")
                  obj->scaling *= 1.5;
               else if (parsed.get("scale", 1) == "down")
                  obj->scaling /= 1.5;
               printMessage("%s scaling set to %f", obj->name.c_str(), obj->scaling);
               for (int type=lucMinType; type<lucMaxType; type++)
                  geometry[type]->redraw = true;
               redraw(id);
               redrawViewports();
            }
         }
      }
   }
   else if (parsed.exists("history"))
   {
      std::string scriptfile = parsed["history"];
      if (scriptfile.length() > 0)
      {
         std::ofstream file(scriptfile.c_str(), std::ios::out);
         if (file.is_open())
         {
            for (unsigned int l=0; l<history.size(); l++)
               file << history[l] << std::endl;
            file.close();
         }
         else
            printMessage("Unable to open file: %s", scriptfile.c_str());
      }
      else
      {
         for (unsigned int l=0; l<history.size(); l++)
            std::cout << history[l] << std::endl;
      }
   }
   else if (parsed.exists("clearhistory"))
   {
      history.clear();
      return false; //Skip record
   }
   else if (parsed.exists("resize"))
   {
      float w = 0, h = 0;
      if (parsed.has(w, "resize", 0) && parsed.has(h, "resize", 1))
      {
         if (w != viewer->width && h != viewer->height)
         {
            viewer->setsize(w, h);
            resetViews(true);
         }
      }
   }
   else if (parsed.has(ival, "pause"))
   {
      PAUSE(ival);
   }
   else if (parsed.exists("delete"))
   {
      //Delete drawing object by name/ID match 
      std::string what = parsed["delete"];
      int id = parsed.Int("delete", 0);
      DrawingObject* obj = findObject(what, id);
      if (obj)
      {
         amodel->reopen(true);  //Open writable
         char SQL[256];
         sprintf(SQL, "DELETE FROM object WHERE id==%1$d; DELETE FROM geometry WHERE object_id=%1$d; DELETE FROM viewport_object WHERE object_id=%1$d;", obj->id);
         amodel->issue(SQL);
         printMessage("%s deleted from database", obj->name.c_str());
         loadWindow(window, timestep);
         amodel->issue("vacuum");
         for (unsigned int i=0; i<amodel->objects.size(); i++)
         {
            if (!amodel->objects[i]) continue;
            if (obj == amodel->objects[i])
            {
               amodel->objects[i] = NULL; //Don't erase as objects referenced by id
               break;
            }
         }
         for (unsigned int i=0; i<awin->objects.size(); i++)
         {
            if (!awin->objects[i]) continue;
            if (obj == awin->objects[i])
            {
               awin->objects.erase(awin->objects.begin()+i);
               break;
            }
         }
         for (unsigned int i=0; i<aview->objects.size(); i++)
         {
            if (!aview->objects[i]) continue;
            if (obj == aview->objects[i])
            {
               aview->objects.erase(aview->objects.begin()+i);
               break;
            }
         }
      }
   }
   else if (parsed.exists("load"))
   {
      //Load drawing object by name/ID match 
      std::string what = parsed["load"];
      for (int c=0; c<10; c++) //Allow multiple id/name specs on line
      {
         int id = parsed.Int("load", 0, c);
         DrawingObject* obj = findObject(what, id);
         if (obj)
         {
            loadGeometry(obj->id);
            //if (!loadWindow(window, timestep)) return false;
            //Update the views
            resetViews(false);
            redrawObjects();
            //aview->init(true);
            //resetViews(true);
            //Delete the cache as object list changed
            amodel->deleteCache();
         }
      }
   }
   else if (parsed.exists("script"))
   {
      std::string scriptfile = parsed["script"];
      std::ifstream file(scriptfile.c_str(), std::ios::in);
      if (file.is_open())
      {
         printMessage("Running script: %s", scriptfile.c_str());
         std::string line;
         while(std::getline(file, line))
            parseCommands(line);
         file.close();
         aview->inertia(false); //Clear inertia
      }
      else
         printMessage("Unable to open file: %s", scriptfile.c_str());
   }
   //TODO: NOT YET DOCUMENTED
   else if (parsed.exists("inertia"))
   {
      aview->use_inertia = !aview->use_inertia;
      printMessage("Inertia is %s", aview->use_inertia ? "ON":"OFF");
   }
   else if (parsed.exists("sort"))
   {
      //Always disables sort on rotate mode
      sort_on_rotate = false;
      aview->sort = true;
      viewer->notIdle(1500); //Start idle redisplay timer
      printMessage("Sorting geometry (auto-sort has been disabled)");
   }
   else if (parsed.exists("idle"))
   {
      if (!sort_on_rotate && aview->rotated)
         aview->sort = true;
      return true; //Skip record
   }
   //Special commands for passing keyboard/mouse actions directly (web server mode)
   else if (parsed.exists("mouse"))
   {
      std::string data = parsed["mouse"];

      std::replace(data.begin(), data.end(), ',', '\n');
      std::istringstream iss(data);
      PropertyParser props = PropertyParser(iss, '=');

      int x = props.Int("x");
      int y = props.Int("y");
      int button = props.Int("button");
      int spin = props.Int("spin");
      std::string action = props["mouse"];
      std::string modifiers = props["modifiers"];

      //Save shift states
      viewer->keyState.shift = modifiers.find('S')+1;
      viewer->keyState.ctrl = modifiers.find('C')+1;
      viewer->keyState.alt = modifiers.find('A')+1;

      //printf("%s button %d x %d y %d\n", action.c_str(), button, x, y);

      //viewer->button = (MouseButton)button; //if (viewer->keyState.ctrl && viewer->button == LeftButton)
      // XOR state of three mouse buttons to the mouseState variable  
      MouseButton btn = (MouseButton)button;

      if (action.compare("down") == 0)
      {
         if (btn <= RightButton) viewer->mouseState ^= (int)pow(2.0f, btn);
         redisplay = mousePress((MouseButton)button, true, x, y);
      }
      if (action.compare("up") == 0)
      {
         viewer->mouseState = 0;
         redisplay = mousePress((MouseButton)button, false, x, y);
      }
      if (action.compare("move") == 0)
      {
         redisplay = mouseMove(x, y);
      }
      if (action.compare("scroll") == 0)
      {
         redisplay = mouseScroll(spin);
      }
   }
   else if (parsed.exists("key"))
   {
      std::string data = parsed["key"];

      std::replace(data.begin(), data.end(), ',', '\n');
      std::istringstream iss(data);
      PropertyParser props = PropertyParser(iss, '=');

      int x = props.Int("x");
      int y = props.Int("y");
      unsigned char key = props.Int("key");
      std::string modifiers = props["modifiers"];

      //Save shift states
      viewer->keyState.shift = modifiers.find('S')+1;
      viewer->keyState.ctrl = modifiers.find('C')+1;
      viewer->keyState.alt = modifiers.find('A')+1;

      redisplay = keyPress(key, x, y);
   }
   else
   {
      //If value parses as integer and contains nothing else 
      //interpret as timestep jump
      ival = atoi(cmd.c_str());
      std::ostringstream oss;
      oss << ival;
      if (oss.str() == cmd && setTimeStep(ival) >= 0)
      {
         printMessage("Go to timestep %d", timestep);
         resetViews(); //Update the viewports
      }
      else
         return false;  //Invalid
   }

   //Always redraw when using multiple viewports in window (urgh sooner this is gone the better)
   if (awin->views.size() > 1 && viewPorts)
     redrawViewports();
   last_cmd = cmd;
   record(false, cmd);
   return redisplay;
}

std::string GLuciferViewer::helpCommand(std::string cmd)
{
   //Verbose command help
   std::string help = "~~~~~~~~~~~~~~~~~~~~~~~~\n" + cmd + "\n~~~~~~~~~~~~~~~~~~~~~~~~\n";
   if (cmd == "help")
   {
      help += "Command help:\n\nUse:\nhelp * [ENTER]\nwhere * is a command, for detailed help\n"
                  "\nMiscellanious commands:\n\n"
                  "quit, repeat, history, clearhistory, pause, list, timestep, jump, model, reload\n"
                  "\nView/camera commands:\n\n"
                  "rotate, rotatex, rotatey, rotatez, rotation, translate, translatex, translatey, translatez\n"
                  "focus, aperture, focallength, eyeseparation, nearclip, farclip, zerocam, reset, camera\n"
                  "resize, fullscreen, fit, autozoom, stereo, coordsystem, sort, rotation, translation\n"
                  "\nOutput commands:\n\n"
                  "image, images, outwidth, movie, play, dump, json\n"
                  "\nObject commands:\n\n"
                  "hide, show, delete, load\n"
                  "\nDisplay commands:\n\n"
                  "background, alpha, opacity, axis, cullface, wireframe, trianglestrips, scaling, rulers, log\n"
                  "antialias, localise, lockscale, lighting, colourmap, pointtype, vectorquality, tracerflat\n"
                  "tracerscale, tracersteps, pointsample, border, title, scale\n";
   }
   else if (cmd == "rotation")
   {
      help += "Set model rotation in 3d coords about given axis vector (replaces any previous rotation)\n\n"
                  "Usage: rotation x y z degrees\n\n"
                  "x (number) : axis of rotation x component\n"
                  "y (number) : axis of rotation y component\n"
                  "z (number) : axis of rotation z component\n"
                  "degrees (number) : degrees of rotation\n";
   }
   else if (cmd == "translation")
   {
      help += "Set model translation in 3d coords (replaces any previous translation)\n\n"
                  "Usage: translation x y z\n\n"
                  "x (number) : x axis shift\n"
                  "y (number) : y axis shift\n"
                  "z (number) : z axis shift\n";
   }
   else if (cmd == "rotate")
   {
      help += "Rotate model\n\n"
                  "Usage: rotate axis degrees\n\n"
                  "axis (x/y/z) : axis of rotation\n"
                  "degrees (number) : degrees of rotation\n"
                  "\nUsage: rotate x y z\n\n"
                  "x (number) : x axis degrees of rotation\n"
                  "y (number) : y axis degrees of rotation\n"
                  "z (number) : z axis degrees of rotation\n";
   }
   else if (cmd == "rotatex")
   {
      help += "Rotate model about x-axis\n\n"
                  "Usage: rotatex degrees\n\n"
                  "degrees (number) : degrees of rotation\n";
   }
   else if (cmd == "rotatey")
   {
      help += "Rotate model about y-axis\n\n"
                  "Usage: rotatey degrees\n\n"
                  "degrees (number) : degrees of rotation\n";
   }
   else if (cmd == "rotatez")
   {
      help += "Rotate model about z-axis\n\n"
                  "Usage: rotatez degrees\n\n"
                  "degrees (number) : degrees of rotation\n";
   }
   else if (cmd == "zoom")
   {
      help += "Translate model in Z direction (zoom)\n\n"
                  "Usage: zoom units\n\n"
                  "units (number) : distance to zoom, units are based on model size\n"
                  "                 1 unit is approx the model bounding box size\n"
                  "                 negative to zoom out, positive in\n";
   }
   else if (cmd == "translatex")
   {
      help += "Translate model in X direction\n\n"
                  "Usage: translationx shift\n\n"
                  "shift (number) : x axis shift\n";
   }
   else if (cmd == "translatey")
   {
      help += "Translate model in Y direction\n\n"
                  "Usage: translationy shift\n\n"
                  "shift (number) : y axis shift\n";
   }
   else if (cmd == "translatez")
   {
      help += "Translate model in Z direction\n\n"
                  "Usage: translationz shift\n\n"
                  "shift (number) : z axis shift\n";
   }
   else if (cmd == "translate")
   {
      help += "Translate model in 3 dimensions\n\n"
                  "Usage: translate dir shift\n\n"
                  "dir (x/y/z) : direction to translate\n"
                  "shift (number) : amount of translation\n"
                  "\nUsage: translation x y z\n\n"
                  "x (number) : x axis shift\n"
                  "y (number) : y axis shift\n"
                  "z (number) : z axis shift\n";
   }
   else if (cmd == "focus")
   {
      help += "Set model focal point in 3d coords\n\n"
                  "Usage: focus x y z\n\n"
                  "x (number) : x coord\n"
                  "y (number) : y coord\n"
                  "z (number) : z coord\n";
   }
   else if (cmd == "aperture")
   {
      help += "Set camera aperture (field-of-view)\n\n"
                  "Usage: aperture degrees\n\n"
                  "degrees (number) : degrees of viewing range\n";
   }
   else if (cmd == "focallength")
   {
      help += "Set camera focal length (for stereo viewing)\n\n"
                  "Usage: focallength len\n\n"
                  "len (number) : distance from camera to focal point\n";
   }
   else if (cmd == "eyeseparation")
   {
      help += "Set camera stereo eye separation\n\n"
                  "Usage: eyeseparation len\n\n"
                  "len (number) : distance between left and right eye camera viewpoints\n";
   }
   else if (cmd == "nearclip")
   {
      help += "Set near clip plane, before which geometry is not displayed\n\n"
                  "Usage: nearclip dist\n\n"
                  "dist (number) : distance from camera to near clip plane\n";
   }
   else if (cmd == "farclip")
   {
      help += "Set far clip plane, beyond which geometry is not displayed\n\n"
                  "Usage: farrclip dist\n\n"
                  "dist (number) : distance from camera to far clip plane\n";
   }
   else if (cmd == "timestep")  //Absolute
   {
      help += "Set timestep to view\n\n"
                  "Usage: timestep up/down/value\n\n"
                  "value (integer) : the timestep to view\n"
                  "up : switch to previous timestep if available\n"
                  "down : switch to next timestep if available\n";
   }
   else if (cmd == "jump")      //Relative
   {
      help += "Jump from current timestep forward/backwards\n\n"
                  "Usage: jump value\n\n"
                  "value (integer) : how many timesteps to jump (negative for backwards)\n";
   }
   else if (cmd == "model")      //Model switch
   {
      help += "Set model to view (when multiple models loaded)\n\n"
                  "Usage: model up/down/value\n\n"
                  "value (integer) : the model index to view [1,n]\n"
                  "up : switch to previous model if available\n"
                  "down : switch to next model if available\n";
   }
   else if (cmd == "hide" || cmd == "show")
   {
      help += "Hide/show objects/geometry types\n\n"
                  "Usage: hide/show object_id/object_name\n\n"
                  "object_id (integer) : the index of the object to hide/show (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to hide/show (see: \"list objects\")\n"
                  "\nUsage: hide/show geometry_type id\n\n"
                  "geometry_type : points/labels/vectors/tracers/triangles/quads/shapes/lines\n"
                  "id (integer, optional) : id of geometry to hide/show\n"
                  "eg: 'hide points' will hide all point data\n";
   }
   else if (cmd == "dump")
   {
      help += "Dump object vertices & values to CSV\n\n"
                  "Usage: dump object_id/object_name\n\n"
                  "object_id (integer) : the index of the object to export (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to export (see: \"list objects\")\n";
   }
   else if (cmd == "tracersteps")
   {
      help += "Limit tracer time-steps to plot\n\n"
                  "Usage: tracersteps limit\n\n"
                  "limit (integer) : maximum steps back in time to plot of tracer trajectory\n";
   }
   else if (cmd == "alpha")
   {
      help += "Set global transparency value\n\n"
                  "Usage: alpha value\n\n"
                  "value (integer > 1) : sets alpha as integer in range [1,255] where 255 is fully opaque\n"
                  "value (number [0,1]) : sets alpha as real number in range [0,1] where 1.0 is fully opaque\n";
   }
   else if (cmd == "opacity")
   {
      help += "Set object transparency value\n\n"
                  "Usage: opacity object_id/object_name value\n\n"
                  "object_id (integer) : the index of the object (see: \"list objects\")\n"
                  "object_name (string) : the name of the object (see: \"list objects\")\n"
                  "value (integer > 1) : sets alpha as integer in range [1,255] where 255 is fully opaque\n"
                  "value (number [0,1]) : sets alpha as real number in range [0,1] where 1.0 is fully opaque\n";
   }
   else if (cmd == "movie")
   {
      help += "Encode video of model running from current timestep to specified timestep\n"
                  "(Requires libavcodec)\n\n"
                  "Usage: movie endstep\n\n"
                  "endstep (integer) : last frame timestep\n";
   }
   else if (cmd == "play")
   {
      help += "Display model timesteps in sequence from current timestep to specified timestep\n\n"
                  "Usage: play endstep\n\n"
                  "endstep (integer) : last frame timestep\n"
                  "If endstep omitted, will loop back to start until 'stop' command entered\n";
   }
   else if (cmd == "next")
   {
      help += "Go to next timestep in sequence\n";
   }
   else if (cmd == "stop")
   {
      help += "Stop the looping 'play' command\n";
   }
   else if (cmd == "images")
   {
      help += "Write images in sequence from current timestep to specified timestep\n\n"
                  "Usage: images endstep\n\n"
                  "endstep (integer) : last frame timestep\n";
   }
   else if (cmd == "repeat")
   {
      help += "Repeat commands from history\n\n"
                  "Usage: repeat count (animate)\n\n"
                  "count (integer) : repeat the last entered command count times\n"
                  "animate (optional) : update display after each command\n"
                  "\nUsage: repeat history count (animate)\n\n"
                  "count (integer) : repeat every command in history buffer count times\n"
                  "animate (optional) : update display after each command\n";
   }
   else if (cmd == "quit")
   {
      help += "Quit the program\n";
   }
   else if (cmd == "axis")
   {
      help += "Display/hide the axis legend\n\n"
                  "Usage: axis (on/off)\n\n"
                  "on/off (optional) : show/hide the axis, if omitted switches state\n";
   }
   else if (cmd == "cullface")
   {
      help += "Switch surface face culling (global setting)\n\n"
                  "Will not effect objects with cullface set explicitly\n";
   }
   else if (cmd == "wireframe")
   {
      help += "Switch surface wireframe (global setting)\n\n"
                  "Will not effect objects with wireframe set explicitly\n";
   }
   else if (cmd == "trianglestrips")
   {
      help += "Draw quad surfaces with triangle strips\n"
                  "(Default is on, provides better detail for terrain surfaces)\n";
   }
   else if (cmd == "fullscreen")
   {
      help += "Switch viewer to full-screen mode and back to windowed mode\n";
   }
   else if (cmd == "redraw")
   {
      help += "Redraw all object data, required after changing some parameters but may be slow\n";
   }
   else if (cmd == "scaling")
   {
      help += "Disable/enable scaling, default is on, disable to view model un-scaled\n";
   }
   else if (cmd == "fit")
   {
      help += "Adjust camera view to fit the model in window\n";
   }
   else if (cmd == "autozoom")
   {
      help += "Automatically adjust camera view to always fit the model in window (enable/disable)\n";
   }
   else if (cmd == "stereo")
   {
      help += "Enable/disable stereo projection\n"
                  "If no stereo hardware found will use red/cyan anaglyph mode\n";
   }
   else if (cmd == "coordsystem")
   {
      help += "Switch between Right-handed and Lef-handed coordinate system\n";
   }
   else if (cmd == "rulers")
   {
      help += "Enable/disable axis rulers (unfinished)\n";
   }
   else if (cmd == "title")
   {
      help += "Enable/disable title heading\n";
   }
   else if (cmd == "log")
   {
      help += "Over-ride colourmap settings to use log scales\n"
                  "Cycles between ON/OFF/DEFAULT\n"
                  "(default uses original settings for each colourmap)\n";
   }
   else if (cmd == "antialias")
   {
      help += "Enable/disable multi-sample anti-aliasing (if supported by OpenGL drivers)\n";
   }
   else if (cmd == "localise")
   {
      help += "Experimental: adjust colourmaps on each object to fit actual value range\n";
   }
   else if (cmd == "json")
   {
      help += "Dump object data to JSON file for viewing in WebGL viewer\n\n"
                  "Usage: json object_id/object_name\n\n"
                  "object_id (integer) : the index of the object to export (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to export (see: \"list objects\")\n"
                  "If object ommitted all will be exported\n";
   }
   else if (cmd == "lockscale")
   {
      help += "Enable/disable colourmap lock\n"
                  "When locked, dynamic range colourmaps will keep current values\n"
                  "when switching between timesteps instead of being re-calibrated\n";
   }
   else if (cmd == "lighting")
   {
      help += "Enable/disable lighting for objects that support this setting\n";
   }
   else if (cmd == "list")
   {
      help += "List available data\n\n"
                  "Usage: list objects/colourmaps/elements\n\n"
                  "objects : enable object list (stays on screen until disabled)\n"
                  "          (dimmed objects are hidden or not in selected viewport)\n"
                  "colourmaps : show colourmap list (onlt temporarily shown)\n"
                  "elements : show geometry elements by id in terminal\n";
   }
   else if (cmd == "reset")
   {
      help += "Reset the camera to the default model view\n";
   }
   else if (cmd == "reload")
   {
      help += "Reload all data of current model/timestep from database\n";
   }
   else if (cmd == "zerocam")
   {
      help += "Set the camera postiion to the origin (for scripting, not generally advisable)\n";
   }
   else if (cmd == "colourmap")
   {
      help += "Set colourmap on object\n\n"
                  "Usage: colourmap object_id/object_name colourmap_id/colourmap_name\n\n"
                  "object_id (integer) : the index of the object to set (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to set (see: \"list objects\")\n"
                  "colourmap_id (integer) : the index of the colourmap to apply (see: \"list colourmaps\")\n"
                  "colourmap_name (string) : the name of the colourmap to apply (see: \"list colourmaps\")\n";
   }
   else if (cmd == "pointtype")
   {
      help += "Set point-rendering type on object\n\n"
                  "Usage: pointtype all/object_id/object_name type/up/down\n\n"
                  "all : use 'all' to set the global default point type\n"
                  "object_id (integer) : the index of the object to set (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to set (see: \"list objects\")\n"
                  "type (integer) : Point type [0,3] to apply (gaussian/flat/sphere/highlight sphere)\n"
                  "up/down : use 'up' or 'down' to switch to the previous/next type in list\n";
   }
   else if (cmd == "vectorquality")
   {
      help += "Set vector rendering quality\n\n"
                  "Usage: vectorquality value\n\n"
                  "value (integer) : 0=flat lines, [1-10] increasing quality 3d arrows (default 2)\n";
   }
   else if (cmd == "tracerflat")
   {
      help += "Enable/disable flat tracer rendering (tracers drawn as simple lines)\n";
   }
   else if (cmd == "tracerscale")
   {
      help += "Enable/disable scaled tracers (earlier timesteps = narrower lines)\n";
   }
   else if (cmd == "pointsample")
   {
      help += "Set point sub-sampling value\n\n"
                  "Usage: pointsample value/up/down\n\n"
                  "value (integer) : 1=no sub-sampling=50%% sampled etc..\n"
                  "up/down : use 'up' or 'down' to sample more (up) or less (down) points\n";
   }
   else if (cmd == "image")
   {
      help += "Save an image of the current view\n";
   }
   else if (cmd == "outwidth")
   {
      help += "Set output image width (height calculated to match window aspect)\n\n"
                  "Usage: outwidth value\n\n"
                  "value (integer) : width in pixels for output images\n";
   }
   else if (cmd == "background")
   {
      help += "Set window background colour\n\n"
                  "Usage: background value/white/black/grey/invert\n\n"
                  "value (number [0,1]) : sets to greyscale value with luminosity in range [0,1] where 1.0 is white\n";
                  "white/black/grey : sets background to specified value\n"
                  "invert (or omitted value) : inverts current background colour\n";
   }
   else if (cmd == "border")
   {
      help += "Set model border frame\n\n"
                  "Usage: border on/off/filled\n\n"
                  "on : line border around model bounding-box\n"
                  "off : no border\n"
                  "filled : filled background bounding box"
                  "(no value) : switch between possible values\n";
   }
   else if (cmd == "camera")
   {
      help += "Output camera view as XML for use in model scripts\n";
   }
   else if (cmd == "sort")
   {
      help += "Sort geometry on demand (with idle timer)\n";
   }
   else if (cmd == "scale")
   {
      help += "Scale applicable objects up/down in size (points/vectors/shapes)\n\n"
                  "Usage: scale axis value\n\n"
                  "axis : x/y/z\n"
                  "value (number) : scaling value applied to all geometry on specified axis\n\n"
                  "Usage: scale geometry_type value/up/down\n\n"
                  "geometry_type : points/vectors/tracers/shapes\n"
                  "value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling\n"
                  "\nUsage: scale object_id/object_name value/up/down\n\n"
                  "object_id (integer) : the index of the object to set (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to set (see: \"list objects\")\n"
                  "value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling\n";
   }
   else if (cmd == "history")
   {
      help += "Write command history to output (eg: terminal)"
                  "Usage: history [filename]\n\n"
                  "filename (string) : optional file to write data to\n";
   }
   else if (cmd == "clearhistory")
   {
      help += "Clear command history\n";
   }
   else if (cmd == "resize")
   {
      help += "Resize view window\n\n"
                  "Usage: resize width height\n\n"
                  "width (integer) : width in pixels\n"
                  "height (integer) : height in pixels\n";
   }
   else if (cmd == "pause")
   {
      help += "Pause execution (for scripting)\n\n"
                  "Usage: pause msecs\n\n"
                  "msecs (integer) : how long to pause for in milliseconds\n";
   }
   else if (cmd == "delete")
   {
      help += "Delete objects from database\n"
                  "WARNING: This is irreversable! Backup your database before using!\n\n"
                  "Usage: delete object_id/object_name\n\n"
                  "object_id (integer) : the index of the object to delete (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to delete (see: \"list objects\")\n";
   }
   else if (cmd == "load")
   {
      help += "Load objects from database\n"
                  "Used when running with deferred loading (-N command line switch)\n\n"
                  "Usage: load object_id/object_name\n\n"
                  "object_id (integer) : the index of the object to load (see: \"list objects\")\n"
                  "object_name (string) : the name of the object to load (see: \"list objects\")\n";
   }
   else if (cmd == "script")
   {
      help += "Run script file\n"
                  "Load a saved script of viewer commands from a file\n\n"
                  "Usage: script filename\n\n"
                  "filename (string) : path and name of the script file to load\n";
   }
   else
      return std::string("");
   return help;
}

