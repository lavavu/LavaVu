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

#include "LavaVu.h"
#include "ColourMap.h"

#define HELP_INTERACTION "\
\n## User Interface commands\n\n\
Hold the Left mouse button and drag to Rotate about the X & Y axes  \n\
Hold the Right mouse button and drag to Pan (left/right/up/down)  \n\
Hold the Middle mouse button and drag to Rotate about the Z axis  \n\
Hold [shift] and use the scroll wheel to move the clip plane in and out.  \n\
\n```\n\
[F1]         Print help\n\
[UP]         Previous command in history if available\n\
[DOWN]       Next command in history if available\n\
[TAB]        Nearest match from history to partially typed command\n\
[ALT+UP]     Load previous model in list at current time-step if data available\n\
[ALT+DOWN]   Load next model in list at current time-step if data available\n\
[LEFT]       Previous time-step\n\
[RIGHT]      Next time-step\n\
[Page Up]    Select the previous viewport if available\n\
[Page Down]  Select the next viewport if available\n\
[`]          Full screen ON/OFF\n\
\n\
\nHold [ALT] plus:\n\
[*]          Auto zoom to fit ON/OFF\n\
[/]          Stereo ON/OFF\n\
[\\]          Switch coordinate system Right-handed/Left-handed\n\
[|]          Switch rulers ON/OFF\n\
[,]          Switch to next particle rendering texture\n\
[+/=]        More particles (reduce sub-sampling)\n\
[-]          Less particles (increase sub-sampling)\n\
\n\
[a]          Hide/show axis\n\
[b]          Background colour switch WHITE/BLACK\n\
[B]          Background colour grey\n\
[c]          Camera info: output to console current camera parameters\n\
[f]          Frame border ON/OFF\n\
[i]          Take screen-shot and save as png/jpeg image file\n\
[j]          Localise colour scales, minimum and maximum calibrated to each object drawn\n\
[k]          Lock colour scale calibrations to current values ON/OFF\n\
[o]          Object legend, print list of object names with id numbers.\n\
[r]          Reset camera viewpoint\n\
[q]          Quit program\n\
[u]          Backface Culling ON/OFF\n\
[w]          Wireframe ON/OFF\n\
\n\
[v]          Increase vector size scaling\n\
[V]          Reduce vector size scaling\n\
[t]          Increase tracer size scaling\n\
[T]          Reduce tracer size scaling\n\
[p]          Increase particle size scaling\n\
[P]          Reduce particle size scaling\n\
[s]          Increase shape size scaling\n\
[S]          Reduce shape size scaling\n\
\n\
[p] [ENTER]  hide/show all particle swarms\n\
[v] [ENTER]  hide/show all vector arrow fields\n\
[t] [ENTER]  hide/show all tracer trajectories\n\
[q] [ENTER]  hide/show all quad surfaces (scalar fields, cross sections etc.)\n\
[u] [ENTER]  hide/show all triangle surfaces (isosurfaces)\n\
[s] [ENTER]  hide/show all shapes\n\
[l] [ENTER]  hide/show all lines\n\
[ESC][ENTER] quit program\n\
\n\
Verbose commands:\n\
help commands [ENTER] -- list of all verbose commands\n\
help * [ENTER] where * is a command -- detailed help for a command\n\
\n```\n\
"

/////////////////////////////////////////////////////////////////////////////////
// Event handling
/////////////////////////////////////////////////////////////////////////////////
bool LavaVu::mousePress(MouseButton btn, bool down, int x, int y)
{
  //Only process on mouse release
  static bool translated = false;
  bool redraw = false;
  int scroll = 0;
  viewer->idleReset(); //Reset idle timer
  if (down)
  {
    translated = false;
    //if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
    viewer->button = btn;
    viewer->last_x = x;
    viewer->last_y = y;
    //Flag rotation in progress
    if (btn == LeftButton && !viewer->keyState.alt && !viewer->keyState.shift)
        aview->rotating = true;
  }
  else
  {
    aview->rotating = false;

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
    if (translated) history.push_back(aview->translateString());
    if (aview->rotated) history.push_back(aview->rotateString());

    viewer->button = NoButton;
  }
  return redraw;
}

bool LavaVu::mouseMove(int x, int y)
{
  float adjust;
  int dx = x - viewer->last_x;
  int dy = y - viewer->last_y;
  viewer->last_x = x;
  viewer->last_y = y;
  viewer->idleReset(); //Reset idle timer

  //For mice with no right button, ctrl+left
  if (viewer->keyState.ctrl && viewer->button == LeftButton)
    viewer->button = RightButton;

  switch (viewer->button)
  {
  case LeftButton:
    if (viewer->keyState.alt || viewer->keyState.shift)
      //Mac glut scroll-wheel alternative
      history.push_back(aview->zoom(-dy * 0.01));
    else
    {
      // left = rotate
      aview->rotate(dy / 5.0f, dx / 5.0, 0.0f);
    }
    break;
  case RightButton:
    if (viewer->keyState.alt || viewer->keyState.shift)
      //Mac glut scroll-wheel alternative
      history.push_back(aview->zoomClip(-dy * 0.001));
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

bool LavaVu::mouseScroll(float scroll)
{
  //Only process on mouse release
  //Process wheel scrolling
  //CTRL+ALT+SHIFT = eye-separation adjust
  if (viewer->keyState.alt && viewer->keyState.shift && viewer->keyState.ctrl)
    history.push_back(aview->adjustStereo(0, 0, scroll / 500.0));
  //ALT+SHIFT = focal-length adjust
  if (viewer->keyState.alt && viewer->keyState.shift)
    history.push_back(aview->adjustStereo(0, scroll * aview->model_size / 100.0, 0));
  //CTRL+SHIFT - fine adjust near clip-plane
  if (viewer->keyState.shift && viewer->keyState.ctrl)
    history.push_back(aview->zoomClip(scroll * 0.001));
  //SHIFT = move near clip-plane
  else if (viewer->keyState.shift)
    history.push_back(aview->zoomClip(scroll * 0.01));
  //ALT = adjust field of view (aperture)
  else if (viewer->keyState.alt)
    history.push_back(aview->adjustStereo(scroll, 0, 0));
  else if (viewer->keyState.ctrl)
    //Fast zoom
    history.push_back(aview->zoom(scroll * 0.1));
  //Default = slow zoom
  else
    history.push_back(aview->zoom(scroll * 0.01));

  return true;
}

bool LavaVu::keyPress(unsigned char key, int x, int y)
{
  viewer->idleReset(); //Reset idle timer
  //if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
  return parseChar(key);
}

bool LavaVu::parseChar(unsigned char key)
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
    case KEY_UP:
      return parseCommands("model up");
    case KEY_DOWN:
      return parseCommands("model down");
    case '*':
      return parseCommands("autozoom");
    case '/':
      return parseCommands("stereo");
    case '\\':
      return parseCommands("coordsystem");
    case ',':
      return parseCommands("pointtype all");
    case '-':
      return parseCommands("pointsample down");
    case '+':
      return parseCommands("pointsample up");
    case '=':
      return parseCommands("pointsample up");
    case '|':
      return parseCommands("rulers");
    case 'a':
      return parseCommands("axis");
    case 'b':
      return parseCommands("background invert");
    case 'c':
      return parseCommands("camera");
    case 'e':
      return parseCommands("list elements");
    case 'f':
      return parseCommands("border");
    case 'h':
      return parseCommands("history");
    case 'i':
      return parseCommands("image");
    case 'j':
      return parseCommands("valuerange");
    case 'k':
      return parseCommands("lockscale");
    case 'u':
      return parseCommands("toggle cullface");
    case 'w':
      parseCommands("toggle wireframe");
      amodel->redraw(true);
      return true;
    case 'o':
      return parseCommands("list objects");
    case 'q':
      return parseCommands("quit");
    case 'r':
      return parseCommands("reset");
    case 'p':
      return parseCommands("scale points up");
    case 's':
      return parseCommands("scale shapes up");
    case 't':
      return parseCommands("scale tracers up");
    case 'v':
      return parseCommands("scale vectors up");
    case 'l':
      return parseCommands("scale lines up");
    case 'R':
      return parseCommands("reload");
    case 'B':
      return parseCommands("background");
    case 'S':
      return parseCommands("scale shapes down");
    case 'V':
      return parseCommands("scale vectors down");
    case 'T':
      return parseCommands("scale tracers down");
    case 'P':
      return parseCommands("scale points down");
    case 'L':
      return parseCommands("scale lines down");
    case ' ':
      if (loop)
        return parseCommands("stop");
      else
        return parseCommands("play");
    default:
      return false;
    }
  }

  //Direct commands and verbose command entry
  switch (key)
  {
  case KEY_RIGHT:
    return parseCommands("timestep down");
  case KEY_LEFT:
    return parseCommands("timestep up");
  case KEY_ESC:
    //Don't quit immediately on ESC by request, if entry, then clear, 
    msg = true;
    if (entry.length() == 0)
      entry = "quit";
    else
      entry = "";
    break;
  case KEY_ENTER:
    if (entry.length() == 0)
      return parseCommands("repeat");
    if (entry.length() == 1)
    {
      char ck = entry.at(0);
      //Digit? (9 == ascii 57)
      if (ck > 47 && ck < 58)
      {
        response = parseCommands(entry);
        entry = "";
        return response;
      }
      switch (ck)
      {
      case 'p':   //Points
        if (Model::points->allhidden)
          response = parseCommands("show points");
        else
          response = parseCommands("hide points");
        break;
      case 'v':   //Vectors
        if (Model::vectors->allhidden)
          response = parseCommands("show vectors");
        else
          response = parseCommands("hide vectors");
        break;
      case 't':   //Tracers
        if (Model::tracers->allhidden)
          response = parseCommands("show tracers");
        else
          response = parseCommands("hide tracers");
        break;
      case 'u':   //TriSurfaces
        if (Model::triSurfaces->allhidden)
          response = parseCommands("show triangles");
        else
          response = parseCommands("hide triangles");
        break;
      case 'q':   //QuadSurfaces
        if (Model::quadSurfaces->allhidden)
          response = parseCommands("show quads");
        else
          response = parseCommands("hide quads");
        break;
      case 's':   //Shapes
        if (Model::shapes->allhidden)
          response = parseCommands("show shapes");
        else
          response = parseCommands("hide shapes");
        break;
      case 'l':   //Lines
        if (Model::lines->allhidden)
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
    {
      //Add to linehistory if not a duplicate of previous entry
      int lend = linehistory.size()-1;
      if (lend < 0 || entry != linehistory[lend])
        linehistory.push_back(entry);

      //Execute
      response = parseCommands(entry);
      //Clear
      entry = "";
    }
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
    if (historyline >= (int)linehistory.size())
    {
      historyline = -1;
      entry = "";
    }
    else
      entry = linehistory[historyline];
    msg = true;
    break;
  case KEY_PAGEUP:
    //Previous figure/view
    if (amodel->views.size() < 2)
      return parseCommands("figure up");
    else
      return parseCommands("view up");
    break;
  case KEY_PAGEDOWN:
    //Next figure/view
    if (amodel->views.size() < 2)
      return parseCommands("figure down");
    else
      return parseCommands("view down");
    break;
  case KEY_HOME:
    break;
  case KEY_END:
    break;
  case '`':
    return parseCommands("fullscreen");
  case KEY_F1:
    return parseCommands("help");
  case KEY_F2:
    return parseCommands("antialias");
  case KEY_TAB:
    //Tab-completion from history
    for (int l=history.size()-1; l>=0; l--)
    {
      std::cout << entry << " ==? " << history[l].substr(0, entry.length()) << std::endl;
      if (entry == history[l].substr(0, entry.length()))
      {
        entry = history[l];
        msg = true;
        break;
      }
    }
    break;
  default:
    //Only add printable characters
    if (key > 31 && key < 127)
      entry += key;
    msg = true;
    break;
  }

  if (entry.length() > 0 && msg)
  {
    printMessage(": %s", entry.c_str());
    response = false;
    viewer->postdisplay = true;
  }

  return response;
}

