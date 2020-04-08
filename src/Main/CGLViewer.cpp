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

#ifdef HAVE_CGL

#include "CGLViewer.h"

// Create a new CGL window
CGLViewer::CGLViewer() : OpenGLViewer()
{
  visible = false;
  context = NULL;
  debug_print("Using CGL Viewer (offscreen only)\n");
}

CGLViewer::~CGLViewer()
{
  if (context)
    CGLDestroyContext(context);
}

void CGLViewer::open(int w, int h)
{
  //Call base class open to set width/height
  OpenGLViewer::open(w, h);

  CGLPixelFormatObj pix;
  int num = 0;
  CGLError err;
  CGLPixelFormatAttribute attribs[8] = 
    {
      kCGLPFAColorSize,     (CGLPixelFormatAttribute)24,
      kCGLPFAAlphaSize,     (CGLPixelFormatAttribute)8,
      kCGLPFAAccelerated,
      kCGLPFAOpenGLProfile,
      (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
      (CGLPixelFormatAttribute) 0
    };

  err = CGLChoosePixelFormat(attribs, &pix, &num);

  if (err == kCGLBadPixelFormat)
    abort_program("Error creating OpenGL context");

  err = CGLCreateContext(pix, NULL, &context);

  if (err == kCGLBadContext)
    abort_program("Error creating OpenGL context");

  CGLDestroyPixelFormat(pix);

  err = CGLSetCurrentContext(context);

  debug_print("CGL viewer created\n");

  //Call OpenGL init
  OpenGLViewer::init();
}

void CGLViewer::display(bool redraw)
{
  if (!context) return;
  // Set the current window's context as active
  CGLSetCurrentContext(context);
  OpenGLViewer::display(redraw);
}

void CGLViewer::execute()
{
  // Set the current window's context as active
  CGLSetCurrentContext(context);

  // Enter dummy event loop processing
  OpenGLViewer::execute();
}

void CGLViewer::show()
{
  //Null function, no visible window
}

void CGLViewer::hide()
{
  //Null function, no visible window
}


#endif   //HAVE_CGL

