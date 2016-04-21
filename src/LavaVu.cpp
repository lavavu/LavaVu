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

//TODO/FIX:
//Volume colourmap not selected by default
//can't seem to catch exception on property lookup
//Value data types independent from geometry types?
//Timestep inconsistencies in tecplot load
//Lock down save/restore state json format so translation between script commands and json props unecessary
//Merge WebGL/standard shader code

//Viewer class
#include "Include.h"
#include "LavaVu.h"
#include "Shaders.h"
#include <typeinfo>
#include "tiny_obj_loader.h"
#include "Util.h"
#include "Server.h"
#include "OpenGLViewer.h"
#include "Main/SDLViewer.h"
#include "Main/X11Viewer.h"
#include "Main/GlutViewer.h"
#include "Main/OSMesaViewer.h"
#include "Main/CGLViewer.h"
#include "Main/CocoaViewer.h"

#define HELP_COMMANDS "\
\n## Command line arguments\n\n\
```\n\
\nStart and end timesteps\n\
 -# : Any integer entered as a switch will be interpreted as the initial timestep to load\n\
      Any following integer switch will be interpreted as the final timestep for output\n\
      eg: -10 -20 will run all output commands on timestep 10 to 20 inclusive\n\
 -c#: caching, set # of timesteps to cache data in memory for\n\
 -P#: subsample points, loading only every #'th\n\
 -A : All objects hidden initially, use 'show object' to display\n\
 -N : No load, deferred loading mode, use 'load object' to load & display from database\n\
 -S : Skip default script, don't run init.script\n\
\nGeneral options\n\
 -v : Verbose output, debugging info displayed to console\n\
 -o : output mode: all commands entered dumped to standard output,\n\
      useful for redirecting to a script file to recreate a visualisation setup.\n\
 -a : Automation mode, don't activate event processing loop\n\
 -p#: port, web server interface listen on port #\n\
 -q#: quality, web server jpeg quality (0=don't serve images)\n\
 -n#: number of threads to launch for web server #\n\
 -l: use local shaders, locate in working directory not executable directory\n\
 -Q: quiet mode, no status updates to screen\n\
\nModel options\n\
 -C: global camera, each model shares the same camera view\n\
 -y: swap z/y axes of imported static models\n\
 -T#: subdivide imported static model triangles into #\n\
 -V#,#,#: volume data resolution in X,Y,Z\n\
 -D#,#,#-#,#,#: volume data bounds min/max X,Y,Z\n\
 -e: each data set loaded from non time varying source has new timestep inserted\n\
\nImage/Video output\n\
 -z#: render images # times larger and downsample for output\n\
 -i/w: write images of all loaded timesteps/windows then exit\n\
 -I/W: write images as above but using input database path as output path for images\n\
 -u: returns images encoded as string data (for library use)\n\
 -U: returns json model data encoded string (for library use)\n\
 -t: write transparent background png images (if supported)\n\
 -m#: write movies of all loaded timesteps/windows #=fps(30) (if supported)\n\
 -xWIDTH,HEIGHT: set output image width (height optional, will be calculated if omitted)\n\
\nData export\n\
 -d#: export object id # to CSV vertices + values\n\
 -j#: export object id # to JSON, if # omitted will output all compatible objects\n\
 -g#: export object id # to GLDB (zlib compressed), if # omitted will output all compatible objects\n\
 -G#: export object id # to GLDB (uncompressed), if # omitted will output all compatible objects\n\
\nWindow settings\n\
 -rWIDTH,HEIGHT: resize initial viewer window to width x height\n\
 -h: hidden window, will exit after running any provided input script and output options\n\
```\n\
"

ViewerApp* app = NULL;

std::string execute(int argc, char **argv)
{
  //Default entry point, create a LavaVu application and run
  if (!app) app = new LavaVu();
  return execute(argc, argv, app);
}

std::string execute(int argc, char **argv, ViewerApp* myApp)
{
  //Customisable entry point, create viewer and run provided application
  app = myApp;

  //Read command line
  std::vector<std::string> args;
  for (int i=1; i<argc; i++)
  {
    //Help?
    if (strstr(argv[i], "-?") || strstr(argv[i], "help"))
    {
      std::cout << HELP_COMMANDS << std::endl;
      return "";
    }
    if (strlen(argv[i]) > 0)
      args.push_back(argv[i]);
  }

  if (!app->viewer)
  {
    //Create the viewer window
    OpenGLViewer* viewer = createViewer();
    viewer->app = (ApplicationInterface*)app;
    app->viewer = viewer;

    //Shader path (default to program path if not set)
    std::string xpath = GetBinaryPath(argv[0], "LavaVu");
    if (Shader::path.length() == 0) Shader::path = xpath;
    //Set default web server path
    Server::htmlpath = xpath + "html";

    //Add any output attachments to the viewer
    if (Server::port > 0)
      viewer->addOutput(Server::Instance(viewer));
    static StdInput stdi;
    viewer->addInput(&stdi);
  }

  //Reset defaults
  app->defaults();

  //App specific argument processing
  app->arguments(args);

  //Run visualisation task
  return app->run(); 
}

void command(std::string cmd)
{
  app->parseCommands(cmd);
}

std::string image(std::string filename, int width, int height)
{
  std::string result = "";
  if (!app) abort_program("No application initialised!");
  app->display();
  //Set width/height override
  app->viewer->outwidth = width;
  app->viewer->outheight = height;
  //Write image to file or return as string
  result = app->viewer->image(filename);
  return result;
}

void clear()
{
  //Close any open models and free memory
  app->close();
}

void kill()
{
  Server::Delete();
  delete app->viewer;
  app = NULL;
}

OpenGLViewer* createViewer()
{
  OpenGLViewer* viewer = NULL;

//Evil platform specific extension handling stuff
#if defined _WIN32
  //GetProcAddress = (getProcAddressFN)wglGetProcAddress;
#elif defined HAVE_OSMESA
  GetProcAddress = (getProcAddressFN)OSMesaGetProcAddress;
#elif defined HAVE_X11  //not defined __APPLE__
  //GetProcAddress = (getProcAddressFN)glXGetProcAddress;
  GetProcAddress = (getProcAddressFN)glXGetProcAddressARB;
#elif defined HAVE_GLUT and not defined __APPLE__
  GetProcAddress = (getProcAddressFN)glXGetProcAddress;
#endif

  //Create viewer window
#if defined HAVE_X11
  if (!viewer) viewer = new X11Viewer();
#endif
#if defined HAVE_GLUT
  if (!viewer) viewer = new GlutViewer();
#endif
#if defined HAVE_SDL || defined _WIN32
  if (!viewer) viewer = new SDLViewer();
#endif
#if defined HAVE_OSMESA
  if (!viewer) viewer = new OSMesaViewer();
#endif
#if defined HAVE_CGL
  if (!viewer) viewer = new CocoaViewer();
  if (!viewer) viewer = new CGLViewer();
#endif
  if (!viewer) abort_program("No windowing system configured (requires X11, GLUT, SDL, Cocoa, CGL or OSMesa)");

  return viewer;
}

