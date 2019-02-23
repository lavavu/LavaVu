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

//BUGS:
//Animate speed can't be changed once started
//Model db not showing on title bar
//Movie command broken
//Tracer filter not applied on initial load (non-db)
//
//TODO:
//Object property to exclude from bounding box calc
//Shader based cross sections of volume data sets
//
//Value data types independent from geometry types?
//Merge WebGL/standard shader code

//Viewer class
#include "version.h"
#include "Include.h"
#include "LavaVu.h"
#include "Shaders.h"
#include "Util.h"
#include "OpenGLViewer.h"
#include "Main/SDLViewer.h"
#include "Main/X11Viewer.h"
#include "Main/GlutViewer.h"
#include "Main/CGLViewer.h"
#include "Main/CocoaViewer.h"
#include "Main/EGLViewer.h"
#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif
#include "tiny_obj_loader.h"

//Viewer class implementation...
LavaVu::LavaVu(std::string binpath, bool havecontext, bool omegalib) : ViewerApp(), binpath(binpath)
{
  viewer = NULL;
  encoder = NULL;
  verbose = dbpath = false;
  frametime = std::chrono::system_clock::now();
  fps = framecount = 0;
  session.havecontext = havecontext;
  session.omegalib = omegalib;
  historyline = -1;
  last_cmd = multiline = "";

  defaultScript = "init.script";

  //Create the viewer window
  //(Evil platform specific extension handling stuff)
#if defined _WIN32
  //GetProcAddress = (getProcAddressFN)wglGetProcAddress;
#elif defined HAVE_X11  //not defined __APPLE__
  //GetProcAddress = (getProcAddressFN)glXGetProcAddress;
  GetProcAddress = (getProcAddressFN)glXGetProcAddressARB;
#elif defined HAVE_GLUT and not defined __APPLE__
  GetProcAddress = (getProcAddressFN)glXGetProcAddress;
#elif defined HAVE_EGL
  GetProcAddress = (getProcAddressFN)eglGetProcAddress;
#endif

  //Create viewer window
  if (!omegalib && !havecontext)
  {
#if defined HAVE_X11
  if (!viewer) viewer = new X11Viewer();
#endif
#if defined HAVE_GLUT
  if (!viewer) viewer = new GlutViewer();
#endif
#if defined HAVE_SDL || defined _WIN32
  if (!viewer) viewer = new SDLViewer();
#endif
#if defined HAVE_CGL
  if (!viewer) viewer = new CocoaViewer();
  if (!viewer) viewer = new CGLViewer();
#endif
#if defined HAVE_EGL
  if (!viewer) viewer = new EGLViewer();
#endif
  }
  //Assume an OpenGL context is already provided
  if (!viewer) viewer = new OpenGLViewer();

  viewer->app = (ApplicationInterface*)this;

  //Shader path (default to program path if not set)
  if (binpath.back() != '/') binpath += "/";
  if (Shader::path.length() == 0) Shader::path = binpath + "shaders/";

  //Initialise the session
  session.init(binpath);

  //Add default console input attachment to viewer
  if (!omegalib)
  {
    static StdInput stdi;
    viewer->addInput(&stdi);
  }
}

void LavaVu::defaults()
{
  //Set all values/state to defaults
  if (viewer) 
  {
    //Now the app init phase can be repeated, need to reset all data
    //viewer->visible = true;
    viewer->quitProgram = false;
    close();
  }

  //Reset state
  session.reset();

  //Clear any queued commands
  viewer->commands.clear();

  axis = new Triangles(session);
  rulers = new Lines(session);
  border = new QuadSurfaces(session);

  initfigure = 0;
  viewset = RESET_NO;
  view = -1;
  model = -1;
  aview = NULL;
  amodel = NULL;
  aobject = NULL;

  startstep = -1;
  endstep = -1;
  dump = lucExportNone;
  dumpid = 0;
  status = true;
  writeimage = false;
  writemovie = 0;
  //Omegalib defaults
  if (session.omegalib)
    session.globals["vectorfont"] = true;
  message[0] = '\0';
  volume = NULL;

  tracersteps = 0;
  objectlist = false;

  //Interaction command prompt
  entry = "";

  animate = false;
  repeat = 0;
  encoder = NULL;

  history.clear();
  linehistory.clear();
  replay.clear();
  help = "";
}

LavaVu::~LavaVu()
{
  if (sort_thread.joinable())
  {
    viewer->quitProgram = true;
    sortcv.notify_one();
    sort_thread.join();
  }

  close();

  if (encoder) delete encoder;
  debug_print("LavaVu closing: peak geometry memory usage: %.3f mb\n", mempeak__/1000000.0f);
  if (viewer) delete viewer;
}

void LavaVu::arguments(std::vector<std::string> args)
{
  //Read command line switches
  for (unsigned int i=0; i<args.size(); i++)
  {
    //Version
    if (args[i] == "--version" || args[i] == "-version")
    {
      std::cout << "LavaVu " << version << std::endl;
      exit(0);
    }
    //Help?
    if (args[i] == "-?" || args[i] == "-help")
    {
      std::cout << "## Command line arguments\n\n";
      std::cout << "|         | General options\n";
      std::cout << "| ------- | ---------------\n";
      std::cout << "| -#      | Any integer entered as a switch will be interpreted as the initial timestep to load. ";
      std::cout << "Any following integer switch will be interpreted as the final timestep for output. ";
      std::cout << "eg: -10 -20 will run all output commands on timestep 10 to 20 inclusive\n";
      std::cout << "| -c#     | caching, set # of timesteps to cache data in memory for\n";
      std::cout << "| -A      | All objects hidden initially, use 'show object' to display\n";
      std::cout << "| -N      | No load, deferred loading mode, use 'load object' to load & display from database\n";
      std::cout << "| -S      | Skip default script, don't run init.script\n";
      std::cout << "| -v      | Verbose output, debugging info displayed to console\n";
      std::cout << "| -a      | Automation mode, don't activate event processing loop\n";
      std::cout << "| -p#     | port, web server interface listen on port #\n";
      std::cout << "| -q#     | quality, web server jpeg quality (0=don't serve images)\n";
      std::cout << "| -n#     | number of threads to launch for web server #\n";
      std::cout << "| -Q      | quiet mode, no status updates to screen\n";
      std::cout << "| -s      | stereo, request a quad-buffer stereo context\n";
      std::cout << "\n";
      std::cout << "|         | Model options\n";
      std::cout << "| ------- | -------------\n";
      std::cout << "| -f#     | Load initial figure number # [1-n]\n";
      std::cout << "| -C      | Global camera, each model shares the same camera view\n";
      std::cout << "| -y      | Swap z/y axes of imported static models\n";
      std::cout << "| -T#     | Subdivide imported static model triangles into #\n";
      std::cout << "| -V#,#,# | Volume data resolution in X,Y,Z\n";
      std::cout << "| -e      | Each data set loaded from non time varying source has new timestep inserted\n";
      std::cout << "\n";
      std::cout << "|         | Image/Video output\n";
      std::cout << "| ------- | ------------------\n";
      std::cout << "| -z#     | Render images # times larger and downsample for output\n";
      std::cout << "| -i/w    | Write images of all loaded timesteps/windows then exit\n";
      std::cout << "| -I/W    | Write images as above but using input database path as output path for images\n";
      std::cout << "| -u      | Returns images encoded as string data (for library use)\n";
      std::cout << "| -U      | Returns json model data encoded string (for library use)\n";
      std::cout << "| -t      | Write transparent background png images (if supported)\n";
      std::cout << "| -m#     | Write movies of all loaded timesteps/windows #=fps(30) (if supported)\n";
      std::cout << "| -x#,#   | Set output image width, height (height will be calculated if omitted)\n";
      std::cout << "\n";
      std::cout << "|         | Data Export\n";
      std::cout << "| ------- | -----------\n";
      std::cout << "| -d#     | Export object id # to CSV vertices + values\n";
      std::cout << "| -j#     | Export object id # to JSON, if # omitted will output all compatible objects\n";
      std::cout << "| -g#     | Export object id # to GLDB (zlib compressed), if # omitted will output all compatible objects\n";
      std::cout << "| -G#     | Export object id # to GLDB (uncompressed), if # omitted will output all compatible objects\n";
      std::cout << "\n";
      std::cout << "|         | Window Settings\n";
      std::cout << "| ------- | ---------------\n";
      std::cout << "| -r#,#   | Resize initial viewer window width, height\n";
      std::cout << "| -h      | hidden window, will exit after running any provided input script and output options\n";
      exit(0);
    }

    char x;
    int vars[2];
    std::istringstream ss(args[i]);
    ss >> x;
    //Switches can be before or after files but not between
    if (x == '-' && args[i].length() > 1)
    {
      ss >> x;
      //Unused switches: bklnopqsu, BDEFHKLMOPUXYZ
      switch (x)
      {
      case 'a':
        //Automation mode:
        //return from run() immediately after loading
        //All actions to be performed by subsequent 
        //library calls
        session.automate = true;
        break;
      case 'f':
        ss >> initfigure;
        break;
      case 'z':
        //Downsample images
        ss >> vars[0];
        viewer->downSample(vars[0]);
        break;
      case 'r':
        ss >> vars[0] >> x >> vars[1];
        session.globals["resolution"] = json::array({vars[0], vars[1]});
        break;
      case 'N':
        session.globals["noload"] = true;
        break;
      case 'A':
        session.globals["hideall"] = true;
        break;
      case 'v':
        parseCommands("verbose on");
        break;
      case 'x':
        ss >> viewer->outwidth >> x >> viewer->outheight;
        break;
      case 'c':
        ss >> vars[0];
        //-1 : disable initial cache and clear step data on change
        //0  : disable initial cache, step data still retained
        //1  : enable initial cache
        if (vars[0] < 0)
          session.globals["clearstep"] = true;
        if (vars[0] <= 0)
          session.globals["cache"] = false;
        else if (vars[0] > 0)
          session.globals["cache"] = true;
        break;
      case 't':
        //Use alpha channel in png output
        session.globals["pngalpha"] = true;
        break;
      case 'y':
        //Swap y & z axis on import
        session.globals["swapyz"] = true;
        break;
      case 'T':
        //Split triangles
        ss >> vars[0];
        session.globals["trisplit"] = vars[0];
        break;
      case 'C':
        //Global camera
        session.globals["globalcam"] = true;
        break;
      case 'V':
        {
          float res[3];
          ss >> res[0] >> x >> res[1] >> x >> res[2];
          session.globals["volres"] = {res[0], res[1], res[2]};
        }
        break;
      case 'd':
        if (args[i].length() > 2) ss >> dumpid;
        dump = lucExportCSV;
        break;
      case 'J':
        if (args[i].length() > 2) ss >> dumpid;
        dump = lucExportJSONP;
        break;
      case 'j':
        if (args[i].length() > 2) ss >> dumpid;
        dump = lucExportJSON;
        break;
      case 'g':
        if (args[i].length() > 2) ss >> dumpid;
        dump = lucExportGLDBZ;
        break;
      case 'G':
        if (args[i].length() > 2) ss >> dumpid;
        dump = lucExportGLDB;
        break;
      case 'S':
        //Don't run default script
        defaultScript = "";
        break;
      case 'Q':
        //Quiet display, no status messages
        status = false;
        break;
      case 'h':
        //Invisible window requested
        viewer->visible = false;
        break;
      case 's':
        //Stereo context requested
        viewer->stereo = true;
        break;
      case 'W':
      case 'I':
        //Use db path for image output (pass through: also write image)
        dbpath = true;
      //Intended pass-through...
      case 'w':
      case 'i':
        //Image write
        writeimage = true;
        break;
      case 'm':
        //Movie write (fps optional)
        writemovie = 30;
        if (args[i].length() > 2) ss >> writemovie;
        break;
      case 'e':
        //Add new timesteps after loading files
        session.globals["filestep"] = true;
        break;
      default:
        //Attempt to interpret as timestep
        std::istringstream ss2(args[i]);
        ss2 >> x;
        if (startstep < 0)
          ss2 >> startstep;
        else
          ss2 >> endstep;
      }
      //Clear non file arguments
      args[i].clear();
    }
  }

  //Output and exit modes?
  if (writeimage || writemovie || dump > lucExportNone)
    viewer->visible = false;

  //Set default timestep if none specified
  if (startstep < 0) startstep = 0;
  //session.now = startstep;

  //Add default script
  if (defaultScript.length() && FileExists(defaultScript))
    args.push_back(defaultScript);

  //Process remaining args: Loads files, runs scripts, execute script commands
  for (unsigned int i=0; i<args.size(); i++)
  {
    if (args[i].length() == 0) continue;

    //Model data file, load if exists and recognised
    if (!loadFile(args[i]))
    {
      //Otherwise, attempt to run as script command
      //(will be queued for when viewer is opened if invalid before)
      parseCommand(args[i]);
    }
  }
}

void LavaVu::run(std::vector<std::string> args)
{
  //Reset defaults
  defaults();

  //App specific argument processing
  arguments(args);

  //Require a model from here on, set a default
  if (!amodel) defaultModel();

  if (writeimage || writemovie || dump > lucExportNone)
  {
    //Load vis data for each model and write image
    if (!writeimage && !writemovie && dump != lucExportJSON && dump != lucExportJSONP)
      viewer->isopen = true; //Skip open
    for (unsigned int m=0; m < models.size(); m++)
    {
      //Load the data
      loadModelStep(m, startstep, true);

      //Bounds checks
      int last = amodel->lastStep();
      if (endstep < 0) endstep = last;
      if (endstep < startstep) endstep = startstep;
      if (endstep > last) endstep = last;
      debug_print("StartStep %d EndStep %d\n", startstep, endstep);

      if (writeimage || writemovie)
      {
        //Loop through figures unless one set
        if (amodel->figures.size() == 0) amodel->addFigure();
        for (unsigned int f=0; f < amodel->figures.size(); f++)
        {
          if (initfigure != 0) f = initfigure-1;
          amodel->loadFigure(f);

          resetViews(true);
          viewer->display();
          //Read input script from stdin on first timestep
          viewer->pollInput();

          if (writeimage)
          {
            std::cout << "... Writing image(s) for model/figure " << session.global("caption")
                      << " Timesteps: " << startstep << " to " << endstep << std::endl;
          }
          if (writemovie)
          {
            std::cout << "... Writing movie for model/figure " << session.global("caption")
                      << " Timesteps: " << startstep << " to " << endstep << std::endl;
            //Other formats?? avi/mpeg4?
            encodeVideo("", writemovie);
          }

          //Write output
          writeSteps(writeimage, startstep, endstep);

          if (initfigure != 0) break;
        }
      }

      //Export data (this will be a bit broken for time varying data)
      Triangles* tris = (Triangles*)amodel->getRenderer(lucTriangleType);
      if (tris) tris->loadMesh();  //Optimise triangle meshes before export
      //Need to call merge to get timestep data
      //for (auto g : geometry)
      //  g->merge(startstep, startstep);
      DrawingObject* obj = NULL;
      std::vector<DrawingObject*> objs;
      if (dumpid > 0) obj = amodel->objects[dumpid-1];
      if (obj) objs.push_back(obj);
      exportData(dump, objs);
    }
  }
  else
  {
    //Cache data if enabled
    if (session.global("cache")) //>= amodel->timesteps.size())
    {
      debug_print("Caching all geometry data...\n");
      for (auto m : models)
        m->cacheLoad();
      model = -1;
    }

    //Load first model if not yet loaded
    if (model < 0)
      loadModelStep(0, startstep, true);
  }

  //If automation mode turned on, return at this point
  if (session.automate) return;

  //Start event loop
  if (viewer->visible)
    viewer->loop();
  else
  {
    //Read input script from stdin on first timestep
    viewer->pollInput();
    viewer->display();
  }
}

