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

//Viewer class
#include "Include.h"
#include "LavaVu.h"
#include "Shaders.h"
#include "VideoEncoder.h"
#include <typeinfo>
#include "tiny_obj_loader.h"

//Viewer class implementation...
LavaVu::LavaVu(std::vector<std::string> args, OpenGLViewer* viewer, int width, int height) : ViewerApp(viewer)
{
   bool output = false, verbose = false, hideall = false, dbpath = false;
   float alpha = 0, subsample = 0;

   //Defaults
   timestep = -1;
   endstep = -1;
   dump = lucExportNone;
   dumpid = 0;
   noload = false;
   viewAll = false;
   viewPorts = true;
   writeimage = writemovie = false;
   sort_on_rotate = true;
   message[0] = '\0';
   volres[0] = volres[1] = volres[2] = 256;
   volmin[0] = volmin[1] = volmin[2] = -1;
   volmax[0] = volmax[1] = volmax[2] = 1;

   fixedwidth = width;
   fixedheight = height;

   window = 0;
   tracersteps = 0;
   objectlist = noload;

   view = -1;
   aview = NULL;
   awin = NULL;
   amodel = NULL;
   aobject = NULL;

   //A set of default arguments can be stored in a file...
   std::ifstream argfile("gLucifer_args.cfg");
   if (argfile) //Found a config file, load arguments
   {
      std::string line;
      while (std::getline(argfile, line))
         if (line.length() > 0 && line.at(0) != '#')
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
         case 'P':
            if (args[i].length() > 2)
              ss >> subsample;
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
            dump = lucExportCSV;
            viewer->visible = false;
            break;
         case 'J':
            if (args[i].length() > 2) ss >> dumpid;
            dump = lucExportJSONP;
            viewer->visible = false;
            break;
         case 'j':
            if (args[i].length() > 2) ss >> dumpid;
            dump = lucExportJSON;
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
            std::cerr << "Ignoring shader path, using current directory\n";
            Shader::path = NULL;
            break;
         case 'V':
            ss >> volres[0] >> x >> volres[1] >> x >> volres[2];
            break;
         case 'D':
            ss >> volmin[0] >> x >> volmin[1] >> x >> volmin[2];
            ss >> x >> volmax[0] >> x >> volmax[1] >> x >> volmax[2];
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
         FilePath fn(args[i]);
         if (fn.type != "gldb" && fn.type != "db")
         {
            //Not a db file? Store other in files list - height maps etc
            files.push_back(fn);
         }
         else
         {
            //Load all objects in db
            loadModel(fn, hideall);
            //Save path of first sucessfully loaded model
            if (dbpath && viewer->output_path.length() == 0)
            {
               viewer->output_path = fn.path;
               debug_print("Output path set to %s\n", viewer->output_path.c_str());
            }
         }
      }
   }
   
   if (!amodel)
   {
      //Adds a default model, window & viewport
      FilePath fn = FilePath(":memory");
      amodel = new Model(fn, hideall);
      models.push_back(amodel);

      //Set a default window, viewport & camera
      awin = new Win("LavaVu");
      aview = awin->addView(new View());
      windows.push_back(awin);
      amodel->windows.push_back(awin);
      
      //Setup default colourmaps
      amodel->initColourMaps();
   }

   //Set script output flag
   this->output = output;
   //Opacity override: either [0,1] or (1,255]
   if (alpha > 1.0) alpha /= 255.0;
   GeomData::opacity = alpha;
   //Points initial sub-sampling
   if (subsample) Points::subSample = subsample;

}

LavaVu::~LavaVu()
{
   //Kill all models
   for (unsigned int i=0; i < models.size(); i++)
      delete models[i];

   //Clear windows
   for(unsigned int i=0; i<windows.size(); i++)
      if (windows[i]) delete windows[i]; 

   debug_print("Peak geometry memory usage: %.3f mb\n", FloatValues::mempeak/1000000.0f);
}

