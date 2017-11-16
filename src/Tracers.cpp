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

Tracers::Tracers(DrawState& drawstate) : Glyphs(drawstate)
{
  type = lucTracerType;
}

void Tracers::update()
{
  //Convert tracers to triangles/lines
  lines->clear();
  tris->clear();
  points->clear();
  //All tracers stored as single vertex/value block
  //Contains vertex/value for every tracer particle at each timestep
  //Number of particles is number of entries divided by number of timesteps
  Vec3d scale(view->scale);
  tris->unscale = view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0;
  tris->iscale = Vec3d(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);
  for (unsigned int i=0; i<geom.size(); i++)
  {
    Properties& props = geom[i]->draw->properties;
    bool filter = geom[i]->draw->filterCache.size();

    //Create a new data stores for output geometry
    tris->add(geom[i]->draw);
    lines->add(geom[i]->draw);
    points->add(geom[i]->draw);

    //Calculate particle count using data count / data steps
    unsigned int particles = geom[i]->width;
    if (particles == 0)
    {
      printf("WARNING: No particles to trace\n");
      continue;
    }

    int count = geom[i]->count();
    int datasteps = count / particles;
    int timesteps = (datasteps-1) * drawstate.gap + 1; //Multiply by gap between recorded steps

    //Per-Swarm step limit
    int drawSteps = props["steps"];
    if (drawSteps > 0 && timesteps > drawSteps)
      timesteps = drawSteps;

    //Get start and end indices
    int range = timesteps;
    //Skipped steps? Use closest available step
    if (drawstate.gap > 1) range = ceil(timesteps/(float)(drawstate.gap-1));
    int end = drawstate.now;      //Finish at current step;
    int start = end - range + 1;  //First step
    if (start < 0) start = 0;
    debug_print("Tracing %d positions from step indices %d to %d (timesteps %d datasteps %d)\n", particles, start, end, timesteps, datasteps);

    //Calibrate colour maps on timestep if no value data
    bool timecolour = false;
    //Calibrate colour map on provided value range
    ColourLookup& getColour = geom[i]->colourCalibrate();
    ColourMap* cmap = geom[i]->draw->colourMap;
    if (cmap && !geom[i]->colourData())
    {
      timecolour = true;
      cmap->calibrate(drawstate.timesteps[start]->time, drawstate.timesteps[end]->time);
    }

    //Get properties
    bool taper = props["taper"];
    bool fade = props["fade"];
    int quality = 4 * (int)props["glyphs"];
    float size0 = 0.001 * (float)props["scaling"];
    float limit = props.getFloat("limit", view->model_size * 0.3);
    float factor = props["scaling"];
    float scaling = props["scaletracers"];
    factor *= scaling * drawstate.gap * 0.0005;
    float arrowSize = props["arrowhead"];
    bool flat = props["flat"] || quality < 1;
    bool connect = props["connect"];
    //Iterate individual tracers
    float size = 0;
    for (unsigned int p=0; p < particles; p++)
    {
      float* oldpos = NULL;
      Colour colour, oldColour;
      float radius, oldRadius = 0;
      size = size0;
      //Loop through time steps
      for (int step=start; step <= end; step++)
      {
        // Scale up line towards head of trajectory
        if (taper && step > start) size += factor;

        //Lookup by provided particle index?
        int pidx = p;
        if (geom[i]->render->indices.size() > 0)
        {
          for (unsigned int x=0; x<particles; x++)
          {
            if (geom[i]->render->indices[step * particles + x] == p)
            {
              pidx = x;
              break;
            }
          }
        }

        //Filtering
        int pp = step * particles + pidx;
        if (!drawable(i) || (filter && geom[i]->filter(pp))) continue;

        float* pos = geom[i]->render->vertices[pp];
        //printf("p %d step %d POS = %f,%f,%f\n", p, step, pos[0], pos[1], pos[2]);

        //Get colour either from supplied colour values or time step
        if (timecolour)
          colour = cmap->getfast(drawstate.timesteps[step]->time);
        else if ((unsigned int)geom[i]->colourCount() > particles)
          getColour(colour, pp); //Have a colour value per particle and ste;p
        else if ((unsigned int)geom[i]->colourCount() <= particles)
          getColour(colour, p);  //Fixed colour value per particle regardless of step

        //Fade out
        if (fade) colour.a = 255 * (step-start) / (float)(end-start);

        radius = scaling * size;

        //Un-connected? Draw points at each position only
        if (!connect)
        {
          points->read(geom[i]->draw, 1, lucVertexData, pos);
          points->read(geom[i]->draw, 1, lucRGBAData, &colour);
        }
        // Draw connected section
        else if (oldpos)
        {

          if (flat)
          {
            if (limit == 0.f || (Vec3d(pos) - Vec3d(oldpos)).magnitude() <= limit)
            {
              lines->read(geom[i]->draw, 1, lucVertexData, oldpos);
              lines->read(geom[i]->draw, 1, lucVertexData, pos);
              lines->read(geom[i]->draw, 1, lucRGBAData, &oldColour);
              lines->read(geom[i]->draw, 1, lucRGBAData, &colour);
            }
          }
          else
          {
            //Coord scaling passed to drawTrajectory (as global scaling disabled to avoid distorting glyphs)
            float arrowHead = -1;
            if (step == end) arrowHead = arrowSize; //geom[i]->draw->properties["arrowhead"].ToFloat(2.0);
            int diff = tris->getVertexIdx(geom[i]->draw);
            tris->drawTrajectory(geom[i]->draw, oldpos, pos, oldRadius, radius, arrowHead, view->scale, limit, quality);
            diff = tris->getVertexIdx(geom[i]->draw) - diff;
            //Per vertex colours
            for (int c=0; c<diff; c++)
            {
              //Top of shaft and arrowhead use current colour, others (base) use previous
              //(Every second vertex is at top of shaft, first quality*2 are shaft verts)
              Colour& col = oldColour;
              if (c%2==1 || c > quality*2) col = colour;
              tris->read(geom[i]->draw, 1, lucRGBAData, &col);
            }
          }
        }

        //oldtime = time;
        oldpos = pos;
        oldRadius = radius;
        oldColour = colour;
      }
    }
    if (taper) debug_print("Tapered tracers from %f to %f (step %f)\n", size0, size, factor);

    //Adjust bounding box
    //tris->compareMinMax(geom[i]->min, geom[i]->max);
    //lines->compareMinMax(geom[i]->min, geom[i]->max);
  }
  GL_Error_Check;

  Glyphs::update();
}


