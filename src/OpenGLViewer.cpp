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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

std::thread::id no_thread;

// FBO buffers
ImageData* FrameBuffer::pixels(ImageData* image, int channels)
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
  //OpenGL buffer sourced images are always flipped in the Y axis
  image->flipped = true;
  GL_Error_Check;
  return image;
}

FBO::~FBO()
{
  destroy();
}

bool FBO::create(int w, int h, int samples)
{
  //Re-render at specified output size (in a framebuffer object if available)
  int mt = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mt);
  if (downsample > 1)
  {
    float factor = downsampleFactor();
    //Check texture size
    if (w*factor > mt || h*factor > mt)
    {
      //Reduce downsampling
      downsample--;
      std::cerr << "Max texture size is " << mt << " : FBO too large at " << (w*factor) << "x" << (h*factor) << ", reducing downsample to " << downsample << std::endl;
      return create(w, h, samples);
    }
    w *= factor;
    h *= factor;
  }

  //Skip if already created at this size
  if (enabled && frame && texture && depth && width==w && height==h && samples==msaa)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, frame);
    GL_Error_Check;
    target = GL_COLOR_ATTACHMENT0;
#ifndef GLES2
    glDrawBuffer(target);
#endif
    GL_Error_Check;
    debug_print("FBO already exists, enabling %d x %d (downsampling %d)\n", width, height, downsample);
    return false;
  }
  GL_Error_Check;

  width = w;
  height = h;
  msaa = samples;
  destroy();

  // create a texture to use as the backbuffer
  GL_Error_Check;
  //glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &texture);
  if (samples > 1)
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture);
  else
    glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  GL_Error_Check;
  if (downsample > 1)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  GL_Error_Check;

  // make sure this is the same color format as the screen
#ifndef GLES2
  if (samples > 1)
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA, width, height, GL_TRUE);
  else
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  GL_Error_Check;

  // Depth buffer
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  if (samples > 1)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

  if (samples > 1)
  {
    // Colour buffer
    glGenRenderbuffers(1, &rgba);
    glBindRenderbuffer(GL_RENDERBUFFER, rgba);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
  }
  //else
  //  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);

  // Attach backbuffer texture, depth & rgba
  glGenFramebuffers(1, &frame);
  glBindFramebuffer(GL_FRAMEBUFFER, frame);

  //Image buffer texture attachment
  if (samples > 1)
  {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, texture, 0);
    //Colour attachment
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rgba);
  }
  else
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  GL_Error_Check;
  GL_Error_Check;
  //Depth attachment
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
  GL_Error_Check;

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
      std::cerr << "FBO failed INCOMPLETE_ATTACHMENT" << std::endl;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
      std::cerr << "FBO failed INCOMPLETE_DRAW_BUFFER" << std::endl;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
      std::cerr << "FBO failed MISSING_ATTACHMENT" << std::endl;
    else if (status == GL_FRAMEBUFFER_UNSUPPORTED)
      std::cerr << "FBO failed UNSUPPORTED" << std::endl;
    else if (status == GL_FRAMEBUFFER_UNDEFINED)
      std::cerr << "FBO failed UNDEFINED" << std::endl;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
      std::cerr << "FBO failed INCOMPLETE_READ_BUFFER" << std::endl;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
      std::cerr << "FBO failed INCOMPLETE_MULTISAMPLE" << std::endl;
    else
      std::cerr << "FBO failed UNKNOWN ERROR: " << status << std::endl;
    enabled = false;
    GL_Error_Check;
    std::cerr << " frame " << frame << " depth " << depth << " dims " << width << " , " << height << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    abort_program("FBO creation failed, can't continue");
  }
  else
  {
    // Override buffers
    debug_print("FBO setup completed successfully %d x %d (downsampling %d)\n", width, height, downsample);
    enabled = true;
    target = GL_COLOR_ATTACHMENT0;
#ifndef GLES2
    glDrawBuffer(target);
#endif
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
  GL_Error_Check;
  return enabled;
}

void FBO::destroy()
{
  if (texture) glDeleteTextures(1, &texture);
  if (depth) glDeleteRenderbuffers(1, &depth);
  if (rgba) glDeleteRenderbuffers(1, &rgba);
  if (frame) glDeleteFramebuffers(1, &frame);
  texture = depth = frame = rgba = 0;
}

void FBO::disable()
{
//Show a previously hidden window
  debug_print("FBO disabled\n");
  enabled = false;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
#ifndef GLES2
  glDrawBuffer(GL_BACK);
#endif
}

