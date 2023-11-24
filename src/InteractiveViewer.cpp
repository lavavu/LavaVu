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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define HELP_INTERACTION "\
\n# User Interface commands\n\n\
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
[space]      Play/pause animation or stop recording\n\
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
[u] [ENTER]  hide/show all triangle surfaces\n\
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
  message[0] = '\0';
  //Only process on mouse release
  static bool translated = false;
  bool redraw = false;
  int scroll = 0;

  if (down)
  {
    translated = false;
    //if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
#ifdef __EMSCRIPTEN__
    //Skip false events when using touchscreen
    if (!x && !y) return false;
#endif
    viewer->button = btn;
    viewer->lastX = x;
    viewer->lastY = y;

#ifdef __EMSCRIPTEN__
  //Ondrag mode switched in gui
  if (viewer->button == LeftButton)
  {
    ondrag = EM_ASM_INT({
      //console.log(window.viewer.mode);
      if (window.viewer.mode == "Translate") return 1;
      if (window.viewer.mode == "Zoom") return 2;
      return 0;
    });
  }
#endif


  }
  else
  {
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
      redraw = true;
      break;
    case MiddleButton:
      break;
    case RightButton:
      if (!viewer->keyState.alt && !viewer->keyState.shift)
        translated = true;
    default:
      break;
    }

    if (scroll)
      mouseScroll(scroll);

    //Update cam move in history
    if (translated) history.push_back(aview->translateString());
    if (aview->rotated) history.push_back(aview->rotateString());

    viewer->button = NoButton;
  }

  if (!down)
    viewer->idle = 0; //Reset idle timer

  if (redraw) gui_sync();
  return redraw;
}

bool LavaVu::mouseMove(int x, int y)
{
#ifdef __EMSCRIPTEN__
  //Skip false events when using touchscreen
  if (!x && !y) return false;
#endif
  viewer->mouseX = x;
  viewer->mouseY = y;
  if (!viewer->mouseState) return false;

  float adjust;
  int dx = x - viewer->lastX;
  int dy = y - viewer->lastY;
  viewer->lastX = x;
  viewer->lastY = y;
  if (viewer->button)
    viewer->idle = 0; //Reset idle timer

  //For mice with no right button, ctrl+left
  if (viewer->keyState.ctrl && viewer->button == LeftButton)
    viewer->button = RightButton;

  //Drag mode switched
  if (viewer->button == LeftButton && ondrag == 1)
    viewer->button = RightButton;
  if (viewer->button == LeftButton && ondrag == 2)
    viewer->button = WheelUp;

  switch (viewer->button)
  {
  case LeftButton:
    if (viewer->keyState.alt || viewer->keyState.shift)
      //Mac glut scroll-wheel alternative
      history.push_back(aview->zoom((-dy+dx) * 0.01));
    else
    {
      // left = rotate
      aview->rotate(dy / 5.0f, dx / 5.0, 0.0f);
    }
    break;
  case RightButton:
    if (viewer->keyState.alt || viewer->keyState.shift)
      //Mac glut scroll-wheel alternative
      history.push_back(aview->zoomClip((-dy+dx) * 0.001));
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
  case WheelUp:
    // zoom (ondrag leftbutton)
    adjust = aview->model_size / 1000.0;   //1/1000th of size
    aview->translate(0, 0, dx * adjust);
    break;

  default:
    return false;
  }

  return true;
}

bool LavaVu::mouseScroll(float scroll)
{
  message[0] = '\0';
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
  //ALT = adjust field of view
  else if (viewer->keyState.alt)
    history.push_back(aview->adjustStereo(scroll, 0, 0));
  else if (viewer->keyState.ctrl)
    //Slower zoom
    history.push_back(aview->zoom(scroll * 0.1/sqrt(viewer->idle+1)));
  else
    //Default zoom
    history.push_back(aview->zoom(scroll * 0.25/sqrt(viewer->idle+1)));

  viewer->idle = 0; //Reset idle timer

  gui_sync();

  return true;
}

bool LavaVu::keyPress(unsigned char key, int x, int y)
{
  viewer->idle = 0; //Reset idle timer
  //if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
  return parseChar(key);
}


bool LavaVu::toggleType(const std::string& name)
{
  std::vector<Geometry*> activelist = amodel->getRenderersByTypeName(name);
  //Just use the first valid to check current state for toggle
  if (activelist.size() > 0)
  {
    if (activelist[0]->allhidden)
      return parseCommands("show " + name);
    else
      return parseCommands("hide " + name);
  }
  return false;
}

