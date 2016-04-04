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

// gcc CocoaViewer.m -o OSXWindow -framework Cocoa -framework Quartz -framework OpenGL

#ifdef HAVE_COCOA

#include "CocoaViewer.h"

#include <sys/select.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>
#import <OpenGL/OpenGL.h>

@class View;
static CVReturn GlobalDisplayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void*);

@interface View : NSOpenGLView <NSWindowDelegate>
{
@public
  CVDisplayLinkRef displayLink;
  bool running;
  NSRect windowRect;
  NSRecursiveLock* appLock;
}
@end

@implementation View
// Initialize
- (id) initWithFrame: (NSRect) frame
{
  running = true;

  // No multisampling
  int samples = 0;

  // Keep multisampling attributes at the start of the attribute lists since code below assumes they are array elements 0 through 4.
  NSOpenGLPixelFormatAttribute windowedAttrs[] =
  {
    NSOpenGLPFAMultisample,
    NSOpenGLPFASampleBuffers, samples ? 1 : 0,
    NSOpenGLPFASamples, samples,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize, 32,
    NSOpenGLPFADepthSize, 24,
    NSOpenGLPFAAlphaSize, 8,
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
    0
  };

  // Try to choose a supported pixel format
  NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:windowedAttrs];

  if (!pf)
  {
    bool valid = false;
    while (!pf && samples > 0)
    {
      samples /= 2;
      windowedAttrs[2] = samples ? 1 : 0;
      windowedAttrs[4] = samples;
      pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:windowedAttrs];
      if (pf)
      {
        valid = true;
        break;
      }
    }

    if (!valid)
    {
      NSLog(@"OpenGL pixel format not supported.");
      return nil;
    }
  }

  self = [super initWithFrame:frame pixelFormat:[pf autorelease]];
  appLock = [[NSRecursiveLock alloc] init];

  return self;
}

- (void) prepareOpenGL
{
  [super prepareOpenGL];

  [[self window] setLevel: NSNormalWindowLevel];
  [[self window] makeKeyAndOrderFront: self];

  // Make all the OpenGL calls to setup rendering and build the necessary rendering objects
  [[self openGLContext] makeCurrentContext];
  // Synchronize buffer swaps with vertical refresh rate
  GLint swapInt = 1; // Vsynch on!
  [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

  // Create a display link capable of being used with all active displays
  CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);

  // Set the renderer output callback function
  CVDisplayLinkSetOutputCallback(displayLink, &GlobalDisplayLinkCallback, self);

  CGLContextObj cglContext = (CGLContextObj)[[self openGLContext] CGLContextObj];
  CGLPixelFormatObj cglPixelFormat = (CGLPixelFormatObj)[[self pixelFormat] CGLPixelFormatObj];
  CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);

  GLint dim[2] = {windowRect.size.width, windowRect.size.height};
  CGLSetParameter(cglContext, kCGLCPSurfaceBackingSize, dim);
  CGLEnable(cglContext, kCGLCESurfaceBackingSize);

  [appLock lock];
  CGLLockContext((CGLContextObj)[[self openGLContext] CGLContextObj]);
  NSLog(@"Initialize");

  NSLog(@"GL version:   %s", glGetString(GL_VERSION));
  NSLog(@"GLSL version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
  // Temp
  glClearColor(0.5f, 0.6f, 0.7f, 1.0f);
  glViewport(0, 0, windowRect.size.width, windowRect.size.height);
  glEnable(GL_DEPTH_TEST);
  // End temp
  CGLUnlockContext((CGLContextObj)[[self openGLContext] CGLContextObj]);
  [appLock unlock];

  // Activate the display link
  CVDisplayLinkStart(displayLink);
}

// Tell the window to accept input events
- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (void)mouseMoved:(NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Mouse pos: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void) mouseDragged: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Mouse pos: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void)scrollWheel: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Mouse wheel at: %lf, %lf. Delta: %lf", point.x, point.y, [event deltaY]);
  [appLock unlock];
}