Geometry* LavaVu::getGeometryType(std::string what)
{
  if (what == "points")
    return Model::points;
  if (what == "labels")
    return Model::labels;
  if (what == "vectors")
    return Model::vectors;
  if (what == "tracers")
    return Model::tracers;
  if (what == "triangles")
    return Model::triSurfaces;
  if (what == "quads")
    return Model::quadSurfaces;
  if (what == "shapes")
    return Model::shapes;
  if (what == "lines")
    return Model::lines;
  if (what == "volumes")
    return Model::volumes;
  return NULL;
}

DrawingObject* LavaVu::lookupObject(PropertyParser& parsed, const std::string& key, int idx)
{
  //Try index(id) first
  int id = parsed.Int(key, -1, idx);
  if (id > 0 && id <= (int)amodel->objects.size()) return amodel->objects[id-1];

  //Otherwise lookup by name
  std::string what = parsed.get(key, idx);
  return lookupObject(what);
}

DrawingObject* LavaVu::lookupObject(std::string& name)
{
  //Lookup by name only
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);
  for (unsigned int i=0; i<amodel->objects.size(); i++)
  {
    std::string namekey = amodel->objects[i]->name();
    std::transform(namekey.begin(), namekey.end(), namekey.begin(), ::tolower);
    if (namekey == name)
      //std::cerr << "Found by " << (namekey == what ? " NAME : " : " ID: ") << what << " -- " << id << std::endl;
      return amodel->objects[i];
  }
  //std::cerr << "Not found, returning " << (nodefault ? "NULL" : "DEFAULT : ") << aobject << std::endl;
  return NULL;
}

std::vector<DrawingObject*> LavaVu::lookupObjects(PropertyParser& parsed, const std::string& key, int start)
{
  std::vector<DrawingObject*> list;
  for (int c=0; c<20; c++) //Allow multiple id/name specs on line (up to 20)
  {
    DrawingObject* obj = lookupObject(parsed, key, c+start);
    if (obj)
      list.push_back(obj);
    else
      break;
  }
  return list;
}

int LavaVu::lookupColourMap(PropertyParser& parsed, const std::string& key, int idx)
{
  //Try index(id) first
  int id = parsed.Int(key, -1, idx);
  if (id > 0 && id <= (int)amodel->colourMaps.size()) return id-1;

  //Find by name match in all colour maps
  std::string what = parsed.get(key, idx);
  std::transform(what.begin(), what.end(), what.begin(), ::tolower);
  for (unsigned int i=0; i<amodel->colourMaps.size(); i++)
  {
    if (!amodel->colourMaps[i]) continue;
    std::string namekey = amodel->colourMaps[i]->name;
    std::transform(namekey.begin(), namekey.end(), namekey.begin(), ::tolower);
    if (namekey == what)
      return i;
  }
  return -1;
}

float LavaVu::parseCoord(const json& val)
{
  if (val.is_string())
  {
    std::string vstr = val;
    return parseCoord(vstr);
  }
  return (float)val;
}

float LavaVu::parseCoord(const std::string& str)
{
  //Return a coord value macro replacement for bounds if valid string
  //minX/Y/Z max/X/Y/Z
  if (str == "minX")
    return aview->min[0];
  if (str == "maxX")
    return aview->max[0];
  if (str == "minY")
    return aview->min[1];
  if (str == "maxY")
    return aview->max[1];
  if (str == "minZ")
    return aview->min[2];
  if (str == "maxZ")
    return aview->max[2];
  std::stringstream ss(str);
  float val;
  ss >> val;
  return val;
}

bool LavaVu::parseCommands(std::string cmd)
{
  //Support multi-line command input by splitting on newlines
  //and parsing separately
  if (cmd.length() == 0) return false;
  bool redisplay = false;

  //Json 
  if (cmd.at(0) == '{')
  {
    amodel->jsonRead(cmd);
    return true;
  }

  std::string line;
  std::stringstream ss(cmd);
  while(std::getline(ss, line))
  {
    redisplay |= parseCommand(line);
  }

  return redisplay;
}

