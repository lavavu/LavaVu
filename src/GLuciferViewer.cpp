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
** CONSEQUENTIAL DAMAnGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
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

//Viewer class
#include "Include.h"
#include "GLuciferViewer.h"
#include "Shaders.h"
#include "VideoEncoder.h"
#include <typeinfo>

//Include the decompression routines
#ifdef USE_ZLIB
#include <zlib.h>
#else
#include "miniz/tinfl.c"
#endif

//Viewer class implementation...
GLuciferViewer::GLuciferViewer(std::vector<std::string> args, OpenGLViewer* viewer, int width, int height) : ViewerApp(viewer)
{
   bool output = false, verbose = false, hideall = false, dbpath = false;
   float alpha = 0, subsample = 0;
   FilePath* dbfile = NULL;

   //Defaults
   timestep = -1;
   endstep = -1;
   dump = dumpid = 0;
   noload = false;
   viewAll = false;
   viewPorts = true;
   writeimage = writemovie = false;
   sort_on_rotate = true;
   message[0] = '\0';

   fixedwidth = width;
   fixedheight = height;

   window = 0;
   tracersteps = 0;
   objectlist = noload;

   view = -1;
   aview = NULL;
   awin = NULL;
   amodel = NULL;

   filters[0] = filters[1] = filters[2] = filters[3] = filters[4] = filters[5] = filters[6] = filters[7] = filters[8] = true;

   //A set of default arguments can be stored in a file...
   std::ifstream argfile("gLucifer_args.cfg");
   if (argfile) //Found a config file, load arguments
   {
      std::string line;
      while (std::getline(argfile, line))
         args.push_back(line);
      argfile.close();
   }

   //Interaction command prompt
   entry = "";
   recording = true;
   loop = false;

   //Read command line switches
   for (int i=0; i<args.size(); i++)
   {
      char x;
      std::istringstream ss(args[i]);
      ss >> x;
      //Switches can be before or after files but not between
      if (x == '-' && args[i].length() > 1)
      {
         ss >> x;
         switch (x)
         {
         case 'B':
            filters[lucLabelType] = false;
            break;
         case 'P':
            if (args[i].length() > 2)
              ss >> subsample;
            else
              filters[lucPointType] = false;
            break;
         case 'S':
            filters[lucGridType] = false;
            break;
         case 'U':
            filters[lucTriangleType] = false;
            break;
         case 'V':
            filters[lucVectorType] = false;
            break;
         case 'T':
            filters[lucTracerType] = false;
            break;
         case 'L':
            filters[lucLineType] = false;
            break;
         case 'H':
            filters[lucShapeType] = false;
            break;
         case 'O':
            filters[lucVolumeType] = false;
            break;
         case 'N':
            noload = true;
            break;
         case 'A':
            hideall = true;
            break;
         case 'v':
            verbose = true;
            break;
         case 'o':
            output = true;
            break;
         case 'x':
            ss >> viewer->outwidth >> x >> viewer->outheight;
            break;
         case 'a':
            ss >> alpha;
            break;
         case 'c':
            ss >> GeomCache::size;
            break;
         case 'd':
            if (args[i].length() > 2) ss >> dumpid;
            dump = 1;
            viewer->visible = false;
            break;
         case 'J':
            if (args[i].length() > 2) ss >> dumpid;
            dump = 3;
            viewer->visible = false;
            break;
         case 'j':
            if (args[i].length() > 2) ss >> dumpid;
            dump = 2;
            viewer->visible = false;
            break;
         case 'h':
            //Invisible window requested
            viewer->visible = false;
            break;
         case 't':
            //Use alpha channel in png output
            viewer->alphapng = true;
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
            viewer->visible = false;
            break;
         case 'm':
            //Movie write
            writemovie = true;
            viewer->visible = false;
            break;
         case 'l':
            //Use local shader files (set shader path to current working directory)
            Shader::path = NULL;
            break;
         default:
            //Attempt to interpret as timestep
            std::istringstream ss2(args[i]);
            ss2 >> x;
            if (timestep < 0)
               ss2 >> timestep;
            else
               ss2 >> endstep;
         }
         //Clear non file arguments
         args[i].clear();
      }
   }

   //Set default timestep if none specified
   if (timestep < 0) timestep = 0;

   //Set info/error stream
   if (verbose && !output)
      infostream = stderr;

   //Load list of models passed on command line
   for (int i=0; i<args.size(); i++)
   {
      if (args[i].length() > 0)
      {
         //Load all objects in db
         if (loadModel(args[i]) && !dbfile)
            //Save path of first sucessfully loaded model
            dbfile = new FilePath(args[i]);
      }
   }

   //Create new geometry containers
   geometry.resize(lucMaxType);
   geometry[lucLabelType] = labels = new Geometry(hideall);
   geometry[lucPointType] = points = new Points(hideall);
   geometry[lucVectorType] = vectors = new Vectors(hideall);
   geometry[lucTracerType] = tracers = new Tracers(hideall);
   geometry[lucGridType] = quadSurfaces = new QuadSurfaces(hideall);
   geometry[lucVolumeType] = volumes = new Volumes(hideall);
   geometry[lucTriangleType] = triSurfaces = new TriSurfaces(hideall);
   geometry[lucLineType] = lines = new Lines(hideall);
   geometry[lucShapeType] = shapes = new Shapes(hideall);

   //Set script output flag
   this->output = output;
   //Opacity override: either [0,1] or (1,255]
   if (alpha > 1.0) alpha /= 255.0;
   GeomData::opacity = alpha;
   //Points initial sub-sampling
   if (subsample) Points::subSample = subsample;

   //Strip db path
   if (dbpath && dbfile)
   {
      strncpy(viewer->output_path, dbfile->path.c_str(), 1023);
      delete dbfile;
   }
   debug_print("Output path set to %s\n", viewer->output_path);
}

GLuciferViewer::~GLuciferViewer()
{
   clearObjects(true);

   //Kill all objects
   for (unsigned int i=0; i < geometry.size(); i++)
      delete geometry[i];

   //Clear windows
   for(unsigned int i=0; i<windows.size(); i++)
      if (windows[i]) delete windows[i]; 

   debug_print("Peak geometry memory usage: %.3f mb\n", FloatValues::mempeak/1000000.0f);
}

