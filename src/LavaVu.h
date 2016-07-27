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

#ifndef LavaVu__
#define LavaVu__

#include "Util.h"
#include "GraphicsUtil.h"
#include "ColourMap.h"
#include "Geometry.h"
#include "View.h"
#include "ViewerApp.h"
#include "Model.h"
#include "VideoEncoder.h"

#ifndef APPNAME__
#define APPNAME__ "LavaVu"
#endif

#define MAX_MSG 256

typedef enum
{
  lucExportNone,
  lucExportCSV,
  lucExportJSON,
  lucExportJSONP,
  lucExportGLDB,
  lucExportGLDBZ,
  lucExportIMAGE
} lucExportType;

class LavaVu : public ViewerApp
{
protected:
  bool verbose, hideall, dbpath;
  std::string defaultScript;

  int viewset;
  int initfigure;
  bool sort_on_rotate;
  bool status;
  bool writeimage;
  int writemovie;
  DrawingObject *volume;
  int repeat;
#ifdef HAVE_LIBAVCODEC
  VideoEncoder* encoder;
#else
  //Ensure LavaVu class is same size in case linked from executable compiled
  //with different definitions (this was the cause of a hard to find bug)
  void* encoder;
#endif

  std::vector<Model*> models;

  // Loaded model parameters
  int startstep, endstep;
  lucExportType dump;
  int dumpid;
  int model;
  int tracersteps;
  bool objectlist;

  //Interaction: Key command entry
  std::string entry;
  std::vector<std::string> history;
  std::vector<std::string> linehistory;
  std::vector<std::string> replay;

  TriSurfaces* axis;
  Lines* rulers;
  QuadSurfaces* border;

public:
  bool automate;
  bool quiet;
  bool loop;
  int animate;
  char message[MAX_MSG];
  std::string help;

  int view;

  Model* amodel; //Active model
  View* aview;   //Active viewport
  DrawingObject* aobject; //Selected object

  LavaVu(std::string binary=APPNAME__);
  void defaults();
  virtual ~LavaVu();

  //Argument parser
  virtual void arguments(std::vector<std::string> args);
  //Execute function
  void run(std::vector<std::string> args={});
  void clearData(bool objects=false);

  void exportData(lucExportType type, DrawingObject* obj=NULL);

  void parseProperties(std::string& properties);
  void parseProperty(std::string& data);
  void printProperties();
  void printDefaultProperties();

  void reloadShaders();

  void addTriangles(DrawingObject* obj, float* a, float* b, float* c, int level);
  void readHeightMap(const FilePath& fn);
  void readHeightMapImage(const FilePath& fn);
  void readOBJ(const FilePath& fn);
  void readRawVolume(const FilePath& fn);
  void readXrwVolume(const FilePath& fn);
  void readVolumeSlice(const FilePath& fn);
  void readVolumeSlice(const std::string& name, GLubyte* imageData, int width, int height, int bytesPerPixel);
  void readVolumeTIFF(const FilePath& fn);
  void createDemoModel(unsigned int numpoints);
  void createDemoVolume();
  void newModel(std::string name, int bg=0, float mmin[3]=NULL, float mmax[3]=NULL);
  DrawingObject* addObject(DrawingObject* obj);
  void setOpacity(unsigned int id, float opacity);
  void redraw(DrawingObject* obj);

  //*** Our application interface:

  // Virtual functions for window management
  virtual void open(int width, int height);
  virtual void resize(int new_width, int new_height);
  virtual void display();
  virtual void close();

  // Virtual functions for interactivity
  virtual bool mouseMove(int x, int y);
  virtual bool mousePress(MouseButton btn, bool down, int x, int y);
  virtual bool mouseScroll(float scroll);
  virtual bool keyPress(unsigned char key, int x, int y);

  float parseCoord(const std::string& str);
  virtual bool parseCommands(std::string cmd);
  bool parseCommand(std::string cmd, bool gethelp=false);
  bool parsePropertySet(std::string cmd);
  virtual std::string requestData(std::string key);
  //***

  void resetViews(bool autozoom=false);
  void viewSelect(int idx, bool setBounds=false, bool autozoom=false);

  GeomData* getGeometry(DrawingObject* obj);
  void displayObjectList(bool console=true);
  void printMessage(const char *fmt, ...);
  void text(const std::string& str, int xpos=10, int ypos=0, float scale=1.0, Colour* colour=NULL);
  void displayText(const std::string& str, int lineno=1, Colour* colour=NULL);
  void displayMessage();
  void drawColourBar(DrawingObject* draw, int startx, int starty, int length, int height);
  void drawScene(void);
  void drawSceneBlended();

  void drawRulers();
  void drawRuler(DrawingObject* obj, float start[3], float end[3], float labelmin, float labelmax, int ticks, int axis);
  void drawBorder();
  void drawAxis();

  void encodeVideo(std::string filename="", int fps=30);
  void writeSteps(bool images, int start, int end);

  //data loading
  bool loadFile(const FilePath& fn);
  void defaultModel();
  void loadModel(FilePath& fn);
  bool loadModelStep(int model_idx, int at_timestep=-1, bool autozoom=false);
  void cacheLoad();
  bool tryTimeStep(int ts);

  //Interactive command & script processing
  bool parseChar(unsigned char key);
  Geometry* getGeometryType(std::string what);
  DrawingObject* lookupObject(PropertyParser& parsed, const std::string& key, int idx=0);
  std::vector<DrawingObject*> lookupObjects(PropertyParser& parsed, const std::string& key, int start=0);
  int lookupColourMap(PropertyParser& parsed, const std::string& key, int idx=0);
  void helpCommand(std::string cmd);
  void dumpCSV(DrawingObject* obj=NULL);
  std::string jsonWriteFile(DrawingObject* obj=NULL, bool jsonp=false, bool objdata=true);
  void jsonWriteFile(std::string fn, DrawingObject* obj=NULL, bool jsonp=false, bool objdata=true);
  void jsonReadFile(std::string fn);

  //Python interface functions
  std::string image(std::string filename="", int width=0, int height=0);
  std::string web(bool tofile=false);
  void addObject(std::string name, std::string properties);
  void setState(std::string state);
  std::string getStates();
  std::string getTimeSteps();
  void vertices(std::vector< std::vector <float> > array);
  void values(std::vector <float> array);
};

#endif //LavaVu__
