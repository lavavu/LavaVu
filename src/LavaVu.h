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
#include "Win.h"
#include "VideoEncoder.h"

#define MAX_MSG 256

std::string execute(int argc, char **argv);
std::string execute(int argc, char **argv, ViewerApp* myApp);
void command(std::string cmd);
std::string image(std::string filename="", int width=0, int height=0);
void addObject(std::string name, std::string properties);
void loadVertices(std::vector< std::vector <float> > array);
void loadValues(std::vector <float> array);
void display();
void clear();
void kill();

OpenGLViewer* createViewer();

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
  bool output, verbose, hideall, dbpath;
  std::string defaultScript;

  int viewset;
  bool globalCam;
  bool sort_on_rotate;
  bool status;
  int fixedwidth, fixedheight;
  bool writeimage;
  int writemovie;
  int volres[3];
  float volmin[3], volmax[3];
  float volss[3], inscale[3]; //Scaling as data loaded, only for volumes currently
  int volchannels;
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
  std::vector<Win*> windows;
  std::vector<FilePath> files;

  // Loaded model parameters
  int startstep, endstep;
  lucExportType dump;
  lucExportType returndata;
  int dumpid;
  int window;
  int tracersteps;
  bool objectlist;
  bool swapY;
  int trisplit;

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
  bool recording;
  bool loop;
  int animate;
  char message[MAX_MSG];
  std::string help;

  int view;

  Model* amodel; //Active model
  Win* awin;     //Active window
  View* aview;   //Active viewport
  DrawingObject* aobject; //Selected object

  LavaVu();
  void defaults();
  virtual ~LavaVu();

  //Argument parser
  virtual void arguments(std::vector<std::string> args);
  //Execute function
  std::string run();
  void clearData(bool objects=false);

  void exportData(lucExportType type, unsigned int id=0);

  void parseProperties(std::string& properties);
  void parseProperty(std::string& data);
  void printProperties();
  void printDefaultProperties();

  void reloadShaders();

  void addTriangles(DrawingObject* obj, float* a, float* b, float* c, int level);
  void readHeightMap(FilePath& fn);
  void readHeightMapImage(FilePath& fn);
  void readOBJ(FilePath& fn);
  void readTecplot(FilePath& fn);
  void readRawVolume(FilePath& fn);
  void readXrwVolume(FilePath& fn);
  void readVolumeSlice(FilePath& fn);
  void readVolumeSlice(std::string& name, GLubyte* imageData, int width, int height, int bytesPerPixel, int outChannels);
  void readVolumeTIFF(FilePath& fn);
  void createDemoModel();
  void createDemoVolume();
  void newModel(std::string name, int bg=0, float mmin[3]=NULL, float mmax[3]=NULL);
  DrawingObject* addObject(DrawingObject* obj);
  void setOpacity(unsigned int id, float opacity);
  void redraw(unsigned int id);

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

  void addWindow(Win* win);
  void resetViews(bool autozoom=false);
  void viewSelect(int idx, bool setBounds=false, bool autozoom=false);
  int viewFromPixel(int x, int y);

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

  void writeImages(int start, int end);
  void encodeVideo(std::string filename="", int fps=30);
  void writeSteps(bool images, int start, int end);

  //data loading
  void loadFile(FilePath& fn);
  void defaultModel();
  void loadModel(FilePath& fn);
  bool loadWindow(int window_idx, int at_timestep=-1, bool autozoom=false);
  void cacheLoad();
  bool tryTimeStep(int ts);

  //Interactive command & script processing
  bool parseChar(unsigned char key);
  Geometry* getGeometryType(std::string what);
  DrawingObject* findObject(std::string what, unsigned int id);
  ColourMap* findColourMap(std::string what, unsigned int id);
  std::string helpCommand(std::string cmd);
  void record(bool mouse, std::string command);
  void dumpById(unsigned int id);
  void jsonWriteFile(unsigned int id=0, bool jsonp=false, bool objdata=true);
  void jsonWriteFile(std::string fn, unsigned int id, bool jsonp, bool objdata);
  void jsonWrite(std::ostream& os, unsigned int id=0, bool objdata=false);
  void jsonReadFile(std::string fn);
  void jsonRead(std::string data);
};

#endif //LavaVu__
