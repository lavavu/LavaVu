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

//Model class
#include "Model.h"

int TimeStep::gap = 0;  //Here for now, probably should be in separate TimeStep.cpp
int GeomCache::size = 0;

bool Model::open(bool write)
{
   //Single file database
   char path[256];
   strcpy(path, file.full.c_str());
   if (sqlite3_open_v2(path, &db, write ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, NULL))
   {
      //Try 0th timestep of multi-file split database
      sprintf(path, "%s%05d.%s", file.base.c_str(), 0, file.ext.c_str());
      if (sqlite3_open_v2(path, &db, write ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, NULL))
      {
         // failed
         debug_print("Can't open database %s: %s\n", path, sqlite3_errmsg(db));
         return false;
      }
   }
   // success
   debug_print("Open database %s successful, SQLite version %s\n", path, sqlite3_libversion());
   //rc = sqlite3_busy_handler(db, busy_handler, void*);
   sqlite3_busy_timeout(db, 10000); //10 seconds

   //Workaround slow query (auto indexing) bug in SQLite3 3.7.5 > version < 3.7.8
   //Not necessary now as we are bundling v3.7.8
   //issue("PRAGMA automatic_index = false;");

   if (write) readonly = false;

   return true;
}

void Model::reopen(bool write)
{
   if (!readonly) return;
   if (db) sqlite3_close(db);
   open(write);

   //Re-attach any attached db file
   if (attached)
   {
      char SQL[1024];
      sprintf(SQL, "attach database '%s' as t%d", apath.c_str(), attached);
      if (issue(SQL))
         debug_print("Model %s found and re-attached\n", apath.c_str());
   }
}

void Model::attach(int timestep)
{
   //Detach any attached db file
   char SQL[1024];
   if (attached && attached != timestep)
   {
      sprintf(SQL, "detach database 't%d'", attached);
      if (issue(SQL))
      {
         debug_print("Model t%d detached\n", attached);
         attached = 0;
         prefix[0] = '\0';
      }
      else
         debug_print("Model t%d detach failed!\n", attached);
   }

   //Attach n'th timestep database if available
   if (timestep > 0 && !attached)
   {
      char path[1024];
      FILE* fp;
      sprintf(path, "%s%s%05d.%s", file.path.c_str(), file.base.c_str(), timestep, file.ext.c_str());
      if (fp = fopen(path, "r"))
      {
         fclose(fp);
         sprintf(SQL, "attach database '%s' as t%d", path, timestep);
         if (issue(SQL))
         {
            sprintf(prefix, "t%d.", timestep);
            debug_print("Model %s found and attached\n", path);
            attached = timestep;
            apath = path;
         }
         else
         {
            debug_print("Model %s found but attach failed!\n", path);
         }
      }
      //else
      //   debug_print("Model %s not found, loading from current db\n", path);
   }
}

void Model::loadWindows()
{
   //Load windows list from database and insert into models
   sqlite3_stmt* statement = select("SELECT id,name,width,height,colour,minX,minY,minZ,maxX,maxY,maxZ from window");
   //sqlite3_stmt* statement = model->select("SELECT id,name,width,height,colour,minX,minY,minZ,maxX,maxY,maxZ,properties from window");
   //window (id, name, width, height, colour, minX, minY, minZ, maxX, maxY, maxZ, properties)
   while ( sqlite3_step(statement) == SQLITE_ROW)
   {
      int id = sqlite3_column_int(statement, 0);
      const char *wtitle = (char*)sqlite3_column_text(statement, 1);
      int width = sqlite3_column_int(statement, 2);
      int height = sqlite3_column_int(statement, 3);
      int bg = sqlite3_column_int(statement, 4);
      float min[3], max[3];
      for (int i=0; i<3; i++)
      {
         min[i] = (float)sqlite3_column_double(statement, 5+i);
         max[i] = (float)sqlite3_column_double(statement, 8+i);
      }

      Win* win = new Win(id, std::string(wtitle), width, height, bg, min, max);
      windows.push_back(win);

      //Link the window viewports, objects & colourmaps
      loadLinks(win);
   }
   sqlite3_finalize(statement);
}