void LavaVu::clearAll(bool objects, bool colourmaps)
{
  //Clear all data
  if (!amodel) return;
  amodel->clearObjects(true);

  //Delete all objects? only works for active view/model
  if (objects)
    amodel->objects.clear();

  if (colourmaps)
    amodel->colourMaps.clear();

  if (aview)
    aview->clear(objects);

  //Reload, requires render thread, skip it,
  //or fix this call to be thread safe from python
  //loadModelStep(model, amodel->step());

  aobject = NULL;
}

std::string LavaVu::exportData(lucExportType type, std::vector<DrawingObject*> list, std::string filename)
{
  //Export data
  viewer->display();

  if (type == lucExportJSON || type == lucExportJSONP)
  {
    if (list.size() == 0)
      return jsonWriteFile(NULL, type == lucExportJSONP);

    for (unsigned int c=0; c<list.size(); c++)
      jsonWriteFile(list[c], type == lucExportJSONP);
  }
  else if (type == lucExportGLDB || type == lucExportGLDBZ)
  {
    if (list.size() == 0)
      amodel->writeDatabase(filename.c_str(), NULL, type == lucExportGLDBZ);
    else
    {
      for (unsigned int c=0; c<list.size(); c++)
        amodel->writeDatabase(filename.c_str(), list[c], type == lucExportGLDBZ);
    }
    return filename;
  }
  else if (type == lucExportCSV)
  {
    std::string files;
    for (unsigned int i=0; i < amodel->objects.size(); i++)
    {
      if (!amodel->objects[i]->skip && (list.size() == 0 || std::find(list.begin(), list.end(), amodel->objects[i]) != list.end()))
      {
        for (auto g : amodel->geometry)
        {
          std::ostringstream ss;
          g->dump(ss, amodel->objects[i]);

          std::string results = ss.str();
          if (results.size() > 0)
          {
            char filename[FILE_PATH_MAX];
            sprintf(filename, "%s_%s.%05d.csv", amodel->objects[i]->name().c_str(),
                    GeomData::names[g->type].c_str(), amodel->stepInfo());
            std::ofstream csv;
            csv.open(filename, std::ios::out | std::ios::trunc);
            std::cout << " * Writing object " << amodel->objects[i]->name() << " to " << filename << std::endl;
            csv << results;
            csv.close();
            files += std::string(filename) + ",";
          }
        }
      }
    }
    return files;
  }
  return "";
}

//Property containers now using json
//Parse lines with delimiter, ie: key=value
void LavaVu::parseProperties(std::string& properties, DrawingObject* obj)
{
  //Process all lines
  std::stringstream ss(properties);
  std::string line;
  while(std::getline(ss, line))
    parseProperty(line, obj);
};

bool LavaVu::parseProperty(std::string data, DrawingObject* obj)
{
  std::size_t pos = data.find("=");
  if (pos == std::string::npos) return false;
  //Set properties of selected object or view/globals
  std::string key = data.substr(0,pos);
  int reload = 0;

  //Special syntax allows object selection in property set command
  // obj:key=value
  if (key.find(":") != std::string::npos)
  {
    pos = data.find(":");
    std::string sel = data.substr(0,pos);
    parseCommand("select " + sel);
    data = data.substr(pos+1);
    obj = aobject;
  }

  //Extract key name, lowercase, everything before first non-alpha character
  std::transform(key.begin(), key.end(), key.begin(), ::tolower);
  std::size_t found = key.find_first_not_of("abcdefghijklmnopqrstuvwxyz");
  std::string rawkey = key;
  if (found != std::string::npos)
    rawkey = key.substr(0,found);

  //@prop : temporal property
  if (key.at(0) == '@')
  {
    if (session.now < 0) return false;
    data = data.substr(1);
    reload = session.parse(&session.timesteps[session.now]->properties, data);
    //Re-apply to active step
    Properties::mergeJSON(session.globals, session.timesteps[session.now]->properties.data);
  }
  else if (obj)
  {
    //Properties reserved for colourmaps can be set from any objects that use that map
    if (obj->colourMap && std::find(session.colourMapProps.begin(), session.colourMapProps.end(), rawkey) != session.colourMapProps.end())
    {
      reload = session.parse(&obj->colourMap->properties, data);
      if (verbose) std::cerr << "COLOURMAP " << std::setw(2) << obj->colourMap->name
                             << ", DATA: " << obj->colourMap->properties.data << std::endl;
    }
    else
    {
      reload = session.parse(&obj->properties, data);
      if (verbose) std::cerr << "OBJECT " << std::setw(2) << obj->name()
                             << ", DATA: " << obj->properties.data << std::endl;
      obj->setup();
    }
  }
  else
  {
    //Properties set globally or on view
    if (aview && std::find(session.viewProps.begin(), session.viewProps.end(), rawkey) != session.viewProps.end())
    {
      reload = session.parse(&aview->properties, data);
      if (verbose) std::cerr << "VIEW: " << std::setw(2) << aview->properties.data << std::endl;
      aview->importProps();
    }
    else
    {
      reload = session.parse(NULL, data);
      if (verbose) std::cerr << "GLOBAL: " << std::setw(2) << session.globals << std::endl;
    }
  }

  applyReload(obj, reload);
  return true;
}

void LavaVu::applyReload(DrawingObject* obj, int reload)
{
  //Reload required for prop set?
  //1 = redraw only
  //2 = full data reload
  //3 = full reload and view reset with autozoom
  if (amodel && reload > 0)
    amodel->reloadRedraw(obj, reload > 1);

  //Always do a view reset after prop change
  viewset = RESET_YES; //Force bounds check

  if (reload == 3)
    viewset = RESET_ZOOM; //Force check for resize and autozoom
}

void LavaVu::printProperties()
{
  //Show properties of selected object or view/globals
  if (aobject)
    std::cerr << "OBJECT " << aobject->name() << ", DATA: " << std::setw(2) << aobject->properties.data << std::endl;
  else
  {
    std::cerr << "VIEW: " << std::setw(2) << aview->properties.data << std::endl;
    std::cerr << "GLOBAL: " << std::setw(2) << session.globals << std::endl;
  }
}

void LavaVu::printDefaultProperties()
{
  //Show default properties
  std::cerr << std::setw(2) << session.defaults << std::endl;
}

void LavaVu::readRawVolume(const FilePath& fn)
{
  //Raw float volume data
  std::fstream file(fn.full.c_str(), std::ios::in | std::ios::binary);
  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (!file.is_open() || size <= 0) abort_program("File error %s\n", fn.full.c_str());
  std::vector<char> buffer(size);
  file.read(&buffer[0], size);
  file.close();

  int volres[3];
  Properties::toArray<int>(session.global("volres"), volres, 3);

  readVolumeCube(fn, (GLubyte*)buffer.data(), volres[0], volres[1], volres[2]);
}

void LavaVu::readXrwVolume(const FilePath& fn)
{
  //Xrw format volume data
  std::vector<char> buffer;
  unsigned int bytes = 0;
  float volscale[3];
  int volres[3];
#ifdef USE_ZLIB
  if (fn.type != "xrwu")
  {
    gzFile f = gzopen(fn.full.c_str(), "rb");
    gzread(f, (char*)volres, sizeof(int)*3);
    gzread(f, (char*)volscale, sizeof(float)*3);
    bytes = volres[0]*volres[1]*volres[2];
    buffer.resize(bytes);
    int chunk = 100000000; //Read in 100MB chunks
    int len, err;
    unsigned int offset = 0;
    do
    {
      if (chunk+offset > bytes) chunk = bytes - offset; //Last chunk?
      debug_print("Offset %ld Chunk %ld\n", offset, chunk);
      len = gzread(f, &buffer[offset], chunk);
      if (len != chunk) abort_program("gzread err: %s\n", gzerror(f, &err));

      offset += chunk;
    }
    while (offset < bytes);
    gzclose(f);
  }
  else
#endif
  {
    std::fstream file(fn.full.c_str(), std::ios::in | std::ios::binary);
    file.seekg(0, std::ios::end);
    bytes = file.tellg();
    file.seekg(0, std::ios::beg);
    file.read((char*)volres, sizeof(int)*3);
    file.read((char*)volscale, sizeof(float)*3);
    bytes -= sizeof(int)*3 + sizeof(float)*3;
    if (!file.is_open() || bytes <= 0) abort_program("File error %s\n", fn.full.c_str());
    buffer.resize(bytes);
    file.read(&buffer[0], bytes);
    file.close();
  }

  readVolumeCube(fn, (GLubyte*)buffer.data(), volres[0], volres[1], volres[2], volscale);
}

void LavaVu::readVolumeCube(const FilePath& fn, GLubyte* data, int width, int height, int depth, float* scale, int channels)
{
  //Loads full volume, optionally as slices
  Geometry* volumes = amodel->getRenderer(lucVolumeType);
  if (!volumes) return;
  bool splitslices = session.global("slicevolumes");
  bool dumpslices = session.global("slicedump");
  if (splitslices || dumpslices)
  {
    //Slicing is slower but allows sub-sampling and cropping
    GLubyte* ptr = data;
    int slicesize = width * height;
    json volss = session.global("volsubsample");
    //TODO: average samples instead of discarding
    int ss = volss[2];
    char path[FILE_PATH_MAX];
    for (int slice=0; slice<depth; slice++)
    {
      if (dumpslices)
      {
        //Write data to image file
        sprintf(path, "slice-%03d.png", slice);
        std::cout << path << std::endl;
        std::ofstream file(path, std::ios::binary);
        write_png(file, 1, width, height, ptr);
        file.close();
      }
      else if (slice % ss == 0) //Depth sub-sampling
      {
        readVolumeSlice(fn.base, ptr, width, height, channels);
      }
      ptr += slicesize;
    }
  }
  else
  {
    //Create volume object, or if static volume object exists, use it
    DrawingObject *vobj = volume;
    if (!vobj) vobj = aobject; //Use active object
    if (!vobj) vobj = new DrawingObject(session, fn.base);
    addObject(vobj);

    volumes->add(vobj);
    //Apply provided scaling factor to volume cube (only apply first time through)
    if (scale && !vobj->properties.has("volmax"))
    {
      float volmax[3];
      Properties::toArray<float>(vobj->properties["volmax"], volmax, 3);
      vobj->properties.data["volmax"] = json::array({scale[0]*volmax[0], scale[1]*volmax[0], scale[1]*volmax[2]});
      //std::cout << vobj->properties["volmax"] << std::endl;
    }

    //Load full cube
    int bytes = channels * width * height * depth;
    debug_print("Loading %u bytes, res %d %d %d\n", bytes, width, height, depth);
    volumes->read(vobj, bytes, lucLuminanceData, data, width, height, depth);
  }
}

void LavaVu::readVolumeSlice(const FilePath& fn)
{
  //png/jpg data
  static std::string path = "";
  if (!volume || path != fn.path)
  {
    path = fn.path; //Store the path, multiple volumes can be loaded if slices in different folders
    volume = NULL;  //Ensure new volume created
  }

  //Use the texture loader to read any supported image
  ImageLoader img(fn.full);
  img.read();
  if (img.source->pixels)
    readVolumeSlice(fn.base, img.source->pixels, img.source->width, img.source->height, img.source->channels);
  else
    debug_print("Slice load failed: %s\n", fn.full.c_str());
}

