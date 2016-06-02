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
#include "Geometry.h"
#include "TimeStep.h"

#define SQL_QUERY_MAX 4096

class Model
{
private:
  bool readonly;
  int attached;
  char prefix[10];   //attached db prefix

public:
  FilePath file;
  std::string basename;
  sqlite3 *db;
  bool memorydb;

  std::vector<std::string> fignames;
  std::vector<std::string> figures;
  int figure;

  static int now;
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

  std::vector<TimeStep*> timesteps;
  std::vector<View*> views;
  std::vector<DrawingObject*> objects;
  std::vector<ColourMap*> colourMaps;

  sqlite3_stmt* select(const char* SQL, bool silent=false);
  bool issue(const char* SQL, sqlite3* odb=NULL);
  bool open(bool write=false);
  void reopen(bool write=false);
  void attach(int stepidx);
  void close();
  void clearObjects(bool all=false);
  void redraw(bool reload=false);
  unsigned int addColourMap(ColourMap* cmap=NULL);
  void loadWindows();
  void loadLinks();
  void loadLinks(DrawingObject* obj);
  void clearTimeSteps();
  int loadTimeSteps(bool scan=false);
  std::string checkFileStep(unsigned int ts, const std::string& basename);
  void loadViewports();
  void loadViewCamera(int viewport_id);
  void loadObjects();
  void loadColourMaps();
  void loadColourMapsLegacy();

  Model();
  Model(const FilePath& fn);
  void init();
  ~Model();

  bool loadFigure(int fig);
  void addObject(DrawingObject* obj);
  DrawingObject* findObject(unsigned int id);
  View* defaultView();

  //Timestep caching
  void deleteCache();
  void cacheStep();
  bool restoreStep();
  void printCache();

  int step()
  {
    //Current actual step
    return now < 0 || (int)timesteps.size() <= now ? -1 : timesteps[now]->step;
  }

  int stepInfo()
  {
    //Current actual step (returns 0 if none instead of -1 for output functions)
    return now < 0 || (int)timesteps.size() <= now ? 0 : timesteps[now]->step;
  }

  int lastStep()
  {
    if (timesteps.size() == 0) return -1;
    return timesteps[timesteps.size()-1]->step;
  }

  bool hasTimeStep(int ts);
  int nearestTimeStep(int requested);
  void addTimeStep(int step=0, double time=0.0, const std::string& path="")
  {
    timesteps.push_back(new TimeStep(step, time, path));
  }

  int setTimeStep(int stepidx=now);
  int loadGeometry(int obj_id=0, int time_start=-1, int time_stop=-1, bool recurseTracers=true);
  void mergeDatabases();
  int decompressGeometry(int timestep);
  void writeDatabase(const char* path, DrawingObject* obj, bool compress=false);
  void writeState(sqlite3* outdb=NULL);
  void writeObjects(sqlite3* outdb, DrawingObject* obj, int step, bool compress);
  void writeGeometry(sqlite3* outdb, lucGeometryType type, DrawingObject* obj, int step, bool compress);
  void deleteObject(unsigned int id);
  void backup(sqlite3 *fromDb, sqlite3* toDb);

  void jsonWrite(std::ostream& os, DrawingObject* obj=NULL, bool objdata=false);
  void jsonRead(std::string data);
};

#endif //Model__
