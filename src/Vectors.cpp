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

Vectors::Vectors(Session& session) : Glyphs(session)
{
  type = lucVectorType;
}

void Vectors::update()
{
  //Convert vectors to triangles
  lines->clear(true);
  tris->clear(true);
  clock_t t1,tt;
  tt=clock();
  int tot = 0;
  Colour colour;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (geom[i]->render->vectors.size() < geom[i]->count())
    {
      debug_print("Insufficient vector values for vertices %d < %d\n", geom[i]->render->vectors.size(), geom[i]->count());
      continue;
    }

    //Create new data stores for output geometry
    Geom_Ptr tg = tris->add(geom[i]->draw);
    Geom_Ptr lg = lines->add(geom[i]->draw);

    tot += geom[i]->count();

    Properties& props = geom[i]->draw->properties;
    float arrowHead = props["arrowhead"];
    float normalise = props["normalise"];
    float vscaling = props["scaling"];
    float oscaling = props["scalevectors"];
    float fixedlen = props["length"];

    //Scale up by factor of model size order of magnitude
    float order = floor(log10(view->model_size));
    order = pow(10,order);

    //Dynamic range? Skip if has a fixed scaling property
    if (props["autoscale"] && vscaling == 1.0)
    {
      //Check has maximum property
      if (props.has("scalemax"))
        geom[i]->render->vectors.maximum = props["scalemax"];

      //printf("VEC CUR MAX: %f\n", geom[i]->render->vectors.maximum);
      if (geom[i]->render->vectors.maximum == 0.0)
      {
        //Get and store the maximum length
        for (unsigned int v=0; v < geom[i]->count(); v++)
        {
          Vec3d vec(geom[i]->render->vectors[v]);
          float mag = vec.magnitude();

          if (std::isnan(mag) || std::isinf(mag)) continue;
          if (mag > geom[i]->render->vectors.maximum)
            geom[i]->render->vectors.maximum = mag;
        }
      }
      //printf("VEC NEW MAX: %f\n", geom[i]->render->vectors.maximum);
      float autoscale = 0.1/geom[i]->render->vectors.maximum;
      debug_print("[Adjusted vector scaling from %.2e by %.2e to %.2e ] (ORDER %f)\n",
                  vscaling*oscaling, vscaling*oscaling*autoscale, autoscale, order);
      //Replace with the auto scale
      vscaling = autoscale * order;
    }

    //Load scaling factors from properties
    int quality = 4 * (int)props["glyphs"];
    //debug_print("Scaling %f * %f arrowhead %f quality %d\n", vscaling, oscaling, arrowHead, quality);

    //Default (0) = automatically calculated radius based on length and "radius" property
    float radius = props["thickness"];

    ColourLookup& getColour = geom[i]->colourCalibrate();
    //Override opacity property temporarily, or will be applied twice
    geom[i]->draw->opacity = 1.0;
    //Skip colour lookups for just colour property, will be applied later
    Colour* cptr = &getColour == &geom[i]->_getColour ? NULL : &colour;
    bool flat = props["flat"] || quality < 1;
    bool filter = geom[i]->draw->filterCache.size();
    float scaling = vscaling * oscaling;
    if (scaling <= 0) scaling = 1.0;
    for (unsigned int v=0; v < geom[i]->count(); v++)
    {
      if (!drawable(i) || (filter && geom[i]->filter(v))) continue;
      Vec3d pos(geom[i]->render->vertices[v]);
      Vec3d vec(geom[i]->render->vectors[v]);
      if (cptr) getColour(colour, v);

      //Constant length and normalise enabled
      //scale the vectors by their length multiplied by constant length factor
      //when this reaches 1, all vectors are scaled to the same size
      if (fixedlen > 0.0 && normalise > 0.0)
      {
        float len = vec.magnitude();
        vec.normalise();
        if (normalise < 1.0)
          scaling = oscaling * normalise * fixedlen + (1.0 - normalise) * (len * vscaling);
        else
          scaling = oscaling * fixedlen;
      }
      //Always draw the lines so when zoomed out shaft visible (prevents visible boundary between 2d/3d renders)
      lines->drawVector(geom[i]->draw, pos.ref(), vec.ref(), true, scaling, 0, radius, arrowHead, 0, cptr);
      //Per arrow colours (can do this as long as sub-renderer always outputs same tri count)
      //lg->_colours->read1(colour.value);

      if (!flat)
      {
        tris->drawVector(geom[i]->draw, pos.ref(), vec.ref(), true, scaling, 0, radius, arrowHead, quality, cptr);
        //Per arrow colours (can do this as long as sub-renderer always outputs same tri count)
        //tg->_colours->read1(colour.value);
      }

    }

    //Adjust bounding box
    //tris->compareMinMax(geom[i]->min, geom[i]->max);
    //lines->compareMinMax(geom[i]->min, geom[i]->max);
  }
  t1 = clock();
  debug_print("Plotted %d vector arrows in %.4lf seconds\n", tot, (t1-tt)/(double)CLOCKS_PER_SEC);

  Glyphs::update();
}