void GLuciferViewer::run(bool persist)
{
   if (persist)
   {
     //Server mode, disable viewports
     viewAll = true;
     viewPorts = false;
   }

   if (writeimage || writemovie || dump > 1)
   {
      //Load vis data for each window and write image
      int win = 0;
      while (loadWindow(win, timestep, true) > 0)
      {
         //Bounds checks
         if (endstep < timestep) endstep = timestep;
         int final = amodel->lastStep();
         if (endstep < final) final = endstep;

         setTimeStep(timestep);

         if (writeimage || writemovie)
         {
            resetViews(true);
            viewer->display();
            //Read input script from stdin on first timestep
            viewer->pollInput();

            char path[256];
            if (writeimage)
            {
               std::cout << "... Writing image(s) for window " << awin->name << " Timesteps: " << timestep << " to " << endstep << std::endl;
            }
            if (writemovie)
            {
               std::cout << "... Writing movie for window " << awin->name << " Timesteps: " << timestep << " to " << endstep << std::endl;
               //Other formats?? avi/mpeg4?
               sprintf(path, "%s.mp4", awin->name.c_str());
            }

            //Write output
            writeSteps(writeimage, writemovie, timestep, endstep, path);
         }

         //Dump json data if requested
         if (dump == 2)
            jsonWriteFile(dumpid);
         else if (dump == 3)
            jsonWriteFile(dumpid, true);

         win++;
      }
   }
   else
   {
      //Load vis data for first window
      if (!loadWindow(0, timestep, true))
         abort_program("Model file load error, no window data\n");

   }

   //Dump csv data if requested
   if (dump == 1)
      dumpById(dumpid);

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

void GLuciferViewer::readScriptFile(FilePath& fn)
{
   if (fn.ext == "script")
      parseCommands("script " + fn.full);
}

void GLuciferViewer::readHeightMap(FilePath& fn)
{
   int rows, cols, size = 2, subsample;
   int byteorder = 0;   //Little endian default
   float xmap, ymap, xdim, ydim;
   int geomtype = lucTriangleType, header = 0;
   float downscale = 1;
   bool newView = false;
   std::string texfile;

   //Can only parse dem format wth ers or hdr header
   if (fn.ext != "dem") return;

   char ersfilename[256];
   char hdrfilename[256];
   sprintf(ersfilename, "%s%s.ers", fn.path.c_str(), fn.base.c_str());
   sprintf(hdrfilename, "%s%s.hdr", fn.path.c_str(), fn.base.c_str());

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

   float min[3] = {0,0,0};
   float max[3] = {0,0,0};
   float range[3] = {0,0,0};

   min[0] = xmap;
   max[0] = xmap + (cols * xdim);
   max[1] = 1000;
   min[2] = ymap - (rows * xdim); //Northing decreases with latitude
   max[2] = ymap;
   range[0] = fabs(max[0] - min[0]);
   range[2] = fabs(max[2] - min[2]);

   //Create default window
   if (!amodel) 
   {
      //Setup a default model
      newModel("", 800, 600, 0xff000000, min, max);
      newView = true;
      //Scale height * 10 to see features
      aview->setScale(1,10,1);
   }
   else if (!aview)
      aview = awin->views[0];
   //   viewSelect(0);
   strcpy(viewer->title, fn.base.c_str());

   //Demo colourmap
   ColourMap* colourMap = new ColourMap();
   addColourMap(colourMap);

   //Colours: hex, abgr
   colourMap->add(0x55660000, 0);         //Sea
   colourMap->add(0xffffff00);
   colourMap->add(0xff118866, 0.000001); //Land
   colourMap->add(0xff006633);
   colourMap->add(0xff77ffff);
   colourMap->add(0xff0088ff);
   colourMap->add(0xff0000ff);
   colourMap->add(0xff000000, 1.0);

   //Add colour bar display
   //newObject("colourbar", true, 0, colourMap, 1.0, "colourbar=1\n");

   //Create a height map grid
   int sx=cols, sz=rows;
   debug_print("Height dataset %d x %d Sampling at X,Z %d,%d\n", sx, sz, sx / subsample, sz / subsample);
                                                                    //opacity [0,1]
   DrawingObject *obj, *sea;
   char props[256];
   sprintf(props, "cullface=0\ntexturefile=%s\n", texfile.c_str());
   obj = newObject(fn.base, true, 0, colourMap, 1.0, props);
   //Sea level surf
   //sea = newObject("Sea level", true, 0xffffff00, NULL, 0.5, "cullface=1\n");
   sea = newObject("Sea level", true, 0xffffcc00, NULL, 0.5, "cullface=0\n");

   int gridx = ceil(sx / (float)subsample);
   int gridz = ceil(sz / (float)subsample);
   int seax = ceil(sx / (float)(subsample*50));
   int seaz = ceil(sz / (float)(subsample*50));

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
            vertex[1] = -30; //Nodata
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
         geometry[geomtype]->read(obj, 1, lucVertexData, vertex.ref(), gridx, gridz);
         //Colour by height
         geometry[geomtype]->read(obj, 1, lucColourValueData, &colourval);

         vertex[0] += xdim * subsample;
      }
      vertex[0] = min[0];
      vertex[2] -= ydim * subsample;
   }
   file.close();
   geometry[geomtype]->setup(obj, lucColourValueData, min[1], max[1]);

   //Sea grid points
   vertex[0] = min[0];
   vertex[1] = 0;
   vertex[2] = min[2];
   quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);
   vertex[0] = max[0];
   quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);
   vertex[0] = min[0];
   vertex[2] = max[2];
   quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);
   vertex[0] = max[0];
   quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);

   quadSurfaces->setup(sea, lucColourValueData, min[1], max[1]);

      Geometry::checkPointMinMax(min);
      Geometry::checkPointMinMax(max);

   range[1] = max[1] - min[1];
   debug_print("Sampled %d values, min height %f max height %f\n", (sx / subsample) * (sz / subsample), min[1], max[1]);
   //debug_print("Range %f,%f,%f to %f,%f,%f\n", min[0], min[1], min[2], max[0], max[1], max[2]);

   debug_print("X min %f max %f range %f\n", min[0], max[0], range[0]);
   debug_print("Y min %f max %f range %f\n", min[1], max[1], range[1]);
   debug_print("Z min %f max %f range %f\n", min[2], max[2], range[2]);
}

void GLuciferViewer::createDemoModel()
{
   int RANGE = 2;
   float min[3] = {-RANGE,-RANGE,-RANGE};
   float max[3] = {RANGE,RANGE,RANGE};
   float dims[3] = {RANGE*2,RANGE*2,RANGE*2};
   float size = sqrt(dotProduct(dims,dims));
   strcpy(viewer->title, "Test Pattern\0");

   //Setup a default model
   newModel("GLucifer Viewer", 800, 600, 0xff000000, min, max);

   //Demo colourmap, distance from model origin
   ColourMap* colourMap = new ColourMap();
   addColourMap(colourMap);
   //Colours: hex, abgr
   unsigned int colours[] = {0xff33bb66,0xff00ff00,0xffff3333,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff,0xff000000};
   colourMap->add(colours, 8);
   colourMap->calibrate(0, size);

   //Add colour bar display
   newObject("colour-bar", false, 0, colourMap, 1.0, "colourbar=1\n");

   //Add points object
   DrawingObject* obj = newObject("particles", false, 0, colourMap, 0.75, "lit=0\n");
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

      points->read(obj, 1, lucVertexData, ref);
      points->read(obj, 1, lucColourValueData, &colour);

      if (i % NUMSWARM == NUMSWARM-1)
      {
         points->setup(obj, lucColourValueData, 0, size);
         if (i != NUMPOINTS-1)
            points->add(obj);
      }
   }

   //Add lines
   obj = newObject("line-segments", false, 0, colourMap, 1.0, "lit=0\n");
   for (int i=0; i < 50; i++) 
   {
      float colour, ref[3];
      ref[0] = min[0] + (max[0] - min[0]) * frand;
      ref[1] = min[1] + (max[1] - min[1]) * frand;
      ref[2] = min[2] + (max[2] - min[2]) * frand;

      //Demo colourmap value: distance from model origin
      colour = sqrt(pow(ref[0]-min[0], 2) + pow(ref[1]-min[1], 2) + pow(ref[2]-min[2], 2));
 
      lines->read(obj, 1, lucVertexData, ref);
      lines->read(obj, 1, lucColourValueData, &colour);
   }
   lines->setup(obj, lucColourValueData, 0, size);

   //Add some quads
   {
      float verts[3][12] = {{-2,-2,0,  2,-2,0,  -2,2,0,  2,2,0},
                            {-2,0,-2,  2,0,-2,  -2,0,2,  2,0,2},
                            {0,-2,-2,  0,2,-2,   0,-2,2, 0,2,2}};
      char axischar[3] = {'Z', 'Y', 'X'};
      for (int i=0; i<3; i++)
      {
         char label[64];
         sprintf(label, "%c-cross-section", axischar[i]);
         obj = newObject(label, false, 0xff000000 | 0xff<<(8*i), NULL, 0.5);
         triSurfaces->read(obj, 4, lucVertexData, verts[i], 2, 2);
      }
   }
}