- (void) mouseDown: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Left mouse down: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void) mouseUp: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Left mouse up: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void) rightMouseDown: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Right mouse down: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void) rightMouseUp: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Right mouse up: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void)otherMouseDown: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Middle mouse down: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void)otherMouseUp: (NSEvent*) event
{
  [appLock lock];
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  NSLog(@"Middle mouse up: %lf, %lf", point.x, point.y);
  [appLock unlock];
}

- (void) mouseEntered: (NSEvent*)event
{
  [appLock lock];
  NSLog(@"Mouse entered");
  [appLock unlock];
}

- (void) mouseExited: (NSEvent*)event
{
  [appLock lock];
  NSLog(@"Mouse left");
  [appLock unlock];
}

- (void) keyDown: (NSEvent*) event
{
  [appLock lock];
  if ([event isARepeat] == NO)
  {
    NSLog(@"Key down: %d", [event keyCode]);
  }
  [appLock unlock];
}

- (void) keyUp: (NSEvent*) event
{
  [appLock lock];
  NSLog(@"Key up: %d", [event keyCode]);
  [appLock unlock];
}

// Update
- (CVReturn) getFrameForTime:(const CVTimeStamp*)outputTime
{
  [appLock lock];

  [[self openGLContext] makeCurrentContext];
  CGLLockContext((CGLContextObj)[[self openGLContext] CGLContextObj]);

  NSLog(@"Update");
  // Temp
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // EndTemp

  CGLFlushDrawable((CGLContextObj)[[self openGLContext] CGLContextObj]);
  CGLUnlockContext((CGLContextObj)[[self openGLContext] CGLContextObj]);

  if (false)   // Update loop returns false
  {
    [NSApp terminate:self];
  }

  [appLock unlock];

  return kCVReturnSuccess;
}

// Resize
- (void)windowDidResize:(NSNotification*)notification
{
  NSSize size = [ [ _window contentView ] frame ].size;
  [appLock lock];
  [[self openGLContext] makeCurrentContext];
  CGLLockContext((CGLContextObj)[[self openGLContext] CGLContextObj]);
  NSLog(@"Window resize: %lf, %lf", size.width, size.height);
  // Temp
  windowRect.size.width = size.width;
  windowRect.size.height = size.height;
  glViewport(0, 0, windowRect.size.width, windowRect.size.height);
  // End temp
  CGLUnlockContext((CGLContextObj)[[self openGLContext] CGLContextObj]);
  [appLock unlock];
}

- (void)resumeDisplayRenderer
{
  [appLock lock];
  CVDisplayLinkStop(displayLink);
  [appLock unlock];
}

- (void)haltDisplayRenderer
{
  [appLock lock];
  CVDisplayLinkStop(displayLink);
  [appLock unlock];
}

// Terminate window when the red X is pressed
-(void)windowWillClose:(NSNotification *)notification
{
  if (running)
  {
    running = false;

    [appLock lock];
    NSLog(@"Cleanup");

    CVDisplayLinkStop(displayLink);
    CVDisplayLinkRelease(displayLink);

    [appLock unlock];
  }

  [NSApp terminate:self];
}

// Cleanup
- (void) dealloc
{
  [appLock release];
  [super dealloc];
}
@end

static CVReturn GlobalDisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
  CVReturn result = [(View*)displayLinkContext getFrameForTime:outputTime];
  return result;
}

int main(int argc, const char * argv[])
{

}

// Create a new AGL window
CocoaViewer::CocoaViewer() : OpenGLViewer(false, false)
{
  visible = false;
  graphicsContext = NULL;
}

CocoaViewer::~CocoaViewer()
{
}

