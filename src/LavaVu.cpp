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
//Value data types independent from geometry types?
//Merge WebGL/standard shader code

//Viewer class
#include "Include.h"
#include "LavaVu.h"
#include "Shaders.h"
#include "tiny_obj_loader.h"
#include "Util.h"
#include "Server.h"
#include "OpenGLViewer.h"
#include "Main/SDLViewer.h"
#include "Main/X11Viewer.h"
#include "Main/GlutViewer.h"
#include "Main/CGLViewer.h"
#include "Main/CocoaViewer.h"

//Viewer class implementation...
LavaVu::LavaVu(std::string binpath) : binpath(binpath)
{
  viewer = NULL;
  axis = NULL;
  border = NULL;
  rulers = NULL;
  encoder = NULL;
  verbose = dbpath = false;

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
#if defined HAVE_CGL
  if (!viewer) viewer = new CocoaViewer();
  if (!viewer) viewer = new CGLViewer();
#endif
#if defined HAVE_GLCONTEXT
  //Assumes an OpenGL context is already provided
  if (!viewer) viewer = new OpenGLViewer();
#endif
  if (!viewer) abort_program("No windowing system configured (requires X11, GLUT, SDL or Cocoa/CGL)");

  viewer->app = (ApplicationInterface*)this;

  //Shader path (default to program path if not set)
  if (binpath.back() != '/') binpath += "/";
  if (Shader::path.length() == 0) Shader::path = binpath;
  //Set default web server path
  Server::htmlpath = binpath + "html";

  //Add default console input attachment to viewer
#ifndef USE_OMEGALIB
  static StdInput stdi;
  viewer->addInput(&stdi);
#endif
}

void LavaVu::defaults()
{
  //Reset state
  drawstate.reset();

  //Set all values/state to defaults
  if (viewer) 
  {
    //Now the app init phase can be repeated, need to reset all data
    //viewer->visible = true;
    viewer->quitProgram = false;
    close();
  }

  //Clear any queued commands
  viewer->commands.clear();

  axis = new TriSurfaces(drawstate);
  rulers = new Lines(drawstate);
  border = new QuadSurfaces(drawstate);

  initfigure = 0;
  viewset = 0;
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
#ifdef USE_OMEGALIB
  drawstate.globals["sort"] = 0;
#endif
  message[0] = '\0';
  volume = NULL;

  tracersteps = 0;
  objectlist = false;

  //Interaction command prompt
  entry = "";

  loop = false;
  animate = 0;
  repeat = 0;
  encoder = NULL;
}

LavaVu::~LavaVu()
{
  close();

  Server::Delete();

#ifdef HAVE_LIBAVCODEC
  if (encoder) delete encoder;
#endif
  debug_print("LavaVu closing: peak geometry memory usage: %.3f mb\n", mempeak__/1000000.0f);
  if (viewer) delete viewer;
}

void LavaVu::arguments(std::vector<std::string> args)
{
  //Read command line switches
  for (unsigned int i=0; i<args.size(); i++)
  {
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
      //Unused switches: bklosu, BDEFHKLMOPUXYZ
      switch (x)
      {
      case 'a':
        //Automation mode:
        //return from run() immediately after loading
        //All actions to be performed by subsequent 
        //library calls
        drawstate.automate = true;
        break;
      case 'f':
        ss >> initfigure;
        break;
      case 'z':
        //Downsample images
        ss >> vars[0];
        viewer->downSample(vars[0]);
        break;
      case 'p':
        //Web server enable
        ss >> Server::port;
        break;
      case 'q':
        //Web server JPEG quality
        ss >> Server::quality;
        if (Server::quality > 0) Server::render = true;
        break;
      case 'n':
        //Web server threads
        ss >> Server::threads;
        break;
      case 'r':
        ss >> vars[0] >> x >> vars[1];
        drawstate.globals["resolution"] = json::array({vars[0], vars[1]});
        break;
      case 'N':
        drawstate.globals["noload"] = true;
        break;
      case 'A':
        drawstate.globals["hideall"] = true;
        break;
      case 'v':
        parseCommands("verbose on");
        break;
      case 'x':
        ss >> viewer->outwidth >> x >> viewer->outheight;
        break;
      case 'c':
        ss >> vars[0];
        if (vars[0]) drawstate.globals["cache"] = true;
        break;
      case 't':
        //Use alpha channel in png output
        drawstate.globals["pngalpha"] = true;
        break;
      case 'y':
        //Swap y & z axis on import
        drawstate.globals["swapyz"] = true;
        break;
      case 'T':
        //Split triangles
        ss >> vars[0];
        drawstate.globals["trisplit"] = vars[0];
        break;
      case 'C':
        //Global camera
        drawstate.globals["globalcam"] = true;
        break;
      case 'V':
        {
          float res[3];
          ss >> res[0] >> x >> res[1] >> x >> res[2];
          drawstate.globals["volres"] = {res[0], res[1], res[2]};
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
        drawstate.globals["filestep"] = true;
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
  {
    viewer->visible = false;
    Server::port = 0;
  }

  //Set default timestep if none specified
  if (startstep < 0) startstep = 0;
  //drawstate.now = startstep;

  //Add default script
  if (defaultScript.length())
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

  //Add server attachments to the viewer
  if (Server::port > 0)
    viewer->addOutput(Server::Instance(viewer));

  //If server running, always stay open (persist flag)
  bool persist = Server::running();

  //Require a model from here on, set a default
  if (!amodel) defaultModel();

  if (writeimage || writemovie || dump > lucExportNone)
  {
    persist = false; //Disable persist
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
            std::cout << "... Writing image(s) for model/figure " << drawstate.global("caption")
                      << " Timesteps: " << startstep << " to " << endstep << std::endl;
          }
          if (writemovie)
          {
            std::cout << "... Writing movie for model/figure " << drawstate.global("caption")
                      << " Timesteps: " << startstep << " to " << endstep << std::endl;
            //Other formats?? avi/mpeg4?
            encodeVideo("", writemovie);
          }

          //Write output
          writeSteps(writeimage, startstep, endstep);

          if (initfigure != 0) break;
        }
      }

      //Export data
      amodel->triSurfaces->loadMesh();  //Optimise triangle meshes before export
      DrawingObject* obj = NULL;
      if (dumpid > 0) obj = amodel->findObject(dumpid);
      exportData(dump, obj);
    }
  }
  else
  {
    //Cache data if enabled
    if (drawstate.global("cache")) //>= amodel->timesteps.size())
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
  if (drawstate.automate) return;

  //Start event loop
  if (viewer->visible)
    viewer->execute();
  else if (persist)
    viewer->OpenGLViewer::execute();
  else
  {
    //Read input script from stdin on first timestep
    viewer->pollInput();
    viewer->display();
  }
}

void LavaVu::clearData(bool objects)
{
  //Clear data
  if (amodel) amodel->clearObjects(true);
  //Delete objects? only works for active view/model
  if (objects)
  {
    if (aview) aview->objects.clear();
    if (amodel) amodel->objects.clear();
  }
  aobject = NULL;
}

