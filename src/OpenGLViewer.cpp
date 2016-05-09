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

//OpenGLViewer class
#include "OpenGLViewer.h"

std::deque<std::string> OpenGLViewer::commands;
pthread_mutex_t OpenGLViewer::cmd_mutex;
int OpenGLViewer::idle = 0;
int OpenGLViewer::displayidle = 0;
std::vector<InputInterface*> OpenGLViewer::inputs; //Additional input attachments

//OpenGLViewer class implementation...
OpenGLViewer::OpenGLViewer() : stereo(false), fullscreen(false), postdisplay(false), quitProgram(false), isopen(false), mouseState(0), button(NoButton), blend_mode(BLEND_NORMAL), outwidth(0), outheight(0)
{
  keyState.shift = keyState.ctrl = keyState.alt = 0;

  timer = 0;
  visible = true;
  fbo_enabled = false;
  fbo_texture = fbo_depth = fbo_frame = 0;

  title = "LavaVu";
  output_path = "";

  setBackground();

  downsample = 1;

  /* Init mutex */
  pthread_mutex_init(&cmd_mutex, NULL);

  //Clear static data
  inputs.clear();
  outputs.clear();
}

OpenGLViewer::~OpenGLViewer()
{
  animate(0);

  DeleteFont();
  lucDeleteFont();
}

void OpenGLViewer::open(int w, int h)
{
  //Open window, called before window manager open
  //Set width and height

  //Always use the output width when set for hidden window
  fbo_enabled = !visible; //Always use fbo for hidden window
  if (!visible && outwidth > 0 && outwidth != w)
  {
    h = outheight;
    w = outwidth;
  }
  //Set width/height or use defaults
  width = w > 0 ? w : 1024;
  height = h > 0 ? h : 768;
  debug_print("Window opened %d x %d\n", width, height);
}

void OpenGLViewer::init()
{
  //Init OpenGL (called after context creation)
  GLboolean b;
  GLint i, d, s, u, a, sb, ss;

  glGetBooleanv(GL_RGBA_MODE, &b);
  glGetIntegerv(GL_ALPHA_BITS, &i);
  glGetIntegerv(GL_STENCIL_BITS, &s);
  glGetIntegerv(GL_DEPTH_BITS, &d);
  glGetIntegerv(GL_ACCUM_RED_BITS, &a);
  glGetIntegerv(GL_MAX_TEXTURE_UNITS, &u);
  glGetIntegerv(GL_SAMPLE_BUFFERS, &sb);
  glGetIntegerv(GL_SAMPLES, &ss);
  glGetBooleanv(GL_STEREO, &stereoBuffer);
  glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffer);

  //Set buffers
  if (doubleBuffer) renderBuffer = GL_BACK;
  else renderBuffer = GL_FRONT;

  debug_print("Stereo %d Double-buffer %d RGBA Mode = %d, Alpha bits = %d, Depth bits = %d, Stencil bits = %d, Accum bits = %d, Texture units %d, SampleBuffers %d, Samples %d\n", stereoBuffer, doubleBuffer, b, i, d, s, a, u, sb, ss);

  //Load OpenGL extensions
  OpenGL_Extensions_Init();

  //Depth testing
  glEnable(GL_DEPTH_TEST);

  //Transparency
  glEnable(GL_BLEND);

  //Smooth shading
  glShadeModel(GL_SMOOTH);

  //Font textures (bitmap fonts)
  lucSetupRasterFont();

  //Enable scissor test
  glEnable(GL_SCISSOR_TEST);

  //Init fbo
  if (fbo_enabled) fbo(width, height);

  // Clear full window buffer
  glViewport(0, 0, width, height);
  glScissor(0, 0, width, height);
  glDrawBuffer(renderBuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  //Call the application open function
  app->open(width, height);

  //Flag window opened
  if (!isopen)
  {
    isopen = true;
    //Begin polling for input
    animate(TIMER_INC);
  }

  //Call open on any output interfaces
  for (unsigned int o=0; o<outputs.size(); o++)
    outputs[o]->open(width, height);
}

void OpenGLViewer::show()
{
  if (!isopen || !visible) return;

  //Show a previously hidden window
#ifdef GL_FRAMEBUFFER_EXT
  if (fbo_enabled)
  {
    debug_print("FBO disabled\n");
    fbo_enabled = false;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    //glBindTexture(GL_TEXTURE_2D, 0);
    if (doubleBuffer) renderBuffer = GL_BACK;
    else renderBuffer = GL_FRONT;
  }
#endif
}

