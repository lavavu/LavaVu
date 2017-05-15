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

// FBO buffers
GLubyte* FrameBuffer::pixels(GLubyte* image, int channels, bool flip)
{
  // Read the pixels
  assert(width && height);
  //TODO: store as TextureData so can check width/height
  if (!image)
    image = new GLubyte[width * height * channels];

  if (!target) target = GL_BACK;
  GLint type = (channels == 4 ? GL_RGBA : GL_RGB);

  //Read pixels from the specified render target
  glPixelStorei(GL_PACK_ALIGNMENT, 1); //No row padding required
  GL_Error_Check;
  glReadBuffer(target);
  GL_Error_Check;
  glReadPixels(0, 0, width, height, type, GL_UNSIGNED_BYTE, image);
  GL_Error_Check;
  if (flip)
    RawImageFlip(image, width, height, channels);
  GL_Error_Check;
  return image;
}

bool FBO::create(int w, int h)
{
#ifdef GL_FRAMEBUFFER_EXT
  //Re-render at specified output size (in a framebuffer object if available)
  if (downsample > 1)
  {
    float factor = pow(2, downsample-1);
    w *= factor;
    h *= factor;
  }

  //Skip if already created at this size
  if (enabled && frame && texture && depth && width==w && height==h)
  {
    target = GL_COLOR_ATTACHMENT0_EXT;
    glDrawBuffer(target);
    GL_Error_Check;
    return false;
  }

  width = w;
  height = h;
  destroy();

  // create a texture to use as the backbuffer
  glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
  //glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (downsample > 1)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  // make sure this is the same color format as the screen
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  // Depth buffer
  glGenRenderbuffersEXT(1, &depth);
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth);
  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);

  // Attach backbuffer texture, depth & stencil
  glGenFramebuffersEXT(1, &frame);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frame);
  //Depth attachment
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth);
  //Image buffer texture attachment
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);
  GL_Error_Check;

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
  {
    if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT)
      std::cerr << "FBO failed INCOMPLETE_ATTACHMENT" << std::endl;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT)
      std::cerr << "FBO failed INCOMPLETE_DIMENSIONS" << std::endl;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT)
      std::cerr << "FBO failed MISSING_ATTACHMENT" << std::endl;
    else if (status == GL_FRAMEBUFFER_UNSUPPORTED_EXT)
      std::cerr << "FBO failed UNSUPPORTED" << std::endl;
    else
      std::cerr << "FBO failed UNKNOWN ERROR: " << status << std::endl;
    enabled = false;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
  }
  else
  {
    // Override buffers
    debug_print("FBO setup completed successfully %d x %d (downsampling %d)\n", width, height, downsample);
    enabled = true;
    target = GL_COLOR_ATTACHMENT0_EXT;
    glDrawBuffer(target);
  }
#else
  // Framebuffer objects not supported
  enabled = false;
#endif
  glBindTexture(GL_TEXTURE_2D, 0);
  GL_Error_Check;
  return enabled;
}

void FBO::destroy()
{
#ifdef GL_FRAMEBUFFER_EXT
  if (texture) glDeleteTextures(1, &texture);
  if (depth) glDeleteRenderbuffersEXT(1, &depth);
  if (frame) glDeleteFramebuffersEXT(1, &frame);
  texture = depth = frame = 0;
#endif
}

void FBO::disable()
{
//Show a previously hidden window
#ifdef GL_FRAMEBUFFER_EXT
  debug_print("FBO disabled\n");
  enabled = false;
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
  glDrawBuffer(GL_BACK);
#endif
}

GLubyte* FBO::pixels(GLubyte* image, int channels, bool flip)
{
  if (!enabled || frame == 0 || downsample < 2)
    return FrameBuffer::pixels(image, channels, flip);

#ifdef GL_FRAMEBUFFER_EXT
  //Output width
  float factor = 1.0/pow(2, downsample-1);
  unsigned int w = width*factor;
  unsigned int h = height*factor;
  //TODO: store as TextureData so can check width/height
  if (!image)
    image = new GLubyte[w * h * channels];

  // Read the pixels from mipmap image
  GLint type = (channels == 4 ? GL_RGBA : GL_RGB);
  glBindTexture(GL_TEXTURE_2D, texture);
  glGenerateMipmapEXT(GL_TEXTURE_2D);
  glGetTexImage(GL_TEXTURE_2D, downsample-1, type, GL_UNSIGNED_BYTE, image);
  GL_Error_Check;
  glBindTexture(GL_TEXTURE_2D, 0);

  if (flip)
    RawImageFlip(image, w, h, channels);
  GL_Error_Check;
#endif
  return image;
}

//OpenGLViewer class implementation...
OpenGLViewer::OpenGLViewer() : savewidth(0), saveheight(0), stereo(false), fullscreen(false), postdisplay(false), quitProgram(false), isopen(false), mouseState(0), button(NoButton), blend_mode(BLEND_NORMAL), outwidth(0), outheight(0)
{
  app = NULL;
  keyState.shift = keyState.ctrl = keyState.alt = 0;
  idle = displayidle = 0;

  timer = 0;
  visible = true;

  title = "LavaVu";
  output_path = "";

  /* Init mutex */
  pthread_mutex_init(&cmd_mutex, NULL);
}

OpenGLViewer::~OpenGLViewer()
{
  animate(0);
}