void LavaVu::readVolumeSlice(const std::string& name, GLubyte* imageData, int width, int height, int channels, bool flip)
{
  Geometry* volumes = amodel->getRenderer(lucVolumeType);
  if (!volumes) return;

  int outChannels = session.global("volchannels");
  json volss = session.global("volsubsample");
  int volres[3];
  Properties::toArray<int>(session.global("volres"), volres, 3);

  //Detect volume atlas and load (requires atlas/mosaic in filename)
  if ((name.find("atlas") != std::string::npos || name.find("mosaic") != std::string::npos) && width > volres[0] && height > volres[1]) // && width % (int)volres[0] == 0 && height % (int)volres[1] == 0)
  {
    debug_print("Attempting to load image as Volume Atlas %d %d => %f %f @ %d bpp\n", width, height, volres[0], volres[1], channels);
    RawImageFlip(imageData, width, height, channels);
    int tscanline = width * channels;
    int scanline = volres[1] * channels;
    for (int y=0; y < height; y += volres[1])
    {
      for (int x=0; x < width; x += volres[0])
      {
        //printf("%d,%d scan %d tscan %d offset %d\n", x, y, scanline, tscanline, y * tscanline + x * channels);
        ImageData subimage(volres[0], volres[1], channels);
        GLubyte* src = imageData + y * tscanline + x * channels;
        GLubyte* dst = subimage.pixels;
        for (int yy=0; yy < volres[1]; yy++)
        {
          memcpy(dst, src, scanline);
          dst += scanline;
          src += tscanline;
        }
        //Read the subimage slice
        readVolumeSlice(name, subimage.pixels, volres[0], volres[1], channels, false);
      }
    }

    return;
  }

  //Create volume object, or if static volume object exists, use it
  DrawingObject *vobj = aobject; //Use active object first
  static int count = 0;
  if (!vobj) vobj = volume; //Use default volume object
  if (!vobj)
  {
    count = 0;
    vobj = addObject(new DrawingObject(session, name));
  }
  volumes->add(vobj);

  //Flip if requested
  if (flip) RawImageFlip(imageData, width, height, channels);

  //Save static volume for loading multiple slices
  volume = vobj;
  count++;

  //Setup sub-sampling
  int w = ceil(width / (float)volss[0]);
  int h = ceil(height / (float)volss[1]);
  int wstep = volss[0];
  int hstep = volss[1];

  int offset=0;
  if (outChannels == 1)
  {
    //Convert to luminance (just using red channel now, other options in future)
    ImageData luminance(w, h, 1);
    GLubyte byte;
    for (int y=0; y<height; y+=hstep)
    {
      for (int x=0; x<width; x+=wstep)
      {
        //If input data rgb/rgba take highest of R/G/B
        //TODO: to a proper greyscale conversion or allow channel select
        assert(offset < w*h);
        byte = imageData[(y*width+x)*channels];
        if (channels == 3 || channels == 4)
        {
          if (imageData[(y*width+x)*channels+1] > byte)
            byte = imageData[(y*width+x)*channels+1];
          if (imageData[(y*width+x)*channels+2] > byte)
            byte = imageData[(y*width+x)*channels+2];
        }
        luminance.pixels[offset++] = byte;
      }
    }

    //Now using a container designed for byte data
    volumes->read(vobj, w * h, lucLuminanceData, luminance.pixels, w, h);
  }
  else if (outChannels == 3)
  {
    //Already in the correct format/layout with no sub-sampling, load directly
    if (channels == 3 && w == width && h == height)
      volumes->read(vobj, width*height*3, lucRGBData, imageData, width, height);
    else
    {
      //Convert LUM/RGBA to RGB
      ImageData rgb(w, h, 3);
      for (int y=0; y<height; y+=hstep)
      {
        for (int x=0; x<width; x+=wstep)
        {
          assert(offset+2 < w*h*3);
          if (channels == 1)
          {
            rgb.pixels[offset++] = imageData[y*width+x];
            rgb.pixels[offset++] = imageData[y*width+x];
            rgb.pixels[offset++] = imageData[y*width+x];
          }
          else if (channels >= 3)
          {
            rgb.pixels[offset++] = imageData[(y*width+x)*channels];
            rgb.pixels[offset++] = imageData[(y*width+x)*channels+1];
            rgb.pixels[offset++] = imageData[(y*width+x)*channels+2];
          }
        }
      }
      volumes->read(vobj, w*h*3, lucRGBData, rgb.pixels, w, h);
    }
  }
  else
  {
    //Already in the correct format/layout with no sub-sampling, load directly
    if (channels == 4 && w == width && h == height)
      volumes->read(vobj, width*height, lucRGBAData, imageData, width, height);
    else
    {
      //Convert LUM/RGB to RGBA
      ImageData rgba(w, h, 4);
      for (int y=0; y<height; y+=hstep)
      {
        for (int x=0; x<width; x+=wstep)
        {
          assert(offset+3 < w*h*4);
          if (channels == 1)
          {
            rgba.pixels[offset++] = imageData[y*width+x];
            rgba.pixels[offset++] = imageData[y*width+x];
            rgba.pixels[offset++] = imageData[y*width+x];
            rgba.pixels[offset++] = 255;
          }
          else if (channels >= 3)
          {
            rgba.pixels[offset++] = imageData[(y*width+x)*channels];
            rgba.pixels[offset++] = imageData[(y*width+x)*channels+1];
            rgba.pixels[offset++] = imageData[(y*width+x)*channels+2];
            //if (x%16==0&&y%16==0) printf("RGBA %d %d %d\n", rgba.pixels[offset-3], rgba.pixels[offset-2], rgba.pixels[offset-1]);
            if (channels == 4)
              rgba.pixels[offset++] = imageData[(y*width+x)*channels+3];
            else
              rgba.pixels[offset++] = 255;
          }
        }
      }
      volumes->read(vobj, w*h, lucRGBAData, rgba.pixels, w, h);
    }
  }
  std::cout << "Slice loaded " << count << " : " << width << "," << height << " channels: " << outChannels
            << " ==> " << w << "," << h << " channels: " << outChannels << std::endl;
}

void LavaVu::readVolumeTIFF(const FilePath& fn)
{
#ifdef HAVE_LIBTIFF
  TIFF* tif = TIFFOpen(fn.full.c_str(), "r");
  if (tif)
  {
    unsigned int width, height;
    size_t npixels;
    int channels = 4;
    GLubyte* imageData;
    int count = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    npixels = width * height;
    imageData = (GLubyte*)_TIFFmalloc(npixels * channels * sizeof(GLubyte));
    if (imageData)
    {
      json volss = session.global("volsubsample");
      int d = TIFFNumberOfDirectories(tif);
      int ds = volss[2];
      if (d > 1) std::cout << "TIFF contains " << d << " pages, sub-sampling z " << ds << std::endl;
      do
      {
        if (TIFFReadRGBAImage(tif, width, height, (uint32*)imageData, 0))
        {
          //Subsample
          if (count % ds != 0) {count++; continue;}
          readVolumeSlice(fn.base, imageData, width, height, channels, true);
        }
        count++;
      }
      while (TIFFReadDirectory(tif));
      _TIFFfree(imageData);
    }
    TIFFClose(tif);
  }
#else
  abort_program("Require libTIFF to load TIFF images\n");
#endif
}

void LavaVu::createDemoVolume(unsigned int width, unsigned int height, unsigned int depth)
{
  //Create volume object
  DrawingObject *vobj = NULL;
  Geometry* volumes = amodel->getRenderer(lucVolumeType);
  if (!volumes) return;
  if (!vobj)
  {
    unsigned int samples = (width+height+depth)*0.6;
    if (samples < 128) samples = 128;
    vobj = new DrawingObject(session, "volume");
    json props = {
      {"density",    50},
      {"samples",    samples}
    };
    vobj->properties.merge(props);
    addObject(vobj);

    //Demo colourmap, depth 
    ColourMap* cmap = addColourMap("volume", "#000000:0 #ff0000 #ff8800 #ffff77 #00ffff #3333ff");
    vobj->properties.data["colourmap"] = cmap->name;
    //Add colour bar display
    colourBar(vobj);
  }

  unsigned int block = width/10;
  size_t npixels = width*height;
  int channels = 4;
  GLubyte* imageData;
  npixels = width * height;
  imageData = (GLubyte*)malloc(npixels * channels * sizeof(GLubyte));
  if (imageData)
  {
    bool val;
    for (unsigned int z=0; z<depth; z++)
    {
      //Add new store for each slice
      //Subsample
      GLuint* bp = (GLuint*)imageData;
      for (unsigned int y=0; y<height; y ++)
      {
        for (unsigned int x=0; x<width; x ++)
        {
          assert((bp-(GLuint*)imageData) < width * height);
          if (z/block%2==0 || x/block%2==0 || y/block%2==0)
            val = false;
          else
            val = true;
          memset(bp, val ? 255 : 0, 4);
          bp++;
        }
      }

      if (z > 0) volumes->add(vobj);
      volumes->read(vobj, width * height, lucRGBAData, imageData, width, height);
      if (verbose) std::cerr << "SLICE LOAD " << z << " : " << width << "," << height << " channels: " << channels << std::endl;
      //Demo colour values - depth, these aren't used for volume render but can be used to plot an isosurface
      float colourvals[npixels];
      std::fill(colourvals + 0, colourvals + npixels, z/(float)depth);
      volumes->read(vobj, npixels, colourvals, "depth");
    }
    free(imageData);
  }
}

void LavaVu::readHeightMap(const FilePath& fn)
{
  int rows, cols, size = 2, subsample;
  int byteorder = 0;   //Little endian default
  float xmap, ymap, xdim, ydim;
  int geomtype = lucTriangleType;
  //int geomtype = lucGridType;
  int header = 0;
  float downscale = 1;
  std::string texfile;

  //Can only parse dem format wth ers or hdr header

  char ersfilename[FILE_PATH_MAX];
  char hdrfilename[FILE_PATH_MAX];
  snprintf(ersfilename, FILE_PATH_MAX-1, "%s%s.ers", fn.path.c_str(), fn.base.c_str());
  snprintf(hdrfilename, FILE_PATH_MAX-1, "%s%s.hdr", fn.path.c_str(), fn.base.c_str());

  //Parse ERS file
  std::ifstream ersfile(ersfilename, std::ios::in);
  if (ersfile.is_open())
  {
    PropertyParser props = PropertyParser(ersfile, '=');
    header = props.Int("headeroffset");
    rows = props.Int("NrOfLines");
    cols = props.Int("NrOfCellsPerLine");
    std::string temp;
    temp = props["CellType"];
    if (temp.compare("IEEE4ByteReal") == 0) size = 4;
    xdim = props.Float("Xdimension");
    ydim = props.Float("Ydimension");
    xmap = props.Float("Eastings");
    ymap = props.Float("Northings");
    subsample = props.Int("Subsample", 1);
    texfile = props["texture"];
    temp = props["ByteOrder"];
    if (temp.compare("LSBFirst") != 0) byteorder = 1;   //Big endian
  }
  else
  {
    std::ifstream hdrfile(hdrfilename, std::ios::in);

    if (!hdrfile.is_open())
    {
      debug_print("HEADER FILE ERROR %s or %s not found\n", hdrfilename, ersfilename);
      return;
    }

    PropertyParser props = PropertyParser(hdrfile);
    rows = props.Int("NROWS");
    cols = props.Int("NCOLS");
    size = props.Int("NBITS") / 8;   //Convert from bits to bytes
    xdim = props.Float("XDIM");
    ydim = props.Float("YDIM");
    xmap = props.Float("ULXMAP");
    ymap = props.Float("ULYMAP");
    std::string temp = props["BYTEORDER"];
    if (temp.compare("M") == 0) byteorder = 1;   //Big endian
    subsample = props.Int("SUBSAMPLE", 1);
    geomtype = props.Int("GEOMETRY", lucTriangleType);
    texfile = props["texture"];

    //HACK: scale down height for lat/lon data
    downscale = 100000;
  }

  float min[3], max[3];
  Properties::toArray<float>(aview->properties["min"], min, 3);
  Properties::toArray<float>(aview->properties["max"], max, 3);
  float range[3] = {0,0,0};

  min[0] = xmap;
  max[0] = xmap + (cols * xdim);
  min[1] = 0;
  max[1] = 1000;
  min[2] = ymap - (rows * xdim); //Northing decreases with latitude
  max[2] = ymap;
  range[0] = fabs(max[0] - min[0]);
  range[2] = fabs(max[2] - min[2]);

  session.globals["caption"] = fn.base;

  //Create a height map grid
  int sx=cols, sz=rows;
  debug_print("Height dataset %d x %d Sampling at X,Z %d,%d\n", sx, sz, sx / subsample, sz / subsample);
  //opacity [0,1]
  DrawingObject *obj;
  std::string props = "colour=[238,238,204]\ncullface=0\ntexturefile=" + texfile + "\n";
  obj = addObject(new DrawingObject(session, fn.base, props));
  int gridx = ceil(sx / (float)subsample);
  int gridz = ceil(sz / (float)subsample);

  Vec3d vertex;
  vertex[0] = min[0];
  vertex[2] = max[2];

  std::ifstream file(fn.full.c_str(), std::ios::in|std::ios::binary);
  if (!file.is_open())
  {
    debug_print("DEM FILE ERROR %s\n", fn.full.c_str());
    return;
  }

  Geometry* active = amodel->getRenderer((lucGeometryType)geomtype);

  for (int j=0; j<sz; j++)
  {
    file.seekg(header + j*size*cols, std::ios::beg);

    if (j % subsample != 0) continue;

    for (int i=0; i<sx; i++)
    {
      union bheight
      {
        char  bytes[4];
        short sheight;
        float fheight;
      } h1;

      if (file.eof()) debug_print("EOF!\n");
      file.read(h1.bytes, size);

      if (i % subsample != 0) continue;

      if (byteorder == 1)  //Motorola (big endian)
      {
        //Switch endianness
        for (int x=0; x<size/2; x++)
        {
          char temp = h1.bytes[x];
          h1.bytes[x] = h1.bytes[size-x-1];
          h1.bytes[size-x-1] = temp;
        }
      }

      //vertex[0] = min[0] + range[0] * i/(float)sx;
      //vertex[2] = min[2] + range[2] * j/(float)sz;

      if (size == 4)
        vertex[1] = h1.fheight;
      else
        vertex[1] = (float)h1.sheight;

      float colourval = vertex[1];
      if (vertex[1] <= -9999)
      {
        //vertex[1] = -30; //Nodata
        vertex[1] = 0; //Nodata
        colourval = -20;
        if (min[1] < vertex[1])
        {
          vertex[1] = min[1];
          colourval = min[1];
        }
      }
      vertex[1] /= downscale;

      if (vertex[1] < min[1]) min[1] = vertex[1];
      if (vertex[1] > max[1]) max[1] = vertex[1];

      //Add grid point
      active->read(obj, 1, lucVertexData, vertex.ref(), gridx, gridz);
      //Colour by height
      active->read(obj, 1, &colourval, "height");

      vertex[0] += xdim * subsample;
    }
    vertex[0] = min[0];
    vertex[2] -= ydim * subsample;
  }
  file.close();

  range[1] = max[1] - min[1];
  debug_print("Sampled %d values, min height %f max height %f\n", (sx / subsample) * (sz / subsample), min[1], max[1]);
  //debug_print("Range %f,%f,%f to %f,%f,%f\n", min[0], min[1], min[2], max[0], max[1], max[2]);

  debug_print("X min %f max %f range %f\n", min[0], max[0], range[0]);
  debug_print("Y min %f max %f range %f\n", min[1], max[1], range[1]);
  debug_print("Z min %f max %f range %f\n", min[2], max[2], range[2]);
}

void LavaVu::readHeightMapImage(const FilePath& fn)
{
  ImageLoader image(fn.full);
  image.read();

  if (!image.source || !image.source->pixels) return;

  int geomtype = lucTriangleType;
  //int geomtype = lucGridType;
  Geometry* active = amodel->getRenderer((lucGeometryType)geomtype);

  float heightrange = 10.0;
  float min[3] = {0, 0, 0};
  float max[3] = {(float)image.source->width, heightrange, (float)image.source->height};

  session.globals["caption"] = fn.base;

  //Create a height map grid
  std::string texfile = fn.base + "-texture." + fn.ext;
  DrawingObject *obj;
  std::string props = "cullface=0\ntexturefile=" + texfile + "\n";
  obj = addObject(new DrawingObject(session, fn.base, props));

  //Default colourmap
  ColourMap* cmap = addColourMap("elevation", "darkgreen yellow brown");
  obj->properties.data["colourmap"] = cmap->name;

  Vec3d vertex;

  //Use red channel as luminance for now
  for (unsigned int z=0; z<image.source->height; z++)
  {
    vertex[2] = z;
    for (unsigned int x=0; x<image.source->width; x++)
    {
      vertex[0] = x;
      vertex[1] = heightrange * image.source->pixels[(z*image.source->width+x)*image.source->channels] / 255.0;

      float colourval = vertex[1];

      if (vertex[1] < min[1]) min[1] = vertex[1];
      if (vertex[1] > max[1]) max[1] = vertex[1];

      //Add grid point
      active->read(obj, 1, lucVertexData, vertex.ref(), image.source->width, image.source->height);
      //Colour by height
      active->read(obj, 1, &colourval, "height");
    }
  }
}