void LavaVu::run(bool persist)
{
   if (persist)
   {
     //Server mode, disable viewports
     viewAll = true;
     viewPorts = false;
   }

   if (writeimage || writemovie || dump > lucExportCSV)
   {
      //Load vis data for each window and write image
      int win = 0;
      if (!writeimage && !writemovie) viewer->isopen = true; //Skip open
      while (loadWindow(win, timestep, true) > 0)
      {
         //Bounds checks
         if (endstep < timestep) endstep = timestep;
         int final = amodel->lastStep();
         if (endstep < final) final = endstep;

         if (writeimage || writemovie)
         {
            resetViews(true);
            viewer->display();
            //Script files?
            runScripts();
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
         if (dump == lucExportJSON)
            jsonWriteFile(dumpid);
         else if (dump == lucExportJSONP)
            jsonWriteFile(dumpid, true);

         win++;
      }
   }
   else
   {
      //Load vis data for first window
      if (!loadWindow(0, timestep, true))
         abort_program("Model file load error, no window data\n");

      runScripts();
   }

   //Dump csv data if requested
   if (dump == lucExportCSV)
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

void LavaVu::runScripts()
{
   //Script files?
   for (unsigned int m=0; m < files.size(); m++)
   {
      std::cerr << files[m].full << std::endl;
      if (files[m].type == "script")
         parseCommands("script " + files[m].full);
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
   if (aobject)
   {
      jsonParseProperty(data, aobject->properties);
      std::cerr << "OBJECT " << aobject->name << ", DATA: " << json::Serialize(aobject->properties) << std::endl;
   }
   else if (aview)
   {
      jsonParseProperty(data, aview->properties);
      std::cerr << "VIEW: " << json::Serialize(aview->properties) << std::endl;
   }
   else
   {
      jsonParseProperty(data, globals);
      std::cerr << "DATA: " << json::Serialize(globals) << std::endl;
   }
}

void LavaVu::printProperties()
{
   //Show properties of selected object or view/globals
   if (aobject)
      std::cerr << "OBJECT " << aobject->name << ", DATA: " << json::Serialize(aobject->properties) << std::endl;
   else if (aview)
      std::cerr << "VIEW: " << json::Serialize(aview->properties) << std::endl;
   else
      std::cerr << "DATA: " << json::Serialize(globals) << std::endl;
}

void LavaVu::readVolume(FilePath& fn)
{
   //Check for raw format volume data
   if (fn.type != "raw") return;

   awin->background.value = 0xff000000;
   
   Geometry::checkPointMinMax(volmin);
   Geometry::checkPointMinMax(volmax);
      
      ColourMap* colourMap = NULL;
      /*/Demo colourmap
      ColourMap* colourMap = new ColourMap();
      addColourMap(colourMap);
      //Colours: hex, abgr
      unsigned int colours[] = {0x00000000, 0xffffffff};
      colourMap->add(colours, 2);*/
      
   //Create volume object
   DrawingObject *vobj = newObject(fn.base, true, 0xff000000, colourMap, 1.0, "");
     
   std::cerr << "LOADING ... " << fn.full << std::endl;
   std::fstream file(fn.full.c_str(), std::ios::in | std::ios::binary);
   file.seekg(0, std::ios::end);
   std::streamsize size = file.tellg();
   file.seekg(0, std::ios::beg);
   if (!file.is_open() || size <= 0) abort_program("File error %s\n", fn.full.c_str());
   std::vector<char> buffer(size);
   file.read(buffer.data(), size);
   file.close();

   //Define the bounding cube by corners
   Model::volumes->add(vobj);
   Model::volumes->read(vobj, 1, lucVertexData, volmin);
   Model::volumes->read(vobj, 1, lucVertexData, volmax);
   float floatcount = size / 4.0;
   Model::volumes->read(vobj, floatcount, lucColourValueData, buffer.data(), volres[0], volres[1], volres[2]);
   Model::volumes->setup(vobj, lucColourValueData, 0, 1);
}

void LavaVu::readVolumeSlice(FilePath& fn)
{
   //Check for jpg data
   if (fn.type != "jpg" && fn.type != "jpeg" && fn.type != "png") return;

   awin->background.value = 0xff000000;
   
   ColourMap* colourMap = NULL;
      
   Geometry::checkPointMinMax(volmin);
   Geometry::checkPointMinMax(volmax);
      
   //Create volume object
   static DrawingObject *vobj = NULL;
   static int count = 0;
   if (!vobj)
   {
      vobj = newObject(fn.base, true, 0xff000000, colourMap, 1.0, "");
      //Define the bounding cube by corners
      Model::volumes->add(vobj);
      Model::volumes->read(vobj, 1, lucVertexData, volmin);
      Model::volumes->read(vobj, 1, lucVertexData, volmax);    
   }
   
   std::cerr << "LOADING ... " << fn.full << std::endl;
   count++;

   int width, height, bytesPerPixel, bpp;
   GLubyte* imageData;
   if (fn.type == "png")
   {
      GLuint uwidth, uheight, ubpp;
      std::ifstream file(fn.full.c_str(), std::ios::binary);
      if (!file) abort_program("Cannot open '%s'\n", fn.full.c_str());
      imageData = (GLubyte*)read_png(file, ubpp, uwidth, uheight);
      file.close();
      bytesPerPixel = ubpp/8;
      width = uwidth;
      height = uheight;
   }
   else
   {
      imageData = (GLubyte*)jpgd::decompress_jpeg_image_from_file(fn.full.c_str(), &width, &height, &bytesPerPixel, 3);
      bytesPerPixel = 3;
   }
   
   if (imageData)
   {
      //Convert to luminance (just using red channel now, other options in future)
      GLubyte* luminance = new GLubyte[width*height];
      for (int y=0; y<height; y++)
          for (int x=0; x<width; x++)
            luminance[y*width+x] = imageData[(y*width+x)*bytesPerPixel];
      
      int floatcount = (width * height) / sizeof(float);
      Model::volumes->read(vobj, floatcount, lucColourValueData, luminance, width, height, count);
      Model::volumes->setup(vobj, lucColourValueData, 0, 1);
      
      delete[] luminance;
      delete[] imageData;
   }
   else
     debug_print("Slice load failed: %s\n", fn.full.c_str());
}

void LavaVu::readHeightMap(FilePath& fn)
{
   int rows, cols, size = 2, subsample;
   int byteorder = 0;   //Little endian default
   float xmap, ymap, xdim, ydim;
   int geomtype = lucTriangleType, header = 0;
   float downscale = 1;
   std::string texfile;

   //Can only parse dem format wth ers or hdr header
   if (fn.type != "dem") return;

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

   float* min = awin->min;
   float* max = awin->max;
   awin->background.value = 0xff000000;
   float range[3] = {0,0,0};

   min[0] = xmap;
   max[0] = xmap + (cols * xdim);
   max[1] = 1000;
   min[2] = ymap - (rows * xdim); //Northing decreases with latitude
   max[2] = ymap;
   range[0] = fabs(max[0] - min[0]);
   range[2] = fabs(max[2] - min[2]);
   
   viewer->title = fn.base;

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
   std::string props = "cullface=0\ntexturefile=%s\n" + texfile;
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

   //Sea grid points
   vertex[0] = min[0];
   vertex[1] = 0;
   vertex[2] = min[2];
   Model::quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);
   vertex[0] = max[0];
   Model::quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);
   vertex[0] = min[0];
   vertex[2] = max[2];
   Model::quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);
   vertex[0] = max[0];
   Model::quadSurfaces->read(sea, 1, lucVertexData, vertex.ref(), 2, 2);

   Model::quadSurfaces->setup(sea, lucColourValueData, min[1], max[1]);

      Geometry::checkPointMinMax(min);
      Geometry::checkPointMinMax(max);

   range[1] = max[1] - min[1];
   debug_print("Sampled %d values, min height %f max height %f\n", (sx / subsample) * (sz / subsample), min[1], max[1]);
   //debug_print("Range %f,%f,%f to %f,%f,%f\n", min[0], min[1], min[2], max[0], max[1], max[2]);

   debug_print("X min %f max %f range %f\n", min[0], max[0], range[0]);
   debug_print("Y min %f max %f range %f\n", min[1], max[1], range[1]);
   debug_print("Z min %f max %f range %f\n", min[2], max[2], range[2]);
}