void OpenGLViewer::open(int w, int h)
{
  //Open window, called before window manager open
  //Set width and height

  fbo.enabled = !visible; //Always use fbo for hidden window

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

  debug_print("Stereo %d Double-buffer %d RGBA Mode = %d, Alpha bits = %d, Depth bits = %d, Stencil bits = %d, Accum bits = %d, Texture units %d, SampleBuffers %d, Samples %d\n", stereoBuffer, doubleBuffer, b, i, d, s, a, u, sb, ss);

  //Load OpenGL extensions
  OpenGL_Extensions_Init();

  //Depth testing
  glEnable(GL_DEPTH_TEST);

  //Transparency
  glEnable(GL_BLEND);

  //Smooth shading
  glShadeModel(GL_SMOOTH);

  //Enable scissor test
  glEnable(GL_SCISSOR_TEST);

  //Create and use fbo at this point if enabled
  if (fbo.enabled)
    fbo.create(width, height);

  // Clear full window buffer
  glViewport(0, 0, width, height);
  glScissor(0, 0, width, height);
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
  if (fbo.enabled) fbo.disable();
}

void OpenGLViewer::setsize(int w, int h)
{
  //Resize fbo
  display(false); //Ensure correct context active
  if (fbo.enabled && fbo.create(w, h))
    //Reset the viewer size
    resize(w, h);
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
  fbo.destroy();

  //Call the application close function
  app->close();
  isopen = false;

  //Call close on any output interfaces
  for (unsigned int o=0; o<outputs.size(); o++)
    outputs[o]->close();
}

void OpenGLViewer::execute()
{
  //Default event processing, sleep for TIMER_INC microseconds

  //New frame? call display
  if (postdisplay || pollInput())
    display();
#ifdef _WIN32
  Sleep(TIMER_INC);
#else
  usleep(TIMER_INC * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
}

void OpenGLViewer::loop(bool interactive)
{
  //Event loop processing
  while (!quitProgram)
  {
    if (interactive)
      execute();
    else
      OpenGLViewer::execute();
  }
}

// Render
void OpenGLViewer::display(bool redraw)
{
  if (!redraw) return;

  postdisplay = false;
  //Parse stored commands first...
  bool idling = false;
  while (commands.size() > 0)
  {
    //Critical section
    pthread_mutex_lock(&cmd_mutex);
    std::string cmd = commands.front();
    commands.pop_front();
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
      if (app->drawstate.omegalib)
      {
        //Is this needed for Omegalib mode param updates?
        postdisplay = true;
      }
      else
      {
        //This break causes server commands to back up and not all be processed in loop
        //However, animate "play" repeats forever without display if not enabled
        if (displayidle > 0)
          break;
      }
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

void OpenGLViewer::outputON(int& w, int& h, int channels)
{
  //This function switches to the defined output resolution
  //and enables downsampling if possible
  //Used when capturing images and video frames
  //(only supported with hidden window => fbo output)
  assert(isopen);
  //Save current viewer size
  savewidth = width;
  saveheight = height;

  if (w && !h)
  {
    //Calculate output height based on ratio
    float ratio = height / (float)width;
    h = w * ratio;
  }

  //Use out dims if defined
  if (!w) w = width;
  if (!h) h = height;
  assert(w && h);
  //debug_printf("SWITCHING OUTPUT DIMS %d x %d TO %d x %d\n", width, height, w, h);

  //Redraw blended output for transparent PNG
  if (channels == 4) blend_mode = BLEND_PNG;

  //Ensure correct GL context selected first
  //display(false);
  //Do a full display or setsize will be called again
  //with the original display size before we output
  display();

  if (!fbo.enabled)
  {
    //outwidth/outheight only support if fbo available
    if (w != width || h != height)
      std::cerr << "FBO Support required to save image at different size to window\n";
    w = width;
    h = height;
  }
  else
  {
    //Activate fbo if enabled
    fbo.create(w, h);

    //Use the fbo size for output
    width = fbo.width;
    height = fbo.height;

    //Re-render to fbo first
    display();
  }
}

void OpenGLViewer::outputOFF()
{
  //Restore normal viewing dims when output mode is finished
  width = savewidth;
  height = saveheight;
  //Restore settings
  blend_mode = BLEND_NORMAL;
  savewidth = saveheight = 0;
}

GLubyte* OpenGLViewer::pixels(GLubyte* image, int channels, bool flip)
{
  assert(isopen);
  if (fbo.enabled)
    return fbo.pixels(image, channels, flip);
  else
    return FrameBuffer::pixels(image, channels, flip);
}

GLubyte* OpenGLViewer::pixels(GLubyte* image, int& w, int& h, int channels, bool flip)
{
  assert(isopen);

  outputON(w, h, channels);
  image = pixels(image, channels, flip);
  outputOFF();

  return image;
}

std::string OpenGLViewer::image(const std::string& path, int jpegquality, bool transparent)
{
  assert(isopen);
  FilePath filepath(path);
  if (filepath.type == "jpeg" || filepath.type == "jpg") jpegquality = 95;
  bool alphapng = !jpegquality && (transparent || app->drawstate.global("pngalpha"));
  int channels = 3;
  if (alphapng) channels = 4;
  std::string retImg;

  // Read the pixels
  // TODO: store in TextureData so can check width/height
  GLubyte* image = pixels(NULL, outwidth, outheight, channels, false);

  //Write PNG/JPEG to string or file
  if (path.length() == 0)
    retImg = getImageUrlString(image, outwidth, outheight, channels, jpegquality);
  else
    retImg = writeImage(image, outwidth, outheight, path, channels);

  delete[] image;

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
  //while (commands.front().length() == 0)
  // commands.pop_front();
  pthread_mutex_lock(&cmd_mutex);

  //Check for input at stdin...
  bool parsed = false;

  //Poll any input interfaces until no more data
  std::string cmd;
  for (unsigned int i=0; i<inputs.size(); i++)
  {
    while (inputs[i]->get(cmd))
    {
      commands.push_back(cmd);
      parsed = true;
    }
  }

  //Idle timer
  idle += TIMER_INC;
  if (displayidle > 0 && idle > displayidle)
  {
    commands.push_back("idle");
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


