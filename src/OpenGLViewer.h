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

//OpenGLViewer - handles all GL rendering & display
#ifndef OpenGLViewer__
#define OpenGLViewer__

#include "GraphicsUtil.h"
#include "ApplicationInterface.h"
#include "OutputInterface.h"
#include "InputInterface.h"

extern std::thread::id no_thread;
#ifdef DEBUG
#define GL_Check_Thread(thread) \
{ \
  if (thread != no_thread && thread != std::this_thread::get_id()) \
    abort_program("FATAL: must call GL functions from render thread"); \
}
#else
#define GL_Check_Thread(thread)
#endif //DEBUG

class FrameBuffer
{
public:
  int width = 0, height = 0;
  GLuint target = 0;

  FrameBuffer() {}
  virtual ~FrameBuffer() {}
  virtual ImageData* pixels(ImageData* image, int channels=3);
  virtual int getOutWidth() {return width;}
  virtual int getOutHeight() {return height;}
};

class FBO : public FrameBuffer
{
public:
  bool enabled = false;
  GLuint frame = 0;
  GLuint texture = 0;
  GLuint depth = 0;
  GLuint rgba = 0;
  int downsample = 1;
  int msaa = 1;

  FBO() : FrameBuffer() {}

  ~FBO();

  bool create(int w, int h, int samples=1);
  void destroy();
  void disable();
  ImageData* pixels(ImageData* image, int channels=3);

  float downsampleFactor()   {return pow(2, downsample-1);}
  virtual int getOutWidth()  {return width / downsampleFactor();}
  virtual int getOutHeight() {return height / downsampleFactor();}
};

class OpenGLViewer : public ApplicationInterface, public FrameBuffer
{
private:

protected:
  int timer = 0;
  int elapsed = 0;
  int animate = 0; //Redisplay when idle for # milliseconds

  std::vector<OutputInterface*> outputs; //Additional output attachments
  std::vector<InputInterface*> inputs; //Additional input attachments
  FBO fbo;
  FBO fbo_blit; //Non-multisampled fbo to blit to
  int savewidth = 0, saveheight = 0;

public:
  int idle = 0;
  //Timer increments in ms
  int timer_msec = 10;
  int timer_animate = 50;
  bool timeloop = false;

  ApplicationInterface* app;
  std::deque<std::string> commands;
  std::mutex cmd_mutex;

  GLboolean stereoBuffer, doubleBuffer;
  GLint sb, ss;
  bool visible = true;
  bool stereo = false;
  bool fullscreen = false;
  bool postdisplay = false; //Flag to request a frame when animating
  bool quitProgram = false;
  bool isopen = false;   //Set when window is first opened
  bool nodisplay = false; //Set to disable calling display and timers - when managing render loop externally

  std::thread::id render_thread;

  int mouseState = 0;
  ShiftState keyState = {false, false, false};
  MouseButton button = NoButton;
  int mouseX, mouseY;
  int lastX, lastY;

  int blend_mode = BLEND_NONE;
  int outwidth = 0, outheight = 0;
  std::string output_path = "";
  bool imagemode = false;

  OpenGLViewer();
  virtual ~OpenGLViewer();

  virtual int getOutWidth() {return (fbo.enabled ? fbo.getOutWidth() : FrameBuffer::getOutWidth());}
  virtual int getOutHeight() {return (fbo.enabled ? fbo.getOutHeight() : FrameBuffer::getOutHeight());}

  //Window app management - called by derived classes, in turn call application interface virtuals
  virtual void open(int width=0, int height=0);
  virtual void init();
  bool useFBO(int width=0, int height=0);
  virtual void setsize(int width, int height);
  virtual void resize(int new_width, int new_height);
  virtual void resizeOutputs(int w, int h);
  virtual void display(bool redraw=true);
  virtual void close() {}
  virtual void setTimer(int msec);

  // Default virtual functions for interactivity (call application interface)
  virtual bool mouseMove(int x, int y)
  {
    return app->mouseMove(x,y);
  }
  virtual bool mousePress(MouseButton btn, bool down, int x, int y)
  {
    return app->mousePress(btn, down, x, y);
  }
  virtual bool mouseScroll(float scroll)
  {
    return app->mouseScroll(scroll);
  }
  virtual bool keyPress(unsigned char key, int x, int y)
  {
    return app->keyPress(key, x, y);
  }

  virtual void show();
  virtual void hide();
  virtual void title(std::string title) {}
  virtual void execute();
  virtual bool events();
  void loop(bool interactive=true);

  virtual void fullScreen() {}
  void outputON(int w, int h, int channels=3, bool vid=false);
  void outputOFF();
  void disableFBO();
  ImageData* pixels(ImageData* image, int channels=3);
  ImageData* pixels(ImageData* image, int w, int h, int channels=3);
  std::string image(const std::string& path="", int jpegquality=0, bool transparent=false, int w=0, int h=0);

  void downSample(int q);
  void multiSample(int q);

  void animateTimer(int msec=-1);

  void addOutput(OutputInterface* output)
  {
    outputs.push_back(output);
  }

  void removeOutput()
  {
    outputs.pop_back();
  }

  void addInput(InputInterface* input)
  {
    inputs.push_back(input);
  }

  bool pollInput(void);
};

#endif //OpenGLViewer__