//Adds a default model, window & viewport to the viewer
void GLuciferViewer::newModel(std::string name, int w, int h, int bg, float mmin[3], float mmax[3])
{
   amodel = new Model();

   //Set a default window, viewport & camera
   awin = new Win(0, name, w, h, bg, mmin, mmax);
   aview = awin->addView(new View(name.c_str()));

   //Add window to global window list
   windows.push_back(awin);
   amodel->windows.push_back(awin);
}

DrawingObject* GLuciferViewer::newObject(std::string name, bool persistent, int colour, ColourMap* map, float opacity, const char* properties)
{
   DrawingObject* obj = new DrawingObject(0, persistent, name, colour, map, opacity, properties);
   aview->addObject(obj);
   awin->addObject(obj);
   amodel->addObject(obj); //Add to model master list
   return obj;
}

void GLuciferViewer::open(int width, int height)
{
   //Init geometry containers
   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->init();

   //Initialise all viewports to window size
   for (unsigned int w=0; w<windows.size(); w++)
      windows[w]->initViewports();

   // Load shaders
   Points::prog = new Shader("pointShader.vert", "pointShader.frag");
   const char* pUniforms[6] = {"uPointScale", "uPointType", "uOpacity", "uPointDist", "uTextured", "uTexture"};
   Points::prog->loadUniforms(pUniforms, 6);
   const char* pAttribs[2] = {"aSize", "aPointType"};
   Points::prog->loadAttribs(pAttribs, 2);
   if (strstr(typeid(*viewer).name(), "OSMesa"))
   {
     //Hack to speed up default point drawing size when using OSMesa
     points->pointType = 4;
     debug_print("OSMesa Detected: pointType default = 4\n");
   }
   else
   {
      //Triangle shaders too slow with OSMesa
      TriSurfaces::prog = new Shader("triShader.vert", "triShader.frag");
      const char* tUniforms[4] = {"uOpacity", "uLighting", "uTextured", "uTexture"};
      TriSurfaces::prog->loadUniforms(tUniforms, 4);
      QuadSurfaces::prog = TriSurfaces::prog;
   }

   //Volume ray marching shaders
   Volumes::prog = new Shader("volumeShader.vert", "volumeShader.frag");
   const char* vUniforms[22] = {"uPMatrix", "uMVMatrix", "uNMatrix", "uVolume", "uTransferFunction", "uBBMin", "uBBMax", "uResolution", "uEnableColour", "uBrightness", "uContrast", "uPower", "uFocalLength", "uWindowSize", "uSamples", "uDensityFactor", "uIsoValue", "uIsoColour", "uIsoSmooth", "uIsoWalls", "uFilter", "uRange"};
   Volumes::prog->loadUniforms(vUniforms, 22);
   const char* vAttribs[2] = {"aVertexPosition"};
   Volumes::prog->loadAttribs(pAttribs, 2);
}

void GLuciferViewer::resize(int new_width, int new_height)
{
   //On resizes after initial size set, adjust point scaling
   if (new_width > 0)
   {
      float size0 = viewer->width * viewer->height;
      if (size0 > 0)
      {
         float size1 = new_width * new_height;
         float r = sqrt(size1 / size0);
         // Adjust particle size by smallest of dimension changes
         debug_print("Adjusting particle size scale %f to ", points->scale);
         points->scale *= r; 
         debug_print("%f ( * %f )\n", points->scale, r);
         std::ostringstream ss;
         ss << "resize " << new_width << " " << new_height;
         record(true, ss.str());
      }
   }

   redrawViewports();
}

void GLuciferViewer::close()
{
   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->close();
}

void GLuciferViewer::showById(unsigned int id, bool state)
{
   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->showById(id, state);
   //Setting here to allow hiding of objects without geometry (colourbars)
   amodel->objects[id-1]->visible = state;
}

void GLuciferViewer::setOpacity(unsigned int id, float opacity)
{
   if (opacity > 1.0) opacity /= 255.0;
   amodel->objects[id-1]->opacity = opacity;
   redraw(id);
}

void GLuciferViewer::redraw(unsigned int id)
{
   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->redrawObject(id);
}

//Called when model loaded/changed, updates all views and window settings
void GLuciferViewer::resetViews(bool autozoom)
{
   //if (!aview) viewSelect(0);  //Switch to the first loaded viewport
   //viewSelect(view); //Re-select viewport - done in viewModel anyway

   //Setup view(s) for new model dimensions
   if (!viewPorts || awin->views.size() == 1)
      //Current view only
      viewModel(view, autozoom);
   else
      //All viewports...
      for (unsigned int v=0; v < awin->views.size(); v++)
         viewModel(v, autozoom);

   //Flag redraw required
   redrawViewports();

   //Copy window title
   sprintf(viewer->title, "%s timestep %d", awin->name.c_str(), timestep);
   if (viewer->isopen && viewer->visible)  viewer->show(); //Update title etc
   viewer->setBackground(awin->background.value); //Update background colour
}

//Called when all viewports need to update
void GLuciferViewer::redrawViewports()
{
   for (unsigned int v=0; v<awin->views.size(); v++)
      awin->views[v]->redraw = true;

   //Flag redraw on objects??
   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->redraw = true;
}

//Called when view changed
void GLuciferViewer::viewSelect(int idx)
{
   if (awin->views.size() == 0) abort_program("No views available!");
   view = idx;
   if (view < 0) view = awin->views.size() - 1;
   if (view >= (int)awin->views.size()) view = 0;

   aview = awin->views[view];

   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->setView(aview);
}

//Called when timestep/window changed (new model data)
//Set model size from geometry / bounding box and apply auto zoom
void GLuciferViewer::viewModel(int idx, bool autozoom)
{
   //Ensure correct view is selected
   viewSelect(idx);
   if (viewPorts)
      //Set viewport based on window size
      aview->port(viewer->width, viewer->height);
   else
      //Set viewport to entire window
      aview->port(0, 0, viewer->width, viewer->height);

   // Apply initial autozoom if set (only applied based on provided dimensions)
   //if (autozoom && aview->zoomstep == 0)
   //   aview->init(false, awin->min, awin->max);

   //Call check on window min/max coords
   if (awin->min[0] != awin->max[0] || awin->min[1] != awin->max[1] || awin->min[2] != awin->max[2])
   {
      Geometry::checkPointMinMax(awin->min);
      Geometry::checkPointMinMax(awin->max);
   }

   //Set the model bounding box
   aview->init(false, Geometry::min, Geometry::max);

   // Apply step autozoom if set (applied based on detected bounding box)
   if (autozoom && aview->zoomstep > 0 && timestep % aview->zoomstep == 0)
       aview->zoomToFit();
}

int GLuciferViewer::viewFromPixel(int x, int y)
{
   // Select a viewport by mouse position, leave unchanged if pixel out of range
   for (unsigned int v=0; v<awin->views.size(); v++)
      //Viewport coords opposite to windowing system coords, flip y
      if (awin->views[v]->hasPixel(x, viewer->height - y)) return v;
   return view;
}

