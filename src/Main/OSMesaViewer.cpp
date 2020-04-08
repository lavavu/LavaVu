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

#ifdef HAVE_OSMESA

#include "OSMesaViewer.h"

typedef OSMesaContext GLAPIENTRY (*OSMesaCreateContextAttribs_func)(const int *attribList, OSMesaContext sharelist);

OSMesaViewer::OSMesaViewer() : OpenGLViewer()
{
  visible = false;
  pixelBuffer = NULL;
  osMesaContext = NULL;
  debug_print("Using OSMesa Viewer (offscreen only)\n");
}

OSMesaViewer::~OSMesaViewer()
{
  OSMesaDestroyContext(osMesaContext);
  delete[] pixelBuffer;
}

void OSMesaViewer::open(int w, int h)
{
  //Call base class open to set width/height
  OpenGLViewer::open(w, h);

  if (osMesaContext)
    OSMesaDestroyContext(osMesaContext);
  if (pixelBuffer)
    delete[] pixelBuffer;

  // Init OSMesa display buffer
  pixelBuffer = new GLubyte[width * height * 4];

  static const int attribs[] = {
     OSMESA_FORMAT,                 OSMESA_RGBA,
     OSMESA_DEPTH_BITS,             16,
     OSMESA_STENCIL_BITS,           0,
     OSMESA_ACCUM_BITS,             0,
     OSMESA_PROFILE,                OSMESA_CORE_PROFILE,
     OSMESA_CONTEXT_MAJOR_VERSION,  3,
     OSMESA_CONTEXT_MINOR_VERSION,  3,
     0
  };

  //Must get function pointer for Mesa version < 12
  OSMesaCreateContextAttribs_func OSMesaCreateContextAttribs =
     (OSMesaCreateContextAttribs_func)OSMesaGetProcAddress("OSMesaCreateContextAttribs");
  assert(OSMesaCreateContextAttribs);

  osMesaContext = OSMesaCreateContextAttribs(attribs, NULL);

  //Post-processing: only work in the frame buffer, not in FBO so no use unless we write texture back
  //OSMesaPostprocess(osMesaContext, "pp_jimenezmlaa", 8);
  //OSMesaPostprocess(osMesaContext, "pp_jimenezmlaa_color", 8);
  //OSMesaPostprocess(osMesaContext, "pp_celshade", 1);
  //OSMesaPostprocess(osMesaContext, "pp_nored", 1);

  OSMesaMakeCurrent(osMesaContext, pixelBuffer, GL_UNSIGNED_BYTE, width, height);
  debug_print("OSMesa viewer created\n");

  //Call OpenGL init
  OpenGLViewer::init();
}

void OSMesaViewer::display(bool redraw)
{
  // Make sure we are using the correct context when more than one are created
  OSMesaContext context = OSMesaGetCurrentContext();
  if (context != osMesaContext)
    OSMesaMakeCurrent(osMesaContext, pixelBuffer, GL_UNSIGNED_BYTE, width, height);

   OpenGLViewer::display(redraw);
}

void OSMesaViewer::execute()
{
  OSMesaContext context = OSMesaGetCurrentContext();
  if (context != osMesaContext)
    OSMesaMakeCurrent(osMesaContext, pixelBuffer, GL_UNSIGNED_BYTE, width, height);

  // Enter dummy event loop processing
  OpenGLViewer::execute();
}

void OSMesaViewer::show()
{
  //Null function, no visible window
}

void OSMesaViewer::hide()
{
  //Null function, no visible window
}
#endif   //HAVE_OSMESA

