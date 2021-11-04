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

#include "X11Viewer.h"

#include <signal.h>
#include <sys/time.h>

int X11_error(Display* Xdisplay, XErrorEvent* error)
{
  char error_str[256];
  XGetErrorText(Xdisplay, error->error_code, error_str, 256);
  fprintf(stderr, "X11 Error: %d -> %s\n", error->error_code, error_str);
  return error->error_code;
}

// Create a new X11 window
X11Viewer::X11Viewer() : OpenGLViewer(), hidden(false), redisplay(true)
{
  XSetErrorHandler(X11_error);
  debug_print("Using X11 Viewer (interactive)\n");
}

X11Viewer::~X11Viewer()
{
  if (Xdisplay)
  {
    if (glxcontext)
    {
      glXMakeCurrent(Xdisplay, None, NULL);
      glXDestroyContext(Xdisplay, glxcontext);
    }
    if (sHints) XFree(sHints);
    if (wmHints) XFree(wmHints);
    if (win) XDestroyWindow(Xdisplay, win);
    if (colormap) XFreeColormap(Xdisplay, colormap);
    if (vi) XFree(vi);
    XSetCloseDownMode(Xdisplay, DestroyAll);
    XCloseDisplay(Xdisplay);
  }
}

void X11Viewer::open(int w, int h)
{
  //Call base class open to set width/height
  OpenGLViewer::open(w, h);

  if (!Xdisplay)
  {
    //********************** Create Display *****************************
    Xdisplay = XOpenDisplay(NULL);
    debug_print("Opening default X display\n");
    if (Xdisplay == NULL)
    {
      // Second Try
      debug_print("Failed, trying %s\n", displayName);
      Xdisplay = XOpenDisplay(displayName);
      if (Xdisplay == NULL)
      {
        abort_program("Failed to open X display\n");
        return;
      }
    }

    // Check to make sure display we've just opened has a glx extension
    if (!glXQueryExtension(Xdisplay, NULL, NULL))
    {
      abort_program("X display has no OpenGL GLX extension\n");
      return;
    }

    // Get visual
    if (!chooseVisual()) return;

    // Create a window or buffer to render to
    createWindow(width, height);
  }
  else
  {
    //Resize
    if (glxcontext)
    {
      glXMakeCurrent(Xdisplay, None, NULL);
      glXDestroyContext(Xdisplay, glxcontext);
    }
    if (win)
      XDestroyWindow(Xdisplay, win);
    createWindow(width, height);
    //show();
    XSync(Xdisplay, false);  // Flush output buffer
  }

  //Call OpenGL init
  if (Xdisplay && glxcontext)
    OpenGLViewer::init();
}

void X11Viewer::setsize(int w, int h)
{
  if (w <= 0 || h <= 0 || !Xdisplay || (w==width && h==height)) return;
  if (win)
    XResizeWindow(Xdisplay, win, w, h);
  //Call base class setsize
  OpenGLViewer::setsize(w, h);
}

