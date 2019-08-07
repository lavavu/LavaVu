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

Tracers::Tracers(Session& session) : Glyphs(session)
{
  type = lucTracerType;
}

void Tracers::update()
{
  //Require at least 2 steps to trace
  if (geom.size() < 2) return;

  //Enumerate steps and elements
  int laststep = -2;
  unsigned int t = 0;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    //printf("-- EL %d/%d STEP %d\n", i, geom.size(), geom[i]->step);
    if (laststep > -2 && geom[i]->step != laststep)
      break;
    laststep = geom[i]->step;
    t++;
  }

  unsigned int datasteps = geom.size() / t;
  debug_print("Tracer records %d elements %u stride %u\n", (int)geom.size(), t, datasteps);

  //Convert tracers to triangles/lines
  lines->clear(true);
  tris->clear(true);
  points->clear(true);
  //All tracers stored as single vertex/value block
  //Contains vertex/value for every tracer particle at each timestep
  //Number of particles is number of entries divided by number of timesteps
  for (unsigned int i=0; i<t; i++)
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

    //To get actual timesteps, multiply by gap between recorded steps
    int timesteps = (datasteps-1) * session.gap + 1;

    //Per-Swarm step limit
    int drawSteps = props["steps"];
    if (drawSteps > 0 && timesteps > drawSteps)
      timesteps = drawSteps;

    //Get start and end indices
    int range = timesteps;
    //Skipped steps? Use closest available step
    if (session.gap > 1) range = ceil(timesteps/(float)(session.gap-1));
    int end = session.now;      //Finish at current step;
    int start = end - range + 1;  //First step
    if (start < 0) start = 0;
    debug_print("Tracing %d positions from step indices %d to %d (timesteps %d datasteps %d)\n", particles, start, end, timesteps, datasteps);

    //Calibrate colour maps on timestep if no value data
    bool timecolour = false;
    //Calibrate colour map on provided value range
    ColourLookup& getColour = geom[i]->colourCalibrate();
    //Override opacity property temporarily, or will be applied twice
    geom[i]->draw->opacity = 1.0;
    ColourMap* cmap = geom[i]->draw->colourMap;
    if (cmap && !geom[i]->colourData())
    {
      timecolour = true;
      cmap->calibrate(session.timesteps[start]->time(), session.timesteps[end]->time());
    }

    //Get properties
    bool taper = props["taper"];
    bool fade = props["fade"];
    int quality = 4 * (int)props["glyphs"];
    float size0 = 0.001 * (float)props["scaling"];
    float limit = props.getFloat("limit", view->model_size * 0.3);
    float factor = props["scaling"];
    float scaling = props["scaletracers"];
    factor *= scaling * session.gap * 0.0005;
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

        //Current record, interleaved elements and timesteps
        unsigned int rec = i + step*t;
        if (rec >= geom.size())
        {
          printf("WARNING: Step %d out of range %d\n", rec, (int)geom.size());
          break;
        }

        //Lookup by provided particle index?
        if (geom[rec]->render->indices.size() > 0)
        {
          for (unsigned int x=0; x<particles; x++)
          {
            if (geom[rec]->render->indices[x] == p)
            {
              p = x;
              break;
            }
          }
        }

        //Filtering
        if (!drawable(rec) || (filter && geom[rec]->filter(p))) continue;

        float* pos = geom[rec]->render->vertices[p];
        //printf("rec %d p %d step %d POS = %f,%f,%f\n", rec, p, step, pos[0], pos[1], pos[2]);

        //Get colour either from supplied colour values or time step
        if (timecolour)
          colour = cmap->getfast(session.timesteps[step]->time());
        else if ((unsigned int)geom[rec]->colourCount() == particles)
        {
          //Need to re-init lookup functor to this data block
          FloatValues* vals = geom[rec]->colourData();
          FloatValues* ovals = geom[rec]->valueData(geom[rec]->draw->opacityIdx);
          getColour.init(geom[rec]->draw, geom[rec]->render, vals, ovals);
          getColour(colour, p);  //Fixed colour value per particle regardless of step
        }
        //Only first step has colours, use them
        else if ((unsigned int)geom[0]->colourCount() == particles)
        {
          //Need to re-init lookup functor to this data block
          FloatValues* vals = geom[0]->colourData();
          FloatValues* ovals = geom[0]->valueData(geom[0]->draw->opacityIdx);
          getColour.init(geom[0]->draw, geom[0]->render, vals, ovals);
          getColour(colour, p);  //Fixed colour value per particle regardless of step
        }


        //Fade out
        if (fade) colour.a = 255 * (step-start) / (float)(end-start);

        radius = scaling * size;

        //Un-connected? Draw points at each position only
        if (!connect)
        {
          Geom_Ptr g = points->read(geom[rec]->draw, 0, lucVertexData, NULL);
          g->readVertex(pos);
          g->_colours->read1(colour.value);
        }
        // Draw connected section
        else if (oldpos)
        {

          if (flat)
          {
            if (limit == 0.f || (Vec3d(pos) - Vec3d(oldpos)).magnitude() <= limit)
            {
              Geom_Ptr g = lines->read(geom[rec]->draw, 0, lucVertexData, NULL);
              g->readVertex(oldpos);
              g->readVertex(pos);
              g->_colours->read1(oldColour.value);
              g->_colours->read1(colour.value);
            }
          }
          else
          {
            //Coord scaling passed to drawTrajectory (as global scaling disabled to avoid distorting glyphs)
            float arrowHead = -1;
            if (step == end) arrowHead = arrowSize; //geom[rec]->draw->properties["arrowhead"].ToFloat(2.0);
            Geom_Ptr g = tris->read(geom[rec]->draw, 0, lucVertexData, NULL);
            int diff = g->count();
            tris->drawTrajectory(geom[rec]->draw, oldpos, pos, oldRadius, radius, arrowHead, view->scale, limit, quality);
            diff = g->count() - diff;
            //Per vertex colours
            for (int c=0; c<diff; c++)
            {
              //Top of shaft and arrowhead use current colour, others (base) use previous
              //(Every second vertex is at top of shaft, first quality*2 are shaft verts)
              Colour& col = oldColour;
              if (c%2==1 || c > quality*2) col = colour;
              g->_colours->read1(col.value);
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


