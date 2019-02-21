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

Database::Database() : readonly(true), silent(true), attached(NULL), db(NULL), memory(false)
{
  prefix[0] = '\0';
}

Database::Database(const FilePath& fn) : readonly(true), silent(true), attached(NULL), db(NULL), file(fn), memory(false)
{
  prefix[0] = '\0';
}

Database::~Database()
{
  if (db) sqlite3_close(db);
}

bool Database::open(bool write)
{
  //Single file database
  char path[FILE_PATH_MAX];
  int flags = SQLITE_OPEN_READONLY;
  if (write)
  {
    flags = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    readonly = false;
  }

  strcpy(path, file.full.c_str());
  if (strstr(path, "file:")) 
  {
    flags = flags | SQLITE_OPEN_URI;
    memory = true;
  }

  if (strstr(path, "mode=memory"))
    memory = true;

  debug_print("Opening db %s with flags %d\n", path, flags);

  int ret = sqlite3_open_v2(path, &db, flags, NULL);
  if (ret != SQLITE_OK)
  {
    debug_print("Can't open database %s: %s\n", path, sqlite3_errmsg(db));
    return false;
  }

  // Success
  debug_print("Open database %s successful, SQLite version %s\n", path, sqlite3_libversion());
  //rc = sqlite3_busy_handler(db, busy_handler, void*);
  sqlite3_busy_timeout(db, 10000); //10 seconds

  return true;
}

void Database::reopen(bool write)
{
  if (!readonly || !db) return;
  if (db) sqlite3_close(db);
  open(write);

  //Re-attach any attached db file
  if (attached)
  {
    char SQL[SQL_QUERY_MAX];
    sprintf(SQL, "attach database '%s' as t%d", attached->path.c_str(), attached->step);
    if (issue(SQL))
      debug_print("Database %s found and re-attached\n", attached->path.c_str());
  }
}

void Database::attach(TimeStep* timestep)
{
  //Detach any attached db file
  if (memory) return;
  char SQL[SQL_QUERY_MAX];
  if (attached && attached->step != timestep->step)
  {
    sprintf(SQL, "detach database 't%d'", attached->step);
    if (issue(SQL))
    {
      debug_print("Database t%d detached\n", attached->step);
      attached = NULL;
      prefix[0] = '\0';
    }
    else
      debug_print("Database t%d detach failed!\n", attached->step);
  }

  //Attach n'th timestep database if available
  if (timestep->step > 0 && !attached)
  {
    const std::string& path = timestep->path;
    if (path.length() > 0)
    {
      sprintf(SQL, "attach database '%s' as t%d", path.c_str(), timestep->step);
      if (issue(SQL))
      {
        sprintf(prefix, "t%d.", timestep->step);
        debug_print("Database %s found and attached\n", path.c_str());
        attached = timestep;
      }
      else
      {
        debug_print("Database %s found but attach failed!\n", path.c_str());
      }
    }
    //else
    //   debug_print("Database %s not found, loading from current db\n", path.c_str());
  }
}

