/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
** Copyright (c) 2016, Monash University
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

#ifndef Contour__
#define Contour__
#include "Include.h"
#include "Geometry.h"

#define I_AXIS 0
#define J_AXIS 1
#define K_AXIS 2

template <typename T> class array2d 
{
public:
  array2d(size_t x=0, size_t y=0, T const & t=T())
         : x(x), y(y), data(x*y, t) {}

  T & at(size_t i, size_t j)
  {
    return data[i*y + j];
  }

  T const at(size_t i, size_t j) const
  {
    return data[i*y + j];
  }

private:
  size_t x, y;
  std::vector<T> data;
};

typedef array2d<IVertex> vertices2;

class Contour
{
public:
  float                        isovalue;
  unsigned int                 resolution[3];
  float                        min[3];
  float                        max[3];
  unsigned int                 nI;
  unsigned int                 nJ;
  Geometry* lines;
  DrawingObject* target;
  FloatValues* colourVals;
  vertices2* vertex;
  std::string labelformat;
  int printedIndex = -1;
  int coordIndex = 0;

  Contour(std::vector<Geom_Ptr>& geom, Geometry* lines, DrawingObject* draw, DrawingObject* target, Geometry* source);

  void VertexInterp(IVertex* point, IVertex* vertex1, IVertex* vertex2);
  void MarchingRectangles();
  void addVertex(char edge, int aIndex, int bIndex);
};

#endif //Contour__
