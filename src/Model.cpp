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

std::vector<TimeStep*> TimeStep::timesteps; //Active model timesteps
int TimeStep::gap = 0;  //Here for now, probably should be in separate TimeStep.cpp
int TimeStep::cachesize = 0;
int Model::now = -1;

//Static geometry containers, shared by all models for fast switching/drawing
std::vector<Geometry*> Model::geometry;
Geometry* Model::labels = NULL;
Points* Model::points = NULL;
Vectors* Model::vectors = NULL;
Tracers* Model::tracers = NULL;
QuadSurfaces* Model::quadSurfaces = NULL;
TriSurfaces* Model::triSurfaces = NULL;
Lines* Model::lines = NULL;
Shapes* Model::shapes = NULL;
Volumes* Model::volumes = NULL;

Model::Model() : readonly(true), attached(0), db(NULL), memorydb(false), figure(-1)
{
  prefix[0] = '\0';

  //Create new geometry containers
  init();
}

void Model::load(const FilePath& fn)
{
  //Open database file
  file = fn;
  if (open())
  {
    loadTimeSteps();
    loadColourMaps();
    loadObjects();
    loadViewports();
    loadWindows();
  }
  else
  {
    std::cerr << "Model database open failed: " << fn.full << std::endl;
  }
}


View* Model::defaultView()
{
  //No views?
  if (views.size() == 0)
  {
    //Default view
    View* view = new View();
    views.push_back(view);

    //Add objects to viewport
    for (unsigned int o=0; o<objects.size(); o++)
    {
      view->addObject(objects[o]);
      loadLinks(objects[o]);
    }
  }

  //Return first
  return views[0];
}

void Model::init()
{
  //Create new geometry containers
  geometry.resize(lucMaxType);
  geometry[lucLabelType] = labels = new Geometry();
  geometry[lucPointType] = points = new Points();
  geometry[lucVectorType] = vectors = new Vectors();
  geometry[lucTracerType] = tracers = new Tracers();
  geometry[lucGridType] = quadSurfaces = new QuadSurfaces(true);
  geometry[lucVolumeType] = volumes = new Volumes();
  geometry[lucTriangleType] = triSurfaces = new TriSurfaces(true);
  geometry[lucLineType] = lines = new Lines();
  geometry[lucShapeType] = shapes = new Shapes();
  debug_print("Created %d new geometry containers\n", geometry.size());

  for (unsigned int i=0; i < geometry.size(); i++)
  {
    bool hideall = Properties::global("hideall");
    if (hideall)
      geometry[i]->hideShowAll(true);
    //Reset static data
    geometry[i]->close();
  }
}

Model::~Model()
{
  clearTimeSteps();

  //Clear drawing objects
  for(unsigned int i=0; i<objects.size(); i++)
    if (objects[i]) delete objects[i];

  //Clear views
  for(unsigned int i=0; i<views.size(); i++)
    if (views[i]) delete views[i];

  //Clear colourmaps
  for(unsigned int i=0; i<colourMaps.size(); i++)
    if (colourMaps[i]) delete colourMaps[i];

  if (db) sqlite3_close(db);

  fignames.clear();
  figures.clear();
}

bool Model::loadFigure(int fig)
{
  if (fig < 0 || figures.size() == 0) return false;
  //Save currently selected first
  if (figure >= 0 && figures.size() > figure)
  {
    std::ostringstream json;
    jsonWrite(json);
    figures[figure] = json.str();
  }
  if (fig >= (int)figures.size()) fig = 0;
  if (fig < 0) fig = figures.size()-1;
  figure = fig;
  jsonRead(figures[figure]);

  //Set window caption
  if (fignames[figure].length() > 0)
    Properties::globals["caption"] = fignames[figure];
  return true;
}

void  Model::addObject(DrawingObject* obj)
{
  //Create master drawing object list entry
  obj->colourMaps = &colourMaps;
  objects.push_back(obj);
}

DrawingObject*  Model::findObject(unsigned int id)
{
  for (unsigned int i=0; i<objects.size(); i++)
    if (objects[i]->dbid == id) return objects[i];
  return NULL;
}

bool Model::open(bool write)
{
  //Single file database
  char path[FILE_PATH_MAX];
  int flags = write ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
  strcpy(path, file.full.c_str());
  if (strstr(path, "file:")) 
  {
    flags = flags | SQLITE_OPEN_URI;
    memorydb = true;
  }
  if (strstr(path, "mode=memory")) memorydb = true;
  debug_print("Opening db %s with flags %d\n", path, flags);
  if (sqlite3_open_v2(path, &db, flags, NULL))
  {
    debug_print("Can't open database %s: %s\n", path, sqlite3_errmsg(db));
    return false;
  }
  // success
  debug_print("Open database %s successful, SQLite version %s\n", path, sqlite3_libversion());
  //rc = sqlite3_busy_handler(db, busy_handler, void*);
  sqlite3_busy_timeout(db, 10000); //10 seconds

  if (write) readonly = false;

  return true;
}

void Model::reopen(bool write)
{
  if (!readonly || !db) return;
  if (db) sqlite3_close(db);
  open(write);

  //Re-attach any attached db file
  if (attached)
  {
    char SQL[SQL_QUERY_MAX];
    sprintf(SQL, "attach database '%s' as t%d", timesteps[now]->path.c_str(), attached);
    if (issue(SQL))
      debug_print("Model %s found and re-attached\n", timesteps[now]->path.c_str());
  }
}

void Model::attach(int stepidx)
{
  int timestep = timesteps[stepidx]->step;
  //Detach any attached db file
  if (memorydb) return;
  char SQL[SQL_QUERY_MAX];
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
    const std::string& path = timesteps[stepidx]->path;
    if (path.length() > 0)
    {
      sprintf(SQL, "attach database '%s' as t%d", path.c_str(), timestep);
      if (issue(SQL))
      {
        sprintf(prefix, "t%d.", timestep);
        debug_print("Model %s found and attached\n", path.c_str());
        attached = timestep;
      }
      else
      {
        debug_print("Model %s found but attach failed!\n", path.c_str());
      }
    }
    //else
    //   debug_print("Model %s not found, loading from current db\n", path.c_str());
  }
}

void Model::close()
{
  for (unsigned int i=0; i < geometry.size(); i++)
    geometry[i]->close();
}

void Model::clearObjects(bool all)
{
  if (membytes__ > 0 && geometry.size() > 0)
    debug_print("Clearing geometry, geom memory usage before clear %.3f mb\n", membytes__/1000000.0f);

  //Clear containers...
  for (unsigned int i=0; i < geometry.size(); i++)
  {
    geometry[i]->redraw = true;
    geometry[i]->clear(all);
  }
}

void Model::redraw(bool reload)
{
  //Flag redraw on all objects...
  for (unsigned int i=0; i < geometry.size(); i++)
  {
    if (reload) 
      //Flag redraw and clear element count to force colour reload...
      geometry[i]->reset();
    else
      //Just flag a redraw, will only be reloaded if vertex count changed
      geometry[i]->redraw = true;
  }


  for (unsigned int i = 0; i < colourMaps.size(); i++)
  {
    colourMaps[i]->calibrated = false;
  }
}

