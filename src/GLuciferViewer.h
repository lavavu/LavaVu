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

#ifndef GLuciferViewer__
#define GLuciferViewer__

#include "GraphicsUtil.h"
#include "ColourMap.h"
#include "Geometry.h"
#include "View.h"
#include "ViewerApp.h"
#include "Model.h"
#include "Win.h"

class GLuciferViewer : public ViewerApp
{
  protected:
   bool viewAll;
   bool viewPorts;
   bool sort_on_rotate;
   int fixedwidth, fixedheight;
   bool writeimage, writemovie;

   char message[256];

   std::vector<Model*> models;
   std::vector<Win*> windows;
   std::vector<FilePath> files;

   // Loaded model parameters
   int timestep, endstep;
   int dump, dumpid;
   int window;
   int tracersteps;
   bool noload;
   bool objectlist;

   //Interaction: Key command entry 
   std::string entry;
   std::vector<std::string> history;
   std::vector<std::string> linehistory;

  public:
   bool recording;
   bool loop;

   bool filters[lucMaxType];
   bool output;
   int view;

   Model* amodel; //Active model
   Win* awin;     //Active window
   View* aview;   //Active viewport

   //Object geometry
   std::vector<Geometry*> geometry;
   Geometry* labels;
   Points* points;
   Vectors* vectors;
   Tracers* tracers;
   QuadSurfaces* quadSurfaces;
   TriSurfaces* triSurfaces;
   Lines* lines;
   Shapes* shapes;
   Volumes* volumes;

   GLuciferViewer(std::vector<std::string> args, OpenGLViewer* viewer, int width=0, int height=0);
   virtual ~GLuciferViewer();

   void run(bool persist=false);

   void readScriptFile(FilePath& fn);
   void readHeightMap(FilePath& fn);
   void createDemoModel();
   void newModel(std::string name, int w, int h, int bg, float mmin[3], float mmax[3]);
   DrawingObject* newObject(std::string name="", bool persistent=false, int colour=0, ColourMap* map=NULL, float opacity=1.0, const char* properties="");
   void showById(unsigned int id, bool state);
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
   virtual std::string requestData(std::string key);
   //***


   void addWindow(Win* win);
   void addColourMap(ColourMap* cmap);

   void displayCurrentView();

   void resetViews(bool autozoom=false);
   void viewSelect(int idx);
   void viewModel(int idx, bool autozoom=false);
   void redrawViewports();
   int viewFromPixel(int x, int y);

   void clearObjects(bool all=false);
   void redrawObjects();
   void displayObjectList(bool console=true);
   void printMessage(const char *fmt, ...);
   void displayText(std::string text, int lineno=1, int colour=0);
   void displayMessage();
   void drawColourBar(DrawingObject* draw, int startx, int starty, int length, int height);
   void drawScene(void);
   void drawSceneBlended();

   void writeImages(int start, int end);
   void encodeVideo(const char* filename, int start, int end);
   void writeSteps(bool images, bool video, int start, int end, const char* filename);

   //db loading
   bool loadModel(std::string& fn);
   bool loadWindow(int window_idx, int at_timestep=-1, bool autozoom=false);
   bool tryTimeStep(int ts);
   int setTimeStep(int ts);
   int getTimeStep() {return timestep;}
   int loadGeometry(int object_id);
   int loadGeometry(int object_id, int time_start, int time_stop, bool recurseTracers);
   int decompressGeometry(int object_id, int timestep);

   //Interactive command & script processing
   bool parseChar(unsigned char key);
   Geometry* getGeometryType(std::string what);
   DrawingObject* findObject(std::string what, int id);
   ColourMap* findColourMap(std::string what, int id);
   std::string helpCommand(std::string cmd);
   void record(bool mouse, std::string command);
   void dumpById(unsigned int id);
   void jsonWriteFile(unsigned int id=0, bool jsonp=false);
   void jsonWrite(std::ostream& json, unsigned int id=0, bool objdata=false);
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
[Home]       View All mode ON/OFF, shows all objects in a single viewport\n\
[End]        ViewPort mode ON/OFF, shows all viewports in window together\n\
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

#endif //GLuciferViewer__
