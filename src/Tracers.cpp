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
#include "TimeStep.h"

Tracers::Tracers(bool hidden) : Geometry(hidden)
{
   type = lucTracerType;
   flat = false;
   scaling = true;
   steps = 0;
   timestep = 0;
}

Tracers::~Tracers()
{
}

void Tracers::update()
{
   Geometry::update();

   //All tracers stored as single vertex/value block
   //Contains vertex/value for every tracer particle at each timestep
   //Number of particles is number of entries divided by number of timesteps
   union
   {
      float val;
      unsigned int idx;
   } fidx;

   for (unsigned int i=0; i<geom.size(); i++) 
   {
      glNewList(displaylists[i], GL_COMPILE);
      if (!drawable(i)) {glEndList(); continue;}   ////

      //Calculate particle count using data count / data steps
      unsigned int particles = geom[i]->width;
      int datasteps = particles > 0 ? geom[i]->count / particles : geom[i]->count;
      int timesteps = (datasteps-1) * TimeStep::gap + 1; //Multiply by gap between recorded steps

      //Swarm limit
      if (geom[i]->draw->steps > 0 && timesteps > geom[i]->draw->steps)
         timesteps = geom[i]->draw->steps;
      //Global limit
      if (steps > 0 && steps < timesteps)
         timesteps = steps;

      /*/Erase data no longer required
      if (timesteps < timestep + 1)
      {
         int start = timestep + 1 - timesteps;
         int data_type;
         for (data_type=lucMinDataType; data_type<lucMaxDataType; data_type++)
         {
            int size = geom[i]->data[data_type]->datasize;
            geom[i]->data[data_type]->erase(0, start*size);
         }
      }*/

      //Get start and end indices
      int max = timesteps;
      //Skipped steps? Use closest available step
      if (TimeStep::gap > 1) max = ceil(timesteps/(float)(TimeStep::gap-1));
      int end = datasteps-1;
      int start = end - max + 1;
      if (start < 0) start = 0;
      debug_print("Tracing from step indices %d to %d (timesteps %d datasteps %d max %d)\n", start, end, timesteps, datasteps, max);

      //Calibrate colourMap
      geom[i]->colourCalibrate();
      //Calibrate colour maps on timestep if no value data
      bool timecolour = false;
      if (geom[i]->draw->colourMaps[lucColourValueData] && geom[i]->colourValue.size() == 0)
      {
         float mintime = Geometry::timesteps[start].time;
         float maxtime = Geometry::timesteps[end].time;
         geom[i]->draw->colourMaps[lucColourValueData]->calibrate(mintime, maxtime);
         timecolour = true;
      }

      //Set draw state
      setState(i);

      //Iterate individual tracers
      for (unsigned int p=0; p < particles; p++) 
      {
         float size = geom[i]->draw->scaling * 0.001;
         if (size <= 0) size = 1.0;

         //Iterate timesteps
         Colour colour, oldColour;
         float* oldpos = NULL;
         for (int step=start; step <= end; step++) 
         {
            // Scale up line towards head of trajectory
            if (scaling && step > start)
            {
               float factor = geom[i]->draw->scaling * scale * TimeStep::gap * 0.0005;
               //if (p==0) debug_print("Scaling tracers from %f by %f to %f\n", size, factor, size+factor);
               size += factor;
            }

            //Lookup by provided particle index?
            int pidx = p;
            if (geom[i]->indices.size() > 0)
            {
               for (unsigned int x=0; x<particles; x++)
               {
                  fidx.val = geom[i]->indices[step * particles + x];
                  if (fidx.idx == p)
                  {
                     pidx = x;
                     break;
                  }
               }
            }

            float* pos = geom[i]->vertices[step * particles + pidx];

            //Get colour, either from colour values or time step
            if (timecolour)
            {
               float time = Geometry::timesteps[step].time;
               colour = geom[i]->draw->colourMaps[lucColourValueData]->getfast(time);
            }
            else
               geom[i]->getColour(colour, TimeStep::gap * step * particles + pidx);

            // Draw section
            if (flat || geom[i]->draw->flat)
            {
               if (step > start)
               {
                  glColor4ubv(colour.rgba);
                  glLineWidth(size * scale);
                  glBegin(GL_LINES);
                  glVertex3fv(oldpos);
                  glVertex3fv(pos);
                  glEnd();
               }
            }
            else
            {
               //Coord scaling passed to drawTrajectory (as global scaling disabled to avoid distorting glyphs)
               float arrowHead = -1;
               if (step == end) arrowHead = geom[i]->draw->arrowHead;
               drawTrajectory(oldpos, pos, scale * size, arrowHead, 8, view->scale, &oldColour, &colour, view->model_size * 0.3);
            }

            oldColour = colour;
            oldpos = pos;
         }
      }
      glEndList();
   }
   GL_Error_Check;
}

void Tracers::draw()
{
   // Undo any scaling factor for tracer drawing...
   glPushMatrix();
   if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
      glScalef(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);

   Geometry::draw();

   // Re-Apply scaling factors
   glPopMatrix();
}