//Adds colourmap
unsigned int Model::addColourMap(ColourMap* cmap)
{
  if (!cmap)
  {
    //Create a default greyscale map
    cmap = new ColourMap();
    unsigned int colours[] = {0xff000000, 0xffffffff};
    cmap->add(colours, 2);
  }
  //Save colour map in list
  colourMaps.push_back(cmap);
  //Return index
  return colourMaps.size()-1;
}

void Model::loadWindows()
{
  //Load state from database if available
  sqlite3_stmt* statement = select("SELECT name, data from state ORDER BY id");
  while (sqlite3_step(statement) == SQLITE_ROW)
  {
    const char *name = (const char*)sqlite3_column_text(statement, 0);
    const char *data = (const char*)sqlite3_column_text(statement, 1);
    fignames.push_back(name);
    figures.push_back(data);
  }

  if (figures.size() > 0)
  {
    //Load the most recently added (last entry)
    //(so the previous state saved is restored on load)
    loadFigure(figures.size()-1);

    //Load object links (colourmaps)
    for (unsigned int o=0; o<objects.size(); o++)
      loadLinks(objects[o]);
  }
  else //Old db uses window structure
  {
    //Load windows list from database and insert into models
    statement = select("SELECT id,name,width,height,minX,minY,minZ,maxX,maxY,maxZ from window");
    //sqlite3_stmt* statement = model->select("SELECT id,name,width,height,colour,minX,minY,minZ,maxX,maxY,maxZ,properties from window");
    //window (id, name, width, height, colour, minX, minY, minZ, maxX, maxY, maxZ, properties)
    //Single window only supported now, use viewports for multiple
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
      std::string wtitle = std::string((char*)sqlite3_column_text(statement, 1));
      int width = sqlite3_column_int(statement, 2);
      int height = sqlite3_column_int(statement, 3);
      float min[3], max[3];
      for (int i=0; i<3; i++)
      {
        if (sqlite3_column_type(statement, 4+i) != SQLITE_NULL)
          min[i] = (float)sqlite3_column_double(statement, 4+i);
        else
          min[i] = FLT_MAX;
        if (sqlite3_column_type(statement, 7+i) != SQLITE_NULL)
          max[i] = (float)sqlite3_column_double(statement, 7+i);
        else
          max[i] = -FLT_MAX;
      }

      Properties::globals["caption"] = wtitle;
      Properties::globals["resolution"] = {width, height};
      Properties::globals["min"] = {min[0], min[1], min[2]};
      Properties::globals["max"] = {max[0], max[1], max[2]};

      //Link the window viewports, objects & colourmaps
      loadLinks();
    }
  }
  sqlite3_finalize(statement);
}

//Load model viewports
void Model::loadViewports()
{
  sqlite3_stmt* statement;
  statement = select("SELECT id,x,y,near,far FROM viewport ORDER BY y,x", true);

  //viewport:
  //(id, title, x, y, near, far, translateX,Y,Z, rotateX,Y,Z, scaleX,Y,Z, properties
  while (sqlite3_step(statement) == SQLITE_ROW)
  {
    float x = (float)sqlite3_column_double(statement, 2);
    float y = (float)sqlite3_column_double(statement, 3);
    float nearc = (float)sqlite3_column_double(statement, 4);
    float farc = (float)sqlite3_column_double(statement, 5);

    //Create the view object and add to list
    views.push_back(new View(x, y, nearc, farc));
    debug_print("Loaded viewport at %f,%f\n", x, y);
  }
  sqlite3_finalize(statement);
}

//Load and apply viewport camera settings
void Model::loadViewCamera(int viewport_id)
{
  int adj=0;
  //Load specified viewport and apply camera settings
  char SQL[SQL_QUERY_MAX];
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
    v->properties.parseSet(std::string(vprops));
    v->properties["coordsystem"] = orientation;
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

    //Create drawing object and add to master list
    std::string props = "";
    if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
      props = std::string((char*)sqlite3_column_text(statement, 4));
    DrawingObject* obj = new DrawingObject(otitle, props, -1, object_id);

    //Convert old colour/opacity from hard coded fields if provided
    int colour = 0x00000000;
    float opacity = -1;
    if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
    {
      colour = sqlite3_column_int(statement, 2);
      if (!obj->properties.has("colour")) obj->properties.data["colour"] = colour;
    }
    if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
    {
      opacity = (float)sqlite3_column_double(statement, 3);
      if (!obj->properties.has("opacity")) obj->properties.data["opacity"] = opacity;
    }

    addObject(obj);
  }
  sqlite3_finalize(statement);
}

//Load viewports in current window, objects in each viewport, colourmaps for each object
void Model::loadLinks()
{
  //Select statment to get all viewports in window and all objects in viewports
  char SQL[SQL_QUERY_MAX];
  //sprintf(SQL, "SELECT id,title,x,y,near,far,aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", win->id);
  sprintf(SQL, "SELECT viewport.id,object.id,object.colourmap_id,object_colourmap.colourmap_id,object_colourmap.data_type FROM window_viewport,viewport,viewport_object,object LEFT OUTER JOIN object_colourmap ON object_colourmap.object_id=object.id WHERE viewport_object.viewport_id=viewport.id AND object.id=viewport_object.object_id");
  sqlite3_stmt* statement = select(SQL, true); //Don't report errors as these tables are allowed to not exist

  int last_viewport = 0, last_object = 0;
  DrawingObject* draw = NULL;
  View* view = NULL;
  while ( sqlite3_step(statement) == SQLITE_ROW)
  {
    int viewport_id = sqlite3_column_int(statement, 0);
    int object_id = sqlite3_column_int(statement, 1);
    unsigned int colourmap_id = sqlite3_column_int(statement, 3); //Linked colourmap id

    //Fields from object_colourmap
    if (!colourmap_id)
    {
      //Backwards compatibility with old db files
      colourmap_id = sqlite3_column_int(statement, 2);
    }

    //Get viewport
    view = views[viewport_id-1];
    if (last_viewport != viewport_id)
    {
      loadViewCamera(viewport_id);
      last_viewport = viewport_id;
      last_object = 0;  //Reset required, in case of single object which is shared between viewports
    }

    //Get drawing object
    draw = findObject(object_id);
    if (!draw) continue; //No geometry
    if (last_object != object_id)
    {
      view->addObject(draw);
      last_object = object_id;
    }

    //Add colour maps to drawing objects...
    if (colourmap_id)
    {
      if (colourMaps.size() < colourmap_id || !colourMaps[colourmap_id-1])
        abort_program("Invalid colourmap id %d\n", colourmap_id);
      //Find colourmap by id == index
      //Add colourmap to drawing object
      draw->properties.data["colourmap"] = colourmap_id-1;
    }
  }
  sqlite3_finalize(statement);
}