//Load model viewports
void Model::loadViewports()
{
   sqlite3_stmt* statement;
   statement = select("SELECT id,title,x,y,near,far FROM viewport ORDER BY y,x", true);

   //viewport:
   //(id, title, x, y, near, far, translateX,Y,Z, rotateX,Y,Z, scaleX,Y,Z, properties
   while (sqlite3_step(statement) == SQLITE_ROW)
   {
      unsigned int viewport_id = (unsigned int)sqlite3_column_int(statement, 0);
      const char *vtitle = (char*)sqlite3_column_text(statement, 1);
      float x = (float)sqlite3_column_double(statement, 2);
      float y = (float)sqlite3_column_double(statement, 3);
      float nearc = (float)sqlite3_column_double(statement, 4);
      float farc = (float)sqlite3_column_double(statement, 5);

      //Create the view object
      View* v = new View(vtitle, false, x, y, nearc, farc);
      //Add to list
      if (views.size() < viewport_id) views.resize(viewport_id);
      views[viewport_id-1] = v;
      debug_print("Loaded viewport \"%s\" at %f,%f\n", vtitle, x, y);
   }
   sqlite3_finalize(statement);
}

//Load and apply viewport camera settings
void Model::loadViewCamera(int viewport_id)
{
   int adj=0;
   //Load specified viewport and apply camera settings
   char SQL[1024];
   sprintf(SQL, "SELECT aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", viewport_id);
   sqlite3_stmt* statement = select(SQL);
   if (statement == NULL)
   {
      //Old db
      adj = -5;
      sprintf(SQL, "SELECT translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ FROM viewport WHERE id=%d;", viewport_id);
      statement = select(SQL);
   }

   //viewport:
   //(aperture, orientation, focalPointX,Y,Z, translateX,Y,Z, rotateX,Y,Z, scaleX,Y,Z, properties
   if (sqlite3_step(statement) == SQLITE_ROW)
   {
      View* v = views[viewport_id-1];
      float aperture = 45.0;
      int orientation = RIGHT_HANDED;
      if (adj == 0)
      {
         aperture = (float)sqlite3_column_double(statement, 0);
         orientation = sqlite3_column_int(statement, 1);
      }
      float focus[3] = {0,0,0}, rotate[3], translate[3], scale[3] = {1,1,1};
      for (int i=0; i<3; i++)
      {
         //camera
         if (adj == 0)
         {
            if (sqlite3_column_type(statement, 2+i) != SQLITE_NULL)
               focus[i] = (float)sqlite3_column_double(statement, 2+i);
            else
               focus[i] = FLT_MIN;
         }
         translate[i] = (float)sqlite3_column_double(statement, 5+i+adj);
         rotate[i] = (float)sqlite3_column_double(statement, 8+i+adj);
         scale[i] = (float)sqlite3_column_double(statement, 11+i+adj);
      }

      const char *vprops = adj == 0 ? (char*)sqlite3_column_text(statement, 14) : "";

      if (adj == 0) v->focus(focus[0], focus[1], focus[2], aperture, true);
      v->translate(translate[0], translate[1], translate[2]);
      v->rotate(rotate[0], rotate[1], rotate[2]);
      v->setScale(scale[0], scale[1], scale[2]);
      v->setCoordSystem(orientation);
      v->setProperties(vprops);
      //debug_print("Loaded \"%s\" at %f,%f\n");
   }
   sqlite3_finalize(statement);
}

//Load model objects
void Model::loadObjects()
{
   sqlite3_stmt* statement;
   statement = select("SELECT id, name, colour, opacity, properties FROM object", true);
   if (statement == NULL)
      statement = select("SELECT id, name, colour, opacity FROM object");

   //object (id, name, colourmap_id, colour, opacity, properties)
   while (sqlite3_step(statement) == SQLITE_ROW)
   {
      int object_id = sqlite3_column_int(statement, 0);
      const char *otitle = (char*)sqlite3_column_text(statement, 1);
      int colour = sqlite3_column_int(statement, 2);
      float opacity = (float)sqlite3_column_double(statement, 3);

      //Create drawing object and add to master list
      if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
      {
         const char *oprops = (char*)sqlite3_column_text(statement, 4);
         addObject(new DrawingObject(object_id, false, otitle, colour, NULL, opacity, oprops));
      }
      else
         addObject(new DrawingObject(object_id, false, otitle, colour, NULL, opacity));
   }
   sqlite3_finalize(statement);
}