void readOBJ_material(const FilePath& fn, const tinyobj::material_t& material, DrawingObject* obj, Geometry* tris, bool verbose)
{
  //std::cerr << "Applying material : " << material.name << std::endl;

  //Use the diffuse property as the colour/texture
  std::string texpath = material.diffuse_texname;
  if (texpath.length() == 0)
  {
    texpath = material.ambient_texname;
    if (texpath.length() == 0)
    {
      texpath = material.specular_texname;
      if (texpath.length() > 0 && verbose)
        std::cerr << "Applying specular texture: " << texpath << std::endl;
    }
    else if (verbose)
      std::cerr << "Applying ambient texture: " << texpath << std::endl;
  }
  else if (verbose)
    std::cerr << "Applying diffuse texture: " << texpath << std::endl;
  if (material.diffuse_texname.length() > 0)
  {
    if (fn.path.length() > 0)
      texpath = fn.path + "/" + texpath;

    //Add per-object texture
    Texture_Ptr texture = std::make_shared<ImageLoader>(texpath);
    tris->setTexture(obj, texture);
  }
  else
  {
    Colour c;
    c.r = material.diffuse[0] * 255;
    c.g = material.diffuse[1] * 255;
    c.b = material.diffuse[2] * 255;
    c.a = material.dissolve * 255;
    if (c.a == 0.0) c.a = 255;
    //std::cout << c << std::endl;
    tris->read(obj, 1, lucRGBAData, &c.value);
  }
}

void LavaVu::readOBJ(const FilePath& fn)
{
  //Use tiny_obj_loader to load a model
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fn.full.c_str(), fn.path.c_str());
  if (!err.empty())
  {
    std::cerr << "Error loading OBJ file: " << fn.full << " - " << err << std::endl;
    //return; //Could just be a warning, continue anyway
  }

  if (verbose)
  {
    std::cerr << "# of shapes    : " << shapes.size() << std::endl;
    std::cerr << "# of materials : " << materials.size() << std::endl;
    std::cerr << "# of vertices : " << attrib.vertices.size() << std::endl;
    std::cerr << "# of normals : " << attrib.normals.size() << std::endl;
    std::cerr << "# of texcoords : " << attrib.texcoords.size() << std::endl;
  }

  //Add single drawing object per file, if one is already active append to it
  DrawingObject* tobj = aobject;
  if (!tobj) tobj = addObject(new DrawingObject(session, fn.base, "geometry=triangles\ncolour=[128,128,128]\n"));
  Geometry* tris = lookupObjectRenderer(tobj);
  if (!tris) return;

  for (size_t i = 0; i < shapes.size(); i++)
  {
    //Strip path from name
    size_t last_slash = shapes[i].name.find_last_of("\\/");
    if (std::string::npos != last_slash)
      shapes[i].name = shapes[i].name.substr(last_slash + 1);

    debug_print("shape[%ld].name = %s\n", i, shapes[i].name.c_str());
    debug_print("Size of shape[%ld].material_ids: %ld\n", i, shapes[i].mesh.material_ids.size());

    //Add new triangles data store to object
    tris->add(tobj);

    //Load first material as default for object
    if (materials.size() > 0 && shapes[i].mesh.material_ids.size() > 0)
      readOBJ_material(fn, materials[shapes[i].mesh.material_ids[0]], tobj, tris, verbose);

    //Default is to load the indexed triangles and provided normals
    //Can be overridden by setting trisplit (-T#)
    //Setting to 1 will calculate our own normals and optimise mesh
    //Setting > 1 also divides triangles into smaller pieces first
    int trisplit = session.global("trisplit");
    bool swapY = session.global("swapyz");
    debug_print("Loading: shape[%ld].indices: %ld\n", i, shapes[i].mesh.indices.size());
    int vi;
    //Load
    int voffset = 0;
    int current_material_id = 0;
    for (size_t f=0; f < shapes[i].mesh.indices.size()-2; f+=3)
    {
      //Get the material, if it has changed, load the new data and add an element
      if (materials.size() > 0 && shapes[i].mesh.material_ids.size() > 1)
      {
        int material_id = shapes[i].mesh.material_ids[f/3];
        if (material_id < 0 || material_id >= materials.size())
        {
          // Invaid material ID. Use default material.
          //printf("~~~ !!! Material id %d is invalid\n", material_id);
          material_id = 0;
        }

        if (current_material_id != material_id)
        {
          //printf("%d current material %d == %d\n", f, current_material_id, material_id);
          tris->add(tobj); //Start new container
          readOBJ_material(fn, materials[material_id], tobj, tris, verbose);
          voffset = 0;
          current_material_id = material_id;
        }
      }

      if (trisplit == 0)
      {
        //Use the provided indices
        //re-index to use indices for this shape only (the global list is for all shapes)
        tinyobj::index_t ids[3] = {shapes[i].mesh.indices[f], shapes[i].mesh.indices[f+1], shapes[i].mesh.indices[f+2]};
        for (int c=0; c<3; c++)
        {
          vi = 3*ids[c].vertex_index;
          if (swapY)
          {
            float vert[3] = {attrib.vertices[vi], attrib.vertices[vi+2], attrib.vertices[vi+1]};
            tris->read(tobj, 1, lucVertexData, vert);
          }
          else
            tris->read(tobj, 1, lucVertexData, &attrib.vertices[vi]);
          tris->read(tobj, 1, lucIndexData, &voffset);
          voffset++;
          if (attrib.texcoords.size())
            tris->read(tobj, 1, lucTexCoordData, &attrib.texcoords[2*ids[c].texcoord_index]);
          if (attrib.normals.size())
          {
            //Some files skip the normal index, so assume it is the same as vertex index
            int nidx = 3*ids[c].normal_index;
            if (nidx < 0) nidx = 3*ids[c].vertex_index;
            if (swapY)
            {
              float norm[3] = {attrib.normals[nidx], attrib.normals[nidx+2], attrib.normals[nidx+1]};
              tris->read(tobj, 1, lucNormalData, norm);
            }
            else
              tris->read(tobj, 1, lucNormalData, &attrib.normals[nidx]);
          }
        }
      }
      //TriSplit enabled, don't use built in index data
      else if (attrib.texcoords.size())
      {
        float* v0 = &attrib.vertices[shapes[i].mesh.indices[f].vertex_index*3];
        float* v1 = &attrib.vertices[shapes[i].mesh.indices[f+1].vertex_index*3];
        float* v2 = &attrib.vertices[shapes[i].mesh.indices[f+2].vertex_index*3];
        float* t0 = &attrib.texcoords[shapes[i].mesh.indices[f].texcoord_index*2];
        float* t1 = &attrib.texcoords[shapes[i].mesh.indices[f+1].texcoord_index*2];
        float* t2 = &attrib.texcoords[shapes[i].mesh.indices[f+2].texcoord_index*2];
        float tv0[5] = {v0[0], v0[1], v0[2], t0[0], t0[1]};
        float tv1[5] = {v1[0], v1[1], v1[2], t1[0], t1[1]};
        float tv2[5] = {v2[0], v2[1], v2[2], t2[0], t2[1]};
        tris->addTriangle(tobj, tv0, tv1, tv2, trisplit, swapY, true);
      }
      else
      {
        tris->addTriangle(tobj,
                   &attrib.vertices[shapes[i].mesh.indices[f].vertex_index*3],
                   &attrib.vertices[shapes[i].mesh.indices[f+1].vertex_index*3],
                   &attrib.vertices[shapes[i].mesh.indices[f+2].vertex_index*3],
                   trisplit, swapY);
      }
    }
  }
  return;

  for (size_t i = 0; i < materials.size(); i++)
  {
    printf("material[%ld].name = %s\n", i, materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n", materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
    printf("  material.Kd = (%f, %f ,%f)\n", materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
    printf("  material.Ks = (%f, %f ,%f)\n", materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
    printf("  material.Tr = (%f, %f ,%f)\n", materials[i].transmittance[0], materials[i].transmittance[1], materials[i].transmittance[2]);
    printf("  material.Ke = (%f, %f ,%f)\n", materials[i].emission[0], materials[i].emission[1], materials[i].emission[2]);
    printf("  material.Ns = %f\n", materials[i].shininess);
    printf("  material.Ni = %f\n", materials[i].ior);
    printf("  material.dissolve = %f\n", materials[i].dissolve);
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(materials[i].unknown_parameter.end());
    for (; it != itEnd; it++)
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    printf("\n");
  }
}

void LavaVu::createDemoModel(unsigned int numpoints)
{
  Geometry* tris = amodel->getRenderer(lucTriangleType);
  Geometry* grid = NULL; //amodel->getRenderer(lucGridType);
  Geometry* points = amodel->getRenderer(lucPointType);
  Geometry* lines = amodel->getRenderer(lucLineType);
  float RANGE = 2.f;
  float min[3] = {-RANGE,-RANGE,-RANGE};
  float max[3] = {RANGE,RANGE,RANGE};
  //float dims[3] = {RANGE*2.f,RANGE*2.f,RANGE*2.f};
  session.globals["caption"] = "Test Pattern";

  //Demo colourmap, distance from model origin
  ColourMap* cmap = addColourMap("particles", "#66bb33 #00ff00 #3333ff #00ffff #ffff77 #ff8800 #ff0000 #000000");

  //Add points object
  if (points)
  {
    DrawingObject* obj = addObject(new DrawingObject(session, "particles", "opacity=0.75\nlit=0\n"));
    obj->properties.data["colourmap"] = cmap->name;
    //Add colour bar display
    colourBar(obj);
    unsigned int pointsperswarm = numpoints/4; //4 swarms
    for (unsigned int i=0; i < numpoints; i++)
    {
      float colour, ref[3];
      ref[0] = min[0] + (max[0] - min[0]) * session.random_d();
      ref[1] = min[1] + (max[1] - min[1]) * session.random_d();
      ref[2] = min[2] + (max[2] - min[2]) * session.random_d();

      //Demo colourmap value: distance from model origin
      colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));

      points->read(obj, 1, lucVertexData, ref);
      points->read(obj, 1, &colour, "demo colours");

      if (i % pointsperswarm == pointsperswarm-1 && i != numpoints-1)
          points->add(obj);
    }
  }

  //Add lines
  if (lines)
  {
    DrawingObject* obj = addObject(new DrawingObject(session, "line-segments", "lit=0\n"));
    obj->properties.data["colourmap"] = cmap->name;
    for (int i=0; i < 50; i++)
    {
      float colour, ref[3];
      ref[0] = min[0] + (max[0] - min[0]) * session.random_d();
      ref[1] = min[1] + (max[1] - min[1]) * session.random_d();
      ref[2] = min[2] + (max[2] - min[2]) * session.random_d();

      //Demo colourmap value: distance from model origin
      colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));

      lines->read(obj, 1, lucVertexData, ref);
      lines->read(obj, 1, &colour, "demo colours");
    }
  }

  //Add some triangles
  if (tris || grid)
  {
    float verts[3][12] = {
      {-2,-2,0,  2,-2,0,  -2,2,0,  2,2,0},
      {-2,0,-2,  2,0,-2,  -2,0,2,  2,0,2},
      {0,-2,-2,  0,2,-2,   0,-2,2, 0,2,2}
    };
    char axischar[3] = {'Z', 'Y', 'X'};
    for (int i=0; i<3; i++)
    {
      char label[64];
      sprintf(label, "%c-cross-section", axischar[i]);
      DrawingObject* obj = addObject(new DrawingObject(session, label, "opacity=0.5\n"));
      Colour c;
      c.value = (0xff000000 | 0xff<<(8*i));
      obj->properties.data["colour"] = c.toJson();
      if (grid)
      {
        //Read corners as quads
        grid->read(obj, 4, lucVertexData, &verts[i][0], 2, 2);
      }
      else
      {
        //Read 2 triangles and split recursively for a nicer surface
        tris->addTriangle(obj, &verts[i][0], &verts[i][3], &verts[i][9], 8);
        tris->addTriangle(obj, &verts[i][0], &verts[i][9], &verts[i][6], 8);
      }
    }
  }
}

DrawingObject* LavaVu::addObject(DrawingObject* obj)
{
  if (!amodel || amodel->views.size() == 0) abort_program("No model/view defined!\n");
  if (!aview) aview = amodel->views[0];

  //Add to the active viewport if not already present
  aview->addObject(obj);

  //Add to model master list
  amodel->addObject(obj);

  return obj;
}

void LavaVu::open(int width, int height)
{
  //Init geometry containers
  for (auto g : amodel->geometry)
    g->init();

  //Initialise all viewports to window size
  for (unsigned int v=0; v<amodel->views.size(); v++)
    amodel->views[v]->port(width, height);

  reloadShaders();

  //Load fonts
  session.fonts.init(binpath);
}

void LavaVu::reloadShaders()
{
  for (unsigned int type=0; type <= lucMaxType; type++)
  {
    if (session.shaders[type])
      session.shaders[type] = NULL;
  }

  for (unsigned int i=0; i<amodel->objects.size(); i++)
  {
    if (amodel->objects[i]->shader)
      amodel->objects[i]->shader = nullptr;
  }

  resetViews();
  amodel->redraw();
}

void LavaVu::resize(int new_width, int new_height)
{
  if (new_width > 0)
  {
    float size0 = viewer->width * viewer->height;
    if (size0 > 0)
    {
      std::ostringstream ss;
      ss << "resize " << new_width << " " << new_height;
      history.push_back(ss.str());
    }
  }

  //Set resolution
  session.globals["resolution"] = {new_width, new_height};

  amodel->redraw();
}

void LavaVu::close()
{
  if (amodel)
  {
    //Wait until all sort threads done
    for (auto g : amodel->geometry)
      std::lock_guard<std::mutex> guard(g->sortmutex);
  }

  //Need to call display to switch contexts before freeing OpenGL resources
  if (viewer)
    viewer->display(false);

  //Clear models - will delete contained views, objects
  //Should free all allocated memory
  for (unsigned int i=0; i < models.size(); i++)
    delete models[i];
  models.clear();

  aview = NULL;
  amodel = NULL;
  aobject = NULL;

  if (axis) delete axis;
  if (rulers) delete rulers;
  if (border) delete border;

  axis = NULL;
  border = NULL;
  rulers = NULL;
}

//Called when model loaded/changed, updates all views settings
void LavaVu::resetViews(bool autozoom)
{
  viewset = RESET_NO;

  //Setup view(s) for new model dimensions
  int curview = view;
  for (unsigned int v=0; v < amodel->views.size(); v++)
    viewSelect(v, true, autozoom);
  //Restore active
  viewSelect(curview);

  //Flag redraw required
  amodel->redraw();

  //Set viewer title
  std::stringstream title;
  std::string name = session.global("caption");
  std::string vptitle = aview->properties["title"];
  if (vptitle.length() > 0)
    title << vptitle;
  else
    title << "LavaVu";
  if (name.length() > 0)
    title << " (" << name << ")";

  if (amodel->timesteps.size() > 1)
    title << " - timestep " << std::setw(5) << std::setfill('0') << amodel->stepInfo();

  //Update title
  viewer->title(title.str());

  //Apply any prop changes
  aview->importProps();
}