void CocoaViewer::open(int w, int h)
{
  //Call base class open to set width/height
  OpenGLViewer::open(w, h);

  // Autorelease Pool:
  // Objects declared in this scope will be automatically
  // released at the end of it, when the pool is "drained".
  NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

  // Create a shared app instance.
  // This will initialize the global variable
  // 'NSApp' with the application instance.
  [NSApplication sharedApplication];

  // Create a window:

  // Style flags
  NSUInteger windowStyle = NSTitledWindowMask  | NSClosableWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask;

  // Window bounds (x, y, width, height)
  NSRect screenRect = [[NSScreen mainScreen] frame];
  NSRect viewRect = NSMakeRect(0, 0, w, h);
  NSRect windowRect = NSMakeRect(NSMidX(screenRect) - NSMidX(viewRect),
                                 NSMidY(screenRect) - NSMidY(viewRect),
                                 viewRect.size.width,
                                 viewRect.size.height);

  NSWindow * window = [[NSWindow alloc] initWithContentRect:windowRect
                       styleMask:windowStyle
                       backing:NSBackingStoreBuffered
                       defer:NO];
  [window autorelease];

  // Window controller
  NSWindowController * windowController = [[NSWindowController alloc] initWithWindow:window];
  [windowController autorelease];

  // Since Snow Leopard, programs without application bundles and Info.plist files don't get a menubar
  // and can't be brought to the front unless the presentation option is changed
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  // Next, we need to create the menu bar. You don't need to give the first item in the menubar a name
  // (it will get the application's name automatically)
  id menubar = [[NSMenu new] autorelease];
  id appMenuItem = [[NSMenuItem new] autorelease];
  [menubar addItem:appMenuItem];
  [NSApp setMainMenu:menubar];

  // Then we add the quit item to the menu. Fortunately the action is simple since terminate: is
  // already implemented in NSApplication and the NSApplication is always in the responder chain.
  id appMenu = [[NSMenu new] autorelease];
  id appName = [[NSProcessInfo processInfo] processName];
  id quitTitle = [@"Quit " stringByAppendingString:appName];
  id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
                      action:@selector(terminate:) keyEquivalent:@"q"] autorelease];
  [appMenu addItem:quitMenuItem];
  [appMenuItem setSubmenu:appMenu];

  // Create app delegate to handle system events
  View* view = [[[View alloc] initWithFrame:windowRect] autorelease];
  view->windowRect = windowRect;
  [window setAcceptsMouseMovedEvents:YES];
  [window setContentView:view];
  [window setDelegate:view];

  // Set app title
  [window setTitle:appName];

  // Add fullscreen button
  [window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];

#if 0
CGLContextObj context;
    int num = 0;
    CGLError err;
    CGLPixelFormatAttribute attribs[4] = 
      {
        kCGLPFAAccelerated,
        kCGLPFAOpenGLProfile,
        /*
        (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        (CGLPixelFormatAttribute) kCGLOGLPVersion_GL3_Core,
        (CGLPixelFormatAttribute) kCGLOGLPVersion_Legacy,
        */
        (CGLPixelFormatAttribute) 0
      };

    err = CGLChoosePixelFormat(attribs, &pixelFormat,&num);

    if (err == kCGLBadPixelFormat)
      abort_progam("Error creating OpenGL context");

    err = CGLCreateContext(pixelFormat, NULL, &context);

    if (err == kCGLBadContext)
      abort_progam("Error creating OpenGL context");

    CGLDestroyPixelFormat(pixelFormat);

    err = CGLSetCurrentContext(context);
//    CGLDestroyContext(context);
#endif

  debug_print("Cocoa viewer created\n");

  //Call OpenGL init
  OpenGLViewer::init();
}

void CocoaViewer::setsize(int width, int height)
{
  if (width == 0 || height == 0) return;
  close();
  open(width, height);
}

void CocoaViewer::show()
{
  OpenGLViewer::show();
}

void CocoaViewer::display()
{
  OpenGLViewer::display();
  swap();
}

void CocoaViewer::swap()
{
  // Swap buffers
  //if (doubleBuffer)
  //  aglSwapBuffers(graphicsContext);

  OpenGLViewer::display();
}

void CocoaViewer::execute()
{
  //Ensure window visible for interaction
  if (visible)
  {
    // Show window and run event loop
    [window orderFrontRegardless];
    [NSApp run];

    [pool drain];
  }
  else
  {
    //Run our own event loop
    OpenGLViewer::execute();
  }
}

#endif   //HAVE_COCOA
