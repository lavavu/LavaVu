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

Shapes::Shapes(bool hidden) : Geometry(hidden)
{
   type = lucShapeType;
}

Shapes::~Shapes()
{
}

void Shapes::update()
{
   Geometry::update();

   for (unsigned int i=0; i<geom.size(); i++) 
   {
      glNewList(displaylists[i], GL_COMPILE);
      if (!drawable(i)) {glEndList(); continue;}   ////

      //Calibrate colour map (not yet available)
      geom[i]->colourCalibrate();

      //Set draw state
      setState(i);

      float scaling = geom[i]->draw->scaling;

      //Load constant scaling factors from properties
      float dims[3];
      dims[0] = geom[i]->draw->props.Float("width", FLT_MIN);
      dims[1] = geom[i]->draw->props.Float("height", FLT_MIN);
      dims[2] = geom[i]->draw->props.Float("length", FLT_MIN);
      int shape = geom[i]->draw->props.Int("shape");

      if (scaling <= 0) scaling = 1.0;

      for (int v=0; v < geom[i]->count; v++) 
      {
         //Calculate colour
         geom[i]->setColour(v);

         //Scale the dimensions by variables (dynamic range options? by setting max/min?)
         float sdims[3] = {dims[0], dims[1], dims[2]};
         if (geom[i]->xWidths.size() > 0) sdims[0] = geom[i]->xWidths[v];
         if (geom[i]->yHeights.size() > 0) sdims[1] = geom[i]->yHeights[v]; else sdims[1] = sdims[0];
         if (geom[i]->zLengths.size() > 0) sdims[2] = geom[i]->zLengths[v]; else sdims[2] = sdims[1];

         //Multiply by constant scaling factors if present
         if (dims[0] != FLT_MIN) sdims[0] *= dims[0];
         if (dims[1] != FLT_MIN) sdims[1] *= dims[1];
         if (dims[2] != FLT_MIN) sdims[2] *= dims[2];

         //Apply scaling
         sdims[0] *= scaling * scale;
         sdims[1] *= scaling * scale;
         sdims[2] *= scaling * scale;

         float pos[3];
         memcpy(pos, geom[i]->vertices[v], 3 * sizeof(float));
         //Scale manually (as global scaling is disabled to avoid distorting glyphs)
         for (int d=0; d<3; d++)
            pos[d] *= view->scale[d];

         // Translate to centre position
         glPushMatrix();
         glTranslatef(pos[0], pos[1], pos[2]);

         //Orient view to the alignment vector
         if (geom[i]->vectors.size() > 0)
         {
            Vec3d vec(geom[i]->vectors[v]);
            vec *= Vec3d(view->scale); //Scale
            // Rotate to orient the cone
            //...Want to align our z-axis to point along arrow vector:
            // axis of rotation = (z x vec)
            // cosine of angle between vector and z-axis = (z . vec) / |z|.|vec| */
            vec.normalise();
            float rangle = RAD2DEG * vec.angle(Vec3d(0.0, 0.0, 1.0));
            //Axis of rotation = vec x [0,0,1] = -vec[1],vec[0],0
            glRotatef(rangle, -vec[1], vec[0], 0);
         }

         //Draw shape
         float zpos[3] = {0};
         if (shape == 1)
            //drawCuboid(min, max, true);
            drawCuboid(zpos, sdims[0], sdims[1], sdims[2], true);
         else
            drawEllipsoid(zpos, sdims[0], sdims[1], sdims[2], 24, NULL);

         //Restore model view
         glPopMatrix();
      }
      glEndList();
   }
}

void Shapes::draw()
{
   // Undo any scaling factor for arrow drawing...
   glPushMatrix();
   if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
      glScalef(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);

   Geometry::draw();

   // Re-Apply scaling factors
   glPopMatrix();
}