bool LavaVu::parseCommand(std::string cmd, bool gethelp)
{
  if (cmd.length() == 0) return false;
  bool redisplay = true;
  PropertyParser parsed = PropertyParser();
  static std::string last_cmd = "";
  static std::string multiline = "";

  //Skip comments or empty lines
  if (cmd.length() == 0 || cmd.at(0) == '#') return false;

  //Save in history
  history.push_back(cmd);

  //If the command contains only one double-quote, append until another received before parsing as a single string
  size_t n = std::count(cmd.begin(), cmd.end(), '"');
  size_t len = multiline.length();
  if (len > 0 || n == 1)
  {
    //Append
    multiline += cmd + "\n";
    //Finished appending?
    if (len > 0 && n == 1)
    {
      cmd = multiline;
      multiline = "";
    }
    else
      return false;
  }

  //Parse the line
  parsed.parseLine(cmd);

  //Verbose command processor
  float fval;
  int ival;

  //******************************************************************************
  //First check for settings commands that don't require a model to be loaded yet!
  if (parsed.exists("docs:scripting"))
  {
    helpCommand("docs:scripting");
    return false;
  }
  else if (parsed.exists("docs:interaction"))
  {
    std::cout << HELP_INTERACTION;
    return false;
  }
  else if (parsed.exists("file"))
  {
    if (gethelp)
    {
      help += "> Load database or model file  \n"
              "> **Usage:** file \"filename\"\n\n"
              "> filename (string) : the path to the file, quotes necessary if path contains spaces  \n";
      return false;
    }

    std::string what = parsed["file"];
    //Attempt to load external file
    loadFile(what);
  }
  else if (parsed.exists("script"))
  {
    if (gethelp)
    {
      help += "> Run script file  \n"
              "> Load a saved script of viewer commands from a file\n\n"
              "> **Usage:** script filename\n\n"
              "> filename (string) : path and name of the script file to load  \n";
      return false;
    }

    std::string scriptfile = parsed["script"];
    std::ifstream file(scriptfile.c_str(), std::ios::in);
    if (file.is_open())
    {
      printMessage("Running script: %s", scriptfile.c_str());
      std::string line;
      entry = "";
      while(std::getline(file, line))
        parseCommands(line);
      entry = "";
      file.close();
    }
    else
    {
      if (scriptfile != "init.script")
        printMessage("Unable to open file: %s", scriptfile.c_str());
    }
  }
  else if (parsed.exists("save"))
  {
    if (gethelp)
    {
      help += "> Export all settings as json state file that can be reloaded later\n\n"
              "> **Usage:** save [\"filename\"]\n\n"
              "> file (string) : name of file to import  \n"
              "> If filename omitted and database loaded, will save the state  \n"
              "> to the active figure in the database instead  \n";
      return false;
    }

    //Export json settings only (no object data)
    std::string what = parsed["save"];
    if (what.length() == 0 && amodel->db)
    {
      amodel->reopen(true);  //Open writable
      amodel->writeState();
    }
    else
      jsonWriteFile(what, 0, false, false);
  }
  else if (parsed.has(ival, "cache"))
  {
    if (gethelp)
    {
      help += "> Set the timestep cache size  \n"
              "> When cache enabled, timestep data will be loaded into memory for faster access  \n"
              "> (currently any size > 0 attempts to cache all steps)\n\n"
              "> **Usage:** cache size\n\n"
              "> size (integer) : number of timesteps, 0 to disable  \n";
      return false;
    }

    TimeStep::cachesize = ival;
    printMessage("Geometry cache set to %d timesteps", TimeStep::cachesize);
  }
  else if (parsed.exists("verbose"))
  {
    if (gethelp)
    {
      help += "> Switch verbose output mode on/off\n\n"
              "> **Usage:** verbose [off]  \n";
      return false;
    }

    std::string what = parsed["verbose"];
    verbose = what != "off";
    printMessage("Verbose output is %s", verbose ? "ON":"OFF");
    //Set info/error stream
    if (verbose)
      infostream = stderr;
    else
      infostream = NULL;
  }
  else if (parsed.exists("createvolume"))
  {
    if (gethelp)
    {
      help += "> Create a static volume object which will receive all following volumes as timesteps  \n";
      return false;
    }

    //Use this to load multiple volumes as timesteps into the same object
    volume = new DrawingObject("volume");
    printMessage("Created static volume object");
  }
  else if (parsed.has(fval, "alpha") || parsed.has(fval, "opacity"))
  {
    if (gethelp)
    {
      help += "> Set global transparency value\n\n"
              "> **Usage:** opacity/alpha value\n\n"
              "> value (integer > 1) : sets opacity as integer in range [1,255] where 255 is fully opaque  \n"
              "> value (number [0,1]) : sets opacity as real number in range [0,1] where 1.0 is fully opaque  \n";
      return false;
    }

    float opacity = Properties::global("opacity");
    if (opacity == 0.0) opacity = 1.0;
    if (fval > 1.0)
      opacity = fval / 255.0;
    else
      opacity = fval;
    Properties::globals["opacity"] = opacity;
    printMessage("Set global opacity to %.2f", opacity);
    if (amodel)
      amodel->redraw(true);
  }
  else if (parsed.exists("interactive"))
  {
    if (gethelp)
    {
      help += "> Switch to interactive mode, for use in scripts  \n";
      return false;
    }

    viewer->execute();
  }
  else if (parsed.exists("open"))
  {
    if (gethelp)
    {
      help += "> Open the viewer window if not already done, for use in scripts  \n";
      return false;
    }

    loadModelStep(0, 0, true);
    resetViews(); //Forces bounding box update
  }
  else if (parsed.exists("resize"))
  {
    if (gethelp)
    {
      help += "> Resize view window\n\n"
              "> **Usage:** resize width height\n\n"
              "> width (integer) : width in pixels  \n"
              "> height (integer) : height in pixels  \n";
      return false;
    }

    float w = 0, h = 0;
    if (parsed.has(w, "resize", 0) && parsed.has(h, "resize", 1))
    {
      aview->properties.data["resolution"] = json::array({w, h});
      viewset = 2; //Force check for resize and autozoom
    }
  }
  else if (parsed.exists("quit") || parsed.exists("exit"))
  {
    if (gethelp)
    {
      help += "> Quit the program  \n";
      return false;
    }

    viewer->quitProgram = true;
  }
  else if (parsed.exists("record"))
  {
    if (gethelp)
    {
      help += "> Encode video of all frames displayed at specified framerate  \n"
              "> (Requires libavcodec)\n\n"
              "> **Usage:** record (framerate)\n\n"
              "> framerate (integer): frames per second (default 30)  \n";
      return false;
    }

    //Default to 30 fps
    if (!parsed.has(ival, "record")) ival = 30;
    encodeVideo("", ival);
  }
  else if (parsed.exists("scan"))
  {
    if (gethelp)
    {
      help += "> Rescan the current directory for timestep database files  \n"
              "> based on currently loaded filename\n\n";
      return false;
    }

    amodel->loadTimeSteps(true);
  }
  //******************************************************************************
  //Following commands require a model!
  else if (!gethelp && (!amodel || !aview))
  {
    //Attempt to parse as property=value first
    if (parsePropertySet(cmd)) return true;
    if (verbose) std::cerr << "Model/View required to execute command: " << cmd << ", deferred" << std::endl;
    //Add to the queue to be processed once open
    OpenGLViewer::commands.push_back(cmd);
    return false;
  }
  else if (parsed.exists("rotation"))
  {
    if (gethelp)
    {
      help += "> Set model rotation in 3d coords about given axis vector (replaces any previous rotation)\n\n"
              "> **Usage:** rotation x y z degrees\n\n"
              "> x (number) : axis of rotation x component  \n"
              "> y (number) : axis of rotation y component  \n"
              "> z (number) : axis of rotation z component  \n"
              "> degrees (number) : degrees of rotation  \n";
      return false;
    }

    float x = 0, y = 0, z = 0, w = 0;
    if (parsed.has(x, "rotation", 0) &&
        parsed.has(y, "rotation", 1) &&
        parsed.has(z, "rotation", 2) &&
        parsed.has(w, "rotation", 3))
    {
      aview->setRotation(x, y, z, w);
      aview->rotated = true;  //Flag rotation finished
    }
  }
  else if (parsed.exists("translation"))
  {
    if (gethelp)
    {
      help += "> Set model translation in 3d coords (replaces any previous translation)\n\n"
              "> **Usage:** translation x y z\n\n"
              "> x (number) : x axis shift  \n"
              "> y (number) : y axis shift  \n"
              "> z (number) : z axis shift  \n";
      return false;
    }

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
    if (gethelp)
    {
      help += "> Rotate model\n\n"
              "> **Usage:** rotate axis degrees\n\n"
              "> axis (x/y/z) : axis of rotation  \n"
              "> degrees (number) : degrees of rotation  \n"
              "> \n**Usage:** rotate x y z\n\n"
              "> x (number) : x axis degrees of rotation  \n"
              "> y (number) : y axis degrees of rotation  \n"
              "> z (number) : z axis degrees of rotation  \n";
      return false;
    }

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
    if (gethelp)
    {
      help += "> Rotate model about x-axis\n\n"
              "> **Usage:** rotatex degrees\n\n"
              "> degrees (number) : degrees of rotation  \n";
      return false;
    }

    aview->rotate(fval, 0, 0);
    aview->rotated = true;  //Flag rotation finished
  }
  else if (parsed.has(fval, "rotatey"))
  {
    if (gethelp)
    {
      help += "> Rotate model about y-axis\n\n"
              "> **Usage:** rotatey degrees\n\n"
              "> degrees (number) : degrees of rotation  \n";
      return false;
    }

    aview->rotate(0, fval, 0);
    aview->rotated = true;  //Flag rotation finished
  }
  else if (parsed.has(fval, "rotatez"))
  {
    if (gethelp)
    {
      help += "> Rotate model about z-axis\n\n"
              "> **Usage:** rotatez degrees\n\n"
              "> degrees (number) : degrees of rotation  \n";
      return false;
    }

    aview->rotate(0, 0, fval);
    aview->rotated = true;  //Flag rotation finished
  }
  else if (parsed.has(fval, "zoom"))
  {
    if (gethelp)
    {
      help += "> Translate model in Z direction (zoom)\n\n"
              "> **Usage:** zoom units\n\n"
              "> units (number) : distance to zoom, units are based on model size  \n"
              ">                  1 unit is approx the model bounding box size  \n"
              ">                  negative to zoom out, positive in  \n";
      return false;
    }

    aview->zoom(fval);
  }
  else if (parsed.has(fval, "translatex"))
  {
    if (gethelp)
    {
      help += "> Translate model in X direction\n\n"
              "> **Usage:** translationx shift\n\n"
              "> shift (number) : x axis shift  \n";
      return false;
    }

    aview->translate(fval, 0, 0);
  }
  else if (parsed.has(fval, "translatey"))
  {
    if (gethelp)
    {
      help += "> Translate model in Y direction\n\n"
              "> **Usage:** translationy shift\n\n"
              "> shift (number) : y axis shift  \n";
      return false;
    }

    aview->translate(0, fval, 0);
  }
  else if (parsed.has(fval, "translatez"))
  {
    if (gethelp)
    {
      help += "> Translate model in Z direction\n\n"
              "> **Usage:** translationz shift\n\n"
              "> shift (number) : z axis shift  \n";
      return false;
    }

    aview->translate(0, 0, fval);
  }
  else if (parsed.exists("translate"))
  {
    if (gethelp)
    {
      help += "> Translate model in 3 dimensions\n\n"
              "> **Usage:** translate dir shift\n\n"
              "> dir (x/y/z) : direction to translate  \n"
              "> shift (number) : amount of translation  \n"
              "> \n**Usage:** translation x y z\n\n"
              "> x (number) : x axis shift  \n"
              "> y (number) : y axis shift  \n"
              "> z (number) : z axis shift  \n";
      return false;
    }

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
    if (gethelp)
    {
      help += "> Set model focal point in 3d coords\n\n"
              "> **Usage:** focus x y z\n\n"
              "> x (number) : x coord  \n"
              "> y (number) : y coord  \n"
              "> z (number) : z coord  \n";
      return false;
    }

    float xf = 0, yf = 0, zf = 0;
    if (parsed.has(xf, "focus", 0) &&
        parsed.has(yf, "focus", 1) &&
        parsed.has(zf, "focus", 2))
    {
      aview->focus(xf, yf, zf);
    }
  }
  else if (parsed.has(fval, "aperture"))
  {
    if (gethelp)
    {
      help += "> Set camera aperture (field-of-view)\n\n"
              "> **Usage:** aperture degrees\n\n"
              "> degrees (number) : degrees of viewing range  \n";
      return false;
    }

    aview->adjustStereo(fval, 0, 0);
  }
  else if (parsed.has(fval, "focallength"))
  {
    if (gethelp)
    {
      help += "> Set camera focal length (for stereo viewing)\n\n"
              "> **Usage:** focallength len\n\n"
              "> len (number) : distance from camera to focal point  \n";
      return false;
    }

    aview->adjustStereo(0, fval, 0);
  }
  else if (parsed.has(fval, "eyeseparation"))
  {
    if (gethelp)
    {
      help += "> Set camera stereo eye separation\n\n"
              "> **Usage:** eyeseparation len\n\n"
              "> len (number) : distance between left and right eye camera viewpoints  \n";
      return false;
    }

    aview->adjustStereo(0, 0, fval);
  }
  else if (parsed.has(fval, "zoomclip"))
  {
    if (gethelp)
    {
      help += "> Adjust near clip plane relative to current value\n\n"
              "> **Usage:** zoomclip multiplier\n\n"
              "> multiplier (number) : multiply current near clip plane by this value  \n";
      return false;
    }

    aview->zoomClip(fval);
  }
  else if (parsed.has(fval, "nearclip"))
  {
    if (gethelp)
    {
      help += "> Set near clip plane, before which geometry is not displayed\n\n"
              "> **Usage:** nearclip dist\n\n"
              "> dist (number) : distance from camera to near clip plane  \n";
      return false;
    }

    aview->near_clip = fval;
  }
  else if (parsed.has(fval, "farclip"))
  {
    if (gethelp)
    {
      help += "> Set far clip plane, beyond which geometry is not displayed\n\n"
              "> **Usage:** farrclip dist\n\n"
              "> dist (number) : distance from camera to far clip plane  \n";
      return false;
    }

    aview->far_clip = fval;
  }
  else if (parsed.exists("timestep"))  //Absolute
  {
    if (gethelp)
    {
      help += "> Set timestep to view\n\n"
              "> **Usage:** timestep up/down/value\n\n"
              "> value (integer) : the timestep to view  \n"
              "> up : switch to previous timestep if available  \n"
              "> down : switch to next timestep if available  \n";
      return false;
    }
    if (!parsed.has(ival, "timestep"))
    {
      if (parsed["timestep"] == "up")
        ival = Model::now-1;
      else if (parsed["timestep"] == "down")
        ival = Model::now+1;
      else
        ival = Model::now;
    }
    else //Convert to step idx
      ival = amodel->nearestTimeStep(ival);

    if (amodel->setTimeStep(ival) >= 0)
    {
      printMessage("Go to timestep %d", amodel->step());
      resetViews(); //Update the viewports
    }
    else
      return false;  //Invalid
  }
  else if (parsed.has(ival, "jump"))      //Relative
  {
    if (gethelp)
    {
      help += "> Jump from current timestep forward/backwards\n\n"
              "> **Usage:** jump value\n\n"
              "> value (integer) : how many timesteps to jump (negative for backwards)  \n";
      return false;
    }

    //Relative jump
    if (amodel->setTimeStep(Model::now+ival) >= 0)
    {
      printMessage("Jump to timestep %d", amodel->step());
      resetViews(); //Update the viewports
    }
    else
      return false;  //Invalid
  }
  else if (parsed.exists("model"))      //Model switch
  {
    if (gethelp)
    {
      help += "> Set model to view (when multiple models loaded)\n\n"
              "> **Usage:** model up/down/value\n\n"
              "> value (integer) : the model index to view [1,n]  \n"
              "> up : switch to previous model if available  \n"
              "> down : switch to next model if available  \n"
              "> add : add a new model  \n";
      return false;
    }

    if (model < 0) model = 0; //No model selection yet...
    if (!parsed.has(ival, "model"))
    {
      if (parsed["model"] == "up")
        ival = model-1;
      else if (parsed["model"] == "down")
        ival = model+1;
      else if (parsed["model"] == "add")
      {
        ival = models.size();
        defaultModel();
      }
      else
        ival = model;
    }
    if (ival < 0) ival = models.size()-1;
    if (ival >= (int)models.size()) ival = 0;
    if (!loadModelStep(ival, amodel->step())) return false;  //Invalid
    amodel->setTimeStep(Model::now); //Reselect ensures all loaded correctly
    printMessage("Load model %d", model);
  }
  else if (parsed.exists("figure"))
  {
    if (gethelp)
    {
      help += "> Set figure to view (when available)\n\n"
              "> **Usage:** figure up/down/value\n\n"
              "> value (integer/string) : the figure index or name to view  \n"
              "> up : switch to previous figure if available  \n"
              "> down : switch to next figure if available  \n";
      return false;
    }

    if (!parsed.has(ival, "figure"))
    {
      if (parsed["figure"] == "up")
        ival = amodel->figure-1;
      else if (parsed["figure"] == "down")
        ival = amodel->figure+1;
      else
      {
        ival = -1;
        for (unsigned int i=0; i<amodel->fignames.size(); i++)
          if (amodel->fignames[i] == parsed["figure"]) ival = i;
        //Not found? Create it
        if (ival < 0)
        {
          amodel->fignames.push_back(parsed["figure"]);
          std::stringstream ss;
          amodel->jsonWrite(ss, 0, false);
          amodel->figures.push_back(ss.str());
          ival = amodel->figures.size()-1;
        }
      }
    }

    if (!amodel->loadFigure(ival)) return false; //Invalid
    viewset = 2; //Force check for resize and autozoom
    printMessage("Load figure %d", amodel->figure);
  }
  else if (parsed.exists("view"))
  {
    if (gethelp)
    {
      help += "> Set view (when available)\n\n"
              "> **Usage:** view up/down/value\n\n"
              "> value (integer) : the view index to switch to  \n"
              "> up : switch to previous view if available  \n"
              "> down : switch to next view if available  \n";
      return false;
    }

    if (!parsed.has(ival, "view"))
    {
      if (parsed["view"] == "up")
        ival = view-1;
      else if (parsed["view"] == "down")
        ival = view+1;
      else
        ival = view;
    }

    viewSelect(ival);
    printMessage("Set viewport to %d", view);
    amodel->redraw();
  }
  else if (parsed.exists("hide") || parsed.exists("show"))
  {
    std::string action = parsed.exists("hide") ? "hide" : "show";

    if (gethelp)
    {
      help += "> " + action + " objects/geometry types\n\n"
              "> **Usage:** " + action + " object\n\n"
              "> object (integer/string) : the index or name of the object to " + action + " (see: \"list objects\")  \n"
              "> \n**Usage:** " + action + " geometry_type id\n\n"
              "> geometry_type : points/labels/vectors/tracers/triangles/quads/shapes/lines/volumes  \n"
              "> id (integer, optional) : id of geometry to " + action + "  \n"
              "> eg: 'hide points' will " + action + " all objects containing point data  \n"
              "> note: 'hide all' will " + action + " all objects  \n";
      return false;
    }

    std::string what = parsed[action];

    if (what == "all")
    {
      for (unsigned int i=0; i < Model::geometry.size(); i++)
        Model::geometry[i]->hideShowAll(action == "hide");
      return true;
    }

    //Have selected a geometry type?
    Geometry* active = getGeometryType(what);
    if (active)
    {
      int id;
      std::string range = parsed.get(action, 1);
      if (range.find('-') != std::string::npos)
      {
        std::stringstream rangess(range);
        int start, end;
        char delim;
        rangess >> start >> delim >> end;
        for (int i=start; i<=end; i++)
        {
          if (action == "hide") active->hide(i);
          else active->show(i);
          printMessage("%s %s range %s", action.c_str(), what.c_str(), range.c_str());
        }
      }
      else if (parsed.has(id, action, 1))
      {
        parsed.has(id, action, 1);
        if (action == "hide")
        {
          if (!active->hide(id)) return false; //Invalid
        }
        else
        {
          if (!active->show(id)) return false; //Invalid
        }
        printMessage("%s %s %d", action.c_str(), what.c_str(), id);
      }
      else
      {
        active->hideShowAll(action == "hide");
        printMessage("%s all %s", action.c_str(), what.c_str());
      }
      amodel->redraw();
    }
    else
    {
      //Hide/show by name/ID match in all drawing objects
      std::vector<DrawingObject*> list = lookupObjects(parsed, action);
      for (unsigned int c=0; c<list.size(); c++)
      {
        if (list[c]->skip)
        {
          std::ostringstream ss;
          ss << "load " << list[c]->name();
          return parseCommands(ss.str());
        }
        else
        {
          //Hide/show all data for this object
          bool state = (action == "show");
          for (unsigned int i=0; i < Model::geometry.size(); i++)
            Model::geometry[i]->showObj(list[c], state);
          list[c]->properties.data["visible"] = state; //This allows hiding of objects without geometry (colourbars)
          printMessage("%s object %s", action.c_str(), list[c]->name().c_str());
          amodel->redraw();
        }
      }
    }
  }
  else if (parsed.has(ival, "movie"))
  {
    if (gethelp)
    {
      help += "> Encode video of model running from current timestep to specified timestep  \n"
              "> (Requires libavcodec)\n\n"
              "> **Usage:** movie endstep\n\n"
              "> endstep (integer) : last frame timestep  \n";
      return false;
    }

    encodeVideo();
    writeSteps(false, amodel->step(), ival);
    encodeVideo(); //Write final step and stop encoding
  }
  else if (parsed.exists("play"))
  {
    if (gethelp)
    {
      help += "> Display model timesteps in sequence from current timestep to specified timestep\n\n"
              "> **Usage:** play endstep\n\n"
              "> endstep (integer) : last frame timestep  \n"
              "> If endstep omitted, will loop continually until 'stop' command entered  \n";
      return false;
    }

    if (parsed.has(ival, "play"))
    {
      writeSteps(false, amodel->step(), ival);
    }
    else
    {
      //Play loop
      if (animate < 1) animate = TIMER_INC;
      viewer->idleTimer(animate); //Start idle redisplay timer for frequent frame updates
      replay.clear();
      last_cmd = "next";
      replay.push_back(last_cmd);
      repeat = -1; //Infinite
      loop = true;
      OpenGLViewer::commands.push_back(std::string("next"));
    }
    return true;  //Skip record
  }
  else if (parsed.exists("next"))
  {
    if (gethelp)
    {
      help += "> Go to next timestep in sequence  \n";
      return false;
    }

    int old = Model::now;
    if (amodel->timesteps.size() < 2) return false;
    amodel->setTimeStep(Model::now+1);
    //Allow loop back to start when using next command
    if (Model::now > 0 && Model::now == old)
      amodel->setTimeStep(0);
    resetViews(); //Update the viewports

    if (loop)
      OpenGLViewer::commands.push_back(std::string("next"));
  }
  else if (parsed.exists("stop"))
  {
    if (gethelp)
    {
      help += "> Stop the looping 'play' command  \n";
      return false;
    }

    viewer->idleTimer(0); //Stop idle redisplay timer
    loop = false;
  }
  else if (parsed.exists("images"))
  {
    if (gethelp)
    {
      help += "> Write images in sequence from current timestep to specified timestep\n\n"
              "> **Usage:** images endstep\n\n"
              "> endstep (integer) : last frame timestep  \n";
      return false;
    }

    //Default to writing from current to final step
    int end = TimeStep::timesteps[TimeStep::timesteps.size()-1]->step;
    if (parsed.has(ival, "images"))
      end = ival;

    writeSteps(true, amodel->step(), end);
  }
  else if (parsed.exists("animate"))
  {
    if (gethelp)
    {
      help += "> Update display between each command\n\n"
              "> **Usage:** animate rate\n\n"
              "> rate (integer) : animation timer to fire every (rate) msec (default: 50)  \n"
              "> When on, if multiple commands are issued the frame is re-rendered at set framerate  \n"
              "> When off, all commands will be processed before the display is updated  \n";
      return false;
    }

    if (parsed.has(ival, "animate"))
      animate = ival;
    else if (animate > 0)
      animate = 0;
    else
      animate = TIMER_INC;
    viewer->idleTimer(animate); //Start idle redisplay timer for frequent frame updates
    printMessage("Animate mode %d millseconds", animate);
    return true; //No record
  }
  else if (parsed.exists("repeat"))
  {
    if (gethelp)
    {
      help += "> Repeat commands from history\n\n"
              "> **Usage:** repeat count\n\n"
              "> count (integer) : repeat the last entered command count times  \n"
              "> \n**Usage:** repeat history count (animate)\n\n"
              "> count (integer) : repeat every command in history buffer count times  \n";
      return false;
    }

    //Repeat N commands from history
    if (parsed["repeat"] == "history" && parsed.has(ival, "repeat", 1))
    {
      if (animate > 0 && repeat == 0)
      {
        repeat = ival;
        replay = history;
      }
      else
      {
        for (int r=0; r<ival; r++)
        {
          for (unsigned int l=0; l<history.size(); l++)
            parseCommands(history[l]);
        }
      }
      return true; //Skip record
    }
    //Repeat last command N times
    else if (parsed.has(ival, "repeat"))
    {
      if (animate > 0)
      {
        repeat = ival;
        replay.push_back(last_cmd);
      }
      else
      {
        for (int r=0; r<ival; r++)
          parseCommands(last_cmd);
      }
      return true;  //Skip record
    }
    else
      return parseCommands(last_cmd);
  }
  else if (parsed.exists("axis"))
  {
    if (gethelp)
    {
      help += "> Display/hide the axis legend\n\n"
              "> **Usage:** axis (on/off)\n\n"
              "> on/off (optional) : show/hide the axis, if omitted switches state  \n";
      return false;
    }

    std::string axis = parsed["axis"];
    if (parsed["axis"] == "on")
      aview->properties.data["axis"] = true;
    else if (parsed["axis"] == "off")
      aview->properties.data["axis"] = false;
    else
      aview->properties.data["axis"] = !aview->properties["axis"];
    printMessage("Axis %s", aview->properties["axis"] ? "ON" : "OFF");
  }
  else if (parsed.exists("toggle"))
  {
    if (gethelp)
    {
      help += "> Toggle a boolean property\n\n"
              "> **Usage:** toogle (property-name)\n\n"
              "> property-name : name of property to switch  \n"
              "> If an object is selected, will try there, then view, then global\n";
      return false;
    }

    std::string what = parsed["toggle"];
    if (aobject && aobject->properties.has(what) && aobject->properties[what].is_boolean())
    {
      bool current = aobject->properties[what];
      aobject->properties.data[what] = !current;
      amodel->redraw();
      printMessage("Property '%s' set to %s", what.c_str(), !current ? "ON" : "OFF");
    }
    else if (aview->properties.has(what) && aview->properties[what].is_boolean())
    {
      bool current = aview->properties[what];
      aview->properties.data[what] = !current;
      amodel->redraw();
      printMessage("Property '%s' set to %s", what.c_str(), !current ? "ON" : "OFF");
    }
    else if (Properties::defaults.count(what) > 0 && Properties::global(what).is_boolean())
    {
      bool current = Properties::global(what);
      Properties::global(what) = !current;
      amodel->redraw();
      printMessage("Property '%s' set to %s", what.c_str(), !current ? "ON" : "OFF");
    }
  }
  else if (parsed.exists("redraw"))
  {
    if (gethelp)
    {
      help += "> Redraw all object data, required after changing some parameters but may be slow  \n";
      return false;
    }

    //for (int type=lucMinType; type<lucMaxType; type++)
    //   Model::geometry[type]->redraw = true;
    amodel->redraw(true); //Redraw & reload
    printMessage("Redrawing all objects");
  }
  else if (parsed.exists("fullscreen"))
  {
    if (gethelp)
    {
      help += "> Switch viewer to full-screen mode and back to windowed mode  \n";
      return false;
    }

    viewer->fullScreen();
    printMessage("Full-screen is %s", viewer->fullscreen ? "ON":"OFF");
  }
  else if (parsed.exists("scaling"))
  {
    if (gethelp)
    {
      help += "> Disable/enable scaling, default is on, disable to view model un-scaled  \n";
      return false;
    }

    printMessage("Scaling is %s", aview->scaleSwitch() ? "ON":"OFF");
  }
  else if (parsed.exists("fit"))
  {
    if (gethelp)
    {
      help += "> Adjust camera view to fit the model in window  \n";
      return false;
    }

    aview->zoomToFit();
  }
  else if (parsed.exists("autozoom"))
  {
    if (gethelp)
    {
      help += "> Automatically adjust camera view to always fit the model in window (enable/disable)  \n";
      return false;
    }

    aview->autozoom = !aview->autozoom;
    printMessage("AutoZoom is %s", aview->autozoom ? "ON":"OFF");
  }
  else if (parsed.exists("stereo"))
  {
    if (gethelp)
    {
      help += "> Enable/disable stereo projection  \n"
              "> If no stereo hardware found will use red/cyan anaglyph mode  \n";
      return false;
    }

    aview->stereo = !aview->stereo;
    printMessage("Stereo is %s", aview->stereo ? "ON":"OFF");
  }
  else if (parsed.exists("coordsystem"))
  {
    if (gethelp)
    {
      help += "> Switch between Right-handed and Left-handed coordinate system  \n";
      return false;
    }

    printMessage("%s coordinate system", (aview->switchCoordSystem() == RIGHT_HANDED ? "Right-handed":"Left-handed"));
  }
  else if (parsed.exists("title"))
  {
    if (gethelp)
    {
      help += "> Set title heading to following text  \n";
      return false;
    }

    aview->properties.data["title"] = parsed.getall("title", 0);
  }
  else if (parsed.exists("rulers"))
  {
    if (gethelp)
    {
      help += "> Enable/disable axis rulers (unfinished)  \n";
      return false;
    }

    //Show/hide rulers
    aview->properties.data["rulers"] = !aview->properties["rulers"];
    printMessage("Rulers %s", aview->properties["rulers"] ? "ON" : "OFF");
  }
  else if (parsed.exists("help"))
  {
    if (gethelp)
    {
      help += "> Display help about a command  \n";
      return false;
    }

    viewer->display();  //Ensures any previous help text wiped
    std::string cmd = parsed["help"];
    if (cmd.length() > 0)
    {
      help = "~~~~~~~~~~~~~~~~~~~~~~~~  \n" + cmd + "\n~~~~~~~~~~~~~~~~~~~~~~~~  \n";
      helpCommand(cmd);
      displayText(help);
      std::cout << help;
    }
    else
    {
      help = "~~~~~~~~~~~~~~~~~~~~~~~~\nhelp\n~~~~~~~~~~~~~~~~~~~~~~~~  \n";
      helpCommand("help");
      displayText(help);
      std::cout << HELP_INTERACTION;
      std::cout << help;
    }
    viewer->swap();  //Immediate display
    redisplay = false;
  }
  else if (parsed.exists("antialias"))
  {
    if (gethelp)
    {
      help += "> Enable/disable multi-sample anti-aliasing (if supported by OpenGL drivers)  \n";
      return false;
    }

    aview->properties.data["antialias"] = !aview->properties["antialias"];
    printMessage("Anti-aliasing %s", aview->properties["antialias"] ? "ON":"OFF");
  }
  else if (parsed.exists("valuerange"))
  {
    if (gethelp)
    {
      help += "> Adjust colourmaps on each object to fit actual value range  \n";
      return false;
    }

    DrawingObject* obj = aobject;
    if (!obj)
      obj = lookupObject(parsed, "valuerange");
    if (obj)
    {
      for (int type=lucMinType; type<lucMaxType; type++)
        Model::geometry[type]->setValueRange(obj);
      printMessage("ColourMap scales set to local value range");
      amodel->redraw(true); //Colour change so force reload
    }
  }
  else if (parsed.exists("export"))
  {
    if (gethelp)
    {
      help += "> Export object data\n\n"
              "> **Usage:** export [format] [object]\n\n"
              "> format (string) : json/csv/db (left blank: compressed db)  \n"
              "> object (integer/string) : the index or name of the object to export (see: \"list objects\")  \n"
              "> object_name (string) : the name of the object to export (see: \"list objects\")  \n"
              "> If object ommitted all will be exported  \n";
      return false;
    }

    std::string what = parsed["export"];
    lucExportType type = what == "json" ? lucExportJSON : (what == "csv" ? lucExportCSV : (what == "db" ? lucExportGLDB : lucExportGLDBZ));
    //Export drawing object by name/ID match
    std::vector<DrawingObject*> list = lookupObjects(parsed, "export");
    if (list.size() == 0)
    {
      //Only export all if no other spec provided
      exportData(type);
      printMessage("Dumped all objects to %s", what.c_str());
    }
    else
    {
      for (unsigned int c=0; c<list.size(); c++)
      {
        exportData(type, list[c]);
        printMessage("Dumped object %s to %s", list[c]->name().c_str(), what.c_str());
      }
    }
  }
  else if (parsed.exists("lockscale"))
  {
    if (gethelp)
    {
      help += "> Enable/disable colourmap lock  \n"
              "> When locked, dynamic range colourmaps will keep current values  \n"
              "> when switching between timesteps instead of being re-calibrated  \n";
      return false;
    }

    ColourMap::lock = !ColourMap::lock;
    printMessage("ColourMap scale locking %s", ColourMap::lock ? "ON":"OFF");
    amodel->redraw(true); //Colour change so force reload
  }
  else if (parsed.exists("list"))
  {
    if (gethelp)
    {
      help += "> List available data\n\n"
              "> **Usage:** list objects/colourmaps/elements\n\n"
              "> objects : enable object list (stays on screen until disabled)  \n"
              ">           (dimmed objects are hidden or not in selected viewport)  \n"
              "> colourmaps : show colourmap list  \n"
              "> elements : show geometry elements by id in terminal  \n"
              "> data : show available data sets in selected object or all  \n";
      return false;
    }

    if (parsed["list"] == "elements")
    {
      //Print available elements by id
      for (unsigned int i=0; i < Model::geometry.size(); i++)
        Model::geometry[i]->print();
      viewer->swap();  //Immediate display
      return false;
    }
    else if (parsed["list"] == "colourmaps")
    {
      int offset = 0;
      std::cerr << "ColourMaps:\n===========\n";
      for (unsigned int i=0; i < amodel->colourMaps.size(); i++)
      {
        if (amodel->colourMaps[i])
        {
          std::ostringstream ss;
          ss << std::setw(5) << (i+1) << " : " << amodel->colourMaps[i]->name;

          displayText(ss.str(), ++offset);
          std::cerr << ss.str() << std::endl;
        }
      }
      viewer->swap();  //Immediate display
      return false;
    }
    else if (parsed["list"] == "data")
    {
      int offset = 0;
      std::vector<std::string> list;
      if (aobject)
      {
        displayText("Data sets for: " + aobject->name(), ++offset);
        std::cout << ("Data sets for: " + aobject->name()) << std::endl;
      }
      displayText("-----------------------------------------", ++offset);
      std::cout << "-----------------------------------------" << std::endl;
      for (unsigned int i=0; i < Model::geometry.size(); i++)
      {
        json list = Model::geometry[i]->getDataLabels(aobject);
        for (unsigned int l=0; l < list.size(); l++)
        {
          std::stringstream ss;
          ss << "[" << l << "] " << list[l]["label"]
           << " (range: " << list[l]["minimum"]
           << " to " << list[l]["maximum"] << ")"
           << " -- " << list[l]["size"] << "";
          displayText(ss.str(), ++offset);
          std::cerr << ss.str() << std::endl;
        }
      }
      displayText("-----------------------------------------", ++offset);
      std::cout << "-----------------------------------------" << std::endl;
      viewer->swap();  //Immediate display
      return false;
    }
    else //if (parsed["list"] == "objects")
    {
      objectlist = !objectlist;
      displayObjectList(true);
    }
  }
  else if (parsed.exists("reset"))
  {
    if (gethelp)
    {
      help += "> Reset the camera to the default model view  \n";
      return false;
    }

    aview->reset();     //Reset camera
    aview->init(true);  //Reset camera to default view of model
    printMessage("View reset");
  }
  else if (parsed.exists("bounds"))
  {
    if (gethelp)
    {
      help += "> Recalculate the model bounding box from geometry  \n";
      return false;
    }

    //Remove any existing fixed bounds
    aview->properties.data.erase("min");
    aview->properties.data.erase("max");
    Properties::globals.erase("min");
    Properties::globals.erase("max");
    //Update the viewports and recalc bounding box
    resetViews();
    //Update fixed bounds
    aview->properties.data["min"] = {aview->min[0], aview->min[1], aview->min[2]};
    aview->properties.data["max"] = {aview->max[0], aview->max[1], aview->max[2]};
    printMessage("View bounds update");
  }
  else if (parsed.exists("clear"))
  {
    if (gethelp)
    {
      help += "> Clear all data of current model/timestep\n\n"
              "> **Usage:** clear [objects]\n\n"
              "> objects : optionally clear all object entries  \n"
              ">           (if omitted, only the objects geometry data is cleared)  \n";
      return false;
    }

    clearData(parsed["clear"] == "objects");
  }
  else if (parsed.exists("reload"))
  {
    if (gethelp)
    {
      help += "> Reload all data of current model/timestep from database  \n"
              "> (If no database, will still reload window)  \n";
      return false;
    }

    //Restore original data
    amodel->loadTimeSteps();
    if (model < 0 || !loadModelStep(model)) return false;
  }
  else if (parsed.exists("zerocam"))
  {
    if (gethelp)
    {
      help += "> Set the camera postiion to the origin (for scripting, not generally advisable)  \n";
      return false;
    }

    aview->reset();     //Zero camera
  }
  else if (parsed.exists("colourmap"))
  {
    if (gethelp)
    {
      help += "> Set colourmap on object\n\n"
              "> **Usage:** colourmap object [colourmap / \"add\" | \"data\"]\n\n"
              "> object (integer/string) : the index or name of the object to set (see: \"list objects\")  \n"
              "> colourmap (integer/string) : the index or name of the colourmap to apply (see: \"list colourmaps\")  \n"
              "> add : add a new colourmap to selected object  \n"
              "> data (string) : data to load into selected object's colourmap  \n";
      return false;
    }

    //Set colourmap on object by name/ID match
    DrawingObject* obj = aobject;
    int next = 0;
    parsed.has(ival, "colourmap"); //Get id if any
    if (!obj)
    {
      obj = lookupObject(parsed, "colourmap");
      next++;
    }
    if (obj)
    {
      int cmap = lookupColourMap(parsed, "colourmap", next);
      std::string what = parsed.get("colourmap", next);
      //Only able to set the value colourmap now
      if (cmap >= 0)
      {
        obj->properties.data["colourmap"] = cmap;
        printMessage("%s colourmap set to %s (%d)", obj->name().c_str(), amodel->colourMaps[cmap]->name.c_str(), cmap);
      }
      else if (ival < 0 || what.length() == 0)
      {
        obj->properties.data["colourmap"] = -1;
        printMessage("%s colourmap set to none", obj->name().c_str());
      }
      else
      {
        //No cmap id, parse a colourmap string
        cmap = obj->properties["colourmap"];
        if (what == "add")
        {
          cmap = amodel->addColourMap();
          obj->properties.data["colourmap"] = cmap;
          if (what == "add")
            what = parsed.getall("colourmap", next+1);
          else
            what = "";
        }
        else
          what = parsed.getall("colourmap", next);

        if (what.length() > 0)
        {
          //Add new map if none set on object
          json current = obj->properties["colourmap"];
          if (current.is_number() && current >= 0 && amodel->colourMaps.size() > 0)
            cmap = current;
          else
            cmap = amodel->addColourMap();
          amodel->colourMaps[cmap]->loadPalette(what);
          //cmap->print();
          obj->properties.data["colourmap"] = cmap;
          //amodel->colourMaps[cmap]->calibrate(); //Recalibrate
        }
      }
      redraw(obj);
      amodel->redraw(true); //Redraw & reload
    }
  }
  else if (parsed.exists("colour"))
  {
    if (gethelp)
    {
      help += "> Add a colour value to selected object, appends to colour value array\n\n"
              "> **Usage:** colour colourval\n\n"
              "> colourval (string) : json [r,g,b(,a)] or hex RRGGBB(AA)  \n";
      return false;
    }

    //Add colours to object
    //(this now only adds rgba data values, to set global colour use property: colour=val)
    if (aobject)
    {
      Colour c(parsed.get("colour"));
      /*/Find the first available geometry container for this drawing object and append a colour
      GeomData* geomdata = getGeometry(aobject);
      if (geomdata)
      {
        geomdata->data[lucRGBAData]->read(1, &c.value);
        printMessage("%s colour appended %x", aobject->name().c_str(), c.value);
      }*/
      //Use the "geometry" property to get the type to read into
      std::string gtype = aobject->properties["geometry"];
      Geometry* active = getGeometryType(gtype);
      active->read(aobject, 1, lucRGBAData, &c.value);
      printMessage("%s colour appended %x", aobject->name().c_str(), c.value);
      redraw(aobject);
      amodel->redraw(true); //Redraw & reload
    }
  }
  else if (parsed.exists("value"))
  {
    if (gethelp)
    {
      help += "> Add a float value to selected object, appends to value array\n\n"
              "> **Usage:** value val\n\n"
              "> val (number) : value to append  \n";
      return false;
    }

    //Add values to object
    if (aobject && parsed.has(fval, "value"))
    {
      //Use the "geometry" property to get the type to read into
      std::string gtype = aobject->properties["geometry"];
      Geometry* active = getGeometryType(gtype);
      active->read(aobject, 1, lucColourValueData, &fval);
      printMessage("%s value appended %f", aobject->name().c_str(), fval);

      redraw(aobject);
      amodel->redraw(true); //Redraw & reload
    }
  }
  else if (parsed.exists("vertex") || parsed.exists("vector") || parsed.exists("normal"))
  {
    std::string action = "vertex";
    lucGeometryDataType dtype = lucVertexData;
    if (parsed.exists("vector"))
    {
      action = "vector";
      dtype = lucVectorData;
    }
    if (parsed.exists("normal"))
    {
      action = "normal";
      dtype = lucNormalData;
    }

    if (gethelp)
    {
      help += "> Add a " + action + " to selected object, appends to " + action + " array\n\n"
              "> **Usage:** " + action + " x y z\n\n"
              "> x (number) : " + action + " X coord  \n"
              "> y (number) : " + action + " Y coord  \n"
              "> z (number) : " + action + " Z coord\n\n"
              "> (minX/Y/Z or maxX/Y/Z can be used in place of number for model bounding box edges)  \n";
      return false;
    }

    if (aobject)
    {
      float xyz[3];
      for (int i=0; i<3; i++)
      {
        if (!parsed.has(xyz[i], action, i))
          xyz[i] = parseCoord(parsed.get(action, i));
      }

      //Use the "geometry" property to get the type to read into
      std::string gtype = aobject->properties["geometry"];
      Geometry* active = getGeometryType(gtype);
      active->read(aobject, 1, dtype, xyz);
      printMessage("%s %s appended", aobject->name().c_str(), action.c_str());

      redraw(aobject);
      amodel->redraw(true); //Redraw & reload
    }
  }
  else if (parsed.exists("read"))
  {
    if (gethelp)
    {
      help += "> Read data into selected object, appends to specified data array\n\n"
              "> **Usage:** read type\n\n"
              "> type (string) : data type to load vertices/vectors/normals/values/colours  \n"
              "> Before using this command, store the data a json array in a property of same name\n\n";
      return false;
    }

    std::string what = parsed["read"];
    if (aobject && aobject->properties.has(what))
    {
      json data = aobject->properties[what];
      lucGeometryDataType dtype;

      int width = 3;
      if (what == "vertices")
        dtype = lucVertexData;
      else if (what == "normals")
        dtype = lucNormalData;
      else if (what == "vectors")
        dtype = lucVectorData;
      else if (what == "colours")
      {
        dtype = lucRGBAData;
        width = 1;
        if (data.is_string())
        {
          //Convert to colour values using colourmap parser
          ColourMap cmap;
          cmap.parse(data);
          data = json::array();
          for (unsigned int i=0; i<cmap.colours.size(); i++)
            data.push_back((float)cmap.colours[i].colour.value);
        }
      }
      else if (what == "values")
      {
        dtype = lucColourValueData;
        width = 1;
      }

      //Use the "geometry" property to get the type to read into
      std::string gtype = aobject->properties["geometry"];
      Geometry* active = getGeometryType(gtype);
      if (!data.is_array() || data.size() == 0) return false;
      int size = data.size();
      if (data[0].is_array()) size *= data[0].size();
      float* vals = new float[size];
      for (unsigned int i=0; i<data.size(); i++)
      {
        if (data[i].is_array())
        {
          if (width != 3) return false;
          //Support 2d array for vertex/vector/normal
          vals[i*3] = parseCoord(data[i][0]);
          vals[i*3+1] = parseCoord(data[i][1]);
          vals[i*3+2] = parseCoord(data[i][2]);
        }
        else if (dtype == lucRGBAData)
        {
          Colour c(data[i]);
          vals[i] = c.fvalue;
        }
        else
          vals[i] = parseCoord(data[i]);
      }

      int len = size/width;
      active->read(aobject, len, dtype, vals);
      delete[] vals;
      printMessage("%s %s appended %d", aobject->name().c_str(), what.c_str(), len);

      redraw(aobject);
      amodel->redraw(true); //Redraw & reload
    }
  }
  else if (parsed.exists("pointtype"))
  {
    if (gethelp)
    {
      help += "> Set point-rendering type on object\n\n"
              "> **Usage:** pointtype all/object type/up/down\n\n"
              "> all : use 'all' to set the global default point type  \n"
              "> object (integer/string) : the index or name of the object to set (see: \"list objects\")  \n"
              "> type (integer) : Point type [0,3] to apply (gaussian/flat/sphere/highlight sphere)  \n"
              "> up/down : use 'up' or 'down' to switch to the previous/next type in list  \n";
      return false;
    }

    std::string what = parsed["pointtype"];
    if (what == "all")
    {
      int pt = 1;
      if (Properties::globals.count("pointtype") > 0)
        pt = Properties::globals["pointtype"];
      if (parsed.has(ival, "pointtype", 1))
        Properties::globals["pointtype"] = ival % 5;
      else
        Properties::globals["pointtype"] = (pt+1) % 5;
      printMessage("Point type %d", (int)Properties::globals["pointtype"]);
    }
    else
    {
      //Point type by name/ID match in all drawing objects
      DrawingObject* obj = aobject;
      int next = 0;
      if (!obj)
      {
        obj = lookupObject(parsed, "pointtype");
        next++;
      }
      if (obj)
      {
        int pt = obj->properties["pointtype"];
        if (parsed.has(ival, "pointtype", next))
          obj->properties.data["pointtype"] = ival;
        else if (parsed.get("pointtype", next) == "up")
          obj->properties.data["pointtype"] = (pt-1) % 5;
        else if (parsed.get("pointtype", next) == "down")
          obj->properties.data["pointtype"] = (pt+1) % 5;
        printMessage("%s point type set to %d", obj->name().c_str(), (int)obj->properties["pointtype"]);
        redraw(obj); //Full reload of this object only
        amodel->redraw();
      }
    }
  }
  else if (parsed.exists("pointsample"))
  {
    if (gethelp)
    {
      help += "> Set point sub-sampling value\n\n"
              "> **Usage:** pointsample value/up/down\n\n"
              "> value (integer) : 1=no sub-sampling=50%% sampled etc..  \n"
              "> up/down : use 'up' or 'down' to sample more (up) or less (down) points  \n";
      return false;
    }

    if (parsed.has(ival, "pointsample"))
      Properties::globals["pointsubsample"] = ival;
    else if (parsed["pointsample"] == "up")
      Properties::globals["pointsubsample"] = (int)Properties::global("pointsubsample") / 2;
    else if (parsed["pointsample"] == "down")
      Properties::globals["pointsubsample"] = (int)Properties::global("pointsubsample") * 2;
    if ((int)Properties::global("pointsubsample") < 1) Properties::globals["pointsubsample"] = 1;
    Model::points->redraw = true;
    printMessage("Point sampling %d", (int)Properties::global("pointsubsample"));
  }
  else if (parsed.exists("image"))
  {
    if (gethelp)
    {
      help += "> Save an image of the current view  \n"
              "> **Usage:** image [filename]\n\n"
              "> filename (string) : optional base filename without extension  \n";
      return false;
    }

    std::string filename = parsed["image"];
    if (filename.length() > 0)
      viewer->image(getImageFilename(filename));
    else
    {
      //Apply image counter to default filename when multiple images output
      static int imagecounter = 0;
      std::stringstream outpath;
      std::string title = Properties::global("caption");
      outpath << title;
      if (imagecounter > 0)
        outpath <<  "-" << imagecounter;
      std::string fn = getImageFilename(outpath.str());
      viewer->image(fn);
      imagecounter++;
    }

    if (viewer->outwidth > 0)
      printMessage("Saved image %d x %d", viewer->outwidth, viewer->outheight);
    else
      printMessage("Saved image %d x %d", viewer->width, viewer->height);
  }
  else if (parsed.has(ival, "outwidth"))
  {
    if (gethelp)
    {
      help += "> Set output image width (height calculated to match window aspect)\n\n"
              "> **Usage:** outwidth value\n\n"
              "> value (integer) : width in pixels for output images  \n";
      return false;
    }

    if (ival < 10) return false;
    viewer->outwidth = ival;
    printMessage("Output image width set to %d", viewer->outwidth);
  }
  else if (parsed.has(ival, "outheight"))
  {
    if (gethelp)
    {
      help += "> Set output image height\n\n"
              "> **Usage:** outheight value\n\n"
              "> value (integer) : height in pixels for output images  \n";
      return false;
    }

    if (ival < 10) return false;
    viewer->outheight = ival;
    printMessage("Output image height set to %d", viewer->outheight);
  }
  else if (parsed.exists("background"))
  {
    if (gethelp)
    {
      help += "> Set window background colour\n\n"
              "> **Usage:** background colour/value/white/black/grey/invert\n\n"
              "> colour (string) : any valid colour string  \n"
              "> value (number [0,1]) : sets to greyscale value with luminosity in range [0,1] where 1.0 is white  \n"
              "> white/black/grey : sets background to specified value  \n"
              "> invert (or omitted value) : inverts current background colour  \n";
      return false;
    }
    
    std::string val = parsed["background"];
    if (val == "invert")
    {
      viewer->background.invert();
    }
    else if (parsed.has(fval, "background"))
    {
      viewer->background.a = 255;
      if (fval <= 1.0) fval *= 255;
      viewer->background.r = viewer->background.g = viewer->background.b = fval;
    }
    else if (val.length() > 0)
    {
      parsePropertySet("background=" + parsed["background"]);
      viewer->background = Colour(aview->properties["background"]);
    }
    else
    {
      if (viewer->background.r == 255 || viewer->background.r == 0)
        viewer->background.value = 0xff666666;
      else
        viewer->background.value = 0xff000000;
    }
    aview->properties.data["background"] = viewer->background.toString();
    viewer->setBackground();
    printMessage("Background colour set");
  }
  else if (parsed.exists("border"))
  {
    if (gethelp)
    {
      help += "> Set model border frame\n\n"
              "> **Usage:** border on/off/filled\n\n"
              "> on : line border around model bounding-box  \n"
              "> off : no border  \n"
              "> filled : filled background bounding box  \n"
              "> (no value) : switch between possible values  \n";
      return false;
    }

    //Frame off/on/filled
    aview->properties.data["fillborder"] = false;
    if (parsed["border"] == "on")
      aview->properties.data["border"] = 1;
    else if (parsed["border"] == "off")
      aview->properties.data["border"] = 0;
    else if (parsed["border"] == "filled")
    {
      aview->properties.data["fillborder"] = true;
      aview->properties.data["border"] = 1;
    }
    else
    {
      if (parsed.has(ival, "border"))
        aview->properties.data["border"] = ival;
      else
        aview->properties.data["border"] = !aview->properties["border"];
    }
    printMessage("Frame set to %s, filled=%d", (bool)aview->properties["border"] ? "ON" : "OFF", (bool)aview->properties["fillborder"]);
  }
  else if (parsed.exists("camera"))
  {
    if (gethelp)
    {
      help += "> Output camera state for use in model scripts  \n";
      return false;
    }

    //Output camera view xml and translation/rotation commands
    aview->print();
    return false;
  }
  else if (parsed.has(fval, "modelscale"))
  {
    if (gethelp)
    {
      help += "> Set model scaling, multiples by existing values\n\n"
              "> **Usage:** scale xval yval zval\n\n"
              "> xval (number) : scaling value applied to x axis\n\n"
              "> yval (number) : scaling value applied to y axis  \n"
              "> zval (number) : scaling value applied to z axis  \n";
      return false;
    }

    float y, z;
    if (parsed.has(y, "modelscale", 1) && parsed.has(z, "modelscale", 2))
    {
      aview->setScale(fval, y, z, false);
      amodel->redraw();
    }
    else
    {
      //Scale everything
      aview->setScale(fval, fval, fval, false);
      amodel->redraw();
    }
  }
  else if (parsed.exists("scale"))
  {
    if (gethelp)
    {
      help += "> Scale model or applicable objects\n\n"
              "> **Usage:** scale axis value\n\n"
              "> axis : x/y/z  \n"
              "> value (number) : scaling value applied to all geometry on specified axis (multiplies existing)\n\n"
              "> **Usage:** scale geometry_type value/up/down\n\n"
              "> geometry_type : points/vectors/tracers/shapes  \n"
              "> value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling  \n"
              "> \n**Usage:** scale object value/up/down\n\n"
              "> object (integer/string) : the index or name of the object to scale (see: \"list objects\")  \n"
              "> value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling  \n";
      return false;
    }

    std::string what = parsed["scale"];
    Geometry* active = getGeometryType(what);
    if (active)
    {
      std::string key = "scale" + what;
      float scale = 1.0;
      if (Properties::globals.count(key) > 0) scale = Properties::globals[key];
      if (parsed.has(fval, "scale", 1))
        Properties::globals[key] = fval;
      else if (parsed.get("scale", 1) == "up")
        Properties::globals[key] = scale * 1.5;
      else if (parsed.get("scale", 1) == "down")
        Properties::globals[key] = scale / 1.5;
      active->redraw = true;
      printMessage("%s scaling set to %f", what.c_str(), (float)Properties::globals[key]);
    }
    else
    {
      //X,Y,Z: geometry scaling
      if (what == "x")
      {
        if (parsed.has(fval, "scale", 1))
        {
          aview->setScale(fval, 1, 1, true); //Replace existing
          amodel->redraw();
        }
      }
      else if (what == "y")
      {
        if (parsed.has(fval, "scale", 1))
        {
          aview->setScale(1, fval, 1, true); //Replace existing
          amodel->redraw();
        }
      }
      else if (what == "z")
      {
        if (parsed.has(fval, "scale", 1))
        {
          aview->setScale(1, 1, fval, true); //Replace existing
          amodel->redraw();
        }
      }
      else if (what == "all" && parsed.has(fval, "scale", 1))
      {
        //Scale everything
        aview->setScale(fval, fval, fval, true); //Replace existing
        amodel->redraw();
      }
      else
      {
        //Scale by name/ID match in all drawing objects
        DrawingObject* obj = aobject;
        int next = 0;
        if (!obj)
        {
          obj = lookupObject(parsed, "scale");
          next++;
        }
        if (obj)
        {
          float sc = obj->properties["scaling"];
          if (sc <= 0.0) sc = 1.0;
          if (parsed.has(fval, "scale", next))
            obj->properties.data["scaling"] = fval;
          else if (parsed.get("scale", next) == "up")
            obj->properties.data["scaling"] = sc * 1.5;
          else if (parsed.get("scale", next) == "down")
            obj->properties.data["scaling"] = sc / 1.5;
          printMessage("%s scaling set to %f", obj->name().c_str(), (float)obj->properties["scaling"]);
          for (int type=lucMinType; type<lucMaxType; type++)
            Model::geometry[type]->redraw = true;
          redraw(obj); //Full reload of object by id
          amodel->redraw();
        }
      }
    }
  }
  else if (parsed.exists("history"))
  {
    if (gethelp)
    {
      help += "> Write command history to output (eg: terminal)\n\n"
              "> **Usage:** history [filename]\n\n"
              "> filename (string) : optional file to write data to  \n";
      return false;
    }

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
    return true; //No record
  }
  else if (parsed.exists("clearhistory"))
  {
    if (gethelp)
    {
      help += "> Clear command history  \n";
      return false;
    }

    history.clear();
    return false; //Skip record
  }
  else if (parsed.has(ival, "pause"))
  {
    if (gethelp)
    {
      help += "> Pause execution (for scripting)\n\n"
              "> **Usage:** pause msecs\n\n"
              "> msecs (integer) : how long to pause for in milliseconds  \n";
      return false;
    }

    PAUSE(ival);
  }
  else if (parsed.exists("delete"))
  {
    if (gethelp)
    {
      help += "> Delete objects  \n"
              "> **Usage:** delete object\n\n"
              "> object (integer/string) : the index or name of the object to delete (see: \"list objects\")  \n";
      return false;
    }

    //Delete drawing object by name/ID match
    std::vector<DrawingObject*> list = lookupObjects(parsed, "delete");
    for (unsigned int c=0; c<list.size(); c++)
    {
      printMessage("%s deleted", list[c]->name().c_str());
      //Delete geometry
      for (unsigned int i=0; i < Model::geometry.size(); i++)
        Model::geometry[i]->remove(list[c]);
      //Delete from model obj list
      for (unsigned int i=0; i<amodel->objects.size(); i++)
      {
        if (list[c] == amodel->objects[i])
        {
          amodel->objects.erase(amodel->objects.begin()+i);
          break;
        }
      }
      //Delete from viewport obj list
      for (unsigned int i=0; i<aview->objects.size(); i++)
      {
        if (list[c] == aview->objects[i])
        {
          aview->objects.erase(aview->objects.begin()+i);
          break;
        }
      }
      //Free memory
      delete list[c];
      amodel->redraw(true); //Redraw & reload
    }
  }
  else if (parsed.exists("deletedb"))
  {
    if (gethelp)
    {
      help += "> Delete objects from database  \n"
              "> WARNING: This is irreversible! Backup your database before using!\n\n"
              "> **Usage:** delete object_id\n\n"
              "> object (integer/string) : the index or name of the object to delete (see: \"list objects\")  \n";
      return false;
    }

    std::vector<DrawingObject*> list = lookupObjects(parsed, "deletedb");
    for (unsigned int c=0; c<list.size(); c++)
    {
      if (list[c]->dbid == 0) continue;
      amodel->deleteObject(list[c]->dbid);
      printMessage("%s deleted from database", list[c]->name().c_str());
      //Delete the loaded object data
      parseCommand("delete " + list[c]->name());
    }
  }
  else if (parsed.exists("merge"))
  {
    if (gethelp)
    {
      help += "> Merge data from attached databases into main (warning: backup first!)  \n";
      return false;
    }

    amodel->mergeDatabases();
    parseCommands("quit");
  }
  else if (parsed.exists("load"))
  {
    if (gethelp)
    {
      help += "> Load objects from database  \n"
              "> Used when running with deferred loading (-N command line switch)\n\n"
              "> **Usage:** load object\n\n"
              "> object (integer/string) : the index or name of the object to load (see: \"list objects\")  \n";
      return false;
    }

    //Load drawing object by name/ID match
    std::vector<DrawingObject*> list = lookupObjects(parsed, "load");
    for (unsigned int c=0; c<list.size(); c++)
    {
      if (list[c]->dbid == 0) continue;
      amodel->loadGeometry(list[c]->dbid);
    }
    //Update the views
    resetViews(false);
    amodel->redraw(true); //Redraw & reload
    //Delete the cache as object list changed
    amodel->deleteCache();
  }
  else if (parsed.exists("sort"))
  {
    if (gethelp)
    {
      help += "> Sort geometry on demand (with idle timer)\n\n"
              "> **Usage:** sort on/off/timer\n\n"
              "> on : (default) sort after model rotation  \n"
              "> off : no sorting  \n"
              "> timer : sort after 1.5 second timeout  \n"
              "> If no options passed, flags re-sort required  \n";
      return false;
    }

    if (parsed["sort"] == "off")
    {
      //Disables all sorting
      sort_on_rotate = false;
      aview->sort = false;
      printMessage("Geometry sorting has been disabled");
    }
    else if (parsed["sort"] == "timer")
    {
      //Disables sort on rotate mode
      sort_on_rotate = false;
      //Enables sort on timer
      aview->sort = true;
      viewer->idleTimer(TIMER_IDLE); //Start idle redisplay timer (default 1.5 seconds)
      printMessage("Sort geometry on timer instead of rotation enabled");
    }
    else if (parsed["sort"] == "on")
    {
      //Enables sort on rotate mode
      sort_on_rotate = true;
      //Disables sort on timer
      aview->sort = false;
      viewer->idleTimer(0); //Stop/disable idle redisplay timer
      printMessage("Sort geometry on rotation enabled");
    }
    else
      //Flag rotated
      aview->rotated = true;
  }
  else if (parsed.exists("idle"))
  {
    //Internal usage, no help
    if (gethelp) return false;

    if (!sort_on_rotate && aview->rotated)
      aview->sort = true;
    //Command playback
    if (repeat != 0)
    {
      //Playback
      for (unsigned int l=0; l<replay.size(); l++)
        parseCommands(replay[l]);
      repeat--;
      if (repeat == 0)
      {
        viewer->idleTimer(0); //Disable idle redisplay timer
        replay.clear();
      }
    }
    return true; //Skip record
  }
  //Special commands for passing keyboard/mouse actions directly (web server mode)
  else if (parsed.exists("mouse"))
  {
    //Internal usage, no help
    if (gethelp) return false;

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
    //Internal usage, no help
    if (gethelp) return false;

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
  else if (parsed.exists("add"))
  {
    if (gethelp)
    {
      help += "> Add a new object and select as the active object\n\n"
              "> **Usage:** add [object_nam] [data_type]\n\n"
              "> object_name (string) : optional name of the object to add  \n"
              "> data_type (string) : optional name of the default data type  \n"
              "> (points/labels/vectors/tracers/triangles/quads/shapes/lines)  \n";
      return false;
    }

    std::string name = parsed["add"];
    aobject = addObject(new DrawingObject(name));
    if (aobject)
    {
      std::string type = parsed.get("add", 1);
      if (type.length() > 0) aobject->properties.data["geometry"] = type;
      printMessage("Added object: %s", aobject->name().c_str());
    }
  }
  else if (parsed.exists("select"))
  {
    if (gethelp)
    {
      help += "> Select object as the active object  \n"
              "> Used for setting properties of objects  \n"
              "> following commands that take an object id will no longer require one)\n\n"
              "> **Usage:** select object\n\n"
              "> object (integer/string) : the index or name of the object to select (see: \"list objects\")  \n"
              "> Leave object parameter empty to clear selection.  \n";
      return false;
    }

    aobject = lookupObject(parsed, "select");
    if (aobject)
      printMessage("Selected object: %s", aobject->name().c_str());
    else
      printMessage("Object selection cleared");
  }
  else if (parsed.exists("shaders"))
  {
    if (gethelp)
    {
      help += "> Reload shader program files from disk  \n";
      return false;
    }

    printMessage("Reloading shaders");
    reloadShaders();
    return false;
  }
  else if (parsed.exists("blend"))
  {
    if (gethelp)
    {
      help += "> Select blending type\n\n"
              "> **Usage:** blend mode\n\n"
              "> mode (string) : normal: multiplicative, add: additive, png: premultiplied for png output  \n";
      return false;
    }

    std::string what = parsed["blend"];
    if (what == "png")
      viewer->blend_mode = BLEND_PNG;
    else if (what == "add")
      viewer->blend_mode = BLEND_ADD;
    else
      viewer->blend_mode = BLEND_NORMAL;
  }
  else if (parsed.exists("props"))
  {
    if (gethelp)
    {
      help += "> Print properties on active object or global and view if none selected  \n";
      return false;
    }

    printProperties();
  }
  else if (parsed.exists("defaults"))
  {
    if (gethelp)
    {
      help += "> Print all available properties and their default values  \n";
      return false;
    }

    printDefaultProperties();
  }
  else if (parsed.exists("test"))
  {
    if (gethelp)
    {
      help += "> Create and display some test data, points, lines, quads  \n";
      return false;
    }

    if (!parsed.has(ival, "test")) ival = 200000;
    createDemoModel(ival);
    resetViews();
  }
  else if (parsed.exists("voltest"))
  {
    if (gethelp)
    {
      help += "> Create and display a test volume dataset  \n";
      return false;
    }

    createDemoVolume();
    resetViews();
  }
  else if (parsed.exists("name"))
  {
    if (gethelp)
    {
      help += "> Set object name\n\n"
              "> **Usage:** name object newname\n\n"
              "> object (integer/string) : the index or name of the object to rename (see: \"list objects\")  \n"
              "> newname (string) : new name to apply  \n";
      return false;
    }

    DrawingObject* obj = aobject;
    int next = 0;
    if (!obj)
    {
      obj = lookupObject(parsed, "name");
      next++;
    }
    if (obj)
    {
      std::string name = parsed.get("name", next);
      if (name.length() > 0)
      {
        obj->name() = name;
        printMessage("Renamed object: %s", obj->name().c_str());
      }
    }
  }
  else if (parsed.exists("newstep"))
  {
    if (gethelp)
    {
      help += "> Add a new timestep after the current one  \n";
      return false;
    }

    amodel->addTimeStep(amodel->step()+1);
    amodel->setTimeStep(Model::now+1);

    //Don't record
    return false;
  }
  else if (parsed.exists("filter") || parsed.exists("filterout"))
  {
    if (gethelp)
    {
      help += "> Set a data filter on selected object\n\n"
              "> **Usage:** filter index min max [range]\n\n"
              "> index (integer) : the index of the data set to filter on (see: \"list data\")  \n"
              "> min (number) : the minimum value of the range to filter in or out  \n"
              "> max (number) : the maximum value of the range to filter in or out  \n"
              "> map (literal) : add map keyword for min/max [0,1] on available data range  \n"
              ">                   eg: 0.1-0.9 will filter the lowest and highest 10% of values  \n";
      return false;
    }

    //Require a selected object
    if (aobject)
    {
      json filter;
      std::string action = "filter";
      if (parsed.exists("filterout"))
      {
        action = "filterout";
        filter["out"] = true;
      }
      filter["by"] = parsed.Int(action, 0);
      float min, max;
      parsed.has(min, action, 1);
      parsed.has(max, action, 2);
      filter["minimum"] = min;
      filter["maximum"] = max;
      //Range keyword defines a filter applied over the range of available values not literal values
      //eg: 0.1-0.9 will filter out the lowest and highest 10% of values
      bool map = (parsed.get(action, 3) == "map");
      filter["map"] = map;
      filter["inclusive"] = false;
      aobject->properties.data["filters"].push_back(filter);
      printMessage("%s filter on value index %d from %f to %f", (map ? "Map" : "Value"), (int)filter["index"], min, max);
      amodel->redraw(true); //Force reload
    }
  }
  else if (parsed.exists("clearfilters"))
  {
    if (gethelp)
    {
      help += "> Clear filters on selected object  \n";
      return false;
    }

    //Require a selected object
    if (aobject)
    {
      printMessage("Filters cleared on object %s", aobject->name().c_str());
      aobject->properties.data.erase("filters");
      amodel->redraw(true); //Force reload
    }
  }
  else if (parsed.exists("freeze"))
  {
    if (gethelp)
    {
      help += "> Fix currently loaded geometry as not time-varying\n\n"
              "> This data will then exist for all time steps\n";
      return false;
    }

    amodel->freeze();
  }
  else
  {
    //If value parses as integer and contains nothing else
    //interpret as timestep jump
    ival = atoi(cmd.c_str());
    std::ostringstream oss;
    oss << ival;
    if (oss.str() == cmd && amodel->setTimeStep(amodel->nearestTimeStep(ival)) >= 0)
    {
      printMessage("Go to timestep %d", amodel->step());
      resetViews(); //Update the viewports
    }
    else if (parsePropertySet(cmd))
    {
      redisplay = true; //Redisplay after prop change
    }
    else
    {
      if (verbose) std::cerr << "# Unrecognised command or file not found: \"" << cmd << "\"" << std::endl;
      return false;  //Invalid
    }
  }

  last_cmd = cmd;
  if (animate && redisplay) viewer->postdisplay = true;
  return redisplay;
}

bool LavaVu::parsePropertySet(std::string cmd)
{
  std::size_t found = cmd.find("=");
  json jval;
  if (found == std::string::npos) return false;
  parseProperty(cmd);
  if (aobject) redraw(aobject);
  return true;
}

void LavaVu::helpCommand(std::string cmd)
{
  //This list of categories and commands must be maintained along with the individual command help strings
  std::vector<std::string> categories = {"General", "Input", "Output", "View/Camera", "Object", "Display", "Scripting", "Miscellanious"};
  std::vector<std::vector<std::string> > cmdlist = {
    {"quit", "repeat", "animate", "history", "clearhistory", "pause", "list", "timestep", "jump", "model", "reload", "clear"},
    {"file", "script", "figure", "view", "scan"},
    {"image", "images", "outwidth", "outheight", "movie", "export", "save"},
    {"rotate", "rotatex", "rotatey", "rotatez", "rotation", "zoom", "translate", "translatex", "translatey", "translatez",
     "focus", "aperture", "focallength", "eyeseparation", "nearclip", "farclip", "zoomclip", "zerocam", "reset", "bounds", "camera",
     "resize", "fullscreen", "fit", "autozoom", "stereo", "coordsystem", "sort", "rotation", "translation"},
    {"hide", "show", "delete", "load", "select", "add", "read", "name",
     "vertex", "normal", "vector", "value", "colour"},
    {"background", "alpha", "axis", "scaling", "rulers",
     "antialias", "valuerange", "lockscale", "colourmap", "pointtype",
     "pointsample", "border", "title", "scale", "modelscale"},
    {"next", "play", "stop", "open", "interactive"},
    {"shaders", "blend", "props", "defaults", "test", "voltest", "newstep", "filter", "filterout", "clearfilters",
     "cache", "verbose", "toggle", "createvolume"}
  };

  //Verbose command help
  if (cmd == "help")
  {
    help += "For detailed help on a command, type:\n\nhelp command [ENTER]";
    for (unsigned int i=0; i<categories.size(); i++)
    {
      help += "\n\n" + categories[i] + " commands:\n\n";
      for (unsigned int j=0; j<cmdlist[i].size(); j++)
      {
        if (j % 11 == 10) help += "\n  ";
        help += cmdlist[i][j];
        if (j < cmdlist[i].size() - 1) help += ", ";
      }
    }
  }
  else if (cmd == "docs:scripting")
  {
    std::cout << "\n##Scripting command reference\n\n";
    for (unsigned int i=0; i<categories.size(); i++)
    {
      std::cout <<  "\n---\n###" + categories[i] + " commands:\n\n";
      for (unsigned int j=0; j<cmdlist[i].size(); j++)
      {
        help = "\n\n####" + cmdlist[i][j] + "\n\n";
        helpCommand(cmdlist[i][j]);
        std::cout << help;
      }
    }
  }
  else
  {
    parseCommand(cmd + " 1.0", true);
  }
}

