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

Shapes::Shapes(DrawState& drawstate) : Geometry(drawstate)
{
  type = lucShapeType;
  //Create sub-renderers
  tris = new TriSurfaces(drawstate);
  tris->internal = true;
}

Shapes::~Shapes()
{
  delete tris;
}

void Shapes::close()
{
  tris->close();
}

void Shapes::update()
{
  if (!reload && drawstate.global("gpucache")) return;
  //Convert shapes to triangles
  tris->clear();
  tris->setView(view);
  Vec3d scale(view->scale);
  tris->unscale = view->scale[0] != 1.0 || view->scale[1] != 1.0 || view->scale[2] != 1.0;
  tris->iscale = Vec3d(1.0/view->scale[0], 1.0/view->scale[1], 1.0/view->scale[2]);
  for (unsigned int i=0; i<geom.size(); i++)
  {
    Properties& props = geom[i]->draw->properties;

    //Create a new data store for output geometry
    tris->add(geom[i]->draw);

    float scaling = props["scaling"];

    //Load constant scaling factors from properties
    float dims[3];
    dims[0] = props["shapewidth"];
    dims[1] = props["shapeheight"];
    dims[2] = props["shapelength"];
    int shape = props["shape"];
    int quality = 4 * props.getInt("glyphs", 3);
    //Points drawn as shapes?
    if (!geom[i]->draw->properties.has("shape"))
    {
      dims[0] = dims[1] = dims[2] = (float)props["pointsize"] / 8.0;
      quality = 4 * props.getInt("glyphs", 4);
    }

    if (scaling <= 0) scaling = 1.0;

    Colour colour;
    geom[i]->colourCalibrate();

    unsigned int idxW = geom[i]->valuesLookup(geom[i]->draw->properties["widthby"]);
    unsigned int idxH = geom[i]->valuesLookup(geom[i]->draw->properties["heightby"]);
    unsigned int idxL = geom[i]->valuesLookup(geom[i]->draw->properties["lengthby"]);

    for (unsigned int v=0; v < geom[i]->count; v++)
    {
      if (!drawable(i) || geom[i]->filter(v)) continue;
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
        //Apply scaling, also inverse of model scaling to avoid distorting glyphs
        sdims[c] *= scaling * (float)props["scaleshapes"] * tris->iscale[c];
      }

      //Setup orientation using alignment vector
      Quaternion rot;
      if (geom[i]->vectors.size() > 0)
      {
        Vec3d vec(geom[i]->vectors[v]);
        //vec *= Vec3d(view->scale); //Scale

        // Rotate to orient the shape
        //...Want to align our z-axis to point along arrow vector:
        // axis of rotation = (z x vec)
        // cosine of angle between vector and z-axis = (z . vec) / |z|.|vec| *
        Vec3d rvector(vec);
        rvector.normalise();
        float rangle = RAD2DEG * rvector.angle(Vec3d(0.0, 0.0, 1.0));
        //Axis of rotation = vec x [0,0,1] = -vec[1],vec[0],0
        if (rangle == 180.0)
        {
          rot.y = 1;
          rot.w = 0.0;
        }
        else if (rangle > 0.0)
        {
          rot.fromAxisAngle(Vec3d(-rvector.y, rvector.x, 0), rangle);
        }
        std::cout << vec << " ==> " << rot << std::endl;
      }

      //Create shape
      Vec3d pos = Vec3d(geom[i]->vertices[v]);
      if (shape == 1)
        tris->drawCuboidAt(geom[i]->draw, pos, sdims, rot);
      else
        tris->drawEllipsoid(geom[i]->draw, pos, sdims, rot, quality);

      //Per shape colours (can do this as long as sub-renderer always outputs same tri count per shape)
      geom[i]->getColour(colour, v);
      tris->read(geom[i]->draw, 1, lucRGBAData, &colour.value);
    }

    //Adjust bounding box
    tris->compareMinMax(geom[i]->min, geom[i]->max);
  }

  tris->update();
}

void Shapes::draw()
{
  GL_Error_Check;
  Geometry::draw();
  if (drawcount == 0) return;

  GL_Error_Check;
  // Undo any scaling factor for arrow drawing...
  glPushMatrix();
  if (tris->unscale)
    glScalef(tris->iscale[0], tris->iscale[1], tris->iscale[2]);

  tris->draw();

  // Re-Apply scaling factors
  glPopMatrix();
  GL_Error_Check;
}

void Shapes::jsonWrite(DrawingObject* draw, json& obj)
{
  tris->jsonWrite(draw, obj);
}