//SQLite3 utility functions
sqlite3_stmt* Database::select(const char* fmt, ...)
{
  GET_VAR_ARGS(fmt, SQL);

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

bool Database::issue(const char* fmt, ...)
{
  GET_VAR_ARGS(fmt, SQL);
  // Executes a basic SQLite command (ie: without pointer objects and ignoring result sets) and checks for errors
  //debug_print("Issuing: %s\n", SQL);
  char* zErrMsg = NULL;
  if (sqlite3_exec(db, SQL, NULL, 0, &zErrMsg) != SQLITE_OK)
  {
    std::cerr << "SQLite error: " << (zErrMsg ? zErrMsg : "(no error msg)") << std::endl;
    std::cerr << " -- " << SQL << std::endl;
    sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

Model::Model(Session& session) : now(-1), session(session), figure(-1)
{
}

Geometry* Model::getRenderer(lucGeometryType type, std::vector<Geometry*>& renderers)
{
  //Create new geometry containers if required
  if (geometry.size() == 0) init();

  //Return first renderer for primitive type specified if found
  for (auto g : renderers)
  {
    if (g->type == type)
    {
      return g;
      break;
    }
  }
  std::cout << "RENDERER NOT FOUND: " << GeomData::names[type] << std::endl;
  return NULL;
}

Geometry* Model::getRenderer(lucGeometryType type)
{
  return getRenderer(type, geometry);
}

template <typename T>
Geometry* Model::getRendererOfType()
{
  //Create new geometry containers if required
  if (geometry.size() == 0) init();

  //Return first renderer of type specified (or derived) if found
  for (auto g : geometry)
  {
    //dynamic_cast only works if a valid descendant class
    T* obj = dynamic_cast<T*>(g);
    if (obj)
    {
      return g;
      break;
    }
  }

  //Create and add one then
  Geometry* g = new T(session);
  geometry.push_back(g);
  return g;
}

Geometry* Model::getRenderer(const std::string& what)
{
  //Define the renderers to use for all labeled geometry types

  //Specific types: require exact renderer if available, create one if not
  if (what == "links")
    return getRendererOfType<Links>();
  if (what == "spheres")
    return getRendererOfType<Spheres>();
  if (what == "mesh")
    return getRendererOfType<Mesh>();

  //Default/base types: use the default renderer for the type name
  json renderers = session.global("renderers");
  for (int i=0; i<renderers.size(); i++)
    for (std::string s : renderers[i])
      if (what == s)
        return getRenderer((lucGeometryType)i);

  //Must be a custom type, find by label if exists
  for (auto g : geometry)
  {
    if (g->custom == what)
    {
      return g;
      break;
    }
  }

  return NULL;
}

void Model::load(const FilePath& fn)
{
  //Open database file
  database = Database(fn);
  if (database.open())
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


View* Model::defaultView(Properties* properties)
{
  //No views?
  if (views.size() == 0)
  {
    //Default view
    View* view = new View(session);
    if (properties)
      view->properties.merge(properties->data);
    views.push_back(view);
  }

  //No objects?
  if (views[0]->objects.size() == 0)
  {
    //Add all objects to first viewport
    for (unsigned int o=0; o<objects.size(); o++)
    {
      views[0]->addObject(objects[o]);
      loadLinks(objects[o]);
    }
  }

  //Return first
  return views[0];
}

void Model::init()
{
  //All renderers are switchable and user defined based on "renderlist" global property
  geometry.clear();
  std::string renderlist = session.global("renderlist");
  std::istringstream iss(renderlist);
  std::string s;
  while (getline(iss, s, ' '))
    geometry.push_back(createRenderer(session, s));

  debug_print("Created %d new geometry containers: %s\n", (int)geometry.size(), renderlist.c_str());

  for (auto g : geometry)
  {
    bool hideall = session.global("hideall");
    if (hideall)
      g->hideShowAll(true);
  }
}

Model::~Model()
{
  for (auto g : geometry)
    delete g;
  geometry.clear();

  clearTimeSteps();

  //Clear drawing objects
  for(unsigned int i=0; i<objects.size(); i++)
    delete objects[i];

  //Clear views
  for(unsigned int i=0; i<views.size(); i++)
    delete views[i];

  //Clear colourmaps
  for(unsigned int i=0; i<colourMaps.size(); i++)
    delete colourMaps[i];
}

bool Model::loadFigure(int fig, bool preserveGlobals)
{
  if (fig < 0 || figures.size() == 0) return false;
  if (fig >= (int)figures.size()) fig = 0;
  if (fig < 0) fig = figures.size()-1;
  figure = fig;
  assert(figure >= 0);
  json globals = session.globals;

  //Hide all existing objects before loading if there are multiple figures
  //Only objects present in new figure's state data will be shown
  if (figures.size() > 1)
  {
    for (unsigned int i=0; i < objects.size(); i++)
      objects[i]->properties.data["visible"] = false;
  }

  jsonRead(figures[figure]);

  //Set window caption
  if (fignames[figure].length() > 0)
    session.globals["caption"] = fignames[figure];
  else if (!database.memory) 
    session.globals["caption"] = database.file.base;

  //Preserves existing global settings by merging after load
  if (preserveGlobals)
    Properties::mergeJSON(session.globals, globals);

  return true;
}

void Model::storeFigure()
{
  //Add default figure
  if (figure < 0)
  {
    figure = 0;
    if (figures.size() == 0)
      figure = addFigure("default");
  }
  assert(figure >= 0);
  assert(figure < (int)figures.size());

  //Save current state in current selected figure
  figures[figure] = jsonWrite();
}

int Model::addFigure(std::string name, const std::string& state)
{
  if (name.length() == 0) name = "(unnamed)";
  //Check for duplicate names
  bool found = false;
  int count = 0;
  std::string newname = name;
  do
  {
    found = false;
    for (auto n : fignames)
    {
      //Duplicate name, add number until not a duplicate
      if (n == newname)
      {
        found = true;
        std::stringstream ns;
        ns << name << ++count;
        newname = ns.str();
        break;
      }
    }
  } while (found);

  name = newname;

  fignames.push_back(name);
  if (state.length())
    figures.push_back(state);
  else
    figures.push_back(jsonWrite());

  return figures.size()-1;
}

void Model::addObject(DrawingObject* obj)
{
  //Check for duplicate names / objects
  bool found = false;
  int count = 0;
  std::string name = obj->name();
  do
  {
    found = false;
    for (auto o : objects)
    {
      //Duplicate object, skip
      if (o == obj) return;
      //Duplicate name, add number until not a duplicate
      if (o->name() == name)
      {
        found = true;
        std::stringstream ns;
        ns << obj->name() << ++count;
        name = ns.str();
        break;
      }
    }
  } while (found);

  obj->properties.data["name"] = name;

  //Create master drawing object list entry
  objects.push_back(obj);
}

//Adds colourmap
ColourMap* Model::addColourMap(std::string name, std::string colours, std::string properties)
{
  if (name.length() == 0)
    name = "default";

  //Create a default greyscale map if no data provided
  if (!colours.length()) colours = "#000000 #ffffff";

  //Check for existing and update instead if found
  for (auto cm : colourMaps)
  {
    if (cm->name == name)
    {
      updateColourMap(cm, colours, properties);
      return cm;
    }
  }

  //Add a new colourmap
  ColourMap* cmap = new ColourMap(session, name, properties);
  cmap->loadPalette(colours);
  colourMaps.push_back(cmap);
  return cmap;
}

//Loads colourmap
void Model::updateColourMap(ColourMap* colourMap, std::string colours, std::string properties)
{
  if (!colourMap) return;

  //Parse and merge property strings
  session.parseSet(colourMap->properties, properties);
  colourMap->loadPalette(colours);

  //Update this and other objects using this map
  for (auto o : objects)
  {
    o->setup();
    if (o->colourMap == colourMap)
      reload(o);
  }
}

DrawingObject* Model::findObject(unsigned int id)
{
  for (unsigned int i=0; i<objects.size(); i++)
    if (objects[i]->dbid == id) return objects[i];
  return NULL;
}

DrawingObject* Model::findObject(const std::string& name, DrawingObject* def)
{
  //Lookup by name only
  if (name.length() == 0) return NULL;
  std::string lname = name;
  std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
  for (unsigned int i=0; i<objects.size(); i++)
  {
    std::string namekey = objects[i]->name();
    std::transform(namekey.begin(), namekey.end(), namekey.begin(), ::tolower);
    if (namekey == lname)
      return objects[i];
  }
  return def;
}

ColourMap* Model::findColourMap(const std::string& name, ColourMap* def)
{
  //Lookup by name only
  if (name.length() == 0) return NULL;
  std::string lname = name;
  std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
  for (unsigned int i=0; i<colourMaps.size(); i++)
  {
    std::string namekey = colourMaps[i]->name;
    std::transform(namekey.begin(), namekey.end(), namekey.begin(), ::tolower);
    if (namekey == lname)
      return colourMaps[i];
  }
  return def;
}

void Model::clearObjects(bool fixed)
{
  if (membytes__ > 0 && geometry.size() > 0)
    debug_print("Clearing geometry, geom memory usage before clear %.3f mb\n", membytes__/1000000.0f);

  //Clear containers...
  for (auto g : geometry)
    g->clear(fixed);
}

void Model::setup()
{
  //Adjust the steprange default, should be off if all steps loaded
  if (!database || session.global("cache"))
  {
    session.properties["steprange"]["default"] = false;
    session.defaults["steprange"] = false;
  }

  //Setup min/max on all object data
  for (unsigned int i=0; i < objects.size(); i++)
  {
    //Clear existing data, only needed if using per step ranges
    if (objects[i]->properties["steprange"])
      objects[i]->ranges.clear();

    for (auto g : geometry)
      g->scanDataRange(objects[i]);
  }
}

void Model::reloadRedraw(DrawingObject* obj, bool reload)
{
  //Reload or redraw by object or all
  if (obj)
  {
    //Reload or redraw selected object only
    for (auto g : geometry)
      g->redrawObject(obj, reload);

    if (obj->colourMap)
      obj->colourMap->calibrated = false;
    if (obj->opacityMap)
      obj->opacityMap->calibrated = false;

    obj->setup();
  }
  else
  {
    //Flag reload on all objects...
    for (auto g : geometry)
    {
      if (reload)
        //Full data reload...
        g->reload = true;
      else
        //Just flag a redraw, will only be reloaded if vertex count changed
        g->redraw = true;
    }

    for (unsigned int i = 0; i < colourMaps.size(); i++)
      colourMaps[i]->calibrated = false;

    for (auto o : objects)
      o->setup();
  }
}

void Model::redraw(DrawingObject* obj)
{
  //Flag redraw
  reloadRedraw(obj, false);
}

void Model::reload(DrawingObject* obj)
{
  //Flag full reload
  reloadRedraw(obj, true);
}

void Model::bake(DrawingObject* obj)
{
  //Convert all Glyphs type elements into their primitives (triangles/lines/points)
  //Will no longer be able to be scaled etc but can be cached and loaded much faster
  Lines* lines = (Lines*)getRenderer(lucLineType);
  Triangles* tris = (Triangles*)getRenderer(lucTriangleType);
  Points* points = (Points*)getRenderer(lucPointType);
  int savestep = step();
  if (timesteps.size() == 0)
    addTimeStep(0, 0);
  for (unsigned int idx=0; idx < timesteps.size(); idx++)
  {
    //Change the step
    setTimeStep(idx);
    std::cout << now << " " << std::flush;

    //Re-calc data ranges
    setup();

    //TODO: also bake in colour info to RGBA?

    for (auto g : geometry)
    {
      //dynamic_cast only works if a valid descendant class
      Glyphs* glyphs = dynamic_cast<Glyphs*>(g);
      if (glyphs)
      {
        //Redraw/merge
        glyphs->display();

        //Copy entries
        //printf("L %d T %d P %d\n", glyphs->lines->records.size(), glyphs->tris->records.size(), glyphs->points->records.size());
        for (auto gp : glyphs->lines->records)
          lines->records.push_back(gp);
        for (auto gp : glyphs->tris->records)
          tris->records.push_back(gp);
        for (auto gp : glyphs->points->records)
          points->records.push_back(gp);
      }
    }
  }

  std::cout << std::endl;

  //Restore step
  setTimeStep(savestep);

  //Clear old glyph data
  for (auto g : geometry)
  {
    Glyphs* glyphs = dynamic_cast<Glyphs*>(g);
    if (glyphs)
      glyphs->clear(true);
  }

}

void Model::loadWindows()
{
  //Load state from database if available
  sqlite3_stmt* statement = database.select("SELECT name, data from state ORDER BY id");
  while (sqlite3_step(statement) == SQLITE_ROW)
  {
    const char *name = (const char*)sqlite3_column_text(statement, 0);
    const char *data = (const char*)sqlite3_column_text(statement, 1);
    addFigure(name, data);
  }

  if (figures.size() > 0)
  {
    //Load the most recently added (last entry)
    //(so the previous state saved is restored on load)
    //Preserve the globals so command line arg settings are retained
    loadFigure(figures.size()-1, true);

    //Load object links (colourmaps)
    for (auto o: objects)
      loadLinks(o);
  }
  else //Old db uses window structure
  {
    //Load windows list from database and insert into models
    statement = database.select("SELECT id,name,width,height,colour,minX,minY,minZ,maxX,maxY,maxZ from window");
    //sqlite3_stmt* statement = model->select("SELECT id,name,width,height,colour,minX,minY,minZ,maxX,maxY,maxZ,properties from window");
    //window (id, name, width, height, colour, minX, minY, minZ, maxX, maxY, maxZ, properties)
    //Single window only supported now, use viewports for multiple
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
      std::string wtitle = std::string((char*)sqlite3_column_text(statement, 1));
      int width = sqlite3_column_int(statement, 2);
      int height = sqlite3_column_int(statement, 3);
      Colour colour;
      colour.value = sqlite3_column_int(statement, 4);
      float min[3], max[3];
      for (int i=0; i<3; i++)
      {
        if (sqlite3_column_type(statement, 5+i) != SQLITE_NULL)
          min[i] = (float)sqlite3_column_double(statement, 5+i);
        else
          min[i] = FLT_MAX;
        if (sqlite3_column_type(statement, 8+i) != SQLITE_NULL)
          max[i] = (float)sqlite3_column_double(statement, 8+i);
        else
          max[i] = -FLT_MAX;
      }

      session.globals["caption"] = wtitle;
      session.globals["resolution"] = {width, height};
      //Support legacy colour field
      if (colour.value != 0 && !session.globals.count("colour"))
        session.globals["background"] = colour.toJson();

      //Link the window viewports, objects & colourmaps
      loadLinks(min, max);
    }
  }
  sqlite3_finalize(statement);
}

//Load model viewports
void Model::loadViewports()
{
  //Clear existing views
  for(unsigned int i=0; i<views.size(); i++)
    delete views[i];
  views.clear();

  sqlite3_stmt* statement;
  statement = database.select("SELECT id,x,y,near,far FROM viewport ORDER BY y,x");

  //viewport:
  //(id, title, x, y, near, far, translateX,Y,Z, rotateX,Y,Z, scaleX,Y,Z, properties
  while (sqlite3_step(statement) == SQLITE_ROW)
  {
    float x = (float)sqlite3_column_double(statement, 1);
    float y = (float)sqlite3_column_double(statement, 2);
    float nearc = (float)sqlite3_column_double(statement, 3);
    float farc = (float)sqlite3_column_double(statement, 4);

    //Create the view object and add to list
    views.push_back(new View(session, x, y, nearc, farc));
  }
  sqlite3_finalize(statement);

  //Calculate viewport scaling ( x,y width,height )
  for (unsigned int v=0; v<views.size(); v++)
  {
    float nextx = 1.0, nexty = 1.0;
   
    //Find next largest x & y positions
    if (v+1 < views.size() && views[v+1]->x > views[v]->x)
       nextx = views[v+1]->x;

    for (unsigned int vv=v+1; vv<views.size(); vv++)
    {
       if (views[vv]->y > views[v]->y && views[vv]->y < nexty)
          nexty = views[vv]->y;
    }

    views[v]->w = nextx - views[v]->x;
    views[v]->h = nexty - views[v]->y;
    debug_print("-- Viewport %d at %f,%f %f x %f\n", v, views[v]->x, views[v]->y, views[v]->w, views[v]->h);
  }
}

//Load and apply viewport camera settings
void Model::loadViewCamera(int viewport_id)
{
  int adj=0;
  //Load specified viewport and apply camera settings
  sqlite3_stmt* statement = database.select("SELECT aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", viewport_id);
  if (statement == NULL)
  {
    //Old db
    adj = -5;
    statement = database.select("SELECT translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ FROM viewport WHERE id=%d;", viewport_id);
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
    session.parseSet(v->properties, std::string(vprops));
    v->properties["coordsystem"] = orientation;
  }
  sqlite3_finalize(statement);
}

//Load model objects
void Model::loadObjects()
{
  sqlite3_stmt* statement;
  statement = database.select("SELECT id, name, colour, opacity, properties FROM object", true);
  if (statement == NULL)
    statement = database.select("SELECT id, name, colour, opacity FROM object");

  //object (id, name, colourmap_id, colour, opacity, properties)
  while (sqlite3_step(statement) == SQLITE_ROW)
  {
    int object_id = sqlite3_column_int(statement, 0);
    const char *otitle = (char*)sqlite3_column_text(statement, 1);

    //Create drawing object and add to master list
    std::string props = "";
    if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
      props = std::string((char*)sqlite3_column_text(statement, 4));
    DrawingObject* obj = new DrawingObject(session, otitle, props, object_id);

    //Convert old colour/opacity from hard coded fields if provided
    if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
    {
      Colour cobj;
      cobj.value = sqlite3_column_int(statement, 2);
      if (cobj.value != 0 && !obj->properties.has("colour")) obj->properties.data["colour"] = cobj.toJson();
    }
    if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
    {
      float opacity = (float)sqlite3_column_double(statement, 3);
      if (opacity > 0 && !obj->properties.has("opacity")) obj->properties.data["opacity"] = opacity;
    }

    addObject(obj);
  }
  sqlite3_finalize(statement);
}

//Load viewports in current window, objects in each viewport, colourmaps for each object
void Model::loadLinks(float* min, float* max)
{
  //Select statment to get all viewports in window and all objects in viewports
  //sprintf(SQL, "SELECT id,title,x,y,near,far,aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", win->id);
  sqlite3_stmt* statement = database.select("SELECT viewport.id,object.id,object.colourmap_id,object_colourmap.colourmap_id,object_colourmap.data_type FROM window_viewport,viewport,viewport_object,object LEFT OUTER JOIN object_colourmap ON object_colourmap.object_id=object.id WHERE viewport_object.viewport_id=viewport.id AND object.id=viewport_object.object_id"); //Don't report errors as these tables are allowed to not exist

  int last_viewport = 0, last_object = 0;
  DrawingObject* draw = NULL;
  View* view = NULL;
  while ( sqlite3_step(statement) == SQLITE_ROW)
  {
    int viewport_id = sqlite3_column_int(statement, 0);
    int object_id = sqlite3_column_int(statement, 1);
    unsigned int colourmap_id = sqlite3_column_int(statement, 3); //Linked colourmap id
    int data_type = sqlite3_column_int(statement, 4); //Colour/Opacity/R/G/B etc (legacy)

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
      //Set the view bounds, passed from window
      view->properties.data["min"] = {min[0], min[1], min[2]};
      view->properties.data["max"] = {max[0], max[1], max[2]};
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
      //Add colourmap to drawing object Colour & Opacity still suported, R/G/B are not
      if (data_type == lucColourValueData)
        draw->properties.data["colourmap"] = colourMaps[colourmap_id-1]->name;
      if (data_type == lucOpacityValueData)
        draw->properties.data["opacitymap"] = colourMaps[colourmap_id-1]->name;
    }
  }
  sqlite3_finalize(statement);
}

//Load colourmaps for each object only
void Model::loadLinks(DrawingObject* obj)
{
  //Only for objects from db files
  if (obj->dbid <= 0) return;

  //Select statment to get all viewports in window and all objects in viewports
  //sprintf(SQL, "SELECT id,title,x,y,near,far,aperture,orientation,focalPointX,focalPointY,focalPointZ,translateX,translateY,translateZ,rotateX,rotateY,rotateZ,scaleX,scaleY,scaleZ,properties FROM viewport WHERE id=%d;", win->id);
  sqlite3_stmt* statement = database.select("SELECT object.id,object.colourmap_id,object_colourmap.colourmap_id,object_colourmap.data_type FROM object LEFT OUTER JOIN object_colourmap ON object_colourmap.object_id=object.id WHERE object.id=%d", obj->dbid);

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
      obj->properties.data["colourmap"] = colourMaps[colourmap_id-1]->name;
    }
  }
  sqlite3_finalize(statement);
}

