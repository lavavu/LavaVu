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

#include "DrawingObject.h"

unsigned int DrawingObject::lastid = 0;

DrawingObject::DrawingObject(unsigned int id, bool persistent, std::string name, int colour, ColourMap* map, float opacity, const char* properties) : id(id), persistent(persistent), name(name), skip(true), visible(true), opacity(opacity)
{
   if (id == 0) this->id = DrawingObject::lastid+1;
   DrawingObject::lastid = this->id;
   colourMaps.resize(lucMaxDataType);
   this->colour.value = colour;
   //Sets the default colour map if provided, newer databases provide separately
   if (map) colourMaps[lucColourValueData] = map;

   std::stringstream propss(properties);
   props.parse(propss, '=');
   lit = props.Bool("lit", true);
   cullface = props.Bool("cullface", false);
   wireframe = props.Bool("wireframe", false);
   lineWidth = props.Float("lineWidth", 1.0);
   pointSize = props.Float("pointSize", 1.0);
   pointType = props.Int("pointType", -1);
   pointSmooth = props.Bool("pointSmooth", true);
   scaling = props.Float("scaling", 1.0);
   arrowHead = props.Float("arrowHead", 2.0);
   flat = props.Bool("flat", false);
   steps = props.Int("steps", 0);
   time = props.Float("time", 0);
   colourbar = props.Bool("colourbar", false);

   texture = NULL;
}

void DrawingObject::addColourMap(ColourMap* map, lucGeometryDataType data_type)
{
   //Sets the colour map for the specified geometry data type
   colourMaps[data_type] = map;
/*
   if (data_type == lucRedValueData) 
      map->setComponent(0);
   if (data_type == lucGreenValueData) 
      map->setComponent(1);
   if (data_type == lucBlueValueData) 
      map->setComponent(2);
*/
}

int DrawingObject::useTexture()
{
   std::string texfn = props["texturefile"];
   if (texfn.length() && !texture)
   {
      texture = new TextureData();
      GLenum mode = GL_REPLACE;
      int data_type;
      for (data_type=lucMinDataType; data_type<lucMaxDataType; data_type++)
      {
         if (colourMaps[data_type])
         {
            mode = GL_MODULATE;
            break;
         }
      }
      //Load textures
      std::string ext = texfn.substr(texfn.find_last_of(".") + 1);
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (ext == "png")
         LoadTexturePNG(texture, texfn.c_str(), true, mode);
      if (ext == "tga")
         LoadTextureTGA(texture, texfn.c_str(), true, mode);
      if (ext == "ppm")
         LoadTexturePPM(texture, texfn.c_str(), true, mode);
   }

   if (texture && texture->width)
   {
      if (texture->depth > 1)
      {
         glEnable(GL_TEXTURE_3D);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_3D, texture->id);
      }
      else
      {
         glEnable(GL_TEXTURE_2D);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_2D, texture->id);
      }
      return 0; //Return unit id
   }

   //No texture:
   glDisable(GL_TEXTURE_2D);
   return -1;
}
