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

Vectors::Vectors(bool hidden) : Geometry(hidden)
{
   glyphs = -1;
   type = lucVectorType;
}

Vectors::~Vectors()
{
}

void Vectors::update()
{
   Geometry::update();

   int tot = 0;
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      glNewList(displaylists[i], GL_COMPILE);
      if (!drawable(i)) {glEndList(); continue;}   ////

      //Calibrate colour map
      geom[i]->colourCalibrate();

      //Set draw state
      setState(i);

      //Dynamic range?
      float scaling = scale * geom[i]->draw->scaling;

      if (geom[i]->vectors.maximum > 0) 
      {
         debug_print("[Adjusted vector scaling from %.2e by %.2e to %.2e ] ", 
               scaling, 1/geom[i]->vectors.maximum, scaling/geom[i]->vectors.maximum);
         scaling *= 1.0/geom[i]->vectors.maximum;
      }
      tot += geom[i]->count;

      //Load scaling factors from properties
      int quality = glyphs;
      if (quality < 0) quality = geom[i]->draw->props.Int("glyphs", 2);
      scaling = geom[i]->draw->props.Float("length", scaling);
      float thickness = geom[i]->draw->lineWidth;
      float radius = 0; 
      if (thickness != 1.0) radius = thickness;

      if (scaling <= 0) scaling = 1.0;

      for (int v=0; v < geom[i]->count; v++) 
      {
         //Calculate colour
         geom[i]->setColour(v);

         //Scale position & vector manually (as global scaling is disabled to avoid distorting glyphs)
         float pos[3], vec[3];
         memcpy(pos, geom[i]->vertices[v], 3 * sizeof(float));
         memcpy(vec, geom[i]->vectors[v], 3 * sizeof(float));
         if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
         {
            for (int d=0; d<3; d++)
            {
               pos[d] *= view->scale[d];
               vec[d] *= view->scale[d];
            }
         }

         if (quality == 0)
            radius = 1.0e-4 / scale;    //Draw as lines

         //Combine model vector scale with user vector scale factor
         drawVector3d(pos, vec, scaling * scale, radius * scale, geom[i]->draw->arrowHead, 4.0*quality, NULL, NULL );
      }
      glEndList();
   }
   debug_print("Plotted %d vector arrows\n", tot);
}

void Vectors::draw()
{
   // Undo any scaling factor for arrow drawing...
   glPushMatrix();
   if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
      glScalef(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);

   Geometry::draw();

   // Re-Apply scaling factors
   glPopMatrix();
}