void LavaVu::addTriangles(DrawingObject* obj, float* a, float* b, float* c, int level)
{
   level--;
   float a_b[3], a_c[3], b_c[3];
   vectorSubtract(a_b, a, b);
   vectorSubtract(a_c, a, c);
   vectorSubtract(b_c, b, c);
   float max = 100000; //aview->model_size / 100.0;
   //printf("%f\n", max); getchar();
   
   if (level <= 0) // || (dotProduct(a_b,a_b) < max && dotProduct(a_c,a_c) < max && dotProduct(b_c,b_c) < max))
   {
      //Read the triangle
      Model::triSurfaces->read(obj, 1, lucVertexData, a);
      Model::triSurfaces->read(obj, 1, lucVertexData, b);
      Model::triSurfaces->read(obj, 1, lucVertexData, c);
   }
   else
   {
      //Process a triangle into 4 sub-triangles
      float ab[3] = {0.5*(a[0]+b[0]), 0.5*(a[1]+b[1]), 0.5*(a[2]+b[2])};
      float ac[3] = {0.5*(a[0]+c[0]), 0.5*(a[1]+c[1]), 0.5*(a[2]+c[2])};
      float bc[3] = {0.5*(b[0]+c[0]), 0.5*(b[1]+c[1]), 0.5*(b[2]+c[2])};

      addTriangles(obj, a, ab, ac, level);
      addTriangles(obj, ab, b, bc, level);
      addTriangles(obj, ac, bc, c, level);
      addTriangles(obj, ab, bc, ac, level);
   }
}

