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

int GLFWViewer::window_count = 0;

static void error_callback(int error, const char *description)
{
  fputs(description, stderr);
}

void resize_callback(GLFWwindow* window, int width, int height)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);
  self->resize(width, height);
  self->redisplay = true;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);

  //Save shift states
  self->get_modifiers(mods);

  if (action != GLFW_PRESS)
  {
    //GLFW modifier key handling is utterly stupid, thus this hack, and below manual setting of modifiers
    //(shift key status flags when pressed are off, on when released, really?)
    if (key >= GLFW_KEY_LEFT_SHIFT && key <= GLFW_KEY_RIGHT_SUPER)
      self->keyState.shift = self->keyState.ctrl = self->keyState.alt = 0;
    return;
  }

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

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
  case GLFW_KEY_LEFT_SHIFT:
  case GLFW_KEY_RIGHT_SHIFT:
    self->keyState.shift = 1;
    break;
  case GLFW_KEY_LEFT_CONTROL:
  case GLFW_KEY_RIGHT_CONTROL:
    self->keyState.ctrl = 1;
    break;
  case GLFW_KEY_LEFT_ALT:
  case GLFW_KEY_RIGHT_ALT:
    self->keyState.alt = 1;
    break;
  default:
    return;
  }
  //printf("KEY: %d %c\n", key, (char)key);

  self->redisplay = true;
}

static void character_callback(GLFWwindow *window, unsigned int code, int mods)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);

  //Save shift states
  self->get_modifiers(mods);

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  //printf("CHAR: %u %c\n", code, (char)code);
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
  if (self->mouseState)
    self->redisplay = self->mouseMove((int)xpos, (int)ypos);
}

static void mousescroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  GLFWViewer* self = (GLFWViewer*)glfwGetWindowUserPointer(window);
  float delta = yoffset / 2.0;
  if (delta != 0)
    self->redisplay = self->mouseScroll(delta);
}

// Create a new GLFW window
GLFWViewer::GLFWViewer() : OpenGLViewer()
{
  debug_print("GLFW viewer created\n");
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
  glfwSetCharModsCallback(window, character_callback);

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
  static int savewidth, saveheight;
  fullscreen = fullscreen ? 0 : 1;
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
}

void GLFWViewer::animate(int msec)
{
  //TODO: not implemented
}

void GLFWViewer::get_modifiers(int modifiers)
{
  //Save shift states
  keyState.shift = (modifiers & GLFW_MOD_SHIFT);
  keyState.ctrl = (modifiers & GLFW_MOD_CONTROL);
  keyState.alt = (modifiers & GLFW_MOD_ALT);
  //keyState.super = (modifiers & GLFW_MOD_SUPER);;
}
#endif