ImageData* FBO::pixels(ImageData* image, int channels)
{
  if (!enabled || frame == 0 || downsample < 2)
    return FrameBuffer::pixels(image, channels);

  glPixelStorei(GL_PACK_ALIGNMENT, 1); //No row padding required
  //Output width
  unsigned int w = getOutWidth();
  unsigned int h = getOutHeight();
  if (!image)
    image = new ImageData(w, h, channels);

  //printf("(%d, %f) Bounds check %d x %d (%d) == %d x %d (%d)\n", downsample, factor, image->width, image->height, image->channels, w, h, channels);
  glBindTexture(GL_TEXTURE_2D, texture);
  glGenerateMipmap(GL_TEXTURE_2D);
#if defined(DEBUG) && !defined(__EMSCRIPTEN__)
  //Check size
  int outw, outh;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, downsample-1,  GL_TEXTURE_WIDTH, &outw);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, downsample-1,  GL_TEXTURE_HEIGHT, &outh);
  assert(w==(unsigned int)outw && h==(unsigned int)outh);
  debug_print("Get image %d : %d %d ==> %d %d (buffer %d,%d)\n", downsample-1, w, h, outw, outh, width, height);
  assert(image->width == w && image->height == h && image->channels == (unsigned int)channels);
  assert(width/downsampleFactor() == w);
  assert(height/downsampleFactor() == h);
  assert(channels == 3 || channels == 4);
#endif

  // Read the pixels from mipmap image
#ifndef GLES2
  GLint type = (channels == 4 ? GL_RGBA : GL_RGB);
  //printf("DOWNSAMPLE GET %d %dx%d (%dx%d) samples %d\n", channels, w, h, width, height, downsample);
  glGetTexImage(GL_TEXTURE_2D, downsample-1, type, GL_UNSIGNED_BYTE, image->pixels);
#else
  //Need to read the texture from the framebuffer at original size, then downsample ourselves
  ImageData* fullpixels = FrameBuffer::pixels(NULL, channels);
  //Default downsample filter is STBIR_FILTER_MITCHELL
  stbir_resize_uint8(fullpixels->pixels, fullpixels->width, fullpixels->height, 0,
                     image->pixels, image->width, image->height, 0, channels);
  delete fullpixels;
#endif
  GL_Error_Check;
  glBindTexture(GL_TEXTURE_2D, 0);

  //OpenGL buffer sourced images are always flipped in the Y axis
  image->flipped = true;
  GL_Error_Check;
  return image;
}

//OpenGLViewer class implementation...
OpenGLViewer::OpenGLViewer()
{
  app = NULL;
}

OpenGLViewer::~OpenGLViewer()
{
  setTimer(0);
  // cleanup opengl memory - required before resize if context destroyed, then call open after resize
  GL_Check_Thread(render_thread);
  fbo.destroy();
  fbo_blit.destroy();

  //Call the application close function
  app->close();
  isopen = false;
}

void OpenGLViewer::open(int w, int h)
{
  render_thread = std::this_thread::get_id();
  //std::cerr << "Render thread: " << render_thread << std::endl;

  //Open window, called before window manager open
  //Set width and height

  fbo.enabled = !visible; //Always use fbo for hidden window

  //Set width/height or use defaults
  width = w > 0 ? w : 1024;
  height = h > 0 ? h : 768;
  debug_print("Window opened %d x %d\n", width, height);

  //If context provided, call init ourselves
  if (app->session.havecontext)
    init();
}

void OpenGLViewer::init()
{
  GL_Check_Thread(render_thread);
  GL_Error_Check;
  //Init OpenGL (called after context creation)
  glGetIntegerv(GL_SAMPLE_BUFFERS, &sb);
  glGetIntegerv(GL_SAMPLES, &ss);
#ifndef __EMSCRIPTEN__
  glGetBooleanv(GL_STEREO, &stereoBuffer);
  glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffer);
#endif
  app->session.context.antialiased = ss > 1;

  const char* gl_v = (const char*)glGetString(GL_VERSION);
  
  glGetIntegerv(GL_MAJOR_VERSION, &app->session.context.major);
  glGetIntegerv(GL_MINOR_VERSION, &app->session.context.minor);
  GLint profile;
#ifdef __EMSCRIPTEN__
  profile = GL_CONTEXT_CORE_PROFILE_BIT;