//Load colourmaps for each object only
void Model::loadLinks(DrawingObject* obj)
{
  //Select statment to get all viewports in window and all objects in viewports
  char SQL[SQL_QUERY_MAX];
  //sprintf(SQL, "SELECT id,title,x,y,near,far,aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", win->id);
  sprintf(SQL, "SELECT object.id,object.colourmap_id,object_colourmap.colourmap_id,object_colourmap.data_type FROM object LEFT OUTER JOIN object_colourmap ON object_colourmap.object_id=object.id WHERE object.id=%d", obj->dbid);
  sqlite3_stmt* statement = select(SQL);

  while ( sqlite3_step(statement) == SQLITE_ROW)
  {
    unsigned int colourmap_id = sqlite3_column_int(statement, 2); //Linked colourmap id

    //Fields from object_colourmap
    if (!colourmap_id)
    {
      //Backwards compatibility with old db files
      colourmap_id = sqlite3_column_int(statement, 1);
    }

    //Add colour maps to drawing objects...
    if (colourmap_id > 0)
    {
      if (colourMaps.size() < colourmap_id || !colourMaps[colourmap_id-1])
        abort_program("Invalid colourmap id %d\n", colourmap_id);
      //Add colourmap to drawing object by index
      obj->properties.data["colourmap"] = colourmap_id-1;
    }
  }
  sqlite3_finalize(statement);
}

void Model::clearTimeSteps()
{
  for (unsigned int idx=0; idx < timesteps.size(); idx++)
  {
    //Clear the store first on current timestep to avoid deleting active (double free)
    if (idx == now) timesteps[idx]->cache.clear();
    delete timesteps[idx];
  }
  timesteps.clear();
}

int Model::loadTimeSteps(bool scan)
{
  //Strip any digits from end of filename to get base
  basename = file.base.substr(0, file.base.find_last_not_of("0123456789") + 1);

  //Don't reload timesteps when data has been cached
  if (TimeStep::cachesize > 0 && timesteps.size() > 0) return timesteps.size();
  clearTimeSteps();
  TimeStep::gap = 0;
  int rows = 0;
  int last_step = 0;

  if (!scan && db)
  {
    sqlite3_stmt* statement = select("SELECT * FROM timestep");
    //(id, time, dim_factor, units)
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
      int step = sqlite3_column_int(statement, 0);
      double time = sqlite3_column_double(statement, 1);
      addTimeStep(step, time);
      //Save gap
      if (step - last_step > TimeStep::gap) TimeStep::gap = step - last_step;
      last_step = step;

      //Look for additional db file (minimum 3 digit padded step in names)
      std::string path = checkFileStep(step, basename, 3);
      if (path.length() > 0)
      {
        debug_print("Found step %d database %s\n", step, path.c_str());
        timesteps[rows]->path = path;
      }

      rows++;
    }
    sqlite3_finalize(statement);
  }

  //Assume we have at least one current timestep, even if none in table
  if (timesteps.size() == 0) addTimeStep();

  //Check for other timesteps in external files
  if (scan || timesteps.size() == 1)
  {
    debug_print("Scanning for timesteps...\n");
    for (unsigned int ts=0; ts<10000; ts++)
    {
      //If no steps found after trying 100, give up scanning
      if (timesteps.size() < 2 && ts > 100) break;
      int len = (ts == 0 ? 1 : (int)log10((float)ts) + 1);
      std::string path = checkFileStep(ts, basename);
      if (path.length() > 0)
      {
        debug_print("Found step %d database %s\n", ts, path.c_str());
        if (path == file.full && timesteps.size() == 1)
        {
          //Update step if this is the initial db file
          timesteps[0]->step = ts;
          timesteps[0]->path = path;
        }
        else
          addTimeStep(ts, 0.0, path);
      }
    }
    debug_print("Scanning complete, found %d steps.\n", timesteps.size());
  }

  //Copy to static for use in Tracers etc
  if (infostream) std::cerr << timesteps.size() << " timesteps loaded\n";
  TimeStep::timesteps = timesteps;
  return timesteps.size();
}

std::string Model::checkFileStep(unsigned int ts, const std::string& basename, unsigned int limit)
{
  int len = (ts == 0 ? 1 : (int)log10((float)ts) + 1);
  //Lower limit of digits to look for, default 1-5
  if (len < limit) len = limit;
  for (int w = 5; w >= len; w--)
  {
    std::ostringstream ss;
    ss << file.path << basename << std::setw(w) << std::setfill('0') << ts;
    ss << "." << file.ext;
    std::string path = ss.str();
    if (FileExists(path))
      return path;
  }
  return "";
}

void Model::loadColourMaps()
{
  sqlite3_stmt* statement = select("select count(*) from colourvalue");
  if (statement) return loadColourMapsLegacy();

  //New databases have only a colourmap table with colour data in properties
  statement = select("SELECT id,name,minimum,maximum,logscale,discrete,properties FROM colourmap");
  double minimum;
  double maximum;
  ColourMap* colourMap = NULL;
  while ( sqlite3_step(statement) == SQLITE_ROW)
  {
    char *cmname = (char*)sqlite3_column_text(statement, 1);
    minimum = sqlite3_column_double(statement, 2);
    maximum = sqlite3_column_double(statement, 3);
    int logscale = sqlite3_column_int(statement, 4);
    int discrete = sqlite3_column_int(statement, 5);
    char *props = (char*)sqlite3_column_text(statement, 6);
    colourMap = new ColourMap(cmname, minimum, maximum, props ? props : "");
    if (logscale) colourMap->properties.data["logscale"] = true;
    if (discrete) colourMap->properties.data["discrete"] = true;
    colourMaps.push_back(colourMap);
  }

  sqlite3_finalize(statement);

  //Initial calibration for all maps
  for (unsigned int i=0; i<colourMaps.size(); i++)
    colourMaps[i]->calibrate();
}