//Called when view changed
void LavaVu::viewSelect(int idx, bool setBounds, bool autozoom)
{
  //Set a default viewport/camera if none
  assert(amodel->views.size());
  if (idx < 0) idx = 0;
  view = idx;
  if (view < 0) view = amodel->views.size() - 1;
  if (view >= (int)amodel->views.size()) view = 0;

  aview = amodel->views[view];

  //Called when timestep/model changed (new model data)
  //Set model size from geometry / bounding box and apply auto zoom
  //- View bounds used for camera calc and border (view->min/max)
  //- Actual bounds used by geometry clipping etc (session.min/max)
  //NOTE: sometimes we can reach this call before the GL context is created, hence the check
  if (viewer->isopen && setBounds)
  {
    //Ensure correct context selected!
    viewer->display(false);

    //Auto-calc data ranges
    amodel->setup();

    float min[3], max[3];
    Properties::toArray<float>(aview->properties["min"], min, 3);
    Properties::toArray<float>(aview->properties["max"], max, 3);

    //Calculate the model bounds by contained geometry
    amodel->calculateBounds(aview, min, max);

    //Set viewport based on window size
    aview->port(viewer->width, viewer->height);

    //Update the model bounding box - use global bounds if provided and sane in at least 2 dimensions
    if (fabs(max[0]-min[0]) > EPSILON && fabs(max[1]-min[1]) > EPSILON)
    {
      debug_print("Applied Model bounds %f,%f,%f - %f,%f,%f from global properties\n",
                  min[0], min[1], min[2],
                  max[0], max[1], max[2]);
      aview->init(false, min, max);
    }
    else
    {
      debug_print("Applied Model bounds %f,%f,%f - %f,%f,%f from geometry\n",
                  amodel->min[0], amodel->min[1], amodel->min[2],
                  amodel->max[0], amodel->max[1], amodel->max[2]);
      aview->init(false, amodel->min, amodel->max);
    }

    //Update actual bounding box max/min/range - it is possible for the view box to be smaller
    clearMinMax(session.min, session.max);
    compareCoordMinMax(session.min, session.max, amodel->min);
    compareCoordMinMax(session.min, session.max, amodel->max);
    //Apply viewport/global dims if valid
    if (min[0] != max[0] && min[1] != max[1])
    {
      compareCoordMinMax(session.min, session.max, min);
      compareCoordMinMax(session.min, session.max, max);
    }
    getCoordRange(session.min, session.max, session.dims);
    debug_print("Calculated Actual bounds %f,%f,%f - %f,%f,%f \n",
                session.min[0], session.min[1], session.min[2],
                session.max[0], session.max[1], session.max[2]);

    // Apply step autozoom if set (applied based on detected bounding box)
    int zstep = aview->properties["zoomstep"];
    if (autozoom && zstep > 0 && amodel->step() % zstep == 0)
      aview->zoomToFit();

    //Apply auto rotate once all commands processed
    if (aview->initialised && (session.min[0] == session.max[0] || session.min[1] == session.max[1]))
      parseCommand("autorotate");
  }
  else
  {
    //Set view on geometry objects only, no boundary check
    for (auto g : amodel->geometry)
      g->setup(aview);
  }

  aview->setBackground(); //Update background colour
}

