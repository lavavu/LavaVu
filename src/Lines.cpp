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

Lines::Lines() : Geometry()
{
   type = lucLineType;
}

Lines::~Lines()
{
}

void Lines::update()
{
   if (drawcount == 0) return;
   Geometry::update();

   for (unsigned int i=0; i<geom.size(); i++) 
   {
      glNewList(displaylists[i], GL_COMPILE);
      if (!drawable(i)) {glEndList(); continue;}   ////

      //Disable depth test on 2d models
      if (view->is3d)
         glEnable(GL_DEPTH_TEST);
      else
         glDisable(GL_DEPTH_TEST);

      //Calibrate colour map on range for this line
      geom[i]->colourCalibrate();

      //Set draw state
      setState(i);
      glPushAttrib(GL_ENABLE_BIT);
      
      //Default is to render 2d lines
      if (geom[i]->draw->properties["flat"].ToBool(true))
      {
         glDisable(GL_LIGHTING); //Turn off lighting (for databases without properties exported)
      
         if (geom[i]->draw->properties["link"].ToBool(false))
            glBegin(GL_LINE_STRIP);
         else
            glBegin(GL_LINES);
        
         for (int j=0; j < geom[i]->count; j++) 
         {
            geom[i]->setColour(j);
            glVertex3fv(geom[i]->vertices[j]);
         }
        
         glEnd();
      }
      else
      {
         float* oldpos = NULL;
         for (int j=0; j < geom[i]->count; j++) 
         {
            if (j%2 == 0 && !geom[i]->draw->properties["link"].ToBool(false)) oldpos = NULL;
            Colour colour;
            geom[i]->getColour(colour, j);
            float* pos = geom[i]->vertices[j];
            drawTrajectory_(oldpos, pos, 0.15, -1, 8, view->scale, &colour, &colour, view->model_size);
            oldpos = pos;
         }

      }

      glPopAttrib();
      glEndList();
      GL_Error_Check;
   }
}