//Viewer class implementation...
LavaVu::LavaVu()
{
  viewer = NULL;
  output = verbose = hideall = dbpath = false;

  defaultScript = "init.script";

  fixedwidth = 0;
  fixedheight = 0;

  axis = new TriSurfaces();
  rulers = new Lines();
  border = new QuadSurfaces();

  defaults();
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

  //Clear any queued commands
  OpenGLViewer::commands.clear();

  viewset = 0;
  view = -1;
  aview = NULL;
  awin = NULL;
  amodel = NULL;
  aobject = NULL;

  files.clear();

  startstep = -1;
  endstep = -1;
  dump = lucExportNone;
  returndata = lucExportNone;
  dumpid = 0;
  globalCam = false;
  status = true;
  writeimage = false;
  writemovie = 0;
#ifdef USE_OMEGALIB
  sort_on_rotate = false;
#else
  sort_on_rotate = true;
#endif
  message[0] = '\0';
  volres[0] = volres[1] = volres[2] = 256;
  volss[0] = volss[1] = volss[2] = 1.0;
  volmin[0] = volmin[1] = volmin[2] = -1;
  volmax[0] = volmax[1] = volmax[2] = 1;
  volchannels = 1;
  volume = NULL;
  inscale[0] = inscale[1] = inscale[2] = 1.0;

  window = -1;
  tracersteps = 0;
  objectlist = false;
  swapY = false;
  trisplit = 0;

  //Interaction command prompt
  entry = "";
  recording = true;

  loop = false;
  animate = false;
  automate = false;
  quiet = false;
  repeat = 0;
#ifdef HAVE_LIBAVCODEC
  encoder = NULL;
#endif
  //Setup default properties
  //(Comments formatted to be parsed into documentation)
  
  //Object defaults
  // | object | string | Name of object
  Properties::defaults["name"] = "";
  // | object | boolean | Set to false to hide object
  Properties::defaults["visible"] = true;
  // | object | boolean | Set to true if object remains static regardless of timestep
  Properties::defaults["static"] = false;
  // | object | boolean | Apply lighting to object
  Properties::defaults["lit"] = true;
  // | object | boolean | Cull back facing polygons of object surfaces
  Properties::defaults["cullface"] = false;
  // | object | boolean | Render object surfaces as wireframe
  Properties::defaults["wireframe"] = false;
  // | object | boolean | Renders surfaces as flat shaded, lines/vectors as 2d, faster but no 3d or lighting
  Properties::defaults["flat"] = false;
  // | object | boolean | Set to false to disable depth test when drawing object so always drawn regardless of 3d position
  Properties::defaults["depthtest"] = true;

  // | object | real [0,1] | Opacity of object where 0 is transparent and 1 is opaque
  Properties::defaults["opacity"] = 1.0;
  // | object | real [-1,1] | Brightness of object from -1 (full dark) to 0 (default) to 1 (full light)
  Properties::defaults["brightness"] = 0.0;
  // | object | real [0,2] | Contrast of object from 0 (none, grey) to 2 (max)
  Properties::defaults["contrast"] = 1.0;
  // | object | real [0,2] | Saturation of object from 0 (greyscale) to 2 (fully saturated)
  Properties::defaults["saturation"] = 1.0;

  // | object | real [0,1] | Ambient lighting level (background constant light)
  Properties::defaults["ambient"] = 0.4;
  // | object | real [0,1] | Diffuse lighting level (shading light/dark)
  Properties::defaults["diffuse"] = 0.8;
  // | object | real [0,1] | Sepcular highlight lighting level (spot highlights)
  Properties::defaults["specular"] = 0.0;

  // | object | boolean | Allow object to be clipped
  Properties::defaults["clip"] = true;
  // | object | real [0,1] | Object clipping, minimum x
  Properties::defaults["xmin"] = -FLT_MAX;
  // | object | real [0,1] | Object clipping, maximum x
  Properties::defaults["ymin"] = -FLT_MAX;
  // | object | real [0,1] | Object clipping, minimum y
  Properties::defaults["zmin"] = -FLT_MAX;
  // | object | real [0,1] | Object clipping, maximum y
  Properties::defaults["xmax"] = FLT_MAX;
  // | object | real [0,1] | Object clipping, minimum z
  Properties::defaults["ymax"] = FLT_MAX;
  // | object | real [0,1] | Object clipping, maximum z
  Properties::defaults["zmax"] = FLT_MAX;

  // | object | integer [0,n] | Glyph quality 0=none, 1=low, higher=increasing triangulation detail (arrows/shapes etc)
  Properties::defaults["glyphs"] = 2;
  // | object | real | Object scaling factor
  Properties::defaults["scaling"] = 1.0;
  // | object | string | External texture image file path to load and apply to surface or points
  Properties::defaults["texturefile"] = "";
  // | object | integer [0,n] | Index of data set to colour object by (requires colour map)
  Properties::defaults["colourby"] = 0;

  // | colourbar | boolean | Indicates object is a colourbar
  Properties::defaults["colourbar"] = false;
  // | colourbar | integer | Align position of colour bar, 1=towards left/bottom, 0=centre, -1=towards right/top
  Properties::defaults["position"] = 0;
  // | colourbar | string | Alignment of colour bar to screen edge, top/bottom/left/right
  Properties::defaults["align"] = "bottom";
  // | colourbar | real [0,1] | Length of colour bar as ratio of screen width or height
  Properties::defaults["lengthfactor"] = 0.8;
  // | colourbar | integer | Width of colour bar in pixels
  Properties::defaults["width"] = 10; //Note: conflict with shape width below, overridden in View.cpp

  //Labels/text
  // | object/colourbar/view | string | Font typeface vector/small/fixed/sans/serif
  Properties::defaults["font"] = "vector";
  // | object/colourbar/view | real | Font scaling, note that only the vector font scales well
  Properties::defaults["fontscale"] = 1.0;

  // | object(line) | real | Line length limit, can be used to skip drawing line segments that cross periodic boundary
  Properties::defaults["limit"] = 0;
  // | object(line) | boolean | To chain line vertices into longer lines set to true
  Properties::defaults["link"] = false;
  // | object(line) | real | Line scaling multiplier, applies to all line objects
  Properties::defaults["scalelines"] = 1.0;
  // | object(line) | real | Line width scaling
  Properties::defaults["linewidth"] = 1.0;
  // | object(line) | boolean | Draw lines as 3D tubes
  Properties::defaults["tubes"] = false;

  // | object(point) | real | Point size scaling
  Properties::defaults["pointsize"] = 1.0;
  // | object(point) | integer/string | Point type 0/blur, 1/smooth, 2/sphere, 3/shiny, 4/flat
  Properties::defaults["pointtype"] = 1;
  // | object(point) | real | Point scaling multiplier, applies to all points objects
  Properties::defaults["scalepoints"] = 1.0;

  // | object(surface) | boolean | If opaque flag is set skips depth sorting step and allows individual surface properties to be applied
  Properties::defaults["opaque"] = false;

  //Properties::defaults["colourmap"] = 1;
  // | object(volume) | real | Power used when applying transfer function, 1.0=linear mapping
  Properties::defaults["power"] = 1.0;
  // | object(volume) | integer | Number of samples to take per ray cast, higher = better quality, slower
  Properties::defaults["samples"] = 256;
  // | object(volume) | real | Density multiplier for volume data
  Properties::defaults["density"] = 5.0;
  // | object(volume) | real | Isovalue for dynamic isosurface
  Properties::defaults["isovalue"] = 0.0;
  // | object(volume) | colour | RGBA colour for isosurface
  Properties::defaults["colour"] = {220, 220, 200, 255};
  // | object(volume) | real [0,1] | Transparency value for isosurface
  Properties::defaults["isoalpha"] = 1.0;
  // | object(volume) | real | Isosurface smoothing factor for normal calculation
  Properties::defaults["isosmooth"] = 0.1;
  // | object(volume) | boolean | Connect isosurface enclosed area with walls
  Properties::defaults["isowalls"] = true;
  // | object(volume) | boolean | Apply a tricubic filter for increased smoothness
  Properties::defaults["tricubicfilter"] = false;
  // | object(volume) | real | Minimum density value to map, lower discarded
  Properties::defaults["dminclip"] = 0.0;
  // | object(volume) | real | Maximum density value to map, higher discarded
  Properties::defaults["dmaxclip"] = 1.0;

  // | object(vector) | real | Arrow head size as a multiple of width
  Properties::defaults["arrowhead"] = 2.0;
  // | object(vector) | real | Vector scaling multiplier, applies to all vector objects
  Properties::defaults["scalevectors"] = 1.0;
  // | object(vector) | real | Arrow fixed shaft radius, default is to calculate proportional to length
  Properties::defaults["radius"] = 0.0;

  // | object(tracer) | integer | Number of time steps to trace particle path
  Properties::defaults["steps"] = 0;
  // | object(tracer) | boolean | Taper width of tracer arrow up as we get closer to current timestep
  Properties::defaults["taper"] = true;
  // | object(tracer) | boolean | Fade opacity of tracer arrow in from transparent as we get closer to current timestep
  Properties::defaults["fade"] = false;
  // | object(tracer) | real | Tracer scaling multiplier, applies to all tracer objects
  Properties::defaults["scaletracers"] = 1.0;

  // | object(shape) | real | Shape width scaling factor
  Properties::defaults["width"] = FLT_MIN;
  // | object(shape) | real | Shape height scaling factor
  Properties::defaults["height"] = FLT_MIN;
  // | object(shape) | real | Shape length scaling factor
  Properties::defaults["length"] = FLT_MIN;
  // | object(shape) | integer | Shape type: 0=ellipsoid, 1=cuboid
  Properties::defaults["shape"] = 0;
  // | object(shape) | real | Shape scaling multiplier, applies to all shape objects
  Properties::defaults["scaleshapes"] = 1.0;

  // | colourbar | integer | Number of additional tick marks to draw besides start and end
  Properties::defaults["ticks"] = 0;
  // | colourbar | boolean | Set to false to disable drawing of tick values
  Properties::defaults["printticks"] = true;
  // | colourbar | string | Units to print with tick values
  Properties::defaults["units"] = "";
  // | colourbar | boolean | Set to true to use scientific exponential notation for tick values
  Properties::defaults["scientific"] = false;
  // | colourbar | integer | Number of significant decimal digits to show
  Properties::defaults["precision"] = 2;
  // | colourbar | real | Multiplier to scale tick values
  Properties::defaults["scalevalue"] = 1.0;
  // | colourbar | integer | Border width to draw around colour bar
  Properties::defaults["border"] = 1.0; //Conflict with global, overridden below

  // | colourmap | boolean | Set to true to use log scales
  Properties::defaults["log"] = false;
  // | colourmap | boolean | Set to true to apply colours as discrete values rather than gradient
  Properties::defaults["discrete"] = false;
  // | colourmap | colours | Colour list, see [Colour map lists] for more information
  Properties::defaults["colours"] = "";
  // | colourmap | boolean | Automatically calculate colourmap min/max range based on available data range
  Properties::defaults["dynamic"] = true;

  // | view | string | Title to display at top centre of view
  Properties::defaults["title"] = "";
  // | view | integer | When to apply camera auto-zoom to fit model to window, -1=never, 0=first timestep only, 1=every timestep
  Properties::defaults["zoomstep"] = -1;
  // | view | integer | Margin in pixels to leave around edge of model when to applying camera auto-zoom
  Properties::defaults["margin"] = 20; //Also colourbar
  // | view | boolean | Draw rulers around object axes
  Properties::defaults["rulers"] = false;
  // | view | integer | Number of tick marks to display on rulers
  Properties::defaults["rulerticks"] = 5;
  // | view | real | Width of ruler lines
  Properties::defaults["rulerwidth"] = 1.5;
  // | view | integer | Border width around model boundary, 0=disabled
  Properties::defaults["border"] = 1.0;
  // | view | boolean | Draw filled background box around model boundary
  Properties::defaults["fillborder"] = false;
  // | view | colour | Colour of model boundary border
  Properties::defaults["bordercolour"] = "grey";
  // | view | boolean | Draw X/Y/Z legend showing model axes orientation
  Properties::defaults["axis"] = true;
  // | view | real | Axis legend scaling factor
  Properties::defaults["axislength"] = 0.1;
  // | view | boolean | Draw a timestep label at top right of view - CURRENTLY NOT IMPLEMENTED
  Properties::defaults["timestep"] = false;
  // | view | boolean | Enable multisample anti-aliasing, only works with interactive viewing
  Properties::defaults["antialias"] = true; //Should be global
  // | view | integer | Apply a shift to object depth sort index by this amount multiplied by id, improves visualising objects drawn at same depth
  Properties::defaults["shift"] = 0;
  //View: Camera
  // | view | real[4] | Camera rotation quaternion [x,y,z,w]
  Properties::defaults["rotate"] = {0., 0., 0., 1.};
  // | view | real[3] | Camera translation [x,y,z]
  Properties::defaults["translate"] = {0., 0., 0.};
  // | view | real[3] | Camera focal point [x,y,z]
  Properties::defaults["focus"] = {0., 0., 0.};
  // | view | real[3] | Global model scaling factors [x,y,z]
  Properties::defaults["scale"] = {1., 1., 1.};
  // | view | real[3] | Global model minimum bounds [x,y,z]
  Properties::defaults["min"] = {0., 0., 0.};
  // | view | real[3] | Global model maximum bounds [x,y,z]
  Properties::defaults["max"] = {1., 1., 1.};
  // | view | real | Near clipping plane position, adjusts where geometry close to the camera is clipped
  Properties::defaults["near"] = 0.1;
  // | view | real | Far clip plane position, adjusts where far geometry is clipped
  Properties::defaults["far"] = 10.0;
  // | view | integer | Set to determine coordinate system, 1=Right-handed (OpenGL default) -1=Left-handed
  Properties::defaults["orientation"] = 1;

  //Global Properties
  // | global | boolean | Turn on to keep all volumes in GPU memory between timesteps
  Properties::defaults["cachevolumes"] = false;
  // | global | boolean | Turn on to automatically add and switch to a new timestep after loading a data file
  Properties::defaults["filestep"] = false;

#ifdef DEBUG
  //std::cerr << std::setw(2) << Properties::defaults << std::endl;
#endif
}

LavaVu::~LavaVu()
{
  delete axis;
  delete rulers;
  delete border;
#ifdef HAVE_LIBAVCODEC
  if (encoder) delete encoder;
#endif
  //Kill all geom data
  if (TimeStep::cachesize == 0)
  {
    for (unsigned int i=0; i < Model::geometry.size(); i++)
      delete Model::geometry[i];
  }

  //Kill all models
  for (unsigned int i=0; i < models.size(); i++)
    delete models[i];

  debug_print("Peak geometry memory usage: %.3f mb\n", mempeak__/1000000.0f);
}