#else
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
#endif
  app->session.context.core = profile & GL_CONTEXT_CORE_PROFILE_BIT;
  app->session.context.gl_version = std::string(gl_v);
  debug_print("OpenGL %d.%d (%s)\n", app->session.context.major, app->session.context.minor, app->session.context.core ? "core" : "compatibility");
  debug_print("%s Stereo %d Double-buffer %d, SampleBuffers %d, Samples %d\n", gl_v, stereoBuffer, doubleBuffer, sb, ss);
  GL_Error_Check;

  //Load OpenGL extensions
  OpenGL_Extensions_Init();
  GL_Error_Check;

  //Depth testing
  glEnable(GL_DEPTH_TEST);

  //Transparency
  glEnable(GL_BLEND);

  //Enable scissor test
  glEnable(GL_SCISSOR_TEST);
  GL_Error_Check;

  //Create and use fbo at this point if enabled
  useFBO();

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

bool OpenGLViewer::useFBO(int w, int h)
{
  //Create and use fbo at this point if enabled
  if (!fbo.enabled)
    return false;
  if (w == 0) w = width;
  if (h == 0) h = height;
  assert(w && h);
  bool status = false;
  status = fbo_blit.create(w, h);
  status = fbo.create(w, h, app->session.context.samples);
  //Anti-aliasing enabled in FBO?
  app->session.context.antialiased = fbo.downsample > 1 || fbo.msaa > 1;
  return status;
}

void OpenGLViewer::show()
{
  if (!isopen) return;

  //Show a previously hidden window
  disableFBO();
  visible = true;
}

void OpenGLViewer::hide()
{
  fbo.enabled = true; //Always use fbo for hidden window
  visible = false;
}

void OpenGLViewer::setsize(int w, int h)
{
  assert(w && h);
  GL_Check_Thread(render_thread);
  //Resize fbo
  display(false); //Ensure correct context active
  if (useFBO(w, h) && fbo.downsample > 1)
  {
    //Must set app size and attached outputs,
    //BUT NOT CHANGE viewer->width/height
    //Call the application resize function
    app->resize(fbo.width, fbo.height);
  }

  //Actual dimensions changed, full resize
  if (w != width || h != height)
    resize(w, h);

  //Ensure the res property matches viewer dims after resize (not fbo dims)
  app->session.globals["resolution"] = json::array({width, height});
}

void OpenGLViewer::resize(int new_width, int new_height)
{
  assert(new_width && new_height);
  if (new_width > 0 && (width != new_width || height != new_height))
  {
    //Call the application resize function
    app->resize(new_width, new_height);

    width = new_width;
    height = new_height;
    debug_print("%d x %d resized \n", width, height);
  }
}

void OpenGLViewer::resizeOutputs(int w, int h)
{
  //Call resize on any output interfaces
  for (unsigned int o=0; o<outputs.size(); o++)
    outputs[o]->resize(w, h);
}