void Model::clearTimeSteps()
{
  for (unsigned int idx=0; idx < timesteps.size(); idx++)
    delete timesteps[idx];
  timesteps.clear();
}

int Model::loadTimeSteps(bool scan)
{
  //Strip any digits from end of filename to get base
  std::string base = database.file.base;
  std::string basename = base.substr(0, base.find_last_not_of("0123456789") + 1);

  //Don't reload timesteps when all data has been cached on initial load
  if (!scan && session.global("cache") && timesteps.size() > 0) return timesteps.size();
  clearTimeSteps();
  int rows = 0;

  //Scan for additional timestep files with corresponding entries in timestep table
  if (!scan && database && !database.memory)
  {
    sqlite3_stmt* statement = database.select("SELECT * from timestep");
    //(id, time, dim_factor, units)
    while (sqlite3_step(statement) == SQLITE_ROW)
    {
      int step = sqlite3_column_int(statement, 0);
      double time = sqlite3_column_double(statement, 1);
      sqlite3_stmt* statement2 = database.select("SELECT count(id) from geometry where timestep = %d", step);
      int geomcount = sqlite3_column_int(statement2, 0);
      sqlite3_finalize(statement2);
      //Get timestep props
      std::string props = "";
      if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
        props = std::string((char*)sqlite3_column_text(statement, 4));
      addTimeStep(step, time, props);

      //No geometry in current db? Check for attachment db
      if (geomcount == 0)
      {
        //Look for additional db file (minimum 3 digit padded step in names)
        std::string path = checkFileStep(step, basename, 3);
        if (path.length() > 0 && path != database.file.full)
        {
          debug_print("Found step %d database %s\n", step, path.c_str());
          timesteps[rows]->path = path;
        }
      }

      rows++;
    }
    sqlite3_finalize(statement);
  }

  //Assume we have at least one current timestep, even if none in table
  if (timesteps.size() == 0) addTimeStep();

  //Check for other timesteps in external files without table data
  //(only run this if explicitly requested with "scan command")
  if (scan) // || timesteps.size() == 1)
  {
    debug_print("Scanning for timesteps...\n");
    for (unsigned int ts=0; ts<10000; ts++)
    {
      //If no steps found after trying 100, give up scanning
      if (timesteps.size() < 2 && ts > 100) break;
      std::string path = checkFileStep(ts, basename);
      if (path.length() > 0)
      {
        debug_print("Found step %d database %s\n", ts, path.c_str());
        if (path == database.file.full && timesteps.size() == 1)
        {
          //Update step if this is the initial db file
          timesteps[0]->step = ts;
          timesteps[0]->path = path;
        }
        else
          addTimeStep(ts, 0.0, "", path);
      }
    }
    debug_print("Scanning complete, found %d steps.\n", timesteps.size());
  }

  //Copy to static for use in Tracers etc
  if (infostream) std::cerr << timesteps.size() << " timesteps loaded\n";
  session.timesteps = timesteps;
  return timesteps.size();
}