//Adds colourmap to active model
void GLuciferViewer::addColourMap(ColourMap* cmap)
{
   ColourMap* colourMap = cmap;
   //Save colour map in list
   amodel->colourMaps.push_back(colourMap);
}

void GLuciferViewer::clearObjects(bool all)
{
   //Cache currently loaded data before clearing
   if (amodel->lastStep() < 0) return;
   amodel->cacheStep(timestep, geometry);

   if (FloatValues::membytes > 0)
      debug_print("Clearing geometry, geom memory usage before clear %.3f mb\n", FloatValues::membytes/1000000.0f);
   //Clear containers...
   for (unsigned int i=0; i < geometry.size(); i++)
   {
      geometry[i]->redraw = true;
      geometry[i]->clear(all);
   }
}

void GLuciferViewer::redrawObjects()
{
   //Flag redraw on all objects...
   for (unsigned int i=0; i < geometry.size(); i++)
      geometry[i]->redraw = true;
}


// Render
void GLuciferViewer::display(void)
{
   clock_t t1 = clock();

   //if (viewer->mouseState)
   //   sort = 0;

   // Save last drawn view so we know whether it needs to be re-rendered
   static unsigned int lastdraw = 1;

   //Always redraw the active view, others only if flag set
   if (aview)
   {
      aview->redraw = true;
      if (!viewer->mouseState && sort_on_rotate && aview->rotated)
         aview->sort = true;
   }

   //Turn filtering of objects on/off
   if (awin->views.size() > 1 || windows.size() > 1)
   {
      for (unsigned int v=0; v<awin->views.size(); v++)
         awin->views[v]->filtered = !viewAll;
   }
   else  //Single viewport, always disable filter
      awin->views[0]->filtered = false;

   if (!viewPorts || awin->views.size() == 1)
   {
      //Current view only
      displayCurrentView();
   }
   else
   {
      //Process all viewports...
      int selview = view;
      for (unsigned int v=0; v < awin->views.size(); v++)
      {
         if (!awin->views[v]->redraw) continue;
         //All objects need redrawing if the viewport changed
         if (lastdraw != v) redrawObjects();
         awin->views[v]->redraw = false;

         view = v;
         lastdraw = v;

         displayCurrentView();
      }

      //Restore selected
      viewSelect(selview);
   }

   //Print current info message (displayed for one frame only)
   displayMessage();

   //Display object list if enabled
   if (objectlist)
     displayObjectList(false);

   double time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
   if (time > 0.1)
     debug_print("%.4lf seconds to render scene\n", time);

   aview->sort = false;
}

void GLuciferViewer::displayCurrentView()
{
   GL_Error_Check;
   viewSelect(view);
   GL_Error_Check;

#ifdef USE_OMEGALIB
   drawSceneBlended();
   return;
#endif

   if (viewPorts)
      //Set viewport based on window size
      aview->port(viewer->width, viewer->height);
   else
      //Set viewport to entire window
      aview->port(0, 0, viewer->width, viewer->height);

   // Clear viewport 
   GL_Error_Check;
   glDrawBuffer(viewer->renderBuffer);
   GL_Error_Check;
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   GL_Error_Check;

   // View transform
   //   int v = view;
   //debug_print("### Displaying viewport %s (%d) at %d,%d %d x %d\n", views[v]->title, v, (int)(width * views[v]->x), (int)(height * views[v]->y), (int)(width * views[v]->w), (int)(height * views[v]->h));
   if (aview->autozoom)
   {
      aview->projection(EYE_CENTRE);
   GL_Error_Check;
      aview->zoomToFit(0);
   GL_Error_Check;
   }
   else
      aview->apply();
   GL_Error_Check;

   /*/ set up clip planes, x y & z, + and -
   double eqn1[]={1.0,0.0,0.0,-0.5};
   glClipPlane(GL_CLIP_PLANE1, eqn1);
   glEnable(GL_CLIP_PLANE1);

   double eqn2[]={0.0,-1.0,0.0,0.5};
   glClipPlane(GL_CLIP_PLANE2, eqn2);
   glEnable(GL_CLIP_PLANE2);

   double eqn3[]={0.0,0.0,1.0,0.5};
   glClipPlane(GL_CLIP_PLANE3, eqn3);
   glEnable(GL_CLIP_PLANE3);
   */

   if (aview->stereo)
   {
      if (viewer->stereoBuffer)
      {
         // Draw to the left buffer
         glDrawBuffer(GL_LEFT);
         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      }
      else
      {
         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
         // Apply red filter for left eye 
         if (viewer->background.value < viewer->inverse.value)
            glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
         else  //Use opposite mask for light backgrounds
            glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
      }

      // Render the left-eye view 
      aview->projection(EYE_LEFT);
      aview->apply();
      // Draw scene
      drawSceneBlended();
      aview->drawOverlay(viewer->inverse, amodel->timeStamp(timestep));

      if (viewer->stereoBuffer)
      {
         // Draw to the right buffer
         glDrawBuffer(GL_RIGHT);
         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      }
      else
      {
         // Clear the depth buffer so red/cyan components are blended 
         glClear(GL_DEPTH_BUFFER_BIT);
         // Apply cyan filter for right eye 
         if (viewer->background.value < viewer->inverse.value)
            glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
         else  //Use opposite mask for light backgrounds
            glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
      }

      // Render the right-eye view 
      aview->projection(EYE_RIGHT);
      aview->apply();
      // Draw scene
      drawSceneBlended();
      aview->drawOverlay(viewer->inverse, amodel->timeStamp(timestep));

      // Restore full-colour 
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); 
   }
   else
   {
      // Default non-stereo render
   GL_Error_Check;
      aview->projection(EYE_CENTRE);
   GL_Error_Check;
      drawSceneBlended();
   GL_Error_Check;
      aview->drawOverlay(viewer->inverse, amodel->timeStamp(timestep));
   GL_Error_Check;
   }

   glDisable(GL_CLIP_PLANE1);
   glDisable(GL_CLIP_PLANE2);
   glDisable(GL_CLIP_PLANE3);

   //Clear the rotation flag
   if (aview->sort) aview->rotated = false;
}

void GLuciferViewer::displayObjectList(bool console)
{
   //Print available objects by id to screen and stderr
   int offset = 0;
   if (console) std::cerr << "------------------------------------------" << std::endl;
   for (unsigned int i=0; i < amodel->objects.size(); i++)
   {
      if (amodel->objects[i])
      {
         std::ostringstream ss;
         ss << std::setw(5) << amodel->objects[i]->id << " : " << amodel->objects[i]->name;
         if (amodel->objects[i]->skip)
         {
            if (console) std::cerr << "[ no data  ]" << ss.str() << std::endl;
            displayText(ss.str(), ++offset, 0xff222288);
         }
         else if (amodel->objects[i]->visible)
         {
            if (console) std::cerr << "[          ]" << ss.str() << std::endl;
            //Use object colour if provided, unless matches background
            int colour = 0;
            if (amodel->objects[i]->colour.value != viewer->background.value)
               colour = amodel->objects[i]->colour.value;
            displayText(ss.str(), ++offset, colour);
         }
         else
         {
            if (console) std::cerr << "[  hidden  ]" << ss.str() << std::endl;
            displayText(ss.str(), ++offset, 0xff888888);
         }
      }
   }
   if (console) std::cerr << "------------------------------------------" << std::endl;
}

