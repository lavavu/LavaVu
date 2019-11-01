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

#ifndef RenderContext__
#define RenderContext__
#include "GLUtils.h"
#include "Util.h"
#include "Colours.h"
#include "Shaders.h"

class RenderContext
{

public:
  Shader_Ptr fontshader;
  Shader_Ptr defaultshader;

  std::string gl_version = "";
  GLint major = 0, minor = 0, core = 0;
  int samples = 1;
  bool antialiased = false;

  //Matrices
  mat4 MV;
  mat4 N;
  mat4 P;
  std::vector<mat4> MV_stack;
  std::vector<mat4> P_stack;

  //2d scaling for downsample
  float scale2d = 1.0;

  //Copy of model size from view
  float model_size;

  RenderContext()
  {
    MV = linalg::identity;
    P = linalg::identity;
  }

  ~RenderContext()
  {
  }

  void init();
  void push();
  void pop();
  void scale(float factor);
  void scale3(float x, float y, float z);
  void translate3(float x, float y, float z);
  void useDefaultShader(bool is2d=true, bool flat=false);
  void viewport2d(int width, int height);
  int project(float objx, float objy, float objz, float *windowCoordinate);
  int project(float x, float y, float z, int* viewport, float* windowCoordinate);
  mat4 ortho(float left, float right, float bottom, float top, float nearc, float farc);
};

#endif //RenderContext__