// FBO buffers
void OpenGLViewer::fbo(int w, int h)
{
#ifdef GL_FRAMEBUFFER_EXT
  width = w;
  height = h;
  if (fbo_texture) glDeleteTextures(1, &fbo_texture);
  if (fbo_depth) glDeleteRenderbuffersEXT(1, &fbo_depth);
  if (fbo_frame) glDeleteFramebuffersEXT(1, &fbo_frame);
  GL_Error_Check;

  // create a texture to use as the backbuffer
  glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
  //glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &fbo_texture);
  glBindTexture(GL_TEXTURE_2D, fbo_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (downsample > 1)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }
  // make sure this is the same color format as the screen
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  // Depth buffer
  glGenRenderbuffersEXT(1, &fbo_depth);
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo_depth);
  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);
  //glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_STENCIL_INDEX, width, height);
  /*
  // initialize packed depth-stencil renderbuffer
  glGenRenderbuffersEXT(1, &fbo_depth);
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo_depth);
  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_STENCIL_EXT, width, height);
  */

  // Attach backbuffer texture, depth & stencil
  glGenFramebuffersEXT(1, &fbo_frame);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_frame);
  //Depth attachment
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo_depth);
  //Depth & Stencil attachments
  //glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo_depth);
  //glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo_depth);
  //Image buffer texture attachment
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbo_texture, 0);
  GL_Error_Check;

  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
  {
    debug_print("FBO setup failed\n");
    fbo_enabled = false;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
  }
  else
  {
    // Override buffers
    debug_print("FBO setup completed successfully %d x %d\n", width, height);
    fbo_enabled = true;
    renderBuffer = GL_COLOR_ATTACHMENT0_EXT;
    //Reset the viewer size
    resize(width, height);
  }
#else
  // Framebuffer objects not supported
  fbo_enabled = false;
  visible = true;   //Have to use visible window if available
#endif
  glBindTexture(GL_TEXTURE_2D, 0);
  GL_Error_Check;
}

void OpenGLViewer::setsize(int w, int h)
{
  //Resize fbo
  if (fbo_enabled) fbo(w, h);
}

void OpenGLViewer::resize(int new_width, int new_height)
{
  if (new_width > 0 && (width != new_width || height != new_height))
  {
    //Call the application resize function
    app->resize(new_width, new_height);

    width = new_width;
    height = new_height;
    debug_print("%d x %d resized \n", width, height);

    //Call resize on any output interfaces
    for (unsigned int o=0; o<outputs.size(); o++)
      outputs[o]->resize(width, height);
  }
}

void OpenGLViewer::close()
{
  // cleanup opengl memory - required before resize if context destroyed, then call open after resize
  DeleteFont();
  lucDeleteFont();
#ifdef GL_FRAMEBUFFER_EXT
  if (fbo_texture) glDeleteTextures(1, &fbo_texture);
  if (fbo_depth) glDeleteRenderbuffersEXT(1, &fbo_depth);
  if (fbo_frame) glDeleteFramebuffersEXT(1, &fbo_frame);
#endif
  //Call the application close function
  app->close();
  isopen = false;

  //Call close on any output interfaces
  for (unsigned int o=0; o<outputs.size(); o++)
    outputs[o]->close();
}