void LavaVu::arguments(std::vector<std::string> args)
{
  //Read command line switches
  bool queueScript = false;
  for (unsigned int i=0; i<args.size(); i++)
  {
    char x;
    std::istringstream ss(args[i]);
    ss >> x;
    //Switches can be before or after files but not between
    if (x == '-' && args[i].length() > 1)
    {
      ss >> x;
      //Unused switches: bfks, BEFHKLMOXYZ
      switch (x)
      {
      case 'a':
        //Automation mode:
        //return from run() immediately after loading
        //All actions to be performed by subsequent 
        //library calls
        automate = true;
      case 'z':
        //Downsample images
        ss >> viewer->downsample;
        break;
      case 'p':
        //Web server enable
        ss >> Server::port;
        break;
      case 'q':
        //Web server JPEG quality
        ss >> Server::quality;
        break;
      case 'n':
        //Web server threads
        ss >> Server::threads;
        break;
      case 'r':
        ss >> fixedwidth >> x >> fixedheight;
        break;
      case 'P':
        //Points initial sub-sampling
        if (args[i].length() > 2)
          ss >> Points::subSample;
        else
          parseCommands("pointspheres");
        break;
      case 'N':
        parseCommands("noload");
        break;
      case 'A':
        parseCommands("hideall");
        break;
      case 'v':
        parseCommands("verbose on");
        break;
      case 'o':
        //Set script output flag
        output = true;
        break;
      case 'x':
        ss >> viewer->outwidth >> x >> viewer->outheight;
        break;
      case 'c':
        ss >> TimeStep::cachesize;
        break;
      case 't':
        //Use alpha channel in png output
        parseCommands("pngalpha");
        break;
      case 'y':
        //Swap y & z axis on import
        parseCommands("swapyz");
        break;
      case 'T':
        //Split triangles
        ss >> trisplit;
        break;
      case 'C':
        //Global camera
        parseCommands("globalcam");
        break;
      case 'l':
        //Use local shader files (set shader path to current working directory)
        parseCommands("localshaders");
        break;
      case 'V':
        ss >> volres[0] >> x >> volres[1] >> x >> volres[2];
        break;
      case 'D':
        ss >> volmin[0] >> x >> volmin[1] >> x >> volmin[2];
        ss >> x >> volmax[0] >> x >> volmax[1] >> x >> volmax[2];
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
        Properties::globals["filestep"] = true;
        break;
      case 'u':
        //Return encoded image string from run()
        returndata = lucExportIMAGE;
        break;
      case 'U':
        //Return encoded json string from run()
        returndata = lucExportJSON;
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
    else if (x == ':')
    {
      //Queue rest of commands to script
      queueScript = true;
    }
    else if (queueScript)
    {
      //Queue script commands for when viewer is opened
      OpenGLViewer::commands.push_back(args[i]);
    }
    else
    {
      //Model data file
      files.push_back(args[i]);
    }
  }

  //Output and exit modes?
  if (writeimage || writemovie || dump > lucExportNone)
  {
    viewer->visible = false;
    Server::port = 0;
  }
}

std::string LavaVu::run()
{
  std::string ret = "";
  //If server running, always stay open (persist flag)
  bool persist = Server::running();

  //Set default timestep if none specified
  if (startstep < 0) startstep = 0;

  //Add default script
  if (defaultScript.length())
    files.push_back(defaultScript);

  //Loads files, runs scripts
  for (unsigned int m=0; m < files.size(); m++)
    loadFile(files[m]);
    //OpenGLViewer::commands.push_back("file " + files[m].full);

  //Require a model from here on, set a default
  if (!amodel) defaultModel();

  //Moved to loadFile so called after all model data loads, not just first
  //Reselect the active view after loading all data (resets model bounds)
  //NOTE: sometimes we can reach this call before the GL context is created, hence the check
  //if (viewer->isopen)
  //  viewSelect(view, true, true);

  if (writeimage || writemovie || dump > lucExportNone)
  {
    persist = false; //Disable persist
    //Load vis data for each window and write image
    if (!writeimage && !writemovie && dump != lucExportJSON && dump != lucExportJSONP)
      viewer->isopen = true; //Skip open
    for (unsigned int win=0; win < windows.size(); win++)
    {
      //Load the window data
      loadWindow(win, startstep, true);

      //Bounds checks
      if (endstep < startstep) endstep = startstep;
      int final = amodel->lastStep();
      if (endstep < final) final = endstep;

      if (writeimage || writemovie)
      {
        resetViews(true);
        viewer->display();
        //Read input script from stdin on first timestep
        viewer->pollInput();

        if (writeimage)
        {
          std::cout << "... Writing image(s) for window " << awin->name << " Timesteps: " << startstep << " to " << endstep << std::endl;
        }
        if (writemovie)
        {
          std::cout << "... Writing movie for window " << awin->name << " Timesteps: " << startstep << " to " << endstep << std::endl;
          //Other formats?? avi/mpeg4?
          encodeVideo("", writemovie);
        }

        //Write output
        writeSteps(writeimage, startstep, endstep);
      }

      //Export data
      Model::triSurfaces->loadMesh();  //Optimise triangle meshes before export
      exportData(dump, dumpid);
    }
  }
  else
  {
    //Cache data if enabled
    cacheLoad();

    //Load first window if not yet loaded
    if (window < 0)
      loadWindow(0, startstep, true);
  }

  //If automation mode turned on, return at this point
  if (automate) return ret;

  //Return an image encoded as a base64 data url
  if (returndata == lucExportIMAGE)
  {
    ret = viewer->image();
  }
  else if (returndata == lucExportJSON)
  {
    std::stringstream ss;
    Model::triSurfaces->loadMesh();  //Optimise triangle meshes before export
    jsonWrite(ss, 0, true);
    ret = ss.str();
  }
  else
  {
    //Start event loop
    if (persist || viewer->visible)
      viewer->execute();
    else
    {
      //Read input script from stdin on first timestep
      viewer->pollInput();
      viewer->display();
    }
  }

  return ret;
}

void LavaVu::clearData(bool objects)
{
  //Clear data
  if (amodel) amodel->clearObjects(true);
  //Delete objects? only works for active view/window/model
  if (objects)
  {
    if (aview) aview->objects.clear();
    if (amodel) amodel->objects.clear();
    aobject = NULL;
    DrawingObject::lastid = 0;
  }
}

void LavaVu::exportData(lucExportType type, unsigned int id)
{
  //Export data
  viewer->display();
  if (type == lucExportJSON)
    jsonWriteFile(id);
  else if (type == lucExportJSONP)
    jsonWriteFile(id, true);
  else if (type == lucExportGLDB)
    amodel->writeDatabase("exported.gldb", id);
  else if (type == lucExportGLDBZ)
    amodel->writeDatabase("exported.gldb", id, true);
  else if (type == lucExportCSV)
  {
    if (id > 0)
      dumpById(dumpid);
    else
    {
      for (unsigned int i=0; i < amodel->objects.size(); i++)
        dumpById(amodel->objects[i]->id);
    }
  }
}

void LavaVu::cacheLoad()
{
  if (amodel->db && TimeStep::cachesize > 0) //>= amodel->timesteps.size())
  {
    debug_print("Caching all geometry data...\n");
    for (unsigned int m=0; m < models.size(); m++)
    {
      amodel = models[m];
      awin = models[m]->windows[0];
      amodel->loadTimeSteps();
      amodel->now = -1;
      for (unsigned int i=0; i<amodel->timesteps.size(); i++)
      {
        amodel->setTimeStep(i);
        if (amodel->now != (int)i) break; //Finished early using loadGeometry caching
        debug_print("Cached time %d : %d/%d (%s)\n", amodel->step(), i+1, amodel->timesteps.size(), amodel->file.base.c_str());
      }
      //Cache final step
      amodel->cacheStep();
    }
    window = -1;
  }
}

//Property containers now using json
//Parse lines with delimiter, ie: key=value
void LavaVu::parseProperties(std::string& properties)
{
  //Process all lines
  std::stringstream ss(properties);
  std::string line;
  while(std::getline(ss, line))
    parseProperty(line);
};

void LavaVu::parseProperty(std::string& data)
{
  //Set properties of selected object or view/globals
  std::string key = data.substr(0,data.find("="));
  if (aobject)
  {
    aobject->properties.parse(data);
    if (infostream != NULL)
      std::cerr << "OBJECT " << std::setw(2) << aobject->name << ", DATA: " << aobject->properties.data << std::endl;
  }
  else if (aview && aview->properties.data.count(key) > 0)
  {
    aview->properties.parse(data);
    if (infostream != NULL)
      std::cerr << "VIEW: " << std::setw(2) << aview->properties.data << std::endl;
  }
  else
  {
    //Properties not found on view are set globally
    aview->properties.parse(data, true);
    if (infostream != NULL)
      std::cerr << "GLOBAL: " << std::setw(2) << Properties::globals << std::endl;
  }
}

void LavaVu::printProperties()
{
  //Show properties of selected object or view/globals
  if (aobject)
    std::cerr << "OBJECT " << aobject->name << ", DATA: " << std::setw(2) << aobject->properties.data << std::endl;
  else
  {
    std::cerr << "VIEW: " << std::setw(2) << aview->properties.data << std::endl;
    std::cerr << "GLOBAL: " << std::setw(2) << Properties::globals << std::endl;
  }
}

void LavaVu::printDefaultProperties()
{
  //Show default properties
  std::cerr << std::setw(2) << Properties::defaults << std::endl;
}

void LavaVu::readRawVolume(FilePath& fn)
{
  //raw format volume data

  //Create volume object, or if static volume object exists, use it
  DrawingObject *vobj = volume;
  if (!vobj) vobj = new DrawingObject(fn.base);
  addObject(vobj);

  std::fstream file(fn.full.c_str(), std::ios::in | std::ios::binary);
  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (!file.is_open() || size <= 0) abort_program("File error %s\n", fn.full.c_str());
  std::vector<char> buffer(size);
  file.read(&buffer[0], size);
  file.close();

  //Define the bounding cube by corners
  Model::volumes->add(vobj);
  Model::volumes->read(vobj, 1, lucVertexData, volmin);
  Model::volumes->read(vobj, 1, lucVertexData, volmax);
  //Ensure count rounded up when storing bytes in float container
  int floatcount = ceil((float)(size) / sizeof(float));
  Model::volumes->read(vobj, floatcount, lucColourValueData, &buffer[0], volres[0], volres[1], volres[2]);
  Model::volumes->setup(vobj, lucColourValueData, 0, 1);
}

void LavaVu::readXrwVolume(FilePath& fn)
{
  //Xrw format volume data

  //Create volume object, or if static volume object exists, use it
  DrawingObject *vobj = volume;
  if (!vobj) vobj = new DrawingObject(fn.base);
  addObject(vobj);

  std::vector<char> buffer;
  unsigned int size;
  unsigned int floatcount;
#ifdef USE_ZLIB
  if (fn.type != "xrwu")
  {
    gzFile f = gzopen(fn.full.c_str(), "rb");
    gzread(f, (char*)volres, sizeof(int)*3);
    gzread(f, (char*)volmax, sizeof(float)*3);
    volmin[0] = volmin[1] = volmin[2] = 0;
    size = volres[0]*volres[1]*volres[2];
    //Ensure a multiple of 4 bytes
    floatcount = ceil(size / 4.0);
    size = floatcount * 4;
    buffer.resize(size);
    std::cout << "SIZE " << size << " Bytes, " << floatcount << " Floats, rounded up: " << (floatcount*4) << " Bytes, Actual: " << buffer.size() << "\n";
    int chunk = 100000000; //Read in 100MB chunks
    int len, err;
    unsigned int offset = 0;
    do
    {
      if (chunk+offset > size) chunk = size - offset; //Last chunk?
      debug_print("Offset %ld Chunk %ld\n", offset, chunk);
      len = gzread(f, &buffer[offset], chunk);
      if (len != chunk) abort_program("gzread err: %s\n", gzerror(f, &err));

      offset += chunk;
    }
    while (offset < size);
    gzclose(f);
  }
  else
#endif
  {
    std::fstream file(fn.full.c_str(), std::ios::in | std::ios::binary);
    file.seekg(0, std::ios::end);
    size = file.tellg();
    file.seekg(0, std::ios::beg);
    file.read((char*)volres, sizeof(int)*3);
    file.read((char*)volmax, sizeof(float)*3);
    volmin[0] = volmin[1] = volmin[2] = 0;
    size -= sizeof(int)*3 + sizeof(float)*3;
    //Ensure a multiple of 4 bytes
    floatcount = ceil(size / 4.0);
    //std::cout << "SIZE " << size << " Bytes, " << floatcount << " Floats, rounded up: " << (floatcount*4) << " Bytes\n";
    size = floatcount * 4;
    if (!file.is_open() || size <= 0) abort_program("File error %s\n", fn.full.c_str());
    buffer.resize(size);
    file.read(&buffer[0], size);
    file.close();
  }

#if 0
  //Dump slices
  size_t offset = 0;
  char path[FILE_PATH_MAX];
  for (int slice=0; slice<volres[2]; slice++)
  {
    //Write data to image file
    sprintf(path, "slice-%03da.png", slice);
    std::ofstream file(path, std::ios::binary);
    write_png(file, 1, volres[0], volres[1], &buffer[offset]);
    file.close();
    offset += volres[0] * volres[1];
    printf("offset %ld\n", offset);
  }
#endif

  //Scale geometry by input scaling factor
  for (int i=0; i<3; i++)
  {
    volmin[i] *= inscale[i];
    volmax[i] *= inscale[i];
    if (infostream != NULL)
      std::cerr << i << " " << inscale[i] << " : MIN " << volmin[i] << " MAX " << volmax[i] << std::endl;
  }

  //Define the bounding cube by corners
  Model::volumes->add(vobj);
  Model::volumes->read(vobj, 1, lucVertexData, volmin);
  Model::volumes->read(vobj, 1, lucVertexData, volmax);

  Model::volumes->read(vobj, floatcount, lucColourValueData, &buffer[0], volres[0], volres[1], volres[2]);
  Model::volumes->setup(vobj, lucColourValueData, 0, 1);
}

void LavaVu::readVolumeSlice(FilePath& fn)
{
  //png/jpg data
  static std::string path = "";
  if (!volume || path != fn.path)
  {
    path = fn.path; //Store the path, multiple volumes can be loaded if slices in different folders
    volume = NULL;  //Ensure new volume created
  }

  ImageFile image(fn);
  if (image.pixels)
  {
    readVolumeSlice(fn.base, image.pixels, image.width, image.height, image.bytesPerPixel, volchannels);
  }
  else
    debug_print("Slice load failed: %s\n", fn.full.c_str());
}

void LavaVu::readVolumeSlice(std::string& name, GLubyte* imageData, int width, int height, int bytesPerPixel, int outChannels)
{
  //Create volume object, or if static volume object exists, use it
  static int count = 0;
  DrawingObject *vobj = volume;
  if (!vobj)
  {
    vobj = addObject(new DrawingObject(name, "static=1"));
    //Scale geometry by input scaling factor
    for (int i=0; i<3; i++)
    {
      volmin[i] *= inscale[i];
      volmax[i] *= inscale[i];
      if (infostream != NULL)
        std::cerr << i << " " << inscale[i] << " : MIN " << volmin[i] << " MAX " << volmax[i] << std::endl;
    }
    //Define the bounding cube by corners
    Model::volumes->add(vobj);
    Model::volumes->read(vobj, 1, lucVertexData, volmin);
    Model::volumes->read(vobj, 1, lucVertexData, volmax);
  }
  else
    Model::volumes->add(vobj);

  //Save static volume for loading multiple slices
  volume = vobj;
  count++;

  if (outChannels == 1)
  {
    //Convert to luminance (just using red channel now, other options in future)
    GLubyte* luminance = new GLubyte[width*height];
    for (int y=0; y<height; y++)
      for (int x=0; x<width; x++)
        luminance[y*width+x] = imageData[(y*width+x)*bytesPerPixel];

    //Ensure count rounded up when storing bytes in float container
    int floatcount = ceil((float)(width * height) / sizeof(float));
    Model::volumes->read(vobj, floatcount, lucColourValueData, luminance, width, height); //, count);
    Model::volumes->setup(vobj, lucColourValueData, 0, 1);
    //std::cout << "SLICE LOAD: " << width << "," << height << " bpp: " << bytesPerPixel << std::endl;

    delete[] luminance;
  }
  else
  {
    Model::volumes->read(vobj, width*height, lucRGBAData, imageData, width, height); //, count);
    std::cout << "SLICE LOAD " << count << " : " << width << "," << height << " bpp: " << bytesPerPixel << std::endl;
  }
}

void LavaVu::readVolumeTIFF(FilePath& fn)
{
#ifdef HAVE_LIBTIFF
  TIFF* tif = TIFFOpen(fn.full.c_str(), "r");
  if (tif)
  {
    unsigned int width, height;
    size_t npixels;
    int bytesPerPixel = 4;
    GLubyte* imageData;
    int count = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    npixels = width * height;
    imageData = (GLubyte*)_TIFFmalloc(npixels * bytesPerPixel * sizeof(GLubyte));
    if (imageData)
    {
      int w = width * volss[0];
      int h = height * volss[1];
      GLuint* buffer = new GLuint[w*h];
      do
      {
        if (TIFFReadRGBAImage(tif, width, height, (uint32*)imageData, 0))
        {
          //Add new store for each slice
          //Subsample
          GLuint* bp = buffer;
          for (int y=0; y<h; y ++)
          {
            for (int x=0; x<w; x ++)
            {
              assert((bp-buffer) < w * h);
              memcpy(bp, &imageData[(y*width+x)*bytesPerPixel], 4);
              bp++;
            }
          }

          readVolumeSlice(fn.base, (GLubyte*)buffer, w, h, bytesPerPixel, volchannels);
        }
        count++;
      }
      while (TIFFReadDirectory(tif));

      _TIFFfree(imageData);
      delete[] buffer;
    }
    TIFFClose(tif);
  }
#else
  abort_program("Require libTIFF to load TIFF images\n");
#endif
}

void LavaVu::createDemoVolume()
{
  //Create volume object, or if static volume object exists, use it
  DrawingObject *vobj = volume;
  if (!vobj)
  {
    vobj = new DrawingObject("volume", "density=50\n");
    addObject(vobj);
    //Scale geometry by input scaling factor
    for (int i=0; i<3; i++)
    {
      volmin[i] *= inscale[i];
      volmax[i] *= inscale[i];
      if (infostream != NULL)
        std::cerr << i << " " << inscale[i] << " : MIN " << volmin[i] << " MAX " << volmax[i] << std::endl;
    }
    //Define the bounding cube by corners
    Model::volumes->read(vobj, 1, lucVertexData, volmin);
    Model::volumes->read(vobj, 1, lucVertexData, volmax);
  }

#if 1
  unsigned int width = 256, height = 256, depth = 256, block = 24;
  size_t npixels = width*height;
  int bytesPerPixel = 4;
  GLubyte* imageData;
  npixels = width * height;
  imageData = (GLubyte*)malloc(npixels * bytesPerPixel * sizeof(GLubyte));
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

      if (z > 0) Model::volumes->add(vobj);
      Model::volumes->read(vobj, width * height, lucRGBAData, imageData, width, height);
      std::cout << "SLICE LOAD " << z << " : " << width << "," << height << " bpp: " << bytesPerPixel << std::endl;
    }
  }
#else
  unsigned int size;
  unsigned int width = 256, height = 256, depth = 256, block = 24;
  size_t npixels = width*height;
  int bytesPerPixel = 1;
  GLubyte* imageData;
  int count = 0;
  npixels = width * height;
  imageData = (GLubyte*)malloc(npixels * bytesPerPixel * sizeof(GLubyte));
  if (imageData)
  {
    bool val;
    for (int z=0; z<depth; z++)
    {
      //Add new store for each slice
      //GLuint* bp = (GLuint*)imageData;
      GLubyte* bp = imageData;
      for (int y=0; y<height; y ++)
      {
        for (int x=0; x<width; x ++)
        {
          //assert((bp-(GLuint*)imageData) < width * height);
          if (z/block%2==0 || x/block%2==0 || y/block%2==0)
            val = false;
          else
            val = true;
          //memset(bp, val ? 255 : 0, 4);
          *bp = (val ? 255 : 0);
          bp++;
        }
      }

      //if (z > 0) Model::volumes->add(vobj);
      int floatcount = ceil((float)(width * height) / sizeof(float));
      //Model::volumes->read(vobj, width * height, lucRGBAData, imageData, width, height, depth);
      Model::volumes->read(vobj, floatcount, lucColourValueData, (float*)imageData, width, height, depth);
      Model::volumes->setup(vobj, lucColourValueData, 0, 1);
      std::cout << "SLICE LOAD " << z << " : " << width << "," << height << " bpp: " << bytesPerPixel << std::endl;
    }
  }
  free(imageData);
#endif
}

