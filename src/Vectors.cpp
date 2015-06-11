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

Vectors::Vectors() : TriSurfaces()
{
   type = lucVectorType;
}

Vectors::~Vectors()
{
}

void Vectors::update()
{
   //Convert vectors to triangles
   clock_t t1,t2,tt;
   tt=clock();
   int tot = 0;
   float minR = view->model_size * 0.0001; //Minimum radius for visibility
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      //Clear existing vertex related data
      tot += geom[i]->positions.size()/3;
      geom[i]->count = 0;
      geom[i]->data[lucVertexData]->clear();
      geom[i]->data[lucIndexData]->clear();
      geom[i]->data[lucTexCoordData]->clear();
      geom[i]->data[lucNormalData]->clear();

      vertex_index = 0; //Reset current index

      float arrowHead = geom[i]->draw->properties["arrowhead"].ToFloat(2.0);

      //Dynamic range?
      float scaling = scale * geom[i]->draw->properties["scaling"].ToFloat(1.0);

      if (geom[i]->vectors.maximum > 0) 
      {
         debug_print("[Adjusted vector scaling from %.2e by %.2e to %.2e ]\n", 
               scaling, 1/geom[i]->vectors.maximum, scaling/geom[i]->vectors.maximum);
         scaling *= 1.0/geom[i]->vectors.maximum;
      }

      //Load scaling factors from properties
      int quality = glyphSegments(geom[i]->draw->properties["glyphs"].ToInt(2));
      scaling *= geom[i]->draw->properties["length"].ToFloat(1.0);
      //debug_print("Scaling %f arrowhead %f quality %d %d\n", scaling, arrowHead, glyphs);

      //Default (0) = automatically calculated radius
      float radius = scale * geom[i]->draw->properties["radius"].ToFloat(0.0);
      if (radius < minR) radius = minR;

      if (scaling <= 0) scaling = 1.0;

      //for (int v=0; v < geom[i]->count; v++) 
      for (int v=0; v < geom[i]->positions.size()/3; v++) 
      {
         //Calculate colour
         //geom[i]->setColour(v);

         //Scale position & vector manually (as global scaling is disabled to avoid distorting glyphs)
         Vec3d pos(geom[i]->positions[v]);
         Vec3d vec(geom[i]->vectors[v]);
         if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
         {
            Vec3d scale(view->scale);
            pos *= scale;
            vec *= scale;
         }

         //Combine model vector scale with user vector scale factor
         //drawVector3d(pos, vec, scaling, radius, arrowHead, quality, NULL, NULL );
         drawVector(geom[i], pos.ref(), vec.ref(), scaling, radius, radius, arrowHead, quality);
         vertex_index = geom[i]->count; //Reset current index to match vertex count
      }
   }
   t1 = clock(); debug_print("Plotted %d vector arrows in %.4lf seconds\n", tot, (t1-tt)/(double)CLOCKS_PER_SEC);
   elements = -1;
   TriSurfaces::update();
}

void Vectors::draw()
{
   // Undo any scaling factor for arrow drawing...
   glPushMatrix();
   if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
      glScalef(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);

   TriSurfaces::draw();

   // Re-Apply scaling factors
   glPopMatrix();
}
