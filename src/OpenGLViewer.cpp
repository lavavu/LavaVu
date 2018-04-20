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
ImageData* FrameBuffer::pixels(ImageData* image, int channels, bool flip)
{
  // Read the pixels
  assert(width && height);
  if (!image)
    image = new ImageData(width, height, channels);

  if (!target) target = GL_BACK;
  GLint type = (channels == 4 ? GL_RGBA : GL_RGB);

  //Read pixels from the specified render target
  assert(image->width == (unsigned int)width && image->height == (unsigned int)height && image->channels == (unsigned int)channels);
  glPixelStorei(GL_PACK_ALIGNMENT, 1); //No row padding required
  GL_Error_Check;
  glReadBuffer(target);
  GL_Error_Check;
  glReadPixels(0, 0, width, height, type, GL_UNSIGNED_BYTE, image->pixels);
  GL_Error_Check;
  if (flip)
    image->flip();
  GL_Error_Check;
  return image;
}

bool FBO::create(int w, int h)
{
#ifdef GL_FRAMEBUFFER_EXT
  //Re-render at specified output size (in a framebuffer object if available)
  if (downsample > 1)
  {
    float factor = downsampleFactor();
    w *= factor;
    h *= factor;
  }

  //Skip if already created at this size
  if (enabled && frame && texture && depth && width==w && height==h)
  {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frame);
    target = GL_COLOR_ATTACHMENT0_EXT;
    glDrawBuffer(target);
    GL_Error_Check;
    debug_print("FBO already exists, enabling %d x %d (downsampling %d)\n", width, height, downsample);
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

ImageData* FBO::pixels(ImageData* image, int channels, bool flip)
{
  if (!enabled || frame == 0 || downsample < 2)
    return FrameBuffer::pixels(image, channels, flip);

#ifdef GL_FRAMEBUFFER_EXT
  glPixelStorei(GL_PACK_ALIGNMENT, 1); //No row padding required
  //Output width
  float factor = 1.0/downsampleFactor();
  unsigned int w = getOutWidth();
  unsigned int h = getOutHeight();
  if (!image)
    image = new ImageData(w, h, channels);

  // Read the pixels from mipmap image
  //printf("(%d, %f) Bounds check %d x %d (%d) == %d x %d (%d)\n", downsample, factor, image->width, image->height, image->channels, w, h, channels);
  assert(image->width == w && image->height == h && image->channels == (unsigned int)channels);
  assert(w/factor == width && h/factor == height);
  assert(channels == 3 || channels == 4);
  GLint type = (channels == 4 ? GL_RGBA : GL_RGB);
  glBindTexture(GL_TEXTURE_2D, texture);
  glGenerateMipmapEXT(GL_TEXTURE_2D);

#ifdef DEBUG
  //Check size
  int outw, outh;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, downsample-1,  GL_TEXTURE_WIDTH, &outw);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, downsample-1,  GL_TEXTURE_HEIGHT, &outh);
  assert(w==(unsigned int)outw && h==(unsigned int)outh);
  debug_print("Get image %d : %d %d ==> %d %d\n", downsample-1, w, h, outw, outh);
#endif

  glGetTexImage(GL_TEXTURE_2D, downsample-1, type, GL_UNSIGNED_BYTE, image->pixels);
  GL_Error_Check;
  glBindTexture(GL_TEXTURE_2D, 0);

  if (flip)
    image->flip();
  GL_Error_Check;
#endif
  return image;
}

//OpenGLViewer class implementation...
OpenGLViewer::OpenGLViewer() : savewidth(0), saveheight(0), stereo(false), fullscreen(false), 
  postdisplay(false), quitProgram(false), isopen(false), 
  mouseState(0), button(NoButton), blend_mode(BLEND_NORMAL), outwidth(0), outheight(0), imagemode(false)
{
  app = NULL;
  keyState.shift = keyState.ctrl = keyState.alt = 0;

  visible = true;

  output_path = "";
}