//Load viewports in current window, objects in each viewport, colourmaps for each object
void Model::loadLinks(Win* win)
{
   //Select statment to get all viewports in window and all objects in viewports
   char SQL[1024];
   //sprintf(SQL, "SELECT id,title,x,y,near,far,aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", win->id);
   sprintf(SQL, "SELECT viewport.id,object.id,object.colourmap_id,object_colourmap.colourmap_id,object_colourmap.data_type FROM window_viewport,viewport,viewport_object,object LEFT OUTER JOIN object_colourmap ON object_colourmap.object_id=object.id WHERE window_viewport.window_id=%d AND viewport.id=window_viewport.viewport_id AND viewport_object.viewport_id=viewport.id AND object.id=viewport_object.object_id", win->id);
   sqlite3_stmt* statement = select(SQL);

   int last_viewport = 0, last_object = 0;
   DrawingObject* draw = NULL;
   View* view = NULL;
   while ( sqlite3_step(statement) == SQLITE_ROW)
   {
      int viewport_id = sqlite3_column_int(statement, 0);
      int object_id = sqlite3_column_int(statement, 1);
      int colourmap_id = sqlite3_column_int(statement, 3); //Linked colourmap id

      //Fields from object_colourmap
      lucGeometryDataType colourmap_datatype = (lucGeometryDataType)sqlite3_column_int(statement, 4);
      if (!colourmap_id)
      {
         //Backwards compatibility with old db files
         colourmap_id = sqlite3_column_int(statement, 2);
         colourmap_datatype = lucColourValueData;
      }

      //Get viewport
      view = views[viewport_id-1];
      if (last_viewport != viewport_id)
      {
         win->addView(view);
         loadViewCamera(viewport_id);
         last_viewport = viewport_id;
         last_object = 0;  //Reset required, in case of single object which is shared between viewports
      }

      //Get drawing object
      if (!objects[object_id-1]) continue; //No geometry
      draw = objects[object_id-1];
      if (last_object != object_id)
      {
         view->addObject(draw);
         win->addObject(draw);
         last_object = object_id;
      }

      //Add colour maps to drawing objects...
      if (colourmap_id)
      {
         //Find colourmap by id
         ColourMap* cmap = colourMaps[colourmap_id-1];
         //Add colourmap to drawing object
         draw->addColourMap(cmap, colourmap_datatype);
      }
   }
   sqlite3_finalize(statement);
}

//Load colourmaps for each object only
void Model::loadLinks(DrawingObject* draw)
{
   //Select statment to get all viewports in window and all objects in viewports
   char SQL[1024];
   //sprintf(SQL, "SELECT id,title,x,y,near,far,aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", win->id);
   sprintf(SQL, "SELECT object.id,object.colourmap_id,object_colourmap.colourmap_id,object_colourmap.data_type FROM object LEFT OUTER JOIN object_colourmap ON object_colourmap.object_id=object.id WHERE object.id=%d", draw->id);
   sqlite3_stmt* statement = select(SQL);

   while ( sqlite3_step(statement) == SQLITE_ROW)
   {
      int object_id = sqlite3_column_int(statement, 0);
      int colourmap_id = sqlite3_column_int(statement, 2); //Linked colourmap id

      //Fields from object_colourmap
      lucGeometryDataType colourmap_datatype = (lucGeometryDataType)sqlite3_column_int(statement, 3);
      if (!colourmap_id)
      {
         //Backwards compatibility with old db files
         colourmap_id = sqlite3_column_int(statement, 1);
         colourmap_datatype = lucColourValueData;
      }

      //Add colour maps to drawing objects...
      if (colourmap_id)
      {
         //Find colourmap by id
         ColourMap* cmap = colourMaps[colourmap_id-1];
         //Add colourmap to drawing object
         draw->addColourMap(cmap, colourmap_datatype);
      }
   }
   sqlite3_finalize(statement);
}

int Model::loadTimeSteps()
{
   timesteps.clear();
   TimeStep::gap = 0;
   if (!db) return 0;
   int rows = 0;
   int last_step = 0;
   sqlite3_stmt* statement = select("SELECT * FROM timestep");
   //(id, time, dim_factor, units)
   while ( sqlite3_step(statement) == SQLITE_ROW)
   {
      int step = sqlite3_column_int(statement, 0);
      double time = sqlite3_column_double(statement, 1);
      //Next two fields are not present in pre-release databases
      if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
      {
         double dimCoeff = sqlite3_column_double(statement, 2);
         const char* units = (const char*)sqlite3_column_text(statement, 3);
         timesteps.push_back(TimeStep(step, time, dimCoeff, std::string(units)));
      }
      else
         timesteps.push_back(TimeStep(step, time));
      //Save gap
      if (step - last_step > TimeStep::gap) TimeStep::gap = step - last_step;
      last_step = step;
      rows++;
   }
   sqlite3_finalize(statement);
   //Copy to geometry static for use in Tracers
   Geometry::timesteps = timesteps;
   return timesteps.size();
}

