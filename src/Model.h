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

#include "sqlite3/sqlite3.h"

#include "Session.h"
#include "Util.h"
#include "GraphicsUtil.h"
#include "ColourMap.h"
#include "View.h"
#include "Geometry.h"
#include "TimeStep.h"

#define SQL_QUERY_MAX 4096

class Database
{
  friend class Model; //Allow private access from Model
private:
  bool readonly;
  bool silent;
  char SQL[SQL_QUERY_MAX];

protected:
  TimeStep* attached;
  char prefix[10];    //attached db prefix
  sqlite3 *db;
  FilePath file;
  bool memory;

public:

  Database();
  Database(const FilePath& fn);
  ~Database();

  bool open(bool write=false);
  void reopen(bool write=false);
  void attach(TimeStep* timestep);

  sqlite3_stmt* select(const char* fmt, ...);
  bool issue(const char* fmt, ...);

  operator bool() const { return db != NULL; }
};

class Model
{
private:
  int now;            //Loaded step per model
  Session& session;

public:

  Database database;
  int figure;
  std::vector<std::string> fignames;
  std::vector<std::string> figures;

  //Current timestep geometry
  std::vector<Geometry*> geometry;

  std::vector<TimeStep*> timesteps;
  std::vector<View*> views;
  std::vector<DrawingObject*> objects;
  std::vector<ColourMap*> colourMaps;

  DrawingObject* borderobj = NULL;
  DrawingObject* axisobj = NULL;
  DrawingObject* rulerobj = NULL;
  Geometry* border = NULL, * axis = NULL, * rulers = NULL;

  float min[3], max[3]; //Calculated model bounding box

  Geometry* getRenderer(lucGeometryType type, std::vector<Geometry*>& renderers);
  Geometry* getRenderer(lucGeometryType type);
  std::vector<Geometry*> getRenderersByTypeName(const std::string& typestr);
  Geometry* lookupObjectRenderer(DrawingObject* obj, lucGeometryType type=lucPointType);

  void clearObjects(bool fixed=false);
  void setup();
  void reload(DrawingObject* obj=NULL);
  void redraw(DrawingObject* obj=NULL);
  void reloadRedraw(DrawingObject* obj, bool reload);
  void bake(DrawingObject* obj, bool colours, bool texture);

  void loadWindows();
  void loadLinks();
  void loadLinks(DrawingObject* obj);
  void clearTimeSteps();
  int loadTimeSteps(bool scan=false);
  void loadFixed();
  std::string checkFileStep(unsigned int ts, const std::string& basename, unsigned int limit=1);
  void loadViewports();
  void loadViewCamera(int viewport_id);
  void loadObjects();
  void loadColourMaps();
  void loadColourMapsLegacy();
  void setColourMapProps(Properties& properties, float  minimum, float maximum, bool logscale, bool discrete);

  Model(Session& session);
  void init(bool clear=true);
  void load(const FilePath& fn);
  void load();
  void clearRenderers();
  ~Model();

  bool loadFigure(int fig, bool preserveGlobals=false);
  void storeFigure();
  int addFigure(std::string name="", const std::string& state="");
  void addObject(DrawingObject* obj);
  ColourMap* addColourMap(std::string name="", std::string colours="", std::string properties="");
  void updateColourMap(ColourMap* colourMap, std::string colours="", std::string properties="");
  DrawingObject* findObject(unsigned int id);
  DrawingObject* findObject(const std::string& name, DrawingObject* def=NULL);
  ColourMap* findColourMap(const std::string& name, ColourMap* def=NULL);
  View* defaultView(Properties* properties=NULL);

  void cacheLoad();

public:
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
  int addTimeStep(int step=0, const std::string& props="", const std::string& path="");
  int setTimeStep(int stepidx);
  int loadGeometry(int obj_id=0, int time_start=-1, int time_stop=-1);
  int loadFixedGeometry(int obj_id=0);
  int readGeometryRecords(sqlite3_stmt* statement, bool cache=true);
  void mergeDatabases();
  void mergeRecords(Model* other);
  void updateObject(DrawingObject* target, lucGeometryType type);
  void writeDatabase(const char* path, DrawingObject* obj=NULL, bool overwrite=true);
  void writeDatabase(Database& outdb, DrawingObject* obj=NULL);
  void writeState();
  void writeState(Database& outdb);
  void writeObjects(Database& outdb, DrawingObject* obj=NULL, int step=-1);
  void deleteGeometry(Database& outdb, lucGeometryType type, DrawingObject* obj, int step);
  void writeGeometry(Database& outdb, Geometry* g, DrawingObject* obj, int step);
  void writeGeometryRecord(Database& outdb, lucGeometryType type, lucGeometryDataType dtype, unsigned int objid, Geom_Ptr data, DataContainer* block, int step);
  void deleteObjectRecord(unsigned int id);
  void backup(Database& fromdb, Database& todb);
  void calculateBounds(View* aview, float* default_min=NULL, float* default_max=NULL);
  void objectBounds(DrawingObject* obj, float* min, float* max);
  void deleteObject(DrawingObject* obj);

  json objectDataSets(DrawingObject* o);
  std::string jsonWrite(bool objdata=false);
  void jsonWrite(std::ostream& os, DrawingObject* obj=NULL, bool objdata=false);
  int jsonRead(std::string data);

  std::vector<unsigned char> serialize();
  void deserialize(unsigned char* source, unsigned int len, bool persist=false);
};

#endif //Model__