void Model::loadColourMapsLegacy()
{
  //Handles old databases with colourvalue table
  bool old = false;
  sqlite3_stmt* statement = select("SELECT colourmap.id,minimum,maximum,logscale,discrete,colour,value,name,properties FROM colourmap,colourvalue WHERE colourvalue.colourmap_id=colourmap.id");
  if (statement == NULL)
  {
    //Old DB format, had no name or props
    statement = select("SELECT colourmap.id,minimum,maximum,logscale,discrete,colour,value FROM colourmap,colourvalue WHERE colourvalue.colourmap_id=colourmap.id");
    old = true;
  }
  //colourmap (id, minimum, maximum, logscale, discrete, centreValue)
  //colourvalue (id, colourmap_id, colour, position)
  int map_id = 0;
  double minimum;
  double maximum;
  bool parsed = false;
  ColourMap* colourMap = NULL;
  while ( sqlite3_step(statement) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(statement, 0);
    char idname[10];
    sprintf(idname, "%d", id);
    char *cmname = NULL;
    if (!old) cmname = (char*)sqlite3_column_text(statement, 7);

    //New map?
    if (id != map_id)
    {
      map_id = id;
      minimum = sqlite3_column_double(statement, 1);
      maximum = sqlite3_column_double(statement, 2);
      int logscale = sqlite3_column_int(statement, 3);
      int discrete = sqlite3_column_int(statement, 4);
      char *props = NULL;
      if (!old) props = (char*)sqlite3_column_text(statement, 8);
      colourMap = new ColourMap(cmname ? cmname : idname, minimum, maximum, props ? props : "");
      colourMaps.push_back(colourMap);
      if (logscale) colourMap->properties.data["logscale"] = true;
      if (discrete) colourMap->properties.data["discrete"] = true;
      //Colours already parsed from properties?
      if (colourMap->colours.size() > 0) parsed = true;
      else parsed = false;
    }

    if (!parsed)
    {
      //Add colour value
      int colour = sqlite3_column_int(statement, 5);
      //const char *name = sqlite3_column_name(statement, 7);
      if (sqlite3_column_type(statement, 6) != SQLITE_NULL)
      {
        double value = sqlite3_column_double(statement, 6);
        colourMap->add(colour, value);
      }
      else
        colourMap->add(colour);
    }
    //debug_print("ColourMap: %d min %f, max %f Value %d \n", id, minimum, maximum, colour);
  }

  sqlite3_finalize(statement);

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

bool Model::issue(const char* SQL, sqlite3* odb)
{
  if (!odb) odb = db; //Use existing database
  // Executes a basic SQLite command (ie: without pointer objects and ignoring result sets) and checks for errors
  //debug_print("Issuing: %s\n", SQL);
  char* zErrMsg = NULL;
  if (sqlite3_exec(odb, SQL, NULL, 0, &zErrMsg) != SQLITE_OK)
  {
    std::cerr << "SQLite error: " << (zErrMsg ? zErrMsg : "(no error msg)") << std::endl;
    std::cerr << " -- " << SQL << std::endl;
    sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

void Model::freeze()
{
  //Freeze fixed geometry
  TimeStep::freeze(geometry);

  //Need new geometry containers after freeze
  //(Or new data will be appended to the frozen containers!)
  init();
  if (timesteps.size() == 0) addTimeStep();
  timesteps[now]->loadFixed(geometry);
}

void Model::deleteCache()
{
  if (TimeStep::cachesize == 0) return;
  debug_print("~~~ Cache emptied\n");
  //cache.clear();
}

void Model::cacheStep()
{
  //Don't cache if we already loaded from cache or out of range!
  if (TimeStep::cachesize == 0 || now < 0 || (int)timesteps.size() <= now) return;
  if (timesteps[now]->cache.size() > 0) return; //Already cached this step

  printf("~~~ Caching geometry @ %d (step %d : %s), geom memory usage: %.3f mb\n", step(), now, file.base.c_str(), membytes__/1000000.0f);

  //Copy all elements
  if (membytes__ > 0)
  {
    timesteps[now]->write(geometry);
    debug_print("~~~ Cached step, at: %d\n", step());
    geometry.clear();
  }
  else
    debug_print("~~~ Nothing to cache\n");

  //TODO: fix support for partial caching?
  /*/Remove if over limit
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
  }*/
}

bool Model::restoreStep()
{
  if (now < 0 || TimeStep::cachesize == 0) return false;
  if (timesteps[now]->cache.size() == 0)
    return false; //Nothing cached this step

  //Load the cache and save loaded timestep
  timesteps[now]->read(geometry);
  debug_print("~~~ Cache hit at ts %d (idx %d), loading! %s\n", step(), now, file.base.c_str());

  //Switch geometry containers
  labels = geometry[lucLabelType];
  points = (Points*)geometry[lucPointType];
  vectors = (Vectors*)geometry[lucVectorType];
  tracers = (Tracers*)geometry[lucTracerType];
  quadSurfaces = (QuadSurfaces*)geometry[lucGridType];
  volumes = (Volumes*)geometry[lucVolumeType];
  triSurfaces = (TriSurfaces*)geometry[lucTriangleType];
  lines = (Lines*)geometry[lucLineType];
  shapes = (Shapes*)geometry[lucShapeType];

  debug_print("~~~ Geom memory usage after load: %.3f mb\n", membytes__/1000000.0f);
  redraw(true);  //Force reload
  return true;
}

void Model::printCache()
{
  debug_print("-----------CACHE %d steps\n", timesteps.size());
  for (unsigned int idx=0; idx < timesteps.size(); idx++)
    debug_print(" %d: has %d records\n", idx, timesteps[idx]->cache.size());
}

//Set time step if available, otherwise return false and leave unchanged
bool Model::hasTimeStep(int ts)
{
  if (timesteps.size() == 0 && loadTimeSteps() == 0) return false;
  for (unsigned int idx=0; idx < timesteps.size(); idx++)
    if (ts == timesteps[idx]->step)
      return true;
  return false;
}

int Model::nearestTimeStep(int requested)
{
  //Find closest matching timestep to requested, returns index
  int idx;
  if (timesteps.size() == 0 && loadTimeSteps() == 0) return -1;
  //if (loadTimeSteps() == 0 || timesteps.size() == 0) return -1;
  //if (timesteps.size() == 1 && now >= 0 && ) return -1;  //Single timestep

  for (idx=0; idx < (int)timesteps.size(); idx++)
    if (timesteps[idx]->step >= requested) break;

  //Reached end of list?
  if (idx == (int)timesteps.size()) idx--;

  if (idx < 0) idx = 0;
  if (idx >= (int)timesteps.size()) idx = timesteps.size() - 1;

  return idx;
}

//Load data at specified timestep
int Model::setTimeStep(int stepidx)
{
  int rows = 0;
  clock_t t1 = clock();

  //Default timestep only? Skip load
  if (timesteps.size() == 0)
  {
    now = -1;
    return -1;
  }

  if (stepidx < 0) stepidx = 0; //return -1;
  if (stepidx >= (int)timesteps.size())
    stepidx = timesteps.size()-1;

  //Unchanged...
  if (now >= 0 && stepidx == now) return -1;

  //Setting initial step?
  bool first = (now < 0);

  //Cache currently loaded data
  if (TimeStep::cachesize > 0) cacheStep();

  //Set the new timestep index
  TimeStep::timesteps = timesteps; //Set to current model timestep vector
  now = stepidx;
  debug_print("TimeStep set to: %d (%d)\n", step(), stepidx);

  if (!restoreStep())
  {
    //Create new geometry containers if required
    if (geometry.size() == 0) init();

    if (first)
      //Freeze any existing geometry as non time-varying when first step loaded
      freeze();
    else
      //Normally just clear any existing geometry
      clearObjects();

    //Import fixed data first
    if (now > 0) 
      timesteps[now]->loadFixed(geometry);

    //Attempt to load from cache first
    //if (restoreStep(now)) return 0; //Cache hit successful return value
    if (db)
    {
      //Detach any attached db file and attach n'th timestep database if available
      attach(now);

      if (TimeStep::cachesize > 0)
        //Attempt caching all geometry from database at start
        rows += loadGeometry(0, 0, timesteps[timesteps.size()-1]->step, true);
      else
        rows += loadGeometry();

      debug_print("%.4lf seconds to load %d geometry records from database\n", (clock()-t1)/(double)CLOCKS_PER_SEC, rows);
    }
  }

  return rows;
}

int Model::loadGeometry(int obj_id, int time_start, int time_stop, bool recurseTracers)
{
  if (!db)
  {
    std::cerr << "No database loaded!!\n";
    return 0;
  }
  clock_t t1 = clock();

  //Default to current timestep
  if (time_start < 0) time_start = step();
  if (time_stop < 0) time_stop = step();

  //Load geometry
  char SQL[SQL_QUERY_MAX];
  char filter[256] = {'\0'};
  char objfilter[32] = {'\0'};

  //Setup filters, object...
  if (obj_id > 0)
  {
    sprintf(objfilter, "WHERE object_id=%d", obj_id);
    //Remove the skip flag now we have explicitly loaded object
    DrawingObject* obj = findObject(obj_id);
    if (obj) obj->skip = false;
  }

  //...timestep...(if ts db attached, assume all geometry is at current step)
  if (time_start >= 0 && time_stop >= 0 && !attached)
  {
    if (strlen(objfilter) > 0)
      sprintf(filter, "%s AND timestep BETWEEN %d AND %d", objfilter, time_start, time_stop);
    else
      sprintf(filter, " WHERE timestep BETWEEN %d AND %d", time_start, time_stop);
  }
  else
    strcpy(filter, objfilter);

  int datacol = 21;
  //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
  //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, labels,
  //minX, minY, minZ, maxX, maxY, maxZ, data)
  sprintf(SQL, "SELECT id,object_id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,minX,minY,minZ,maxX,maxY,maxZ,data FROM %sgeometry %s ORDER BY timestep,object_id,idx,rank", prefix, filter);
  sqlite3_stmt* statement = select(SQL, true);

  //Old database compatibility
  if (statement == NULL)
  {
    //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
    //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, data)
    sprintf(SQL, "SELECT id,object_id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,data FROM %sgeometry %s ORDER BY timestep,object_id,idx,rank", prefix, filter);
    statement = select(SQL, true);
    datacol = 15;

    //Fix
#ifdef ALTER_DB
    reopen(true);  //Open writable
    sprintf(SQL, "ALTER TABLE %sgeometry ADD COLUMN minX REAL; ALTER TABLE %sgeometry ADD COLUMN minY REAL; ALTER TABLE %sgeometry ADD COLUMN minZ REAL; "
            "ALTER TABLE %sgeometry ADD COLUMN maxX REAL; ALTER TABLE %sgeometry ADD COLUMN maxY REAL; ALTER TABLE %sgeometry ADD COLUMN maxZ REAL; ",
            prefix, prefix, prefix, prefix, prefix, prefix, prefix);
    debug_print("%s\n", SQL);
    issue(SQL);
#endif
  }

  //Very old database compatibility
  if (statement == NULL)
  {
    sprintf(SQL, "SELECT id,object_id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,data FROM %sgeometry %s ORDER BY timestep,object_id,idx,rank", prefix, filter);
    statement = select(SQL);
    datacol = 14;
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
      int object_id = sqlite3_column_int(statement, 1);
      int timestep = sqlite3_column_int(statement, 2);
      int height = sqlite3_column_int(statement, 3);  //unused - was rank, now height
      int depth = sqlite3_column_int(statement, 4); //unused - was idx, now depth
      lucGeometryType type = (lucGeometryType)sqlite3_column_int(statement, 5);
      lucGeometryDataType data_type = (lucGeometryDataType)sqlite3_column_int(statement, 6);
      int size = sqlite3_column_int(statement, 7);
      int count = sqlite3_column_int(statement, 8);
      int items = count / size;
      int width = sqlite3_column_int(statement, 9);
      if (height == 0) height = width > 0 ? items / width : 0;
      float minimum = (float)sqlite3_column_double(statement, 10);
      float maximum = (float)sqlite3_column_double(statement, 11);
      //New fields for the scaling features, applied when drawing colour bars
      //float dimFactor = (float)sqlite3_column_double(statement, 12);
      //const char *units = (const char*)sqlite3_column_text(statement, 13);
      const char *labels = datacol < 15 ? "" : (const char*)sqlite3_column_text(statement, 14);

      const void *data = sqlite3_column_blob(statement, datacol);
      unsigned int bytes = sqlite3_column_bytes(statement, datacol);

      DrawingObject* obj = findObject(object_id);

      //Deleted or Skip object? (When noload enabled)
      if (!obj || obj->skip) continue;

      //Bulk load: switch timestep and cache if timestep changes!
      if (step() != timestep && !attached) //Will not work with attached db
      {
        cacheStep();
        now = nearestTimeStep(timestep);
        debug_print("TimeStep set to: %d, rows %d\n", step(), rows);
      }

      //Create new geometry containers if required
      if (geometry.size() == 0) init();

      if (type == lucTracerType)
      {
        height = 0;
        //Default particle count:
        if (width == 0) width = items;
      }

      //Create object and set parameters
      if (type == lucPointType && Properties::global("pointspheres")) type = lucShapeType;
      //if (type == lucGridType) type = lucTriangleType;
      active = geometry[type];

      if (recurseTracers && type == lucTracerType)
      {
        //if (datacol == 13) continue;  //Don't bother supporting tracers from old dbs
        //Only load tracer timesteps when on the vertex data object or will repeat for every type found
        if (data_type != lucVertexData) continue;

        //Tracers are loaded with a new select statement across multiple timesteps...
        //objects[object_id]->steps = timestep+1;
        //Tracers* tracers = (Tracers*)active;
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
          unsigned long dst_len = (unsigned long)(count * sizeof(float));
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

        //Always add a new element for each new vertex geometry record, not suitable if writing db on multiple procs!
        if (data_type == lucVertexData && recurseTracers) active->add(obj);

        //Read data block
        GeomData* g = active->read(obj, items, data_type, data, width, height, depth);
        active->setup(obj, data_type, minimum, maximum);
        if (labels) active->label(obj, labels);

        //Where min/max vertex provided, load
        if (data_type == lucVertexData && type != lucLabelType)
        {
          float min[3] = {0,0,0}, max[3] = {0,0,0};
          if (datacol > 15 && sqlite3_column_type(statement, 15) != SQLITE_NULL)
          {
            for (int i=0; i<3; i++)
            {
              min[i] = (float)sqlite3_column_double(statement, 15+i);
              max[i] = (float)sqlite3_column_double(statement, 18+i);
            }
          }

          //Detect null dims data due to bugs in dimension output
          if (min[0] != max[0] || min[1] != max[1] || min[2] != max[2])
          {
            g->checkPointMinMax(min);
            g->checkPointMinMax(max);
          }
          else
          {
            //Slow way, detects bounding box by checking each vertex
            for (int p=0; p < items*3; p += 3)
              g->checkPointMinMax((float*)data + p);

            debug_print("No bounding dims provided for object %d, calculated for %d vertices...%f,%f,%f - %f,%f,%f\n", obj->dbid, items, g->min[0], g->min[1], g->min[2], g->max[0], g->max[1], g->max[2]);

            //Fix for future loads
#ifdef ALTER_DB
            reopen(true);  //Open writable
            sprintf(SQL, "UPDATE %sgeometry SET minX = '%f', minY = '%f', minZ = '%f', maxX = '%f', maxY = '%f', maxZ = '%f' WHERE id==%d;",
                    prefix, id, obj->min[0], obj->min[1], obj->min[2], obj->max[0], obj->max[1], obj->max[2]);
            printf("%s\n", SQL);
            issue(SQL);
#endif
          }
        }



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
  }
  while (ret == SQLITE_ROW);

  sqlite3_finalize(statement);
  debug_print("... loaded %d rows, %d bytes, %.4lf seconds\n", rows, tbytes, (clock()-t1)/(double)CLOCKS_PER_SEC);

  return rows;
}

void Model::mergeDatabases()
{
  if (!db) return;
  char SQL[SQL_QUERY_MAX];
  reopen(true);  //Open writable
  for (unsigned int i=0; i<=timesteps.size(); i++)
  {
    debug_print("MERGE %d/%d...%d\n", i, timesteps.size(), step());
    setTimeStep(i);
    if (attached == step())
    {
      snprintf(SQL, SQL_QUERY_MAX, "insert into geometry select null, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, labels, properties, data, minX, minY, minZ, maxX, maxY, maxZ from %sgeometry", prefix);
      issue(SQL);
    }
  }
}

int Model::decompressGeometry(int timestep)
{
  if (!db) return 0;
  reopen(true);  //Open writable
  clock_t t1 = clock();
  //Load geometry
  char SQL[SQL_QUERY_MAX];

  sprintf(SQL, "SELECT id,count,data FROM %sgeometry WHERE timestep=%d ORDER BY idx,rank", prefix, timestep);
  sqlite3_stmt* statement = select(SQL, true);

  if (!statement) return 0;
  int rows = 0;
  int tbytes = 0;
  int ret;
  do
  {
    ret = sqlite3_step(statement);
    if (ret == SQLITE_ROW)
    {
      rows++;
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
        snprintf(SQL, SQL_QUERY_MAX, "UPDATE %sgeometry SET data = ? WHERE id==%d;", prefix, id);
        printf("%s\n", SQL);
        /* Prepare statement... */
        if (sqlite3_prepare_v2(db, SQL, -1, &statement2, NULL) != SQLITE_OK)
        {
          printf("SQL prepare error: (%s) %s\n", SQL, sqlite3_errmsg(db));
          abort(); //Database errors fatal?
          return 0;
        }
        /* Setup blob data for insert */
        if (sqlite3_bind_blob(statement2, 1, buffer, bytes, SQLITE_STATIC) != SQLITE_OK)
        {
          printf("SQL bind error: %s\n", sqlite3_errmsg(db));
          abort(); //Database errors fatal?
        }
        /* Execute statement */
        if (sqlite3_step(statement2) != SQLITE_DONE )
        {
          printf("SQL step error: (%s) %s\n", SQL, sqlite3_errmsg(db));
          abort(); //Database errors fatal?
        }
        sqlite3_finalize(statement2);
#endif
      }

      if (buffer) delete[] buffer;

    }
    else if (ret != SQLITE_DONE)
      printf("DB STEP FAIL %d %d\n", ret, (ret>>8));
  }
  while (ret == SQLITE_ROW);

  sqlite3_finalize(statement);
  debug_print("... modified %d rows, %d bytes, %.4lf seconds\n", rows, tbytes, (clock()-t1)/(double)CLOCKS_PER_SEC);

  return rows;
}

void Model::writeDatabase(const char* path, DrawingObject* obj, bool compress)
{
  //Write objects to a new database
  sqlite3 *outdb;
  if (sqlite3_open_v2(path, &outdb, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL))
  {
    debug_print("Can't open database %s: %s\n", path, sqlite3_errmsg(outdb));
    return;
  }

  // Remove existing data?
  issue("drop table IF EXISTS geometry", outdb);
  issue("drop table IF EXISTS timestep", outdb);
  issue("drop table IF EXISTS object_colourmap", outdb);
  issue("drop table IF EXISTS colourmap", outdb);
  issue("drop table IF EXISTS object", outdb);
  issue("drop table IF EXISTS state", outdb);

  // Create new tables when not present
  issue("create table IF NOT EXISTS geometry (id INTEGER PRIMARY KEY ASC, object_id INTEGER, timestep INTEGER, rank INTEGER, idx INTEGER, type INTEGER, data_type INTEGER, size INTEGER, count INTEGER, width INTEGER, minimum REAL, maximum REAL, dim_factor REAL, units VARCHAR(32), minX REAL, minY REAL, minZ REAL, maxX REAL, maxY REAL, maxZ REAL, labels VARCHAR(2048), properties VARCHAR(2048), data BLOB, FOREIGN KEY (object_id) REFERENCES object (id) ON DELETE CASCADE ON UPDATE CASCADE, FOREIGN KEY (timestep) REFERENCES timestep (id) ON DELETE CASCADE ON UPDATE CASCADE)", outdb);

  issue(
    "create table IF NOT EXISTS timestep (id INTEGER PRIMARY KEY ASC, time REAL, dim_factor REAL, units VARCHAR(32), properties VARCHAR(2048))", outdb);

  issue(
    "create table object (id INTEGER PRIMARY KEY ASC, name VARCHAR(256), colourmap_id INTEGER, colour INTEGER, opacity REAL, properties VARCHAR(2048), FOREIGN KEY (colourmap_id) REFERENCES colourmap (id) ON DELETE CASCADE ON UPDATE CASCADE)", outdb);

  issue(
    "create table colourmap (id INTEGER PRIMARY KEY ASC, name VARCHAR(256), minimum REAL, maximum REAL, logscale INTEGER, discrete INTEGER, centreValue REAL, properties VARCHAR(2048))", outdb);

  //Write state
  writeState(outdb);

  issue("BEGIN EXCLUSIVE TRANSACTION", outdb);

  char SQL[SQL_QUERY_MAX];

  //Write objects
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (!obj || objects[i] == obj)
    {
      snprintf(SQL, SQL_QUERY_MAX, "insert into object (name, properties) values ('%s', '%s')", objects[i]->name().c_str(), objects[i]->properties.data.dump().c_str());
      //printf("%s\n", SQL);
      if (!issue(SQL, outdb)) return;
      //Store the id
      objects[i]->dbid = sqlite3_last_insert_rowid(outdb);
    }
  }

  //Write timesteps & objects...
  if (timesteps.size() == 0)
  {
    //Create a timestep and
    if (!issue("insert into timestep (id, time) values (0, 0)", outdb)) return;
    writeObjects(outdb, obj, 0, compress);
  }

  for (unsigned int i = 0; i < timesteps.size(); i++)
  {
    snprintf(SQL, SQL_QUERY_MAX, "insert into timestep (id, time, properties) values (%d, %g, '%s')", timesteps[i]->step, timesteps[i]->time, "");
    //printf("%s\n", SQL);
    if (!issue(SQL, outdb)) return;

    //Get data at this timestep
    setTimeStep(i);

    //Write object data
    writeObjects(outdb, obj, step(), compress);
  }

  issue("COMMIT", outdb);
}

void Model::writeState(sqlite3* outdb)
{
  //Write state
  if (!outdb) outdb = db; //Use existing database
  issue("create table if not exists state (id INTEGER PRIMARY KEY ASC, name VARCHAR(256), data TEXT)", outdb);

  std::stringstream ss;
  jsonWrite(ss, 0, false);
  std::string state = ss.str();

  //Add default figure
  if (figure < 0)
  {
    figure = 0;
    if (figures.size() == 0)
    {
      fignames.push_back("default");
      figures.push_back(state);
    }
  }

  char SQL[SQL_QUERY_MAX];

  // Delete any state entry with same name
  snprintf(SQL, SQL_QUERY_MAX,  "delete from state where name == '%s'", fignames[figure].c_str());
  issue(SQL, outdb);

  snprintf(SQL, SQL_QUERY_MAX, "insert into state (name, data) values ('%s', ?)", fignames[figure].c_str());
  sqlite3_stmt* statement;

  if (sqlite3_prepare_v2(outdb, SQL, -1, &statement, NULL) != SQLITE_OK)
    abort_program("SQL prepare error: (%s) %s\n", SQL, sqlite3_errmsg(outdb));

  if (sqlite3_bind_text(statement, 1, state.c_str(), state.length(), SQLITE_STATIC) != SQLITE_OK)
    abort_program("SQL bind error: %s\n", sqlite3_errmsg(db));

  if (sqlite3_step(statement) != SQLITE_DONE )
    abort_program("SQL step error: (%s) %s\n", SQL, sqlite3_errmsg(db));

  sqlite3_finalize(statement);
}

void Model::writeObjects(sqlite3* outdb, DrawingObject* obj, int step, bool compress)
{
  //Write object data
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (!obj || obj == objects[i])
    {
      //Loop through all geometry classes (points/vectors etc)
      for (int type=lucMinType; type<lucMaxType; type++)
      {
        writeGeometry(outdb, (lucGeometryType)type, objects[i], step, compress);
      }
    }
  }
}

void Model::writeGeometry(sqlite3* outdb, lucGeometryType type, DrawingObject* obj, int step, bool compressdata)
{
  std::vector<GeomData*> data = geometry[type]->getAllObjects(obj);
  //Loop through and write out all object data
  char SQL[SQL_QUERY_MAX];
  unsigned int i, data_type;
  for (i=0; i<data.size(); i++)
  {
    for (data_type=0; data_type<data[i]->data.size(); data_type++)
    {
      if (!data[i]->data[data_type] || data[i]->data[data_type]->size() == 0) continue;
      std::cerr << "Writing geometry (type[" << data_type << "] * " << data[i]->data[data_type]->size()
                << ") for object : " << obj->dbid << " => " << obj->name() << std::endl;
      //Get the data block
      DataContainer* block = data[i]->data[data_type];

      sqlite3_stmt* statement;
      unsigned char* buffer = (unsigned char*)block->ref(0);
      unsigned long src_len = block->size() * sizeof(float);
      // Compress the data if enabled and > 1kb
      unsigned long cmp_len = 0;
      if (compressdata &&  src_len > 1000)
      {
        cmp_len = compressBound(src_len);
        buffer = (unsigned char*)malloc((size_t)cmp_len);
        if (buffer == NULL)
          abort_program("Compress database: out of memory!\n");
        if (compress(buffer, &cmp_len, (const unsigned char *)block->ref(0), src_len) != Z_OK)
          abort_program("Compress database buffer failed!\n");
        if (cmp_len >= src_len)
        {
          free(buffer);
          buffer = (unsigned char*)block->ref(0);
          cmp_len = 0;
        }
        else
          src_len = cmp_len;
      }

      if (block->minimum == HUGE_VAL) block->minimum = 0;
      if (block->maximum == -HUGE_VAL) block->maximum = 0;

      float min[3], max[3];
      for (int c=0; c<3; c++)
      {
        if (ISFINITE(data[i]->min[c])) min[c] = data[i]->min[c];
        else data[i]->min[c] = Geometry::min[c];
        if (ISFINITE(data[i]->max[c])) max[c] = data[i]->max[c];
        else data[i]->max[c] = Geometry::max[c];

      }

      snprintf(SQL, SQL_QUERY_MAX, "insert into geometry (object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, minX, minY, minZ, maxX, maxY, maxZ, labels, data) values (%d, %d, %d, %d, %d, %d, %d, %d, %d, %g, %g, %g, '%s', %g, %g, %g, %g, %g, %g, ?, ?)", obj->dbid, step, data[i]->height, data[i]->depth, type, data_type, block->datasize, block->size(), data[i]->width, block->minimum, block->maximum, 0.0, "", min[0], min[1], min[2], max[0], max[1], max[2]);

      /* Prepare statement... */
      if (sqlite3_prepare_v2(outdb, SQL, -1, &statement, NULL) != SQLITE_OK)
      {
        abort_program("SQL prepare error: (%s) %s\n", SQL, sqlite3_errmsg(outdb));
      }

      /* Setup text data for insert (on vertex block only) */
      std::string labels = data[i]->getLabels().c_str();
      if (data_type == lucVertexData && labels.length() > 0)
      {
        if (sqlite3_bind_text(statement, 1, labels.c_str(), labels.length(), SQLITE_STATIC) != SQLITE_OK)
        //const char* clabels = labels.c_str();
        //if (sqlite3_bind_text(statement, 1, clabels, strlen(clabels), SQLITE_STATIC) != SQLITE_OK)
          abort_program("SQL bind error: %s\n", sqlite3_errmsg(outdb));
      }

      /* Setup blob data for insert */
      if (sqlite3_bind_blob(statement, 2, buffer, src_len, SQLITE_STATIC) != SQLITE_OK)
        abort_program("SQL bind error: %s\n", sqlite3_errmsg(outdb));

      /* Execute statement */
      if (sqlite3_step(statement) != SQLITE_DONE )
        abort_program("SQL step error: (%s) %s\n", SQL, sqlite3_errmsg(outdb));

      sqlite3_finalize(statement);

      // Free compression buffer
      if (cmp_len > 0) free(buffer);
    }
  }
}

void Model::deleteObject(unsigned int id)
{
  if (!db) return;
  reopen(true);  //Open writable
  char SQL[SQL_QUERY_MAX];
  snprintf(SQL, SQL_QUERY_MAX, "DELETE FROM object WHERE id==%1$d; DELETE FROM geometry WHERE object_id=%1$d; DELETE FROM viewport_object WHERE object_id=%1$d;", id);
  issue(SQL);
  issue("vacuum");
}

void Model::backup(sqlite3 *fromDb, sqlite3* toDb)
{
  if (!db) return;
  sqlite3_backup *pBackup;  // Backup object used to copy data
  pBackup = sqlite3_backup_init(toDb, "main", fromDb, "main");
  if (pBackup)
  {
    (void)sqlite3_backup_step(pBackup, -1);
    (void)sqlite3_backup_finish(pBackup);
  }
}

void Model::objectBounds(DrawingObject* draw, float* min, float* max)
{
  if (!min || !max) return;
  for (int i=0; i<3; i++)
    max[i] = -(min[i] = HUGE_VAL);
  //Expand bounds by all geometry objects
  for (unsigned int i=0; i < geometry.size(); i++)
    geometry[i]->objectBounds(draw, min, max);
}

void Model::jsonWrite(std::ostream& os, DrawingObject* obj, bool objdata)
{
  //Write new JSON format objects
  // - globals are all stored on / sourced from Properties::globals
  // - views[] list holds view properies (previously single instance in "options")
  json exported;
  json properties = Properties::globals;
  json cmaps = json::array();
  json outobjects = json::array();
  json outviews = json::array();

  //Converts named colours to js readable
  if (properties.count("background") > 0)
    properties["background"] = Colour(properties["background"]).toString();

  for (unsigned int v=0; v < views.size(); v++)
  {
    View* view = views[v];
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
      scale.push_back(view->scale[i]);
    }

    vprops["rotate"] = rot;
    vprops["translate"] = trans;
    vprops["focus"] = foc;
    vprops["scale"] = scale;

    vprops["near"] = view->near_clip;
    vprops["far"] = view->far_clip;

    //Converts named colours to js readable
    if (vprops.count("background") > 0)
      vprops["background"] = Colour(vprops["background"]).toString();

    //Add the view
    outviews.push_back(vprops);
  }

  for (unsigned int i = 0; i < colourMaps.size(); i++)
  {
    json cmap = colourMaps[i]->properties.data;
    json colours;

    if (!colourMaps[i]->calibrated)
      colourMaps[i]->calibrate();

    for (unsigned int c=0; c < colourMaps[i]->colours.size(); c++)
    {
      json colour;
      colour["position"] = colourMaps[i]->colours[c].position;
      colour["colour"] = colourMaps[i]->colours[c].colour.toString();
      colours.push_back(colour);
    }

    cmap["name"] = colourMaps[i]->name;
    cmap["colours"] = colours;

    cmaps.push_back(cmap);
  }

  //if (!viewer->visible) aview->filtered = false; //Disable viewport filtering
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (!obj || objects[i] == obj)
    {
      //Only able to dump point/triangle based objects currently:
      //TODO: fix to use sub-renderer output for others
      //"Labels", "Points", "Grid", "Triangles", "Vectors", "Tracers", "Lines", "Shapes", "Volumes"

      json obj = objects[i]->properties.data;

      //Include the object bounding box for WebGL
      float min[3], max[3];
      objectBounds(objects[i], min, max);
      obj["min"] = {min[0], min[1], min[2]};
      obj["max"] = {max[0], max[1], max[2]};

      //Texture ? Export first only, as external file for now
      //TODO: dataurl using getImageString(image, iw, ih, bpp)
      if (objects[i]->textures.size() > 0)
        obj["texture"] = objects[i]->textures[0]->fn.full;

      if (!objdata)
      {
        if (Model::points->getVertexCount(objects[i]) > 0) obj["points"] = true;
        if (Model::quadSurfaces->getVertexCount(objects[i]) > 0 ||
            Model::triSurfaces->getVertexCount(objects[i]) > 0 ||
            Model::vectors->getVertexCount(objects[i]) > 0 ||
            Model::tracers->getVertexCount(objects[i]) > 0 ||
            Model::shapes->getVertexCount(objects[i]) > 0) obj["triangles"] = true;
        if (Model::lines->getVertexCount(objects[i]) > 0) obj["lines"] = true;
        if (Model::volumes->getVertexCount(objects[i]) > 0) obj["volume"] = true;

        //Data labels
        json dict;
        for (unsigned int j=0; j < Model::geometry.size(); j++)
        {
          json list = Model::geometry[j]->getDataLabels(objects[i]);
          std::string key;
          for (auto dataobj : list)
          {
            key = dataobj["label"];
            dict[key] = dataobj;
          }
        }
        obj["data"] = dict;

        //std::cout << "HAS OBJ TYPES: (point,tri,vol)" << obj.getBool("points", false) << "," 
        //          << obj.getBool("triangles", false) << "," << obj.getBool("volume", false) << std::endl;
        outobjects.push_back(obj);
        continue;
      }

      for (int type=lucMinType; type<lucMaxType; type++)
      {
        //Collect vertex/normal/index/value data
        //When extracting data, skip objects with no data returned...
        //if (!Model::geometry[type]) continue;
        Model::geometry[type]->jsonWrite(objects[i], obj);
      }

      //Save object if contains data
      if (obj["points"].size() > 0 ||
          obj["triangles"].size() > 0 ||
          obj["lines"].size() > 0 ||
          obj["volume"].size() > 0)
      {

        //Converts named colours to js readable
        if (obj.count("colour") > 0)
          obj["colour"] = Colour(obj["colour"]).toString();

        outobjects.push_back(obj);
      }
    }
  }

  exported["properties"] = properties;
  exported["views"] = outviews;
  exported["colourmaps"] = cmaps;
  exported["objects"] = outobjects;
  exported["reload"] = true;
  if (fignames.size() > figure)
    exported["figure"] = fignames[figure];

  //Export with indentation
  os << std::setw(2) << exported;
}

