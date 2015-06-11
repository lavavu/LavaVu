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

Tracers::Tracers() : TriSurfaces()
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
   //Convert tracers to triangles
   clock_t t1,t2,tt;
   tt=clock();
   //All tracers stored as single vertex/value block
   //Contains vertex/value for every tracer particle at each timestep
   //Number of particles is number of entries divided by number of timesteps
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      //Calculate particle count using data count / data steps
      unsigned int particles = geom[i]->width;
      int count = geom[i]->positions.size() / 3;
      int datasteps = particles > 0 ? count / particles : count;
      int timesteps = (datasteps-1) * TimeStep::gap + 1; //Multiply by gap between recorded steps

      //Clear existing vertex related data
      geom[i]->count = 0;
      geom[i]->data[lucVertexData]->clear();
      geom[i]->ids.read(geom[i]->indices.size(), &geom[i]->indices.value[0]);
      geom[i]->data[lucIndexData]->clear();
      geom[i]->data[lucTexCoordData]->clear();
      geom[i]->data[lucNormalData]->clear();
      //Clear colour values for now, TODO: support supplied colour values as well as auto-calc (will require mapping multiple vertices to colour values)
      geom[i]->data[lucColourValueData]->clear();

      vertex_index = 0; //Reset current index

      //Swarm limit
      int drawSteps = geom[i]->draw->properties["steps"].ToInt(0);
      if (drawSteps > 0 && timesteps > drawSteps)
         timesteps = drawSteps;
      //Global limit
      if (steps > 0 && steps < timesteps)
         timesteps = steps;

      //Get start and end indices
      int max = timesteps;
      //Skipped steps? Use closest available step
      if (TimeStep::gap > 1) max = ceil(timesteps/(float)(TimeStep::gap-1));
      int end = datasteps-1;
      int start = end - max + 1;
      if (start < 0) start = 0;
      debug_print("Tracing %d positions from step indices %d to %d (timesteps %d datasteps %d max %d)\n", particles, start, end, timesteps, datasteps, max);

      //Calibrate colourMap
      geom[i]->colourCalibrate();
      //Calibrate colour maps on timestep if no value data
      bool timecolour = false;
                                              //Force until supplied colour values supported
      if (geom[i]->draw->colourMaps[lucColourValueData])// && geom[i]->colourValue.size() == 0)
      {
         float mintime = TimeStep::timesteps[start]->time;
         float maxtime = TimeStep::timesteps[end]->time;
         //printf("Mintime %f Maxtime %f\n", mintime, maxtime);
         geom[i]->draw->colourMaps[lucColourValueData]->calibrate(mintime, maxtime);
         //Set data min/max
         geom[i]->colourValue.minimum = TimeStep::timesteps[start]->time;
         geom[i]->colourValue.maximum = TimeStep::timesteps[end]->time;
         timecolour = true;
      }

      //Get properties
      int quality = glyphSegments(geom[i]->draw->properties["glyphs"].ToInt(2));
      float size0 = geom[i]->draw->properties["scaling"].ToFloat(1.0) * 0.001;
      float limit = geom[i]->draw->properties["limit"].ToFloat(view->model_size * 0.3);
      float factor = geom[i]->draw->properties["scaling"].ToFloat(1.0) * scale * TimeStep::gap * 0.0005;
      float arrowSize = geom[i]->draw->properties["arrowhead"].ToFloat(2.0);
      float minR = view->model_size * 0.0001; //Minimum radius for visibility
      //Iterate individual tracers
      for (unsigned int p=0; p < particles; p++) 
      {
         float size = size0; 
         float* oldpos = NULL;
         float time, oldtime = TimeStep::timesteps[start]->time;
         float radius, oldRadius = 0;
         for (int step=start; step <= end; step++) 
         {
            // Scale up line towards head of trajectory
            if (scaling && step > start)
            {
               //float factor = geom[i]->draw->properties["scaling"].ToFloat(1.0) * scale * TimeStep::gap * 0.0005;
               if (p==0) debug_print("Scaling tracers from %f by %f to %f\n", size, factor, size+factor);
               size += factor;
            }

            //Lookup by provided particle index?
            int pidx = p;
            if (geom[i]->ids.size() > 0)
            {
               floatidx fidx;
               for (unsigned int x=0; x<particles; x++)
               {
                  fidx.val = geom[i]->ids[step * particles + x];
                  if (fidx.idx == p)
                  {
                     pidx = x;
                     break;
                  }
               }
            }

            //float* pos = geom[i]->vertices[step * particles + pidx];
            float* pos = geom[i]->positions[step * particles + pidx];
            //printf("p %d step %d POS = %f,%f,%f\n", p, step, pos[0], pos[1], pos[2]);

            //Get colour value either from previous colour values or time step
            //if (timecolour)
            time = TimeStep::timesteps[step]->time;
            //geom[i]->getColour(colour, TimeStep::gap * step * particles + pidx);

            /*/ Draw section
            if (flat || geom[i]->draw->properties["flat"].ToBool(false) || quality < 4)
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
            else*/

            //Coord scaling passed to drawTrajectory (as global scaling disabled to avoid distorting glyphs)
            float arrowHead = -1;
            if (step == end) arrowHead = arrowSize; //geom[i]->draw->properties["arrowhead"].ToFloat(2.0);
            radius = scale * size;
            if (radius < minR) radius = minR;
            int diff = geom[i]->count;
            drawTrajectory(geom[i], oldpos, pos, oldRadius, radius, arrowHead, view->scale, limit, quality);
            diff = geom[i]->count - diff;
            vertex_index = geom[i]->count; //Reset current index to match vertex count
            //Per vertex colours
            for (int c=0; c<diff; c++) 
            {
               float t = oldtime;
               //Top of shaft and arrowhead use current colour value, others (base) use previous
               //(Every second vertex is at top of shaft, first quality*2 are shaft verts)
               if (c%2==1 || c > quality*2) t = time;
               read(geom[i], 1, lucColourValueData, &t);
            }

            oldtime = time;
            oldpos = pos;
            oldRadius = radius;
         }
      }

   }
   GL_Error_Check;
   elements = -1;
   TriSurfaces::update();
}

void Tracers::draw()
{
   // Undo any scaling factor for tracer drawing...
   glPushMatrix();
   if (view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0)
      glScalef(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);

   TriSurfaces::draw();

   // Re-Apply scaling factors
   glPopMatrix();
}
