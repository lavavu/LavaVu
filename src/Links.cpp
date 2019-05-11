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

Links::Links(Session& session) : Glyphs(session)
{
  type = lucLineType;
  any3d = false;
  primitive = GL_LINES;
}

void Links::update()
{
  if (reload) elements = 0;
  //TODO: fix: Skip update if count hasn't changed
  //To force update, set geometry->reload = true
  //if (elements > 0 && (linetotal == (unsigned int)elements || total == 0)) return;

  tris->clear(true);
  lines->clear(true);

  clock_t t1,t2,tt;
  tt=clock();
  any3d = false;
  elements = 0;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    t1=tt=clock();
    Properties& props = geom[i]->draw->properties;

    //Calibrate colour maps on range for this object
    ColourLookup& getColour = geom[i]->colourCalibrate();
    //Override opacity property temporarily, or will be applied twice
    geom[i]->draw->opacity = 1.0;
    //Skip colour lookups for just colour property, will be applied later
    Colour colour;
    Colour* cptr = &getColour == &geom[i]->_getColour ? NULL : &colour;
    float limit = props["limit"];
    bool linked = props["link"];
    bool looped = props["loop"];
    float lineWidth = (float)props["linewidth"];
    bool filter = geom[i]->draw->filterCache.size();

    unsigned int hasColours = geom[i]->colourCount();
    unsigned int colrange = hasColours ? geom[i]->count() / hasColours : 1;
    if (colrange < 1) colrange = 1;
    debug_print("Using 1 colour per %d vertices (%d : %d)\n", colrange, geom[i]->count(), hasColours);

    //Draw simple 2d lines if flat=true and tubes=false
    //(ie: enable 3d by switching either flag)
    unsigned int VC = geom[i]->render->indices.size() ? geom[i]->render->indices.size() : geom[i]->count();
    if (props["flat"] && !props["tubes"] && lineWidth == 1.0 && session.context.scale2d == 1.0)
    {
      //Create a new segment
      if (linked) lines->add(geom[i]->draw);

      int count = 0;
      //Use indices if available
      for (unsigned int v=0; v < VC; v++)
      {
        if (filter && geom[i]->filter(v)) continue;

        float* V = (geom[i]->render->indices.size() ? &geom[i]->render->vertices[geom[i]->render->indices[v]][0] : &geom[i]->render->vertices[v][0]);

        //Check length limit if applied (used for periodic boundary conditions)
        //NOTE: will not work with linked lines, require separated segments
        if (limit > 0.f && v%2 == 0 && v < VC-1)
        {
          Vec3d line;
          float* V1 = (geom[i]->render->indices.size() ? &geom[i]->render->vertices[geom[i]->render->indices[v+1]][0] : &geom[i]->render->vertices[v+1][0]);
          vectorSubtract(line, V1, V);
          if (line.magnitude() > limit) 
          {
            if (linked)
            {
              //End the linked segment and start a new one
              lines->add(geom[i]->draw);
            }
            else
            {
              //Skip next segment (two vertices)
              v++;
              continue;
            }
          }
        }

        //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
        unsigned int cidx = v / colrange;
        if (cidx >= hasColours) cidx = hasColours - 1;

        Geom_Ptr g = lines->read(geom[i]->draw, 1, lucVertexData, V);

        if (cptr)
        {
          getColour(colour, cidx);
          g->_colours->read1(colour.value);
        }

        //Count of vertices actually plotted
        count++;
      }

      //Plot start vertex again
      if (linked && looped)
      {
        //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
        float* V0 = (geom[i]->render->indices.size() ? &geom[i]->render->vertices[geom[i]->render->indices[0]][0] : &geom[i]->render->vertices[0][0]);
        Geom_Ptr g = lines->read(geom[i]->draw, 1, lucVertexData, V0);
        if (cptr)
        {
          getColour(colour, 0);
          g->_colours->read1(colour.value);
        }
        //Count of vertices actually plotted
        count++;
      }

      elements += linked ? count - 1 : count * 0.5;
      t2 = clock();
      debug_print("  %.4lf seconds to reload %d vertices\n", (t2-t1)/(double)CLOCKS_PER_SEC, count);
      t1 = clock();
    }
    else
    {
      any3d = true; //Flag 3d tubes drawn

      //Create a new data store for output geometry
      Geom_Ptr g = tris->add(geom[i]->draw);

      //3d lines - using triangle sub-renderer
      //geom[i]->draw->properties.data["lit"] = true; //Override lit
      //Draw as 3d cylinder sections
      int quality = 4 * (int)props["glyphs"];
      float scaling = props["scalelines"];
      scaling *= (float)props["scaling"];
      lineWidth = lineWidth * scaling; // * session.context.scale2d; //Include 2d scale factor
      if (lineWidth <= 0) lineWidth = scaling;

      float line_scale_factor = 0.2*view->model_size;
      if (!view->is3d) line_scale_factor *= 1.5;

      float radius = 0.0025 * lineWidth * line_scale_factor;
      //printf("LINEWIDTH %f RADIUS %f\n", lineWidth, radius);
      if (!props["tubes"] || props["flat"])
      {
        //Thick lines only mode - use minimum quality
        quality = 4;
      }
      float* oldpos = NULL;
      Colour colour;
      int count = 0;
      for (unsigned int v=0; v < VC; v++)
      {
        if (filter && geom[i]->filter(v)) continue;
        
        if (v%2 == 0 && !linked) oldpos = NULL;
        float* pos = (geom[i]->render->indices.size() ? &geom[i]->render->vertices[geom[i]->render->indices[v]][0] : &geom[i]->render->vertices[v][0]);
        if (oldpos)
        {
          tris->drawTrajectory(geom[i]->draw, oldpos, pos, radius, radius, -1, view->scale, limit, quality);

          //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
          unsigned int cidx = v / colrange;
          if (cidx >= hasColours) cidx = hasColours - 1;

          //Per line colours (can do this as long as sub-renderer always outputs same tri count)
          getColour(colour, cidx);
          g->_colours->read1(colour.value);
        }
        oldpos = pos;

        //Count of vertices actually plotted
        count++;
      }

      //Plot start vertex again
      if (linked && looped)
      {
        //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
        float* pos = (geom[i]->render->indices.size() ? &geom[i]->render->vertices[geom[i]->render->indices[0]][0] : &geom[i]->render->vertices[0][0]);
        if (oldpos)
        {
          tris->drawTrajectory(geom[i]->draw, oldpos, pos, radius, radius, -1, view->scale, limit, quality);
          //Per line colours (can do this as long as sub-renderer always outputs same tri count)
          getColour(colour, 0);
          g->_colours->read1(colour.value);
        }
        //Count of vertices actually plotted
        count++;
      }

      elements += linked ? count - 1 : count * 0.5;

      //Adjust bounding box
      //tris->compareMinMax(geom[i]->min, geom[i]->max);
    }
  }

  t1 = clock();
  debug_print("Plotted %d lines in %.4lf seconds\n", elements, (t1-tt)/(double)CLOCKS_PER_SEC);

  Glyphs::update();
}

void Links::jsonWrite(DrawingObject* draw, json& obj)
{
  lines->jsonWrite(draw, obj);
  //Triangles rendered?
  if (any3d)
    tris->jsonWrite(draw, obj);
}