void Model::loadColourMaps()
{
   sqlite3_stmt* statement = select("SELECT * FROM colourmap,colourvalue WHERE colourvalue.colourmap_id=colourmap.id");
   //colourmap (id, minimum, maximum, logscale, discrete, centreValue) 
   //colourvalue (id, colourmap_id, colour, position) 
   int map_id = 0;
   double minimum;
   double maximum;
   ColourMap* colourMap = NULL;
   while ( sqlite3_step(statement) == SQLITE_ROW)
   {
      int offset = 0;
      int id = sqlite3_column_int(statement, 0);
      char *cmname = NULL;
      char idname[10];
      sprintf(idname, "%d", id);
      //Name added to schema, support old db versions by checking number of fields (8 colourmap + 4 colourvalue)
      if (sqlite3_column_count(statement) == 12)
      {
        offset = 1;
        cmname = (char*)sqlite3_column_text(statement, 1);
      }

      //New map?
      if (id != map_id)
      {
         map_id = id;
         minimum = sqlite3_column_double(statement, 1+offset);
         maximum = sqlite3_column_double(statement, 2+offset);
         int logscale = sqlite3_column_int(statement, 3+offset);
         int discrete = sqlite3_column_int(statement, 4+offset);
         float centreValue = sqlite3_column_double(statement, 5+offset);
         const char *oprops = (char*)sqlite3_column_text(statement, 6+offset);
         colourMap = new ColourMap(id, cmname ? cmname : idname, logscale, discrete, centreValue, minimum, maximum);
         colourMaps.push_back(colourMap);
      }

      //Add colour value
      int colour = sqlite3_column_int(statement, 9+offset);
      //const char *name = sqlite3_column_name(statement, 7);
      if (sqlite3_column_type(statement, 10+offset) != SQLITE_NULL)
      {
         double value = sqlite3_column_double(statement, 10+offset);
         colourMap->add(colour, value);
      }
      else
         colourMap->add(colour);
      //debug_print("ColourMap: %d min %f, max %f Value %d \n", id, minimum, maximum, colour);
   }

   sqlite3_finalize(statement);

   //Load default colourmaps
   //Colours: hex, abgr
   colourMap = new ColourMap(0, "Greyscale");
   colourMaps.push_back(colourMap);
   unsigned int colours0[] = {0xff000000,0xffffffff};
   colourMap->add(colours0, 2);

   colourMap = new ColourMap(0, "Topology");
   colourMaps.push_back(colourMap);
   unsigned int colours1[] = {0xff33bb66,0xff00ff00,0xffff3333,0xffffff00,0xff77ffff,0xff0088ff,0xff0000ff,0xff000000};
   colourMap->add(colours1, 8);

   colourMap = new ColourMap(0, "Rainbow");
   colourMaps.push_back(colourMap);
   unsigned int colours2[] = {0xfff020a0,0xffff0000,0xff00ff00,0xff00ffff,0xff00a5ff,0xff0000ff,0xff000000};
   colourMap->add(colours2, 7);

   colourMap = new ColourMap(0, "Heat");
   colourMaps.push_back(colourMap);
   unsigned int colours3[] = {0xff000000,0xff0000ff,0xff00ffff,0xffffffff};
   colourMap->add(colours3, 4);

   colourMap = new ColourMap(0, "Bluered");
   colourMaps.push_back(colourMap);
   unsigned int colours4[] = {0xffff0000,0xffff901e,0xffd1ce00,0xffc4e4ff,0xff00a5ff,0xff2222b2};
   colourMap->add(colours4, 6);

   //Initial calibration for all maps
   for (unsigned int i=0; i<colourMaps.size(); i++)
      colourMaps[i]->calibrate();
}