void GLuciferViewer::printMessage(const char *fmt, ...)
{
   if (fmt)
   {
      va_list ap;                 // Pointer to arguments list
      va_start(ap, fmt);          // Parse format string for variables
      vsprintf(message, fmt, ap);    // Convert symbols
      va_end(ap);
   }
}

void GLuciferViewer::displayMessage()
{
   if (strlen(message))
   {
      //Set viewport to entire window
      glViewport(0, 0, viewer->width, viewer->height);
      glScissor(0, 0, viewer->width, viewer->height);
      Viewport2d(viewer->width, viewer->height);

      //Print in XOR to display against any background
      Colour_SetXOR(true);

      //Print current message
      Print(10, 10, 1.0, message);
      message[0] = '\0';

      //Revert to normal colour
      Colour_SetXOR(false);
      PrintSetColour(viewer->inverse.value);

      Viewport2d(0, 0);
   }
}

void GLuciferViewer::displayText(std::string text, int lineno, int colour)
{
   //Set viewport to entire window
   glViewport(0, 0, viewer->width, viewer->height);
   glScissor(0, 0, viewer->width, viewer->height);
   Viewport2d(viewer->width, viewer->height);

    std::stringstream ss(text);
    std::string line;
    while(std::getline(ss, line))
    {
       //Shadow
       PrintSetColour(viewer->background.value);
       Print(11, viewer->height - 15*lineno - 1, 0.6, line.c_str());

       if (colour == 0)
          PrintSetColour(viewer->inverse.value);
       else
          PrintSetColour(colour | 0xff000000);

       Print(10, viewer->height - 15*lineno, 0.6, line.c_str());
       lineno++;
    }

   //Revert to normal colour
   PrintSetColour(viewer->inverse.value);

   Viewport2d(0, 0);
}

void GLuciferViewer::drawSceneBlended()
{
   switch (viewer->blend_mode) {
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
   }
}

void GLuciferViewer::drawScene()
{
   if (!aview->antialias)
      glDisable(GL_MULTISAMPLE);
   else
      glEnable(GL_MULTISAMPLE);

   // Restore default state
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   glEnable(GL_LIGHTING);
   glDisable(GL_CULL_FACE);
   glDisable(GL_TEXTURE_2D);

   //For some bizarre reason, drawing the border box first on Mac OpenGL breaks the lighting
   //I hereby dedicate this comment as a monument to the 10 hours I lost tracking down this bug...
   //(Drawing border last creates aliasing around transparent objects, moving back for now, is it only a GLUT problem?)
   aview->drawBorder();

   triSurfaces->draw();
   quadSurfaces->draw();
   volumes->draw();
   points->draw();
   vectors->draw();
   tracers->draw();
   shapes->draw();
   lines->draw();
   labels->draw();

}

bool GLuciferViewer::loadModel(std::string& f)
{
   FilePath fn(f);
   if (fn.ext != "gldb" && fn.ext != "db")
   {
      //Not a db file? Store other in files list - height maps etc
      files.push_back(fn);
      return false;
   }

   //Open database file
   amodel = new Model(fn);
   models.push_back(amodel);

   if (!amodel->open()) abort_program("Model database open failed\n");

   //amodel->loadTimeSteps();
   amodel->loadColourMaps();
   amodel->loadObjects();
   amodel->loadViewports();
   amodel->loadWindows();

   //No windows?
   if (amodel->windows.size() == 0)
   {
      //Set a default window, viewport & camera
      awin = new Win(fn.base.c_str());
      aview = awin->addView(new View(fn.base.c_str()));
      //Add objects to window & viewport
      for (unsigned int o=0; o<amodel->objects.size(); o++)
      {
         //Model objects stored by object ID so can have gaps...
         if (!amodel->objects[o]) continue;
         aview->addObject(amodel->objects[o]);
         awin->addObject(amodel->objects[o]);
         amodel->loadLinks(amodel->objects[o]);
      }
      //Add window to global window list
      amodel->windows.push_back(awin);
   }

   //Add all windows to global window list
   for (unsigned int w=0; w<amodel->windows.size(); w++)
      windows.push_back(amodel->windows[w]);
   //Use width of first window for now
   //viewer->width = windows[0]->width;
   //viewer->height = windows[0]->height;
   return true;
}

//Load model window at specified timestep
bool GLuciferViewer::loadWindow(int window_idx, int at_timestep, bool autozoom)
{
   if (windows.size() == 0)
   {
      //Height maps can be loaded here...
      if (files.size() > 0)
      {
         for (unsigned int m=0; m < files.size(); m++)
            readHeightMap(files[m]);
         files.clear();
      }
      else
      {
         //No model, show a demo
         createDemoModel();
      }
   }
   //else
      //Clear current model data
      //clearObjects();

   if (window_idx >= (int)windows.size()) return false;

   //Resized from last window? close so will be re-opened at new size
   if (awin && (awin->width != windows[window_idx]->width || awin->height != windows[window_idx]->height && viewer->isopen))
      viewer->close();

   //Save active window as selected
   awin = windows[window_idx];
   window = window_idx;

   // Ensure correct model is selected
   for (unsigned int m=0; m < models.size(); m++)
      for (unsigned int w=0; w < models[m]->windows.size(); w++)
         if (models[m]->windows[w] == awin) amodel = models[m];

   if (amodel->objects.size() == 0) return false;

   //Set timestep and load geometry at that step
   if (amodel->db)
   {
      setTimeStep(at_timestep);
      debug_print("Loading vis '%s', timestep: %d\n", awin->name.c_str(), timestep);
   }

   //Height maps can be loaded here...
   for (unsigned int m=0; m < files.size(); m++)
      readHeightMap(files[m]);

   //Not yet opened or resized?
   if (!viewer->isopen)
   {
      if (fixedwidth > 0 && fixedheight > 0)
         viewer->open(fixedwidth, fixedheight);
      else
         viewer->open(windows[window_idx]->width, windows[window_idx]->height);
   }

   //Update the views
   resetViews(autozoom);

   //Script files?
   for (unsigned int m=0; m < files.size(); m++)
      readScriptFile(files[m]);


   //Cache fill (if cache large enough for all data)
   if (GeomCache::size >= amodel->timesteps.size())
   {
      printf("Caching all geometry data...\n");
      for (int i=0; i<=amodel->timesteps.size(); i++)
      {
         printf("%d...%d\n", i, timestep);
         setTimeStep(timestep+1);
      }
      setTimeStep(at_timestep);
   }

   return true;
}

//Load data at specified timestep for selected model
int GLuciferViewer::setTimeStep(int ts)
{
   clock_t t1 = clock();
   unsigned int idx=0;
   if (windows.size() == 0) return -1;

   int step = amodel->nearestTimeStep(ts, timestep);
   if (step < 0) return -1;
   //Clear currently loaded (also caches if enabled)
   clearObjects();

   //Closest matching timestep >= requested
   timestep = step;
   debug_print("TimeStep set to: %d\n", timestep);

   //Detach any attached db file and attach n'th timestep database if available
   amodel->attach(timestep);

   //Attempt to load from cache first
   if (amodel->restoreStep(timestep, geometry)) return 0; //Cache hit successful return value

   //clearObjects();
   //noload flag skips loading geometry until "load" commands issued
   int rows = 0;
   for (unsigned int i=0; i<awin->objects.size(); i++)
   {
      if (awin->objects[i] && (!noload || !awin->objects[i]->skip))
         rows += loadGeometry(awin->objects[i]->id);
   } 

   debug_print("%.4lf seconds to load %d geometry records from database\n", (clock()-t1)/(double)CLOCKS_PER_SEC, rows);
   return rows;
}

