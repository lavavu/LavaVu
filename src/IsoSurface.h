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

#ifndef IsoSurface__
#define IsoSurface__
#include "Include.h"
#include "Geometry.h"

#define I_AXIS 0
#define J_AXIS 1
#define K_AXIS 2
typedef struct
{
   float value;
   float pos[3];
   float colourval;
} IVertex;

template <typename T> class array3d 
{
public:
  array3d(size_t x=0, size_t y=0, size_t z=0, T const & t=T())
         : x(x), y(y), z(z), data(x*y*z, t) {}

  T & at(size_t i, size_t j, size_t k)
  {
    return data[i*y*z + j*z + k];
  }

  T const at(size_t i, size_t j, size_t k) const
  {
    return data[i*y*z + j*z + k];
  }

private:
  size_t x, y, z;
  std::vector<T> data;
};

typedef array3d<IVertex> vertices;

class Isosurface
{
public:
  float                        isovalue;
  unsigned int                 resolution[3];
  float                        min[3];
  float                        max[3];
  unsigned int                 nx;
  unsigned int                 ny;
  unsigned int                 nz;
  unsigned int subsample;
  Triangles* surfaces;
  DrawingObject* target;
  FloatValues* colourVals;
  vertices* vertex;

  Isosurface(std::vector<Geom_Ptr>& geom, Triangles* tris, DrawingObject* draw, DrawingObject* target, Volumes* vol, unsigned int subsample=1);

  void MarchingCubes();
  void DrawWalls();
  void MarchingRectangles(IVertex** points, char squareType);
  void WallElement(IVertex** points);
  void VertexInterp(IVertex* point, IVertex* vertex1, IVertex* vertex2);
  void CreateTriangle(IVertex* point1, IVertex* point2, IVertex* point3);
};

#endif //IsoSurface__
