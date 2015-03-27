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
//Model - handles model data files
#ifndef Model__
#define Model__

#include "sqlite3/src/sqlite3.h"

#include "GraphicsUtil.h"
#include "ColourMap.h"
#include "View.h"
#include "Win.h"
#include "Geometry.h"
#include "TimeStep.h"

class GeomCache
{
  public:
   static int size;
   int step;
   std::vector<Geometry*> store;
   float min[3], max[3];  //Track min/max coords

   GeomCache(int step, std::vector<Geometry*> &data) : step(step)
   {
      //Save previous bounds
      copy3(Geometry::min, min);
      copy3(Geometry::max, max);
      //Ensure store correct size
      store.resize(lucMaxType);
      //Copy all object types
      for (unsigned int i=0; i < data.size(); i++)
      {
         store[i] = new Geometry();
         store[i]->move(data[i]);
      }
   }

   void load(std::vector<Geometry*> &data)
   {
      //Replace geometry data with cached store
      for (unsigned int i=0; i < store.size(); i++)
      {
         data[i]->move(store[i]);
      }
      //Restore previous bounds
      copy3(min, Geometry::min);
      copy3(max, Geometry::max);
   }

   void copy3(float src[3], float dst[3])
   {
      for (unsigned int i=0; i < 3; i++)
        dst[i] = src[i];
   }
};

class Model
{
   //Geometry cache
   std::deque<GeomCache*> cache;
  public:
   int timestep;
   //Current timestep geometry
   static std::vector<Geometry*> geometry;
   //Type specific geometry pointers
   static Geometry* labels;
   static Points* points;
   static Vectors* vectors;
   static Tracers* tracers;
   static QuadSurfaces* quadSurfaces;
   static TriSurfaces* triSurfaces;
   static Lines* lines;
   static Shapes* shapes;
   static Volumes* volumes;

   std::vector<TimeStep> timesteps;

   bool readonly;
   FilePath file;
   int attached;
   std::string apath;
   char prefix[10];   //attached db prefix

   std::vector<Win*> windows;
   std::vector<View*> views;
   std::vector<DrawingObject*> objects;
   std::vector<ColourMap*> colourMaps;

   sqlite3 *db;

   sqlite3_stmt* select(const char* SQL, bool silent=false);
   bool issue(const char* SQL, sqlite3* odb=NULL);
   bool open(bool write=false);
   void reopen(bool write=false);
   void attach(int timestep);
   void close();
   void clearObjects(bool all=false);
   void reset();
   void loadWindows();
   void loadLinks(Win* win);
   void loadLinks(DrawingObject* draw);
   int loadTimeSteps();
   void loadViewports();
   void loadViewCamera(int viewport_id);
   void loadObjects();
   void loadColourMaps();
   void initColourMaps();

   Model(FilePath& fn, bool hideall=false);
   ~Model();

   void addObject(DrawingObject* draw)
   {
      //Create master drawing object list entry
      if (objects.size() < draw->id) objects.resize(draw->id);
      objects[draw->id-1] = draw;
   }

   //Timestep caching
   void deleteCache();
   void cacheStep();
   bool restoreStep(int step);

   int lastStep()
   {
     if (timesteps.size() == 0) return -1;
     return timesteps[timesteps.size()-1].step;
   }

   std::string timeStamp(int timestep);
   bool hasTimeStep(int ts);
   int nearestTimeStep(int requested, int current);
   
   int loadGeometry(int object_id, int time_start, int time_stop, bool recurseTracers);
   int decompressGeometry(int object_id, int timestep);
   void writeDatabase(const char* path, unsigned int id, bool compress=false);
   void writeGeometry(sqlite3* outdb, lucGeometryType type, int obj_id, bool compress);
   void deleteObject(int id);
};

#endif //Model__
