//Global includes file for gLucifer Viewer
//
#ifndef Include__
#define Include__

//Handles compatibility on Linux, Windows, Mac OS X

#define __STDC_CONSTANT_MACROS
#include <stdint.h>

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
#include <climits>
#include <typeinfo>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>

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
#include <sys/types.h>
#include <sys/stat.h>

//Include the decompression routines
#if defined HAVE_LIBPNG and not defined USE_ZLIB
#define USE_ZLIB
#endif

#ifdef USE_ZLIB
#include <zlib.h>
#else
#define MINIZ_HEADER_FILE_ONLY
#include "miniz/miniz.c"
#endif

//Utils
#include "jpeg/jpge.h"
#include "jpeg/jpgd.h"
#if not defined HAVE_LIBPNG
#include "png/lodepng.h"
#endif
#include "json.hpp"

//Preserves order of json keys
#include "fifo_map.hpp"

// A workaround to use fifo_map as map, we are just ignoring the 'less' compare
template<class K, class V, class dummy_compare, class A>
using fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;

// for convenience
using json = nlohmann::json;
using json_fifo = nlohmann::basic_json<fifo_map>;

#ifndef _WIN32
#include <sys/poll.h>
#include <unistd.h>

#define PAUSE(msecs) usleep(msecs * 1000);

#if defined __APPLE__ //Don't use function pointers!
#define GL_GLEXT_PROTOTYPES
#elif not defined EXTENSION_POINTERS
//Extension pointer retrieval on Linux now only if explicity enabled
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
#define GL_R32F 0x822E
static float _X_huge_valf = std::numeric_limits<float>::infinity();
#define HUGE_VALF _X_huge_valf
#define snprintf sprintf_s
#include <conio.h>
#include "windows/inc/pthread.h"
#include "windows/inc/zlib.h"
#define PAUSE(msecs) Sleep(msecs);
#define mkdir(dir, mode) _mkdir(dir)
#define rmdir(dir) _rmdir(dir)
//Include the libraries -- */
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "SDLmain.lib")
#pragma comment(lib, "OPENGL32.LIB")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "libpng.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "pthreadVCE2.lib")

//#include "windows/inc/SDL.h"
#include <SDL/SDL_opengl.h>

#endif

//Define pointers to required gl 2.0 functions
#if defined _WIN32
#define EXTENSION_POINTERS
#endif

typedef void* (*getProcAddressFN)(const char* procName);
#if not defined _WIN32
extern getProcAddressFN GetProcAddress;
#endif

#if defined HAVE_EGL and not defined HAVE_X11
//Using libOpenGL instead of libGL, no old functions
#define glGenRenderbuffersEXT glGenRenderbuffers
#define glBindRenderbufferEXT glBindRenderbuffer
#define glRenderbufferStorageEXT glRenderbufferStorage
#define glGenFramebuffersEXT glGenFramebuffers
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glBindFramebufferEXT glBindFramebuffer
#define glDeleteRenderbuffersEXT glDeleteRenderbuffers
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define glGenerateMipmapEXT glGenerateMipmap
#endif

#include "Extensions.h"

#endif //Include__
