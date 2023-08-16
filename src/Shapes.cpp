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

Shapes::Shapes(Session& session) : Glyphs(session)
{
  type = lucShapeType;
}

void Shapes::update()
{
  //Convert shapes to triangles
  tris->clear(true);
  for (unsigned int i=0; i<geom.size(); i++)
  {
    Properties& props = geom[i]->draw->properties;

    //Create a new data store for output geometry
    Geom_Ptr tg = tris->add(geom[i]->draw);

    float scaling = props["scaling"];

    //Load constant scaling factors from properties
    float dims[3];
    dims[0] = props["shapewidth"];
    dims[1] = props["shapeheight"];
    dims[2] = props["shapelength"];
    int shape = defaultshape;
    if (props.has("shape") || props.hasglobal("shape"))
      shape = props["shape"];
    int segments = (int)props["segments"];
    //Points drawn as shapes?
    if (props.has("pointsize") || type == lucPointType)
    {
      //Plotting points as spheres/cuboids, set some defaults
      float scale0 = (float)props["scalepoints"];
      float diam = (float)props["pointsize"] / 8.0 * scale0;
      dims[0] = dims[1] = dims[2] = diam;
      //Lower default sphere quality
      //quality = props.getInt("segments", 16);
      if ((int)props["pointtype"] > 2 && !props.has("specular"))
        props.data["specular"] = 1.0;
      //Plot cuboids?
      if (dynamic_cast<Cuboids*>(this))
        shape = 1;
    }

    //Use radius to set size
    if (props.has("radius"))
      dims[0] = dims[1] = dims[2] = 2.0 * (float)props["radius"];

    //Disable vertex normals for cuboids...
    props.data["vertexnormals"] = (shape != 1);

    if (scaling <= 0) scaling = 1.0;

    Colour colour;
    ColourLookup& getColour = geom[i]->colourCalibrate();
    //Override opacity property temporarily, or will be applied twice
    geom[i]->draw->opacity = 1.0;
    //Skip colour lookups for just colour property, will be applied later
    Colour* cptr = &getColour == &geom[i]->_getColour ? NULL : &colour;

    unsigned int idxW = geom[i]->valuesLookup(geom[i]->draw->properties["widthby"]);
    unsigned int idxH = geom[i]->valuesLookup(geom[i]->draw->properties["heightby"]);
    unsigned int idxL = geom[i]->valuesLookup(geom[i]->draw->properties["lengthby"]);

    bool hasTexture = geom[i]->hasTexture();
    bool filter = geom[i]->draw->filterCache.size();
    for (unsigned int v=0; v < geom[i]->count(); v++)
    {
      if (!drawable(i) || (filter && geom[i]->filter(v))) continue;
      //Scale the dimensions by variables (dynamic range options? by setting max/min?)
      Vec3d sdims = Vec3d(dims[0], dims[1], dims[2]);
      if (geom[i]->valueData(idxW)) sdims[0] = geom[i]->valueData(idxW, v);
      if (geom[i]->valueData(idxH)) sdims[1] = geom[i]->valueData(idxH, v);
      else sdims[1] = sdims[0];
      if (geom[i]->valueData(idxL)) sdims[2] = geom[i]->valueData(idxL, v);
      else sdims[2] = sdims[1];

      //Multiply by constant scaling factors if present
      for (int c=0; c<3; c++)
      {
        if (dims[c] != FLT_MIN) sdims[c] *= dims[c];
        //Apply scaling
        sdims[c] *= scaling * (float)props["scaleshapes"];
      }

      //Scale position & vector manually (global scaling is disabled to avoid distorting glyphs)
      //Vec3d scale = Vec3d(view->scale);
      Vec3d pos = Vec3d(geom[i]->render->vertices[v]);

      //Setup orientation using alignment vector
      Quaternion rot;
      if (geom[i]->render->vectors.size() > 0)
      {
        Vec3d vec(geom[i]->render->vectors[v]);
        rot = vectorRotation(vec);
      }
      //Otherwise use the rotation property
      else
      {
        //std::cout << props.data << std::endl;
        if (props.has("rotation"))
        {
          json jrot = props["rotation"];
          //std::cout << "JROT: " << jrot << std::endl;
          rot = rotationFromProperty(jrot);
        }
        //Default rotation for spheres for texturing
        else if (shape == 0 && hasTexture)
        {
          //Apply a 90 degree rotation to align equirectangular textures correctly
          //(for textures that map longitudes [-180,180] to texcoords [0,1])
          //(if using a longitudes [0,360] texture, pass "rotation" property == [0,-90,0])
          rot = Quaternion(0, 0.707107, 0, 0.707107); //90 degrees about Y == [0,90,0]
        }
        //std::cout << "QROT: " << rot << std::endl;
      }

      //Create shape
      if (shape == 1)
        tris->drawCuboidAt(geom[i]->draw, pos, sdims, rot, true);
      else
        tris->drawEllipsoid(geom[i]->draw, pos, sdims, rot, true, hasTexture, segments);

      //Per shape colours (can do this as long as sub-renderer always outputs same tri count per shape)
      if (cptr)
      {
        getColour(colour, v);
        tg->_colours->read1(colour.value);
      }
    }

    //Adjust bounding box
    //tris->compareMinMax(geom[i]->min, geom[i]->max);
  }

  Glyphs::update();
}