void LavaVu::readOBJ(FilePath& fn)
{
   //Use tiny_obj_loader to load a model
   if (fn.type != "obj") return;
   
   std::cout << "Loading " << fn.full << std::endl;

   std::vector<tinyobj::shape_t> shapes;
   std::vector<tinyobj::material_t> materials;
   std::string err = tinyobj::LoadObj(shapes, materials, fn.full.c_str(), fn.path.c_str());

   if (!err.empty()) {
      std::cerr << err << std::endl;
      return;
   }

  std::cout << "# of shapes    : " << shapes.size() << std::endl;
  std::cout << "# of materials : " << materials.size() << std::endl;

  for (size_t i = 0; i < shapes.size(); i++) {
    //Strip path from name
      size_t last_slash = shapes[i].name.find_last_of("\\/");
      if (std::string::npos != last_slash)
         shapes[i].name = shapes[i].name.substr(last_slash + 1);
    printf("shape[%ld].name = %s\n", i, shapes[i].name.c_str());
    printf("Size of shape[%ld].material_ids: %ld\n", i, shapes[i].mesh.material_ids.size());
    
    //Add triangles object
    DrawingObject *tobj = newObject(shapes[i].name, true, 0xff888888, NULL, 1.0, "\n");
    Model::triSurfaces->add(tobj);
       
    assert((shapes[i].mesh.indices.size() % 3) == 0);
    
    if (shapes[i].mesh.material_ids.size() > 0)
    {
      //Just take the first id...
      int id = shapes[i].mesh.material_ids[0];
      if (id >= 0)
      {
        //Use the diffuse property as the colour
        tobj->colour.r = materials[id].diffuse[0] * 255;
        tobj->colour.g = materials[id].diffuse[1] * 255;
        tobj->colour.b = materials[id].diffuse[2] * 255;
        tobj->colour.a = materials[id].dissolve * 255;
      }
    }
    
    for (size_t f = 0; f < shapes[i].mesh.indices.size() / 3; f++) {
      int ids[3] = {shapes[i].mesh.indices[3*f], shapes[i].mesh.indices[3*f+1], shapes[i].mesh.indices[3*f+2]};
      
      addTriangles(tobj, 
                   &shapes[i].mesh.positions[ids[0]*3],
                   &shapes[i].mesh.positions[ids[1]*3],
                   &shapes[i].mesh.positions[ids[2]*3],
                   0);

        for (int coord=0; coord<3; coord++)
        {
          //Model::triSurfaces->read(tobj, 1, lucVertexData, &shapes[i].mesh.positions[ids[coord]*3]);
          Geometry::checkPointMinMax(&shapes[i].mesh.positions[ids[coord]*3]);
        }
    }

    printf("shape[%ld].vertices: %ld\n", i, shapes[i].mesh.positions.size());
    assert((shapes[i].mesh.positions.size() % 3) == 0);
    
    /*for (size_t v = 0; v < shapes[i].mesh.positions.size() / 3; v++) {
      printf("  v[%ld] = (%f, %f, %f)\n", v,
        shapes[i].mesh.positions[3*v+0],
        shapes[i].mesh.positions[3*v+1],
        shapes[i].mesh.positions[3*v+2]);
    }*/
  }
  return;

  for (size_t i = 0; i < materials.size(); i++) {
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
    printf("  material.map_Ns = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(materials[i].unknown_parameter.end());
    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

void LavaVu::readTecplot(FilePath& fn)
{
   //Can only parse tecplot format type FEBRICK
   //http://paulbourke.net/dataformats/tp/
   if (fn.type != "tec" && fn.type != "dat") return;

      //Demo colourmap
      ColourMap* colourMap = new ColourMap();
      addColourMap(colourMap);

      //Colours: hex, abgr
      unsigned int colours[] = {0xff006600, 0xff00ff00,0xffff7733,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff};
      colourMap->add(colours, 7);

      //Add colour bar display
      newObject("colour-bar", false, 0, colourMap, 1.0, "colourbar=1\n");

   std::ifstream file(fn.full.c_str(), std::ios::in);
   if (file.is_open())
   {
      printMessage("Loading %s", fn.full.c_str());
      std::string line;

      int ELS, N;
      int NTRI = 12;  //12 triangles per element
      int NLN = 12;    //12 lines per element
      float* xyz = NULL;
      float* triverts = NULL;
      float* trivals = NULL;
      float* lineverts = NULL;
      float* values = NULL;
      float* particles = NULL;

      float valuemin = HUGE_VAL;
      float valuemax = -HUGE_VAL;
      int count = 0;
      int coord = 0;
      int tcount = 0;
      int lcount = 0;
      int pcount = 0;
      timestep = -1;
      DrawingObject *pobj, *tobj, *lobj;
      while(std::getline(file, line))
      {
         //std::cerr << line << std::endl;
         //getchar();
         if (line.find("ZONE") != std::string::npos)
         {//ZONE T="Image 1",ZONETYPE=FEBRICK, DATAPACKING=BLOCK, VARLOCATION=([1-3]=NODAL,[4-7]=CELLCENTERED), N=356680, E=44585
            if (timestep >= 0) continue; //Only parse first zone for now
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
            timestep = 0;

            //FEBRICK
            xyz = new float[N*3];
            triverts = new float[ELS*NTRI*3*3]; //6 faces = 12 tris
            trivals = new float[ELS*NTRI*3];
            lineverts = new float[ELS*NLN*2*3]; //12 edges
            values = new float[ELS];
            particles = new float[ELS*3];  //Value of cell at centre

            printf("N = %d, ELS = %d\n", N, ELS);


            //Add points object
            pobj = newObject("particles", true, 0, colourMap, 0.5, "lit=0\n");
            Model::points->add(pobj);
            std::cout << values[0] << "," << valuemin << "," << valuemax << std::endl;

            //Add triangles object
            tobj = newObject("triangles", true, 0xffffffff, colourMap, 1.0, "flat=1\n");
            Model::triSurfaces->add(tobj);

            //Add lines object
            lobj = newObject("lines", true, 0xff000000, NULL, 1.0, "lit=0\n");
            Model::lines->add(lobj);


         }
         else if (line.substr(0, 4) == "TEXT")
         {
               count = tcount = lcount = pcount = 0;
               coord = 6;
            //End of a data step
            amodel->timesteps.push_back(TimeStep(timestep, 0));
               printf("READ TIMESTEP %d SIZE %d\n", timestep, amodel->timesteps.size());

            Model::points->read(pobj, ELS, lucVertexData, particles);
            Model::points->read(pobj, ELS, lucColourValueData, values);
            Model::points->setup(pobj, lucColourValueData, valuemin, valuemax);

            Model::triSurfaces->read(tobj, ELS*NTRI*3, lucVertexData, triverts);
            Model::triSurfaces->read(tobj, ELS*NTRI, lucColourValueData, trivals);
            Model::triSurfaces->setup(tobj, lucColourValueData, valuemin, valuemax);

            Model::lines->read(lobj, ELS*NLN*2, lucVertexData, lineverts);

            amodel->clearObjects(false); //Will cache...

            timestep++;
            //Load timesteps while cache slots available
            if (GeomCache::size <= amodel->timesteps.size()) break;

            valuemin = HUGE_VAL;
            valuemax = -HUGE_VAL;
         }
         else if (timestep >= 0)
         {
            std::stringstream ss(line);

            //First 3 coords X,Y,Z 8 per line
            if (coord < 3)
            {
               float value[8];
               particles[pcount*3+coord] = 0;

               for (int e=0; e<8; e++)
               {
                 //std::cout << line << "[" << count << "*3+" << coord << "] = " << xyz[count*3+coord] << std::endl;
                  ss >> value[e];
                  //if (coord ==2) value[e] *= 20; //HACK SCALE Z
                  xyz[count*3+coord] = value[e];
                  particles[pcount*3+coord] += value[e];

                  count++;

                  //if (value[e] < awin->min[coord]) awin->min[coord] = value[e];
                  //if (value[e] > awin->max[coord]) awin->max[coord] = value[e];
                  Geometry::checkPointMinMax(&value[e]);
               }

               for (int i=0; i<8; i++)
                 assert(!isnan(value[i]));

               //Two triangles per side
               //Front
               triverts[(tcount*3)    * 3 + coord] = value[0];
               triverts[(tcount*3+1)  * 3 + coord] = value[2];
               triverts[(tcount*3+2)  * 3 + coord] = value[1];
               tcount++;
               triverts[(tcount*3)    * 3 + coord] = value[2];
               triverts[(tcount*3+1)  * 3 + coord] = value[3];
               triverts[(tcount*3+2)  * 3 + coord] = value[1];
               tcount++;
               //Back
               triverts[(tcount*3)    * 3 + coord] = value[4];
               triverts[(tcount*3+1)  * 3 + coord] = value[6];
               triverts[(tcount*3+2)  * 3 + coord] = value[5];
               tcount++;
               triverts[(tcount*3)    * 3 + coord] = value[6];
               triverts[(tcount*3+1)  * 3 + coord] = value[7];
               triverts[(tcount*3+2)  * 3 + coord] = value[5];
               tcount++;
               //Right
               triverts[(tcount*3)    * 3 + coord] = value[1];
               triverts[(tcount*3+1)  * 3 + coord] = value[3];
               triverts[(tcount*3+2)  * 3 + coord] = value[5];
               tcount++;
               triverts[(tcount*3)    * 3 + coord] = value[3];
               triverts[(tcount*3+1)  * 3 + coord] = value[7];
               triverts[(tcount*3+2)  * 3 + coord] = value[5];
               tcount++;
               //Left
               triverts[(tcount*3)    * 3 + coord] = value[0];
               triverts[(tcount*3+1)  * 3 + coord] = value[2];
               triverts[(tcount*3+2)  * 3 + coord] = value[4];
               tcount++;
               triverts[(tcount*3)    * 3 + coord] = value[2];
               triverts[(tcount*3+1)  * 3 + coord] = value[6];
               triverts[(tcount*3+2)  * 3 + coord] = value[4];
               tcount++;
               //Top??
               triverts[(tcount*3)    * 3 + coord] = value[2];
               triverts[(tcount*3+1)  * 3 + coord] = value[6];
               triverts[(tcount*3+2)  * 3 + coord] = value[7];
               tcount++;
               triverts[(tcount*3)    * 3 + coord] = value[7];
               triverts[(tcount*3+1)  * 3 + coord] = value[3];
               triverts[(tcount*3+2)  * 3 + coord] = value[2];
               tcount++;
               //Bottom??
               triverts[(tcount*3)    * 3 + coord] = value[0];
               triverts[(tcount*3+1)  * 3 + coord] = value[4];
               triverts[(tcount*3+2)  * 3 + coord] = value[1];
               tcount++;
               triverts[(tcount*3)    * 3 + coord] = value[4];
               triverts[(tcount*3+1)  * 3 + coord] = value[5];
               triverts[(tcount*3+2)  * 3 + coord] = value[1];
               tcount++;
               //std::cout << count << " : " << ptr[0] << "," << ptr[3] << "," << ptr[6] << std::endl;
               //getchar();

               //Edge lines
               lineverts[(lcount*2)    * 3 + coord] = value[0];
               lineverts[(lcount*2+1)  * 3 + coord] = value[1];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[1];
               lineverts[(lcount*2+1)  * 3 + coord] = value[3];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[3];
               lineverts[(lcount*2+1)  * 3 + coord] = value[2];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[2];
               lineverts[(lcount*2+1)  * 3 + coord] = value[0];
               lcount++;

               lineverts[(lcount*2)    * 3 + coord] = value[4];
               lineverts[(lcount*2+1)  * 3 + coord] = value[5];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[5];
               lineverts[(lcount*2+1)  * 3 + coord] = value[7];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[7];
               lineverts[(lcount*2+1)  * 3 + coord] = value[6];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[6];
               lineverts[(lcount*2+1)  * 3 + coord] = value[4];
               lcount++;

               lineverts[(lcount*2)    * 3 + coord] = value[2];
               lineverts[(lcount*2+1)  * 3 + coord] = value[6];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[0];
               lineverts[(lcount*2+1)  * 3 + coord] = value[4];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[3];
               lineverts[(lcount*2+1)  * 3 + coord] = value[7];
               lcount++;
               lineverts[(lcount*2)    * 3 + coord] = value[1];
               lineverts[(lcount*2+1)  * 3 + coord] = value[5];
               lcount++;

               //Average value for particle
               particles[pcount*3+coord] /= 8;
               pcount++;
            }
            else if (coord > 5) //Skip I,J,K indices
            {
               //Load SG (1 per line)
               //if (coord > 6) break; //No more values of interest

              //std::cout << line << "[" << count << "*3+" << coord << "] = " << xyz[count*3+coord] << std::endl;
              float value;
               ss >> value;
               values[count] = value;
               count++;

               if (value < valuemin) valuemin = value;
               if (value > valuemax) valuemax = value;

               for (int n=0; n<NTRI; n++, tcount++)
                  trivals[tcount] = value;
            }
            else
            {
               count++;
            }

            if (count >= N || coord > 2 && count >= ELS) 
            {
               count = tcount = lcount = pcount = 0;
               coord++;
            }
         }
      }
      file.close();

      if (xyz) delete[] xyz;
      if (values) delete[] values;
      if (triverts) delete[] triverts;
      if (trivals) delete[] trivals;
      if (lineverts) delete[] lineverts;

      setTimeStep(0);
   }
   else
      printMessage("Unable to open file: %s", fn.full.c_str());
}

void LavaVu::createDemoModel()
{
   int RANGE = 2;
   float min[3] = {-RANGE,-RANGE,-RANGE};
   float max[3] = {RANGE,RANGE,RANGE};
   float dims[3] = {RANGE*2,RANGE*2,RANGE*2};
   float size = sqrt(dotProduct(dims,dims));
   viewer->title = "Test Pattern";

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

      Model::points->read(obj, 1, lucVertexData, ref);
      Model::points->read(obj, 1, lucColourValueData, &colour);

      if (i % NUMSWARM == NUMSWARM-1)
      {
         Model::points->setup(obj, lucColourValueData, 0, size);
         if (i != NUMPOINTS-1)
            Model::points->add(obj);
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
 
      Model::lines->read(obj, 1, lucVertexData, ref);
      Model::lines->read(obj, 1, lucColourValueData, &colour);
   }
   
   Model::lines->setup(obj, lucColourValueData, 0, size);

   //Add some quads (using tri surface mode)
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
         Model::triSurfaces->read(obj, 4, lucVertexData, verts[i], 2, 2);
      }
   }
   
   //Set model bounds...
   Geometry::checkPointMinMax(min);
   Geometry::checkPointMinMax(max);
}

DrawingObject* LavaVu::newObject(std::string name, bool persistent, int colour, ColourMap* map, float opacity, std::string properties)
{
   DrawingObject* obj = new DrawingObject(0, persistent, name, colour, map, opacity, properties);
   if (!awin || awin->views.size() == 0) abort_program("No window/view defined!\n");
   if (!aview) aview = awin->views[0];
   aview->addObject(obj);
   awin->addObject(obj);
   amodel->addObject(obj); //Add to model master list
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

   // Load shaders
   Points::prog = new Shader("pointShader.vert", "pointShader.frag");
   const char* pUniforms[6] = {"uPointScale", "uPointType", "uOpacity", "uPointDist", "uTextured", "uTexture"};
   Points::prog->loadUniforms(pUniforms, 6);
   const char* pAttribs[2] = {"aSize", "aPointType"};
   Points::prog->loadAttribs(pAttribs, 2);
   if (strstr(typeid(*viewer).name(), "OSMesa"))
   {
     //Hack to speed up default point drawing size when using OSMesa
     Model::points->pointType = 4;
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
   const char* vUniforms[22] = {"uPMatrix", "uInvPMatrix", "uMVMatrix", "uNMatrix", "uVolume", "uTransferFunction", "uBBMin", "uBBMax", "uResolution", "uEnableColour", "uBrightness", "uContrast", "uPower", "uViewport", "uSamples", "uDensityFactor", "uIsoValue", "uIsoColour", "uIsoSmooth", "uIsoWalls", "uFilter", "uRange"};
   Volumes::prog->loadUniforms(vUniforms, 22);
   const char* vAttribs[2] = {"aVertexPosition"};
   Volumes::prog->loadAttribs(pAttribs, 2);
}

void LavaVu::resize(int new_width, int new_height)
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
         debug_print("Adjusting particle size scale %f to ", Model::points->scale);
         Model::points->scale *= r; 
         debug_print("%f ( * %f )\n", Model::points->scale, r);
         std::ostringstream ss;
         ss << "resize " << new_width << " " << new_height;
         record(true, ss.str());
      }
   }

   redrawViewports();
}