void LavaVu::exportData(lucExportType type, DrawingObject* obj)
{
  //Export data
  viewer->display();
  if (type == lucExportJSON)
    jsonWriteFile(obj);
  else if (type == lucExportJSONP)
    jsonWriteFile(obj, true);
  else if (type == lucExportGLDB)
    amodel->writeDatabase("exported.gldb", obj);
  else if (type == lucExportGLDBZ)
    amodel->writeDatabase("exported.gldb", obj, true);
  else if (type == lucExportCSV)
    dumpCSV(obj);
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
    if (verbose) std::cerr << "OBJECT " << std::setw(2) << aobject->name() << ", DATA: " << aobject->properties.data << std::endl;
  }
  else if (aview && aview->properties.data.count(key) > 0)
  {
    aview->properties.parse(data);
    if (verbose) std::cerr << "VIEW: " << std::setw(2) << aview->properties.data << std::endl;
    viewset = 2; //Force check for resize and autozoom
  }
  else
  {
    //Properties not found on view are set globally
    Properties temp(drawstate.globals, drawstate.defaults);
    temp.parse(data, true);
    if (verbose) std::cerr << "GLOBAL: " << std::setw(2) << drawstate.globals << std::endl;
    viewset = 2; //Force check for resize and autozoom
  }
}

void LavaVu::printProperties()
{
  //Show properties of selected object or view/globals
  if (aobject)
    std::cerr << "OBJECT " << aobject->name() << ", DATA: " << std::setw(2) << aobject->properties.data << std::endl;
  else
  {
    std::cerr << "VIEW: " << std::setw(2) << aview->properties.data << std::endl;
    std::cerr << "GLOBAL: " << std::setw(2) << drawstate.globals << std::endl;
  }
}

void LavaVu::printDefaultProperties()
{
  //Show default properties
  std::cerr << std::setw(2) << drawstate.defaults << std::endl;
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

  float volmin[3], volmax[3], volres[3];
  Properties::toFloatArray(drawstate.global("volmin"), volmin, 3);
  Properties::toFloatArray(drawstate.global("volmax"), volmax, 3);
  Properties::toFloatArray(drawstate.global("volres"), volres, 3);

  readVolumeCube(fn, (GLubyte*)buffer.data(), volres[0], volres[1], volres[2], volmin, volmax);
}

void LavaVu::readXrwVolume(const FilePath& fn)
{
  //Xrw format volume data
  std::vector<char> buffer;
  unsigned int bytes = 0;
  float volmin[3], volmax[3];
  int volres[3];
#ifdef USE_ZLIB
  if (fn.type != "xrwu")
  {
    gzFile f = gzopen(fn.full.c_str(), "rb");
    gzread(f, (char*)volres, sizeof(int)*3);
    gzread(f, (char*)volmax, sizeof(float)*3);
    volmin[0] = volmin[1] = volmin[2] = 0;
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
    file.read((char*)volmax, sizeof(float)*3);
    volmin[0] = volmin[1] = volmin[2] = 0;
    bytes -= sizeof(int)*3 + sizeof(float)*3;
    if (!file.is_open() || bytes <= 0) abort_program("File error %s\n", fn.full.c_str());
    buffer.resize(bytes);
    file.read(&buffer[0], bytes);
    file.close();
  }

  float inscale[3];
  Properties::toFloatArray(drawstate.global("inscale"), inscale, 3);

  //Scale geometry by input scaling factor
  for (int i=0; i<3; i++)
  {
    volmin[i] *= inscale[i];
    volmax[i] *= inscale[i];
    if (verbose) std::cerr << i << " " << inscale[i] << " : MIN " << volmin[i] << " MAX " << volmax[i] << std::endl;
  }

  readVolumeCube(fn, (GLubyte*)buffer.data(), volres[0], volres[1], volres[2], volmin, volmax);
}

