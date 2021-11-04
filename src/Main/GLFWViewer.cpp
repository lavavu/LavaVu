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

#ifdef HAVE_GLFW

#include "GLFWViewer.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(int, get_browser_modifiers, (), {
  if (window.m_alt || window.m_shift || window.m_ctrl) {
    //console.log("GETTING COMMANDS: " + window.commands);
    // 'str.length' would return the length of the string as UTF-16
    // units, but Emscripten C strings operate as UTF-8.
    var val = 0;
    if (window.m_shift) val += 1;
    if (window.m_ctrl) val += 2;
    if (window.m_alt) val += 4;
    return val;
  } else {
    return 0;
  }
});

#endif

int GLFWViewer::window_count = 0;

static void error_callback(int error, const char *description)
{
  fputs(description, stderr);
}

static void resize_callback(GLFWwindow* window, int width, int height)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);
  self->resize(width, height);
  self->redisplay = true;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);

  //Save shift states
  //self->get_modifiers(mods);
  //if (self->keyState.shift || self->keyState.ctrl || self->keyState.alt)
  //  printf("ACTION %d MODIFIERS shift %d ctrl %d alt %d\n", action, self->keyState.shift, self->keyState.ctrl, self->keyState.alt);

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  //Set modifiers
  bool modset = false;
  switch (key)
  {
    case GLFW_KEY_LEFT_SHIFT:
    case GLFW_KEY_RIGHT_SHIFT:
      self->keyState.shift = (action == GLFW_PRESS ? 1 : 0);
      modset = true;
      break;
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_RIGHT_CONTROL:
      self->keyState.ctrl = (action == GLFW_PRESS ? 1 : 0);
      modset = true;
      break;
    case GLFW_KEY_LEFT_ALT:
    case GLFW_KEY_RIGHT_ALT:
      self->keyState.alt = (action == GLFW_PRESS ? 1 : 0);
      modset = true;
      break;
    case GLFW_KEY_LEFT_SUPER:
    case GLFW_KEY_RIGHT_SUPER:
      self->keyState.meta = (action == GLFW_PRESS ? 1 : 0);
      modset = true;
      break;
  }

  if (modset)
  {
    //printf("MODKEY: %d %c action %d shift %d ctrl %d alt %d\n", key, (char)key, action, self->keyState.shift, self->keyState.ctrl, self->keyState.alt);
    return;
  }
  else
  {
    //This works, but not for the actual mod key presses
    self->get_modifiers(mods);
  }

  if (action == GLFW_PRESS)
  {
    switch (key)
    {
    case GLFW_KEY_ESCAPE:
      self->keyPress(KEY_ESC, (int)xpos, (int)ypos);
      //glfwSetWindowShouldClose(window, GL_TRUE);
      break;
    case GLFW_KEY_ENTER:
      self->keyPress(KEY_ENTER, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_TAB:
      self->keyPress(KEY_TAB, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_DELETE:
      self->keyPress(KEY_DELETE, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_BACKSPACE:
      self->keyPress(KEY_BACKSPACE, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_F1:
      self->keyPress(KEY_F1, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_F2:
      self->keyPress(KEY_F2, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_LEFT:
      self->keyPress(KEY_LEFT, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_RIGHT:
      self->keyPress(KEY_RIGHT, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_DOWN:
      self->keyPress(KEY_DOWN, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_UP:
      self->keyPress(KEY_UP, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_PAGE_UP:
      self->keyPress(KEY_PAGEUP, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_PAGE_DOWN:
      self->keyPress(KEY_PAGEDOWN, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_HOME:
      self->keyPress(KEY_HOME, (int)xpos, (int)ypos);
      break;
    case GLFW_KEY_END:
      self->keyPress(KEY_END, (int)xpos, (int)ypos);
      break;
    default:
      //If ALT/CTRL used, char presses not activated so need to process here
      if (self->keyState.ctrl || self->keyState.alt)
      {
        //printf("Attempt to process as char %d\n", key);
        self->keyPress((char)key, (int)xpos, (int)ypos);
      }
      else
      {
        //printf("SKIPKEY: %d %c\n", key, (char)key);
        return;
      }
    }
  }

  self->redisplay = true;

  //printf("KEY: %d %c\n", key, (char)key);
  //printf("KEY: %d %c action %d shift %d ctrl %d alt %d\n", key, (char)key, action, self->keyState.shift, self->keyState.ctrl, self->keyState.alt);
}

static void character_callback(GLFWwindow *window, unsigned int code)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  //printf("CHAR: %u %c shift %d ctrl %d alt %d\n", code, (char)code, self->keyState.shift, self->keyState.ctrl, self->keyState.alt);
  self->keyPress(code, (int)xpos, (int)ypos);
  self->redisplay = true;
}

static void mouseclick_callback(GLFWwindow* window, int button, int action, int mods)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);
  //Save shift states
  self->get_modifiers(mods);

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  MouseButton b = LeftButton;
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    b = RightButton;
  if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    b = MiddleButton;

  if (action == GLFW_PRESS)
  {
    // XOR state of three mouse buttons to the mouseState variable
    if (button <= RightButton) self->mouseState ^= (int)pow(2,b+1);
    self->redisplay = self->mousePress(b, true, (int)xpos, (int)ypos);
  }
  else
  {
    // Release
    self->mouseState = 0;
    self->redisplay = self->mousePress(b, false, (int)xpos, (int)ypos);
  }
}

static void mousemove_callback(GLFWwindow* window, double xpos, double ypos)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);
  self->redisplay = self->mouseMove((int)xpos, (int)ypos);
}

static void mousescroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);
#ifdef __EMSCRIPTEN__
  //Flip dir in emscripten and discretise
  yoffset = yoffset > 0 ? -1 : 1;
#endif
  float delta = yoffset / 2.0;
  if (delta != 0)
    self->redisplay = self->mouseScroll(delta);
}

// Create a new GLFW window
GLFWViewer::GLFWViewer() : OpenGLViewer()
{
  debug_print("Using GLFW Viewer (interactive)\n");
}

GLFWViewer::~GLFWViewer()
{
  if (window)
  {
    glfwDestroyWindow(window);
    window = NULL;
    window_count--;
  }

  //Only call terminate when all windows closed
  if (window_count == 0)
    glfwTerminate();
}

void GLFWViewer::open(int w, int h)
{
  //Call base class open to set width/height
  OpenGLViewer::open(w, h);

  if (window)
  {
    //Resize
    glfwDestroyWindow(window);
  }

  if (!glfwInit())
    abort_program("GLFW init failed");
  else
    window_count++;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_VISIBLE, 0);

  window = glfwCreateWindow(w, h, "LavaVu", NULL, NULL);
  if (!window)
  {
    glfwTerminate();
    //abort_program("Window creation failed");
    return;
  }

  glfwMakeContextCurrent(window);

  glfwSetWindowSizeCallback(window, resize_callback);
  glfwSetWindowUserPointer(window, this);
  glfwSetErrorCallback(error_callback);
  glfwSetCursorPosCallback(window, mousemove_callback);
  glfwSetMouseButtonCallback(window, mouseclick_callback);
  glfwSetScrollCallback(window, mousescroll_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetCharCallback(window, character_callback);

  //Call OpenGL init
  OpenGLViewer::init();
}

void GLFWViewer::setsize(int w, int h)
{
  if (w <= 0 || h <= 0 || !window || (w==width && h==height)) return;

  glfwSetWindowSize(window, w, h);

  //Call base class setsize
  OpenGLViewer::setsize(w, h);
}

void GLFWViewer::show()
{
  if (!window) return;
  OpenGLViewer::show();

  glfwShowWindow(window);
}

void GLFWViewer::title(std::string title)
{
  if (!visible || !window || !isopen) return;
  // Update title
  glfwSetWindowTitle(window, title.c_str());
}

void GLFWViewer::hide()
{
  if (!window) return;
  OpenGLViewer::hide();
  glfwHideWindow(window);
}

void GLFWViewer::display(bool redraw)
{
  if (!window) return;

  glfwMakeContextCurrent(window);
  OpenGLViewer::display(redraw);
  // Swap buffers
  if (redraw && doubleBuffer)
    glfwSwapBuffers(window);
}

void GLFWViewer::execute()
{
  //Inside event loop
  if (!window) open(width, height);

  //Exit?
  quitProgram = glfwWindowShouldClose(window);

  //Wait for event or timer elapse
  glfwWaitEventsTimeout(timer/1000.0); //Wait for event, or timer in milliseconds
  //printf("DISPLAY ? %d %f\n", redisplay, timer/1000.0);

  if (redisplay || postdisplay || pollInput())
  {
    display();
    redisplay = false;
  }
}

void GLFWViewer::fullScreen()
{
  fullscreen = fullscreen ? 0 : 1;
#ifdef __EMSCRIPTEN__
  EM_ASM({ if ($0) Module.requestFullscreen(false,true); else document.exitFullscreen(); }, (int)fullscreen);
#else
  static int savewidth, saveheight;
  if (fullscreen)
  {
    savewidth = width;
    saveheight = height;
    glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, 1920, 1080, GLFW_DONT_CARE);
  }
  else
  {
    glfwSetWindowMonitor(window, NULL, 100, 100, savewidth, saveheight, GLFW_DONT_CARE);
    //self->resize(savewidth, saveheight);
  }
  debug_print("fullscreen %d sw %d sh %d\n", fullscreen, savewidth, saveheight);
#endif
}

void GLFWViewer::animate(int msec)
{
  //TODO: not implemented
}

void GLFWViewer::get_modifiers(int modifiers)
{
#ifdef __EMSCRIPTEN__
  keyState.alt = (modifiers & GLFW_MOD_ALT);
  if (keyState.alt) {
    //Alt processing is screwy, get state from browser
    modifiers = get_browser_modifiers();
  }
#endif
  //Save shift states
  keyState.shift = (modifiers & GLFW_MOD_SHIFT);
  keyState.ctrl = (modifiers & GLFW_MOD_CONTROL);
  keyState.alt = (modifiers & GLFW_MOD_ALT);
  //keyState.super = (modifiers & GLFW_MOD_SUPER);;
}
#endif