std::string Model::checkFileStep(unsigned int ts, const std::string& basename, unsigned int limit)
{
  unsigned int len = (ts == 0 ? 1 : (int)log10((float)ts) + 1);
  //Lower limit of digits to look for, default 1-5
  if (len < limit) len = limit;
  for (int w = 5; w >= (int)len; w--)
  {
    std::ostringstream ss;
    ss << database.file.path << basename << std::setw(w) << std::setfill('0') << ts;
    ss << "." << database.file.ext;
    std::string path = ss.str();
    if (FileExists(path))
      return path;
  }
  return "";
}

void Model::loadColourMaps()
{
  sqlite3_stmt* statement = database.select("select count(*) from colourvalue");
  if (statement) return loadColourMapsLegacy();

  //New databases have only a colourmap table with colour data in properties
  statement = database.select("SELECT id,name,minimum,maximum,logscale,discrete,properties FROM colourmap");
  double minimum;
  double maximum;
  ColourMap* colourMap = NULL;
  while ( sqlite3_step(statement) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(statement, 0);
    char *cmname = (char*)sqlite3_column_text(statement, 1);
    minimum = sqlite3_column_double(statement, 2);
    maximum = sqlite3_column_double(statement, 3);
    int logscale = sqlite3_column_int(statement, 4);
    int discrete = sqlite3_column_int(statement, 5);
    std::string props;
    if (sqlite3_column_type(statement, 6) != SQLITE_NULL) props = (char*)sqlite3_column_text(statement, 6);
    std::stringstream name;
    name << cmname << "_" << id; //Prevent duplicate names by appending id
    colourMap = new ColourMap(session, name.str(), props);
    setColourMapProps(colourMap->properties, minimum, maximum, logscale, discrete);
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
  sqlite3_stmt* statement = database.select("SELECT colourmap.id,minimum,maximum,logscale,discrete,colour,value,name,properties FROM colourmap,colourvalue WHERE colourvalue.colourmap_id=colourmap.id");
  if (statement == NULL)
  {
    //Old DB format, had no name or props
    statement = database.select("SELECT colourmap.id,minimum,maximum,logscale,discrete,colour,value FROM colourmap,colourvalue WHERE colourvalue.colourmap_id=colourmap.id");
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
      std::string props;
      if (!old && sqlite3_column_type(statement, 8) != SQLITE_NULL) props = (char*)sqlite3_column_text(statement, 8);

      std::stringstream name;
      if (cmname) name << cmname << "_";
      name << id; //Prevent duplicate names by appending id
      colourMap = new ColourMap(session, name.str(), props);
      colourMaps.push_back(colourMap);
      setColourMapProps(colourMap->properties, minimum, maximum, logscale, discrete);
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

void Model::setColourMapProps(Properties& properties, float minimum, float maximum, bool logscale, bool discrete)
{
  //These properties used to be database fields, convert
  if (logscale) properties.data["logscale"] = true;
  if (discrete) properties.data["discrete"] = true;

  //Set range if dynamic=0 or minimum/maximum values are not defaults
  if (properties.has("dynamic"))
  {
    //Legacy prop, supports int and bool
    json d = properties["dynamic"];
    bool dynamic = d.is_boolean() ? (bool)d : (int)d != 0;
    if (!dynamic)
      properties.data["range"] = {minimum, maximum};
  }

  float range = maximum - minimum;
  if (!properties.has("range") && range != 0.0 && range != 1.0)
  {
    properties.data["range"] = {minimum, maximum};
  }
}

void Model::cacheLoad()
{
  std::cout << "Loading " << timesteps.size() << " steps\n";
  for (unsigned int i=0; i<timesteps.size(); i++)
  {
    setTimeStep(i);
    if (i > 0 && i%10==0) std::cout << std::setw(4) << i << " " << std::flush;
    if (i > 0 && i%50==0) std::cout << std::endl;
    if (session.now != (int)i) break; //All cached in loadGeometry (doesn't work for split db timesteps so still need this loop)
    debug_print("Cached time %d : %d/%d (%s)\n", step(), i+1, timesteps.size(), database.file.base.c_str());
  }
  //Cache final step
  setTimeStep(0);
  std::cout << std::endl;
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
  //if (timesteps.size() == 1 && session.now >= 0 && ) return -1;  //Single timestep

  for (idx=0; idx < (int)timesteps.size(); idx++)
    if (timesteps[idx]->step >= requested) break;

  //Reached end of list?
  if (idx == (int)timesteps.size()) idx--;

  if (idx < 0) idx = 0;
  if (idx >= (int)timesteps.size()) idx = timesteps.size() - 1;

  return idx;
}

void Model::addTimeStep(int step, double time, const std::string& props, const std::string& path)
{
  if (time == -HUGE_VAL) time = step;
  timesteps.push_back(new TimeStep(session.globals, session.defaults, step, time, path));
  int tlen = timesteps.size();
  //Parse temporal properties
  session.parseSet(timesteps[tlen-1]->properties, props);
  //Update gap
  if (tlen > 1)
  {
    int diff = timesteps[tlen-1]->step - timesteps[tlen-2]->step;
    if (diff > session.gap)
      session.gap = diff;
  }
}

//Load data at specified timestep
int Model::setTimeStep(int stepidx)
{
  int rows = -1;
  clock_t t1 = clock();

  //Default timestep only? Skip load
  if (timesteps.size() == 0)
  {
    session.globals["timestep"] = session.now = now = -1;
  }
  else
  {
    rows = 0;

    if (stepidx < 0) stepidx = 0; //return -1;
    if (stepidx >= (int)timesteps.size())
      stepidx = timesteps.size()-1;

    //Unchanged...
    //if (now >= 0 && stepidx == now && session.now == now)
    //  return -1;
    if (now < 0 || stepidx != now || session.now != now)
    {
      //Create new geometry containers if required
      if (geometry.size() == 0)
      {
        init();
      }
      else
      {
        //Clear and tell all geometry objects they need to reload data
        for (auto g : geometry)
        {
          //Wait until all sort threads done
          std::lock_guard<std::mutex> guard(g->sortmutex);
          //Release any graphics memory and clear
          g->close();
        }
      }

      //Setting initial step?
      if (now < 0)
        loadFixedGeometry();

      //Set the new timestep index
      debug_print("===== Model step %d Global step %d Requested step %d =====\n", now, session.now, stepidx);
      session.timesteps = timesteps; //Set to current model timestep vector
      session.now = now = stepidx;
      session.globals["timestep"] = step(); //Save property for read access
      debug_print("TimeStep set to: %d (%d)\n", step(), stepidx);

      reload();
      
      bool clear = session.global("clearstep");
      if (clear)
        //Not caching timesteps from database, clear current step data
        clearObjects();

      //Load the new step data if it isn't already in memory
      if (clear || !timesteps[now]->loaded)
      {
        //Flag loaded
        timesteps[now]->loaded = true;

        //Load new data
        if (database)
        {
          //Detach any attached db file and attach n'th timestep database if available
          database.attach(timesteps[session.now]);

          if (session.global("cache"))
          {
            //Attempt caching all geometry from database at start
            rows += loadGeometry(0, 0, timesteps[timesteps.size()-1]->step);
            std::cout << '.' << std::flush;
          }
          else
            rows += loadGeometry();

          debug_print("%.4lf seconds to load %d geometry records from database\n", (clock()-t1)/(double)CLOCKS_PER_SEC, rows);
        }
      }
      else
        debug_print("Step already cached\n");
    }
  }

  //Load temporal properties
  if (session.now >= 0)
    Properties::mergeJSON(session.globals, session.timesteps[session.now]->properties.data);

  return rows;
}

int Model::loadGeometry(int obj_id, int time_start, int time_stop)
{
  if (!database)
  {
    std::cerr << "No database loaded!!\n";
    return 0;
  }

  //Default to current timestep
  if (time_start < 0) time_start = step();
  if (time_stop < 0) time_stop = step();

  //Load geometry
  char filter[256] = {'\0'};
  char objfilter[64] = {'\0'};

  //Setup filters, object...
  //(Skip tracers, they are loaded with timesteps combined as fixed data)
  if (obj_id > 0)
  {
    sprintf(objfilter, "WHERE type != %d AND object_id=%d", lucTracerType, obj_id);
    //Remove the skip flag now we have explicitly loaded object
    DrawingObject* obj = findObject(obj_id);

    if (obj) obj->skip = false;
  }
  else
    sprintf(objfilter, "WHERE type != %d", lucTracerType);

  //...timestep...(if ts db attached, just load all data from attached db assuming geometry is at current step)
  if (time_start >= 0 && time_stop >= 0 && !database.attached)
    sprintf(filter, "%s AND timestep BETWEEN %d AND %d", objfilter, time_start, time_stop);
  else
    strcpy(filter, objfilter);

  //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
  //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, labels,
  //minX, minY, minZ, maxX, maxY, maxZ, data)
  sqlite3_stmt* statement = database.select("SELECT id,object_id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,minX,minY,minZ,maxX,maxY,maxZ,data FROM %sgeometry %s ORDER BY timestep,object_id", database.prefix, filter);

  //Old database compatibility
  if (statement == NULL)
  {
    //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
    //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, data)
    statement = database.select("SELECT id,object_id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,NULL,NULL,NULL,NULL,NULL,NULL,data FROM %sgeometry %s ORDER BY timestep,object_id", database.prefix, filter);
    printf("Using legacy GLDB format\n");
  }

  if (!statement) return 0;

  //Iterate and process the geometry
  return readGeometryRecords(statement);
}

int Model::loadFixedGeometry()
{
  if (!database)
    return 0;

  //Load geometry (fixed time records only)
  //object (id, name, colourmap_id, colour, opacity, wireframe, cullface, scaling, lineWidth, arrowHead, flat, steps, time)
  //geometry (id, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, labels,
  //minX, minY, minZ, maxX, maxY, maxZ, data)
  sqlite3_stmt* statement = database.select("SELECT id,object_id,timestep,rank,idx,type,data_type,size,count,width,minimum,maximum,dim_factor,units,labels,minX,minY,minZ,maxX,maxY,maxZ,data FROM geometry WHERE (timestep=-1 OR type=%d) ORDER BY timestep,object_id", lucTracerType);

  if (!statement) return 0;

  //Iterate and process the geometry
  return readGeometryRecords(statement, false);
}

int Model::readGeometryRecords(sqlite3_stmt* statement, bool cache)
{
  clock_t t1 = clock();
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
      //Clear if default
      if (maximum - minimum == 1.0) maximum = minimum = 0.0;
      //Units field repurposed for data label
      const char *data_label = (const char*)sqlite3_column_text(statement, 13);
      const char *labels = (const char*)sqlite3_column_text(statement, 14);

      //printf("%d] OBJ %d STEP %d TYPE %d DTYPE %d DIMS (%d x %d x %d) COUNT %d ITEMS %d LABELS %s\n", 
      //       rows, object_id, timestep, type, data_type, width, height, depth, count, items, labels);

      const void *data = sqlite3_column_blob(statement, 21);
      unsigned int bytes = sqlite3_column_bytes(statement, 21);

      DrawingObject* obj = findObject(object_id);

      //Deleted or Skip object? (When noload enabled)
      if (!obj || obj->skip) continue;

      //Bulk load: switch timestep and cache if timestep changes!
      // - disabled when using attached databases (cached in loop via cacheLoad())
      if (cache && step() != timestep && !database.attached)
      {
        std::cout << '~' << std::flush;
        if (timestep > 0 && timestep%10==0) std::cout << std::setw(4) << timestep << " " << std::flush;
        if (timestep > 0 && timestep%50==0) std::cout << std::endl;
        //Change active timestep
        session.now = now = nearestTimeStep(timestep);
        //Flag all data loaded at this step
        timesteps[now]->loaded = true;
      }
      // Similar required when loading tracers in loadFixedData
      if (type == lucTracerType && step() != timestep)
      {
        //Change active timestep
        session.now = now = nearestTimeStep(timestep);
      }

      if (type == lucTracerType)
      {
        height = 0;
        //Default particle count:
        if (width == 0) width = items;
      }

      //Create object and set parameters
      /* Convert grid to tris
       * - need to skip index/normal data as it is setup for tri strips
      if (type == lucGridType) {
        type = lucTriangleType;
        if (data_type == lucIndexData || data_type == lucNormalData) continue;
      }*/
      active = getRenderer(type);
      if (!active) continue; //Can't render this data

      unsigned char* buffer = NULL;
      if (bytes != (unsigned int)(count * GeomData::byteSize(data_type)))
      {
        //Decompress!
        unsigned long dst_len = (unsigned long)(count * GeomData::byteSize(data_type));
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

      //Always add a new element for each new vertex geometry record
      //not suitable if writing db on multiple procs!
      if (data_type == lucVertexData) active->add(obj);

      //Read data block
      Geom_Ptr g;
      //Convert legacy value types to use data labels
      switch (data_type)
      {
        case lucColourValueData:
        case lucOpacityValueData:
        case lucRedValueData:
        case lucGreenValueData:
        case lucBlueValueData:
        case lucXWidthData:
        case lucYHeightData:
        case lucZLengthData:
        case lucSizeData:
        case lucMaxDataType:
        {
          json by;
          if (data_label && strlen(data_label) > 0)
          {
            //Use provided label from units field
            g = active->read(obj, items, data, data_label);
            by = data_label;
          }
          else //Use default/legacy label
          {
            g = active->read(obj, items, data, GeomData::datalabels[data_type]);
            by = GeomData::datalabels[data_type];
          }

          //Set as the opacity/size data if in these categories
          if (data_type == lucOpacityValueData)
            obj->properties.data["opacityby"] = by;
          if (data_type == lucSizeData)
            obj->properties.data["sizeby"] = by;

          //copy max/min fields
          unsigned int valueIdx = g->valuesLookup(by);
          if (valueIdx < g->values.size())
          {
            g->values[valueIdx]->minimum = minimum;
            g->values[valueIdx]->maximum = maximum;
          }
          break;
        }
        default:
          //Non-value data
          g = active->read(obj, items, data_type, data, width, height, depth);

          //copy max/min fields
          Data_Ptr container = g->dataContainer(data_type);
          container->minimum = minimum;
          container->maximum = maximum;
      }

      //Set geom labels if any
      if (labels) active->label(obj, labels);

      //Where min/max vertex provided, load
      if (data_type == lucVertexData && type != lucLabelType)
      {
        float min[3] = {0,0,0}, max[3] = {0,0,0};
        if (sqlite3_column_type(statement, 15) != SQLITE_NULL)
        {
          for (int i=0; i<3; i++)
          {
            min[i] = (float)sqlite3_column_double(statement, 15+i);
            max[i] = (float)sqlite3_column_double(statement, 18+i);
          }
        }

        //Apply dims if provided
        if (min[0] != max[0] || min[1] != max[1] || min[2] != max[2])
        {
          g->checkPointMinMax(min);
          g->checkPointMinMax(max);
        }
      }

      if (buffer) delete[] buffer;
    }
    else if (ret != SQLITE_DONE)
      fprintf(stderr, "Database file problem, sqlite_step returned: %d (%d)\n", ret, (ret>>8));
  }
  while (ret == SQLITE_ROW);

  sqlite3_finalize(statement);
  debug_print("... loaded %d rows, %d bytes, %.4lf seconds\n", rows, tbytes, (clock()-t1)/(double)CLOCKS_PER_SEC);

  return rows;
}

void Model::mergeDatabases()
{
  if (!database) return;
  database.reopen(true);  //Open writable
  for (unsigned int i=0; i<=timesteps.size(); i++)
  {
    debug_print("MERGE %d/%d...%d\n", i, timesteps.size(), step());
    setTimeStep(i);
    if (database.attached->step == step())
    {
      database.issue("insert into geometry select null, object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, labels, properties, data, minX, minY, minZ, maxX, maxY, maxZ from %sgeometry", database.prefix);
    }
  }
}

void Model::updateObject(DrawingObject* target, lucGeometryType type, bool compress)
{
  database.reopen(true); //Ensure opened writable
  database.issue("BEGIN EXCLUSIVE TRANSACTION");
  if (type == lucMaxType)
    writeObjects(database, target, step(), compress);
  else
  {
    Geometry* g = getRenderer(type);
    if (g) writeGeometry(database, g, target, step(), compress);
  }

  //Update object
  database.issue("update object set properties = '%s' where name = '%s'", target->properties.data.dump().c_str(), target->name().c_str());

  database.issue("COMMIT");
}

void Model::writeDatabase(const char* path, DrawingObject* obj, bool compress)
{
  //Write objects to a new database?
  Database outdb;
  if (path)
  {
    if (FileExists(path))
      remove(path);
    outdb = Database(FilePath(path));
    if (!outdb.open(true))
    {
      printf("Database write failed '%s': %s\n", path, sqlite3_errmsg(outdb.db));
      return;
    }
  }
  else
  {
    outdb = database;
    database.reopen(true);  //Open writable
  }

  // Remove existing static data
  //outdb.issue("drop table IF EXISTS object");
  //outdb.issue("drop table IF EXISTS state");

  // Create new tables when not present
  outdb.issue("create table IF NOT EXISTS geometry (id INTEGER PRIMARY KEY ASC, object_id INTEGER, timestep INTEGER, rank INTEGER, idx INTEGER, type INTEGER, data_type INTEGER, size INTEGER, count INTEGER, width INTEGER, minimum REAL, maximum REAL, dim_factor REAL, units VARCHAR(32), minX REAL, minY REAL, minZ REAL, maxX REAL, maxY REAL, maxZ REAL, labels VARCHAR(2048), properties VARCHAR(2048), data BLOB, FOREIGN KEY (object_id) REFERENCES object (id) ON DELETE CASCADE ON UPDATE CASCADE, FOREIGN KEY (timestep) REFERENCES timestep (id) ON DELETE CASCADE ON UPDATE CASCADE)");

  outdb.issue(
    "create table IF NOT EXISTS timestep (id INTEGER PRIMARY KEY ASC, time REAL, dim_factor REAL, units VARCHAR(32), properties VARCHAR(2048))");

  outdb.issue(
    "create table IF NOT EXISTS object (id INTEGER PRIMARY KEY ASC, name VARCHAR(256), colourmap_id INTEGER, colour INTEGER, opacity REAL, properties VARCHAR(2048))");

  //Write state
  writeState(outdb);

  outdb.issue("BEGIN EXCLUSIVE TRANSACTION");

  //Write objects
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (!obj || objects[i] == obj)
    {
      if (!outdb.issue("insert into object (name, properties) values ('%s', '%s')", objects[i]->name().c_str(), objects[i]->properties.data.dump().c_str()))
        return;
      //Store the id
      objects[i]->dbid = sqlite3_last_insert_rowid(outdb.db);
    }
  }

  //Write timesteps & objects...

  //Write any fixed data
  writeObjects(outdb, obj, -1, compress);

  for (unsigned int i = 0; i < timesteps.size(); i++)
  {
    outdb.issue("delete from timestep where id == '%d'", i);
    outdb.issue("insert into timestep (id, time, properties) values (%d, %g, '%s');", 
                i, timesteps[i]->time,  timesteps[i]->properties.data.dump().c_str());

    //Get data at this timestep
    //(Only required to support writing database from another database without caching enabled)
    //setTimeStep(i);

    //Write object data
    writeObjects(outdb, obj, i, compress);
  }

  outdb.issue("COMMIT");
}

void Model::writeState()
{
  writeState(database);
}

void Model::writeState(Database& outdb)
{
  //Save changes to the active state first
  storeFigure();

  //Write state
  outdb.issue("create table if not exists state (id INTEGER PRIMARY KEY ASC, name VARCHAR(256), data TEXT)");

  char SQL[SQL_QUERY_MAX];
  for (unsigned int f=0; f<fignames.size(); f++)
  {
    // Delete any state entry with same name
    outdb.issue("delete from state where name == '%s'", fignames[f].c_str());

    snprintf(SQL, SQL_QUERY_MAX, "insert into state (name, data) values ('%s', ?)", fignames[f].c_str());
    sqlite3_stmt* statement;

    if (sqlite3_prepare_v2(outdb.db, SQL, -1, &statement, NULL) != SQLITE_OK)
      abort_program("SQL prepare error: (%s) %s\n", SQL, sqlite3_errmsg(outdb.db));

    if (sqlite3_bind_text(statement, 1, figures[f].c_str(), figures[f].length(), SQLITE_STATIC) != SQLITE_OK)
      abort_program("SQL bind error: %s\n", sqlite3_errmsg(outdb.db));

    if (sqlite3_step(statement) != SQLITE_DONE )
      abort_program("SQL step error: (%s) %s\n", SQL, sqlite3_errmsg(outdb.db));

    sqlite3_finalize(statement);
  }
}

void Model::writeObjects(Database& outdb, DrawingObject* obj, int step, bool compress)
{
  //Write object data
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (!obj || obj == objects[i])
    {
      //Loop through all geometry classes (points/vectors etc)
      for (auto g : geometry)
      {
        writeGeometry(outdb, g, objects[i], step, compress);
      }
    }
  }
}