void OpenGLViewer::execute()
{
  //Default event processing, sleep for timer_msec milliseconds

  //New frame? call display
  if (events())
    display();
#ifdef _WIN32
  Sleep(timer_msec);
#else
  usleep(timer_msec * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
}

bool OpenGLViewer::events()
{
  //Return true if events require processing
  return postdisplay || pollInput();
}

#ifdef __EMSCRIPTEN__
EM_JS(const char*, get_commands, (), {
  if (window.commands && window.commands.length) {
    //var cmd = window.commands.shift(); //Remove first element
    //Merge multiple commands so they are all processed
    var cmd = window.commands.join(';'); //Concatenate
    window.commands = [];
    //console.log("GETTING COMMANDS: " + window.commands);
    // 'str.length' would return the length of the string as UTF-16
    // units, but Emscripten C strings operate as UTF-8.
    var lengthBytes = lengthBytesUTF8(cmd)+1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(cmd, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
  } else {
    return null;
  }
});

//Called from the Emscripten animation loop
void em_main_callback(void* userData)
{
  if (userData)
  {
    OpenGLViewer* v = (OpenGLViewer*)userData;
    v->execute();

    //Get any waiting commands
    const char* str = get_commands();
    if (str)
    {
      if (strlen(str))
      {
        //printf("UTF8 string retrieved: %s\n", str);
        //v->commands.push_back(str);
        if (v->app->parseCommands(str))
          v->postdisplay = true;
      }
      // Each call to _malloc() must be paired with free(), or heap memory will leak!
      free((void*)str);
    }

    //Idle sort - flag rotated after 150us
    if (v->mouseState != 0 && v->idle > 150)
      v->postdisplay = true;

    //Check the reload flag in JS
    if (EM_ASM_INT({return window.reload_flag;}))
    {
      v->app->fetch("getstate", true);
      EM_ASM({window.reload_flag = false;});
    }

    //Check the resize flag in JS
    if (EM_ASM_INT({return window.resized;}))
    {
      //Set size to match canvas
      double w, h;
      emscripten_get_element_css_size("canvas", &w, &h);
      v->app->session.globals["resolution"] = json::array({w, h});
      v->postdisplay = true;

      EM_ASM({window.resized = false;});
    }
  }
}
#endif

void OpenGLViewer::loop(bool interactive)
{
  GL_Check_Thread(render_thread);
  //Event loop processing
  if (visible)
    show();

#ifdef __EMSCRIPTEN__
  // Receives a function to call and some user data to provide it.
  emscripten_set_main_loop_arg(&em_main_callback, (void*)this, 0, true);
#else
  while (!quitProgram)
  {
    if (interactive)
      execute();
    else
      OpenGLViewer::execute();
  }
#endif

  if (visible)
    hide();

  if (interactive)
    execute();
}

// Render
void OpenGLViewer::display(bool redraw)
{
  GL_Check_Thread(render_thread);
  if (!redraw) return;

  postdisplay = false;
  //Parse stored commands first...
  while (commands.size() > 0)
  {
    //Critical section
    std::string cmd;
    {
      LOCK_GUARD(cmd_mutex);
      cmd = commands.front();
      commands.pop_front();
    }

    if (cmd == "\n")
    {
      //Call postdisplay and return
      postdisplay = true;
      return;
    }

    if (app->parseCommands(cmd))
    {
      //This break causes server commands to back up and not all be processed in loop
      //However, animate "play" repeats forever without display if not enabled
      if (animate > 0)
        break;
    }
  }

  //Call the application display function
  app->display();

  //Call display on any output interfaces
  for (unsigned int o=0; o<outputs.size(); o++)
  {
    //printf("OUTPUT %d/%d render == %d\n", o+1, (int)outputs.size(), outputs[o]->render);
    if (outputs[o]->render)
    {
      //Create the frame buffer if not yet created
      outputs[o]->alloc(width, height);

      //Read the pixels
      //if (outputs[o]->width && outputs[o]->height && (outputs[o]->width != width || outputs[o]->height != height))
        pixels(outputs[o]->buffer, outputs[o]->width, outputs[o]->height, outputs[o]->channels);
      //else
      //Was this for speed in server mode? Fails for video output so disabled for now
      //  pixels(outputs[o]->buffer, outputs[o]->channels);

      //Process in output display callback
      outputs[o]->display();
    }
  }
}

void OpenGLViewer::outputON(int w, int h, int channels, bool vid)
{
  GL_Check_Thread(render_thread);
  //This function switches to the defined output resolution
  //and enables downsampling if possible
  //Used when capturing images and video frames
  //(only supported with hidden window => fbo output)
  assert(isopen);

  //Ensure correct GL context selected first
  display(false);

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

  //Ensure multiple of 2 for video recording
  if (vid)
  {
    if (h > 0 && h % 2 != 0) h -= 1;
    if (w > 0 && w % 2 != 0) w -= 1;
  }

  //debug_print("SWITCHING OUTPUT DIMS %d x %d TO %d x %d\n", width, height, w, h);

  //Redraw blended output for transparent PNG
  auto prev_blend_mode = blend_mode;
  if (blend_mode == BLEND_NONE)
  {
    blend_mode = BLEND_NORMAL;
    if (channels == 4) blend_mode = BLEND_PNG;
  }

  //Enable FBO for image output even while visible if dimensions don't match
  if (visible && (fbo.downsample > 1 || w != width || h != height))
    fbo.enabled = true;

  //Activate fbo if enabled
  useFBO(w, h);

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
    app->session.context.scale2d = fbo.downsampleFactor();
    glViewport(0, 0, width, height);
  }

  //Re-render frame first
  app->display();

  //Restore blend mode if modified
  blend_mode = prev_blend_mode;
}

void OpenGLViewer::outputOFF()
{
  GL_Check_Thread(render_thread);
  //Restore normal viewing dims when output mode is finished
  if (!imagemode)
    return;

  imagemode = false;
  if (visible)
  {
    disableFBO();
    //Restore settings
    blend_mode = BLEND_NONE;
  }

  //Restore size
  if (savewidth && saveheight)
  {
    width = savewidth;
    height = saveheight;
    app->session.context.scale2d = 1.0;
    glViewport(0, 0, width, height);
  }

  savewidth = saveheight = 0;
}

void OpenGLViewer::disableFBO()
{
  GL_Check_Thread(render_thread);
  if (fbo.enabled)
    fbo.disable();
  //Undo 2d scaling for downsampling
  app->session.context.scale2d = 1.0;
  //Anti-aliasing enabled in default framebuffer?
  app->session.context.antialiased = ss > 1;
}

ImageData* OpenGLViewer::pixels(ImageData* image, int channels)
{
  GL_Check_Thread(render_thread);
  assert(isopen);
  if (app->session.context.samples > 1 && fbo.enabled)
  {
    assert(fbo.downsample < 2);
    //printf("MULTISAMPLE GET %d %dx%d samples %d\n", channels, width, height, app->session.context.samples);
    assert(width && height);
    if (!image)
      image = new ImageData(width, height, channels);

    //After rendering scene, blit multisample fbo to normal fbo and read the pixels
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.frame);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_blit.frame);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_blit.frame);

    //Read pixels from the specified render target
    assert(image->width == (unsigned int)width && image->height == (unsigned int)height && image->channels == (unsigned int)channels);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); //No row padding required
    GLint type = (channels == 4 ? GL_RGBA : GL_RGB);
    glReadPixels(0, 0, width, height, type, GL_UNSIGNED_BYTE, image->pixels);

    //OpenGL buffer sourced images are always flipped in the Y axis
    image->flipped = true;
    GL_Error_Check;
    return image;
  }
  else if (fbo.enabled)
    return fbo.pixels(image, channels);
  else
    return FrameBuffer::pixels(image, channels);
}