void LavaVu::viewApply(int idx)
{
  viewSelect(idx);
  // View transform
  
  //std::string title = aview->properties["title"];
  //printf("### Displaying viewport %d %s at %d,%d %d x %d (%f %f, %fx%f)\n", idx, title.c_str(), 
  //       aview->xpos, aview->ypos, aview->width, aview->height, aview->x, aview->y, aview->w, aview->h);

  if (aview->autozoom)
  {
    aview->projection(EYE_CENTRE);
    aview->zoomToFit();
  }
  else
    aview->apply();
  GL_Error_Check;

  //Set viewport based on window size
  aview->port(viewer->width, viewer->height);
  GL_Error_Check;

  //Set GL colours
  glClearColor(aview->background.r/255.0, aview->background.g/255.0, aview->background.b/255.0, 0);
  glColor3ubv(aview->textColour.rgba);

  // Clear viewport
  GL_Error_Check;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

bool LavaVu::sort(bool sync)
{
  //Run the renderer sort functions
  //by default in a thread
  if (sync)
  {
    //Synchronous immediate sort
    for (auto g : amodel->geometry)
    {
      std::lock_guard<std::mutex> guard(g->sortmutex);
      //Not required if reload flagged, will be done in update()
      if (!g->reload)
        g->sort();
    }
    return true;
  }

  //Use sorting thread
  if (!sort_thread.joinable())
  {
    sort_thread = std::thread([&]
    {
      while (true)
      {
        //Wait for sort request
        std::unique_lock<std::mutex> lk(sort_mutex);
        sortcv.wait(lk, [&]{return sorting || viewer->quitProgram;});

        if (viewer->quitProgram)
          return;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (auto g : amodel->geometry)
        {
          std::lock_guard<std::mutex> guard(g->sortmutex);
          //Not required if reload flagged, will be done in update()
          if (!g->reload)
            g->sort();
        }

        if (!animate)
          queueCommands("display");

        // Manual unlocking is done before notifying, to avoid waking up
        // the waiting thread only to block again (see notify_one for details)
        sorting = false;
        lk.unlock();
        sortcv.notify_one();
      }
    });
  }

  //Notify worker thread ready to sort (if not already sorting)
  if (sort_mutex.try_lock())
  {
    sorting = true;
    sort_mutex.unlock();
  }
  else
    return false;
  sortcv.notify_one();
  return true;
}

// Render
void LavaVu::display(bool redraw)
{
  if (!viewer->isopen) return;

  //Lock the state mutex, prevent updates while drawing
  std::lock_guard<std::mutex> guard(session.mutex);

  clock_t t1 = clock();

  if (session.globals.count("resolution") && !viewer->imagemode && !session.omegalib)
  {
    //Resize if required
    int res[2];
    Properties::toArray<int>(session.global("resolution"), res, 2);
    if (res[0] > 0 && res[1] > 0 && (res[0] != viewer->width || res[1] != viewer->height))
    {
      //viewer->setsize(res[0], res[1]);
      std::stringstream ss;
      ss << "resize " << res[0] << " " << res[1];
      queueCommands(ss.str());
      aview->initialised = false; //Force initial autozoom and redisplay
      return;
    }
  }

  //View not yet initialised, call resetViews
  if (!aview->initialised && amodel->objects.size())
    viewset = RESET_ZOOM;

  //Viewport reset flagged
  if (viewset > 0)
  {
    //Update the viewports
    resetViews(viewset == RESET_ZOOM);
  }

  //Turn filtering of objects on/off
  if (amodel->views.size() > 1 || models.size() > 1)
  {
    for (unsigned int v=0; v<amodel->views.size(); v++)
      //Enable filtering only for views with an object list
      if (amodel->views[v]->objects.size() > 0) 
        amodel->views[v]->filtered = true;
  }
  else //Single viewport, always disable filter
    aview->filtered = false;

  if (session.omegalib)
  {
    drawSceneBlended();
  }
  else
  {
    if (aview->stereo)
    {
      viewApply(view);
      GL_Error_Check;

      bool sideBySide = false;
      if (viewer->stereoBuffer)
      {
        // Draw to the left buffer
        glDrawBuffer(GL_LEFT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      }
      else
      {
        /*glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Apply red filter for left eye
        if (aview->background.value < aview->inverse.value)
           glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
        else  //Use opposite mask for light backgrounds
           glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
           */
        sideBySide = true;
      }

      // Render the left-eye view
      if (sideBySide) aview->port(0, 0, viewer->width*0.5, viewer->height*0.5);
      aview->projection(EYE_LEFT);
      if (sideBySide) aview->port(0, 0, viewer->width*0.5, viewer->height);
      aview->apply();
      // Draw scene
      drawSceneBlended();

      if (viewer->stereoBuffer)
      {
        // Draw to the right buffer
        glDrawBuffer(GL_RIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      }
      else
      {
        /*/ Clear the depth buffer so red/cyan components are blended
        glClear(GL_DEPTH_BUFFER_BIT);
        // Apply cyan filter for right eye
        if (aview->background.value < aview->inverse.value)
           glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
        else  //Use opposite mask for light backgrounds
           glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
        */
      }

      // Render the right-eye view
      if (sideBySide) aview->port(viewer->width*0.5, 0, viewer->width*0.5, viewer->height*0.5);
      aview->projection(EYE_RIGHT);
      if (sideBySide) aview->port(viewer->width*0.5, 0, viewer->width*0.5, viewer->height);
      aview->apply();
      // Draw scene (no sort required this time)
      drawSceneBlended(true);

      // Restore full-colour
      //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    else
    {
      //Loop through all viewports and display each
      int selview = view;
      for (unsigned int v=0; v<amodel->views.size(); v++)
      {
        viewApply(v);
        GL_Error_Check;

        // Default non-stereo render
        aview->projection(EYE_CENTRE);
        drawSceneBlended(v > 0);
      }

      if (view != selview)
      viewSelect(selview);
    }
  }

  //Calculate FPS
  if (session.global("fps"))
  {
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<float> diff = now-frametime;
    framecount++;
    if (diff.count() > 1.0f)
    {
      fps = framecount / (float)diff.count();
      framecount = 0;
      frametime = now;
    }
    if (!session.omegalib)
    {
      std::stringstream ss;
      ss << "FPS: " << std::setprecision(3) << fps;
      displayText(ss.str(), 1);
    }
  }

  if ((viewer->visible && !viewer->imagemode) || message[0] == ':')
  {
    //Print current info message (displayed for one frame only)
    if (status) displayMessage();

    //Print help message (displayed for one frame only)
    if (help.length())
      displayText(help);
  }

  //Display object list if enabled
  if (objectlist && help.length() == 0)
    displayObjectList(false);

  double time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
  if (time > 0.1)
    debug_print("%.4lf seconds to render scene\n", time);
}

void LavaVu::drawAxis()
{
  bool doaxis = aview->properties["axis"];
  if (!aview->is3d) doaxis &= aview->properties.has("axis");
  if (!doaxis) return;
  infostream = NULL;

  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_LIGHTING);
  //Clear depth buffer
  glClear(GL_DEPTH_BUFFER_BIT);

  //Setup the projection
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  // Build the viewing frustum - fixed near/far
  float nearc = 0.01, farc = 10.0, right, top;
  top = tan(0.5f * DEG2RAD * 45) * nearc;
  right = top;
  glFrustum(-right, right, -top, top, nearc, farc);

  //Square viewport in lower left corner
  float size = aview->properties["axislength"];
  if (aview->properties.has("axisbox"))
  {
    float vp[4];
    Properties::toArray<float>(aview->properties["axisbox"], vp, 4);

    if (vp[2] == 0.0) vp[2] = size;
    if (vp[3] == 0.0) vp[3] = size;

    vp[0] = aview->width*vp[0];
    vp[1] = aview->height*vp[1];

    vp[2] = (aview->width+aview->height)*2.0*vp[2];
    vp[3] = (aview->width+aview->height)*2.0*vp[3];

    //printf("%f %f %f %f\n", vp[0], vp[1], vp[2], vp[3]);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
  }
  else
  {
    size = 10 + (aview->width+aview->height)*2.0*size;
    glViewport(aview->xpos, aview->ypos, size, size);
    //printf("%d %d %f %f\n", aview->xpos, aview->ypos, size, size);
  }

  //Modelview (rotation only)
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  //Offset from centre for an angled view in 3d
  if (aview->is3d)
    glTranslatef(-0.225, -0.225, -1.0);
  else
    glTranslatef(-0.35, -0.35, -1.0);
  //Apply model rotation
  aview->applyRotation();
  GL_Error_Check;
  // Switch coordinate system if applicable
  glScalef(1.0, 1.0, 1.0 * (int)aview->properties["coordsystem"]);

  //Use a fixed size to fill the viewport,
  //actual size determined by viewport dimensions
  float length = 0.175;
  if (!session.axisobj)
  {
    //Ensure loaded without set timestep
    session.axisobj = new DrawingObject(session);
    session.axisobj->fixed = true; //All data is fixed
    if (!aview->hasObject(session.axisobj)) aview->addObject(session.axisobj);
    axis->add(session.axisobj);
    axis->setup(aview);
    axis->clear(true);
    axis->type = lucVectorType;
    session.axisobj->properties.data = {
      {"wireframe",    false},
      {"clip",         false},
      {"opacity",      1.0},
      {"alpha",        1.0}
    };

    float headsize = 8.0;  //8 x radius (r = 0.01 * length)
    float radius = length*0.01;
    int dims = aview->is3d ? 3 : 2;
    for (int c=0; c<dims; c++)
    {
      float vector[3] = {0, 0, 0};
      float pos[3] = {0, 0, 0};
      Colour colour = {0, 0, 0, 255};
      vector[c] = 1.0;
      pos[c] = length/2;
      colour.rgba[c] = 255;
      Geom_Ptr tg = axis->read(session.axisobj, 0, lucVertexData, NULL);
      axis->drawVector(session.axisobj, pos, vector, false, length, radius, radius, headsize, 16);
      tg->_colours->read1(colour.value);
    }
    axis->update();
  }

  axis->display(true); //Display with forced data update

  //Labels
  glUseProgram(0);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  session.fonts.charset = FONT_VECTOR;
  session.fonts.fontscale = length*6.0;
  glColor3ubv(aview->textColour.rgba);
  float pos = length/2;
  float LH = length * 0.1;
  session.fonts.print3dBillboard(pos, -LH, 0, "X");
  session.fonts.print3dBillboard(-LH, pos, 0, "Y");
  if (aview->is3d)
    session.fonts.print3dBillboard(-LH, -LH, pos, "Z");

  glPopAttrib();

  //Restore
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  GL_Error_Check;

  //Restore viewport
  aview->port(viewer->width, viewer->height);

  //Restore info/error stream
  if (verbose) infostream = stderr;
}

void LavaVu::drawRulers()
{
  if (!aview->properties["rulers"]) return;
  infostream = NULL;
  DrawingObject* obj = session.rulerobj;
  rulers->clear(true);
  rulers->setup(aview);
  if (!obj) obj = new DrawingObject(session, "", "wireframe=false\nclip=false\nlit=false\nopacity=1.0\nalpha=1.0\n");
  if (!aview->hasObject(obj)) aview->addObject(obj);
  rulers->add(obj);
  obj->properties.data["linewidth"] = (float)aview->properties["rulerwidth"];
  float font_scale_factor = 0.25*aview->model_size;
  if (!aview->is3d) font_scale_factor *= 2;
  obj->properties.data["fontscale"] = (float)aview->properties["fontscale"] * font_scale_factor;
  obj->properties.data["font"] = "vector";
  //Colour for labels
  obj->properties.data["colour"] = aview->textColour.toJson();

  int ticks = aview->properties["rulerticks"];
  json labels = aview->properties["rulerlabels"];
  std::string axes = aview->properties["ruleraxes"];
  std::string fmt = aview->properties["rulerformat"];
  if (labels.is_array() && labels.size() > 0) ticks = 0; //Ignore tick settings, use label counts
  
  //Axis rulers
  float shift[3] = {0.01f/aview->scale[0] * aview->model_size,
                    0.01f/aview->scale[1] * aview->model_size,
                    0.01f/aview->scale[2] * aview->model_size
                   };

  if (axes.find_first_of('x') != std::string::npos)
  {
    float sta[3] = {aview->min[0], aview->min[1]-shift[1], aview->max[2]};
    float end[3] = {aview->max[0], aview->min[1]-shift[1], aview->max[2]};
    json l = labels.is_array() && labels.size() > 0 && labels[0].size() ? labels[0] : json::array();
    drawRuler(obj, sta, end, aview->min[0], aview->max[0], fmt.c_str(), ticks, l, 0);
  }
  if (axes.find_first_of('y') != std::string::npos)
  {
    float sta[3] = {aview->min[0]-shift[0], aview->min[1], aview->max[2]};
    float end[3] = {aview->min[0]-shift[0], aview->max[1], aview->max[2]};
    json l = labels.is_array() && labels.size() > 1 && labels[1].size() ? labels[1] : json::array();
    drawRuler(obj, sta, end, aview->min[1], aview->max[1], fmt.c_str(), ticks, l, 1);
  }
  if (axes.find_first_of('z') != std::string::npos)
  {
    float sta[3] = {aview->min[0]-shift[0], aview->min[1]-shift[1], aview->min[2]};
    float end[3] = {aview->min[0]-shift[0], aview->min[1]-shift[1], aview->max[2]};
    json l = labels.is_array() && labels.size() > 2 && labels[2].size() ? labels[2] : json::array();
    drawRuler(obj, sta, end, aview->min[2], aview->max[2], fmt.c_str(), ticks, l, 2);
  }
  if (axes.find_first_of('X') != std::string::npos)
  {
    float sta[3] = {aview->min[0], aview->max[1]+shift[1], aview->max[2]};
    float end[3] = {aview->max[0], aview->max[1]+shift[1], aview->max[2]};
    json l = labels.is_array() && labels.size() > 0 && labels[0].size() ? labels[0] : json::array();
    drawRuler(obj, sta, end, aview->min[0], aview->max[0], fmt.c_str(), ticks, l, 0, -1);
  }
  if (axes.find_first_of('Y') != std::string::npos)
  {
    float sta[3] = {aview->max[0]+shift[0], aview->min[1], aview->max[2]};
    float end[3] = {aview->max[0]+shift[0], aview->max[1], aview->max[2]};
    json l = labels.is_array() && labels.size() > 1 && labels[1].size() ? labels[1] : json::array();
    drawRuler(obj, sta, end, aview->min[1], aview->max[1], fmt.c_str(), ticks, l, 1, -1);
  }
  if (axes.find_first_of('Z') != std::string::npos)
  {
    float sta[3] = {aview->max[0]+shift[0], aview->min[1]-shift[1], aview->min[2]};
    float end[3] = {aview->max[0]+shift[0], aview->min[1]-shift[1], aview->max[2]};
    json l = labels.is_array() && labels.size() > 2 && labels[2].size() ? labels[2] : json::array();
    drawRuler(obj, sta, end, aview->min[2], aview->max[2], fmt.c_str(), ticks, l, 2, -1);
  }

  rulers->display(true); //Display with forced data update

  //Restore info/error stream
  if (verbose) infostream = stderr;
}

void LavaVu::drawRuler(DrawingObject* obj, float start[3], float end[3], float labelmin, float labelmax, const char* fmt, int ticks, json& labels, int axis, int tickdir)
{
  // Draw rulers with optional tick marks
  float vec[3];
  float length;
  vectorSubtract(vec, end, start);

  // Length of the drawn vector = vector magnitude
  length = sqrt(dotProduct(vec,vec));
  if (length <= FLT_MIN) return;

  //Draw ruler line
  if (!ticks && !labels.size()) return; //Skip this line
  float pos[3] = {start[0] + vec[0] * 0.5f, start[1] + vec[1] * 0.5f, start[2] + vec[2] * 0.5f};
  rulers->drawVector(obj, pos, vec, false, 1.0, 0, 0, 0, 0);
  Geom_Ptr lg = rulers->add(obj); //Add new object for ticks

  std::string align = "";
  if (labels.size()) ticks = labels.size(); //Use custom labels
  for (int i = 0; i < ticks; i++)
  {
    // Get tick value
    float scaledPos = i / (float)(ticks-1);
    // Calculate pixel position
    float height = -0.01 * aview->model_size;

    //Labels have custom positions?
    float pos = start[axis] + vec[axis] * scaledPos;
    if (labels.size() && labels[i].is_number())
      pos = (float)labels[i];

    // Draws the tick
    if (axis == 0)
    {
      height /= aview->scale[1];
      float tvec[3] = {0, tickdir*height, 0};
      float tpos[3] = {pos, start[1] + height*tickdir * 0.5f, start[2]};
      rulers->drawVector(obj, tpos, tvec, false, 1.0, 0, 0, 0, 0);
      align = tickdir > 0 ? "|" : "|^"; //Centre (reverse vertical shift if flipped)
    }
    else if (axis == 1)
    {
      height /= aview->scale[0];
      float tvec[3] = {tickdir*height, 0, 0};
      float tpos[3] = {start[0] + height*tickdir * 0.5f, pos, start[2]};
      rulers->drawVector(obj, tpos, tvec, false, 1.0, 0, 0, 0, 0);
      align = tickdir > 0 ? "!_" : "_"; //Right/Left align no vertical shift
    }
    else if (axis == 2)
    {
      height /= aview->scale[1];
      float tvec[3] = {0, height, 0};
      float tpos[3] = {start[0], start[1] + height*tickdir * 0.5f, pos};
      rulers->drawVector(obj, tpos, tvec, false, 1.0, 0, 0, 0, 0);
      align = tickdir > 0 ? "!_" : "_"; //Right/Left align no vertical shift
    }

    //Draw a label
    std::string labelstr;
    if (labels.size() && labels[i].is_string())
    {
      labelstr = labels[i];
    }
    else
    {
      char label[16];
      sprintf(label, fmt, pos);
      // Trim trailing space
      char* end = label + strlen(label) - 1;
      while(end > label && isspace(*end)) end--;
      *(end+1) = 0; //Null terminator
      labelstr = label;
    }

    std::string blank = "";
    lg->label(blank);
    labelstr = align + "  " + labelstr + "  ";
    lg->label(labelstr);
  }
}

void LavaVu::drawBorder()
{
  //Check for boolean or numeric value for border
  float bordersize = 1.0;
  json& bord = aview->properties["border"];
  if (bord.is_number())
    bordersize = bord;
  else if (!bord)
    bordersize = 0.0;
  if (bordersize <= 0.0) return;

  DrawingObject* obj = session.borderobj;
  border->clear(true);
  border->setup(aview);
  if (!obj) obj = session.borderobj = new DrawingObject(session, "", "clip=false\nopacity=1.0\nalpha=1.0\n");
  if (!aview->hasObject(obj)) aview->addObject(obj);
  obj->properties.data["colour"] = aview->properties["bordercolour"];
  obj->properties.data["depthtest"] = aview->is3d;

  infostream = NULL; //Disable debug output while drawing this
  bool filled = aview->properties["fillborder"];

  if (!filled)
  {
    obj->properties.data["lit"] = false;
    obj->properties.data["wireframe"] = true;
    obj->properties.data["cullface"] = false;
    obj->properties.data["linewidth"] = bordersize - 0.5;
  }
  else
  {
    obj->properties.data["lit"] = true;
    obj->properties.data["wireframe"] = false;
    obj->properties.data["cullface"] = true;
  }

  Vec3d minvert = Vec3d(aview->min);
  Vec3d maxvert = Vec3d(aview->max);
  if (aview->is3d)
  {
    // Draw model bounding box with optional filled background surface
    Quaternion qrot;
    //Min/max swapped to draw inverted box, see through to back walls
    border->drawCuboid(obj, maxvert, minvert, qrot, false);
  }
  else
  {
    float z = aview->min[2];
    if (std::isinf(z) || std::isnan(z)) z = 0.0f;
    minvert.z = maxvert.z = z;
    Vec3d vert1 = Vec3d(aview->max[0], aview->min[1], z);
    Vec3d vert2 = Vec3d(aview->min[0], aview->max[1], z);
    border->read(obj, 1, lucVertexData, minvert.ref());
    border->read(obj, 1, lucVertexData, vert1.ref());
    border->read(obj, 1, lucVertexData, maxvert.ref());
    border->read(obj, 1, lucVertexData, vert2.ref());
  }

  border->display(true); //Display with forced data update

  //Restore info/error stream
  if (verbose) infostream = stderr;
}

void LavaVu::displayObjectList(bool console)
{
  //Print available objects by id to screen and stderr
  int offset = 1;
  if (console) std::cerr << "------------------------------------------" << std::endl;
  for (unsigned int i=0; i < amodel->objects.size(); i++)
  {
    std::ostringstream ss;
    ss << "  ";
    ss << std::setw(5) << (i+1) << " : " << amodel->objects[i]->name();
    if (amodel->objects[i] == aobject) ss << "*";
    if (amodel->objects[i]->skip)
    {
      if (console) std::cerr << "[ no data  ]" << ss.str() << std::endl;
      Colour c;
      c.value = 0xff222288;
      displayText(ss.str(), ++offset, &c);
    }
    else if (amodel->objects[i]->properties["visible"])
    {
      if (console) std::cerr << "[          ]" << ss.str() << std::endl;
      //Use cached object colour, unless matches background
      Colour c = amodel->objects[i]->colour;
      c.a = 255;
      offset++;
      displayText(ss.str(), offset);
      if (c.value != aview->background.value)
        displayText(std::string(1, 0x7f), offset, &c);
    }
    else
    {
      if (console) std::cerr << "[  hidden  ]" << ss.str() << std::endl;
      Colour c;
      c.value = 0xff888888;
      displayText(ss.str(), ++offset, &c);
    }
  }
  if (console) std::cerr << "------------------------------------------" << std::endl;
}

void LavaVu::printMessage(const char *fmt, ...)
{
  if (fmt)
  {
    va_list ap;                 // Pointer to arguments list
    va_start(ap, fmt);          // Parse format string for variables
    vsnprintf(message, MAX_MSG, fmt, ap);    // Convert symbols
    va_end(ap);
  }
}

void LavaVu::printall(const std::string& str)
{
  //Print string to console and frame, callable from python
  std::cout << str << std::endl << std::flush;
  //Multiline?
  std::size_t pos = str.find("\n");
  if (pos == std::string::npos)
    printMessage(str.c_str());
  else
    help = str;
}

void LavaVu::text(const std::string& str, int xpos, int ypos, float scale, Colour* colour)
{
  if (!viewer->isopen) return;
  //Black on white or reverse depending on background
  Colour scol = aview->textColour;
  scol.invert();

  //Shadow
  glColor3ubv(scol.rgba);
  session.fonts.charset = FONT_VECTOR;
  session.fonts.fontscale = scale;

  session.fonts.print(xpos+1, ypos-1, str.c_str());

  //Use provided text colour or calculated
  if (colour)
    glColor3ubv(colour->rgba);
  else
    glColor3ubv(aview->textColour.rgba);

  session.fonts.print(xpos, ypos, str.c_str());

  //Revert to normal colour
  glColor3ubv(aview->textColour.rgba);
}

void LavaVu::displayMessage()
{
  if (!viewer->isopen) return;
  //Skip if no message or recording frames to video
  if (strlen(message) && !encoder)
  {
    //Set viewport to entire window
    aview->port(0, 0, viewer->width, viewer->height);
    Viewport2d(viewer->width, viewer->height);

    //Print current message
    text(message, 10, 10, 1.0);

    Viewport2d(0, 0);
  }
}

void LavaVu::displayText(const std::string& str, int lineno, Colour* colour)
{
  if (!viewer->isopen) return;
  //Set viewport to entire window
  aview->port(0, 0, viewer->width, viewer->height);
  Viewport2d(viewer->width, viewer->height);

  float size = viewer->height / 1250.0;
  if (size < 0.5) size = 0.5;
  float lineht = 27 * size;

  std::stringstream ss(str);
  std::string line;
  while(std::getline(ss, line))
  {
    text(line, 5, viewer->height - lineht*lineno, size, colour);
    lineno++;
  }

  Viewport2d(0, 0);
}

void LavaVu::drawSceneBlended(bool nosort)
{
  //Sort required? (only on first call per frame, by nosort flag)
  if (!nosort && session.global("sort") && aview && aview->rotated)
  {
    //Immediate sort (when automating and no visible viewer window)
    if (session.automate && !viewer->visible)
    {
      aview->rotated = false;
      sort(true);
    }
    //Async sort (interactive mode)
    else
      queueCommands("asyncsort");
  }

  switch (viewer->blend_mode)
  {
  case BLEND_NORMAL:
    // Blending setup for interactive display...
    // Normal alpha blending for rgb colour, accumulate opacity in alpha channel with additive blending
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA);
    //Render!
    drawScene();
    break;
  case BLEND_PNG:
    // Blending setup for write to transparent PNG...
    // This works well but with some blend errors that show if only rendering a few particle layers
    // Rendering colour only first pass at fully transparent, then second pass renders as usual
    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ZERO);
    drawScene();
    // Clear the depth buffer so second pass is blended or nothing will be drawn
    glClear(GL_DEPTH_BUFFER_BIT);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawScene();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    break;
  case BLEND_ADD:
    // Additive blending
    //glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //Render!
    drawScene();
    break;
  }

  if (!session.omegalib)
  {
    drawAxis();
    aview->drawOverlay();
  }
}

void LavaVu::drawScene()
{
  if (session.global("antialias"))
    glEnable(GL_MULTISAMPLE);
  else
    glDisable(GL_MULTISAMPLE);

  // Setup default state
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);
  glShadeModel(GL_SMOOTH);
  glPushAttrib(GL_ENABLE_BIT);

  //Run the renderers
  for (auto g : amodel->geometry)
    g->display();

  if (!session.omegalib)
    drawBorder();
  drawRulers();

  //Restore default state
  glPopAttrib();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram(0);
}