void Model::jsonRead(std::string data)
{
  json imported = json::parse(data);
  Properties::globals = imported["properties"];
  json inviews;
  //If "options" exists (old format) read it as first view properties
  if (imported.count("options") > 0)
    inviews.push_back(imported["options"]);
  else
    inviews = imported["views"];

  // Import views
  for (unsigned int v=0; v < inviews.size(); v++)
  {
    if (v >= views.size())
    {
      //Insert a view
      View* view = new View();
      views.push_back(view);
      //Insert all objects for now
      view->objects = objects;
    }

    View* view = views[v];

    //Apply base properties with merge
    view->properties.merge(inviews[v]);

    //TODO: Fix view to use all these properties directly
    json rot, trans, foc, scale, min, max;
    //Skip import cam if not provided
    if (inviews[v].count("rotate") > 0)
    {
      rot = view->properties["rotate"];
      view->setRotation(rot[0], rot[1], rot[2], rot[3]);
    }
    if (inviews[v].count("translate") > 0)
    {
      trans = view->properties["translate"];
      view->setTranslation(trans[0], trans[1], trans[2]);
    }
    if (inviews[v].count("focus") > 0)
    {
      foc = view->properties["focus"];
      view->focus(foc[0], foc[1], foc[2]);
    }
    if (inviews[v].count("scale") > 0)
    {
      scale = view->properties["scale"];
      view->scale[0] = scale[0];
      view->scale[1] = scale[1];
      view->scale[2] = scale[2];
    }
    //min = aview->properties["min"];
    //max = aview->properties["max"];
    //view->init(false, newmin, newmax);
    if (view->properties.has("near") && (float)view->properties["near"] > 0.0)
      view->near_clip = view->properties["near"];
    if (view->properties.has("far") && (float)view->properties["far"] > 0.0)
      view->far_clip = view->properties["far"];
  }

  // Import colourmaps
  json cmaps = imported["colourmaps"];
  for (unsigned int i=0; i < cmaps.size(); i++)
  {
    if (i >= colourMaps.size())
      addColourMap();

    //Replace properties with imported
    json cmap = colourMaps[i]->properties.data = cmaps[i];

    json colours = cmap["colours"];
    if (cmap["name"].is_string()) colourMaps[i]->name = cmap["name"];
    //Replace colours
    colourMaps[i]->colours.clear();
    for (unsigned int c=0; c < colours.size(); c++)
    {
      json colour = colours[c];
      Colour newcolour(colour["colour"]);
      colourMaps[i]->addAt(newcolour, colour["position"]);
    }
  }

  //Import objects
  json inobjects = imported["objects"];
  //Before loading state, set all object visibility to hidden
  //Only objects present in state data will be shown
  //for (unsigned int i=0; i < geometry.size(); i++)
  //  geometry[i]->showObj(NULL, false);

  unsigned int len = objects.size();
  if (len < inobjects.size()) len = inobjects.size();
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (i >= objects.size()) continue; //No adding objects from json now
    /*if (i >= objects.size())
    {
      std::string name = inobjects[i]["name"];
      addObject(new DrawingObject(name));
    }*/

    if (i >= inobjects.size())
    {
      //Not in imported list, assume hidden
      objects[i]->properties.data["visible"] = false;
    }
    
    //Merge properties
    objects[i]->properties.merge(inobjects[i]);
  }

  //if (viewer && viewer->isopen)
  //  viewer->setBackground(Colour(aview->properties["background"])); //Update background colour

  bool reload = (imported["reload"].is_boolean() && imported["reload"]);
  redraw(reload);
}