bool LavaVu::parseChar(unsigned char key)
{
  int hline = historyline;
  if (hline < 0) hline = linehistory.size();
  bool response = true;
  bool msg = false;
  historyline = -1;

  //Clear any on screen status/help text
  message[0] = '\0';
  help = "";

  //ALT+SHIFT commands
  if (viewer->keyState.alt && viewer->keyState.shift)
  {
    switch(key)
    {
    case 'r':
    case 'R':
      return parseCommands("reload");
    case 'b':
    case 'B':
      return parseCommands("background");
    case 's':
    case 'S':
      return parseCommands("scale shapes down");
    case 'v':
    case 'V':
      return parseCommands("scale vectors down");
    case 't':
    case 'T':
      return parseCommands("scale tracers down");
    case 'p':
    case 'P':
      return parseCommands("scale points down");
    case 'l':
    case 'L':
      return parseCommands("scale lines down");
    case '\\':
    case '|':
      return parseCommands("rulers");
    case '=':
      return parseCommands("pointsample up");
    default:
      return false;
    }
  }
  //ALT commands
  else if (viewer->keyState.alt)
  {
    switch(key)
    {
    case KEY_UP:
      return parseCommands("model up");
    case KEY_DOWN:
      return parseCommands("model down");
    case '8':
      return parseCommands("autozoom");
    case '/':
      return parseCommands("stereo");
    case '\\':
      return parseCommands("coordsystem");
    case ',':
      return parseCommands("pointtype all");
    case '-':
      return parseCommands("pointsample down");
    case '=':
      return parseCommands("pointsample up");
    case 'a':
    case 'A':
      return parseCommands("axis");
    case 'b':
    case 'B':
      return parseCommands("background invert");
    case 'c':
    case 'C':
      return parseCommands("camera");
    case 'e':
    case 'E':
      return parseCommands("list elements");
    case 'f':
    case 'F':
      return parseCommands("border");
    case 'h':
    case 'H':
      return parseCommands("history");
    case 'i':
    case 'I':
      return parseCommands("image");
    case 'j':
    case 'J':
      return parseCommands("valuerange");
    case 'o':
    case 'O':
      return parseCommands("list objects");
    case 'q':
    case 'Q':
      return parseCommands("quit");
    case 'r':
    case 'R':
      return parseCommands("reset");
    case 'u':
    case 'U':
      return parseCommands("toggle cullface");
    case 'w':
    case 'W':
      parseCommands("toggle wireframe");
      return true;
    case 'p':
    case 'P':
      return parseCommands("scale points up");
    case 's':
    case 'S':
      return parseCommands("scale shapes up");
    case 't':
    case 'T':
      return parseCommands("scale tracers up");
    case 'v':
    case 'V':
      return parseCommands("scale vectors up");
    case 'l':
    case 'L':
      return parseCommands("scale lines up");
    case ' ':
      if (encoder)
        return parseCommands("record"); //Stop recording
      if (viewer->timeloop)
        return parseCommands("stop");
      else
        return parseCommands("play");
    default:
      return false;
    }
  }
  //CTRL commands
  else if (viewer->keyState.ctrl)
  {
    //Skip these, usually built in / browser shortcuts
    return false;
  }

  //Direct commands and verbose command entry
  switch (key)
  {
  case KEY_RIGHT:
    return parseCommands("timestep +");
  case KEY_LEFT:
    return parseCommands("timestep -");
  case KEY_ESC:
    //Don't quit immediately on ESC by request, if entry, then clear, 
    msg = true;
    if (entry.length() == 0)
      entry = "quit";
    else
      entry = "";
    break;
  case KEY_ENTER:
    if (entry.length() == 0 || entry == "repeat")
    {
      entry = "";
      return parseCommands("repeat");
    }
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
      case 'p':
        toggleType("points");
        break;
      case 'v':
        toggleType("vectors");
        break;
      case 't':
        toggleType("tracers");
        break;
      case 'u':
        toggleType("triangles");
        break;
      case 'q':
        toggleType("quads");
        break;
      case 's':
        toggleType("shapes");
        break;
      case 'l':
        toggleType("lines");
        break;
      case 'o':
        toggleType("volumes");
        break;
      default:
        //Parse as if entered with ALT
        viewer->keyState.alt = true;
        response = parseChar(ck);
      }
      entry = "";
    }
    else
    {
      //Execute
      response = parseCommands(entry);
      //Add to linehistory if not a duplicate of previous entry
      if (last_cmd == entry) //If last_cmd does not match, returned early so skip
      {
        int lend = linehistory.size()-1;
        if (lend < 0 || entry != linehistory[lend])
          linehistory.push_back(entry);
      }
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

DrawingObject* LavaVu::lookupObject(PropertyParser& parsed, const std::string& key, int idx)
{
  //Try index(id) first
  if (!amodel) return NULL;
  int id = parsed.Int(key, -1, idx);
  if (id > 0 && id <= (int)amodel->objects.size()) return amodel->objects[id-1];

  //Otherwise lookup by name
  std::string what;
  if (idx==0)
    //(using getall to allow spaces without quotes on first iteration only)
    what = parsed.getall(key);
  else
    //Using get() requires quotes for spaces but handles multiple strings on line
    what = parsed.get(key, idx);
  return amodel->findObject(what);
}

std::vector<DrawingObject*> LavaVu::lookupObjects(PropertyParser& parsed, const std::string& key, int start)
{
  std::vector<DrawingObject*> list;
  for (int c=0; c<20; c++) //Allow multiple id/name specs on line (up to 20)
  {
    DrawingObject* obj = lookupObject(parsed, key, c+start);
    if (obj)
      list.push_back(obj);
  }
  return list;
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

  //Json, ignore if truncated
  if (cmd.at(0) == '{' && cmd.length() > 100)
  {
    int r = amodel->jsonRead(cmd);
    applyReload(NULL, r);
    return true;
  }

  //Replace semi-colon with newlines (multiple commands on single line)
  std::replace(cmd.begin(), cmd.end(), ';', '\n');

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
  if (viewer->isopen && !gethelp)
    viewer->display(false); //Display without redraw, ensures correct context active
  //Trim leading whitespace
  size_t pos = cmd.find_first_not_of(" \t\n\r");
  if (std::string::npos != pos) cmd = cmd.substr(pos);
  bool redisplay = true;

  //Skip comments or empty lines
  if (cmd.length() == 0) return false;

  //Select shortcuts
  if (cmd.at(0) == '#')
  {
    //## to de-select
    if (cmd.length() == 2 && cmd.at(1) == '#')
      return parseCommand("select");
    //# followed by valid object id, select it
    std::string s = cmd.substr(1);
    std::stringstream ss(s);
    unsigned int id;
    if (amodel && ss >> id && id > 0 && id <= amodel->objects.size())
      return parseCommand("select " + s);
    return false;
  }

  //Save in history
  if (!gethelp && cmd != "display" && cmd != "asyncsort" && cmd != "history" && cmd != "idle")
  {
    history.push_back(cmd);
    if (!gethelp && verbose) std::cerr << "CMD: " << cmd << std::endl;
  }

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

  if (!gethelp)
    GL_Check_Thread(viewer->render_thread);

  //Parse the line
  PropertyParser parsed = PropertyParser();
  parsed.parseLine(cmd);

  //Check for commands that require viewer to be open and view initialised,
  //if found, open the viewer / setup view first
  if (!gethelp && viewer->isopen && aview && !aview->initialised)
  //Fails on MacOS if viewer not open (isopen=false), can't open viewer from here
  //if (!gethelp && (!viewer->isopen || (aview && !aview->initialised)))
  {
    std::vector<std::string> viewcmds = commandList("View");

    for (auto c : viewcmds)
    {
      if (parsed.exists(c))
      {
        debug_print("%s : Camera command requires view to be initialised, performing auto-init\n", c.c_str());
        loadModelStep(0, 0, true);
        resetViews(); //Forces bounding box update
      }
    }
  }

  //Verbose command processor
  float fval;
  int ival;

  //******************************************************************************
  //First check for settings commands that don't require a model to be loaded yet!
  if (parsed.exists("docs:scripting"))
  {
    help = helpCommand("docs:scripting");
    return false;
  }
  else if (parsed.exists("docs:interaction"))
  {
    std::cout << HELP_INTERACTION;
    return false;
  }
  if (parsed.exists("docs:properties"))
  {
    help = helpCommand("docs:properties");
    return false;
  }
  else if (parsed.exists("file"))
  {
    if (gethelp)
    {
      help += "Load database or model file\n"
              "**Usage:** file \"filename\"\n\n"
              "filename (string) : the path to the file, quotes necessary if path contains spaces\n";
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
      help += "Run script file\n"
              "Load a saved script of viewer commands from a file\n\n"
              "**Usage:** script filename\n\n"
              "filename (string) : path and name of the script file to load\n";
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
  else if (parsed.exists("verbose"))
  {
    if (gethelp)
    {
      help += "Switch verbose output mode on/off\n\n"
              "**Usage:** verbose [off]\n";
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
      help += "Create a static volume object which will receive all following volumes as timesteps\n";
      return false;
    }

    //Use this to load multiple volumes as timesteps into the same object
    volume = new DrawingObject(session, "volume");
    printMessage("Created static volume object");
  }
  else if (parsed.exists("clearvolume"))
  {
    if (gethelp)
    {
      help += "Clear global static volume object, any future volume loads will create a new object automatically\n";
      return false;
    }

    //Use this to load multiple image-stack volumes as separate objects
    volume = NULL;
    printMessage("Cleared static volume object");
  }
  else if (parsed.exists("texture"))
  {
    if (gethelp)
    {
      help += "Set or clear object texture data\n";
      return false;
    }
    if (aobject)
    {
      std::string props = parsed.get("texture");
      if (props.length() > 0)
      {
        aobject->properties.data["texture"] = props;
        printMessage("Set texture on object: %s to %s", aobject->name().c_str(), props.c_str());
      }
      else
      {
        clearTexture(aobject);
        printMessage("Cleared texture on object: %s", aobject->name().c_str());
      }
    }
  }
  else if (parsed.exists("populate"))
  {
    if (gethelp)
    {
      help += "Populate generated data set\n\n"
              "**Usage:** populate label\n\n"
              "label (string) : the label of the built in data to copy to a value data set, one of:\n"
              "                 x,y,z,index,count,width,height,length,size,x0,y0,z0,x1,y1,z1,magnitude,nx,ny,nz,nmean\n";
      return false;
    }

    if (aobject && amodel)
    {
      std::string label = parsed["populate"];
      //Calculate data array for a generated/preset label
      for (auto g : amodel->geometry)
      {
        std::vector<Geom_Ptr> geomlist = g->getAllObjects(aobject);
        for (auto geom : geomlist)
          geom->valuesLookup(label);
      }
      printMessage("Populate %s complete", label.c_str());
    }
  }
  else if (parsed.has(fval, "alpha") || parsed.has(fval, "opacity"))
  {
    std::string action = parsed.exists("alpha") ? "alpha" : "opacity";
    if (gethelp)
    {
      help += "Set global transparency value\n\n"
              "**Usage:** opacity/alpha value\n\n"
              "'opacity' is global default, applies to all objects without own opacity setting\n"
              "'alpha' is global override, is combined with opacity setting of all objects\n"
              "value (integer > 1) : sets opacity as integer in range [1,255] where 255 is fully opaque\n"
              "value (number [0,1]) : sets opacity as real number in range [0,1] where 1.0 is fully opaque\n";
      return false;
    }

    float value = session.global(action);
    if (value == 0.0) value = 1.0;
    if (fval > 1.0)
      value = _CTOF(fval);
    else
      value = fval;
    session.globals[action] = value;
    printMessage("Set global %s to %.2f", action.c_str(), value);
  }
  else if (parsed.exists("interactive"))
  {
    if (gethelp)
    {
      help += "Switch to interactive mode, for use in scripts\n\n"
              "**Usage:** interactive [noshow]\n\n"
              "options (string) : \"noshow\" default is to show visible interactive window, set this to activate event loop\n"
              "                   without showing window, can use browser interactive mode\n"
              "                   \"noloop\" : set this option to show the interactive window only,\n"
              "                   and handle events outside on returning\n";
      return false;
    }

    viewer->quitProgram = false;
    bool interactive = true;
    std::string opt = parsed["interactive"];
    if (opt == "noshow")
      interactive = false;
    //viewer->visible = interactive;
    if (opt == "noloop")
        return parseCommands("animate 20");
    else
    {
      //parseCommands("animate 1");
      viewer->loop(interactive);
      viewer->visible = false;
      viewer->quitProgram = false;
    }
    return false;
  }
  else if (parsed.exists("open"))
  {
    if (gethelp)
    {
      help += "Open the viewer window if not already done, for use in scripts\n";
      return false;
    }

    loadModelStep(0, 0, true);
    resetViews(); //Forces bounding box update
  }
  else if (parsed.exists("resize"))
  {
    if (gethelp)
    {
      help += "Resize view window\n\n"
              "**Usage:** resize width height\n\n"
              "width (integer) : width in pixels\n"
              "height (integer) : height in pixels\n";
      return false;
    }

    float w = 0, h = 0;
    if (parsed.has(w, "resize", 0) && parsed.has(h, "resize", 1))
    {
      viewer->setsize(w, h);
      viewset = RESET_ZOOM; //Force check for resize and autozoom
    }
  }
  else if (parsed.exists("quit") || parsed.exists("exit"))
  {
    if (gethelp)
    {
      help += "Quit the program\n";
      return false;
    }

    viewer->quitProgram = true;
  }
  //******************************************************************************
  //Following commands require a model!
  else if (!gethelp && (!amodel || !aview))
  {
    //Attempt to parse as property=value first
    if (parseProperty(cmd, aobject)) return true;
    if (verbose) std::cerr << "Model/View required to execute command: " << cmd << ", deferred" << std::endl;
    //Add to the queue to be processed once open
    queueCommands(cmd);
    return false;
  }
  else if (parsed.exists("cache"))
  {
    if (gethelp)
    {
      help += "Load all timesteps into cache\n\n"
              "**Usage:** cache\n\n";
      return false;
    }
    amodel->cacheLoad();
  }
  else if (parsed.exists("save"))
  {
    if (gethelp)
    {
      help += "Export all settings as json state file that can be reloaded later\n\n"
              "**Usage:** save [\"filename\"]\n\n"
              "file (string) : name of file to import\n"
              "If filename omitted and database loaded, will save the state\n"
              "to the active figure in the database instead\n";
      return false;
    }

    //Export json settings only (no object data)
    std::string what = parsed["save"];
    if (what.length() == 0 && amodel->database)
    {
      amodel->database.reopen(true);  //Open writable
      amodel->writeState();
    }
    else
      jsonWriteFile(what, 0, false, false);
  }
  else if (parsed.exists("scan"))
  {
    if (gethelp)
    {
      help += "Rescan the current directory for timestep database files\n"
              "based on currently loaded filename\n\n";
      return false;
    }

    amodel->loadTimeSteps(true);
  }
  else if (parsed.exists("rotation"))
  {
    if (gethelp)
    {
      help += "Set model rotation (replaces any previous rotation)\n\n"
              "**Usage:** rotation x y z [w]\n\n"
              "x (number) : rotation x component\n"
              "y (number) : rotation y component\n"
              "z (number) : rotation z component\n"
              "w (number) : rotation w component\n"
              "If only x,y,z are provided they determine the rotation\n"
              "about these axis (roll/pitch/yaw)\n"
              "If x,y,z,w are provided, the rotation is loaded\n"
              "as a quaternion with these components\n\n";
      return false;
    }

    float x = 0, y = 0, z = 0, w = 0;
    if (parsed.has(x, "rotation", 0) &&
        parsed.has(y, "rotation", 1) &&
        parsed.has(z, "rotation", 2))
    {
      if (parsed.has(w, "rotation", 3))
        aview->setRotation(x, y, z, w);
      else
        aview->setRotation(x, y, z);
    }
  }
  else if (parsed.exists("translation"))
  {
    if (gethelp)
    {
      help += "Set model translation in 3d coords (replaces any previous translation)\n\n"
              "**Usage:** translation x y z\n\n"
              "x (number) : x axis shift\n"
              "y (number) : y axis shift\n"
              "z (number) : z axis shift\n";
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
      help += "Rotate model\n\n"
              "**Usage:** rotate axis degrees\n\n"
              "axis (x/y/z) : axis of rotation 'x/y/z' or a series of 3 numbers / json array defining a 3d vector\n"
              "degrees (number) : degrees of rotation\n\n"
              "**Usage:** rotate x y z\n\n"
              "x (number) : x axis degrees of rotation\n"
              "y (number) : y axis degrees of rotation\n"
              "z (number) : z axis degrees of rotation\n";
      return false;
    }

    float x = 0, y = 0, z = 0, d = 0;
    std::string axis = parsed["rotate"];
    if (axis == "x")
    {
      if (!parsed.has(x, "rotate", 1)) return false;
      aview->rotate(x, 0, 0);
    }
    else if (axis == "y")
    {
      if (!parsed.has(y, "rotate", 1)) return false;
      aview->rotate(0, y, 0);
    }
    else if (axis == "z")
    {
      if (!parsed.has(z, "rotate", 1)) return false;
      aview->rotate(0, 0, z);
    }
    else if (parsed.has(x, "rotate", 0) && parsed.has(y, "rotate", 1) && parsed.has(z, "rotate", 2))
    {
      if (parsed.has(d, "rotate", 3))
        aview->rotate(d, Vec3d(x, y, z)); //d defines degrees, x,y,z define axis
      else
        aview->rotate(x, y, z);
    }
    else if (parsed.has(d, "rotate", 1))
    {
      // Parse without exceptions - required for emscripten
      json ax = json::parse(axis, nullptr, false);
      if (!ax.is_discarded())
        aview->rotate(d, Vec3d(ax[0], ax[1], ax[2]));
      else
        std::cout << "Error parsing rotation axis: " << axis << std::endl;
    }
  }
  else if (parsed.has(fval, "rotatex"))
  {
    if (gethelp)
    {
      help += "Rotate model about x-axis\n\n"
              "**Usage:** rotatex degrees\n\n"
              "degrees (number) : degrees of rotation\n";
      return false;
    }

    aview->rotate(fval, 0, 0);
  }
  else if (parsed.has(fval, "rotatey"))
  {
    if (gethelp)
    {
      help += "Rotate model about y-axis\n\n"
              "**Usage:** rotatey degrees\n\n"
              "degrees (number) : degrees of rotation\n";
      return false;
    }

    aview->rotate(0, fval, 0);
  }
  else if (parsed.has(fval, "rotatez"))
  {
    if (gethelp)
    {
      help += "Rotate model about z-axis\n\n"
              "**Usage:** rotatez degrees\n\n"
              "degrees (number) : degrees of rotation\n";
      return false;
    }

    aview->rotate(0, 0, fval);
  }
  else if (parsed.exists("autorotate"))
  {
    if (gethelp)
    {
      help += "Auto-Rotate model to face camera if flat in X or Y dimensions\n\n";
      return false;
    }

    aview->autoRotate();
  }
  else if (parsed.exists("spin"))
  {
    if (gethelp)
    {
      help += "Auto-spin model, animates\n\n"
              "**Usage:** spin axis degrees\n\n"
              "axis (x/y/z) : axis of rotation\n"
              "degrees (number) : degrees of rotation per frame\n\n";
      return false;
    }

    std::string rcmd = "rotate " + parsed.getall("spin", 0);
    parseCommands("animate 1");
    parseCommands(rcmd);
    parseCommands("repeat -1");
  }
  else if (parsed.has(fval, "zoom"))
  {
    if (gethelp)
    {
      help += "Translate model in Z direction (zoom)\n\n"
              "**Usage:** zoom units\n\n"
              "units (number) : distance to zoom, units are based on model size\n"
              "                 1 unit is approx the model bounding box size\n"
              "                 negative to zoom out, positive in\n";
      return false;
    }

    aview->zoom(fval);
  }
  else if (parsed.has(fval, "translatex"))
  {
    if (gethelp)
    {
      help += "Translate model in X direction\n\n"
              "**Usage:** translationx shift\n\n"
              "shift (number) : x axis shift\n";
      return false;
    }

    aview->translate(fval, 0, 0);
  }
  else if (parsed.has(fval, "translatey"))
  {
    if (gethelp)
    {
      help += "Translate model in Y direction\n\n"
              "**Usage:** translationy shift\n\n"
              "shift (number) : y axis shift\n";
      return false;
    }

    aview->translate(0, fval, 0);
  }
  else if (parsed.has(fval, "translatez"))
  {
    if (gethelp)
    {
      help += "Translate model in Z direction\n\n"
              "**Usage:** translationz shift\n\n"
              "shift (number) : z axis shift\n";
      return false;
    }

    aview->translate(0, 0, fval);
  }
  else if (parsed.exists("translate"))
  {
    if (gethelp)
    {
      help += "Translate model in 3 dimensions\n\n"
              "**Usage:** translate dir shift\n\n"
              "dir (x/y/z) : direction to translate\n"
              "shift (number) : amount of translation\n\n"
              "**Usage:** translation x y z\n\n"
              "x (number) : x axis shift\n"
              "y (number) : y axis shift\n"
              "z (number) : z axis shift\n";
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
      help += "Set model focal point in 3d coords\n\n"
              "**Usage:** focus x y z\n\n"
              "x (number) : x coord\n"
              "y (number) : y coord\n"
              "z (number) : z coord\n";
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
  else if (parsed.has(fval, "fov"))
  {
    if (gethelp)
    {
      help += "Set camera fov (field-of-view)\n\n"
              "**Usage:** fov degrees\n\n"
              "degrees (number) : degrees of viewing range\n";
      return false;
    }

    aview->adjustStereo(fval, 0, 0);
  }
  else if (parsed.has(fval, "focallength"))
  {
    if (gethelp)
    {
      help += "Set camera focal length (for stereo viewing)\n\n"
              "**Usage:** focallength len\n\n"
              "len (number) : distance from camera to focal point\n";
      return false;
    }

    aview->adjustStereo(0, fval, 0);
  }
  else if (parsed.has(fval, "eyeseparation"))
  {
    if (gethelp)
    {
      help += "Set camera stereo eye separation\n\n"
              "**Usage:** eyeseparation len\n\n"
              "len (number) : distance between left and right eye camera viewpoints\n";
      return false;
    }

    aview->adjustStereo(0, 0, fval);
  }
  else if (parsed.has(fval, "zoomclip"))
  {
    if (gethelp)
    {
      help += "Adjust near clip plane relative to current value\n\n"
              "**Usage:** zoomclip multiplier\n\n"
              "multiplier (number) : multiply current near clip plane by this value\n";
      return false;
    }

    aview->zoomClip(fval);
  }
  else if (parsed.has(fval, "nearclip"))
  {
    if (gethelp)
    {
      help += "Set near clip plane, before which geometry is not displayed\n\n"
              "**Usage:** nearclip dist\n\n"
              "dist (number) : distance from camera to near clip plane\n";
      return false;
    }

    aview->properties.data["near"] = fval;
  }
  else if (parsed.has(fval, "farclip"))
  {
    if (gethelp)
    {
      help += "Set far clip plane, beyond which geometry is not displayed\n\n"
              "**Usage:** farrclip dist\n\n"
              "dist (number) : distance from camera to far clip plane\n";
      return false;
    }

    aview->properties.data["far"] = fval;
  }
  else if (parsed.exists("timestep") || parsed.exists("step"))  //Absolute
  {
    std::string what = parsed.exists("timestep") ? "timestep" : "step";
    if (gethelp)
    {
      help += "Set timestep to view\n\n"
              "**Usage:** timestep -/+/value\n\n"
              "value (integer) : the timestep to view\n"
              "- : switch to previous timestep if available\n"
              "+ : switch to next timestep if available\n";
      return false;
    }
    if (!parsed.has(ival, what))
    {
      if (parsed[what] == "-")
        ival = session.now-1;
      else if (parsed[what] == "+")
        ival = session.now+1;
      else
        ival = session.now;
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
      help += "Jump from current timestep forward/backwards\n\n"
              "**Usage:** jump value\n\n"
              "value (integer) : how many timesteps to jump (negative for backwards)\n";
      return false;
    }

    //Relative jump
    if (amodel->setTimeStep(session.now+ival) >= 0)
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
      help += "Set model to view (when multiple models loaded)\n\n"
              "**Usage:** model up/down/value\n\n"
              "value (integer) : the model index to view [1,n]\n"
              "up : switch to previous model if available\n"
              "down : switch to next model if available\n"
              "add : add a new model\n";
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
    amodel->setTimeStep(session.now); //Reselect ensures all loaded correctly
    printMessage("Load model %d", model);
  }
  else if (parsed.exists("savefigure"))
  {
    if (gethelp)
    {
      help += "Store current vis settings in a figure\n\n"
              "**Usage:** figure name\n\n"
              "name (string) : the figure name to save\n"
              "                if ommitted, currently active or default figure will be saved\n"
              "                if it doesn't exist it will be created\n";
      return false;
    }

    std::string what = parsed["savefigure"];
    if (what.length() == 0)
    {
      //Save changes to currently selected
      amodel->storeFigure();
    }
    else
    {
      amodel->figure = -1;
      for (unsigned int i=0; i<amodel->fignames.size(); i++)
        if (amodel->fignames[i] == what) amodel->figure = i;
      //Not found? Create it
      if (amodel->figure < 0)
        amodel->figure = amodel->addFigure(what, "");
      else
        amodel->storeFigure();
    }

    printMessage("Saved figure %d", amodel->figure);
  }
  else if (parsed.exists("figure"))
  {
    if (gethelp)
    {
      help += "Set figure to view\n\n"
              "**Usage:** figure up/down/value\n\n"
              "value (integer/string) : the figure index or name to view\n"
              "up : switch to previous figure if available\n"
              "down : switch to next figure if available\n";
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
        std::string figname = parsed["figure"];
        for (unsigned int i=0; i<amodel->fignames.size(); i++)
          if (amodel->fignames[i] == figname) ival = i;
      }
    }

    if (ival >= 0)
    {
      //Save changes to currently selected first
      amodel->storeFigure();

      //Load new selection
      if (!amodel->loadFigure(ival)) return false; //Invalid
      viewset = RESET_ZOOM; //Force check for resize and autozoom
      printMessage("Load figure %d", amodel->figure);
    }
  }
  else if (parsed.exists("view"))
  {
    if (gethelp)
    {
      help += "Set view (when available)\n\n"
              "**Usage:** view up/down/value\n\n"
              "value (integer) : the view index to switch to\n"
              "up : switch to previous view if available\n"
              "down : switch to next view if available\n";
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
      help += "" + action + " objects/geometry types\n\n"
              "**Usage:** " + action + " object\n\n"
              "object (integer/string) : the index or name of the object to " + action + " (see: \"list objects\")\n\n"
              "**Usage:** " + action + " geometry_type id\n\n"
              "geometry_type : points/labels/vectors/tracers/triangles/quads/shapes/lines/volumes\n"
              "id (integer, optional) : id of geometry to " + action + "\n"
              "eg: 'hide points' will " + action + " all point data\n"
              "note: 'hide all' will " + action + " all objects\n";
      return false;
    }

    std::string what = parsed[action];
    std::vector<Geometry*> activelist = amodel->getRenderersByTypeName(what);
    std::vector<DrawingObject*> list = lookupObjects(parsed, action);
    //Trigger view redraw
    viewset = RESET_YES;
    //Have selected a geometry type?
    if (activelist.size())
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
          for (auto active : activelist)
          {
            if (action == "hide") active->hide(i);
            else active->show(i);
            printMessage("%s %s range %s", action.c_str(), what.c_str(), range.c_str());
          }
        }
      }
      else if (parsed.has(id, action, 1))
      {
        parsed.has(id, action, 1);
        for (auto active : activelist)
        {
          if (action == "hide")
          {
            if (!active->hide(id)) return false; //Invalid
          }
          else
          {
            if (!active->show(id)) return false; //Invalid
          }
        }
        printMessage("%s %s %d", action.c_str(), what.c_str(), id);
      }
      else
      {
        for (auto active : activelist)
          active->hideShowAll(action == "hide");
        printMessage("%s all %s", action.c_str(), what.c_str());
      }
    }
    else if (list.size())
    {
      //Hide/show by name/ID match in all drawing objects
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
          bool vis = (action == "show");
          if (vis)
          {
            //Only when "show" called, ensure all elements also set to visible
            //(Previously did the same for "hide" but this prevents using the visible property to unhide)
            for (auto g : amodel->geometry)
              g->showObj(list[c], vis);
          }
          list[c]->properties.data["visible"] = vis; //This allows hiding of objects without geometry (colourbars)
          printMessage("%s object %s", action.c_str(), list[c]->name().c_str());
        }
      }
    }
    else if (what == "all")
    {
      for (auto g : amodel->geometry)
        g->hideShowAll(action == "hide");
      return true;
    }

  }
  else if (parsed.exists("axis"))
  {
    if (gethelp)
    {
      help += "Display/hide the axis legend\n\n"
              "**Usage:** axis (on/off)\n\n"
              "on/off (optional) : show/hide the axis, if omitted switches state\n";
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
      help += "Toggle a boolean property\n\n"
              "**Usage:** toogle (property-name)\n\n"
              "property-name : name of property to switch\n"
              "If an object is selected, will try there, then view, then global\n";
      return false;
    }

    std::string what = parsed["toggle"];
    if (aobject && aobject->properties.has(what) && aobject->properties[what].is_boolean())
    {
      bool current = aobject->properties[what];
      aobject->properties.data[what] = !current;
      printMessage("Property '%s' set to %s", what.c_str(), !current ? "ON" : "OFF");
    }
    else if (aview->properties.has(what) && aview->properties[what].is_boolean())
    {
      bool current = aview->properties[what];
      aview->properties.data[what] = !current;
      printMessage("Property '%s' set to %s", what.c_str(), !current ? "ON" : "OFF");
    }
    else if (session.defaults.count(what) > 0 && session.global(what).is_boolean())
    {
      bool current = session.global(what);
      session.global(what) = !current;
      printMessage("Property '%s' set to %s", what.c_str(), !current ? "ON" : "OFF");
    }
  }
  else if (parsed.exists("redraw"))
  {
    if (gethelp)
    {
      help += "Redraw all object data, required after changing some parameters but may be slow\n";
      return false;
    }

    amodel->redraw(); //Redraw only
    printMessage("Redrawing all objects");
  }
  else if (parsed.exists("fullscreen"))
  {
    if (gethelp)
    {
      help += "Switch viewer to full-screen mode and back to windowed mode\n";
      return false;
    }

    viewer->fullScreen();
    session.globals["resolution"] = json::array({viewer->width, viewer->height});
    printMessage("Full-screen is %s", viewer->fullscreen ? "ON":"OFF");
  }
  else if (parsed.exists("fit"))
  {
    if (gethelp)
    {
      help += "Adjust camera view to fit the model in window\n";
      return false;
    }

    aview->zoomToFit();
  }
  else if (parsed.exists("autozoom"))
  {
    if (gethelp)
    {
      help += "Automatically adjust camera view to always fit the model in window (enable/disable)\n";
      return false;
    }

    aview->autozoom = !aview->autozoom;
    printMessage("AutoZoom is %s", aview->autozoom ? "ON":"OFF");
  }
  else if (parsed.exists("stereo"))
  {
    if (gethelp)
    {
      help += "Enable/disable stereo projection\n"
              "If no stereo hardware found will use side-by-side mode\n";
      return false;
    }

    aview->stereo = !aview->stereo;
    printMessage("Stereo is %s", aview->stereo ? "ON":"OFF");
  }
  else if (parsed.exists("coordsystem"))
  {
    if (gethelp)
    {
      help += "Switch between Right-handed and Left-handed coordinate system\n";
      return false;
    }

    printMessage("%s coordinate system", (aview->switchCoordSystem() == RIGHT_HANDED ? "Right-handed":"Left-handed"));
  }
  else if (parsed.exists("title"))
  {
    if (gethelp)
    {
      help += "Set title heading to following text\n";
      return false;
    }

    aview->properties.data["title"] = parsed.getall("title", 0);
  }
  else if (parsed.exists("rulers"))
  {
    if (gethelp)
    {
      help += "Enable/disable axis rulers (unfinished)\n";
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
      help += "Display help about a command\n";
      return false;
    }

    viewer->display();  //Ensures any previous help text wiped
    std::string cmd = parsed["help"];
    if (cmd.length() > 0)
    {
      help = helpCommand(cmd);
      if (help.length() == 0) return false;
      help = "~~~~~~~~~~~~~~~~~~~~~~~~\n" + cmd + "\n~~~~~~~~~~~~~~~~~~~~~~~~\n" + help;
      std::cout << help;
    }
    else
    {
      help = helpCommand(cmd);
      if (help.length() == 0) return false;
      help = "~~~~~~~~~~~~~~~~~~~~~~~~\nhelp\n~~~~~~~~~~~~~~~~~~~~~~~~\n"
             "For detailed help on a command, type:\n\nhelp command [ENTER]" + help;
      std::cout << HELP_INTERACTION;
      std::cout << help;
    }
    viewer->display(false);  //Immediate display
  }
  else if (parsed.exists("antialias"))
  {
    if (gethelp)
    {
      help += "Enable/disable multi-sample anti-aliasing (if supported by OpenGL drivers)\n";
      return false;
    }

    session.globals["antialias"] = !session.global("antialias");
    printMessage("Anti-aliasing %s", session.global("antialias") ? "ON":"OFF");
  }
  else if (parsed.exists("valuerange"))
  {
    if (gethelp)
    {
      help += "Adjust colourmaps on object to fit actual value range or provided min/max\n\n"
              "**Usage:** valuerange [min max]\n"
              "min (number) : minimum of user defined range, default is to use the data minimum\n"
              "max (number) : maximum of user defined range, default is to use the data maximum\n\n";
      return false;
    }

    DrawingObject* obj = aobject;
    if (!obj)
      obj = lookupObject(parsed, "valuerange");
    if (obj)
    {
      float fmin = 0, fmax = 0;
      float* min = NULL;
      float* max = NULL;
      if (parsed.has(fmin, "valuerange", 0) && parsed.has(fmax, "valuerange", 1))
      {
        min = &fmin;
        max = &fmax;
      }
      for (auto g : amodel->geometry)
        g->setValueRange(obj, min, max);
      printMessage("ColourMap scales set to value range");
      //Colour change so force full reload
      amodel->reload(aobject);
    }
  }
  else if (parsed.exists("bake") || parsed.exists("bakecolour") || parsed.exists("glaze"))
  {
    if (gethelp)
    {
      help += "'Bake' model geometry, turns dynamic meshes into static by permanently replacing higher level\n"
              "glyph types (vector/tracer/shape) with generated low-level primitives (point/line/triangle)\n"
              "This removes the ability to change the glyph properties used to create primitives,\n"
              "but makes the meshes render much faster in animations as they don't need to be regenerated\n\n"
              "**Usage:** bake [object]\n\n"
              "object (integer/string) : the index or name of object to bake, defaults to all (see: \"list objects\")\n\n"
              "**Usage:** bakecolour [object]\n\n"
              "As above, but also convert colourmap + value data into calculated RGBA values\n\n"
              "**Usage:** glaze [object]\n\n"
              "As above, but also convert colourmap + value data into a texture image and associated texcoords\n\n";
      return false;
    }

    DrawingObject* obj = NULL;
    if (parsed.exists("bakecolour"))
    {
      obj = lookupObject(parsed, "bakecolour");
      if (!obj) obj = aobject;
      amodel->bake(obj, true, false);
    }
    else if (parsed.exists("glaze"))
    {
      obj = lookupObject(parsed, "glaze");
      if (!obj) obj = aobject;
      amodel->bake(obj, false, true);
    }
    else
    {
      obj = lookupObject(parsed, "bake");
      if (!obj) obj = aobject;
      amodel->bake(obj, false, false);
    }
    printMessage("Model baked");
  }
  else if (parsed.exists("export"))
  {
    if (gethelp)
    {
      help += "Export model database\n\n"
              "**Usage:** export [filename] [objects]\n\n"
              "filename (string) : the name of the file to export to, extension defaults to .gldb\n"
              "objects (integer/string) : the indices or names of the objects to export (see: \"list objects\")\n"
              "If object ommitted all will be exported, hidden objects will be skipped.\n"
              "If object(s) provided, filename must also be provided.\n";
      return false;
    }

    std::string dbfn = parsed["export"];
    if (dbfn.find(".gldb") == std::string::npos)
    {
      if (dbfn.length())
        dbfn += ".gldb";
      else
        dbfn = "exported.gldb";
    }
    std::vector<DrawingObject*> list = lookupObjects(parsed, "export", 1);
    exportData(lucExportGLDB, list, dbfn);
    printMessage("Database export complete");
  }
  else if (parsed.exists("csv"))
  {
    if (gethelp)
    {
      help += "Export CSV data\n\n"
              "**Usage:** csv [objects]\n\n"
              "objects (integer/string) : the indices or names of the objects to export (see: \"list objects\")\n"
              "If object ommitted all will be exported\n";
      return false;
    }

    std::vector<DrawingObject*> list = lookupObjects(parsed, "csv");
    exportData(lucExportCSV, list);
    printMessage("CSV export complete");
  }
  else if (parsed.exists("json"))
  {
    if (gethelp)
    {
      help += "Export json data\n\n"
              "**Usage:** json [object]\n\n"
              "objects (integer/string) : the indices or names of the objects to export (see: \"list objects\")\n"
              "If object ommitted all will be exported\n";
      return false;
    }


    std::vector<DrawingObject*> list = lookupObjects(parsed, "json");
    exportData(lucExportJSON, list);
    printMessage("JSON export complete");
  }
  else if (parsed.exists("list"))
  {
    if (gethelp)
    {
      help += "List available data\n\n"
              "**Usage:** list objects/colourmaps/elements\n\n"
              "objects : enable object list (stays on screen until disabled)\n"
              "          (dimmed objects are hidden or not in selected viewport)\n"
              "colourmaps : show colourmap list\n"
              "elements : show geometry elements by id in terminal\n"
              "data : show available data sets in selected object or all\n";
      return false;
    }

    std::ostringstream ss;
    if (parsed["list"] == "elements")
    {
      //Print available elements by id
      for (auto g : amodel->geometry)
        g->print(ss);

    }
    else if (parsed["list"] == "colourmaps")
    {
      ss << "ColourMaps:\n===========\n";
      for (unsigned int i=0; i < amodel->colourMaps.size(); i++)
        if (amodel->colourMaps[i])
          ss << std::setw(5) << (i+1) << " : " << amodel->colourMaps[i]->name << std::endl;
    }
    else if (parsed["list"] == "data")
    {
      if (aobject)
        ss << ("Data sets for: " + aobject->name()) << std::endl;
      ss << "-----------------------------------------" << std::endl;
      for (auto g : amodel->geometry)
      {
        json list = g->getDataLabels(aobject);
        for (unsigned int l=0; l < list.size(); l++)
        {
          std::string name = list[l]["name"];
          ss << "[" << l << "] " << list[l]["label"]
             << " (range: " << list[l]["minimum"]
             << " to " << list[l]["maximum"] << ")"
             << " -- " << list[l]["size"]
             << " (" + name << ")" << std::endl;
        }
      }
      ss << "-----------------------------------------" << std::endl;
    }
    else if (parsed["list"] == "render")
    {
      ss << "Renderers:\n===========\n";
      for (unsigned int i=0; i < amodel->geometry.size(); i++)
        if (amodel->geometry[i])
          ss << std::setw(5) << (i+1) << " : (" << amodel->geometry[i]->name << ") " << amodel->geometry[i]->renderer << " - base: " << GeomData::names[amodel->geometry[i]->type] << std::endl;
    }
    else
    {
      objectlist = !objectlist;
      displayObjectList(true);
    }
    std::cerr << ss.str();
    help = ss.str();
  }
  else if (parsed.exists("clear"))
  {
    if (gethelp)
    {
      help += "Clear data from current model/timestep\n\n"
              "**Usage:** clear [type/\"objects\"/\"all\"]\n\n"
              "type (string) : clear only data of this type, eg: \"indices\", applies to selected object.\n"
              "objects : optionally clear all object entries\n"
              "          (if omitted, only the object's geometry data is cleared)\n"
              "all : clear everything, including models\n";
      return false;
    }
    std::string what = parsed["clear"];
    if (what == "all")
      close();
    else if (what == "" || what == "objects")
      clearAll(what == "objects");
    else
    {
      auto res = std::find(std::begin(GeomData::datalabels), std::end(GeomData::datalabels), what);
      if (res == std::end(GeomData::datalabels))
      {
        //Assume value label
        clearValues(aobject, what);
      }
      else
      {
        //Get data type index
        int pos = res - std::begin(GeomData::datalabels);
        clearData(aobject, (lucGeometryDataType)pos);
      }
    }
  }
  else if (parsed.exists("reload"))
  {
    if (gethelp)
    {
      help += "Reload all data of current model/timestep from database\n\n"
              "**Usage:** reload [object_id]\n\n"
              "If an object is selected, reload will apply to that object only\n"
              "(If no database, will still reload data to gpu)\n";
      return false;
    }

    DrawingObject* obj = lookupObject(parsed, "reload");

    //Restore original data
    if (amodel->database)
      amodel->loadTimeSteps();

    amodel->reload(obj); //Redraw & reload

    //View reset is all we need here? If not, must be called on render thread
    //loadModelStep(model, amodel->step());
    viewset = RESET_YES;
  }
  else if (parsed.exists("colourmap"))
  {
    if (gethelp)
    {
      help += "Set colourmap on object or create a new colourmap\n\n"
              "**Usage:** colourmap [colourmap / data]\n"
              "If an object is selected, the colourmap will be set on that object, otherwise a new colourmap will be created\n\n"
              "colourmap (integer/string) : the index or name of the colourmap to apply (see: \"list colourmaps\")\n"
              "data (string) : data to load into selected object's colourmap, if no colourmap one will be added\n";
      return false;
    }

    //Legacy: set colourmap on object by name/ID match
    DrawingObject* obj = aobject;
    int next = 0;
    //Select by id, or active as fallback
    ival = 0;
    if (!obj || parsed.has(ival, "colourmap"))
    {
      obj = lookupObject(parsed, "colourmap");
      next++;
    }
    if (obj)
    {
      //Now uses single execution path for setting colourmaps via property set
      parseProperty("colourmap=" + parsed.getall("colourmap", next), obj);
    }
    else
    {
      std::string name = parsed["colourmap"];
      std::string data = parsed.get("colourmap", 1);
      //Just add a colormap without an object
      LavaVu::addColourMap(name, data);
    }
  }
  else if (parsed.exists("colourbar"))
  {
    if (gethelp)
    {
      help += "Add a colour bar display to selected object\n\n"
              "**Usage:** colourbar [alignment] [size]\n\n"
              "alignment (string) : viewport alignment (left/right/top/bottom)\n";
      return false;
    }

    if (aobject)
    {
      DrawingObject* cbar = colourBar(aobject);
      std::string align = parsed["colourbar"];
      if (align.length())
        cbar->properties.data["align"] = align;
    }
  }
  else if (parsed.exists("palette"))
  {
    if (gethelp)
    {
      help += "Export colour data of selected object\n\n"
              "**Usage:** palette [type] [filename]\n\n"
              "type (string) : type of export (image/json/text) default = text\n"
              "filename (string) : defaults to palette.[png/json/txt]\n";
      return false;
    }

    if (aobject)
    {
      std::string what = parsed["palette"];
      std::string fn = parsed.get("palette", 1);
      if (!fn.length()) fn = "palette";
      ColourMap* cmap = aobject->getColourMap("colourmap");
      if (!cmap) return false;
      if (what == "json")
      {
        std::ofstream of(fn + ".json");
        of << std::setw(2) << cmap->toJSON();
        of.close();
      }
      else if (what == "image")
      {
        ImageData* paletteData = cmap->toImage(false);
        paletteData->write(fn + ".png");
        delete paletteData;
      }
      else
      {
        std::ofstream of(fn + ".txt");
        of << cmap->toString();
        of.close();
      }
    }
  }
  else if (parsed.exists("isosurface"))
  {
    if (gethelp)
    {
      help += "Generate an isosurface from a volume object\n\n"
              "**Usage:** isosurface [isovalue]\n\n"
              "isovalue (number) : isovalue to draw the surface at,\n"
              "                    if not specified, will use 'isovalues' property\n";
      return false;
    }

    if (aobject)
    {
      std::string props = parsed.get("isosurface");
      if (props.length() > 0)
        props = "isovalues=" + props;
      isoSurface(NULL, aobject, props, false);
      printMessage("Generating IsoSurface for object: %s", aobject->name().c_str());
    }
  }
  else if (parsed.exists("pointtype"))
  {
    if (gethelp)
    {
      help += "Set point-rendering type on object\n\n"
              "**Usage:** pointtype all/object type/up/down\n\n"
              "all : use 'all' to set the global default point type\n"
              "object (integer/string) : the index or name of the object to set (see: \"list objects\")\n"
              "type (integer) : Point type [0,3] to apply (gaussian/flat/sphere/highlight sphere)\n"
              "up/down : use 'up' or 'down' to switch to the previous/next type in list\n";
      return false;
    }

    std::string what = parsed["pointtype"];
    if (what == "all")
    {
      int pt = session.global("pointtype");
      if (parsed.has(ival, "pointtype", 1))
        session.globals["pointtype"] = ival % 5;
      else
        session.globals["pointtype"] = (pt+1) % 5;
      printMessage("Point type %d", (int)session.globals["pointtype"]);
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
        //Full object reload as data changed
        amodel->reload(obj);
        amodel->redraw();
      }
    }
  }
  else if (parsed.exists("pointsample"))
  {
    if (gethelp)
    {
      help += "Set point sub-sampling value\n\n"
              "**Usage:** pointsample value/up/down\n\n"
              "value (integer) : 1=no sub-sampling=50%% sampled etc..\n"
              "up/down : use 'up' or 'down' to sample more (up) or less (down) points\n";
      return false;
    }

    if (parsed.has(ival, "pointsample"))
      session.globals["pointsubsample"] = ival;
    else if (parsed["pointsample"] == "up")
      session.globals["pointsubsample"] = (int)session.global("pointsubsample") / 2;
    else if (parsed["pointsample"] == "down")
      session.globals["pointsubsample"] = (int)session.global("pointsubsample") * 2;
    if ((int)session.global("pointsubsample") < 1) session.globals["pointsubsample"] = 1;
    std::vector<Geometry*> activelist = amodel->getRenderersByTypeName("points");
    for (auto active : activelist)
      active->redraw = true;
    printMessage("Point sampling %d", (int)session.global("pointsubsample"));
  }
  else if (parsed.has(ival, "outwidth"))
  {
    if (gethelp)
    {
      help += "Set output image width (height calculated to match window aspect)\n\n"
              "**Usage:** outwidth value\n\n"
              "value (integer) : width in pixels for output images\n";
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
      help += "Set output image height\n\n"
              "**Usage:** outheight value\n\n"
              "value (integer) : height in pixels for output images\n";
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
      help += "Set window background colour\n\n"
              "**Usage:** background colour/value/white/black/grey/invert\n\n"
              "colour (string) : any valid colour string\n"
              "value (number [0,1]) : sets to greyscale value with luminosity in range [0,1] where 1.0 is white\n"
              "white/black/grey : sets background to specified value\n"
              "invert (or omitted value) : inverts current background colour\n";
      return false;
    }
    
    std::string val = parsed["background"];
    if (val == "invert")
    {
      aview->background.invert();
    }
    else if (parsed.has(fval, "background"))
    {
      aview->background.a = 255;
      if (fval <= 1.0) fval *= 255;
      aview->background.r = aview->background.g = aview->background.b = fval;
    }
    else if (val.length() > 0)
    {
      parseProperty("background=" + parsed["background"]);
      aview->background = Colour(aview->properties["background"]);
    }
    else
    {
      if (aview->background.r == 255 || aview->background.r == 0)
        aview->background.value = 0xff666666;
      else
        aview->background.value = 0xff000000;
    }
    session.globals["background"] = aview->background.toString();
    aview->setBackground();
    printMessage("Background colour set");
  }
  else if (parsed.exists("border"))
  {
    if (gethelp)
    {
      help += "Set model border frame\n\n"
              "**Usage:** border on/off/filled\n\n"
              "on : line border around model bounding-box\n"
              "off : no border\n"
              "filled : filled background bounding box\n"
              "(no value) : switch between possible values\n";
      return false;
    }

    //Frame off/on/filled
    aview->properties.data["fillborder"] = false;
    if (parsed["border"] == "on")
      aview->properties.data["border"] = 1.0;
    else if (parsed["border"] == "off")
      aview->properties.data["border"] = 0.0;
    else if (parsed["border"] == "filled")
    {
      aview->properties.data["fillborder"] = true;
      aview->properties.data["border"] = 1.0;
    }
    else
    {
      if (parsed.has(fval, "border"))
        aview->properties.data["border"] = fval;
      else
      {
        fval = aview->properties["border"];
        aview->properties.data["border"] = fval ? 0.0 : 1.0;
      }
    }
    printMessage("Frame set to %s (%f), filled=%d", (float)aview->properties["border"] > 0.0 ? "ON" : "OFF", (float)aview->properties["border"], (bool)aview->properties["fillborder"]);
  }
  else if (parsed.exists("camera"))
  {
    if (gethelp)
    {
      help += "Output camera state for use in model scripts\n";
      return false;
    }

    //Output camera view xml and translation/rotation commands
    aview->print();
    redisplay = false;
  }
  else if (parsed.has(fval, "modelscale"))
  {
    if (gethelp)
    {
      help += "Set model scaling, multiples by existing values\n\n"
              "**Usage:** scale xval yval zval\n\n"
              "xval (number) : scaling value applied to x axis\n\n"
              "yval (number) : scaling value applied to y axis\n"
              "zval (number) : scaling value applied to z axis\n";
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
      help += "Scale model or applicable objects\n\n"
              "**Usage:** scale axis value\n\n"
              "axis : x/y/z\n"
              "value (number) : scaling value applied to all geometry on specified axis (multiplies existing)\n\n"
              "**Usage:** scale geometry_type value/up/down\n\n"
              "geometry_type : points/vectors/tracers/shapes\n"
              "value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling\n\n"
              "**Usage:** scale object value/up/down\n\n"
              "object (integer/string) : the index or name of the object to scale (see: \"list objects\")\n"
              "value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling\n";
      return false;
    }

    std::string what = parsed["scale"];
    std::vector<Geometry*> activelist = amodel->getRenderersByTypeName(what);
    if (activelist.size())
    {
      std::string key = "scale" + what;
      float scale = session.global(key);
      if (parsed.has(fval, "scale", 1))
        session.globals[key] = fval > 0.0 ? fval : 1.0;
      else if (parsed.get("scale", 1) == "up")
        session.globals[key] = scale * 1.5;
      else if (parsed.get("scale", 1) == "down")
        session.globals[key] = scale / 1.5;
      for (auto active : activelist)
        active->redraw = true;
      printMessage("%s scaling set to %f", what.c_str(), (float)session.globals[key]);
    }
    else
    {
      //X,Y,Z: geometry scaling
      if (what == "x")
      {
        if (parsed.has(fval, "scale", 1))
        {
          aview->setScale(fval, aview->scale[1], aview->scale[2], true); //Replace existing
          amodel->redraw();
        }
      }
      else if (what == "y")
      {
        if (parsed.has(fval, "scale", 1))
        {
          aview->setScale(aview->scale[0], fval, aview->scale[2], true); //Replace existing
          amodel->redraw();
        }
      }
      else if (what == "z")
      {
        if (parsed.has(fval, "scale", 1))
        {
          aview->setScale(aview->scale[0], aview->scale[1], fval, true); //Replace existing
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
            obj->properties.data["scaling"] = fval > 0.0 ? fval : 1.0;
          else if (parsed.get("scale", next) == "up")
            obj->properties.data["scaling"] = sc * 1.5;
          else if (parsed.get("scale", next) == "down")
            obj->properties.data["scaling"] = sc / 1.5;
          printMessage("%s scaling set to %f", obj->name().c_str(), (float)obj->properties["scaling"]);
          //Reload required for per-object scaling
          amodel->reload();
        }
      }
    }
  }
  else if (parsed.exists("history"))
  {
    if (gethelp)
    {
      help += "Write command history to output (eg: terminal)\n\n"
              "**Usage:** history [filename]\n\n"
              "filename (string) : optional file to write data to\n";
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
      help += "Clear command history\n";
      return false;
    }

    history.clear();
    return false; //Skip record
  }
  else if (parsed.has(ival, "pause"))
  {
    if (gethelp)
    {
      help += "Pause execution (for scripting)\n\n"
              "**Usage:** pause msecs\n\n"
              "msecs (integer) : how long to pause for in milliseconds\n";
      return false;
    }

    PAUSE(ival);
  }
  else if (parsed.exists("delete"))
  {
    if (gethelp)
    {
      help += "Delete objects\n"
              "**Usage:** delete object\n\n"
              "object (integer/string) : the index or name of the object to delete (see: \"list objects\")\n";
      return false;
    }

    //Delete drawing object by name/ID match
    std::vector<DrawingObject*> list = lookupObjects(parsed, "delete");
    for (unsigned int c=0; c<list.size(); c++)
    {
      printMessage("%s deleted", list[c]->name().c_str());

      //Delete object and geometry
      amodel->deleteObject(list[c]);
    }

    aobject = NULL;
  }
  else if (parsed.exists("deletedb"))
  {
    if (gethelp)
    {
      help += "Delete objects from database\n"
              "WARNING: This is irreversible! Backup your database before using!\n\n"
              "**Usage:** delete object_id\n\n"
              "object (integer/string) : the index or name of the object to delete (see: \"list objects\")\n";
      return false;
    }

    std::vector<DrawingObject*> list = lookupObjects(parsed, "deletedb");
    for (unsigned int c=0; c<list.size(); c++)
    {
      if (list[c]->dbid == 0) continue;
      printMessage("%s deleting from database", list[c]->name().c_str());
      //Delete from db
      amodel->deleteObjectRecord(list[c]->dbid);
      //Delete the loaded object data
      parseCommand("delete " + list[c]->name());
    }

    aobject = NULL;
  }
  else if (parsed.exists("merge"))
  {
    if (gethelp)
    {
      help += "Merge data from attached databases into main (warning: backup first!)\n";
      return false;
    }

    amodel->mergeDatabases();
    parseCommands("quit");
  }
  else if (parsed.exists("load"))
  {
    if (gethelp)
    {
      help += "Load objects from database\n"
              "Used when running with deferred loading (-N command line switch)\n\n"
              "**Usage:** load object\n\n"
              "object (integer/string) : the index or name of the object to load (see: \"list objects\")\n";
      return false;
    }

    //Load drawing object by name/ID match
    std::vector<DrawingObject*> list = lookupObjects(parsed, "load");
    for (unsigned int c=0; c<list.size(); c++)
    {
      if (list[c]->dbid == 0) continue;
      amodel->loadFixedGeometry(list[c]->dbid);
      amodel->loadGeometry(list[c]->dbid);
    }
    //Update the views
    resetViews(false);
    amodel->reload(); //Redraw & reload
  }
  else if (parsed.exists("sort"))
  {
    if (gethelp)
    {
      help += "Sort geometry on demand (with idle timer)\n\n"
              "**Usage:** sort on/off\n\n"
              "on : (default) sort after model rotation\n"
              "off : no sorting\n"
              "If no options passed, does immediate re-sort\n";
      return false;
    }

    if (parsed["sort"] == "off")
    {
      //Disables all sorting
      session.globals["sort"] = false;
      printMessage("Geometry sorting has been disabled");
      redisplay = false;
    }
    else if (parsed["sort"] == "on")
    {
      //Enables sort on rotate mode
      session.globals["sort"] = true;
      printMessage("Sort geometry on rotation enabled");
      redisplay = false;
    }
    else
    {
      //User requested sort
      aview->rotated = false;
      sort(true);
    }
  }
  else if (parsed.exists("display"))
  {
    //Internal usage, no help
    if (gethelp) return false;
    //Simply return true to render a frame
    return true;
  }
  else if (parsed.exists("asyncsort"))
  {
    //Internal usage, no help
    if (gethelp) return false;

    //Sort required?
    if (aview->rotated)
    {
      //Sort returns true if the sort was initiated, otherwise false
      aview->rotated = !sort();
    }
    return false;
  }
  else if (parsed.exists("idle"))
  {
    //Internal usage, no help
    if (gethelp) return false;

    //Command playback
    if (repeat != 0)
    {
      //Playback
      for (unsigned int l=0; l<replay.size(); l++)
        parseCommands(replay[l]);
      repeat--;
      if (repeat == 0)
      {
        viewer->animateTimer(0); //Disable idle redisplay timer
        replay.clear();
      }
    }
    return false; //true; //Skip record
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
    viewer->button = LeftButton;
    if (button==1) viewer->button = MiddleButton;
    if (button==2) viewer->button = RightButton;
    int spin = props.Int("spin");
    std::string action = props["mouse"];
    std::string modifiers = props["modifiers"];

    //Save shift states
    viewer->keyState.shift = modifiers.find('S')+1;
    viewer->keyState.ctrl = modifiers.find('C')+1;
    viewer->keyState.alt = modifiers.find('A')+1;

    //printf("-- %s button %d x %d y %d spin %d modkeys %s\n", action.c_str(), button, x, y, spin, modifiers.c_str());

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
      help += "Add a new object and select as the active object\n\n"
              "**Usage:** add [object_name] [data_type]\n\n"
              "object_name (string) : optional name of the object to add\n"
              "data_type (string) : optional name of the default data type\n"
              "(points/labels/vectors/tracers/triangles/quads/shapes/lines)\n";
      return false;
    }

    std::string name = parsed["add"];
    aobject = addObject(new DrawingObject(session, name));
    if (aobject)
    {
      std::string type = parsed.get("add", 1);
      if (type.length() > 0) aobject->properties.data["renderer"] = type;
      printMessage("Added object: %s", aobject->name().c_str());
    }
  }
  else if (parsed.exists("append"))
  {
    if (gethelp)
    {
      help += "Append a new geometry container to an object\n"
              "eg: to divide line objects into segments\n\n"
              "**Usage:** append [object]\n\n"
              "object (integer/string) : the index or name of the object to select (see: \"list objects\")\n";
      return false;
    }

    std::string o = parsed["append"];
    DrawingObject* obj;
    if (o.length())
      obj = lookupObject(parsed, "append");
    else
      obj = aobject;
    if (obj)
    {
      appendToObject(obj);
      printMessage("Appended to object: %s", obj->name().c_str());
    }
  }
  else if (parsed.exists("select"))
  {
    if (gethelp)
    {
      help += "Select object as the active object\n"
              "Used for setting properties of objects\n"
              "following commands that take an object id will no longer require one)\n\n"
              "**Usage:** select object\n\n"
              "object (integer/string) : the index or name of the object to select (see: \"list objects\")\n"
              "Leave object parameter empty to clear selection.\n";
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
      help += "Reload shader program files from disk\n";
      return false;
    }

    printMessage("Reloading shaders");
    reloadShaders();
    return true;
  }
  else if (parsed.exists("blend"))
  {
    if (gethelp)
    {
      help += "Select blending type\n\n"
              "**Usage:** blend mode\n\n"
              "mode (string) : normal: multiplicative, add: additive, pre: premultiplied alpha, png: premultiplied for png output\n";
      return false;
    }

    std::string what = parsed["blend"];
    if (what == "png")
      viewer->blend_mode = BLEND_PNG;
    else if (what == "add")
      viewer->blend_mode = BLEND_ADD;
    else if (what == "pre")
      viewer->blend_mode = BLEND_PRE;
    else if (what == "def")
      viewer->blend_mode = BLEND_DEF;
    else
      viewer->blend_mode = BLEND_NORMAL;
  }
  else if (parsed.exists("props"))
  {
    if (gethelp)
    {
      help += "Print properties on active object or global and view if none selected\n";
      return false;
    }

    printProperties();
  }
  else if (parsed.exists("defaults"))
  {
    if (gethelp)
    {
      help += "Print all available properties and their default values\n";
      return false;
    }

    printDefaultProperties();
  }
  else if (parsed.exists("test"))
  {
    if (gethelp)
    {
      help += "Create and display some test data, points, lines, quads\n";
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
      help += "Create and display a test volume dataset\n";
      return false;
    }

    float x = 0, y = 0, z = 0;
    if (parsed.has(x, "voltest", 0) &&
        parsed.has(y, "voltest", 1) &&
        parsed.has(z, "voltest", 2))
    {
      if (x < 32) x = 32;
      if (y < 32) y = 32;
      if (z < 32) z = 32;
      createDemoVolume(x, y, z);
    }
    else
      createDemoVolume();
    resetViews();
  }
  else if (parsed.exists("name"))
  {
    if (gethelp)
    {
      help += "Set object name\n\n"
              "**Usage:** name object newname\n\n"
              "object (integer/string) : the index or name of the object to rename (see: \"list objects\")\n"
              "newname (string) : new name to apply\n";
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
        obj->properties.data["name"] = name;
        printMessage("Renamed object: %s", obj->name().c_str());
      }
    }
  }
  else if (parsed.exists("newstep"))
  {
    if (gethelp)
    {
      help += "Add a new timestep after the current one\n";
      return false;
    }

    int step;
    if (!parsed.has(step, "newstep"))
        step = -1;
    addTimeStep(step);
  }
  else if (parsed.exists("filter") || parsed.exists("filterout"))
  {
    if (gethelp)
    {
      help += "Set a data filter on selected object\n\n"
              "**Usage:** filter index min max [map]\n\n"
              "index (integer) : the index of the data set to filter on (see: \"list data\")\n"
              "min (number) : the minimum value of the range to filter in or out\n"
              "max (number) : the maximum value of the range to filter in or out\n"
              "map (literal) : add map keyword for min/max [0,1] on available data range\n"
              "                  eg: 0.1-0.9 will filter the lowest and highest 10% of values\n";
      return false;
    }

    //Require a selected object
    if (aobject)
    {
      json filter;
      std::string action = "filter";
      filter["out"] = false;
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
      printMessage("%s filter on value index %d from %f to %f", (map ? "Map" : "Value"), (int)filter["by"], min, max);
      //Full data reload
      amodel->reload(aobject);
    }
  }
  else if (parsed.exists("filtermin") || parsed.exists("filtermax"))
  {
    std::string action = parsed.exists("filtermin") ? "filtermin" : "filtermax";
    if (gethelp)
    {
      help += "Modify a data filter on selected object\n\n"
              "**Usage:** " + action + " index value\n\n"
              "index (integer) : the index filter to set [0 - N-1] where N = # of filters added\n"
              "value (number) : the " + (action == "filtermin" ? "minimum" : "maximum") + " value of the range to filter in or out\n";
      return false;
    }

    //Require a selected object
    if (aobject)
    {
      json& filters = aobject->properties["filters"];
      unsigned int idx = parsed.Int(action, 0);
      if (filters.size() <= idx) return false;
      json& filter = filters[idx];
      float val;
      parsed.has(val, action, 1);
      if (action == "filtermin")
        filter["minimum"] = val;
      else
        filter["maximum"] = val;
      printMessage("Filter %d set from %f to %f", idx, (float)filter["minimum"], (float)filter["maximum"]);
      //Full data reload
      amodel->reload(aobject);
    }
  }

  else if (parsed.exists("clearfilters"))
  {
    if (gethelp)
    {
      help += "Clear filters on selected object\n";
      return false;
    }

    //Require a selected object
    if (aobject)
    {
      printMessage("Filters cleared on object %s", aobject->name().c_str());
      aobject->properties.data.erase("filters");
      //Full data reload
      amodel->reload(aobject);
    }
  }
  //******************************************************************************
  else if (!gethelp && !viewer->isopen)
  {
    //Following commands all require open viewer
    //Attempt to parse as property=value first
    if (parseProperty(cmd, aobject)) return true;
    if (verbose) std::cerr << "Viewer must be open to execute command: " << cmd << ", deferred" << std::endl;
    //TODO: categorise other commands that require GL context in the same way
    queueCommands(cmd);
    return false;
  }
  else if (parsed.exists("reset"))
  {
    if (gethelp)
    {
      help += "Reset the camera to the default model view\n";
      return false;
    }

    //if (!aview->initialised) return false;
    //Reset camera
    aview->reset();
    //Force the model dimensions to be re-checked
    viewSelect(view, true, false);
    //Reset camera to default view of model
    aview->init(true);
    printMessage("View reset");
  }
  else if (parsed.exists("bounds"))
  {
    if (gethelp)
    {
      help += "Recalculate the model bounding box from geometry\n";
      return false;
    }

    //Reset bounds per object
    for (auto g : amodel->geometry)
      g->clearBounds();

    //Remove any existing fixed bounds
    aview->properties.data.erase("min");
    aview->properties.data.erase("max");
    session.globals.erase("min");
    session.globals.erase("max");
    //Update the viewports and recalc bounding box
    resetViews();
    //Update fixed bounds
    aview->properties.data["min"] = {aview->min[0], aview->min[1], aview->min[2]};
    aview->properties.data["max"] = {aview->max[0], aview->max[1], aview->max[2]};

    printMessage("View bounds updated");
  }
  else if (parsed.exists("fixbb"))
  {
    if (gethelp)
    {
      help += "Fix the model bounding box to it's current size, will no longer be elastic\n";
      return false;
    }

    //Update fixed bounds
    session.globals["min"] = {aview->min[0], aview->min[1], aview->min[2]};
    session.globals["max"] = {aview->max[0], aview->max[1], aview->max[2]};

    printMessage("Set fixed bounding box");
  }
  else if (parsed.exists("zerocam"))
  {
    if (gethelp)
    {
      help += "Set the camera postiion to the origin (for scripting, not generally advisable)\n";
      return false;
    }

    aview->reset();     //Zero camera
  }
  else if (parsed.exists("play"))
  {
    if (gethelp)
    {
      help += "Display model timesteps in sequence from current timestep to specified timestep\n\n"
              "**Usage:** play endstep\n\n"
              "endstep (integer) : last frame timestep\n"
              "If endstep omitted, will loop continually until 'stop' command entered\n";
      return false;
    }

    if (parsed.has(ival, "play"))
    {
      writeSteps(false, amodel->step(), ival);
    }
    else
    {
      //Play loop
      viewer->animateTimer(); //Start idle redisplay timer for frequent frame updates
      repeat = -1; //Infinite
      viewer->timeloop = true;
    }
    return true;  //Skip record
  }
  else if (parsed.exists("next"))
  {
    if (gethelp)
    {
      help += "Go to next timestep in sequence\n";
      return false;
    }

    int old = session.now;
    if (amodel->timesteps.size() < 2) return false;
    amodel->setTimeStep(session.now+1);
    //Allow loop back to start when using next command
    if (session.now > 0 && session.now == old)
      amodel->setTimeStep(0);
    resetViews();
    if (parsed["next"] == "auto")
      return false; //Skip record to history if automated
  }
  else if (parsed.exists("stop"))
  {
    if (gethelp)
    {
      help += "Stop the looping 'play' command\n";
      return false;
    }

    viewer->timeloop = false;
    viewer->animateTimer(0); //Stop idle redisplay timer
    animate = false;
    repeat = 0;
    replay.clear();
  }
  else if (parsed.exists("animate"))
  {
    if (gethelp)
    {
      help += "Update display between each command\n\n"
              "**Usage:** animate rate\n\n"
              "rate (integer) : animation timer to fire every (rate) msec (default: 50)\n"
              "When on, if multiple commands are issued the frame is re-rendered at set framerate\n"
              "When off, all commands will be processed before the display is updated\n";
      return false;
    }

    //Start idle redisplay timer for frequent frame updates
    if (parsed.has(ival, "animate") && ival > 0)
    {
      viewer->animateTimer(ival);
      animate = true;
      printMessage("Animate mode %d millseconds", viewer->timer_animate);
    }
    else if (animate)
    {
      animate = false;
      viewer->timeloop = false;
      repeat = 0;
      viewer->animateTimer(0);
      printMessage("Animate mode disabled");
    }
    else
    {
      animate = true;
      viewer->animateTimer();
      printMessage("Animate mode %d millseconds", viewer->timer_animate);
    }
  }
  else if (parsed.exists("repeat"))
  {
    if (gethelp)
    {
      help += "Repeat commands from history\n\n"
              "**Usage:** repeat [count=1] [N=1]\n\n"
              "count (integer) : repeat commands count times\n"
              "N (integer) : repeat the last N commands\n";
      return false;
    }

    //Repeat last N commands from history count times
    else if (parsed.has(ival, "repeat"))
    {
      int N;
      if (parsed.has(N, "repeat", 1))
      {
        if ((int)linehistory.size() < N) N = (int)linehistory.size();
        std::copy(std::end(linehistory) - N, std::end(linehistory), std::back_inserter(replay));
      }
      else
        replay.push_back(last_cmd);
      animate = true;
      repeat = ival;
      viewer->animateTimer(); //Start idle redisplay timer for frequent frame updates
      return true;  //Skip record
    }
    else if (linehistory.size() > 0)
      //Manual repeat from line history (ENTER)
      return parseCommands(linehistory.back());
  }

  else if (parsed.exists("image"))
  {
    if (gethelp)
    {
      help += "Save an image of the current view\n"
              "**Usage:** image [filename]\n\n"
              "filename (string) : optional base filename without extension\n";
      return false;
    }
    std::string filename = parsed["image"];
    if (filename.length() > 0)
      viewer->image(filename);
    else
      viewer->image(session.counterFilename());

    if (viewer->outwidth > 0)
      printMessage("Saved image %d x %d", viewer->outwidth, viewer->outheight);
    else
      printMessage("Saved image %d x %d", viewer->width, viewer->height);
  }
  else if (parsed.exists("images"))
  {
    if (gethelp)
    {
      help += "Write images in sequence from current timestep to specified timestep\n\n"
              "**Usage:** images endstep\n\n"
              "endstep (integer) : last frame timestep\n";
      return false;
    }

    //Default to writing from current to final step
    int end = -1;
    if (parsed.has(ival, "images"))
      end = ival;
    writeSteps(true, amodel->step(), end);
  }
  else if (parsed.exists("movie"))
  {
    if (gethelp)
    {
      help += "Encode video of model running from current timestep to specified timestep\n"
              "(Requires libavcodec)\n\n"
              "**Usage:** movie [endstep]\n"
              "**Usage:** movie [startstep] [endstep]\n"
              "**Usage:** movie [startstep] [endstep] [fps]\n\n"
              "startstep (integer) : first frame timestep (defaults to first)\n"
              "endstep (integer) : last frame timestep (defaults to final)\n"
              "fps (integer) : fps, default=30\n";
      return false;
    }

    int start = -1;
    int end = -1;
    int fps = 30;
    if (parsed.has(ival, "movie"))
      end = ival;
    if (parsed.has(ival, "movie", 1))
    {
      start = end;
      end = ival;
    }
    if (parsed.has(ival, "movie", 2))
      fps = ival;
    video("", fps, viewer->outwidth, viewer->outheight, start, end);
  }
  else if (parsed.exists("record"))
  {
    if (gethelp)
    {
      help += "Encode video of all frames displayed at specified framerate\n"
              "(Requires libavcodec)\n\n"
              "**Usage:** record (framerate) (quality)\n\n"
              "framerate (integer): frames per second (default 30)\n"
              "quality (integer): 1=low/2=medium/3=high (default 1)\n";
      return false;
    }

    //Default to 30 fps
    if (!parsed.has(ival, "record"))
      ival = 30;
    //Default to low quality (high is actually a bit too high, files are huge! need to fix)
    int quality;
    if (!parsed.has(quality, "record", 1))
      quality = 1;
    encodeVideo("", ival, quality);
  }
  else if (parsed.exists("tiles"))
  {
    if (gethelp)
    {
      help += "Save a tiled image of selected volume dataset\n"
              "**Usage:** tiles (object)\n";
      return false;
    }
    Volumes* vol = (Volumes*)amodel->getRenderer(lucVolumeType);
    DrawingObject* obj = aobject;
    if (!obj)
      obj = lookupObject(parsed, "tiles");
    if (vol && obj)
    {
      vol->saveTiledImage(obj);
      printMessage("Saved tiled volume mosaic");
    }
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
    else if (parseProperty(cmd, aobject))
    {
      redisplay = true; //Redisplay after prop change
    }
    else
    {
      debug_print("# Unrecognised command or file not found: %s\n", cmd.c_str());
      return false;  //Invalid
    }
  }

  last_cmd = cmd;
  //if (animate && redisplay) viewer->postdisplay = true;

  //Sync menu
  if (redisplay && viewer->isopen && aview && aview->initialised)
    gui_sync();

  return redisplay;
}

