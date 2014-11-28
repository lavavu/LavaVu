//Global includes file for gLucifer Viewer
//
#ifndef Include__
#define Include__

//Handles compatibility on Linux, Windows, Mac OS X

#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#if defined USE_OMEGALIB
#include <omegaGl.h>
#endif

#define ISFINITE(val) (!isnan(val) && !isinf(val))

//C++ STL
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <deque>
#include <iomanip>

//C headers
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <ctype.h>
#include <pthread.h>

//Utils
#include "jpeg/jpge.h"

#ifndef _WIN32
#include <sys/poll.h>
#include <unistd.h>

#define PAUSE(msecs) usleep(msecs * 1000);

#if defined __APPLE__ //Don't use function pointers!
#define GL_GLEXT_PROTOTYPES
#elif defined NO_EXTENSION_POINTERS
//Added to allow disabling of extension pointer retrieval
#define GL_GLEXT_PROTOTYPES
#define GL_GLXEXT_PROTOTYPES
#endif

//This could be problematic with different configurations...
#if defined __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#else

/* WINDOWS */
#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#include <conio.h>
#include "windows/inc/pthread.h"
#define PAUSE(msecs) Sleep(msecs);
//Include the libraries -- */
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "SDLmain.lib")
#pragma comment(lib, "OPENGL32.LIB")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "libpng.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "pthreadVCE2.lib")

//#include "windows/inc/SDL.h"
#include <SDL/SDL_opengl.h>

#endif

//Define pointers to required gl 2.0 functions
#if defined _WIN32 
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
#define EXTENSION_POINTERS
#endif

//Messy but allow disabling until tested on various Linux setups and with OSMesa 
//Known to fix problems in parallels vm, use by adding -DNO_EXTENSION_POINTERS to the build config
#if not defined __APPLE__ and not defined USE_OMEGALIB and not defined NO_EXTENSION_POINTERS
#define EXTENSION_POINTERS
#endif

typedef void* (*getProcAddressFN)(const char* procName);
#if not defined _WIN32
extern getProcAddressFN GetProcAddress;
#endif 

#ifdef EXTENSION_POINTERS
//Define pointers to required gl 2.0 functions
extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
extern PFNGLPOINTPARAMETERFVPROC glPointParameterfv;
extern PFNGLPOINTPARAMETERFPROC glPointParameterf;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLISBUFFERPROC glIsBuffer;
extern PFNGLMAPBUFFERPROC glMapBuffer;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLUNMAPBUFFERPROC glUnmapBuffer;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;

extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLISSHADERPROC glIsShader;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;

extern PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
extern PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
extern PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
extern PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
extern PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;

extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM1FVPROC glUniform1fv;
extern PFNGLUNIFORM2FVPROC glUniform2fv;
extern PFNGLUNIFORM3FVPROC glUniform3fv;
extern PFNGLUNIFORM4FVPROC glUniform4fv;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLISPROGRAMPROC glIsProgram;
#endif

void OpenGL_Extensions_Init();

#endif //Include__
