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
#include "RenderContext.h"

void RenderContext::init()
{
  //Default colour only shader
  defaultshader = std::make_shared<Shader>("default.vert", "default.frag");
  defaultshader->loadUniforms();
  defaultshader->loadAttribs();
  //Default font shader
  fontshader = std::make_shared<Shader>("fontShader.vert", "fontShader.frag");
  fontshader->loadUniforms();
  fontshader->loadAttribs();

  //Default MSAA level for off-screen FBO render
  samples = 1; //8;
}

void RenderContext::push()
{
  MV_stack.push_back(MV);
  P_stack.push_back(P);
}

void RenderContext::pop()
{
  MV = MV_stack.back();
  P = P_stack.back();
  MV_stack.pop_back();
  P_stack.pop_back();
}

void RenderContext::scale(float factor)
{
  //Uniform scaling
  MV = linalg::mul(MV, linalg::scaling_matrix(vec3(factor, factor, factor)));
}

void RenderContext::scale3(float x, float y, float z)
{
  MV = linalg::mul(MV, linalg::scaling_matrix(vec3(x, y, z)));
}

void RenderContext::translate3(float x, float y, float z)
{
  MV = linalg::mul(MV, linalg::translation_matrix(vec3(x, y, z)));
}

void RenderContext::useDefaultShader(bool is2d, bool flat)
{
  //Setup default vertex attributes
  int vcount = is2d ? 2 : 3;
  defaultshader->use();
  int stride = vcount * sizeof(float) + sizeof(Colour);   //2d/3d vertices + 32-bit colour
  GLint aPosition = defaultshader->attribs["aVertexPosition"];
  GLint aColour = defaultshader->attribs["aVertexColour"];
  glEnableVertexAttribArray(aPosition);
  glVertexAttribPointer(aPosition, vcount, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0); // Vertex x,y(,z)
  glEnableVertexAttribArray(aColour);
  glVertexAttribPointer(aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(vcount*sizeof(float)));   // rgba, vertex offset
  defaultshader->setUniformMatrixf("uMVMatrix", MV);
  defaultshader->setUniformMatrixf("uPMatrix", P);
  defaultshader->setUniformi("uFlat", flat);
}

void RenderContext::viewport2d(int width, int height)
{
  if (width && height)
  {
    // Set up 2D Viewer the size of the viewport
    glDisable( GL_DEPTH_TEST );

    // Replace matrices
    push();
    //Left, right, bottom, top, near, far
    P = ortho(0.0, (float)width, 0.0, (float)height, -1.0f, 1.0f);
    MV = linalg::identity;
  }
  else
  {
    // Restore
    glEnable( GL_DEPTH_TEST );
    pop();
  }
}

int RenderContext::project(float objx, float objy, float objz, float *windowCoordinate)
{
  int viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  return project(objx, objy, objz, viewport, windowCoordinate);
}

int RenderContext::project(float x, float y, float z, int* viewport, float *windowCoordinate)
{
  vec4 in(x, y, z, 1.0);
  //Modelview transform
  vec4 a = linalg::mul(MV, in);
  //Projection transform
  vec4 b = linalg::mul(P, a);
  //The result normalizes between -1 and 1
  if (b.w == 0.0) return 0;
  //Perspective division
  b.x /= b.w;
  b.y /= b.w;
  b.z /= b.w;
  //Window coordinates - map to range [0,1] and map x,y to viewport
  windowCoordinate[0] = (b.x * 0.5 + 0.5) * viewport[2] + viewport[0];
  windowCoordinate[1] = (b.y * 0.5 + 0.5) * viewport[3] + viewport[1];
  windowCoordinate[2] = b.z * 0.5 + 0.5; //This is only correct when glDepthRange(0.0, 1.0)
  return 1;
}

mat4 RenderContext::ortho(float left, float right, float bottom, float top, float nearc, float farc)
{
  //Return orthographic projection matrix
  mat4 M = linalg::identity;
	M[0][0] = 2.f / (right - left);
	M[1][1] = 2.f / (top - bottom);
	M[2][2] = -2.f / (farc - nearc);
	M[3][0] = - (right + left) / (right - left);
	M[3][1] = - (top + bottom) / (top - bottom);
	M[3][2] = - (farc + nearc) / (farc - nearc);
  return M;
}