std::vector<std::string> LavaVu::commandList(std::string category)
{
  //Return the list of categories, or list of commands in passed category
  std::vector<std::string> categories = {"General", "Input", "Output", "View", "Object", "Display", "Scripting", "Miscellanious"};
  if (category.length() == 0)
    return categories;

  std::vector<std::vector<std::string> > cmdlist = {
    {"quit", "repeat", "animate", "history", "clearhistory", "pause", "list", "step", "timestep", "jump", "model", "reload", "redraw", "clear"},
    {"file", "script", "figure", "savefigure", "view", "scan"},
    {"image", "images", "outwidth", "outheight", "movie", "record", "export", "csv", "json", "cache", "save", "tiles"},
    {"rotate", "rotatex", "rotatey", "rotatez", "rotation", "zoom", "translate", "translatex", "translatey", "translatez", "translation",
     "autorotate", "spin", "focus", "fov", "focallength", "eyeseparation", "nearclip", "farclip", "zoomclip",
     "zerocam", "reset", "bounds", "fixbb", "camera", "resize", "fullscreen", "fit", "autozoom", "stereo", "coordsystem", "sort"},
    {"hide", "show", "delete", "load", "select", "add", "append", "name", "isosurface", "texture", "populate", "bake", "bakecolour", "glaze", "csv", "json", "tiles", "valuerange", "colourmap", "colourbar", "pointtype", "scale"},
    {"background", "alpha", "axis", "rulers", "colourmap",
     "antialias", "pointtype", "pointsample", "border", "title", "scale", "modelscale"},
    {"next", "play", "stop", "open", "interactive"},
    {"shaders", "blend", "props", "defaults", "test", "voltest", "newstep", "filter", "filterout", "filtermin", "filtermax", "clearfilters",
     "verbose", "toggle", "createvolume", "clearvolume", "palette"}
  };

  for (unsigned int i=0; i<categories.size(); i++)
    if (categories[i] == category) return cmdlist[i];
  return std::vector<std::string>();
}