void Model::deleteGeometry(Database& outdb, lucGeometryType type, DrawingObject* obj, int step)
{
  //Clear existing data of this type before writing, allows object update to db
  outdb.issue("DELETE FROM geometry WHERE object_id=%d and type=%d and timestep=%d;", obj->dbid, type, step);
}

void Model::writeGeometry(Database& outdb, Geometry* g, DrawingObject* obj, int step, bool compressdata)
{
  //Clear existing data of this type before writing, allows object data updates to db
  deleteGeometry(outdb, g->type, obj, step);

  std::vector<Geom_Ptr> data = g->getAllObjectsAt(obj, step);
  //Loop through and write out all object data
  for (unsigned int i=0; i<data.size(); i++)
  {
    for (unsigned int data_type=0; data_type <= lucMaxDataType; data_type++)
    {
      //Write the data entry
      Data_Ptr block = data[i]->dataContainer((lucGeometryDataType)data_type);
      if (!block || block->size() == 0) continue;
      std::cerr << step << "] Writing geometry (type[" << data_type << "] * " << block->size()
                << ") for object : " << obj->dbid << " => " << obj->name() << ", compress: " << compressdata << std::endl;
      writeGeometryRecord(outdb, g->type, (lucGeometryDataType)data_type, obj->dbid, data[i], block.get(), step, compressdata);
    }
    for (unsigned int j=0; j<data[i]->values.size(); j++)
    {
      //Write the value data entry
      DataContainer* block = (DataContainer*)data[i]->values[j].get();
      if (!block || block->size() == 0) continue;
      std::cerr << step << "] Writing geometry (values[" << j << "] * " << block->size()
                << ") for object : " << obj->dbid << " => " << obj->name() << ", compress: " << compressdata << std::endl;
      //TODO: fix to write/read labels for data values from database, preferably in a separate table?
      //This hack will work for up to 7 value data sets for now
      //Filters and colourby properties will need modification though
      unsigned int data_type = lucColourValueData+j;
      if (data_type == lucIndexData) data_type++;
      writeGeometryRecord(outdb, g->type, (lucGeometryDataType)data_type, obj->dbid, data[i], block, step, compressdata);
    }
  }
}

