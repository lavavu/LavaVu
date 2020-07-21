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

#ifdef HAVE_EGL

#include "EGLViewer.h"


// Create a new EGL window
EGLViewer::EGLViewer() : OpenGLViewer()
{
  visible = false;
  eglContext = NULL;
  debug_print("Using EGL Viewer (offscreen only)\n");
}

EGLViewer::~EGLViewer()
{
  if (eglDisplay)
    eglTerminate(eglDisplay);
}

void EGLViewer::open(int w, int h)
{
  //Call base class open to set width/height
  OpenGLViewer::open(w, h);

  const EGLint configAttribs[] =
  {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, // Off-screen
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_DEPTH_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    //EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    //EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  // Initialize EGL
  eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!eglDisplay)
    abort_program("Error creating EGL context");

  EGLint major = 0, minor = 0;
  if (eglInitialize(eglDisplay, &major, &minor) == EGL_FALSE)
    abort_program("Error initializing EGL context");

  // Select configuration
  EGLint numConfigs = 0;
  EGLConfig eglConfig;
  eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs);
  if (!numConfigs)
    abort_program("No valid EGL config found");

  // Bind the API
  eglBindAPI(EGL_OPENGL_API);
  //eglBindAPI(EGL_OPENGL_ES_API);

  // Create a context and make it current
  eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, NULL);

  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);

  debug_print("EGL viewer created, version %d.%d\n", major, minor);

  //Call OpenGL init
  OpenGLViewer::init();
}

void EGLViewer::display(bool redraw)
{
  if (!eglContext) return;
  // Set the current window's context as active
  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);
  OpenGLViewer::display(redraw);
}

void EGLViewer::execute()
{
  if (!eglContext) return;
  // Set the current window's context as active
  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext);

  // Enter dummy event loop processing
  OpenGLViewer::execute();
}

void EGLViewer::show()
{
  //Null function, no visible window
}

void EGLViewer::hide()
{
  //Null function, no visible window
}

#endif   //HAVE_EGL

