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

#ifdef HAVE_X11

#ifndef X11Viewer__
#define X11Viewer__

#include "../GraphicsUtil.h"
#include "../OpenGLViewer.h"

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include "GL/glxext.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

//Derived from interactive window class
class X11Viewer  : public OpenGLViewer
{
  int quitEventLoop;
  GLXContext     glxcontext = 0;
  Display*       Xdisplay = NULL;
  Window         win = 0;
  Colormap       colormap = 0;
  XVisualInfo*   vi = NULL;
  XWMHints*      wmHints = NULL;
  XSizeHints*    sHints = NULL;
  GLXFBConfig    fbconfig = 0;
  Atom           wmDeleteWindow;
  char           displayName[256] = {':', '0', '.', '0'};

  bool hidden;
  bool redisplay;

  //Timer variables and file descriptors for monitoring
  int x11_fd = -1; //Initial unset flag value
  fd_set in_fds;
  struct timeval tv;

public:

  X11Viewer();
  ~X11Viewer();

  //Function implementations
  void open(int w, int h);
  void setsize(int width, int height);
  void show();
  void hide();
  void title(std::string title);
  void display(bool redraw=true);
  void execute();
  void fullScreen();

  bool chooseVisual();
  bool createWindow(int width, int height);
};

#endif //X11Viewer__

#endif //HAVE_X11

