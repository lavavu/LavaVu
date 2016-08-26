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

//Timer increment in ms
#define TIMER_INC 50
#define TIMER_IDLE 1500

#include "GraphicsUtil.h"
#include "ApplicationInterface.h"
#include "OutputInterface.h"
#include "InputInterface.h"

class OpenGLViewer : public ApplicationInterface
{
private:

protected:
  int timer;

  static int idle;
  static int displayidle; //Redisplay when idle for # milliseconds
  std::vector<OutputInterface*> outputs; //Additional output attachments
  static std::vector<InputInterface*> inputs; //Additional input attachments

public:
  ApplicationInterface* app;
  static std::deque<std::string> commands;
  static pthread_mutex_t cmd_mutex;

  GLboolean stereoBuffer, doubleBuffer;
  GLuint renderBuffer;
  bool visible;
  bool stereo;
  bool fullscreen;
  bool postdisplay; //Flag to request a frame when animating
  bool quitProgram;
  bool isopen;   //Set when window is first opened

  bool fbo_enabled;
  GLuint fbo_frame, fbo_texture, fbo_depth;
  int downsample;

  int mouseState;
  ShiftState keyState;
  MouseButton button;
  int last_x, last_y;

  int blend_mode;
  Colour background;
  Colour inverse;

  int outwidth, outheight;
  std::string title;
  std::string output_path;
  int width;
  int height;

  OpenGLViewer();
  virtual ~OpenGLViewer();

  void fbo(int width, int height);

  //Window app management - called by derived classes, in turn call application interface virtuals
  virtual void open(int width=0, int height=0);
  virtual void init();
  virtual void setsize(int width, int height);
  virtual void resize(int new_width, int new_height);
  virtual void display();
  virtual void swap() {};
  virtual void close();
  virtual void animate(int msec);

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
  virtual void setTitle() {}
  virtual void execute();

  virtual void fullScreen() {}
  void pixels(void* buffer, bool alpha=false, bool flip=false);
  std::string image(const std::string& path="", bool jpeg=false);

  void setBackground() {setBackground(background);}
  void setBackground(const Colour& bgcol)
  {
    background = bgcol;
    inverse = background;
    inverse.invert();
    if (isopen)
    {
      PrintSetColour(inverse.value);
      //Set clear colour
      glClearColor(background.r/255.0, background.g/255.0, background.b/255.0, 0);
    }
  }

  void idleReset();
  void idleTimer(int display=TIMER_IDLE);

  void addOutput(OutputInterface* output)
  {
    outputs.push_back(output);
  }

  static void addInput(InputInterface* input)
  {
    inputs.push_back(input);
  }

  static bool pollInput(void);
};

#endif //OpenGLViewer__