void OpenGLViewer::execute()
{
  //Default: fake event loop processing
  display();
  while (!quitProgram)
  {
    //New frame? call display
    if (postdisplay || OpenGLViewer::pollInput())
      display();
#ifdef _WIN32
    Sleep(TIMER_INC);
#else
    usleep(TIMER_INC * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
  }
}

// Render
void OpenGLViewer::display()
{
  postdisplay = false;
  //Parse stored commands first...
  bool idling = false;
  while (OpenGLViewer::commands.size() > 0)
  {
    //Critical section
    pthread_mutex_lock(&cmd_mutex);
    std::string cmd = OpenGLViewer::commands.front();
    OpenGLViewer::commands.pop_front();
    pthread_mutex_unlock(&cmd_mutex);

    //Idle posted?
    idling = cmd.find("idle") != std::string::npos;

    if (cmd == "\n")
    {
      //Call postdisplay and return
      postdisplay = true;
      return;
    }

    if (app->parseCommands(cmd))
    {
      //For what was this needed? test...
      //We're about to call display anyway regardless, if setting should probably return after
#ifdef USE_OMEGALIB
      //Is this needed for Omegalib mode param updates?
      postdisplay = true;
#else
      //This break causes server commands to back up and not all be processed in loop
      //However, animate "play" repeats forever without display if not enabled
      if (displayidle > 0)
        break;
#endif
    }
  }

  //Call the application display function
  app->display();

  //Call display (and idle) on any output interfaces
  for (unsigned int o=0; o<outputs.size(); o++)
  {
    if (idling) outputs[o]->idle();
    outputs[o]->display();
  }
}

void OpenGLViewer::pixels(void* buffer, bool alpha, bool flip)
{
  GLint type = alpha ? GL_RGBA : GL_RGB;

  //No row padding required
  glPixelStorei(GL_PACK_ALIGNMENT, 1);

#ifdef GL_FRAMEBUFFER_EXT
  if (fbo_enabled && fbo_frame > 0)
  {
    glBindTexture(GL_TEXTURE_2D, fbo_texture);
    glGenerateMipmapEXT(GL_TEXTURE_2D);
    glGetTexImage(GL_TEXTURE_2D, downsample-1, type, GL_UNSIGNED_BYTE, buffer);
    GL_Error_Check;
  }
  else
#endif
  {
    //Read pixels from the specified render buffer
    glReadBuffer(renderBuffer);
    glReadPixels(0, 0, width, height, type, GL_UNSIGNED_BYTE, buffer);
  }

  if (flip)
  {
    RawImageFlip(buffer, width, height, alpha ? 4 : 3);
  }
}

std::string OpenGLViewer::image(const std::string& path)
{
  bool alphapng = Properties::global("alphapng");
  int bpp = alphapng ? 4 : 3;
  int savewidth = width;
  int saveheight = height;
  std::string retImg;

  if (outwidth && !outheight)
  {
    //Calculate outheight based on ratio
    float ratio = height / (float)width;
    outheight = outwidth * ratio;
  }

  int w = outwidth ? outwidth : width;
  int h = outheight ? outheight : height;

  //Make sure any status message cleared
  display();

  //Redraw blended for output as transparent PNG
  if (alphapng)
    blend_mode = BLEND_PNG;

  //Re-render at specified output size (in a framebuffer object if available)
  float factor = pow(2, downsample-1);
  if (downsample > 1)
  {
    w *= factor;
    h *= factor;
  }

  //Resize necessary?
  if (w != width || h != height)
  {
#ifdef GL_FRAMEBUFFER_EXT
    //Switch to a framebuffer object
    if (fbo_frame > 0)
    {
      fbo_enabled = true;
      renderBuffer = GL_COLOR_ATTACHMENT0_EXT;
      //glBindTexture(GL_TEXTURE_2D, fbo_texture);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_frame);
    }

    setsize(w, h);
#else
    setsize(w, h);
#endif
  }

  display();

  if (downsample > 1)
  {
    //Saved image will be in original size
    w /= factor;
    h /= factor;
  }

  // Read the pixels
  size_t size = w * h * bpp;
  GLubyte *image = new GLubyte[size];
#ifdef HAVE_LIBPNG
  pixels(image, alphapng);
#else
  pixels(image, false, true);
#endif

  //Write PNG/JPEG to string or file
  if (path.length() == 0)
  {
    retImg = getImageString(image, w, h, bpp);
  }
  else
  {
    retImg = writeImage(image, w, h, path, alphapng);
  }

  delete[] image;

  //Restore settings
  blend_mode = BLEND_NORMAL;
  if (downsample > 1 || w != savewidth || h != saveheight)
  {
#ifdef GL_FRAMEBUFFER_EXT
    show();  //Disables fbo mode
    resize(savewidth, saveheight); //Resized callback
#else
    setsize(savewidth, saveheight);
#endif
  }

  return retImg;
}

void OpenGLViewer::idleReset()
{
  //Reset idle timer to zero if active
  if (idle > 0) idle = 0;
}

void OpenGLViewer::idleTimer(int display)
{
  //Start idle timer
  idle = 0;
  displayidle = display;
}

bool OpenGLViewer::pollInput()
{
  //Delete parsed commands from front of queue
  //while (OpenGLViewer::commands.front().length() == 0)
  // OpenGLViewer::commands.pop_front();
  pthread_mutex_lock(&cmd_mutex);

  //Check for input at stdin...
  bool parsed = false;

  //Poll any input interfaces until no more data
  std::string cmd;
  for (unsigned int i=0; i<inputs.size(); i++)
  {
    while (inputs[i]->read(cmd))
    {
      OpenGLViewer::commands.push_back(cmd);
      parsed = true;
    }
  }

  //Idle timer
  idle += TIMER_INC;
  if (displayidle > 0 && idle > displayidle)
  {
    OpenGLViewer::commands.push_back("idle");
    idle = 0;
    parsed = true;
  }

  pthread_mutex_unlock(&cmd_mutex);
  return parsed;
}

void OpenGLViewer::animate(int msec)
{
  timer = msec;
}