void LavaVu::close()
{
   for (unsigned int i=0; i < models.size(); i++)
      models[i]->close();
}

void LavaVu::showById(unsigned int id, bool state)
{
   for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->showById(id, state);
   //Setting here to allow hiding of objects without geometry (colourbars)
   amodel->objects[id-1]->visible = state;
}

void LavaVu::redraw(unsigned int id)
{
   for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->redrawObject(id);
}

//Called when model loaded/changed, updates all views and window settings
void LavaVu::resetViews(bool autozoom)
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
   std::stringstream title;
   title << awin->name << " timestep " << timestep;
   viewer->title = title.str();
   if (viewer->isopen && viewer->visible)  viewer->show(); //Update title etc
   viewer->setBackground(awin->background.value); //Update background colour
}

//Called when all viewports need to update
void LavaVu::redrawViewports()
{
   for (unsigned int v=0; v<awin->views.size(); v++)
      awin->views[v]->redraw = true;

   //Flag redraw on objects??
   for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->redraw = true;
}

//Called when view changed
void LavaVu::viewSelect(int idx)
{
   if (awin->views.size() == 0) abort_program("No views available!");
   view = idx;
   if (view < 0) view = awin->views.size() - 1;
   if (view >= (int)awin->views.size()) view = 0;

   aview = awin->views[view];

   for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->setView(aview);
}

