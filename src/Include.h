//Global includes file for LavaVu

#ifndef Include__
#define Include__

#if defined _WIN32
//#define GetProcAddress(arg) wglGetProcAddress((LPCSTR)arg)
#elif defined __APPLE__
#undef EXTENSION_POINTERS //Don't support extension pointers on MacOS
#elif defined HAVE_X11
#define GetProcAddress(arg) glXGetProcAddressARB(arg);
#elif defined HAVE_EGL
#define GetProcAddress(arg) eglGetProcAddress(arg);
#endif

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
#ifdef USE_ZLIB
#include <zlib.h>
#else
#include "miniz/miniz.h"
#endif

//Utils
#include "jpeg/jpge.h"
#include "jpeg/jpgd.h"
#include "json.hpp"

//Preserves order of json keys
#include "fifo_map.hpp"

// A workaround to use fifo_map as map, we are just ignoring the 'less' compare
template<class K, class V, class dummy_compare, class A>
using fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;

// for convenience
using json = nlohmann::json;
using json_fifo = nlohmann::basic_json<fifo_map>;

typedef void* (*getProcAddressFN)(const char* procName);

#ifndef _WIN32
//POSIX
extern getProcAddressFN GetProcAddress;
#include <poll.h>
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
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#else
#include <GL/gl.h>
#endif

#else //_WIN32

// WINDOWS
#define GL_R32F 0x822E
#define GL_COMPRESSED_RED 0x8225

#ifndef HUGE_VALF
static float _X_huge_valf = std::numeric_limits<float>::infinity();
#define HUGE_VALF _X_huge_valf
#endif
#define snprintf sprintf_s
#include <conio.h>
#include <direct.h>
#define HAVE_STRUCT_TIMESPEC
#define PAUSE(msecs) Sleep(msecs);
#define mkdir(dir, mode) _mkdir(dir)
#define rmdir(dir) _rmdir(dir)

#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
//Don't include extension prototypes as we are getting them with GetProcAddress
//#define GL_GLEXT_PROTOTYPES
#include <glext.h>

//Define pointers to required gl 2.0 functions
#define EXTENSION_POINTERS
#endif //_WIN32

#include "Extensions.h"

#endif //Include__