bool LavaVu::loadFile(const std::string& file)
{
  //All files on command line plus init.script added to files list
  // - gldb files represent a Model
  // - Non gldb data will be loaded into active Model
  // - If none exists, a default will be created
  // - gldb data will be loaded into active model iff it is not a gldb model
  FilePath fn(file);

  //Load a file based on extension
  debug_print("Loading: %s\n", fn.full.c_str());

  //Database files always create their own Model object
  if (fn.type == "gldb" || fn.type == "db" || fn.full.find("file:") != std::string::npos)
  {
    //Open database file, if a non-db model already loaded, load into that (unless disabled with "merge" property)
    if (models.size() == 0 || (amodel && amodel->database && !session.global("merge")))
    {
      amodel = new Model(session);
      models.push_back(amodel);
    }

    //Load objects from db
    amodel->load(fn);

    //Ensure default view selected
    aview = amodel->defaultView();
    view = 0;

    //Load initial figure
    if (initfigure != 0) amodel->loadFigure(initfigure-1);

    //Save path of first sucessfully loaded model
    if (dbpath && viewer->output_path.length() == 0 && fn.path.length() > 0)
    {
      viewer->output_path = fn.path + '/';
      debug_print("Output path set to %s\n", viewer->output_path.c_str());
    }

    if (viewer->isopen)
    {
      //If the viewer is open, view the loaded model at initial timestep
      loadModelStep(models.size()-1, startstep, true);
      //If the viewer is open, view the loaded model at current timestep
      //loadModelStep(models.size()-1, -1, true);
      viewer->postdisplay = true;
    }

    return true;
  }

  //Following must exist (db can be memory)
  if (!FileExists(fn.full)) return false;

  //Script files, can contain other files to load
  if (fn.type == "script")
  {
    parseCommands("script " + fn.full);
    return true;
  }

  //JSON state file (doesn't load objects if any, only state)
  if (fn.type == "json")
  {
    if (!amodel) 
      //Defer until window open
      viewer->commands.push_back("file \"" + fn.full + "\"");
    else
      jsonReadFile(fn.full);
    return true;
  }

  //Other files require an existing model
  if (!amodel) defaultModel();

  //setting prop "filestep=true" allows automatically adding timesteps before each file loaded
  if (session.global("filestep")) parseCommands("newstep");

  //Load other data by type
  if (fn.type == "dem")
    readHeightMap(fn);
  else if (fn.type == "obj")
    readOBJ(fn);
  else if (fn.type == "raw")
    readRawVolume(fn);
  else if (fn.type == "xrw" || fn.type == "xrwu")
    readXrwVolume(fn);
  else if (fn.type == "jpg" || fn.type == "jpeg" || fn.type == "png" || fn.type == "tif" || fn.type == "tiff")
  {
    if (fn.base.find("heightmap") != std::string::npos || fn.base.find("_dem") != std::string::npos)
      readHeightMapImage(fn);
    else if (fn.type == "tiff" || fn.type == "tif")
      readVolumeTIFF(fn);
    else
      readVolumeSlice(fn);
  }
  else
    //Unknown type
    return false;

  //Reselect the active view after loading any model data
  viewset = RESET_ZOOM; //View will be reset and autozoomed on next display call

  return true;
}

void LavaVu::defaultModel()
{
  //Adds a default model, window & viewport

  //Use current view properties
  Properties* props = NULL;
  if (amodel && aview) props = &aview->properties;

  amodel = new Model(session);
  models.push_back(amodel);

  //Set a default view
  aview = amodel->defaultView(props);

  //Setup default colourmaps
  //amodel->initColourMaps();
}

//Load model data at specified timestep
bool LavaVu::loadModelStep(int model_idx, int at_timestep, bool autozoom)
{
  if (viewer->render_thread != std::this_thread::get_id())
    abort_program("FATAL: must call model load from render thread");
  if (models.size() == 0) defaultModel();
  if (model_idx == model && at_timestep >= 0 && at_timestep == session.now) return false; //No change
  if (model_idx < 0 || model_idx >= (int)models.size()) return false;

  //Save active model as selected
  amodel = models[model_idx];
  model = model_idx;

  //Save active colourmaps list on session
  session.colourMaps = &amodel->colourMaps;

  //Have a database model loaded already?
  if (amodel->objects.size() > 0)
  {
    //Redraw model data
    amodel->redraw();

    //Set timestep and load geometry at that step
    if (amodel->database)
    {
      if (at_timestep < 0)
        amodel->setTimeStep(session.now);
      else
        amodel->setTimeStep(amodel->nearestTimeStep(at_timestep));
      if (verbose) std::cerr << "Loading vis '" << session.global("caption") << "', timestep: " << amodel->step() << std::endl;
    }
  }

  if (!aview) aview = amodel->views[0];

  json res = session.global("resolution");

  //Not yet opened or resized?
  if (!viewer->isopen)
    //Open window at required size
    viewer->open(res[0], res[1]);
  else
    //Resize if necessary
    viewer->setsize(res[0], res[1]);

  //Flag a view update
  viewset = autozoom ? RESET_ZOOM : RESET_YES;

  return true;
}

std::string LavaVu::video(std::string filename, int fps, int width, int height, int start, int end, int quality)
{
  if (width > 0) viewer->outwidth = width;
  if (height > 0) viewer->outheight = height;
  if (end <= 0) end = amodel->lastStep();
  debug_print("VIDEO: w %d h %d fps %d, %d --> %d\n", width, height, fps, start, end);
  filename = encodeVideo(filename, fps, quality);
  writeSteps(false, start, end);
  encodeVideo(); //Write final step and stop encoding
  return filename;
}

std::string LavaVu::encodeVideo(std::string filename, int fps, int quality)
{
  if (!encoder)
  {
    if (filename.length() == 0) 
      filename = session.counterFilename() + ".mp4";
    FilePath fp(filename);
    if (fp.ext.length() == 0) 
      filename += ".mp4"; //Default to mp4

    //Enable output just to get the dimensions
    if (!viewer->outwidth) viewer->outwidth = viewer->width;
    if (!viewer->outheight) viewer->outheight = viewer->height;
    //printf("OUTPUT DIMS %d x %d\n", viewer->outwidth, viewer->outheight);
    viewer->outputON(viewer->outwidth, viewer->outheight, 3, true);
    encoder = new VideoEncoder(filename.c_str(), fps, quality);
    encoder->open(viewer->getOutWidth(), viewer->getOutHeight());
    viewer->addOutput(encoder);
    viewer->outputOFF();
    return filename;
  }
  else
  {
    //Delete the encoder, completes the video
    delete encoder;
    encoder = NULL;
    viewer->outputOFF();
    viewer->removeOutput(); //Deletes the output attachment
  }
  return "";
}

void LavaVu::writeSteps(bool images, int start, int end)
{
  //Default to last available step
  if (end < 0)
  {
    end = amodel->lastStep();
    if (end < 0) end = 0;
  }
  //Swap if out of order
  if (start > end)
  {
    int temp = start;
    start = end;
    end = temp;
  }

  for (int i=start; i<=end; i++)
  {
    //Only load steps that contain geometry data
    if (amodel->hasTimeStep(i) || amodel->timesteps.size() == 0)
    {
      amodel->setTimeStep(amodel->nearestTimeStep(i));
      std::cout << "... Writing timestep: " << amodel->step() << std::endl;
      //Update the views
      resetViews(true);

      if (images)
      {
        std::string title = session.global("caption");
        std::ostringstream filess;
        filess << title << '-' << std::setw(5) << std::setfill('0') << amodel->step();
        viewer->image(viewer->output_path + filess.str());
      }

      //Always output to video encode if it exists
      if (encoder)
        viewer->display();
    }
  }
}

std::string LavaVu::jsonWriteFile(DrawingObject* obj, bool jsonp, bool objdata)
{
  //Write new JSON format objects
  char filename[FILE_PATH_MAX];
  char ext[6];
  std::string name = session.global("caption");
  strcpy(ext, "jsonp");
  if (!jsonp) ext[4] = '\0';
  if (obj)
    sprintf(filename, "%s_%s_%05d.%s", name.c_str(),
            obj->name().c_str(), amodel->stepInfo(), ext);
  else
    sprintf(filename, "%s_%05d.%s", name.c_str(), amodel->stepInfo(), ext);
  jsonWriteFile(filename, obj, jsonp, objdata);
  return std::string(filename);
}

void LavaVu::jsonWriteFile(std::string fn, DrawingObject* obj, bool jsonp, bool objdata)
{
  //Write new JSON format objects
  if (fn.length() == 0) fn = "state.json";
  std::ofstream json(fn);
  if (jsonp) json << "loadData(\n";
  amodel->jsonWrite(json, obj, objdata);
  if (jsonp) json << ");\n";
  json.close();
}

void LavaVu::jsonReadFile(std::string fn)
{
  if (fn.length() == 0) fn = "state.json";
  std::ifstream file(fn.c_str(), std::ios::in);
  if (file.is_open())
  {
    printMessage("Loading state: %s", fn.c_str());
    std::stringstream buffer;
    buffer << file.rdbuf();
    int r = amodel->jsonRead(buffer.str());
    file.close();
    applyReload(NULL, r);
  }
  else
    printMessage("Unable to open file: %s", fn.c_str());
}

//Data request from attached apps
std::string LavaVu::requestData(std::string key)
{
  std::ostringstream result;
  if (key == "objects")
  {
    amodel->jsonWrite(result);
    debug_print("Sending object list (%d bytes)\n", result.str().length());
  }
  else if (key == "history")
  {
    for (unsigned int l=0; l<history.size(); l++)
      result << history[l] << std::endl;
    debug_print("Sending history (%d bytes)\n", result.str().length());
  }
  //std::cerr << result.str();
  return result.str();
}

//Python interface functions
std::string LavaVu::gl_version()
{
  if (!viewer->isopen) return "(not initialised)";
  Shader::version();
  return Shader::gl_version;
}

std::string LavaVu::image(std::string filename, int width, int height, int jpegquality, bool transparent)
{
  if (!amodel || !viewer->isopen) return "";
  //Set width/height override
  //Pass asterisk to generate filename automatically
  if (filename == "*") filename = session.counterFilename();
  //Write image to file if filename provided or return as string (base64 data url)
  return viewer->image(filename, jpegquality, transparent, width, height);
}

std::string LavaVu::web(bool tofile)
{
  if (!amodel) return "";
  display(); //Forces view/window open
  Triangles* tris = (Triangles*)amodel->getRenderer(lucTriangleType);
  if (tris) tris->loadMesh();  //Optimise triangle meshes before export
  if (!tofile)
    return amodel->jsonWrite(true);
  return jsonWriteFile(NULL, false, true);
}

ColourMap* LavaVu::addColourMap(std::string name, std::string colours, std::string properties)
{
  if (!amodel) return NULL;
  return amodel->addColourMap(name, colours, properties);
}

void LavaVu::updateColourMap(ColourMap* colourMap, std::string colours, std::string properties)
{
  if (!amodel || !colourMap) return;
  amodel->updateColourMap(colourMap, colours, properties);
}

ColourMap* LavaVu::getColourMap(unsigned int id)
{
  if (!amodel || amodel->colourMaps.size() <= id) return NULL;
  return amodel->colourMaps[id];
}

ColourMap* LavaVu::getColourMap(std::string name)
{
  if (!amodel) return NULL;
  for (unsigned int i=0; i < amodel->colourMaps.size(); i++)
    if (name ==  amodel->colourMaps[i]->name)
      return amodel->colourMaps[i];
  return NULL;
}

void LavaVu::setColourMap(ColourMap* target, std::string properties)
{
  if (!amodel || !target) return;
  //Parse and merge property strings
  session.parseSet(target->properties, properties);

  //All objects using this colourmap require redraw
  for (unsigned int i=0; i < amodel->objects.size(); i++)
  {
    ColourMap* cmap = amodel->objects[i]->getColourMap("colourmap");
    ColourMap* omap = amodel->objects[i]->getColourMap("opacitymap");
    if (cmap == target || omap == target)
      amodel->redraw(amodel->objects[i]);
  }
}

DrawingObject* LavaVu::colourBar(DrawingObject* obj)
{
  //Add colour bar display to specified object
  std::string name = "colourbar";
  if (obj) name = obj->name() + "_colourbar";
  DrawingObject* cbar = addObject(new DrawingObject(session, name, "colourbar=1\n"));
  if (obj)
    cbar->properties.data["colourmap"] = obj->properties["colourmap"];
  return cbar;
}

void LavaVu::setState(std::string state)
{
  if (!amodel) return;
  int r = amodel->jsonRead(state);
  applyReload(NULL, r);
  viewer->postdisplay = true;
}

std::string LavaVu::getState()
{
  if (!amodel) return "{}";
  //Export current state
  return amodel->jsonWrite();
}

std::string LavaVu::getTimeSteps()
{
  if (!amodel) return "[]";
  json steps = json::array();
  for (unsigned int s=0; s<amodel->timesteps.size(); s++)
  {
    steps.push_back(amodel->timesteps[s]->step);
  }
  std::stringstream ss;
  ss << steps;
  return ss.str();
}

void LavaVu::addTimeStep(int step, std::string properties)
{
  if (step < 0)
    step = amodel->step()+1;
  amodel->addTimeStep(step, step, properties);
  amodel->setTimeStep(step);
}

void LavaVu::setObject(DrawingObject* target, std::string properties)
{
  if (!amodel || !target) return;
  //Parse and merge property strings
  session.parseSet(target->properties, properties);
  target->setup();
}

DrawingObject* LavaVu::createObject(std::string properties)
{
  if (!amodel) defaultModel();
  DrawingObject* obj = addObject(new DrawingObject(session));

  //Parse and merge property strings
  setObject(obj, properties);

  //Ensure has a name
  std::string name = obj->properties["name"];
  if (name.length() == 0)
  {
    //Avoid duplicate default names
    std::stringstream nss;
    nss << "default_" << amodel->objects.size();
    obj->properties.data["name"] = nss.str();
  }

  return obj;
}

DrawingObject* LavaVu::getObject(const std::string& name)
{
  if (!amodel) return NULL;
  if (name.length())
    return amodel->findObject(name, aobject);
  return NULL;
}

DrawingObject* LavaVu::getObject(int id)
{
  if (!amodel) return NULL;
  if (id > 0 && id <= (int)amodel->objects.size())
    return amodel->objects[id-1];
  return NULL;
}

void LavaVu::reloadObject(DrawingObject* target)
{
  //Reload data on specific object only
  if (!amodel || !target) return;
  amodel->reload(target);
}

void LavaVu::appendToObject(DrawingObject* target)
{
  //Append data container to specified object
  if (!amodel || !target) return;
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    container->add(target);
}

void LavaVu::loadTriangles(DrawingObject* target, std::vector< std::vector <float> > array, int split)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    for (unsigned int i=0; i < array.size(); i += 3)
      container->addTriangle(target, &array[i+0][0], &array[i+1][0], &array[i+2][0], split);
}

void LavaVu::loadColours(DrawingObject* target, std::vector <std::string> list)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
  {
    for (auto item : list)
    {
      //Use colour constructor to parse string colours
      Colour c(item);
      container->read(target, 1, lucRGBAData, &c);
    }
  }
}

void LavaVu::loadLabels(DrawingObject* target, std::vector <std::string> labels)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    container->label(target, labels);
}

void LavaVu::clearObject(DrawingObject* target)
{
  if (!amodel || !target) return;
  for (auto g : amodel->geometry)
    g->remove(target);
}