void Model::writeGeometryRecord(Database& outdb, lucGeometryType type, lucGeometryDataType dtype, unsigned int objid, Geom_Ptr data, DataContainer* block, int step, bool compressdata)
{
  char SQL[SQL_QUERY_MAX];
  sqlite3_stmt* statement;
  unsigned char* buffer = (unsigned char*)block->ref(0);
  unsigned long src_len = block->bytes();
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
    min[c] = data->min[c];
    if (!std::isfinite(min[c])) min[c] = session.min[c];
    if (!std::isfinite(min[c])) min[c] = 0.0;
    max[c] = data->max[c];
    if (!std::isfinite(max[c])) max[c] = session.max[c];
    if (!std::isfinite(max[c])) max[c] = 0.0;
  }

  snprintf(SQL, SQL_QUERY_MAX, "insert into geometry (object_id, timestep, rank, idx, type, data_type, size, count, width, minimum, maximum, dim_factor, units, minX, minY, minZ, maxX, maxY, maxZ, labels, data) values (%d, %d, %d, %d, %d, %d, %d, %d, %d, %g, %g, %g, '%s', %g, %g, %g, %g, %g, %g, ?, ?)", objid, step, data->height, data->depth, type, dtype, block->unitsize(), block->size(), data->width, block->minimum, block->maximum, 0.0, block->label.c_str(), min[0], min[1], min[2], max[0], max[1], max[2]);

  /* Prepare statement... */
  if (sqlite3_prepare_v2(outdb.db, SQL, -1, &statement, NULL) != SQLITE_OK)
  {
    abort_program("SQL prepare error: (%s) %s\n", SQL, sqlite3_errmsg(outdb.db));
  }

  /* Setup text data for insert (on vertex block only) */
  std::string labels = data->getLabels().c_str();
  if (dtype == lucVertexData && labels.length() > 0)
  {
    if (sqlite3_bind_text(statement, 1, labels.c_str(), labels.length(), SQLITE_STATIC) != SQLITE_OK)
    //const char* clabels = labels.c_str();
    //if (sqlite3_bind_text(statement, 1, clabels, strlen(clabels), SQLITE_STATIC) != SQLITE_OK)
      abort_program("SQL bind error: %s\n", sqlite3_errmsg(outdb.db));
  }

  /* Setup blob data for insert */
  debug_print("Writing %lu bytes\n", src_len);
  if (sqlite3_bind_blob(statement, 2, buffer, src_len, SQLITE_STATIC) != SQLITE_OK)
    abort_program("SQL bind error: %s\n", sqlite3_errmsg(outdb.db));

  /* Execute statement */
  if (sqlite3_step(statement) != SQLITE_DONE )
    abort_program("SQL step error: (%s) %s\n", SQL, sqlite3_errmsg(outdb.db));

  sqlite3_finalize(statement);

  // Free compression buffer
  if (cmp_len > 0) free(buffer);

  //printf("WROTE ID %d STEP %d TYPE %d DTYPE %d DIMS (%d x %d x %d) COUNT %d LABELS %s\n", 
  //       objid, step, type, dtype, data->width, data->height, data->depth, block->size(), labels.c_str());
}