//SQLite3 utility functions
sqlite3_stmt* Model::select(const char* SQL, bool silent)
{
   //debug_print("Issuing select: %s\n", SQL);
   sqlite3_stmt* statement;
   //Prepare statement...
   int rc = sqlite3_prepare_v2(db, SQL, -1, &statement, NULL);
   if (rc != SQLITE_OK)
   {
      if (!silent)
         debug_print("Prepare error (%s) %s\n", SQL, sqlite3_errmsg(db)); 
      return NULL;
   }
   return statement;
} 

bool Model::issue(const char* SQL)
{
   // Executes a basic SQLite command (ie: without pointer objects and ignoring result sets) and checks for errors
   //debug_print("Issuing: %s\n", SQL);
   char* zErrMsg;
   if (sqlite3_exec(db, SQL, NULL, 0, &zErrMsg) != SQLITE_OK)
   {
      debug_print("SQLite error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return false;
   }
   return true;
}

void Model::deleteCache()
{
   if (GeomCache::size == 0) return;
   debug_print("~~~ Cache emptied\n");
   cache.clear();
}

void Model::cacheStep(int timestep, std::vector<Geometry*> &geometry)
{
   if (GeomCache::size == 0) return;
   debug_print("~~~ Caching geometry @ %d, geom memory usage: %.3f mb\n", timestep, FloatValues::membytes/1000000.0f);

   //Copy all elements
   if (FloatValues::membytes > 0)
   {
      GeomCache* cached = new GeomCache(timestep, geometry);
      cache.push_back(cached);
   }
   else
      debug_print("~~~ Nothing to cache\n");

   //Remove if over limit
   if (cache.size() > GeomCache::size)
   {
      GeomCache* cached = cache.front();
      cache.pop_front();
      //Clear containers...
      for (unsigned int i=0; i < cached->store.size(); i++)
      {
         cached->store[i]->clear(true);
         delete cached->store[i];
      }
      debug_print("~~~ Deleted oldest cached step (%d)\n", cached->step);
      delete cached;
   }
}


bool Model::restoreStep(int timestep, std::vector<Geometry*> &geometry)
{
   if (GeomCache::size == 0) return false;
   //Load new geometry data from active window object list
   for (int c=0; c<cache.size(); c++)
   {
      if (cache[c]->step == timestep && cache[c]->store.size() > 0)
      {
         debug_print("~~~ Cache hit at ts %d, loading!\n", timestep);
         cache[c]->load(geometry);
         //Delete cache entry
         cache.erase(cache.begin() + c);
         return true;
      }
   }
   return false;
}

std::string Model::timeStamp(int timestep)
{
   // Timestep (with scaling applied)
   if (timesteps.size() == 0) return std::string("");
   unsigned int t;
   for (t=0; t<timesteps.size(); t++)
      if (timesteps[t].step == timestep) break;

   if (t == timesteps.size()) return std::string("");

   // Use scaling coeff and units to get display time
   TimeStep* ts = &timesteps[t];
   char displayString[32];
   sprintf(displayString, "Time %g%s", ts->time * ts->dimCoeff, ts->units.c_str());

   return std::string(displayString);
}

//Set time step if available, otherwise return false and leave unchanged
bool Model::hasTimeStep(int ts)
{
   //Load timestep data
   if (timesteps.size() == 0 && loadTimeSteps() == 0) return false;
   for (int idx=0; idx < timesteps.size(); idx++)
      if (ts == timesteps[idx].step)
         return true;
   return false;
}

int Model::nearestTimeStep(int requested, int current)
{
   //Find closest matching timestep to requested but != current
   int idx;
   //if (timesteps.size() == 0 && loadTimeSteps() == 0) return -1;
   if (loadTimeSteps() == 0) return -1;
   //if (timesteps.size() == 1 && current >= 0 && ) return -1;  //Single timestep

   for (idx=0; idx < timesteps.size(); idx++)
      if (timesteps[idx].step >= requested) break;

   //Reached end of list? 
   if (idx == timesteps.size()) idx--;

   //Unchanged...
   //if (requested >= current && timesteps[idx].step == current) return 0;

   //Ensure same timestep not selected if others available
   if (current == timesteps[idx].step && timesteps.size() > 1)
   {
      if (requested < current)
      {
         //Select previous timestep in list (don't loop to end from start)
         if (idx > 0) idx--;
         return timesteps[idx].step;
      }
      //Select next timestep in list
      else if (requested > current && idx+1 < timesteps.size())
      {
         idx++;
         return timesteps[idx].step;
      }
   }

   return timesteps[idx].step;
}