void LavaVu::clearValues(DrawingObject* target, std::string label)
{
  if (!amodel || !target) return;
  for (auto g : amodel->geometry)
    g->clearValues(target, label);
}

void LavaVu::clearData(DrawingObject* target, lucGeometryDataType type)
{
  if (!amodel || !target) return;
  for (auto g : amodel->geometry)
    g->clearData(target, type);
}

std::string LavaVu::getObjectDataLabels(DrawingObject* target)
{
  if (!amodel || !target) return "";
  json dict = amodel->objectDataSets(target);
  std::stringstream ss;
  ss << dict;
  return ss.str();
}

void LavaVu::arrayUChar(DrawingObject* target, unsigned char* array, int len, lucGeometryDataType type)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
  {
    if (type == lucRGBAData)
    {
      int len32 = len / 4;
      assert(len32*4 == len);
      container->read(target, len32, type, array);
    }
    else
      container->read(target, len, type, array);
  }
}

void LavaVu::arrayUInt(DrawingObject* target, unsigned int* array, int len, lucGeometryDataType type)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    container->read(target, len, type, array);
}

void LavaVu::arrayFloat(DrawingObject* target, float* array, int len, lucGeometryDataType type)
{
  Geometry* container = lookupObjectRenderer(target);
  int dsize = 3;
  if (type == lucTexCoordData) dsize = 2;
  if (container)
    container->read(target, len/dsize, type, array);
}

void LavaVu::arrayFloat(DrawingObject* target, float* array, int len, std::string label)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    container->read(target, len, array, label);
}

void LavaVu::textureUChar(DrawingObject* target, unsigned char* array, int len, unsigned int width, unsigned int height, unsigned int channels, bool flip, bool mipmaps, bool bgr)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    container->loadTexture(target, array, width, height, channels, flip, mipmaps, bgr);
}

void LavaVu::textureUInt(DrawingObject* target, unsigned int* array, int len, unsigned int width, unsigned int height, unsigned int channels, bool flip, bool mipmaps, bool bgr)
{
  Geometry* container = lookupObjectRenderer(target);
  if (container)
    container->loadTexture(target, (GLubyte*)array, width, height, channels, flip, mipmaps, bgr);
}

//GeomData interface, for loading/acessing geom store directly
std::vector<Geom_Ptr> LavaVu::getGeometry(DrawingObject* target)
{
  //Gets data from active (geom) - includes fixed data, set timestep first to get time varying
  std::vector<Geom_Ptr> list;
  if (!amodel || !target) return list;
  for (auto g : amodel->geometry)
  {
    std::vector<Geom_Ptr> geomlist = g->getAllObjects(target);
    list.insert(std::end(list), std::begin(geomlist), std::end(geomlist));
  }
  return list;
}

std::vector<Geom_Ptr> LavaVu::getGeometryAt(DrawingObject* target, int timestep)
{
  //Gets data from all entries (records) at specified step
  std::vector<Geom_Ptr> list;
  if (!amodel || !target) return list;
  for (auto g : amodel->geometry)
  {
    std::vector<Geom_Ptr> geomlist = g->getAllObjectsAt(target, timestep);
    list.insert(std::end(list), std::begin(geomlist), std::end(geomlist));
  }
  return list;
}

std::vector<float> LavaVu::getBoundingBox(DrawingObject* target, bool allsteps)
{
  //Gets data from all entries (records) at specified step
  std::vector<float> list;
  if (!amodel || !target) return list;

  float min[3], max[3];
  for (int i=0; i<3; i++)
    max[i] = -(min[i] = HUGE_VAL);

  for (auto g : amodel->geometry)
    g->objectBounds(target, min, max, allsteps);

  for (int i=0; i<3; i++)
    list.push_back(min[i]);
  for (int i=0; i<3; i++)
    list.push_back(max[i]);
  return list;
}

void LavaVu::geometryArrayUChar(Geom_Ptr geom, unsigned char* array, int len, lucGeometryDataType type)
{
  Geometry* container = lookupObjectRenderer(geom->draw);
  if (container && geom)
  {
    geom->dataContainer(type)->clear();
    container->read(geom, len, type, array);
  }
}

void LavaVu::geometryArrayUInt(Geom_Ptr geom, unsigned int* array, int len, lucGeometryDataType type)
{
  Geometry* container = lookupObjectRenderer(geom->draw);
  if (container && geom)
  {
    geom->dataContainer(type)->clear();
    container->read(geom, len, type, array);
  }
}

void LavaVu::geometryArrayFloat(Geom_Ptr geom, float* array, int len, lucGeometryDataType type)
{
  int dsize = 3;
  if (type == lucTexCoordData) dsize = 2;
  Geometry* container = lookupObjectRenderer(geom->draw);
  if (container && geom)
  {
    geom->dataContainer(type)->clear();
    container->read(geom, len/dsize, type, array);
  }
}

void LavaVu::geometryArrayFloat(Geom_Ptr geom, float* array, int len, std::string label)
{
  Geometry* container = lookupObjectRenderer(geom->draw);
  if (container && geom)
  {
    for (auto vals : geom->values)
      if (vals->label == label)
        vals->clear();
    container->read(geom, len, array, label);
    geom->draw->ranges[label] = Range();
    container->scanDataRange(geom->draw);
  }
}

void LavaVu::geometryArrayViewFloat(Geom_Ptr geom, lucGeometryDataType dtype, float** array, int* len)
{
  //Get a view of internal geom array
  //(warning, can be released at any time, copy if needed!)
  if (!geom) return;
  Data_Ptr dat = geom->dataContainer(dtype);
  if (dat == nullptr)
  {
    *len = 0;
    return;
  }
  *array = (float*)dat->ref(0);
  *len = dat->size();
}

void LavaVu::geometryArrayViewFloat(Geom_Ptr geom, float** array, int* len, std::string label)
{
  //Get a view of internal geom array
  //(warning, can be released at any time, copy if needed!)
  if (!geom) return;
  Values_Ptr dat = geom->valueContainer(label);
  if (dat == nullptr)
  {
    *len = 0;
    return;
  }
  *array = (float*)dat->ref(0);
  *len = dat->size();
}

void LavaVu::geometryArrayViewUInt(Geom_Ptr geom, lucGeometryDataType dtype, unsigned int** array, int* len)
{
  //Get a view of internal geom array
  //(warning, can be released at any time, copy if needed!)
  if (!geom) return;
  Data_Ptr dat = geom->dataContainer(dtype);
  *array = (unsigned int*)dat->ref(0);
  *len = dat->size();
}

void LavaVu::geometryArrayViewUChar(Geom_Ptr geom, lucGeometryDataType dtype, unsigned char** array, int* len)
{
  //Get a view of internal geom array
  //(warning, can be released at any time, copy if needed!)
  if (!geom) return;
  Data_Ptr dat = geom->dataContainer(dtype);
  *array = (unsigned char*)dat->ref(0);
  *len = dat->size();
}

std::string rawImageWrite(unsigned char* array, int height, int width, int depth, std::string path, int jpegquality)
{
  // Read the pixels into provided buffer
  ImageData buffer(width, height, depth);
  buffer.copy(array);
  buffer.flip(); //Writer expects OpenGL order, so flip
  //Write PNG/JPEG to string or file
  if (path.length() == 0)
    return buffer.getURIString(jpegquality);
  else
    return buffer.write(path);
}

void LavaVu::imageBuffer(unsigned char* array, int height, int width, int depth)
{
  if (!amodel || !viewer->isopen) return;
  // Read the pixels into provided buffer
  ImageData* buffer = viewer->pixels(NULL, width, height, depth);
  buffer->flip(); //Flip Y axis so origin at top
  buffer->paste(array);
  delete buffer;
}

std::vector<unsigned char> LavaVu::imageJPEG(int width, int height, int quality)
{
  if (!amodel || !viewer->isopen)
    return std::vector<unsigned char>();

  ImageData* image = viewer->pixels(NULL, width, height, 3);
  //Write JPEG to string
  std::string retImg = image->getString(quality);
  delete image;
  std::vector<unsigned char> d(retImg.length());
  std::copy(retImg.begin(), retImg.end(), d.data());
  return d;
}

std::vector<unsigned char> LavaVu::imagePNG(int width, int height, int depth)
{
  if (!amodel || !viewer->isopen) return std::vector<unsigned char>();

  ImageData* image = viewer->pixels(NULL, width, height, depth);
  //Write PNG to string
  std::string retImg = image->getString();
  delete image;
  std::vector<unsigned char> d(retImg.length());
  std::copy(retImg.begin(), retImg.end(), d.data());
  return d;
}

DrawingObject* LavaVu::isoSurface(DrawingObject* target, DrawingObject* source, std::string properties, bool clearvol)
{
  //Create an isosurface from selected volume object
  //If "clearvol" is true, volume data will be deleted leaving only the surface triangles
  if (!amodel || !source) return NULL;

  if (!target)
  {
    //Create a new object for the surface
    target = new DrawingObject(session, source->name() + "_isosurface", properties);
    addObject(target);
    std::vector<std::string> copyprops = {"isovalues", "isovalue", "isowalls", "colour"};
    for (auto prop : copyprops)
      if (!target->properties.has(prop) && source->properties.has(prop))
        target->properties.data[prop] = source->properties[prop];
  }

  Volumes* volumes = (Volumes*)amodel->getRenderer(lucVolumeType);
  Triangles* tris = (Triangles*)amodel->getRenderer(lucTriangleType);
  if (volumes && tris)
    volumes->isosurface(tris, source, target, clearvol);
  target->properties.data["geometry"] = "triangles";
  return target;
}

void LavaVu::update(DrawingObject* target, bool compress)
{
  //Re-write the database geometry at current step for specified object - all types
  update(target, lucMaxType, compress);
}

void LavaVu::update(DrawingObject* target, lucGeometryType type, bool compress)
{
  //Re-write the database geometry at current step for specified object - specified type only
  if (!amodel || !target) return;
  amodel->updateObject(target, type, compress);
}

std::vector<float> LavaVu::imageArray(std::string path, int width, int height, int outchannels)
{
  //Return an image as a vector of floats
  //- first 3 values are width,height,channels
  //- read from disk if path provided
  //- read from framebuffer otherwise
  ImageData* image = NULL;
  ImageLoader img;
  if (path.length() > 0)
  {
    img.fn.parse(path);
    img.read();
    image = img.source;
    //printf("Reading file %s %d x %d @ %d (requested %d)\n", path.c_str(), image->width, image->height, image->channels, outchannels);
    //Will discard alpha but adding alpha channel not supported for now
    if ((unsigned int)outchannels > image->channels)
      outchannels = image->channels;
  }
  else
  {
    //Get current image from framebuffer at requested size and depth
    image = viewer->pixels(NULL, width, height, outchannels);
    //printf("Reading framebuffer %d x %d @ %d\n", width, height, outchannels);
  }

  if (!image) return std::vector<float>();

  //Size in pixels
  int size = image->width*image->height;
  std::vector<float> data(3+size*outchannels);
  //Add dims
  data[0] = image->width;
  data[1] = image->height;
  data[2] = image->channels;
  //Load data
  float r255 = 1.0/255.0;
  //If loaded image channels > requested, reduce RGBA->RGB
  bool skipalpha = image->channels > (unsigned int)outchannels;
  int idx=3;
  for (int i=0; i<size*(int)image->channels; i++)
  {
    if (skipalpha && i%4==3) continue;
    data[idx++] = image->pixels[i] * r255;
  }
  //Free image data if we allocated it
  if (image && path.length() == 0)
    delete image;
  return data;
}

float LavaVu::imageDiff(std::string path1, std::string path2, int downsample)
{
  //std::cout << "Comparing " << path1 << " & " << path2 << std::endl;
  //int channels = 3;
  std::vector<float> image1 = imageArray(path1);
  //Request matching channels (if second image from framebuffer)
  int channels = image1[2];
  int width = image1[0];
  int height = image1[1];
  std::vector<float> image2 = imageArray(path2, width, height, channels);

  //Images are different dimensions or colour channelss?
  if (image1.size() != image2.size() ||
      image1[0] != image2[0] ||
      image1[1] != image2[1] ||
      image1[2] != image2[2])
  {
    std::cout << "Image metrics differ: " << std::endl;
    std::cout << " - " << path1 << " != " << path2 << std::endl;
    std::cout << " - " << image1.size() << " : " << image2.size() << std::endl;
    std::cout << " - " << image1[0] << " x " << image1[1] << " : " << image1[2] << std::endl;
    std::cout << " - " << image2[0] << " x " << image2[1] << " : " << image2[2] << std::endl;
    return 1.0;
  }

  //Calculate Mean Squared Error
  float sum = 0;
  float imgsize;
  if (downsample > 1)
  {
    //Average downsample
    int qdims[2] = {(int)(width/(float)downsample+0.5), (int)(height/(float)downsample+0.5)};
    int qsize = qdims[0] * qdims[1] * channels;
    std::vector<float> compare1(qsize);
    std::vector<float> compare2(qsize);
    int oidx = 0;
    for (int i = 0; i < height; i+=downsample)
    {
      for (int j = 0; j < width; j+=downsample)
      {
        for (int k = 0; k < channels; k++)
        {
          //Get average of surrounding pixels
          compare1[oidx] = compare2[oidx] = 0;
          int samples = 0;
          for (int x = i-downsample; x <= i+downsample; x++)
          {
            for (int y = j-downsample; y <= j+downsample; y++)
            {
              int idx = (x*width + y)*channels + k + 3;
              if (x >= 0 && x < height && y >= 0 && y < width)
              {
                compare1[oidx] += image1[idx];
                compare2[oidx] += image2[idx];
                samples++;
              }
            }
          }

          assert(oidx < qsize);
          compare1[oidx] /= samples;
          compare2[oidx] /= samples;
          oidx++;
        }
      }
    }

    //Mean squared error on downsampled images
    for (int i=0; i<qsize; i++)
    {
      float diff = compare1[i] - compare2[i];
      sum += diff*diff;
    }
    imgsize = qsize;
  }
  else
  {
    //Mean squared error on raw images
    imgsize = image1.size() - 3;
    for (unsigned int i=3; i<image1.size(); i++)
    {
      float diff = image1[i] - image2[i];
      sum += diff*diff;
    }
  }

  float MSE = sum / imgsize;
  //float RMSE = sqrt(sum) / sqrt(imgsize);
  //printf("MSE %f  = sum %f / imgsize %f\n", MSE, sum, imgsize);
  //printf("RMSE %f = sqrt(sum) %f / sqrt(imgsize) %f\n", RMSE, sqrt(sum), sqrt(imgsize));
  //return RMSE;
  return MSE;
}


void LavaVu::queueCommands(std::string cmds)
{
  //Thread safe command processing
  //Push command onto queue to be processed in the viewer thread
  std::lock_guard<std::mutex> guard(viewer->cmd_mutex);
  viewer->commands.push_back(cmds);
  viewer->postdisplay = true;
}