void LavaVu::readVolumeCube(const FilePath& fn, GLubyte* data, int width, int height, int depth, float min[2], float max[3], int channels)
{
  //Loads full volume, optionally as slices
  bool splitslices = drawstate.global("slicevolumes");
  bool dumpslices = drawstate.global("slicedump");
  if (splitslices || dumpslices)
  {
    //Slicing is slower but allows sub-sampling and cropping
    GLubyte* ptr = data;
    int slicesize = width * height;
    json volss = drawstate.global("volsubsample");
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
    if (!vobj) vobj = new DrawingObject(drawstate, fn.base);
    addObject(vobj);

    //Define the bounding cube by corners
    amodel->volumes->add(vobj);
    amodel->volumes->read(vobj, 1, lucVertexData, min);
    amodel->volumes->read(vobj, 1, lucVertexData, max);

    //Load full cube
    int bytes = channels * width * height * depth;
    printf("Loading %u bytes, res %d %d %d\n", bytes, width, height, depth);
    amodel->volumes->read(vobj, bytes, lucLuminanceData, data, width, height, depth);
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

  ImageFile image(fn);
  if (image.pixels)
  {
    readVolumeSlice(fn.base, image.pixels, image.width, image.height, image.channels);
  }
  else
    debug_print("Slice load failed: %s\n", fn.full.c_str());
}

void LavaVu::readVolumeSlice(const std::string& name, GLubyte* imageData, int width, int height, int channels)
{
  //Create volume object, or if static volume object exists, use it
  int outChannels = drawstate.global("volchannels");
  static int count = 0;
  DrawingObject *vobj = volume;
  json volss = drawstate.global("volsubsample");
  if (!vobj)
  {
    count = 0;
    float volmin[3], volmax[3], volres[3], inscale[3];
    Properties::toFloatArray(drawstate.global("volmin"), volmin, 3);
    Properties::toFloatArray(drawstate.global("volmax"), volmax, 3);
    Properties::toFloatArray(drawstate.global("volres"), volres, 3);
    Properties::toFloatArray(drawstate.global("inscale"), inscale, 3);
    vobj = addObject(new DrawingObject(drawstate, name, "static=1"));
    //Scale geometry by input scaling factor
    for (int i=0; i<3; i++)
    {
      volmin[i] *= inscale[i];
      volmax[i] *= inscale[i];
      if (verbose) std::cerr << i << " " << inscale[i] << " : MIN " << volmin[i] << " MAX " << volmax[i] << std::endl;
    }
    //Define the bounding cube by corners
    amodel->volumes->add(vobj);
    amodel->volumes->read(vobj, 1, lucVertexData, volmin);
    amodel->volumes->read(vobj, 1, lucVertexData, volmax);
  }
  else
    amodel->volumes->add(vobj);

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
    GLubyte* luminance = new GLubyte[w*h];
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
        luminance[offset++] = byte;
      }
    }

    //Now using a container designed for byte data
    amodel->volumes->read(vobj, w * h, lucLuminanceData, luminance, w, h);
    delete[] luminance;
  }
  else if (outChannels == 3)
  {
    //Already in the correct format/layout with no sub-sampling, load directly
    if (channels == 3 && w == width && h == height)
      amodel->volumes->read(vobj, width*height*3, lucRGBData, imageData, width, height);
    else
    {
      //Convert LUM/RGBA to RGB
      GLubyte* rgb = new GLubyte[w*h*3];
      int count = 0;
      for (int y=0; y<height; y+=hstep)
      {
        for (int x=0; x<width; x+=wstep)
        {
          assert(offset+2 < w*h*3);
          if (channels == 1)
          {
            rgb[offset++] = imageData[y*width+x];
            rgb[offset++] = imageData[y*width+x];
            rgb[offset++] = imageData[y*width+x];
          }
          else if (channels >= 3)
          {
            rgb[offset++] = imageData[(y*width+x)*channels];
            rgb[offset++] = imageData[(y*width+x)*channels+1];
            rgb[offset++] = imageData[(y*width+x)*channels+2];
          }
        }
      }
      amodel->volumes->read(vobj, w*h*3, lucRGBData, rgb, w, h);
      delete[] rgb;
    }
  }
  else
  {
    //Already in the correct format/layout with no sub-sampling, load directly
    if (channels == 4 && w == width && h == height)
      amodel->volumes->read(vobj, width*height, lucRGBAData, imageData, width, height);
    else
    {
      //Convert LUM/RGB to RGBA
      GLubyte* rgba = new GLubyte[w*h*4];
      int count = 0;
      for (int y=0; y<height; y+=hstep)
      {
        for (int x=0; x<width; x+=wstep)
        {
          assert(offset+3 < w*h*4);
          if (channels == 1)
          {
            rgba[offset++] = imageData[y*width+x];
            rgba[offset++] = imageData[y*width+x];
            rgba[offset++] = imageData[y*width+x];
            rgba[offset++] = 255;
          }
          else if (channels >= 3)
          {
            rgba[offset++] = imageData[(y*width+x)*channels];
            rgba[offset++] = imageData[(y*width+x)*channels+1];
            rgba[offset++] = imageData[(y*width+x)*channels+2];
            //if (x%16==0&&y%16==0) printf("RGBA %d %d %d\n", rgba[offset-3], rgba[offset-2], rgba[offset-1]);
            if (channels == 4)
              rgba[offset++] = imageData[(y*width+x)*channels+3];
            else
              rgba[offset++] = 255;
          }
        }
      }
      amodel->volumes->read(vobj, w*h, lucRGBAData, rgba, w, h);
      delete[] rgba;
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
      json volss = drawstate.global("volsubsample");
      int d = TIFFNumberOfDirectories(tif);
      int ds = volss[2];
      if (d > 1) std::cout << "TIFF contains " << d << " pages, sub-sampling z " << ds << std::endl;
      do
      {
        if (TIFFReadRGBAImage(tif, width, height, (uint32*)imageData, 0))
        {
          //Subsample
          if (count % ds != 0) {count++; continue;}
          RawImageFlip(imageData, width, height, channels);
          readVolumeSlice(fn.base, imageData, width, height, channels);
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

void LavaVu::createDemoVolume()
{
  //Create volume object, or if static volume object exists, use it
  DrawingObject *vobj = volume;
  if (!vobj)
  {
    float volmin[3], volmax[3], volres[3], inscale[3];
    Properties::toFloatArray(drawstate.global("volmin"), volmin, 3);
    Properties::toFloatArray(drawstate.global("volmax"), volmax, 3);
    Properties::toFloatArray(drawstate.global("volres"), volres, 3);
    Properties::toFloatArray(drawstate.global("inscale"), inscale, 3);
    vobj = new DrawingObject(drawstate, "volume", "density=50\n");
    addObject(vobj);
    //Scale geometry by input scaling factor
    for (int i=0; i<3; i++)
    {
      volmin[i] *= inscale[i];
      volmax[i] *= inscale[i];
      if (verbose) std::cerr << i << " " << inscale[i] << " : MIN " << volmin[i] << " MAX " << volmax[i] << std::endl;
    }
    //Define the bounding cube by corners
    amodel->volumes->read(vobj, 1, lucVertexData, volmin);
    amodel->volumes->read(vobj, 1, lucVertexData, volmax);
  }

  unsigned int width = 256, height = 256, depth = 256, block = 24;
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

      if (z > 0) amodel->volumes->add(vobj);
      amodel->volumes->read(vobj, width * height, lucRGBAData, imageData, width, height);
      std::cout << "SLICE LOAD " << z << " : " << width << "," << height << " channels: " << channels << std::endl;
    }
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
  Properties::toFloatArray(aview->properties["min"], min, 3);
  Properties::toFloatArray(aview->properties["max"], max, 3);
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
  obj = addObject(new DrawingObject(drawstate, fn.base, props));
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
      amodel->geometry[geomtype]->read(obj, 1, lucVertexData, vertex.ref(), gridx, gridz);
      //Colour by height
      amodel->geometry[geomtype]->read(obj, 1, &colourval, "height");

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
  std::string props = "static=1\ncullface=0\ntexturefile=" + texfile + "\n";
  obj = addObject(new DrawingObject(drawstate, fn.base, props));

  //Default colourmap
  obj->properties.data["colourmap"] = colourMap("elevation", "darkgreen yellow brown");

  Vec3d vertex;

  //Use red channel as luminance for now
  for (int z=0; z<image.height; z++)
  {
    vertex[2] = z;
    for (int x=0; x<image.width; x++)
    {
      vertex[0] = x;
      vertex[1] = heightrange * image.pixels[(z*image.width+x)*image.channels] / 255.0;

      float colourval = vertex[1];

      if (vertex[1] < min[1]) min[1] = vertex[1];
      if (vertex[1] > max[1]) max[1] = vertex[1];

      //Add grid point
      amodel->geometry[geomtype]->read(obj, 1, lucVertexData, vertex.ref(), image.width, image.height);
      //Colour by height
      amodel->geometry[geomtype]->read(obj, 1, &colourval, "height");
    }
  }
}

void LavaVu::addTriangles(DrawingObject* obj, float* a, float* b, float* c, int level)
{
  level--;
  bool swapY = drawstate.global("swapyz");
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
    amodel->triSurfaces->read(obj, 1, lucVertexData, a);
    amodel->triSurfaces->read(obj, 1, lucVertexData, b);
    amodel->triSurfaces->read(obj, 1, lucVertexData, c);
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

void LavaVu::readOBJ(const FilePath& fn)
{
  //Use tiny_obj_loader to load a model
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fn.full.c_str(), NULL);
  if (!err.empty())
  {
    std::cerr << "Error loading OBJ file: " << fn.full << " - " << err << std::endl;
    return;
  }

  std::cout << "# of shapes    : " << shapes.size() << std::endl;
  std::cout << "# of materials : " << materials.size() << std::endl;
  std::cout << "# of vertices : " << attrib.vertices.size() << std::endl;
  std::cout << "# of normals : " << attrib.normals.size() << std::endl;
  std::cout << "# of texcoords : " << attrib.texcoords.size() << std::endl;

  //Add single drawing object per file, if one is already active append to it
  DrawingObject* tobj = aobject;
  if (!tobj) tobj = addObject(new DrawingObject(drawstate, fn.base, "static=1\ncolour=[128,128,128]\n"));

  for (size_t i = 0; i < shapes.size(); i++)
  {
    //Strip path from name
    size_t last_slash = shapes[i].name.find_last_of("\\/");
    if (std::string::npos != last_slash)
      shapes[i].name = shapes[i].name.substr(last_slash + 1);

    printf("shape[%ld].name = %s\n", i, shapes[i].name.c_str());
    printf("Size of shape[%ld].material_ids: %ld\n", i, shapes[i].mesh.material_ids.size());

    //Add new triangles data store to object
    amodel->triSurfaces->add(tobj);

    if (shapes[i].mesh.material_ids.size() > 0)
    {
      //Just take the first id...
      int id = shapes[i].mesh.material_ids[0];
      if (id >= 0)
      {
        //Use the diffuse property as the colour/texture
        if (materials[id].diffuse_texname.length() > 0)
        {
          std::cerr << "Applying diffuse texture: " << materials[id].diffuse_texname << std::endl;
          std::string texpath = materials[id].diffuse_texname;
          if (fn.path.length() > 0)
            texpath = fn.path + "/" + texpath;

          //Add per-object texture
          amodel->triSurfaces->setTexture(tobj, tobj->addTexture(texpath));
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
          amodel->triSurfaces->read(tobj, 1, lucRGBAData, &c.value);
        }
      }
    }

    //Default is to load the indexed triangles and provided normals
    //Can be overridden by setting trisplit (-T#) (but will break textures)
    //Setting to 1 will calculate our own normals and optimise mesh
    //Setting > 1 also divides triangles into smaller pieces first
    int trisplit = drawstate.global("trisplit");
    printf("Loading: shape[%ld].indices: %ld\n", i, shapes[i].mesh.indices.size());
    if (trisplit == 0 || attrib.texcoords.size())
    {
#if 1
      int voffset = 0;
      for (size_t f=0; f < shapes[i].mesh.indices.size(); f+=3)
      {
        tinyobj::index_t ids[3] = {shapes[i].mesh.indices[f], shapes[i].mesh.indices[f+1], shapes[i].mesh.indices[f+2]};
        for (int c=0; c<3; c++)
        {
          amodel->triSurfaces->read(tobj, 1, lucVertexData, &attrib.vertices[3*ids[c].vertex_index]);
          amodel->triSurfaces->read(tobj, 1, lucIndexData, &voffset);
          voffset++;
          if (attrib.texcoords.size())
            amodel->triSurfaces->read(tobj, 1, lucTexCoordData, &attrib.texcoords[2*ids[c].texcoord_index]);
          if (attrib.normals.size())
          {
            //Some files skip the normal index, so assume it is the same as vertex index
            int nidx = ids[c].normal_index;
            if (nidx < 0) nidx = ids[c].vertex_index;
            amodel->triSurfaces->read(tobj, 1, lucNormalData, &attrib.normals[3*nidx]);
          }
        }
      }
#else
      int v0 = attrib.vertices.size();
      int v1 = 0;
      int n0 = attrib.normals.size();
      int n1 = 0;
      int t0 = attrib.texcoords.size();
      int t1 = 0;
      for (size_t f=0; f < shapes[i].mesh.indices.size(); f+=3)
      {
        tinyobj::index_t ids[3] = {shapes[i].mesh.indices[f], shapes[i].mesh.indices[f+1], shapes[i].mesh.indices[f+2]};
        int vidx = 3*shapes[i].mesh.indices[f].vertex_index;
        if (vidx < v0) v0 = vidx;
        if (vidx > v1) v1 = vidx;
          if (attrib.texcoords.size())
          {
            int tidx = 2*shapes[i].mesh.indices[f].texcoord_index;
            if (tidx < 0) tidx = 2*shapes[i].mesh.indices[f].vertex_index;
            if (tidx < t0) t0 = tidx;
            if (tidx > t1) t1 = tidx;
          }
          if (attrib.normals.size())
          {
            int nidx = 3*shapes[i].mesh.indices[f].normal_index;
            //Some files skip the normal index, so assume it is the same as vertex index
            if (nidx < 0) nidx = vidx;
            if (nidx < v0) n0 = nidx;
            if (nidx > v1) n1 = nidx;
          }
      }
      GeomData* g = Model::triSurfaces->read(tobj, (v1-v0+1)/3, lucVertexData, &attrib.vertices[v0]);
      amodel->triSurfaces->read(tobj, (t1-t0+1)/2, lucTexCoordData, &attrib.texcoords[t0]);
      printf("%d == %d\n", (t1-t0), attrib.texcoords.size());
      amodel->triSurfaces->read(tobj, (n1-n0+1)/3, lucNormalData, &attrib.normals[n0]);
      for (int v=v0; v<v1; v+=3)
      {
        g->checkPointMinMax(&attrib.vertices[v]);
        //printf("%f %f %f\n", attrib.vertices[v], attrib.vertices[v+1], attrib.vertices[v+2]);
        //Model::triSurfaces->read(tobj, 1, lucVertexData, &attrib.vertices[v]);
        //Model::triSurfaces->read(tobj, 1, lucTexCoordData, &attrib.texcoords[v]);
        //Model::triSurfaces->read(tobj, 1, lucNormalData, &attrib.normals[v]);
      }
      for (int id=0; id<shapes[i].mesh.indices.size(); id++)
      {
        int idx = shapes[i].mesh.indices[id].vertex_index - v0;
        amodel->triSurfaces->read(tobj, 1, lucIndexData, &idx);
        //Model::triSurfaces->read(tobj, 1, lucIndexData, &id);
      }
#endif
    }
    else
    {
      for (size_t f = 0; f < shapes[i].mesh.indices.size(); f += 3)
      {
        //This won't work with textures(tex coords) ... calculated indices have incorrect order
        addTriangles(tobj,
                     &attrib.vertices[shapes[i].mesh.indices[f].vertex_index*3],
                     &attrib.vertices[shapes[i].mesh.indices[f+1].vertex_index*3],
                     &attrib.vertices[shapes[i].mesh.indices[f+2].vertex_index*3],
                     trisplit);
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
  float RANGE = 2.f;
  float min[3] = {-RANGE,-RANGE,-RANGE};
  float max[3] = {RANGE,RANGE,RANGE};
  float dims[3] = {RANGE*2.f,RANGE*2.f,RANGE*2.f};
  float size = sqrt(dotProduct(dims,dims));
  viewer->title = "Test Pattern";

  //Demo colourmap, distance from model origin
  ColourMap* colourMap = new ColourMap(drawstate);
  unsigned int cmid = amodel->addColourMap(colourMap);
  //Colours: hex, abgr
  unsigned int colours[] = {0xff33bb66,0xff00ff00,0xffff3333,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff,0xff000000};
  colourMap->add(colours, 8);
  colourMap->calibrate(0, size);

  //Add colour bar display
  DrawingObject* cmap = addObject(new DrawingObject(drawstate, "colour-bar", "colourbar=1\n"));
  cmap->properties.data["colourmap"] = cmid;

  //Add points object
  DrawingObject* obj = addObject(new DrawingObject(drawstate, "particles", "opacity=0.75\nstatic=1\nlit=0\n"));
  obj->properties.data["colourmap"] = cmid;
  int pointsperswarm = numpoints/4; //4 swarms
  for (int i=0; i < numpoints; i++)
  {
    float colour, ref[3];
    ref[0] = min[0] + (max[0] - min[0]) * frand;
    ref[1] = min[1] + (max[1] - min[1]) * frand;
    ref[2] = min[2] + (max[2] - min[2]) * frand;

    //Demo colourmap value: distance from model origin
    colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));

    amodel->points->read(obj, 1, lucVertexData, ref);
    amodel->points->read(obj, 1, &colour, "demo colours");

    if (i % pointsperswarm == pointsperswarm-1 && i != numpoints-1)
        amodel->points->add(obj);
  }

  //Add lines
  obj = addObject(new DrawingObject(drawstate, "line-segments", "static=1\nlit=0\n"));
  obj->properties.data["colourmap"] = cmid;
  for (int i=0; i < 50; i++)
  {
    float colour, ref[3];
    ref[0] = min[0] + (max[0] - min[0]) * frand;
    ref[1] = min[1] + (max[1] - min[1]) * frand;
    ref[2] = min[2] + (max[2] - min[2]) * frand;

    //Demo colourmap value: distance from model origin
    colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));

    amodel->lines->read(obj, 1, lucVertexData, ref);
    amodel->lines->read(obj, 1, &colour, "demo colours");
  }

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
      obj = addObject(new DrawingObject(drawstate, label, "opacity=0.5\nstatic=1\n"));
      Colour c;
      c.value = (0xff000000 | 0xff<<(8*i));
      obj->properties.data["colour"] = c.toJson();
      amodel->triSurfaces->read(obj, 4, lucVertexData, verts[i], 2, 2);
    }
  }

  //Set model bounds...
  //drawstate.checkPointMinMax(min);
  //drawstate.checkPointMinMax(max);
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
  for (unsigned int i=0; i < amodel->geometry.size(); i++)
    amodel->geometry[i]->init();

  //Initialise all viewports to window size
  for (unsigned int v=0; v<amodel->views.size(); v++)
    amodel->views[v]->port(width, height);

  reloadShaders();
}