void Model::deleteObjectRecord(unsigned int id)
{
  if (!database) return;
  database.reopen(true);  //Open writable
  database.issue("DELETE FROM object WHERE id==%1$d; DELETE FROM geometry WHERE object_id=%1$d; DELETE FROM viewport_object WHERE object_id=%1$d;", id);
  database.issue("vacuum");
  //Update state
  writeState();
}

void Model::backup(Database& fromdb, Database& todb)
{
  if (!fromdb || !todb) return;
  sqlite3_backup *pBackup;  // Backup object used to copy data
  pBackup = sqlite3_backup_init(todb.db, "main", fromdb.db, "main");
  if (pBackup)
  {
    (void)sqlite3_backup_step(pBackup, -1);
    (void)sqlite3_backup_finish(pBackup);
  }
}

void Model::calculateBounds(View* aview, float* default_min, float* default_max)
{
  if (default_min && default_max)
  {
    for (int i=0; i<3; i++)
    {
      //Ensure supplied min/max in correct order
      if (default_min[i] > default_max[i]) std::swap(default_min[i], default_max[i]);
      //Init with defaults
      min[i] = default_min[i];
      max[i] = default_max[i];
      //If no range, flag invalid with +/-inf, will be expanded in setView
      if (max[i]-min[i] <= EPSILON) max[i] = -(min[i] = HUGE_VAL);
    }
  }

  //Expand bounds by all geometry objects
  for (auto g : geometry)
    g->setup(aview, min, max);
}