int GLuciferViewer::loadGeometry(int object_id)
{
   //Load at current timestep
   return loadGeometry(object_id, timestep, timestep, true);
}

int GLuciferViewer::loadGeometry(int object_id, int time_start, int time_stop, bool recurseTracers)
{
   clock_t t1 = clock();
   char* prefix = amodel->prefix;
   //Setup filters
   char filter[64] = "";
   //if (rankfilter >= 0) sprintf(filter, "and rank=%d", rankfilter);

   //Load geometry
   char SQL[1024];

   int datacol = 20;
   //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
   //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, labels, 
   //minX, minY, minZ, maxX, maxY, maxZ, data)
   sprintf(SQL, "SELECT id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,minX,minY,minZ,maxX,maxY,maxZ,data FROM %sgeometry WHERE object_id=%d AND timestep BETWEEN %d AND %d %s ORDER BY idx,rank", prefix, object_id, time_start, time_stop, filter);
   sqlite3_stmt* statement = amodel->select(SQL, true);

   //Old database compatibility
   if (statement == NULL)
   {
      //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
      //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, data)
      sprintf(SQL, "SELECT id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,data FROM %sgeometry WHERE object_id=%d AND timestep BETWEEN %d AND %d %s ORDER BY idx,rank", prefix, object_id, time_start, time_stop, filter);
      sqlite3_stmt* statement = amodel->select(SQL, true);
      datacol = 14;

      //Fix
#ifdef ALTER_DB
      amodel->reopen(true);  //Open writable
      sprintf(SQL, "ALTER TABLE %sgeometry ADD COLUMN minX REAL; ALTER TABLE %sgeometry ADD COLUMN minY REAL; ALTER TABLE %sgeometry ADD COLUMN minZ REAL; "
                   "ALTER TABLE %sgeometry ADD COLUMN maxX REAL; ALTER TABLE %sgeometry ADD COLUMN maxY REAL; ALTER TABLE %sgeometry ADD COLUMN maxZ REAL; ",
                   prefix, prefix, prefix, prefix, prefix, prefix, prefix);
      printf("%s\n", SQL);
      amodel->issue(SQL);
#endif
   }

   //Very old database compatibility
   if (statement == NULL)
   {
      sprintf(SQL, "SELECT id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,data FROM %sgeometry WHERE object_id=%d AND timestep BETWEEN %d AND %d %s ORDER BY idx,rank", prefix, object_id, time_start, time_stop, filter);
      statement = amodel->select(SQL);
      datacol = 13;
   }

   if (!statement) return 0;
   int rows = 0;
   int tbytes = 0;
   int ret;
   Geometry* active = NULL;
   do 
   {  
      ret = sqlite3_step(statement);
      if (ret == SQLITE_ROW)
      {
         rows++;
         int id = sqlite3_column_int(statement, 0);
         int timestep = sqlite3_column_int(statement, 1);
         //int rank = sqlite3_column_int(statement, 2);  //unused
         //int index = sqlite3_column_int(statement, 3); //unused
         lucGeometryType type = (lucGeometryType)sqlite3_column_int(statement, 4);
         if (filters && !filters[type]) continue;   //Skip filtered
         lucGeometryDataType data_type = (lucGeometryDataType)sqlite3_column_int(statement, 5);
         int size = sqlite3_column_int(statement, 6);
         int count = sqlite3_column_int(statement, 7);
         int items = count / size;
         int width = sqlite3_column_int(statement, 8);
         int height = width > 0 ? items / width : 0;
         float minimum = (float)sqlite3_column_double(statement, 9);
         float maximum = (float)sqlite3_column_double(statement, 10);
         //New fields for the scaling features, applied when drawing colour bars
         float dimFactor = (float)sqlite3_column_double(statement, 11);
         const char *units = (const char*)sqlite3_column_text(statement, 12);

         const char *labels = datacol < 14 ? "" : (const char*)sqlite3_column_text(statement, 13);

         const void *data = sqlite3_column_blob(statement, datacol);
         unsigned int bytes = sqlite3_column_bytes(statement, datacol);

         if (type == lucTracerType) height = 0;

         //Create object and set parameters
         active = geometry[type];

         if (recurseTracers && type == lucTracerType)
         {
            //if (datacol == 13) continue;  //Don't bother supporting tracers from old dbs
            //Only load tracer timesteps when on the vertex data object or will repeat for every type found
            if (data_type != lucVertexData) continue;

            //Tracers are loaded with a new select statement across multiple timesteps...
            //objects[object_id]->steps = timestep+1;
            tracers->timestep = timestep; //Set current timestep for tracers
            int stepstart = 0; //timestep - objects[object_id]->steps;
            //int stepstart = timestep - tracers->steps;

            loadGeometry(object_id, stepstart, timestep, false);
         }
         else
         {
            unsigned char* buffer = NULL;
            if (bytes != (unsigned int)(count * sizeof(float)))
            {
               //Decompress!
               unsigned long dst_len = (unsigned long )(count * sizeof(float));
               unsigned long uncomp_len = dst_len;
               unsigned long cmp_len = bytes;
               buffer = new unsigned char[dst_len];
               if (!buffer)
                  abort_program("Out of memory!\n");

#ifdef USE_ZLIB
               int res = uncompress(buffer, &uncomp_len, (const unsigned char *)data, cmp_len);
               if (res != Z_OK || dst_len != uncomp_len)
#else
               int res = tinfl_decompress_mem_to_mem(buffer, uncomp_len, (const unsigned char *)data, cmp_len, TINFL_FLAG_PARSE_ZLIB_HEADER);
               if (!res)
#endif
               {
                  abort_program("uncompress() failed! error code %d\n", res);
                  //abort_program("uncompress() failed! error code %d expected size %d actual size %d\n", res, dst_len, uncomp_len);
               }
               data = buffer; //Replace data pointer
               bytes = uncomp_len;
            }

            tbytes += bytes;   //Byte counter

            //Where min/max vertex provided, load
            if (data_type == lucVertexData)
            {
               float min[3] = {0,0,0}, max[3] = {0,0,0};
               if (datacol > 14 && sqlite3_column_type(statement, 14) != SQLITE_NULL)
               {
                  for (int i=0; i<3; i++)
                  {
                     min[i] = (float)sqlite3_column_double(statement, 14+i);
                     max[i] = (float)sqlite3_column_double(statement, 17+i);
                  }
               }

               //Detect null dims data due to bugs in dimension output
               if (min[0] != max[0] || min[1] != max[1] || min[2] != max[2])
               {
                  Geometry::checkPointMinMax(min);
                  Geometry::checkPointMinMax(max);
               }
               else
               {
                  //Slow way, detects bounding box by checking each vertex
                  for (int p=0; p < items*3; p += 3)
                     Geometry::checkPointMinMax((float*)data + p);

                  //Fix for future loads
#ifdef ALTER_DB
                  amodel->reopen(true);  //Open writable
                  sprintf(SQL, "UPDATE %sgeometry SET minX = '%f', minY = '%f', minZ = '%f', maxX = '%f', maxY = '%f', maxZ = '%f' WHERE id==%d;", 
                          prefix, id, Geometry::min[0], Geometry::min[1], Geometry::min[2], Geometry::max[0], Geometry::max[1], Geometry::max[2]);
                  printf("%s\n", SQL);
                  amodel->issue(SQL);
#endif
               }
            }

            //Always add a new element for each new vertex geometry record, not suitable if writing db on multiple procs!
            if (data_type == lucVertexData && recurseTracers) active->add(amodel->objects[object_id-1]);

            //Read data block
            active->read(amodel->objects[object_id-1], items, data_type, data, width, height);
            active->setup(amodel->objects[object_id-1], data_type, minimum, maximum, dimFactor, units);
            if (labels) active->label(amodel->objects[object_id-1], labels);

            if (buffer) delete[] buffer;
   #if 0
            char* types[8] = {"", "POINTS", "GRID", "TRIANGLES", "VECTORS", "TRACERS", "LINES", "SHAPES"};
            if (data_type == lucVertexData)
               printf("[object %d time %d] Read %d vertices into %s object (idx %d) %d x %d\n", 
                        object_id, timestep, items, types[type], active->size()-1, width, height);
            else printf("[object %d time %d] Read %d values of dtype %d into %s object (idx %d) min %f max %f\n", 
                        object_id, timestep, items, data_type, types[type], active->size()-1, minimum, maximum);
            if (labels) printf(labels);
   #endif
         }
      }
      else if (ret != SQLITE_DONE)
         printf("DB STEP FAIL %d %d\n", ret, (ret>>8));
   } while (ret == SQLITE_ROW);

   sqlite3_finalize(statement);
   debug_print("... loaded %d rows, %d bytes, %.4lf seconds\n", rows, tbytes, (clock()-t1)/(double)CLOCKS_PER_SEC);

   return rows;
}