void LavaVu::readHeightMap(FilePath& fn)
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

  float* min = awin->min;
  float* max = awin->max;
  float range[3] = {0,0,0};

  min[0] = xmap;
  max[0] = xmap + (cols * xdim);
  min[1] = 0;
  max[1] = 1000;
  min[2] = ymap - (rows * xdim); //Northing decreases with latitude
  max[2] = ymap;
  range[0] = fabs(max[0] - min[0]);
  range[2] = fabs(max[2] - min[2]);

  viewer->title = fn.base;

  //Create a height map grid
  int sx=cols, sz=rows;
  debug_print("Height dataset %d x %d Sampling at X,Z %d,%d\n", sx, sz, sx / subsample, sz / subsample);
  //opacity [0,1]
  DrawingObject *obj;
  std::string props = "static=1\ncolour=[238,238,204]\ncullface=0\ntexturefile=" + texfile + "\n";
  obj = addObject(new DrawingObject(fn.base, props));
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
      Model::geometry[geomtype]->read(obj, 1, lucVertexData, vertex.ref(), gridx, gridz);
      //Colour by height
      Model::geometry[geomtype]->read(obj, 1, lucColourValueData, &colourval);

      vertex[0] += xdim * subsample;
    }
    vertex[0] = min[0];
    vertex[2] -= ydim * subsample;
  }
  file.close();
  Model::geometry[geomtype]->setup(obj, lucColourValueData, min[1], max[1]);

  range[1] = max[1] - min[1];
  debug_print("Sampled %d values, min height %f max height %f\n", (sx / subsample) * (sz / subsample), min[1], max[1]);
  //debug_print("Range %f,%f,%f to %f,%f,%f\n", min[0], min[1], min[2], max[0], max[1], max[2]);

  debug_print("X min %f max %f range %f\n", min[0], max[0], range[0]);
  debug_print("Y min %f max %f range %f\n", min[1], max[1], range[1]);
  debug_print("Z min %f max %f range %f\n", min[2], max[2], range[2]);
}

void LavaVu::readHeightMapImage(FilePath& fn)
{
  ImageFile image(fn);

  if (!image.pixels) return;

  int geomtype = lucTriangleType;
  //int geomtype = lucGridType;

  float heightrange = 10.0;
  float min[3] = {0, 0, 0};
  float max[3] = {(float)image.width, heightrange, (float)image.height};

  viewer->title = fn.base;

  //Create a height map grid
  std::string texfile = fn.base + "-texture." + fn.ext;
  DrawingObject *obj;
  std::string props = "static=1\ncolour=[238,238,204]\ncullface=0\ntexturefile=" + texfile + "\n";
  obj = addObject(new DrawingObject(fn.base, props));

  Vec3d vertex;

  //Use red channel as luminance for now
  for (int z=0; z<image.height; z++)
  {
    vertex[2] = z;
    for (int x=0; x<image.width; x++)
    {
      vertex[0] = x;
      vertex[1] = heightrange * image.pixels[(z*image.width+x)*image.bytesPerPixel] / 255.0;

      float colourval = vertex[1];

      if (vertex[1] < min[1]) min[1] = vertex[1];
      if (vertex[1] > max[1]) max[1] = vertex[1];

      //Add grid point
      Model::geometry[geomtype]->read(obj, 1, lucVertexData, vertex.ref(), image.width, image.height);
      //Colour by height
      Model::geometry[geomtype]->read(obj, 1, lucColourValueData, &colourval);
    }
  }

  Model::geometry[geomtype]->setup(obj, lucColourValueData, min[1], max[1]);
}

void LavaVu::addTriangles(DrawingObject* obj, float* a, float* b, float* c, int level)
{
  level--;
  //float a_b[3], a_c[3], b_c[3];
  //vectorSubtract(a_b, a, b);
  //vectorSubtract(a_c, a, c);
  //vectorSubtract(b_c, b, c);
  //float max = 100000; //aview->model_size / 100.0;
  //printf("%f\n", max); getchar();

  if (level <= 0) // || (dotProduct(a_b,a_b) < max && dotProduct(a_c,a_c) < max && dotProduct(b_c,b_c) < max))
  {
    float A[3] = {a[0], a[2], a[1]};
    float B[3] = {b[0], b[2], b[1]};
    float C[3] = {c[0], c[2], c[1]};
    if (swapY)
    {
      a = A;
      b = B;
      c = C;
    }

    //Read the triangle
    Model::triSurfaces->read(obj, 1, lucVertexData, a);
    Model::triSurfaces->read(obj, 1, lucVertexData, b);
    Model::triSurfaces->read(obj, 1, lucVertexData, c);
  }
  else
  {
    //Process a triangle into 4 sub-triangles
    float ab[3] = {0.5f*(a[0]+b[0]), 0.5f*(a[1]+b[1]), 0.5f*(a[2]+b[2])};
    float ac[3] = {0.5f*(a[0]+c[0]), 0.5f*(a[1]+c[1]), 0.5f*(a[2]+c[2])};
    float bc[3] = {0.5f*(b[0]+c[0]), 0.5f*(b[1]+c[1]), 0.5f*(b[2]+c[2])};

    addTriangles(obj, a, ab, ac, level);
    addTriangles(obj, ab, b, bc, level);
    addTriangles(obj, ac, bc, c, level);
    addTriangles(obj, ab, bc, ac, level);
  }
}