ImageData* OpenGLViewer::pixels(ImageData* image, int w, int h, int channels)
{
  GL_Check_Thread(render_thread);
  assert(isopen);

  if (!w) w = outwidth;
  if (!h) h = outheight;
  if (!channels) channels = 3;

  outputON(w, h, channels);
  image = pixels(image, channels);
  outputOFF();

  return image;
}

std::string OpenGLViewer::image(const std::string& path, int jpegquality, bool transparent, int w, int h)
{
  GL_Check_Thread(render_thread);
  assert(isopen);
  FilePath filepath(path);
  if ((filepath.type == "jpeg" || filepath.type == "jpg"))
  {
    //Set default quality
    if (jpegquality == 0)
      jpegquality = 95;
  }
  else
  {
    //PNG, clear quality
    jpegquality = 0;
  }

  bool alphapng = jpegquality == 0 && (transparent || app->session.global("pngalpha"));
  int channels = 3;
  if (alphapng) channels = 4;
#ifdef __EMSCRIPTEN__
  channels = 4; //WebGL buffer doesn't support glReadPixels with GL_RGB
#endif
  std::string retImg;

  // Read the pixels
  ImageData* image = pixels(NULL, w, h, channels);

  //Write PNG/JPEG to string or file
  if (path.length() == 0)
  {
    retImg = image->getURIString(jpegquality);
  }
  else
  {
    std::string outpath = path;
    if (output_path.length())
      outpath = output_path + "/" + filepath.file;
    retImg = image->write(outpath, jpegquality);
#ifdef __EMSCRIPTEN__
    std::string type =  jpegquality == 0 ? "image/png" : "image/jpeg";
    EM_ASM({ window.download($0, $1) }, retImg.c_str(), type.c_str());
#endif
  }

  if (image) delete image;

  return retImg;
}

void OpenGLViewer::downSample(int q)
{
  GL_Check_Thread(render_thread);
  display(false);

  fbo.destroy();

  //Disable MSAA when using FSAA (or in GLES2)
#ifndef GLES2
  if (q > 1 && fbo.msaa > 1)
#endif
    app->session.context.samples = 1;

  int ds = q < 1 ? 1 : q;
  if (ds != fbo.downsample)
    fbo.downsample = ds;
}

void OpenGLViewer::multiSample(int q)
{
  GL_Check_Thread(render_thread);
  display(false);
  fbo.destroy();

  //Disable FSAA when using MSAA
  if (q > 1 && fbo.downsample > 1)
    fbo.downsample = 1;

  int ms = q < 1 ? 1 : q;
  if (ms != app->session.context.samples)
    app->session.context.samples = ms;
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
  LOCK_GUARD(cmd_mutex);

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


