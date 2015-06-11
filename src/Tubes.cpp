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

#include "Geometry.h"

Tubes::Tubes() : TriSurfaces()
{
   type = lucTubeType;
}

Tubes::~Tubes()
{
}

void Tubes::update()
{
   //Convert to triangles
   clock_t t1,t2,tt;
   tt=clock();
   total = 0;
   int tot = 0;
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      geom[i]->draw->properties["lit"] = true; //Override lit
      vertex_index = 0; //Reset current index
      if (!drawable(i)) continue;

      //Clear existing vertex related data
      tot += geom[i]->positions.size()/3;
      geom[i]->count = 0;
      geom[i]->data[lucVertexData]->clear();
      geom[i]->data[lucIndexData]->clear();
      geom[i]->data[lucTexCoordData]->clear();
      geom[i]->data[lucNormalData]->clear();

      //Draw as 3d cylinder sections
      int quality = glyphSegments(geom[i]->draw->properties["glyphs"].ToInt(2));
      float radius = scale*0.01;
      float* oldpos = NULL;
      for (int v=0; v < geom[i]->positions.size()/3; v++) 
      {
         if (v%2 == 0 && !geom[i]->draw->properties["link"].ToBool(false)) oldpos = NULL;
         Colour colour;
         geom[i]->getColour(colour, v);
         float* pos = geom[i]->positions[v];
         drawTrajectory(geom[i], oldpos, pos, radius, radius, -1, view->scale, HUGE_VAL, quality);
         oldpos = pos;
            vertex_index = geom[i]->count; //Reset current index to match vertex count
      }
   }

   elements = -1;
   TriSurfaces::update();
}