void LavaVu::readOBJ(FilePath& fn)
{
  //Use tiny_obj_loader to load a model
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  bool ret = tinyobj::LoadObj(shapes, materials, err, fn.full.c_str());

  if (!err.empty() || !ret)
  {
    std::cerr << "Error loading OBJ file: " << fn.full << " - " << err << std::endl;
    return;
  }

  std::cout << "# of shapes    : " << shapes.size() << std::endl;
  std::cout << "# of materials : " << materials.size() << std::endl;

  //Add single drawing object per file, if one is already active append to it
  DrawingObject* tobj = aobject;
  if (!tobj) tobj = addObject(new DrawingObject(fn.base, "static=1\ncolour=[128,128,128]\n"));

  for (size_t i = 0; i < shapes.size(); i++)
  {
    //Strip path from name
    size_t last_slash = shapes[i].name.find_last_of("\\/");
    if (std::string::npos != last_slash)
      shapes[i].name = shapes[i].name.substr(last_slash + 1);

    printf("shape[%ld].name = %s\n", i, shapes[i].name.c_str());
    printf("Size of shape[%ld].material_ids: %ld\n", i, shapes[i].mesh.material_ids.size());

    //Add new triangles data store to object
    Model::triSurfaces->add(tobj);

    if (shapes[i].mesh.material_ids.size() > 0)
    {
      //Just take the first id...
      int id = shapes[i].mesh.material_ids[0];
      if (id >= 0)
      {
        //Use the diffuse property as the colour/texture
        if (materials[id].diffuse_texname.length() > 0)
        {
          std::cerr << "APPLYING DIFFUSE TEXTURE: " << materials[id].diffuse_texname << std::endl;
          std::string texpath = materials[id].diffuse_texname;
          if (fn.path.length() > 0)
            texpath = fn.path + "/" + texpath;

          //Add per-object texture
          Model::triSurfaces->setTexture(tobj, tobj->addTexture(texpath));
          //if (tobj->properties["texturefile"].ToString("").length() == 0)
          //  tobj->properties.data["texturefile"] = texpath;
        }
        else
        {
          Colour c;
          c.r = materials[id].diffuse[0] * 255;
          c.g = materials[id].diffuse[1] * 255;
          c.b = materials[id].diffuse[2] * 255;
          c.a = materials[id].dissolve * 255;
          if (c.a == 0.0) c.a = 255;
          Model::triSurfaces->read(tobj, 1, lucRGBAData, &c.value);
        }
      }
    }

    //Default is to load the indexed triangles and provided normals
    //Can be overridden by setting trisplit (-T#)
    //Setting to 1 will calculate our own normals and optimise mesh
    //Setting > 1 also divides triangles into smaller pieces first
    if (trisplit == 0)
    {
      GeomData* g = Model::triSurfaces->read(tobj, shapes[i].mesh.positions.size()/3, lucVertexData, &shapes[i].mesh.positions[0]);
      Model::triSurfaces->read(tobj, shapes[i].mesh.indices.size(), lucIndexData, &shapes[i].mesh.indices[0]);
      if (shapes[i].mesh.normals.size() > 0)
        Model::triSurfaces->read(tobj, shapes[i].mesh.normals.size()/3, lucNormalData, &shapes[i].mesh.normals[0]);
      for (size_t f = 0; f < shapes[i].mesh.positions.size(); f += 3)
        g->checkPointMinMax(&shapes[i].mesh.positions[f]);
    }
    else
    {
      //This won't work with textures and tex coords... ordering incorrect
      for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++)
      {
        unsigned ids[3] = {shapes[i].mesh.indices[3*f], shapes[i].mesh.indices[3*f+1], shapes[i].mesh.indices[3*f+2]};
        addTriangles(tobj,
                     &shapes[i].mesh.positions[ids[0]*3],
                     &shapes[i].mesh.positions[ids[1]*3],
                     &shapes[i].mesh.positions[ids[2]*3],
                     trisplit);
      }
    }

    if (shapes[i].mesh.texcoords.size())
      Model::triSurfaces->read(tobj, shapes[i].mesh.texcoords.size()/2, lucTexCoordData, &shapes[i].mesh.texcoords[0]);

    printf("shape[%ld].vertices: %ld\n", i, shapes[i].mesh.positions.size());
    printf("shape[%ld].texcoords: %ld\n", i, shapes[i].mesh.texcoords.size());
    printf("shape[%ld].indices: %ld\n", i, shapes[i].mesh.indices.size());
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

void LavaVu::readTecplot(FilePath& fn)
{
  //Can only parse tecplot format type FEBRICK
  //http://paulbourke.net/dataformats/tp/

  //Demo colourmap
  ColourMap* colourMap = new ColourMap();
  amodel->addColourMap(colourMap);

  //Colours: hex, abgr
  //unsigned int colours[] = {0x11224422, 0x44006600, 0xff00ff00,0xffff7733,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff};
  //0xffff00ff,0xffff0088,0xffff4444,0xffff8844,0xffffff22,0xffffffff,0xff888888};
  //colourMap->add(colours, 8);
  unsigned int colours[] = {0xff006600, 0xff00ff00,0xffff7733,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff};
  colourMap->add(colours, 7);

  //Add colour bar display
  addObject(new DrawingObject("colour-bar", "colourbar=1\n", colourMap));

  std::ifstream file(fn.full.c_str(), std::ios::in);
  if (file.is_open())
  {
    std::string line;
    bool zoneparsed = false;

    int ELS, N;
    int NTRI = 12;  //12 triangles per element
    int NLN = 12;    //12 lines per element
    float* xyz = NULL;
    int* triverts = NULL;
    float* lineverts = NULL;
    float* values = NULL;
    float* particles = NULL;

    float valuemin = HUGE_VAL;
    float valuemax = -HUGE_VAL;
    int count = 0;
    unsigned int coord = 0;
    unsigned int outcoord = 0;
    int tcount = 0;
    int lcount = 0;
    int pcount = 0;
    int timestep = -1;
    unsigned int VALUE_OFFSET = 6; //TODO: fix hard coding, this is idx of first time varying value
    DrawingObject *tobj, *lobj;
    GeomData *fixedtris, *fixedlines;
    std::vector<std::string> labels;
    while(std::getline(file, line))
    {
      //std::cerr << line << std::endl;
      if (line.find("VARIABLES=") != std::string::npos)
      {
        //Parse labels
        size_t pos = 0, pos2 = 0;
        while ((pos = line.find("\"", pos+1)) != std::string::npos)
        {
          pos2 = line.find("\"", pos+1);
          if (pos2 == std::string::npos) break;
          labels.push_back(line.substr(pos+1, pos2-pos-1));
          //std::cout << labels[labels.size()-1] << std::endl;
          pos = pos2;
        }
      }
      else if (line.find("ZONE") != std::string::npos)
      {
        //Create the timestep
        if (TimeStep::cachesize <= timestep) TimeStep::cachesize++;
        amodel->addTimeStep(timestep+1);
        amodel->setTimeStep(timestep+1);
        timestep = amodel->now;
        std::cout << "TIMESTEP " << timestep << std::endl;

        //First step, init stuff
        if (!zoneparsed)
        {
          //ZONE T="Image 1",ZONETYPE=FEBRICK, DATAPACKING=BLOCK, VARLOCATION=([1-3]=NODAL,[4-7]=CELLCENTERED), N=356680, E=44585
          zoneparsed = true;

          std::cerr << line << std::endl;
          std::stringstream ss(line);
          std::string token;
          while (ss >> token)
          {
            if (token.substr(0, 2) == "N=")
              N = atoi(token.substr(2).c_str());
            else if (token.substr(0, 2) == "E=")
              ELS = atoi(token.substr(2).c_str());
          }

          //FEBRICK
          xyz = new float[N*3];
          triverts = new int[ELS*NTRI*3]; //6 faces = 12 tris * 3
          lineverts = new float[ELS*NLN*2*3]; //12 edges
          values = new float[ELS];
          particles = new float[ELS*3];  //Value of cell at centre

          printf("N = %d, ELS = %d\n", N, ELS);

          //Add points object
          //pobj = addObject(new DrawingObject("particles", "lit=0\n", colourMap));
          //Model::points->add(pobj);
          //std::cout << values[0] << "," << valuemin << "," << valuemax << std::endl;

          //Add triangles object
          tobj = addObject(new DrawingObject("triangles", "flat=1\n", colourMap));
          Model::triSurfaces->add(tobj);

          //Add lines object
          lobj = addObject(new DrawingObject("lines", "colour=[128,128,128]\nopacity=0.5\nlit=0\n"));
          Model::lines->add(lobj);
        }
        else
        {
          //Subsequent steps

          //New timesteps contain only the changing value data...
          coord = VALUE_OFFSET; //TODO: Fix this hard coding - first zone with time varying data
          printf("READING TIMESTEP %d TRIS %d\n", timestep, ELS*NTRI);
        }
      }
      else if (line.substr(0, 4) == "TEXT")
        continue;
      else if (zoneparsed)
      {
        std::stringstream ss(line);

        //Blocks of 8 values per line
        //First blocks is X coords, Y, Z, then values
        if (coord < 3)
        {
          float value[8];
          particles[pcount*3+outcoord] = 0;

          //8 vertices per element(brick)
          int offset = count;
          for (int e=0; e<8; e++)
          {
            //std::cout << line << "[" << count << "*3+" << coord << "] = " << xyz[count*3+coord] << std::endl;
            ss >> value[e];
            xyz[count*3+outcoord] = value[e];
            particles[pcount*3+outcoord] += value[e];

            count++;

            //if (value[e] < awin->min[coord]) awin->min[coord] = value[e];
            //if (value[e] > awin->max[coord]) awin->max[coord] = value[e];
          }

          for (int i=0; i<8; i++)
            assert(!std::isnan(value[i]));

          //Two triangles per side (set indices only when x,y,z all read)
          //X=0,Y=1,Z=2
          if (coord == 2)
          {
            //Front
            triverts[(tcount*3)  ] = offset + 0;
            triverts[(tcount*3+1)] = offset + 2;
            triverts[(tcount*3+2)] = offset + 1;
            tcount++;
            triverts[(tcount*3)  ] = offset + 2;
            triverts[(tcount*3+1)] = offset + 3;
            triverts[(tcount*3+2)] = offset + 1;
            tcount++;

            //Back
            triverts[(tcount*3)  ] = offset + 4;
            triverts[(tcount*3+1)] = offset + 6;
            triverts[(tcount*3+2)] = offset + 5;
            tcount++;
            triverts[(tcount*3)  ] = offset + 6;
            triverts[(tcount*3+1)] = offset + 7;
            triverts[(tcount*3+2)] = offset + 5;
            tcount++;
            //Right
            triverts[(tcount*3)  ] = offset + 1;
            triverts[(tcount*3+1)] = offset + 3;
            triverts[(tcount*3+2)] = offset + 5;
            tcount++;
            triverts[(tcount*3)  ] = offset + 3;
            triverts[(tcount*3+1)] = offset + 7;
            triverts[(tcount*3+2)] = offset + 5;
            tcount++;
            //Left
            triverts[(tcount*3)  ] = offset + 0;
            triverts[(tcount*3+1)] = offset + 2;
            triverts[(tcount*3+2)] = offset + 4;
            tcount++;
            triverts[(tcount*3)  ] = offset + 2;
            triverts[(tcount*3+1)] = offset + 6;
            triverts[(tcount*3+2)] = offset + 4;
            tcount++;
            //Top??
            triverts[(tcount*3)  ] = offset + 2;
            triverts[(tcount*3+1)] = offset + 6;
            triverts[(tcount*3+2)] = offset + 7;
            tcount++;
            triverts[(tcount*3)  ] = offset + 7;
            triverts[(tcount*3+1)] = offset + 3;
            triverts[(tcount*3+2)] = offset + 2;
            tcount++;
            //Bottom??
            triverts[(tcount*3)  ] = offset + 0;
            triverts[(tcount*3+1)] = offset + 4;
            triverts[(tcount*3+2)] = offset + 1;
            tcount++;
            triverts[(tcount*3)  ] = offset + 4;
            triverts[(tcount*3+1)] = offset + 5;
            triverts[(tcount*3+2)] = offset + 1;
            tcount++;
            //std::cout << count << " : " << ptr[0] << "," << ptr[3] << "," << ptr[6] << std::endl;
          }

          //Edge lines
          lineverts[(lcount*2)    * 3 + outcoord] = value[0];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[1];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[1];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[3];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[3];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[2];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[2];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[0];
          lcount++;

          lineverts[(lcount*2)    * 3 + outcoord] = value[4];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[5];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[5];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[7];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[7];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[6];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[6];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[4];
          lcount++;

          lineverts[(lcount*2)    * 3 + outcoord] = value[2];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[6];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[0];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[4];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[3];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[7];
          lcount++;
          lineverts[(lcount*2)    * 3 + outcoord] = value[1];
          lineverts[(lcount*2+1)  * 3 + outcoord] = value[5];
          lcount++;

          //Average value for particle
          particles[pcount*3+outcoord] /= 8;
          pcount++;
        }
        else if (coord > 2)
        {
          //Load a scalar value (1 per line)
          float value;
          ss >> value;
          //std::cout << line << "[" << count << "*3+" << coord << "] = " << xyz[count*3+coord] << " " << value << std::endl;
          values[count] = value;
          count++;

          if (value < valuemin) valuemin = value;
          if (value > valuemax) valuemax = value;
        }
        else
        {
          count++;
        }

        //Parsed a set of data, load it
        if (count == N || (coord > 2 && count == ELS))
        {
          if (labels.size() == coord) labels.push_back("");
          std::cerr << " LOADED BLOCK: " << labels[coord] << " : COORD " << coord << " (" << outcoord << ")" << std::endl;

          //Load vertex data once all coords/indices loaded
          if (coord == 2) // || timestep > 0 && coord == VALUE_OFFSET)
          {

          }
          else if (coord > 2 && coord < VALUE_OFFSET)
          {
            unsigned int dtype;
            if (coord == 3) dtype = lucXWidthData;
            if (coord == 4) dtype = lucYHeightData;
            if (coord == 5) dtype = lucZLengthData;

            Model::triSurfaces->read(tobj, ELS, (lucGeometryDataType)dtype, values);
            printf("  VALUES min %f max %f (%s : %d)\n", valuemin, valuemax, labels[coord].c_str(), coord-3);
            Model::triSurfaces->setup(tobj, (lucGeometryDataType)dtype, valuemin, valuemax, labels[coord]);

            //Done with fixed data 
            if (coord == VALUE_OFFSET-1)
            {
              GeomData* g = Model::triSurfaces->read(tobj, N, lucVertexData, xyz);
              Model::triSurfaces->read(tobj, ELS*NTRI*3, lucIndexData, triverts);
              //Model::points->read(pobj, ELS, lucVertexData, particles);
              Model::lines->read(lobj, ELS*NLN*2, lucVertexData, lineverts);
              printf("  VERTS %d\n", N);

              //Bounds check
              for (int t=0; t<N*3; t += 3)
                g->checkPointMinMax(&xyz[t]);

              //SET FIXED DATA (clears all existing first)
              fixedtris = Model::triSurfaces->fix();
              fixedlines = Model::lines->fix();
              std::cout << " VERTS? " << fixedtris->vertices.size() << std::endl;
            }
          }
          //if (coord > 2)
          //else if (coord > 2)
          else if (coord >= VALUE_OFFSET)
          {
            //Apply fixed data to each timestep
            Model::triSurfaces->fix(fixedtris);
            Model::lines->fix(fixedlines);
            //Load other data as float values
            unsigned int dtype = lucMaxDataType + coord - VALUE_OFFSET;
            if (coord == VALUE_OFFSET) dtype = lucColourValueData;

            Model::triSurfaces->read(tobj, ELS, (lucGeometryDataType)dtype, values);
            Model::lines->read(lobj, 0, lucMinDataType, NULL); //Read a dummy value as currently won't load fixed data until other data read
            //Model::points->read(pobj, ELS, lucColourValueData, values);
            printf("  VALUES min %f max %f (%s : %d)\n", valuemin, valuemax, labels[coord].c_str(), coord-3);
            Model::triSurfaces->setup(tobj, (lucGeometryDataType)dtype, valuemin, valuemax, labels[coord]);
            //Model::points->setup(pobj, lucColourValueData, valuemin, valuemax);
          }

          valuemin = HUGE_VAL;
          valuemax = -HUGE_VAL;

          count = tcount = lcount = pcount = 0;
          coord++;
          outcoord = coord;
          if (swapY)
          {
            //Swap Y/Z
            if (coord == 2)
              outcoord = 1;
            if (coord == 1)
              outcoord = 2;
            //Also swap J/K
            if (coord == 5)
              outcoord = 4;
            if (coord == 4)
              outcoord = 5;
          }
        }
      }
    }
    file.close();

    if (xyz) delete[] xyz;
    if (values) delete[] values;
    if (triverts) delete[] triverts;
    if (lineverts) delete[] lineverts;

    //Always cache
    TimeStep::cachesize++;
    amodel->setTimeStep(0);
        timestep = amodel->now = 0;
  }
  else
    printMessage("Unable to open file: %s", fn.full.c_str());
}

void LavaVu::createDemoModel()
{
  float RANGE = 2.f;
  float min[3] = {-RANGE,-RANGE,-RANGE};
  float max[3] = {RANGE,RANGE,RANGE};
  float dims[3] = {RANGE*2.f,RANGE*2.f,RANGE*2.f};
  float size = sqrt(dotProduct(dims,dims));
  viewer->title = "Test Pattern";

  //Demo colourmap, distance from model origin
  ColourMap* colourMap = new ColourMap();
  amodel->addColourMap(colourMap);
  //Colours: hex, abgr
  unsigned int colours[] = {0xff33bb66,0xff00ff00,0xffff3333,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff,0xff000000};
  colourMap->add(colours, 8);
  colourMap->calibrate(0, size);

  //Add colour bar display
  addObject(new DrawingObject("colour-bar", "colourbar=1\n", colourMap));

  //Add points object
  DrawingObject* obj = addObject(new DrawingObject("particles", "opacity=0.75\nstatic=1\nlit=0\n", colourMap));
  int NUMPOINTS = 200000;
  int NUMSWARM = NUMPOINTS/4;
  for (int i=0; i < NUMPOINTS; i++)
  {
    float colour, ref[3];
    ref[0] = min[0] + (max[0] - min[0]) * frand;
    ref[1] = min[1] + (max[1] - min[1]) * frand;
    ref[2] = min[2] + (max[2] - min[2]) * frand;

    //Demo colourmap value: distance from model origin
    colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));

    Model::points->read(obj, 1, lucVertexData, ref);
    Model::points->read(obj, 1, lucColourValueData, &colour);

    if (i % NUMSWARM == NUMSWARM-1)
    {
      Model::points->setup(obj, lucColourValueData, 0, size, "demo colours");
      if (i != NUMPOINTS-1)
        Model::points->add(obj);
    }
  }

  //Add lines
  obj = addObject(new DrawingObject("line-segments", "static=1\nlit=0\n", colourMap));
  for (int i=0; i < 50; i++)
  {
    float colour, ref[3];
    ref[0] = min[0] + (max[0] - min[0]) * frand;
    ref[1] = min[1] + (max[1] - min[1]) * frand;
    ref[2] = min[2] + (max[2] - min[2]) * frand;

    //Demo colourmap value: distance from model origin
    colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));

    Model::lines->read(obj, 1, lucVertexData, ref);
    Model::lines->read(obj, 1, lucColourValueData, &colour);
  }

  Model::lines->setup(obj, lucColourValueData, 0, size, "demo colours");

  //Add some quads (using tri surface mode)
  {
    float verts[3][12] = {{-2,-2,0,  2,-2,0,  -2,2,0,  2,2,0},
      {-2,0,-2,  2,0,-2,  -2,0,2,  2,0,2},
      {0,-2,-2,  0,2,-2,   0,-2,2, 0,2,2}
    };
    char axischar[3] = {'Z', 'Y', 'X'};
    for (int i=0; i<3; i++)
    {
      char label[64];
      sprintf(label, "%c-cross-section", axischar[i]);
      obj = addObject(new DrawingObject(label, "opacity=0.5\nstatic=1\n"));
      obj->properties.data["colour"] = Colour_ToJson(0xff000000 | 0xff<<(8*i));
      Model::triSurfaces->read(obj, 4, lucVertexData, verts[i], 2, 2);
    }
  }

  //Set model bounds...
  //Geometry::checkPointMinMax(min);
  //Geometry::checkPointMinMax(max);
}

DrawingObject* LavaVu::addObject(DrawingObject* obj)
{
  if (!awin || awin->views.size() == 0) abort_program("No window/view defined!\n");
  if (!aview) aview = awin->views[0];

  //Add to the active viewport if not already present
  if (!aview->hasObject(obj))
    aview->addObject(obj);

  //Add to model master list
  amodel->addObject(obj);

  return obj;
}

void LavaVu::open(int width, int height)
{
  //Init geometry containers
  for (unsigned int i=0; i < Model::geometry.size(); i++)
    Model::geometry[i]->init();

  //Initialise all viewports to window size
  for (unsigned int w=0; w<windows.size(); w++)
    windows[w]->initViewports(width, height);

  reloadShaders();
}