void Model::objectBounds(DrawingObject* obj, float* min, float* max)
{
  if (!min || !max) return;
  for (int i=0; i<3; i++)
    max[i] = -(min[i] = HUGE_VAL);
  //Expand bounds by all geometry objects
  for (auto g : geometry)
    g->objectBounds(obj, min, max);
}

void Model::deleteObject(DrawingObject* obj)
{
  //Delete geometry
  for (unsigned int i=0; i < geometry.size(); i++)
    geometry[i]->remove(obj);

  //Delete from model obj list
  for (unsigned int i=0; i<objects.size(); i++)
  {
    if (obj == objects[i])
    {
      objects.erase(objects.begin()+i);
      break;
    }
  }

  //Delete from viewport obj list
  for (unsigned int v=0; v < views.size(); v++)
  {
    for (unsigned int i=0; i<views[v]->objects.size(); i++)
    {
      if (obj == views[v]->objects[i])
      {
        views[v]->objects.erase(views[v]->objects.begin()+i);
        break;
      }
    }
  }

  //Free memory
  delete obj;

  //Flag redraw
  redraw();
}

json Model::objectDataSets(DrawingObject* o)
{
  json dict;
  for (auto g : geometry)
  {
    //Flag has data of this type (still necessary? was for old html UI or WebGL?)
    //std::string name = GeomData::names[g->type];
    //if (g->getVertexCount(objects[i]) > 0)
    //  obj[name] = true;

    json list = g->getDataLabels(o);
    std::string key;
    for (auto dataobj : list)
    {
      key = dataobj["label"];
      dataobj.erase("label");
      dict[key] = dataobj;
    }
  }
  return dict;
}

std::string Model::jsonWrite(bool objdata)
{
  std::ostringstream json;
  jsonWrite(json, NULL, objdata);
  return json.str();
}

void Model::jsonWrite(std::ostream& os, DrawingObject* o, bool objdata)
{
  //Write new JSON format objects
  // - globals are all stored on / sourced from session.globals
  // - views[] list holds view properies (previously single instance in "options")
  std::lock_guard<std::mutex> guard(session.mutex);
  json exported;
  json properties = session.globals;
  json cmaps = json::array();
  json outobjects = json::array();
  json outviews = json::array();

  //Converts named colours to js readable
  if (properties.count("background") > 0)
    properties["background"] = Colour(properties["background"]).toString();

  for (unsigned int v=0; v < views.size(); v++)
  {
    //Add the view
    View* view = views[v];
    view->exportProps();
    outviews.push_back(view->properties.data);
  }

  for (unsigned int i = 0; i < colourMaps.size(); i++)
  {
    json cmap = colourMaps[i]->toJSON();
    cmaps.push_back(cmap);
  }

  //if (!viewer->visible) aview->filtered = false; //Disable viewport filtering
  for (unsigned int i=0; i < objects.size(); i++)
  {
    if (!o || objects[i] == o)
    {
      //Only able to dump point/triangle based objects currently:
      //TODO: fix to use sub-renderer output for others
      //"Labels", "Points", "Grid", "Triangles", "Vectors", "Tracers", "Lines", "Shapes", "Volumes"

      json obj = objects[i]->properties.data;

      //Include the object bounding box for WebGL
      //float min[3], max[3];
      //objectBounds(objects[i], min, max);
      //Above doesn't work correctly, just use model bounds
      if (min[0] < max[0] && min[1] < max[1])
      {
        obj["min"] = {min[0], min[1], min[2]};
        obj["max"] = {max[0], max[1], max[2]};
      }

      //TODO: export geom textures
      //Texture ? Export first only, as external file for now
      //TODO: dataurl using getImageUrlString(image, iw, ih, channels)
      //if (objects[i]->textures.size() > 0 && obj.count("texturefile") == 0)
      //  obj["texturefile"] = objects[i]->textures[0]->fn.full;

      if (!objdata)
      {
        //Data labels
        json dict = objectDataSets(objects[i]);
        if (dict.size() > 0)
          obj["data"] = dict;

        //std::cout << "HAS OBJ TYPES: (point,tri,vol)" << obj.count("points") << "," 
        //          << obj.count("triangles") << "," << obj.count("volume") << std::endl;
        outobjects.push_back(obj);
        continue;
      }

      for (auto g : geometry)
      {
        //Collect vertex/normal/index/value data
        g->jsonWrite(objects[i], obj);
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
  //exported["dictionary"] = session.properties; //Store for default info etc
  //Should not set this unless data changed for webgl?
  //exported["reload"] = true;
  exported["reload"] = objdata;
  if (figure >= 0 && figure < (int)fignames.size())
    exported["figure"] = fignames[figure];

  //Export with indentation
  os << std::setw(2) << exported;
}

int Model::jsonRead(std::string data)
{
  std::lock_guard<std::mutex> guard(session.mutex);
  
  json imported = json::parse(data);

  //List of keys to ignore on import if already set
  //(Default is to always replace if found in imported data,
  // this is reversed for these keys, existing value takes precedence)
  std::string skiplist[] = {"resolution", "antialias"};
  for (auto del : skiplist)
    if (session.globals.count(del)) imported["properties"].erase(del);

  //Load globals, merge with existing values
  Properties::mergeJSON(session.globals, imported["properties"]);

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
      View* view = new View(session);
      views.push_back(view);
      //Insert all objects for now
      view->objects = objects;
    }

    View* view = views[v];

    //Process list of keys to ignore on import if already set
    for (auto del : skiplist)
      if (view->properties.has(del) || session.globals.count(del))
        inviews[v].erase(del);

    //Apply base properties with merge
    view->properties.merge(inviews[v]);

    view->importProps();
  }

  // Import colourmaps
  json cmaps = imported["colourmaps"];
  for (unsigned int i=0; i < cmaps.size(); i++)
  {
    //Set default name if none provided
    ColourMap* dest = NULL;
    if (cmaps[i].count("name") > 0)
    {
      dest = findColourMap(cmaps[i]["name"]);
      if (!dest)
        dest = addColourMap(cmaps[i]["name"]);
    }
    else
        dest = addColourMap();

    //Replace/set name in case modified
    cmaps[i]["name"] = dest->name;

    //Replace properties with imported
    dest->properties.data = cmaps[i];
    dest->properties.checkall();
    json cmap = dest->properties.data;

    json colours = cmap["colours"];
    //Replace colours
    dest->colours.clear();
    for (unsigned int c=0; c < colours.size(); c++)
    {
      json colour = colours[c];
      Colour newcolour(colour["colour"]);
      dest->addAt(newcolour, colour["position"]);
    }
  }

  //Import objects
  json inobjects = imported["objects"];
  unsigned int len = objects.size();
  if (len < inobjects.size()) len = inobjects.size();
  for (unsigned int i=0; i < inobjects.size(); i++)
  {
    std::string name = inobjects[i]["name"];
    DrawingObject* dest = findObject(name);
    if (!dest)
    {
      dest = new DrawingObject(session, name);
      addObject(dest);
    }

    //Convert colourmap to use name
    json cm = inobjects[i]["colourmap"];
    if (cm.is_number())
    {
      int cmid = cm;
      if (cmid >= 0 && cmid < (int)cmaps.size())
        inobjects[i]["colourmap"] = cmaps[cmid]["name"];
      else
        inobjects[i]["colourmap"] = "";
    }
    
    //Merge properties
    dest->properties.merge(inobjects[i]);
    //reload(dest);
  }

  //If integer reload code provided, return it, otherwise reload if true, redraw otherwise
  if (imported["reload"].is_number())
  {
    return imported["reload"];
  }
  else if ((imported["reload"].is_boolean() && imported["reload"]))
    reload();
  else
    redraw();
  return 0;
}