void LavaVu::reloadShaders()
{
  //Point shaders
  if (drawstate.prog[lucPointType]) delete drawstate.prog[lucPointType];
  drawstate.prog[lucPointType] = new Shader("pointShader.vert", "pointShader.frag");
  const char* pUniforms[14] = {"uPointScale", "uPointType", "uOpacity", "uPointDist", "uTextured", "uTexture", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation", "uAmbient", "uDiffuse", "uSpecular"};
  drawstate.prog[lucPointType]->loadUniforms(pUniforms, 14);
  const char* pAttribs[2] = {"aSize", "aPointType"};
  drawstate.prog[lucPointType]->loadAttribs(pAttribs, 2);

  //Line shaders
  if (drawstate.prog[lucLineType]) delete drawstate.prog[lucLineType];
  drawstate.prog[lucLineType] = new Shader("lineShader.vert", "lineShader.frag");
  const char* lUniforms[6] = {"uOpacity", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation"};
  drawstate.prog[lucLineType]->loadUniforms(lUniforms, 6);

  //Triangle shaders
  if (drawstate.prog[lucTriangleType]) delete drawstate.prog[lucTriangleType];
  drawstate.prog[lucTriangleType] = new Shader("triShader.vert", "triShader.frag");
  const char* tUniforms[13] = {"uOpacity", "uLighting", "uTextured", "uTexture", "uCalcNormal", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation", "uAmbient", "uDiffuse", "uSpecular"};
  drawstate.prog[lucTriangleType]->loadUniforms(tUniforms, 13);
  drawstate.prog[lucGridType] = drawstate.prog[lucTriangleType];

  //Volume ray marching shaders
  if (drawstate.prog[lucVolumeType]) delete drawstate.prog[lucVolumeType];
  drawstate.prog[lucVolumeType] = new Shader("volumeShader.vert", "volumeShader.frag");
  const char* vUniforms[24] = {"uPMatrix", "uInvPMatrix", "uMVMatrix", "uNMatrix", "uVolume", "uTransferFunction", "uBBMin", "uBBMax", "uResolution", "uEnableColour", "uBrightness", "uContrast", "uSaturation", "uPower", "uViewport", "uSamples", "uDensityFactor", "uIsoValue", "uIsoColour", "uIsoSmooth", "uIsoWalls", "uFilter", "uRange", "uDenMinMax"};
  drawstate.prog[lucVolumeType]->loadUniforms(vUniforms, 24);
  const char* vAttribs[1] = {"aVertexPosition"};
  drawstate.prog[lucVolumeType]->loadAttribs(vAttribs, 1);
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
      history.push_back(ss.str());
    }
  }

  //Set resolution
  //drawstate.globals["resolution"] = {new_width, new_height};
  aview->properties.data["resolution"] = {new_width, new_height};

  amodel->redraw();
}

void LavaVu::close()
{
  for (unsigned int i=0; i < models.size(); i++)
    models[i]->close();

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
  viewset = 0;

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
  std::string name = drawstate.global("caption");
  std::string vpt = aview->properties["title"];
  if (vpt.length() > 0)
  {
    title << aview->properties["title"];
    title << " (" << name << ")";
  }
  else
    title << name;

  if (amodel->timesteps.size() > 1)
    title << " - timestep " << std::setw(5) << std::setfill('0') << amodel->stepInfo();

  viewer->title = title.str();

  if (viewer->isopen && viewer->visible) viewer->show(); //Update title etc
}

//Called when view changed
void LavaVu::viewSelect(int idx, bool setBounds, bool autozoom)
{
  //Set a default viewport/camera if none
  if (idx < 0) idx = 0;
  view = idx;
  if (view < 0) view = amodel->views.size() - 1;
  if (view >= (int)amodel->views.size()) view = 0;

  aview = amodel->views[view];

  //Called when timestep/model changed (new model data)
  //Set model size from geometry / bounding box and apply auto zoom
  //- View bounds used for camera calc and border (view->min/max)
  //- Actual bounds used by geometry clipping etc (drawstate.min/max)
  //NOTE: sometimes we can reach this call before the GL context is created, hence the check
  if (viewer->isopen && setBounds)
  {
    //Auto-calc data ranges
    amodel->setup();

    float min[3], max[3];
    Properties::toFloatArray(aview->properties["min"], min, 3);
    Properties::toFloatArray(aview->properties["max"], max, 3);

    float omin[3] = {min[0], min[1], min[2]};
    float omax[3] = {max[0], max[1], max[2]};

    //If no range, flag invalid with +/-inf, will be expanded in setView
    for (int i=0; i<3; i++)
      if (omax[i]-omin[i] <= EPSILON) omax[i] = -(omin[i] = HUGE_VAL);

    //Expand bounds by all geometry objects
    for (unsigned int i=0; i < amodel->geometry.size(); i++)
      amodel->geometry[i]->setView(aview, omin, omax);

    //Set viewport based on window size
    aview->port(viewer->width, viewer->height);

    //Update the model bounding box - use global bounds if provided and sane in at least 2 dimensions
    if (max[0]-min[0] > EPSILON && max[1]-min[1] > EPSILON)
    {
      debug_print("Applied Model bounds %f,%f,%f - %f,%f,%f from global properties\n",
                  min[0], min[1], min[2],
                  max[0], max[1], max[2]);
      aview->init(false, min, max);
    }
    else
    {
      debug_print("Applied Model bounds %f,%f,%f - %f,%f,%f from geometry\n",
                  omin[0], omin[1], omin[2],
                  omax[0], omax[1], omax[2]);
      aview->init(false, omin, omax);
    }

    //Update actual bounding box max/min/range - it is possible for the view box to be smaller
    clearMinMax(drawstate.min, drawstate.max);
    compareCoordMinMax(drawstate.min, drawstate.max, omin);
    compareCoordMinMax(drawstate.min, drawstate.max, omax);
    //Apply viewport/global dims if valid
    if (min[0] != max[0] && min[1] != max[1])
    {
      compareCoordMinMax(drawstate.min, drawstate.max, min);
      compareCoordMinMax(drawstate.min, drawstate.max, max);
    }
    getCoordRange(drawstate.min, drawstate.max, drawstate.dims);
    debug_print("Calculated Actual bounds %f,%f,%f - %f,%f,%f \n",
                drawstate.min[0], drawstate.min[1], drawstate.min[2],
                drawstate.max[0], drawstate.max[1], drawstate.max[2]);

    // Apply step autozoom if set (applied based on detected bounding box)
    int zstep = aview->properties["zoomstep"];
    if (autozoom && zstep > 0 && amodel->step() % zstep == 0)
      aview->zoomToFit();
  }
  else
  {
    //Set view on geometry objects only, no boundary check
    for (unsigned int i=0; i < amodel->geometry.size(); i++)
      amodel->geometry[i]->setView(aview);
  }

  //Update background colour
  viewer->setBackground(Colour(aview->properties["background"]));
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

  //Scale text and 2d elements when downsampling output image
  aview->scale2d = viewer->scale2d();

  //Set viewport based on window size
  aview->port(viewer->width, viewer->height);
  GL_Error_Check;

  // Clear viewport
  GL_Error_Check;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Render
void LavaVu::display(void)
{
  if (!viewer->isopen) return;

  //Lock the state mutex, prevent updates while drawing
  std::lock_guard<std::mutex> guard(drawstate.mutex);

  clock_t t1 = clock();

  //Viewport reset flagged
  if (viewset > 0)
  {
#ifndef USE_OMEGALIB
    //Resize if required
    json res = aview->properties["resolution"];
    if ((int)res[0] != viewer->width || (int)res[1] != viewer->height)
    {
      viewer->setsize(res[0], res[1]);
      viewer->postdisplay = true;
      aview->initialised = false; //Force initial autozoom
      return;
    }
#endif
    //Update the viewports
    resetViews(viewset == 2);
  }

  //Always redraw the active view, others only if flag set
  if (aview)
  {
    if (!viewer->mouseState && (int)drawstate.global("sort") < 0 && aview->rotated)
    {
      aview->sort = true;
      aview->rotated = false;
    }
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

#ifdef USE_OMEGALIB
  drawSceneBlended();
#else

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
    //Loop through all viewports and display each
    int selview = view;
    for (unsigned int v=0; v<amodel->views.size(); v++)
    {
      viewApply(v);
      GL_Error_Check;

      // Default non-stereo render
      aview->projection(EYE_CENTRE);
      drawSceneBlended();
    }

    if (view != selview)
    viewSelect(selview);
  }
#endif

  //Print current info message (displayed for one frame only)
  if (status) displayMessage();

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
    viewer->pixels(encoder->buffer, 3, true);
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
  glScalef(1.0, 1.0, 1.0 * (int)aview->properties["coordsystem"]);

  float Xpos[3] = {length/2, 0, 0};
  float Ypos[3] = {0, length/2, 0};
  float Zpos[3] = {0, 0, length/2};

  axis->clear();
  axis->setView(aview);
  DrawingObject* aobj = drawstate.axisobj;
  if (!aobj) aobj = new DrawingObject(drawstate, "", "wireframe=false\nclip=false\n");
  if (!aview->hasObject(aobj)) aview->addObject(aobj);
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

  drawstate.fonts.rasterSetFontCharset(FONT_VECTOR);
  drawstate.fonts.rasterSetFontScale(length*6.0);
  drawstate.fonts.printSetColour(viewer->inverse.value);
  drawstate.fonts.print3dBillboard(Xpos[0],    Xpos[1]-LH, Xpos[2], "X");
  drawstate.fonts.print3dBillboard(Ypos[0]-LH, Ypos[1],    Ypos[2], "Y");
  if (aview->is3d)
    drawstate.fonts.print3dBillboard(Zpos[0]-LH, Zpos[1]-LH, Zpos[2], "Z");

  glPopAttrib();

  //Restore
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  GL_Error_Check;

  //Restore info/error stream
  if (verbose) infostream = stderr;
}

void LavaVu::drawRulers()
{
  if (!aview->properties["rulers"]) return;
  infostream = NULL;
  DrawingObject* obj = drawstate.rulerobj;
  rulers->clear();
  rulers->setView(aview);
  if (!obj) obj = new DrawingObject(drawstate, "", "wireframe=false\nclip=false\nlit=false");
  if (!aview->hasObject(obj)) aview->addObject(obj);
  rulers->add(obj);
  obj->properties.data["linewidth"] = (float)aview->properties["rulerwidth"];
  obj->properties.data["fontscale"] = (float)aview->properties["fontscale"] * 0.5*aview->model_size;
  obj->properties.data["font"] = "vector";
  //Colour for labels
  obj->properties.data["colour"] = viewer->inverse.toJson();


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
  if (verbose) infostream = stderr;
}

void LavaVu::drawRuler(DrawingObject* obj, float start[3], float end[3], float labelmin, float labelmax, int ticks, int axis)
{
  // Draw rulers with optional tick marks
  //float fontscale = drawstate.fonts.printSetFont(properties, "vector", 0.05*model_size*textscale);
  //float fontscale = drawstate.fonts.printSetFont(aview->properties, "vector", 1.0, 0.08*aview->model_size);

  float vec[3];
  float length;
  vectorSubtract(vec, end, start);

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
  //Check for boolean or numeric value for border
  float bordersize = 1.0;
  json& bord = aview->properties["border"];
  if (bord.is_number())
    bordersize = bord;
  else if (!bord)
    bordersize = 0.0;
  if (bordersize <= 0.0) return;

  DrawingObject* obj = drawstate.borderobj;
  border->clear();
  border->setView(aview);
  if (!obj) obj = new DrawingObject(drawstate, "", "clip=false\n");
  if (!aview->hasObject(obj)) aview->addObject(obj);
  obj->properties.data["colour"] = aview->properties["bordercolour"];
  if (!aview->is3d) obj->properties.data["depthtest"] = false;

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
    border->drawCuboid(obj, maxvert, minvert, qrot, true);
  }
  else
  {
    minvert.z = maxvert.z = 0;
    Vec3d vert1 = Vec3d(aview->max[0], aview->min[1], 0);
    Vec3d vert2 = Vec3d(aview->min[0], aview->max[1], 0);
    border->read(obj, 1, lucVertexData, minvert.ref(), 2, 2);
    border->read(obj, 1, lucVertexData, vert1.ref());
    border->read(obj, 1, lucVertexData, vert2.ref());
    border->read(obj, 1, lucVertexData, maxvert.ref());
  }

  border->update();
  border->draw();

  //Restore info/error stream
  if (verbose) infostream = stderr;
}

GeomData* LavaVu::getGeometry(DrawingObject* obj)
{
  //Find the first available geometry container for this drawing object
  //Used to append colour values and to get a representative colour for objects
  //Only works if we know object has a single geometry type
  GeomData* geomdata = NULL;
  for (int type=lucMinType; type<lucMaxType; type++)
  {
    geomdata = amodel->geometry[type]->getObjectStore(obj);
    if (geomdata) break;
  }
  return geomdata;
}

void LavaVu::displayObjectList(bool console)
{
  //Print available objects by id to screen and stderr
  int offset = 0;
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
  drawstate.fonts.printSetColour(scol);
  drawstate.fonts.rasterSetFontCharset(FONT_VECTOR);
  drawstate.fonts.rasterSetFontScale(scale);

  drawstate.fonts.print(xpos+1, ypos-1, str.c_str());

  //Use provided text colour or calculated
  if (colour)
    drawstate.fonts.printSetColour(colour->value);
  else
    drawstate.fonts.printSetColour(tcol);

  drawstate.fonts.print(xpos, ypos, str.c_str());

  //Revert to normal colour
  drawstate.fonts.printSetColour(viewer->inverse.value);
}

void LavaVu::displayMessage()
{
  //Skip if no message or recording frames to video
  if (strlen(message) && !encoder)
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
  std::string title = aview->properties["title"];
  //Timestep macro ##
  size_t pos =  title.find("##");
  if (pos != std::string::npos && drawstate.timesteps.size() >= drawstate.now)
    title.replace(pos, 2, std::to_string(drawstate.timesteps[drawstate.now]->step));
  aview->drawOverlay(viewer->inverse, title);
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

  amodel->triSurfaces->draw();
  amodel->quadSurfaces->draw();
  amodel->points->draw();
  amodel->vectors->draw();
  amodel->tracers->draw();
  amodel->shapes->draw();
  amodel->labels->draw();
  amodel->volumes->draw();
  amodel->lines->draw();

#ifndef USE_OMEGALIB
  drawBorder();
#endif
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
    //Open database file, if a non-db model already loaded, load into that
    if (models.size() == 0 || amodel && amodel->db)
    {
      amodel = new Model(drawstate);
      models.push_back(amodel);
    }

    //Load objects from db
    amodel->load(fn);

    //Ensure default view selected
    aview = amodel->defaultView();
    view = 0;

    //Load initial figure
    if (initfigure != 0) amodel->loadFigure(initfigure-1);

    //Set default window title to model name
    std::string name = drawstate.global("caption");
    if (name == APPNAME__ && !amodel->memorydb) drawstate.globals["caption"] = amodel->file.base;

    //Save path of first sucessfully loaded model
    if (dbpath && viewer->output_path.length() == 0)
    {
      viewer->output_path = fn.path;
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

    //Reselect the active view after loading any model data (resets model bounds)
    viewSelect(view, true, true);
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
  if (drawstate.global("filestep")) parseCommands("newstep");

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

  //Reselect the active view after loading any model data (resets model bounds)
  viewSelect(view, true, true);

  return true;
}

void LavaVu::defaultModel()
{
  //Adds a default model, window & viewport
  amodel = new Model(drawstate);
  models.push_back(amodel);

  //Set a default view
  aview = amodel->defaultView();

  //Setup default colourmaps
  //amodel->initColourMaps();
}

//Load model data at specified timestep
bool LavaVu::loadModelStep(int model_idx, int at_timestep, bool autozoom)
{
  if (models.size() == 0) defaultModel();
  if (model_idx == model && at_timestep >= 0 && at_timestep == drawstate.now) return false; //No change
  if (at_timestep >= 0)
  {
    //Cache selected step, then force new timestep set when model changes
    if (drawstate.global("cache")) amodel->cacheStep();
  }

  if (model_idx >= (int)models.size()) return false;

  //Save active model as selected
  amodel = models[model_idx];
  model = model_idx;

  //Have a database model loaded already?
  if (amodel->objects.size() > 0)
  {
    //Redraw model data
    amodel->redraw();

    //Set timestep and load geometry at that step
    if (amodel->db)
    {
      if (at_timestep < 0)
        amodel->setTimeStep(drawstate.now);
      else
        amodel->setTimeStep(amodel->nearestTimeStep(at_timestep));
      if (verbose) std::cerr << "Loading vis '" << drawstate.global("caption") << "', timestep: " << amodel->step() << std::endl;
    }
  }

  if (!aview) aview = amodel->views[0];
  json res = aview->properties["resolution"];

  //Not yet opened or resized?
  if (!viewer->isopen)
    //Open window at required size
    viewer->open(res[0], res[1]);
  else
    //Resize if necessary
    viewer->setsize(res[0], res[1]);

  //Flag a view update
  viewset = autozoom ? 2 : 1;

  return true;
}

void LavaVu::encodeVideo(std::string filename, int fps)
{
  //TODO: - make VideoEncoder use OutputInterface
  //      - make image frame output a default video output
  //        when libavcodec not available
#ifdef HAVE_LIBAVCODEC
  if (!encoder)
  {
    if (filename.length() == 0) 
    {
      filename = drawstate.global("caption");
      filename += ".mp4";
    }
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

      if (images)
      {
        std::string title = drawstate.global("caption");
        std::ostringstream filess;
        filess << title << '-' << std::setw(5) << std::setfill('0') << amodel->step();
        viewer->image(filess.str());
      }

#ifdef HAVE_LIBAVCODEC
      //Always output to video encode if it exists
      if (encoder)
      {
        viewer->pixels(encoder->buffer, 3, true);
        //bitrate settings?
        encoder->frame();
      }
#endif
    }
  }
}

void LavaVu::dumpCSV(DrawingObject* obj)
{
  for (unsigned int i=0; i < amodel->objects.size(); i++)
  {
    if (!amodel->objects[i]->skip && (!obj || amodel->objects[i] == obj))
    {
      for (int type=lucMinType; type<lucMaxType; type++)
      {
        std::ostringstream ss;
        amodel->geometry[type]->dump(ss, amodel->objects[i]);

        std::string results = ss.str();
        if (results.size() > 0)
        {
          char filename[FILE_PATH_MAX];
          sprintf(filename, "%s%s_%s.%05d.csv", viewer->output_path.c_str(), amodel->objects[i]->name().c_str(),
                  GeomData::names[type].c_str(), amodel->stepInfo());
          std::ofstream csv;
          csv.open(filename, std::ios::out | std::ios::trunc);
          std::cout << " * Writing object " << amodel->objects[i]->name() << " to " << filename << std::endl;
          csv << results;
          csv.close();
        }
      }
    }
  }
}

std::string LavaVu::jsonWriteFile(DrawingObject* obj, bool jsonp, bool objdata)
{
  //Write new JSON format objects
  char filename[FILE_PATH_MAX];
  char ext[6];
  std::string name = drawstate.global("caption");
  strcpy(ext, "jsonp");
  if (!jsonp) ext[4] = '\0';
  if (obj)
    sprintf(filename, "%s%s_%s_%05d.%s", viewer->output_path.c_str(), name.c_str(),
            obj->name().c_str(), amodel->stepInfo(), ext);
  else
    sprintf(filename, "%s%s_%05d.%s", viewer->output_path.c_str(), name.c_str(), amodel->stepInfo(), ext);
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
    amodel->jsonRead(buffer.str());
    file.close();
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
void LavaVu::render()
{
  if (!amodel || !viewer->isopen) return;
  //Just render a frame
  viewer->display();
}

void LavaVu::init()
{
  if (viewer->isopen) return;
  viewer->open();
  viewer->init();
}

std::string LavaVu::image(std::string filename, int width, int height, bool frame)
{
  if (!amodel || !viewer->isopen) return "";
  //Set width/height override
  viewer->outwidth = width;
  viewer->outheight = height;
  //Always jpeg encode to string for frame output
  if (frame) return viewer->image("", true);
  //Otherwise write image to file or return as string (base64 data url)
  return viewer->image(filename);
}

std::string LavaVu::web(bool tofile)
{
  if (!amodel) return "";
  display(); //Forces view/window open
  amodel->triSurfaces->loadMesh();  //Optimise triangle meshes before export
  if (!tofile)
  {
    std::stringstream ss;
    amodel->jsonWrite(ss, 0, true);
    return ss.str();
  }
  return jsonWriteFile(NULL, false, true);
}

void LavaVu::addObject(std::string name, std::string properties)
{
  if (!amodel) return;
  aobject = addObject(new DrawingObject(drawstate, name));
  if (properties.length()) setObject(name, properties);
}

void LavaVu::setObject(std::string name, std::string properties)
{
  if (!amodel) return;
  if (name.length() > 0)
    aobject = lookupObject(name);
  if (!aobject) return;
  json imported = json::parse(properties);
  aobject->properties.merge(imported);
}

std::string LavaVu::getObject(std::string name)
{
  if (!amodel) return "{}";
  DrawingObject* object;
  if (name.length() > 0)
    object = lookupObject(name);
  if (!object) return "{}";
  std::stringstream ss;
  //Export object state
  amodel->jsonWrite(ss, object, false);
  json data = json::parse(ss.str());
  return data["objects"][0].dump();
}

int LavaVu::colourMap(std::string name, std::string colours)
{
  if (!amodel) return -1;

  for (unsigned int i=0; i < amodel->colourMaps.size(); i++)
  {
    if (name ==  amodel->colourMaps[i]->name)
    {
      amodel->colourMaps[i]->loadPalette(colours);
      return i;
    }
  }

  //Add a new colourmap if not found
  ColourMap* cmap = new ColourMap(drawstate, name.c_str(), 0, 1);
  cmap->loadPalette(colours);
  amodel->colourMaps.push_back(cmap);
  return amodel->colourMaps.size()-1;
}

void LavaVu::setState(std::string state)
{
  if (!amodel) return;
  amodel->jsonRead(state);
  //Why? This should be explicitly called or we can't set properties without display side effects
  //display();
}

std::string LavaVu::getState()
{
  if (!amodel) return "{}";
  std::stringstream ss;
  //Export current state
  amodel->jsonWrite(ss, 0, false);
  return ss.str();
}

std::string LavaVu::getFigures()
{
  if (!amodel) return "[]";
  std::stringstream ss;
  ss << "[\n";
  for (unsigned int s=0; s<amodel->figures.size(); s++)
  {
    amodel->loadFigure(s);
    amodel->jsonWrite(ss, 0, false);
    if (s < amodel->figures.size()-1) ss << ",\n";
  }
  //Export current state only if no figures
  if (amodel->figures.size() == 0)
    amodel->jsonWrite(ss, 0, false);
  ss << "]\n";
  return ss.str();
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

void LavaVu::loadVectors(std::vector< std::vector <float> > array, lucGeometryDataType type)
{
  //Get selected object or create a new one
  if (!amodel || !aobject) return;

  //Get the type to load (defaults to points)
  std::string gtype = aobject->properties["geometry"];
  Geometry* container = getGeometryType(gtype);
  if (!container) return;

  //Load 3d vertices
  for (unsigned int i=0; i < array.size(); i++)
    container->read(aobject, 1, type, &array[i][0]);
}

void LavaVu::loadValues(std::vector <float> array, std::string label, float minimum, float maximum)
{
  //Get selected object or create a new one
  if (!amodel || !aobject) return;

  //Get the type to load (defaults to points)
  std::string gtype = aobject->properties["geometry"];
  Geometry* container = getGeometryType(gtype);
  if (!container) return;

  //Load scalar values
  container->read(aobject, array.size(), &array[0], label);
}

void LavaVu::loadUnsigned(std::vector <unsigned int> array, lucGeometryDataType type)
{
  //Get selected object or create a new one
  if (!amodel || !aobject) return;

  //Get the type to load (defaults to points)
  std::string gtype = aobject->properties["geometry"];
  Geometry* container = getGeometryType(gtype);
  if (!container) return;

  //Load scalar values
  //for (unsigned int i=0; i < array.size(); i++)
  //  container->read(aobject, 1, type, &array[i]);
  container->read(aobject, array.size(), type, &array[0]);
}

void LavaVu::labels(std::vector <std::string> labels)
{
  //Get selected object or create a new one
  if (!amodel || !aobject) return;

  //Get the type to load (defaults to points)
  std::string gtype = aobject->properties["geometry"];
  Geometry* container = getGeometryType(gtype);
  if (!container) return;

  container->label(aobject, labels);
}

std::vector<float> LavaVu::imageArray(std::string path, int width, int height, int channels)
{
  //Return an image as a vector of floats
  //- first 3 values are width,height,channels
  //- read from disk if path provided
  //- read from framebuffer otherwise
  GLubyte* image = NULL;
  int outchannels = channels;
  ImageFile* img = NULL;
  if (path.length() > 0)
  {
    img = new ImageFile(path);
    width = img->width;
    height = img->height;
    outchannels = img->channels;
    image = img->pixels;
    //printf("Reading file %d x %d @ %d\n", width, height, channels);
  }
  else
  {
    //Get current image from framebuffer
    image = viewer->pixels(NULL, width, height, outchannels);
    //printf("Reading framebuffer %d x %d @ %d\n", width, height, outchannels);
  }

  if (!image) return std::vector<float>();

  //Size in pixels
  int size = width*height;
  std::vector<float> data(3+size*channels);
  //Add dims
  data[0] = width;
  data[1] = height;
  data[2] = channels;
  //Load data
  float r255 = 1.0/255.0;
  bool skipalpha = outchannels > channels;
  int idx=3;
  for (int i=0; i<size*outchannels; i++)
  {
    if (skipalpha && i%4==3) continue;
    data[idx++] = image[i] * r255;
  }
  //Free image data
  if (img)
    delete img;
  else
    delete[] image;
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
    int qdims[2] = {width/downsample, height/downsample};
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
    for (int i=3; i<image1.size(); i++)
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
  pthread_mutex_lock(&viewer->cmd_mutex);
  viewer->commands.push_back(cmds);
  debug_print("QCMD: %s\n", cmds.c_str());
  viewer->postdisplay = true;
  pthread_mutex_unlock(&viewer->cmd_mutex);
}