int GLuciferViewer::decompressGeometry(int object_id, int timestep)
{
   amodel->reopen(true);  //Open writable
   clock_t t1 = clock();
   //Load geometry
   char SQL[1024];

   sprintf(SQL, "SELECT id,count,data FROM %sgeometry WHERE object_id=%d AND timestep=%d ORDER BY idx,rank", amodel->prefix, object_id, timestep);
   sqlite3_stmt* statement = amodel->select(SQL, true);

   if (!statement) return 0;
   int rows = 0;
   int tbytes = 0;
   int ret;
   Geometry* active = NULL;
   do 
   {  
      ret = sqlite3_step(statement);
      if (ret == SQLITE_ROW)
      {
         rows++;
         int id = sqlite3_column_int(statement, 0);
         int count = sqlite3_column_int(statement, 1);
         const void *data = sqlite3_column_blob(statement, 2);
         unsigned int bytes = sqlite3_column_bytes(statement, 2);

         unsigned char* buffer = NULL;
         if (bytes != (unsigned int)(count * sizeof(float)))
         {
            //Decompress!
            unsigned long dst_len = (unsigned long )(count * sizeof(float));
            unsigned long uncomp_len = dst_len;
            unsigned long cmp_len = bytes;
            buffer = new unsigned char[dst_len];
            if (!buffer)
               abort_program("Out of memory!\n");

#ifdef USE_ZLIB
            int res = uncompress(buffer, &uncomp_len, (const unsigned char *)data, cmp_len);
            if (res != Z_OK || dst_len != uncomp_len)
#else
            int res = tinfl_decompress_mem_to_mem(buffer, uncomp_len, (const unsigned char *)data, cmp_len, TINFL_FLAG_PARSE_ZLIB_HEADER);
            if (!res)
#endif
            {
               abort_program("uncompress() failed! error code %d\n", res);
               //abort_program("uncompress() failed! error code %d expected size %d actual size %d\n", res, dst_len, uncomp_len);
            }
            data = buffer; //Replace data pointer
            bytes = uncomp_len;
#ifdef ALTER_DB
   //UNCOMPRESSED
   sqlite3_stmt* statement2;
   snprintf(SQL, 1024, "UPDATE %sgeometry SET data = ? WHERE id==%d;", amodel->prefix, id);
   printf("%s\n", SQL);
   /* Prepare statement... */
   if (sqlite3_prepare_v2(amodel->db, SQL, -1, &statement2, NULL) != SQLITE_OK)
   {
      printf("SQL prepare error: (%s) %s\n", SQL, sqlite3_errmsg(amodel->db));
      abort(); //Database errors fatal?
      return 0;
   }
   /* Setup blob data for insert */
   if (sqlite3_bind_blob(statement2, 1, buffer, bytes, SQLITE_STATIC) != SQLITE_OK)
   {
      printf("SQL bind error: %s\n", sqlite3_errmsg(amodel->db));
      abort(); //Database errors fatal?
   }
   /* Execute statement */
   if (sqlite3_step(statement2) != SQLITE_DONE )
   {
      printf("SQL step error: (%s) %s\n", SQL, sqlite3_errmsg(amodel->db));
      abort(); //Database errors fatal?
   }
   sqlite3_finalize(statement2);
#endif
            }

            if (buffer) delete[] buffer;

      }
      else if (ret != SQLITE_DONE)
         printf("DB STEP FAIL %d %d\n", ret, (ret>>8));
   } while (ret == SQLITE_ROW);

   sqlite3_finalize(statement);
   debug_print("... modified %d rows, %d bytes, %.4lf seconds\n", rows, tbytes, (clock()-t1)/(double)CLOCKS_PER_SEC);

   return rows;
}

void GLuciferViewer::writeImages(int start, int end)
{
   writeSteps(true, false, start, end, NULL);
}

void GLuciferViewer::encodeVideo(const char* filename, int start, int end)
{
   writeSteps(false, true, start, end, filename);
}

void GLuciferViewer::writeSteps(bool images, bool video, int start, int end, const char* filename)
{
   if (start > end)
   {
      int temp = start;
      start = end;
      end = temp;
   }
#ifdef HAVE_LIBAVCODEC
   VideoEncoder* encoder = NULL;
   if (video) 
      encoder = new VideoEncoder(filename, viewer->width, viewer->height);
   unsigned char* buffer = new unsigned char[viewer->width * viewer->height * 3];
#else
   if (video)
      std::cout << "Video output disabled, libavcodec not found!" << std::endl;
#endif
   for (int i=start; i<=end; i++)
   {
      //Only load steps that contain geometry data
      if (amodel->hasTimeStep(i))
      {
         setTimeStep(i);
         std::cout << "... Writing timestep: " << timestep << std::endl;
         //Update the views
         resetViews(true);
         viewer->display();
         if (images)
            viewer->snapshot(awin->name.c_str(), timestep);
#ifdef HAVE_LIBAVCODEC
         if (video)
         {
            viewer->pixels(buffer, false, true);
            //bitrate settings?
            encoder->frame(buffer);
         }
#endif
      }
   }
#ifdef HAVE_LIBAVCODEC
   delete[] buffer;
   if (encoder) delete encoder;
#endif
}

