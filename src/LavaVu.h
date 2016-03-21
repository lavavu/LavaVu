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
std::string image(std::string filename="");
void killViewer();
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
  virtual bool mouseScroll(int scroll);
  virtual bool keyPress(unsigned char key, int x, int y);

  virtual bool parseCommands(std::string cmd);
  bool parseCommand(std::string cmd);
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
  void displayText(std::string text, int lineno=1, int colour=0);
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
  void jsonWrite(std::ostream& os, unsigned int id=0, bool objdata=false);
  void jsonRead(std::string data);
};

#define HELP_MESSAGE "\
\n\
Hold the Left mouse button and drag to Rotate about the X & Y axes\n\
Hold the Right mouse button and drag to Pan (left/right/up/down)\n\
Hold the Middle mouse button and drag to Rotate about the Z axis\n\
Hold [shift] and use the scroll wheel to move the clip plane in and out.\n\
\n\
[F1]         Print help\n\
[UP]         Previous command in history if available\n\
[DOWN]       Next command in history if available\n\
[ALT+UP]     Load previous model in list at current time-step if data available\n\
[ALT+DOWN]   Load next model in list at current time-step if data available\n\
[LEFT]       Previous time-step\n\
[RIGHT]      Next time-step\n\
[Page Up]    Select the previous viewport if available\n\
[Page Down]  Select the next viewport if available\n\
\n\
\nHold [ALT] plus:\n\
[`]          Full screen ON/OFF\n\
[*]          Auto zoom to fit ON/OFF\n\
[/]          Stereo ON/OFF\n\
[\\]          Switch coordinate system Right-handed/Left-handed\n\
[|]          Switch rulers ON/OFF\n\
[,]          Switch to next particle rendering texture\n\
[+/=]        More particles (reduce sub-sampling)\n\
[-]          Less particles (increase sub-sampling)\n\
\n\
[a]          Hide/show axis\n\
[b]          Background colour switch WHITE/BLACK\n\
[B]          Background colour gray\n\
[c]          Camera info: XML output of current camera parameters\n\
[d]          Draw quad surfaces as triangle strips ON/OFF\n\
[f]          Frame box mode ON/FILLED/OFF\n\
[g]          Colour map log scales override DEFAULT/ON/OFF\n\
[i]          Take screen-shot and save as png/jpeg image file\n\
[j]          Experimental: localise colour scales, minimum and maximum calibrated to each object drawn\n\
[J]          Restore original colour scale min & max\n\
[k]          Lock colour scale calibrations to current values ON/OFF\n\
[l]          Lighting ON/OFF\n\
[m]          Model bounding box update - resizes based on min & max vertex coords read\n\
[n]          Recalculate surface normals\n\
[o]          Print list of object names with id numbers.\n\
[r]          Reset camera viewpoint\n\
[q] or [ESC] Quit program\n\
[u]          Backface Culling ON/OFF\n\
[w]          Wireframe ON/OFF\n\
\n\
[v]          Increase vector size scaling\n\
[V]          Reduce vector size scaling\n\
[t]          Increase tracer size scaling\n\
[T]          Reduce tracer size scaling\n\
[p]          Increase particle size scaling\n\
[P]          Reduce particle size scaling\n\
[s]          Increase shape size scaling\n\
[S]          Reduce shape size scaling\n\
\n\
[p] [ENTER]  hide/show all particle swarms\n\
[v] [ENTER]  hide/show all vector arrow fields\n\
[t] [ENTER]  hide/show all tracer trajectories\n\
[q] [ENTER]  hide/show all quad surfaces (scalar fields, cross sections etc.)\n\
[u] [ENTER]  hide/show all triangle surfaces (isosurfaces)\n\
[s] [ENTER]  hide/show all shapes\n\
[i] [ENTER]  hide/show all lines\n\
\n\
Verbose commands:\n\
help commands [ENTER] -- list of all verbose commands\n\
help * [ENTER] where * is a command -- detailed help for a command\n\
\n\
"

#define HELP_COMMANDS "\
Viewer command line options:\n\n\
\nStart and end timesteps\n\
 -# : Any integer entered as a switch will be interpreted as the initial timestep to load\n\
      Any following integer switch will be interpreted as the final timestep for output\n\
      eg: -10 -20 will run all output commands on timestep 10 to 20 inclusive\n\
 -c#: caching, set # of timesteps to cache data in memory for\n\
 -P#: subsample points\n\
 -A : All objects hidden initially, use 'show object' to display\n\
 -N : No load, deferred loading mode, use 'load object' to load & display from database\n\
\nGeneral options\n\
 -v : Verbose output, debugging info displayed to console\n\
 -o : output mode: all commands entered dumped to standard output,\n\
      useful for redirecting to a script file to recreate a visualisation setup.\n\
 -p#: port, web server interface listen on port #\n\
 -q#: quality, web server jpeg quality (0=don't serve images)\n\
 -n#: number of threads to launch for web server #\n\
 -l: use local shaders, locate in working directory not executable directory\n\
\nImage/Video output\n\
 -w: write images of all loaded timesteps/windows then exit\n\
 -i: as above\n\
 -W: write images as above but using input database path as output path for images\n\
 -I: as above\n\
 -t: write transparent background png images (if supported)\n\
 -m#: write movies of all loaded timesteps/windows #=fps(30) (if supported)\n\
 -xWIDTH,HEIGHT: set output image width (height optional, will be calculated if omitted)\n\
\nData export\n\
 -d#: export object id # to CSV vertices + values\n\
 -j#: export object id # to JSON, if # omitted will output all compatible objects\n\
 -g#: export object id # to GLDB, if # omitted will output all compatible objects\n\
\nWindow settings\n\
 -rWIDTH,HEIGHT: resize initial viewer window to width x height\n\
 -h: hidden window, will exit after running any provided input script and output options\n\
"

#endif //LavaVu__