//Called when timestep/window changed (new model data)
//Set model size from geometry / bounding box and apply auto zoom
void LavaVu::viewModel(int idx, bool autozoom)
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
   //if (autozoom && aview->properties["zoomstep"].ToInt(-1) == 0)
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
   if (autozoom && aview->properties["zoomstep"].ToInt(-1) > 0 && timestep % aview->properties["zoomstep"].ToInt(-1) == 0)
       aview->zoomToFit();
}

int LavaVu::viewFromPixel(int x, int y)
{
   // Select a viewport by mouse position, leave unchanged if pixel out of range
   for (unsigned int v=0; v<awin->views.size(); v++)
      //Viewport coords opposite to windowing system coords, flip y
      if (awin->views[v]->hasPixel(x, viewer->height - y)) return v;
   return view;
}

//Adds colourmap to active model
void LavaVu::addColourMap(ColourMap* cmap)
{
   ColourMap* colourMap = cmap;
   //Save colour map in list
   amodel->colourMaps.push_back(colourMap);
}

void LavaVu::redrawObjects()
{
   //Flag redraw on all objects...
   for (unsigned int i=0; i < Model::geometry.size(); i++)
      Model::geometry[i]->redraw = true;
}


// Render
void LavaVu::display(void)
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
#ifndef USE_OMEGALIB
   displayMessage();