void GLuciferViewer::dumpById(unsigned int id)
{
   for (unsigned int i=0; i < amodel->objects.size(); i++)
   {
      if (amodel->objects[i] && !amodel->objects[i]->skip && (id == 0 || amodel->objects[i]->id == id))
      {
         std::string names[] = {"Labels", "Points", "Grid", "Triangles", "Vectors", "Tracers", "Lines", "Shapes"};
         for (int type=lucMinType; type<lucMaxType; type++)
         {
            std::ostringstream ss;
            geometry[type]->dumpById(ss, amodel->objects[i]->id);

            std::string results = ss.str();
            if (results.size() > 0)
            {
               char filename[512];
               sprintf(filename, "%s%s_%s.%05d.csv", viewer->output_path, amodel->objects[i]->name.c_str(),
                                                names[type].c_str(), timestep);
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

void GLuciferViewer::jsonWriteFile(unsigned int id, bool jsonp)
{
   //Write new JSON format objects
   char filename[512];
   char ext[6];
   strcpy(ext, "jsonp");
   if (!jsonp) ext[4] = '\0';
   if (id > 0)
     sprintf(filename, "%s%s_%s.%05d.%s", viewer->output_path, awin->name.c_str(),
                                            amodel->objects[id]->name.c_str(), timestep, ext);
   else
     sprintf(filename, "%s%s.%05d.%s", viewer->output_path, awin->name.c_str(), timestep, ext);
   std::ofstream json(filename);
   if (jsonp) json << "loadData(\n";
   jsonWrite(json, id, true);
   if (jsonp) json << ");\n";
   json.close();
}

void GLuciferViewer::jsonWrite(std::ostream& json, unsigned int id, bool objdata)
{
   //Write new JSON format objects
   float rotate[4], translate[3], focus[3], stereo[3];
   aview->getCamera(rotate, translate, focus);

   json << "{\n  \"options\" :\n  {\n"
        << "    \"pointScale\" : " << points->scale << ",\n"
        << "    \"pointType\" : " << points->pointType << ",\n"
        << "    \"border\" : " << (aview->borderType ? "true" : "false") << ",\n"
        << "    \"opacity\" : " << GeomData::opacity << ",\n"
        << "    \"rotate\" : ["  << rotate[0] << "," << rotate[1] << "," << rotate[2] << "," << rotate[3] << "],\n";
   json << "    \"translate\" : [" << translate[0] << "," << translate[1] << "," << translate[2] << "],\n";
   json << "    \"focus\" : [" << focus[0] << "," << focus[1] << "," << focus[2] << "],\n";
   json << "    \"scale\" : [" << aview->scale[0] << "," << aview->scale[1] << "," << aview->scale[2] << "],\n";
   if (Geometry::min[0] < HUGE_VAL && Geometry::min[1] < HUGE_VAL && Geometry::min[2] < HUGE_VAL)
      json << "    \"min\" : [" << Geometry::min[0] << "," << Geometry::min[1] << "," << Geometry::min[2] << "],\n";
   if (Geometry::max[0] > -HUGE_VAL && Geometry::max[1] > -HUGE_VAL && Geometry::max[2] > -HUGE_VAL)
      json << "    \"max\" : [" << Geometry::max[0] << "," << Geometry::max[1] << "," << Geometry::max[2] << "],\n";
   json << "    \"near\" : " << aview->near_clip << ",\n";
   json << "    \"far\" : " << aview->far_clip << ",\n";
   json << "    \"orientation\" : " << aview->orientation << ",\n";
   json << "    \"background\" : " << awin->background.value << "\n";
   json << "  },\n  \"colourmaps\" :\n  [\n" << std::endl;

   for (unsigned int i = 0; i < amodel->colourMaps.size(); i++)
   {
      json << "    {" << std::endl;
      json << "      \"id\" : " << amodel->colourMaps[i]->id << "," << std::endl;
      json << "      \"name\" : \"" << amodel->colourMaps[i]->name << "\"," << std::endl;
      json << "      \"minimum\" : " << amodel->colourMaps[i]->minimum << "," << std::endl;
      json << "      \"maximum\" : " << amodel->colourMaps[i]->maximum << "," << std::endl;
      json << "      \"log\" : " << amodel->colourMaps[i]->log << "," << std::endl;
      json << "      \"colours\" :\n      [" << std::endl;

      for (unsigned int c=0; c < amodel->colourMaps[i]->colours.size(); c++)
      {
         json << "        {\"position\" : " << amodel->colourMaps[i]->colours[c].position << ",";
         json << " \"colour\" : " << amodel->colourMaps[i]->colours[c].colour.value << "}";
         if (c < amodel->colourMaps[i]->colours.size()-1) json << ",";
         json << std::endl;
      }
      json << "      ]\n    }";

      if (i < amodel->colourMaps.size()-1) json << ",";
      json << std::endl;
   }
   json << "  ],\n  \"objects\" : \n  [" << std::endl;

   bool firstobj = true;
   if (!viewer->visible) aview->filtered = false; //Disable viewport filtering
   for (unsigned int i=0; i < amodel->objects.size(); i++)
   {
      if (amodel->objects[i] && (id == 0 || amodel->objects[i]->id == id))
      {
         //std::string names[] = {"Labels", "Points", "Grid", "Triangles", "Vectors", "Tracers", "Lines", "Shapes"};
         //Only able to dump point/triangle based objects currently:
         std::string names[] = {"", "points", "triangles", "triangles", "", "", "", "", "volume"};
         bool first = true;
         for (int type=lucMinType; type<lucMaxType; type++)
         {
            //Collect vertex/normal/index/value data
            std::ostringstream ss;
            if (objdata)
            {
               //When extracting data, skip objects with no export available or no data returned...
               if (geometry[type]->size() == 0 || names[type].length() == 0) continue;
               geometry[type]->jsonWrite(amodel->objects[i]->id, &ss);
               if (ss.tellp() == (std::streampos)0 || amodel->objects[i]->skip) continue;
            }

            if (!first && !objdata) continue;  //Only output object props
            if (first)
            {
              if (!firstobj) json << ",\n";
              json << "    {" << std::endl;
              json << "      \"id\" : " << amodel->objects[i]->id << "," << std::endl;
              json << "      \"name\" : \"" << amodel->objects[i]->name << "\"," << std::endl;
              json << "      \"visible\": " << (!amodel->objects[i]->skip && amodel->objects[i]->visible ? "true" : "false") << "," << std::endl;
            }
            else
            {
              json << ",";
            }

            int colourmap = -1;
            ColourMap* cmap = amodel->objects[i]->colourMaps[lucColourValueData];
            if (cmap)
            {
               //Search vector
               std::vector<ColourMap*>::iterator it = find(amodel->colourMaps.begin(), amodel->colourMaps.end(), cmap);
               if (it != amodel->colourMaps.end())
                  colourmap = (it - amodel->colourMaps.begin());
            }

            if (amodel->objects[i]->wireframe)
               json << "      \"wireframe\" : " << amodel->objects[i]->wireframe << "," << std::endl;
            if (amodel->objects[i]->cullface)
               json << "      \"cullface\" : " << amodel->objects[i]->cullface << "," << std::endl;
            if (amodel->objects[i]->opacity > 0)
               json << "      \"opacity\" : " << amodel->objects[i]->opacity << "," << std::endl;
            if (amodel->objects[i]->pointSize > 0)
               json << "      \"pointSize\" : " << amodel->objects[i]->pointSize << "," << std::endl;
            if (colourmap >= 0)
               json << "      \"colourmap\" : " << colourmap << "," << std::endl;
            json << "      \"colour\" : " << amodel->objects[i]->colour.value;

            if (objdata) 
            {
               //Write object content
               json << ",\n      \"" << names[type] << "\" : " << std::endl;
               json << "      [" << std::endl;
               json << ss.str();
               json << "\n      ]" << std::endl;
            } else {
               json << std::endl;
            }

            json << "    }";
            firstobj = first = false;
         }
      }
   }
   json << "\n  ]\n}" << std::endl;
}

//Data request from attached apps
std::string GLuciferViewer::requestData(std::string key)
{
   std::ostringstream result;
   if (key == "objects")
   {
      jsonWrite(result);
   }
   printf("%d\n", result.str().length());
   return result.str();
}