void X11Viewer::show()
{
  if (!Xdisplay || !win) return;
  OpenGLViewer::show();

  XMapRaised(Xdisplay, win); // Show the window

  //Notify active window, raises window even if already mapped
  XEvent xev;
  memset(&xev, 0, sizeof(xev));
  xev.type = ClientMessage;
  xev.xclient.window = win;
  xev.xclient.message_type = XInternAtom(Xdisplay, "_NET_ACTIVE_WINDOW", False);
  xev.xclient.format = 32;

  XSendEvent(Xdisplay, DefaultRootWindow(Xdisplay), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

void X11Viewer::title(std::string title)
{
  if (!visible || !Xdisplay || !isopen || !win) return;
  // Update title
  XTextProperty Xtitle;
  char* titlestr = (char*)title.c_str();
  XStringListToTextProperty(&titlestr, 1, &Xtitle);  //\/ argv, argc, normal_hints, wm_hints, class_hints
  XSetWMProperties(Xdisplay, win, &Xtitle, &Xtitle, NULL, 0, sHints, wmHints, NULL);
  XFree(Xtitle.value);
}

void X11Viewer::hide()
{
  if (!Xdisplay || !win) return;
  OpenGLViewer::hide();
  XUnmapWindow(Xdisplay, win); // Hide the window
}

void X11Viewer::display(bool redraw)
{
  if (!Xdisplay || !win) return;
  glXMakeCurrent(Xdisplay, win, glxcontext);
  OpenGLViewer::display(redraw);
  // Swap buffers
  if (redraw && doubleBuffer)
    glXSwapBuffers(Xdisplay, win);
}

void X11Viewer::execute()
{
  //Inside event loop
  //visible = true;
  if (!Xdisplay || !win) open(width, height);
  XEvent event;
  MouseButton button;
  unsigned char key;
  KeySym keysym;
  char buf[64];
  int count;

  //First time through?
  if (x11_fd < 0)
  {
    //Get file descriptor of the X11 display
    x11_fd = ConnectionNumber(Xdisplay);
    //Create a File Description Set containing x11_fd
    FD_ZERO(&in_fds);
    FD_SET(x11_fd, &in_fds);
  }

  // Event processing
  if (!nodisplay && timer > 0)
  {
    // Wait for X Event or timer
    tv.tv_usec  = timer*1000; //Convert to microseconds
    tv.tv_sec   = 0;
    if (select(x11_fd+1, &in_fds, 0, 0, &tv) == 0)
    {
      //Timer fired
      if (postdisplay || pollInput())
        display();
    }
  }

  //Process all pending events
  while (XPending(Xdisplay))
  {
    // Get next event
    XNextEvent(Xdisplay, &event);

    //Save shift states
    keyState.shift = (event.xkey.state & ShiftMask);
    keyState.ctrl = (event.xkey.state & ControlMask);
#ifdef __APPLE__
    keyState.alt = (event.xkey.state & Mod1Mask | event.xkey.state & Mod2Mask);
#else
    keyState.alt = (event.xkey.state & Mod1Mask);
#endif

    switch (event.type)
    {
    case ButtonRelease:
      mouseState = 0;
      button = (MouseButton)event.xbutton.button;
      mousePress(button, false, event.xmotion.x, event.xmotion.y);
      redisplay = true;
      break;
    case ButtonPress:
      // XOR state of three mouse buttons to the mouseState variable
      button = (MouseButton)event.xbutton.button;
      if (button <= RightButton) mouseState ^= (int)pow(2, (float)button);
      mousePress(button, true, event.xmotion.x, event.xmotion.y);
      break;
    case MotionNotify:
      redisplay = mouseMove(event.xmotion.x, event.xmotion.y);
      break;
    case KeyPress:
      // Convert to cross-platform codes for event handler
      count = XLookupString((XKeyEvent *)&event, buf, 64, &keysym, NULL);
      if (count) key = buf[0];
      else if (keysym == XK_KP_Up || keysym == XK_Up) key = KEY_UP;
      else if (keysym == XK_KP_Down || keysym == XK_Down) key = KEY_DOWN;
      else if (keysym == XK_KP_Left || keysym == XK_Left) key = KEY_LEFT;
      else if (keysym == XK_KP_Right || keysym == XK_Right) key = KEY_RIGHT;
      else if (keysym == XK_KP_Page_Up || keysym == XK_Page_Up) key = KEY_PAGEUP;
      else if (keysym == XK_KP_Page_Down || keysym == XK_Page_Down) key = KEY_PAGEDOWN;
      else if (keysym == XK_KP_Home || keysym == XK_Home) key = KEY_HOME;
      else if (keysym == XK_KP_End || keysym == XK_End) key = KEY_END;
      else key = keysym;

      if (keyPress(key, event.xkey.x, event.xkey.y))
        redisplay = true;

      break;
    case ClientMessage:
      if (event.xclient.data.l[0] == (long)wmDeleteWindow)
        quitProgram = true;
      break;
    case MapNotify:
      // Window shown
      if (hidden) redisplay = true;
      hidden = false;
      break;
    case UnmapNotify:
      // Window hidden, iconized or switched to another desktop
      hidden = true;
      redisplay = false;
      break;
    case ConfigureNotify:
    {
      // Notification of window actions, including resize
      resize(event.xconfigure.width, event.xconfigure.height);
      break;
    }
    case Expose:
      if (!hidden) redisplay = true;
      break;
    default:
      redisplay = false;
    }
  }

  //Redisplay if required.
  if (redisplay && !nodisplay)
  {
    // Redraw Viewer (Call virtual to display)
    display();
    redisplay = false;
  }
}

void X11Viewer::fullScreen()
{
  if (!Xdisplay || !win) return;
  XEvent xev;
  memset(&xev, 0, sizeof(xev));
  xev.type = ClientMessage;
  xev.xclient.window = win;
  xev.xclient.message_type = XInternAtom(Xdisplay, "_NET_WM_STATE", False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = fullscreen ? 0 : 1;
  xev.xclient.data.l[1] = XInternAtom(Xdisplay, "_NET_WM_STATE_FULLSCREEN", False);
  xev.xclient.data.l[2] = 0;

  XSendEvent(Xdisplay, DefaultRootWindow(Xdisplay), False, SubstructureNotifyMask, &xev);

  fullscreen = fullscreen ? 0 : 1;
}

bool X11Viewer::chooseVisual()
{
  int scrnum = DefaultScreen(Xdisplay);
  static int configuration[] = { GLX_STEREO, 1, GLX_DOUBLEBUFFER, 1, GLX_DEPTH_SIZE, 8,
                                 GLX_ALPHA_SIZE, 8, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                                 GLX_RENDER_TYPE, GLX_RGBA_BIT,
                                 None
                               };
  const char* configStrings[] = {"Stereo", "Double-buffered"};

  // find an OpenGL-capable display - trying different configurations if nessesary
  // Note: only attempt to get stereo and double buffered visuals when in interactive mode
  //Updated for 3.0+ https://www.khronos.org/opengl/wiki/Tutorial:_OpenGL_3.0_Context_Creation_(GLX)
  int fbcount, fbcidx;
  GLXFBConfig* fbcfg = NULL;
  vi = NULL;
  for (int i = stereo ? 0 : 2; i < 4; i += 2)
  {
    debug_print("Attempting config %d: ( ", (i/2)+1);
    for (int j = i/2; j < 2; j++)
      debug_print("%s ", configStrings[j]);
    debug_print(")\n");

    //OpenGL3+
    fbcfg = glXChooseFBConfig(Xdisplay, scrnum, &configuration[i], &fbcount);
    if (fbcount == 0) continue;

    // Pick the FB config/visual with the most samples per pixel
    debug_print( "Getting XVisualInfos\n" );
    int best_fbc = -1, best_num_samp = -1;
    for (fbcidx = 0; fbcidx<fbcount; fbcidx++)
    {
      vi = glXGetVisualFromFBConfig(Xdisplay, fbcfg[fbcidx]);
      if (vi)
      {
        int samp_buf, samples, depth, db;
        glXGetFBConfigAttrib(Xdisplay, fbcfg[fbcidx], GLX_SAMPLE_BUFFERS, &samp_buf);
        glXGetFBConfigAttrib(Xdisplay, fbcfg[fbcidx], GLX_SAMPLES, &samples);
        glXGetFBConfigAttrib(Xdisplay, fbcfg[fbcidx], GLX_DEPTH_SIZE, &depth);
        glXGetFBConfigAttrib(Xdisplay, fbcfg[fbcidx], GLX_DOUBLEBUFFER, &db);

        debug_print("  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d, SAMPLES = %d DEPTH %d DB %d\n",
                    fbcidx, (unsigned int)vi->visualid, samp_buf, samples, depth, db);

        XFree(vi);

        if ( best_fbc < 0 || (samp_buf && samples > best_num_samp))
          best_fbc = fbcidx, best_num_samp = samples;
        //4 x Multisample is fine
        if (samples == 4)
        {
          best_fbc = fbcidx;
          break;
        }
      }
    }
    //Use the best or first with 4xMSAA
    fbcidx = best_fbc;
    //Store the chosen FB config
    fbconfig = fbcfg[fbcidx];
    vi = glXGetVisualFromFBConfig(Xdisplay, fbconfig);
    //Free the array
    XFree(fbcfg);

    if (vi && fbconfig && fbcount > 0)
    {
      debug_print("Success, Got %d FB configs (Using %d)\n", fbcount, fbcidx);
      break;
    }
    debug_print("Failed\n");
  }

  if (vi == NULL)
  {
    fprintf(stderr,  "In func %s: Couldn't open display\n", __func__);
    return false;
  }
  return true;
}

bool X11Viewer::createWindow(int width, int height)
{
  XSetWindowAttributes swa;

  // Create Colourmap
  swa.colormap = colormap = XCreateColormap(Xdisplay, RootWindow(Xdisplay, vi->screen), vi->visual, AllocNone);
  swa.border_pixel = 0;
  swa.background_pixel = 0;
  swa.event_mask = ExposureMask | StructureNotifyMask | ButtonReleaseMask | ButtonPressMask | PointerMotionMask | ButtonMotionMask | KeyPressMask;

  if (!sHints || !wmHints)
  {
    // Setup window manager hints
    sHints = XAllocSizeHints();
    wmHints = XAllocWMHints();
  }

  if (sHints && wmHints)
  {
    sHints->min_width = 32;
    sHints->min_height = 32;
    sHints->max_height = 4096;
    sHints->max_width = 4096;

    sHints->flags = PMaxSize | PMinSize | USPosition;
    // Center
    sHints->x = (DisplayWidth(Xdisplay, vi->screen) - width) / 2;
    sHints->y = (DisplayHeight(Xdisplay, vi->screen) - height) / 2;


    // Create X window
    win = XCreateWindow(Xdisplay, RootWindow(Xdisplay, vi->screen),
                        sHints->x, sHints->y, width, height,
                        0, vi->depth, InputOutput, vi->visual,
                        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &swa);

    wmHints->initial_state = NormalState;
    wmHints->flags = StateHint;

    wmDeleteWindow = XInternAtom(Xdisplay, "WM_DELETE_WINDOW", 1);
    XSetWMProtocols(Xdisplay, win, &wmDeleteWindow, 1);

    // OpenGL 3.3
    int attribs[] = {
      GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
      GLX_CONTEXT_MINOR_VERSION_ARB, 3,
      //GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
      //GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
      None
    };

    // NOTE: It is not necessary to create or make current to a context before
    // calling glXGetProcAddressARB
    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    assert(fbconfig);
    glxcontext = glXCreateContextAttribsARB(Xdisplay, fbconfig, NULL, true, attribs);

    if (glxcontext)
    {
      if (glXIsDirect(Xdisplay, glxcontext))
        debug_print("GLX: Direct rendering enabled.\n");
      else
        debug_print("GLX: No direct rendering context available, using indirect.\n");

      //if (visible)
      //   XMapRaised( Xdisplay, win ); // Show the window

      glXMakeCurrent(Xdisplay, win, glxcontext);

      //printf("OpenGL:\n\tvendor %s\n\trenderer %s\n\tversion %s\n\tshader language %s\n\n",
      //  glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION),
      //  glGetString(GL_SHADING_LANGUAGE_VERSION));
    }
    else
      fprintf(stderr, "In func %s: Could not create GLX rendering context.\n", __func__);
  }
  else
    return false;


  if (!glxcontext) abort_program("No context!\n");
  return true;
}

#endif