OpenGLViewer::~OpenGLViewer()
{
  setTimer(0);
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
    setTimer(timer_msec);
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

void OpenGLViewer::hide()
{
  fbo.enabled = true; //Always use fbo for hidden window
  visible = false;
}

void OpenGLViewer::setsize(int w, int h)
{
  //Resize fbo
  display(false); //Ensure correct context active
  if (fbo.enabled && fbo.create(w, h))
    resize(fbo.width, fbo.height);  //Reset the viewer size
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
  //Default event processing, sleep for timer_msec milliseconds

  //New frame? call display
  if (postdisplay || pollInput())
    display();
#ifdef _WIN32
  Sleep(timer_msec);
#else
  usleep(timer_msec * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
}

void OpenGLViewer::loop(bool interactive)
{
  //Event loop processing
  if (visible)
    show();

  while (!quitProgram)
  {
    if (interactive)
      execute();
    else
      OpenGLViewer::execute();
  }

  if (visible) hide();
    if (interactive)
      execute();
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
    std::string cmd;
    {
      std::lock_guard<std::mutex> guard(cmd_mutex);
      cmd = commands.front();
      commands.pop_front();
    }

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
      if (!app->session.omegalib)
      {
        //This break causes server commands to back up and not all be processed in loop
        //However, animate "play" repeats forever without display if not enabled
        if (animate > 0)
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

void OpenGLViewer::outputON(int w, int h, int channels)
{
  //This function switches to the defined output resolution
  //and enables downsampling if possible
  //Used when capturing images and video frames
  //(only supported with hidden window => fbo output)
  assert(isopen);

  //Ensure correct GL context selected first
  //Do a full display or setsize will be called again
  //with the original display size before we output
  //Also ensures correct resolution set by setsize if changed
  display();

  //Enable image mode for further display calls
  imagemode = true;

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
  //debug_print("SWITCHING OUTPUT DIMS %d x %d TO %d x %d\n", width, height, w, h);

  //Redraw blended output for transparent PNG
  blend_mode = BLEND_NORMAL;
  if (channels == 4) blend_mode = BLEND_PNG;

  //Enable FBO for image output
  if (visible && (w != width || h != height))
    fbo.enabled = true;

  //Activate fbo if enabled
  if (fbo.enabled)
    fbo.create(w, h);

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
    //Use the fbo size for output
    width = fbo.width;
    height = fbo.height;

    //Scale text and 2d elements when downsampling output image
    app->session.scale2d = fbo.downsampleFactor();
  }

  //Re-render frame first
  display();
}

void OpenGLViewer::outputOFF()
{
  //Restore normal viewing dims when output mode is finished
  imagemode = false;
  if (visible)
  {
    fbo.disable();
    //Undo 2d scaling for downsampling
    app->session.scale2d = 1.0;
    //Restore settings
    blend_mode = BLEND_NORMAL;
  }

  //Restore size
  width = savewidth;
  height = saveheight;

  savewidth = saveheight = 0;
}

ImageData* OpenGLViewer::pixels(ImageData* image, int channels, bool flip)
{
  assert(isopen);
  if (fbo.enabled)
    return fbo.pixels(image, channels, flip);
  else
    return FrameBuffer::pixels(image, channels, flip);
}

ImageData* OpenGLViewer::pixels(ImageData* image, int w, int h, int channels, bool flip)
{
  assert(isopen);

  outputON(w, h, channels);
  image = pixels(image, channels, flip);
  outputOFF();

  return image;
}

std::string OpenGLViewer::image(const std::string& path, int jpegquality, bool transparent, int w, int h)
{
  assert(isopen);
  FilePath filepath(path);
  if ((filepath.type == "jpeg" || filepath.type == "jpg") && jpegquality == 0) jpegquality = 95;
  bool alphapng = jpegquality == 0 && (transparent || app->session.global("pngalpha"));
  int channels = 3;
  if (alphapng) channels = 4;
  std::string retImg;

  // Read the pixels
  ImageData* image = pixels(NULL, w ? w : outwidth, h ? h : outheight, channels, false);

  //Write PNG/JPEG to string or file
  if (path.length() == 0)
    retImg = getImageUrlString(image, jpegquality);
  else
    retImg = image->write(path);

  if (image) delete image;

  return retImg;
}

void OpenGLViewer::downSample(int q)
{
  display(false);
  int ds = q < 1 ? 1 : q;
  if (ds != fbo.downsample)
  {
    fbo.destroy();
    fbo.downsample = ds;
  }
}

void OpenGLViewer::animateTimer(int msec)
{
  //Start animation timer
  elapsed = idle = 0;
  if (msec < 0)
    msec = timer_animate;
  else if (msec > 0)
    timer_animate = msec;

  animate = msec;
}

bool OpenGLViewer::pollInput()
{
  //Delete parsed commands from front of queue
  //while (commands.front().length() == 0)
  // commands.pop_front();
  std::lock_guard<std::mutex> guard(cmd_mutex);

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

  //Animate timer
  elapsed += timer_msec;
  idle += timer_msec;
  if (animate > 0 && elapsed > animate)
  {
    commands.push_back("idle");
    elapsed = 0;
    parsed = true;
    if (timeloop)
      commands.push_back(std::string("next auto"));
  }

  return parsed;
}

void OpenGLViewer::setTimer(int msec)
{
  timer = msec;
}