#endif

   //Display object list if enabled
   if (objectlist)
     displayObjectList(false);

   double time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
   if (time > 0.1)
     debug_print("%.4lf seconds to render scene\n", time);

   aview->sort = false;
}

void LavaVu::displayCurrentView()
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
   //debug_print("### Displaying viewport %s at %d,%d %d x %d\n", aview->title.c_str(), (int)(viewer->width * aview->x), (int)(viewer->height * aview->y), (int)(viewer->width * aview->w), (int)(viewer->height * aview->h));

   if (aview->autozoom)
   {
      aview->projection(EYE_CENTRE);
      aview->zoomToFit(0);
   }
   else
      aview->apply();
   GL_Error_Check;

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
      aview->projection(EYE_CENTRE);
      drawSceneBlended();
      aview->drawOverlay(viewer->inverse, amodel->timeStamp(timestep));
   }

   //Clear the rotation flag
   if (aview->sort) aview->rotated = false;
}

void LavaVu::displayObjectList(bool console)
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
         if (amodel->objects[i] == aobject) ss << "*";
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

void LavaVu::printMessage(const char *fmt, ...)
{
   if (fmt)
   {
      va_list ap;                 // Pointer to arguments list
      va_start(ap, fmt);          // Parse format string for variables
      vsprintf(message, fmt, ap);    // Convert symbols
      va_end(ap);
   }
}

void LavaVu::displayMessage()
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

void LavaVu::displayText(std::string text, int lineno, int colour)
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

void LavaVu::drawSceneBlended()
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

void LavaVu::drawScene()
{
   if (!aview->properties["antialias"].ToBool(true))
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

   Model::triSurfaces->draw();
   Model::quadSurfaces->draw();
   Model::points->draw();
   Model::vectors->draw();
   Model::tracers->draw();
   Model::shapes->draw();
   Model::lines->draw();
   Model::labels->draw();
   Model::volumes->draw();
}

void LavaVu::loadModel(FilePath& fn, bool hideall)
{
   //Open database file
   amodel = new Model(fn, hideall);
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
      awin = new Win(fn.base);
      aview = awin->addView(new View(fn.base));
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
}