std::string LavaVu::helpCommand(std::string cmd, bool heading)
{
  //This list of categories and commands must be maintained along with the individual command help strings
  std::stringstream markdown;
  std::vector<std::string> categories = commandList();
  //Verbose command help
  if (cmd == "help" || cmd.length() == 0)
  {
    for (unsigned int i=0; i<categories.size(); i++)
    {
      markdown << "\n" << categories[i] << " commands:\n\n > **";
      std::vector<std::string> cmds = commandList(categories[i]);
      for (unsigned int j=0; j<cmds.size(); j++)
      {
        if (j % 11 == 10) markdown << "\n  ";
        markdown << cmds[j];
        if (j < cmds.size() - 1) markdown << ", ";
      }
      markdown << "**\n";
    }
    markdown << "\n";
  }
  else if (cmd == "docs:scripting")
  {
    markdown << "\n# Scripting command reference\n\n";
    //Create content index
    for (unsigned int i=0; i<categories.size(); i++)
    {
      std::string anchor = categories[i] + "-commands";
      std::transform(anchor.begin(), anchor.end(), anchor.begin(), ::tolower);
      markdown <<  " - **[" << categories[i] << "](#" << anchor << ")**  \n\n";
      std::vector<std::string> cmds = commandList(categories[i]);
      for (unsigned int j=0; j<cmds.size(); j++)
      {
        //std::cout <<  "    * [" + cmds[j] + "](#" + cmds[j] + ")\n";
        if (j > 0) markdown << ", ";
        markdown <<  "[" << cmds[j] << "](#" << cmds[j] + ")";
      }
      markdown << std::endl;
    }
    //Create content
    for (unsigned int i=0; i<categories.size(); i++)
    {
      std::vector<std::string> cmds = commandList(categories[i]);
      markdown <<  "\n---\n## " << categories[i] << " commands:\n\n";
      for (unsigned int j=0; j<cmds.size(); j++)
        markdown << helpCommand(cmds[j]);
    }
    std::cout << markdown.str();
  }
  else if (cmd == "docs:properties")
  {
    markdown << "\n# Property reference\n\n";
    //Create content index
    std::string TOC;
    std::stringstream content;
    std::string last;
    for (auto it = session.properties.begin(); it != session.properties.end(); ++it)
    {
      std::string name = it.key();
      json prop = it.value();
      std::string target = prop["target"];
      json defp = prop["default"];
      std::string def = defp.dump();
      std::string type = prop["type"];
      std::string doc = prop["desc"];

      if (defp.is_number())
      {
        if (defp == FLT_MAX) def = "Infinity";
        if (defp == -FLT_MAX) def = "-Infinity";
      }
      if (defp.is_array() && defp.size() > 4)
        def = "[...]";

      if (target != last)
      {
        std::string anchor = target;
        std::transform(anchor.begin(), anchor.end(), anchor.begin(), ::tolower);
        //anchor.erase(std::remove_if(anchor.begin(), anchor.end(), &ispunct), anchor.end());
        //anchor.erase(std::remove_if(anchor.begin(), anchor.end(), &isspace), anchor.end());
        std::replace(anchor.begin(), anchor.end(), ',', '-');
        TOC += " * [" + target + "](#" + anchor + ")\n";
        content << "\n## " << anchor << "\n\n";
        content << "| Property         | Type       | Default            | Description                               |\n";
        content << "| ---------------- | ---------- | ------------------ | ----------------------------------------- |\n";
        last = target;
      }
      content << "|" << std::left << std::setw(18) << ("*" + name + "*");
      content << "| " << std::left << std::setw(10) << type << " ";
      content << "| " << std::left << std::setw(14) << def << " ";
      content << "| " << doc << "|\n";
    }
    markdown << TOC << content.str();
    std::cout << markdown.str();
  }
  else if (cmd.at(0) == '@')
  {
    std::string pname = cmd.substr(1);
    for (auto it = session.properties.begin(); it != session.properties.end(); ++it)
    {
      if (it.key() == pname)
      {
        json prop = it.value();
        std::string target = prop["target"];
        json defp = prop["default"];
        std::string def = defp.dump();
        std::string type = prop["type"];
        std::string doc = prop["desc"];

        if (defp.is_number())
        {
          if (defp == FLT_MAX) def = "Infinity";
          if (defp == -FLT_MAX) def = "-Infinity";
        }

        if (heading) markdown << "\n### Property: \"" << pname + "\"\n\n";
        markdown << " > " << doc << "  \n\n";
        markdown << " - Data type: **" << type << "**  \n";
        markdown << " - Applies to: *" << target << "*  \n";
        markdown << " - Default: **" << def << "**  \n";
        markdown << std::endl;
        break;
      }
    }
  }
  else
  {
    if (heading) markdown << "\n### " << cmd << "\n\n";
    help = "";
    parseCommand(cmd + " 1.0", true);
    if (help.length() == 0) return helpCommand("@" + cmd); //No command, try prop
    std::string line;
    std::stringstream ss(help);
    while(std::getline(ss, line))
      markdown << " > " << line << "  \n";
    help = "";
    markdown << std::endl;
  }

  return markdown.str();
}

std::string LavaVu::propertyList()
{
  std::stringstream ss;
  ss << session.properties;
  return ss.str();
}
