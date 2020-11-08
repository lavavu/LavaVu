#ifndef GL_Error_Check
#include "Include.h"
#include "linalg.h"

#define vec3 linalg::aliases::float3
#define vec4 linalg::aliases::float4
#define mat4 linalg::aliases::float4x4

#ifdef DEBUG
#define GL_Error_Check_(fatal) \
  { \
    char buffer[2048]; \
    GLenum error = GL_NO_ERROR; \
    while ((error = glGetError()) != GL_NO_ERROR) { \
      sprintf(buffer, "OpenGL error [ %s : %d ] \"%s\".\n",  \
            __FILE__, __LINE__, glErrorString(error)); \
      if (fatal)  \
        throw std::runtime_error(buffer); \
      else \
        std::cerr << buffer; \
    } \
  }

#ifdef __EMSCRIPTEN__
#define GL_Error_Check GL_Error_Check_(false)
#else
#define GL_Error_Check GL_Error_Check_(true)
#endif
#define GL_Error_Print GL_Error_Check_(false)

#else
#define GL_Error_Check
#define GL_Error_Print
#endif //DEBUG

const char* glErrorString(GLenum errorCode);

#endif //GL_Error_Check