//Load model window at specified timestep
bool LavaVu::loadWindow(int window_idx, int at_timestep, bool autozoom)
{
   //Have a database model loaded already?
   if (amodel->objects.size() > 0)
   {
      //Clear current model data
      //clearObjects();

      if (window_idx >= (int)windows.size()) return false;

      //Save active window as selected
      awin = windows[window_idx];
      window = window_idx;

      // Ensure correct model is selected
      for (unsigned int m=0; m < models.size(); m++)
         for (unsigned int w=0; w < models[m]->windows.size(); w++)
            if (models[m]->windows[w] == awin) amodel = models[m];

      //if (amodel->objects.size() == 0) return false;

      //Set timestep and load geometry at that step
      if (amodel->db)
      {
         setTimeStep(at_timestep);
         debug_print("Loading vis '%s', timestep: %d\n", awin->name.c_str(), timestep);
      }
   }

   //Not yet opened or resized?
   if (!viewer->isopen)
   {
      if (fixedwidth > 0 && fixedheight > 0)
         viewer->open(fixedwidth, fixedheight);
      else
         viewer->open(windows[window_idx]->width, windows[window_idx]->height);
   }
   else
      viewer->setsize(awin->width, awin->height);

   //Height maps or other files can be loaded here...
   for (unsigned int m=0; m < files.size(); m++)
   {
      readHeightMap(files[m]);
      readOBJ(files[m]);
      readTecplot(files[m]);
      readVolume(files[m]);
      readVolumeSlice(files[m]);
   }
   
   //Update the views
   resetViews(autozoom);

   //Cache fill (if cache large enough for all data)
   if (amodel->db && GeomCache::size >= amodel->timesteps.size())
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
int LavaVu::setTimeStep(int ts)
{
   clock_t t1 = clock();
   unsigned int idx=0;
   if (windows.size() == 0) return -1;

   int step = amodel->nearestTimeStep(ts, timestep);
   if (step < 0) return -1;
   //Clear currently loaded (also caches if enabled)
   amodel->clearObjects();

   //Closest matching timestep >= requested
   timestep = step;
   debug_print("TimeStep set to: %d\n", timestep);

   //Detach any attached db file and attach n'th timestep database if available
   amodel->attach(timestep);

   //Attempt to load from cache first
   if (amodel->restoreStep(timestep)) return 0; //Cache hit successful return value

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

int LavaVu::loadGeometry(int object_id)
{
   //Load at current timestep
   return amodel->loadGeometry(object_id, timestep, timestep, true);
}

void LavaVu::writeImages(int start, int end)
{
   writeSteps(true, false, start, end, NULL);
}

void LavaVu::encodeVideo(const char* filename, int start, int end)
{
   writeSteps(false, true, start, end, filename);
}

void LavaVu::writeSteps(bool images, bool video, int start, int end, const char* filename)
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

void LavaVu::dumpById(unsigned int id)
{
   for (unsigned int i=0; i < amodel->objects.size(); i++)
   {
      if (amodel->objects[i] && !amodel->objects[i]->skip && (id == 0 || amodel->objects[i]->id == id))
      {
         std::string names[] = {"Labels", "Points", "Grid", "Triangles", "Vectors", "Tracers", "Lines", "Shapes"};
         for (int type=lucMinType; type<lucMaxType; type++)
         {
            std::ostringstream ss;
            Model::geometry[type]->dumpById(ss, amodel->objects[i]->id);

            std::string results = ss.str();
            if (results.size() > 0)
            {
               char filename[512];
               sprintf(filename, "%s%s_%s.%05d.csv", viewer->output_path.c_str(), amodel->objects[i]->name.c_str(),
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

void LavaVu::jsonWriteFile(unsigned int id, bool jsonp)
{
   //Write new JSON format objects
   char filename[512];
   char ext[6];
   strcpy(ext, "jsonp");
   if (!jsonp) ext[4] = '\0';
   if (id > 0)
     sprintf(filename, "%s%s_%s.%05d.%s", viewer->output_path.c_str(), awin->name.c_str(),
                                            amodel->objects[id]->name.c_str(), timestep, ext);
   else
     sprintf(filename, "%s%s.%05d.%s", viewer->output_path.c_str(), awin->name.c_str(), timestep, ext);
   std::ofstream json(filename);
   if (jsonp) json << "loadData(\n";
   jsonWrite(json, id, true);
   if (jsonp) json << ");\n";
   json.close();
}

void LavaVu::jsonWrite(std::ostream& json, unsigned int id, bool objdata)
{
   //Write new JSON format objects
   float rotate[4], translate[3], focus[3], stereo[3];
   aview->getCamera(rotate, translate, focus);

   json << "{\n  \"options\" :\n  {\n"
        << "    \"pointScale\" : " << Model::points->scale << ",\n"
        << "    \"pointType\" : " << Model::points->pointType << ",\n"
        << "    \"border\" : " << (aview->properties["border"].ToInt(0) ? "true" : "false") << ",\n"
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
               if (Model::geometry[type]->size() == 0 || names[type].length() == 0) continue;
               Model::geometry[type]->jsonWrite(amodel->objects[i]->id, &ss);
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

            //TODO: Export all json properties directly from object...!
            if (amodel->objects[i]->properties.HasKey("wireframe"))
               json << "      \"wireframe\" : " << amodel->objects[i]->properties["wireframe"].ToBool(false) << "," << std::endl;
            if (amodel->objects[i]->properties.HasKey("cullface"))
               json << "      \"cullface\" : " << amodel->objects[i]->properties["cullface"].ToBool(false)<< "," << std::endl;
            if (amodel->objects[i]->properties.HasKey("opacity"))
               json << "      \"opacity\" : " << amodel->objects[i]->properties["opacity"].ToFloat(1.0) << "," << std::endl;
            if (amodel->objects[i]->properties.HasKey("pointsize"))
               json << "      \"pointSize\" : " << amodel->objects[i]->properties["pointsize"].ToFloat(1.0) << "," << std::endl;
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
std::string LavaVu::requestData(std::string key)
{
   std::ostringstream result;
   if (key == "objects")
   {
      jsonWrite(result);
   }
   printf("%d\n", result.str().length());
   return result.str();
}