void LavaVu::reloadShaders()
{
  //Point shaders
  if (Points::prog) delete Points::prog;
  Points::prog = new Shader("pointShader.vert", "pointShader.frag");
  const char* pUniforms[14] = {"uPointScale", "uPointType", "uOpacity", "uPointDist", "uTextured", "uTexture", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation", "uAmbient", "uDiffuse", "uSpecular"};
  Points::prog->loadUniforms(pUniforms, 14);
  const char* pAttribs[2] = {"aSize", "aPointType"};
  Points::prog->loadAttribs(pAttribs, 2);

  //Line shaders
  if (Lines::prog) delete Lines::prog;
  Lines::prog = new Shader("lineShader.vert", "lineShader.frag");
  const char* lUniforms[6] = {"uOpacity", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation"};
  Lines::prog->loadUniforms(lUniforms, 6);

  //Triangle shaders
  if (TriSurfaces::prog) delete TriSurfaces::prog;
  TriSurfaces::prog = new Shader("triShader.vert", "triShader.frag");
  const char* tUniforms[13] = {"uOpacity", "uLighting", "uTextured", "uTexture", "uCalcNormal", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation", "uAmbient", "uDiffuse", "uSpecular"};
  TriSurfaces::prog->loadUniforms(tUniforms, 13);
  QuadSurfaces::prog = TriSurfaces::prog;

  //Volume ray marching shaders
  if (Volumes::prog) delete Volumes::prog;
  Volumes::prog = new Shader("volumeShader.vert", "volumeShader.frag");
  const char* vUniforms[24] = {"uPMatrix", "uInvPMatrix", "uMVMatrix", "uNMatrix", "uVolume", "uTransferFunction", "uBBMin", "uBBMax", "uResolution", "uEnableColour", "uBrightness", "uContrast", "uSaturation", "uPower", "uViewport", "uSamples", "uDensityFactor", "uIsoValue", "uIsoColour", "uIsoSmooth", "uIsoWalls", "uFilter", "uRange", "uDenMinMax"};
  Volumes::prog->loadUniforms(vUniforms, 24);
  const char* vAttribs[1] = {"aVertexPosition"};
  Volumes::prog->loadAttribs(vAttribs, 1);
}

void LavaVu::resize(int new_width, int new_height)
{
  //On resizes after initial size set, adjust point scaling
  if (new_width > 0)
  {
    float size0 = viewer->width * viewer->height;
    if (size0 > 0)
    {
      std::ostringstream ss;
      ss << "resize " << new_width << " " << new_height;
      record(true, ss.str());
    }
  }

  amodel->redraw();
}

void LavaVu::close()
{
  for (unsigned int i=0; i < models.size(); i++)
    models[i]->close();

  //Clear models - will delete contained views, windows, objects
  //Should free all allocated memory
  for (unsigned int i=0; i < models.size(); i++)
  {
    models[i]->clearObjects(true);
    delete models[i];
  }
  models.clear();
  windows.clear();

  //Clear geometry containers
  for (unsigned int i=0; i < Model::geometry.size(); i++)
    delete Model::geometry[i];
  Model::geometry.clear();

  ColourMap::lastid = 0;
}

void LavaVu::redraw(unsigned int id)
{
  for (unsigned int i=0; i < Model::geometry.size(); i++)
    Model::geometry[i]->redrawObject(id);
}

//Called when model loaded/changed, updates all views and window settings
void LavaVu::resetViews(bool autozoom)
{
  viewset = 0;

  //Setup view(s) for new model dimensions
  for (unsigned int v=0; v < awin->views.size(); v++)
    viewSelect(v, true, autozoom);

  //Flag redraw required
  amodel->redraw();

  //Set viewer title
  std::stringstream title;
  std::string vpt = aview->properties["title"];
  if (vpt.length() > 0)
  {
    title << aview->properties["title"];
    title << " (" << awin->name << ")";
  }
  else if (awin->name.length() > 0)
    title << awin->name;
  else
    title << "LavaVu";

  if (amodel->timesteps.size() > 1)
    title << " - timestep " << std::setw(5) << std::setfill('0') << amodel->step();

  viewer->title = title.str();
  OpenGLViewer::imagecounter = 0; //Reset counter when title changes

  if (viewer->isopen && viewer->visible)  viewer->show(); //Update title etc
  viewer->setBackground(awin->background.value); //Update background colour
}

//Called when view changed
void LavaVu::viewSelect(int idx, bool setBounds, bool autozoom)
{
  //Set a default viewport/camera if none
  if (awin->views.size() == 0) 
  {
    aview = new View();
    awin->addView(aview);
    amodel->views.push_back(aview);
  }
  else
  {
    view = idx;
    if (view < 0) view = awin->views.size() - 1;
    if (view >= (int)awin->views.size()) view = 0;
  }

  aview = awin->views[view];

  //Called when timestep/window changed (new model data)
  //Set model size from geometry / bounding box and apply auto zoom
  if (setBounds)
  {
    float omin[3] = {awin->min[0], awin->min[1], awin->min[2]};
    float omax[3] = {awin->max[0], awin->max[1], awin->max[2]};

    for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->setView(aview, omin, omax);

    //Set viewport based on window size
    aview->port(viewer->width, viewer->height);

    // Apply initial autozoom if set (only applied based on provided dimensions)
    //if (autozoom && (int)aview->properties["zoomstep"] == 0)
    //   aview->init(false, awin->min, awin->max);

    //Update the model bounding box - use window bounds if provided and sane in at least 2 dimensions
    if (awin->max[0]-awin->min[0] > EPSILON && awin->max[1]-awin->min[1] > EPSILON)
    {
      debug_print("Applied Model bounds %f,%f,%f - %f,%f,%f from window\n",
                  awin->min[0], awin->min[1], awin->min[2],
                  awin->max[0], awin->max[1], awin->max[2]);
      aview->init(false, awin->min, awin->max);
    }
    else
    {
      debug_print("Applied Model bounds %f,%f,%f - %f,%f,%f from geometry\n",
                  omin[0], omin[1], omin[2],
                  omax[0], omax[1], omax[2]);
      aview->init(false, omin, omax);
    }

    //Save actual bounding box max/min/range - it is possible for the view box to be smaller
    clearMinMax(Geometry::min, Geometry::max);
    compareCoordMinMax(Geometry::min, Geometry::max, omin);
    compareCoordMinMax(Geometry::min, Geometry::max, omax);
    compareCoordMinMax(Geometry::min, Geometry::max, awin->min);
    compareCoordMinMax(Geometry::min, Geometry::max, awin->max);
    getCoordRange(Geometry::min, Geometry::max, Geometry::dims);
    debug_print("Calculated Actual bounds %f,%f,%f - %f,%f,%f \n",
                Geometry::min[0], Geometry::min[1], Geometry::min[2],
                Geometry::max[0], Geometry::max[1], Geometry::max[2]);

    // Apply step autozoom if set (applied based on detected bounding box)
    int zstep = aview->properties["zoomstep"];
    if (autozoom && zstep > 0 && amodel->step() % zstep == 0)
      aview->zoomToFit();
  }
  else
  {
    //Set view on geometry objects only, no boundary check
    for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->setView(aview);
  }
}

int LavaVu::viewFromPixel(int x, int y)
{
  // Select a viewport by mouse position, leave unchanged if pixel out of range
  for (unsigned int v=0; v<awin->views.size(); v++)
    //Viewport coords opposite to windowing system coords, flip y
    if (awin->views[v]->hasPixel(x, viewer->height - y)) return v;
  return view;
}

// Render
void LavaVu::display(void)
{
  clock_t t1 = clock();

  //Viewport reset flagged
  if (viewset > 0)
    resetViews(viewset == 2);

  //Always redraw the active view, others only if flag set
  if (aview)
  {
    if (!viewer->mouseState && sort_on_rotate && aview->rotated)
      aview->sort = true;
  }

  viewSelect(view);
  GL_Error_Check;

  //Turn filtering of objects on/off
  if (awin->views.size() > 1 || windows.size() > 1)
  {
    for (unsigned int v=0; v<awin->views.size(); v++)
      awin->views[v]->filtered = true;
  }
  else //Single viewport, always disable filter
    aview->filtered = false;

  //Scale text and 2d elements when downsampling output image
  aview->textscale = viewer->downsample > 1;
  aview->scale2d = pow(2, viewer->downsample-1);

#ifdef USE_OMEGALIB
  drawSceneBlended();
#else

  //Set viewport based on window size
  aview->port(viewer->width, viewer->height);
  GL_Error_Check;

  // Clear viewport
  glDrawBuffer(viewer->renderBuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // View transform
  //debug_print("### Displaying viewport %s at %d,%d %d x %d\n", aview->title.c_str(), (int)(viewer->width * aview->x), (int)(viewer->height * aview->y), (int)(viewer->width * aview->w), (int)(viewer->height * aview->h));
  if (aview->autozoom)
  {
    aview->projection(EYE_CENTRE);
    aview->zoomToFit();
  }
  else
    aview->apply();
  GL_Error_Check;

  if (aview->stereo)
  {
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
      if (viewer->background.value < viewer->inverse.value)
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
      if (viewer->background.value < viewer->inverse.value)
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
    // Draw scene
    drawSceneBlended();

    // Restore full-colour
    //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }
  else
  {
    // Default non-stereo render
    aview->projection(EYE_CENTRE);
    drawSceneBlended();
  }

  //Print current info message (displayed for one frame only)
  if (status) displayMessage();
#endif

  //Clear the rotation flag
  if (aview->sort) aview->rotated = false;

  //Display object list if enabled
  if (objectlist)
    displayObjectList(false);

  double time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
  if (time > 0.1)
    debug_print("%.4lf seconds to render scene\n", time);

  aview->sort = false;

#ifdef HAVE_LIBAVCODEC
  if (encoder)
  {
    viewer->pixels(encoder->buffer, false, true);
    //bitrate settings?
    encoder->frame();
  }
#endif
}

void LavaVu::drawAxis()
{
  bool doaxis = aview->properties["axis"];
  float axislen = aview->properties["axislength"];

  if (!doaxis) return;
  infostream = NULL;
  float length = axislen;
  float headsize = 8.0;   //8 x radius (r = 0.01 * length)
  float LH = length * 0.1;
  float radius = length * 0.01;
  float aspectRatio = aview->width / (float)aview->height;

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
  right = aspectRatio * top;
  glFrustum(-right, right, -top, top, nearc, farc);
  //Modelview (rotation only)
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  //Position to the bottom left
  glTranslatef(-0.3 * aspectRatio, -0.3, -1.0);
  //Apply model rotation
  aview->applyRotation();
  GL_Error_Check;
  // Switch coordinate system if applicable
  glScalef(1.0, 1.0, 1.0 * aview->orientation);

  float Xpos[3] = {length/2, 0, 0};
  float Ypos[3] = {0, length/2, 0};
  float Zpos[3] = {0, 0, length/2};

  axis->clear();
  axis->setView(aview);
  static DrawingObject* aobj = NULL;
  if (!aobj) aobj = new DrawingObject("axis", "wireframe=false\nclip=false\n");
  axis->add(aobj);

  {
    float vector[3] = {1.0, 0.0, 0.0};
    Colour colour = {255, 0, 0, 255};
    axis->drawVector(aobj, Xpos, vector, length, radius, radius, headsize, 16);
    axis->read(aobj, 1, lucRGBAData, &colour.value);
  }

  {
    float vector[3] = {0.0, 1.0, 0.0};
    Colour colour = {0, 255, 0, 255};
    axis->drawVector(aobj, Ypos, vector, length, radius, radius, headsize, 16);
    axis->read(aobj, 1, lucRGBAData, &colour.value);
  }

  if (aview->is3d)
  {
    float vector[3] = {0.0, 0.0, 1.0};
    Colour colour = {0, 0, 255, 255};
    axis->drawVector(aobj, Zpos, vector, length, radius, radius, headsize, 16);
    axis->read(aobj, 1, lucRGBAData, &colour.value);
  }
  axis->update();
  axis->draw();
  glUseProgram(0);

  //Labels
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);

  lucSetFontCharset(FONT_VECTOR);
  lucSetFontScale(length*6.0);
  PrintSetColour(viewer->inverse.value);
  Print3dBillboard(Xpos[0],    Xpos[1]-LH, Xpos[2], "X");
  Print3dBillboard(Ypos[0]-LH, Ypos[1],    Ypos[2], "Y");
  if (aview->is3d)
    Print3dBillboard(Zpos[0]-LH, Zpos[1]-LH, Zpos[2], "Z");

  glPopAttrib();

  //Restore
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  GL_Error_Check;

  //Restore info/error stream
  if (verbose && !output) infostream = stderr;
}

void LavaVu::drawRulers()
{
  if (!aview->properties["rulers"]) return;
  infostream = NULL;
  static DrawingObject* obj = NULL;
  rulers->clear();
  rulers->setView(aview);
  if (!obj) obj = new DrawingObject("rulers", "wireframe=false\nclip=false\nlit=false");
  rulers->add(obj);
  obj->properties.data["linewidth"] = (float)aview->properties["rulerwidth"];
  obj->properties.data["fontscale"] = (float)aview->properties["fontscale"] * 0.5*aview->model_size;
  obj->properties.data["font"] = "vector";
  //Colour for labels
  obj->properties.data["colour"] = Colour_ToJson(viewer->inverse);


  int ticks = aview->properties["rulerticks"];
  //Axis rulers
  float shift[3] = {0.01f/aview->scale[0] * aview->model_size,
                    0.01f/aview->scale[1] * aview->model_size,
                    0.01f/aview->scale[2] * aview->model_size
                   };
  {
    float sta[3] = {aview->min[0], aview->min[1]-shift[1], aview->max[2]};
    float end[3] = {aview->max[0], aview->min[1]-shift[1], aview->max[2]};
    drawRuler(obj, sta, end, aview->min[0], aview->max[0], ticks, 0);
  }
  {
    float sta[3] = {aview->min[0]-shift[0], aview->min[1], aview->max[2]};
    float end[3] = {aview->min[0]-shift[0], aview->max[1], aview->max[2]};
    drawRuler(obj, sta, end, aview->min[1], aview->max[1], ticks, 1);
  }
  {
    float sta[3] = {aview->min[0]-shift[0], aview->min[1]-shift[1], aview->min[2]};
    float end[3] = {aview->min[0]-shift[0], aview->min[1]-shift[1], aview->max[2]};
    drawRuler(obj, sta, end, aview->min[2], aview->max[2], ticks, 2);
  }

  rulers->update();
  rulers->draw();

  //Restore info/error stream
  if (verbose && !output) infostream = stderr;
}

void LavaVu::drawRuler(DrawingObject* obj, float start[3], float end[3], float labelmin, float labelmax, int ticks, int axis)
{
  // Draw rulers with optional tick marks
  //float fontscale = PrintSetFont(properties, "vector", 0.05*model_size*textscale);
  //float fontscale = PrintSetFont(aview->properties, "vector", 1.0, 0.08*aview->model_size);

  float vec[3];
  float length;
  vectorSubtract(vec, end, start);

  vec[0] *= aview->scale[0];
  vec[1] *= aview->scale[1];
  vec[2] *= aview->scale[2];
  start[0] *= aview->scale[0];
  start[1] *= aview->scale[1];
  start[2] *= aview->scale[2];

  // Length of the drawn vector = vector magnitude
  length = sqrt(dotProduct(vec,vec));
  if (length <= FLT_MIN) return;

  //Draw ruler line
  float pos[3] = {start[0] + vec[0] * 0.5f, start[1] + vec[1] * 0.5f, start[2] + vec[2] * 0.5f};
  rulers->drawVector(obj, pos, vec, 1.0, 0, 0, 0, 0);
  rulers->add(obj); //Add new object for ticks

  // Undo any scaling factor
  //if (aview->scale[0] != 1.0 || aview->scale[1] != 1.0 || aview->scale[2] != 1.0)
  //   glScalef(1.0/aview->scale[0], 1.0/aview->scale[1], 1.0/aview->scale[2]);

  std::string align = "";
  for (int i = 0; i < ticks; i++)
  {
    // Get tick value
    float scaledPos = i / (float)(ticks-1);
    // Calculate pixel position
    float height = -0.01 * aview->model_size;

    // Draws the tick
    if (axis == 0)
    {
      float tvec[3] = {0, height, 0};
      float tpos[3] = {start[0] + vec[0] * scaledPos, start[1] + height * 0.5f, start[2]};
      rulers->drawVector(obj, tpos, tvec, 1.0, 0, 0, 0, 0);
      align = "|"; //Centre
    }
    else if (axis == 1)
    {
      float tvec[3] = {height, 0, 0};
      float tpos[3] = {start[0] + height * 0.5f, start[1] + vec[1] * scaledPos, start[2]};
      rulers->drawVector(obj, tpos, tvec, 1.0, 0, 0, 0, 0);
      align = "^"; //Right, no vertical shift
    }
    else if (axis == 2)
    {
      float tvec[3] = {0, height, 0};
      float tpos[3] = {start[0], start[1] + height * 0.5f, start[2] + vec[2] * scaledPos};
      rulers->drawVector(obj, tpos, tvec, 1.0, 0, 0, 0, 0);
      align = "^"; //Right, no v shift
    }

    //Draw a label
    char label[16];
    float inc = (labelmax - labelmin) / (float)(ticks-1);
    sprintf(label, "%-10.3f", labelmin + i * inc);

    // Trim trailing space
    char* end = label + strlen(label) - 1;
    while(end > label && isspace(*end)) end--;
    *(end+1) = 0; //Null terminator
    strcat(label, "  "); //(Leave two spaces at end)

    GeomData* geomdata = rulers->getObjectStore(obj);
    std::string blank = "";
    geomdata->label(blank);
    std::string labelstr = align + label;
    geomdata->label(labelstr);
  }
}

void LavaVu::drawBorder()
{
  static DrawingObject* obj = NULL;
  border->clear();
  border->setView(aview);
  if (!obj) obj = new DrawingObject("border", "clip=false\n");
  obj->properties.data["colour"] = aview->properties["bordercolour"];

  //Check for boolean or numeric value for border
  float bordersize = 1.0;
  json& bord = aview->properties["border"];
  if (bord.is_number())
    bordersize = bord;
  else if (!bord)
    bordersize = 0.0;
  if (bordersize <= 0.0) return;

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

  if (aview->is3d)
  {
    // Draw model bounding box with optional filled background surface
    Vec3d minvert = Vec3d(aview->min);
    Vec3d maxvert = Vec3d(aview->max);
    Quaternion qrot;
    //Min/max swapped to draw inverted box, see through to back walls
    border->drawCuboid(obj, maxvert, minvert, qrot, true);
  }
  else
  {
    Vec3d vert1 = Vec3d(aview->max[0], aview->min[1], 0);
    Vec3d vert3 = Vec3d(aview->min[0], aview->max[1], 0);
    border->read(obj, 1, lucVertexData, aview->min, 2, 2);
    border->read(obj, 1, lucVertexData, vert1.ref());
    border->read(obj, 1, lucVertexData, vert3.ref());
    border->read(obj, 1, lucVertexData, aview->max);
  }

  border->update();
  border->draw();

  //Restore info/error stream
  if (verbose && !output) infostream = stderr;
}

GeomData* LavaVu::getGeometry(DrawingObject* obj)
{
  //Find the first available geometry container for this drawing object
  //Used to append colour values and to get a representative colour for objects
  //Only works if we know object has a single geometry type
  GeomData* geomdata = NULL;
  for (int type=lucMinType; type<lucMaxType; type++)
  {
    geomdata = Model::geometry[type]->getObjectStore(obj);
    if (geomdata) break;
  }
  return geomdata;
}

void LavaVu::displayObjectList(bool console)
{
  //Print available objects by id to screen and stderr
  if (quiet) return;
  int offset = 0;
  if (console) std::cerr << "------------------------------------------" << std::endl;
  for (unsigned int i=0; i < amodel->objects.size(); i++)
  {
    if (amodel->objects[i])
    {
      std::ostringstream ss;
      ss << "   ";
      ss << std::setw(5) << amodel->objects[i]->id << " : " << amodel->objects[i]->name;
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
        //Use object colour if provided, unless matches background
        Colour c;
        GeomData* geomdata = getGeometry(amodel->objects[i]);
        if (geomdata)
          geomdata->getColour(c, 0);
        else
          c = amodel->objects[i]->properties.getColour("colour", 0, 0, 0, 255);
        c.a = 255;
        offset++;
        displayText(ss.str(), offset);
        if (c.value != viewer->background.value)
          displayText("##", offset, &c);
      }
      else
      {
        if (console) std::cerr << "[  hidden  ]" << ss.str() << std::endl;
        Colour c;
        c.value = 0xff888888;
        displayText(ss.str(), ++offset, &c);
      }
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

void LavaVu::text(const std::string& str, int xpos, int ypos, float scale, Colour* colour)
{
  //Black on white or reverse depending on background intensity
  int avg = (viewer->background.r + viewer->background.g + viewer->background.b) / 3;
  int tcol = 0xff000000;
  int scol = 0xffffffff;
  if (avg < 127) 
  {
    tcol = 0xffffffff;
    scol = 0xff000000;
  }

  //Shadow
  PrintSetColour(scol);
  lucSetFontCharset(FONT_VECTOR);
  lucSetFontScale(scale);

  Print(xpos+1, ypos-1, str.c_str());

  //Use provided text colour or calculated
  if (colour)
    PrintSetColour(colour->value);
  else
    PrintSetColour(tcol);

  Print(xpos, ypos, str.c_str());

  //Revert to normal colour
  PrintSetColour(viewer->inverse.value);
}

void LavaVu::displayMessage()
{
  if (strlen(message))
  {
    //Set viewport to entire window
    aview->port(0, 0, viewer->width, viewer->height);
    Viewport2d(viewer->width, viewer->height);

    //Print current message
    text(message, 10, 10, 1.0);

    Viewport2d(0, 0);

    message[0] = 0;
  }
}

void LavaVu::displayText(const std::string& str, int lineno, Colour* colour)
{
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

void LavaVu::drawSceneBlended()
{
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
    //Render!
    drawScene();
    break;
  }
#ifndef USE_OMEGALIB
  aview->drawOverlay(viewer->inverse);
  drawAxis();
#endif
}

void LavaVu::drawScene()
{
  if (!aview->properties["antialias"])
    glDisable(GL_MULTISAMPLE);
  else
    glEnable(GL_MULTISAMPLE);

  // Setup default state
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glDisable(GL_TEXTURE_2D);
  glShadeModel(GL_SMOOTH);
  glPushAttrib(GL_ENABLE_BIT);

  Model::triSurfaces->draw();
  Model::quadSurfaces->draw();
  Model::points->draw();
  Model::vectors->draw();
  Model::tracers->draw();
  Model::shapes->draw();
  Model::labels->draw();
  Model::volumes->draw();
  Model::lines->draw();

#ifndef USE_OMEGALIB
  drawBorder();
#endif
  drawRulers();

  //Restore default state
  glPopAttrib();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram(0);
}

void LavaVu::loadFile(FilePath& fn)
{
  //All files on command line plus init.script added to files list
  // - gldb files represent a Model
  // - Non gldb data will be loaded into active Model
  // - If none exists, a default will be created
  // - Sequence matters! To display non-gldb data with model, load the gldb first

  //Load a file based on extension
  debug_print("Loading: %s\n", fn.full.c_str());

  //Database files always create their own Model object
  if (fn.type == "gldb" || fn.type == "db" || fn.full.find("file:") != std::string::npos)
  {
    loadModel(fn);
    //Load a window when first database loaded
    if (!amodel || !aview || !awin) loadWindow(0, startstep, true);
    //Set loaded gldb as active model/window if there was already an active window
    //if (window) loadWindow(windows.size()-1);
  }
  //Script files, can contain other files to load
  else if (fn.type == "script")
  {
    parseCommands("script " + fn.full);
    return;
  }
  else if (fn.type == "json")
  {
    parseCommands("jsonscript " + fn.full);
    return;
  }

  //Other files require an existing model
  if (!amodel) defaultModel();

  //setting prop "filestep=true" allows automatically adding timesteps before each file loaded
  if (Properties::global("filestep")) parseCommands("newstep");

  //Load other data by type
  if (fn.type == "dem")
    readHeightMap(fn);
  else if (fn.type == "obj")
    readOBJ(fn);
  else if (fn.type == "tec" || fn.type == "dat")
    readTecplot(fn);
  else if (fn.type == "raw")
    readRawVolume(fn);
  else if (fn.type == "xrw" || fn.type == "xrwu")
    readXrwVolume(fn);
  else if (fn.type == "jpg" || fn.type == "jpeg" || fn.type == "png")
  {
    if (fn.base.find("heightmap") != std::string::npos)
      readHeightMapImage(fn);
    else
      readVolumeSlice(fn);
  }
  else if (fn.type == "tiff" || fn.type == "tif")
    readVolumeTIFF(fn);

  //Reselect the active view after loading any model data (resets model bounds)
  //NOTE: sometimes we can reach this call before the GL context is created, hence the check
  if (viewer->isopen)
    viewSelect(view, true, true);
}

void LavaVu::defaultModel()
{
  //Adds a default model, window & viewport
  FilePath fm = FilePath(":memory:");
  amodel = new Model(fm);
  models.push_back(amodel);

  //Set a default window, viewport/camera
  awin = new Win();
  windows.push_back(awin);
  aview = awin->addView(new View());
  amodel->windows.push_back(awin);
  amodel->views.push_back(aview);

  //Setup default colourmaps
  //amodel->initColourMaps();
}

void LavaVu::loadModel(FilePath& fn)
{
  //Delete default model if empty
  if (models.size() == 1 and amodel->objects.size() == 0)
    close();

  //Open database file
  Model* newmodel = new Model(fn, hideall);
  if (!newmodel->open())
  {
    std::cerr << "Model database open failed\n";
    return;
  }

  amodel = newmodel;
  models.push_back(amodel);

  amodel->loadTimeSteps();
  amodel->loadColourMaps();
  amodel->loadObjects();
  amodel->loadViewports();
  amodel->loadWindows();

  //No windows?
  if (amodel->windows.size() == 0 || amodel->views.size() == 0)
  {
    //Set a default window & viewport
    if (amodel->windows.size())
      awin = amodel->windows[0];
    else
    {
      awin = new Win();
      //Add window to model window list
      amodel->windows.push_back(awin);
    }
    //Default view
    if (globalCam && aview)
      awin->addView(aview);
    else
    {
      aview = awin->addView(new View(fn.base));
      amodel->views.push_back(aview);
    }

    //Add objects to window & viewport
    for (unsigned int o=0; o<amodel->objects.size(); o++)
    {
      //Model objects stored by object ID so can have gaps...
      if (!amodel->objects[o]) continue;
      aview->addObject(amodel->objects[o]);
      amodel->loadLinks(amodel->objects[o]);
    }
  }

  //Select first window, default window name to model name
  if (!awin) awin = amodel->windows[0];
  if (awin->name.length() == 0) awin->name = amodel->file.base;

  //Add all windows to global window list
  int win = windows.size();
  for (unsigned int w=0; w<amodel->windows.size(); w++)
    windows.push_back(amodel->windows[w]);

  //Use width of first window for now
  //viewer->width = windows[0]->width;
  //viewer->height = windows[0]->height;

  //Save path of first sucessfully loaded model
  if (dbpath && viewer->output_path.length() == 0)
  {
    viewer->output_path = fn.path;
    debug_print("Output path set to %s\n", viewer->output_path.c_str());
  }

  if (viewer->isopen)
  {
    loadWindow(win, 0);
    viewer->postdisplay = true;
  }
}

//Load model window at specified timestep
bool LavaVu::loadWindow(int window_idx, int at_timestep, bool autozoom)
{
  if (!amodel) defaultModel();
  if (window_idx == window && at_timestep >= 0 && at_timestep == amodel->now) return false; //No change
  if (at_timestep >= 0)
  {
    //Cache selected step, then force new timestep set when window changes
    if (TimeStep::cachesize > 0) amodel->cacheStep();
    if (amodel->db) amodel->now = -1; //NOTE: fixed for non-db models or will set initial timestep as -1 instead of 0
  }

  //Have a database model loaded already?
  if (amodel->objects.size() > 0)
  {
    if (window_idx >= (int)windows.size()) return false;

    //Save active window as selected
    awin = windows[window_idx];
    window = window_idx;

    // Ensure correct model is selected
    for (unsigned int m=0; m < models.size(); m++)
      for (unsigned int w=0; w < models[m]->windows.size(); w++)
        if (models[m]->windows[w] == awin) amodel = models[m];

    //Reset new model data
    amodel->redraw(true);
    //if (amodel->objects.size() == 0) return false;

    //Set timestep and load geometry at that step
    if (amodel->db)
    {
      if (at_timestep < 0)
        amodel->setTimeStep();
      else
        amodel->setTimeStep(amodel->nearestTimeStep(at_timestep));
      debug_print("Loading vis '%s', timestep: %d\n", awin->name.c_str(), amodel->step());
    }
  }

  //Fixed width & height always override window settings
  if (fixedwidth > 0) windows[window_idx]->width = fixedwidth;
  if (fixedheight > 0) windows[window_idx]->height = fixedheight;

  //Not yet opened or resized?
  if (!viewer->isopen)
    //Open window at required size
    viewer->open(windows[window_idx]->width, windows[window_idx]->height);
  else
    //Resize if necessary
    viewer->setsize(awin->width, awin->height);

  //Flag a view update
  viewset = autozoom ? 2 : 1;

  return true;
}

void LavaVu::writeImages(int start, int end)
{
  writeSteps(true, start, end);
}

void LavaVu::encodeVideo(std::string filename, int fps)
{
  //TODO: - make VideoEncoder use OutputInterface
  //      - make image frame output a default video output
  //        when libavcodec not available
#ifdef HAVE_LIBAVCODEC
  if (!encoder)
  {
    if (filename.length() == 0 && awin) filename = awin->name + ".mp4";
    if (filename.length() == 0) filename = "output.mp4";
    encoder = new VideoEncoder(filename.c_str(), viewer->width, viewer->height, fps);
  }
  else
  {
    //Deleting the encoder completes the video
    delete encoder;
    encoder = NULL;
    return;
  }
#else
  std::cout << "Video output disabled, libavcodec not found!" << std::endl;
#endif
}

void LavaVu::writeSteps(bool images, int start, int end)
{
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
      viewer->display();

      if (images)
        viewer->image(viewer->title);

#ifdef HAVE_LIBAVCODEC
      //Always output to video encode if it exists
      if (encoder)
      {
        viewer->pixels(encoder->buffer, false, true);
        //bitrate settings?
        encoder->frame();
      }
#endif
    }
  }
}

void LavaVu::dumpById(unsigned int id)
{
  for (unsigned int i=0; i < amodel->objects.size(); i++)
  {
    if (amodel->objects[i] && !amodel->objects[i]->skip && (id == 0 || amodel->objects[i]->id == id))
    {
      for (int type=lucMinType; type<lucMaxType; type++)
      {
        std::ostringstream ss;
        Model::geometry[type]->dumpById(ss, amodel->objects[i]->id);

        std::string results = ss.str();
        if (results.size() > 0)
        {
          char filename[FILE_PATH_MAX];
          sprintf(filename, "%s%s_%s.%05d.csv", viewer->output_path.c_str(), amodel->objects[i]->name.c_str(),
                  GeomData::names[type].c_str(), amodel->step());
          std::ofstream csv;
          csv.open(filename, std::ios::out | std::ios::trunc);
          std::cout << " * Writing object " << amodel->objects[i]->id << " to " << filename << std::endl;
          csv << results;
          csv.close();
        }
      }
    }
  }
}

void LavaVu::jsonWriteFile(unsigned int id, bool jsonp, bool objdata)
{
  //Write new JSON format objects
  char filename[FILE_PATH_MAX];
  char ext[6];
  int step = amodel->step();
  strcpy(ext, "jsonp");
  if (!jsonp) ext[4] = '\0';
  if (step < 0) step = 0;
  if (id > 0)
    sprintf(filename, "%s%s_%s_%05d.%s", viewer->output_path.c_str(), awin->name.c_str(),
            amodel->objects[id]->name.c_str(), step, ext);
  else
    sprintf(filename, "%s%s_%05d.%s", viewer->output_path.c_str(), awin->name.c_str(), step, ext);
  std::ofstream json(filename);
  if (jsonp) json << "loadData(\n";
  jsonWrite(json, id, objdata);
  if (jsonp) json << ");\n";
  json.close();
}

void LavaVu::jsonWrite(std::ostream& os, unsigned int id, bool objdata)
{
  //Write new JSON format objects
  // - globals are all stored on / sourced from Properties::globals
  // - views[] list holds view properies (previously single instance in "options")
  json exported;
  json properties = Properties::globals;
  json colourmaps = json::array();
  json objects = json::array();
  json views = json::array();

  //TODO: convert this to a global prop too and store as rgba() string
  properties["background"] = Colour_ToJson(awin->background);

  for (unsigned int v=0; v < awin->views.size(); v++)
  {
    View* view = awin->views[v];
    json& vprops = view->properties.data;

    float rotate[4], translate[3], focus[3];
    view->getCamera(rotate, translate, focus);
    json rot, trans, foc, scale, min, max;
    for (int i=0; i<4; i++)
    {
      rot.push_back(rotate[i]);
      if (i>2) break;
      trans.push_back(translate[i]);
      foc.push_back(focus[i]);
      scale.push_back(aview->scale[i]);
      if (aview->min[i] < HUGE_VAL && aview->max[i] > -HUGE_VAL)
      {
        min.push_back(aview->min[i]);
        max.push_back(aview->max[i]);
      }
    }

    vprops["rotate"] = rot;
    vprops["translate"] = trans;
    vprops["focus"] = foc;
    vprops["scale"] = scale;
    vprops["min"] = min;
    vprops["max"] = max;

    vprops["near"] = view->near_clip;
    vprops["far"] = view->far_clip;
    vprops["orientation"] = view->orientation;

    //Add the view
    views.push_back(vprops);
  }

  for (unsigned int i = 0; i < amodel->colourMaps.size(); i++)
  {
    json cmap;
    json colours;

    for (unsigned int c=0; c < amodel->colourMaps[i]->colours.size(); c++)
    {
      json colour;
      std::stringstream cs;
      Colour& col = amodel->colourMaps[i]->colours[c].colour;
      colour["position"] = amodel->colourMaps[i]->colours[c].position;
      cs << "rgba(" << (int)col.r << "," << (int)col.g << "," << (int)col.b << "," << (col.a/255.0) << ")";
      colour["colour"] = cs.str();
      colours.push_back(colour);
    }

    cmap["name"] = amodel->colourMaps[i]->name;
    cmap["minimum"] = amodel->colourMaps[i]->minimum;
    cmap["maximum"] = amodel->colourMaps[i]->maximum;
    cmap["log"] = amodel->colourMaps[i]->log;
    cmap["colours"] = colours;

    colourmaps.push_back(cmap);
  }

  if (!viewer->visible) aview->filtered = false; //Disable viewport filtering
  for (unsigned int i=0; i < amodel->objects.size(); i++)
  {
    if (amodel->objects[i] && (id == 0 || amodel->objects[i]->id == id))
    {
      //Only able to dump point/triangle based objects currently:
      //TODO: fix to use sub-renderer output for others
      //"Labels", "Points", "Grid", "Triangles", "Vectors", "Tracers", "Lines", "Shapes", "Volumes"
      std::string names[] = {"", "points", "triangles", "triangles", "triangles", "triangles", "triangles", "lines", "triangles", "volume"};

      //Find colourmap
      int colourmap = -1;
      ColourMap* cmap = amodel->objects[i]->colourMaps[lucColourValueData];
      if (cmap)
      {
        //Search vector to find index of selected map
        std::vector<ColourMap*>::iterator it = find(amodel->colourMaps.begin(), amodel->colourMaps.end(), cmap);
        if (it != amodel->colourMaps.end())
          colourmap = (it - amodel->colourMaps.begin());
      }

      json obj = amodel->objects[i]->properties.data;
      obj["name"] = amodel->objects[i]->name;
      if (colourmap >= 0) obj["colourmap"] = colourmap;
      if (!objdata)
      {
        if (Model::points->getVertexCount(amodel->objects[i]) > 0) obj["points"] = true;
        if (Model::quadSurfaces->getVertexCount(amodel->objects[i]) > 0 ||
            Model::triSurfaces->getVertexCount(amodel->objects[i]) > 0 ||
            Model::vectors->getVertexCount(amodel->objects[i]) > 0 ||
            Model::tracers->getVertexCount(amodel->objects[i]) > 0 ||
            Model::shapes->getVertexCount(amodel->objects[i]) > 0) obj["triangles"] = true;
        if (Model::lines->getVertexCount(amodel->objects[i]) > 0) obj["lines"] = true;
        if (Model::volumes->getVertexCount(amodel->objects[i]) > 0) obj["volumes"] = true;

        //std::cout << "HAS OBJ TYPES: (point,tri,vol)" << obj.getBool("points", false) << "," 
        //          << obj.getBool("triangles", false) << "," << obj.getBool("volumes", false) << std::endl;
        objects.push_back(obj);
        continue;
      }

      for (int type=lucMinType; type<lucMaxType; type++)
      {
        //Collect vertex/normal/index/value data
        //When extracting data, skip objects with no data returned...
        //if (!Model::geometry[type]) continue;
        Model::geometry[type]->jsonWrite(amodel->objects[i]->id, obj);
      }

      //Save object if contains data
      if (obj["points"].size() > 0 ||
          obj["triangles"].size() > 0 ||
          obj["lines"].size() > 0 ||
          obj["volumes"].size() > 0)
      {
        objects.push_back(obj);
      }
    }
  }

  exported["properties"] = properties;
  exported["views"] = views;
  exported["colourmaps"] = colourmaps;
  exported["objects"] = objects;

  //Export with indentation
  os << std::setw(2) << exported;
  std::cout << std::setw(2) << exported << std::endl;
}

void LavaVu::jsonRead(std::string data)
{
  json imported = json::parse(data);
  Properties::globals = imported["properties"];
  json views;
  //If "options" exists (old format) read it as first view properties
  if (imported.count("options") > 0)
    views.push_back(imported["options"]);
  else
    views = imported["views"];

  //TODO: get rid of this & store/get from property directly instead of Win class
  awin->background = Colour_FromJson(Properties::global("background"));

  // Import views
  for (unsigned int v=0; v < views.size(); v++)
  {
    if (v >= awin->views.size())
    {
      //TODO:
      //Insert a view based on previous in list (same objects)
      //Apply new properties to that
      //(This will enable adding views in the web interface)
      break;
    }
    View* view = awin->views[v];

    //Apply base properties with merge
    view->properties.merge(views[v]);

    //TODO: Fix view to use all these properties directly
    json rot, trans, foc, scale, min, max;
    rot = view->properties["rotate"];
    trans = view->properties["translate"];
    foc = view->properties["focus"];
    scale = view->properties["scale"];
    //min = aview->properties["min"];
    //max = aview->properties["max"];
    view->setRotation(rot[0], rot[1], rot[2], rot[3]);
    view->setTranslation(trans[0], trans[1], trans[2]);
    view->focus(foc[0], foc[1], foc[2]);
    view->setScale(scale[0], scale[1], scale[2]);
    //view->init(false, newmin, newmax);
    view->near_clip = view->properties["near"];
    view->far_clip = view->properties["far"];
    view->orientation = view->properties["orientation"];
  }

  // Import colourmaps
  json colourmaps = imported["colourmaps"];
  for (unsigned int i=0; i < colourmaps.size(); i++)
  {
    if (i >= amodel->colourMaps.size())
      amodel->addColourMap();

    json cmap = colourmaps[i];
    if (cmap["minimum"].is_number()) amodel->colourMaps[i]->minimum = cmap["minimum"];
    if (cmap["maximum"].is_number()) amodel->colourMaps[i]->maximum = cmap["maximum"];
    if (cmap["log"].is_boolean()) amodel->colourMaps[i]->log = cmap["log"];
    json colours = cmap["colours"];
    //Replace colours
    amodel->colourMaps[i]->colours.clear();
    for (unsigned int c=0; c < colours.size(); c++)
    {
      json colour = colours[c];
      Colour newcolour = Colour_FromJson(colour["colour"]);
      amodel->colourMaps[i]->addAt(newcolour, colour["position"]);
    }
  }

  //Import objects
  json objects = imported["objects"];
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (i >= amodel->objects.size())
    {
      std::string name = objects[i]["name"];
      addObject(new DrawingObject(name));
    }
    
    //Merge properties
    amodel->objects[i]->properties.merge(objects[i]);

    //Colourmap selection?
    if (objects[i].count("colourmap") > 0)
      amodel->objects[i]->colourMaps[lucColourValueData] = amodel->colourMaps[objects[i]["colourmap"]];
  }
}

//Data request from attached apps
std::string LavaVu::requestData(std::string key)
{
  std::ostringstream result;
  if (key == "objects")
  {
    jsonWrite(result);
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
