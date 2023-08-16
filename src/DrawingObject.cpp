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
#include "Model.h"

DrawingObject::DrawingObject(Session& session, std::string name, std::string props, unsigned int id)
  : session(session), dbid(id), properties(session.globals, session.defaults)
{
  texture = NULL;
  skip = session.global("noload");

  //Fix any names with spaces
  std::replace(name.begin(), name.end(), ' ', '_');

  //Name set in properties overrides that passed from database
  if (name.length())
    properties.data["name"] = name;

  session.parseSet(properties, props);

  //All props now lowercase, fix a couple of legacy camelcase values
  if (properties.has("pointSize")) {properties.data["pointsize"] = properties["pointSize"]; properties.data.erase("pointSize");}

  properties.data["visible"] = visible = true;
  colourIdx = 0; //Default colouring data is first value block
  opacityIdx = MAX_DATA_ARRAYS+1;
  colourMap = opacityMap = textureMap = NULL;
  setup();
}

DrawingObject::~DrawingObject()
{
}

void DrawingObject::updateRange(const std::string& label, const Range& newRange)
{
  if (!newRange.valid()) return;

  //std::cout << name() << " UPDATE RANGE: " << label << " " << newRange << std::endl;

  //If no existing entry, create default
  auto range = Range();
  if (ranges.find(label) != ranges.end())
    range = ranges[label];

  //Replace if data modified
  if (range.update(newRange.minimum, newRange.maximum))
    ranges[label] = range;
}

ColourMap* DrawingObject::getColourMap(const std::string propname, ColourMap* current)
{
  if (session.colourMaps.size() == 0) return NULL;
  json prop = properties[propname];
  bool valid = true;
  if (prop.is_number())
  {
    //Attempt to load by id
    //printf("WARNING: Load colourmap by ID is deprecated, use name\n");
    int cmapid = prop;
    if (cmapid >= 0 && cmapid < (int)session.colourMaps.size())
    {
      ColourMap* cmap = session.colourMaps[cmapid];
      //Replace integer props with names
      properties.data[propname] = cmap->name;
      return cmap;
    }
  }
  else if (prop.is_string())
  {
    //Attempt to load by name
    std::string data = prop;
    if (data.length() == 0)
      return NULL;

    //Search defined colourmaps by name
    for (unsigned int i=0; i < session.colourMaps.size(); i++)
      if (data == session.colourMaps[i]->name)
        return session.colourMaps[i];

    //Not found, assume property is a colourmap data string instead
    if (!current)
    {
      //Create a default colour map
      current = new ColourMap(session, name() + "_" + propname);
      session.colourMaps.push_back(current);
    }

    //Load the data string
    valid = current->loadPalette(data);
  }
  else if (prop.is_array() || prop.is_object())
  {
    if (!current)
    {
      //Create a default colour map
      current = new ColourMap(session, name() + "_" + propname);
      session.colourMaps.push_back(current);
    }

    //Load the data array
    valid = current->loadPaletteJSON(prop);
  }
  else if (prop.is_null())
    return NULL;

  if (current)
    //Replace property data with name of loaded map
    properties.data[propname] = current->name;

  return valid ? current : NULL;
}

void DrawingObject::setup()
{
  //Cache values for faster lookups during draw calls
  visible = properties["visible"];
  Properties::toArray<float>(properties["dims"], dims, 3);
  renderer = properties["renderer"];
  colour = properties["colour"];
  opacity = 1.0;
  if (properties.has("opacity"))
    opacity = properties["opacity"];
  //Convert values (1,255] -> [0,1]
  if (opacity > 1.0) opacity = _CTOF(opacity);
  //Disable opacity if zero or out of range
  if (opacity <= 0.0 || opacity > 1.0) opacity = 1.0; 

  //Cache colourmaps
  colourMap = getColourMap("colourmap", colourMap);
  opacityMap = getColourMap("opacitymap", opacityMap);

  //Properties reserved for colourmaps can be set from any objects that use that map
  //Move any colourmap properties to the colourmap
  if (colourMap)
  {
    for (auto p : session.colourMapProps)
    {
      if (properties.has(p))
      {
        //std::cout << "MOVING PROPERTY TO COLOURMAP: " << p << " : " << properties[p] << std::endl;
        colourMap->properties.data[p] = properties[p];
        //Just copy, don't delete from object so it remains present for editing
        //properties.data.erase(p);
      }
    }
  }

  //Cache the filter data
  //The cache stores filter values so we can avoid
  //hitting the json store for every vertex (very slow)
  filterCache.clear();
  json filters = properties["filters"];
  for (unsigned int i=0; i < filters.size(); i++)
  {
    float min = filters[i]["minimum"];
    float max = filters[i]["maximum"];

    filterCache.push_back(Filter());
    filterCache[i].by = filters[i]["by"];
    filterCache[i].map = filters[i]["map"];
    filterCache[i].out = filters[i]["out"];
    filterCache[i].inclusive = filters[i]["inclusive"];
    if (min > max)
    {
      //Swap and change to an out filter
      filterCache[i].minimum = max;
      filterCache[i].maximum = min;
      filterCache[i].out = !filterCache[i].out;
      //Also flip the inclusive flag
      filterCache[i].inclusive = !filterCache[i].inclusive;
    }
    else
    {
      filterCache[i].minimum = min;
      filterCache[i].maximum = max;
    }
    //For tracer filtering
    if (dims[0] > 0 && dims[1] == 0 && dims[2] == 0)
      filterCache[i].elements = dims[0];
  }

  //Default vector radius ratio
  radius_default = properties["radius"];
}

TextureData* DrawingObject::useTexture(Texture_Ptr tex)
{
  GL_Error_Check;
  //Use/load default texture
  if (tex->empty())
  {
    if (tex->source || !tex->fn.empty())
    {
      //printf("Load texture from existing data\n");
      tex->load();
    }
    else if (texture && !texture->empty())
    {
      //printf("Load texture from cached copy on drawing object\n");
      tex = texture;
    }
    else if (properties.has("texture"))
    {
      //printf("Load texture from property\n");
      std::string texfn = properties["texture"];
      if (texfn.length() > 0 && FileExists(texfn))
      {
        bool flip = properties["fliptexture"];
        tex = texture = std::make_shared<ImageLoader>(texfn, flip);
        tex->load();
      }
      else if (texfn.substr(0,22) == "data:image/png;base64,")
      {
        //LOAD FROM DATA URL (only png for now)
        std::string b64 = texfn.substr(22);
        std::string decoded = base64_decode(b64);
        std::stringstream ss(decoded);
        GLuint channels, width, height;
        unsigned char* png = (unsigned char*)read_png(ss, channels, width, height);

        ImageData* data = new ImageData(width, height, channels, png);
        tex = texture = std::make_shared<ImageLoader>(); //Add a new empty texture container

        texture->load(data);

        delete[] png;
        delete data;
      }
      else
      {
        //Value can be "colourmap" to use palette from colourmap prop
        if (texfn == "colourmap")
          textureMap = getColourMap("colourmap", colourMap);
        //Or can be a literal colourmap list/name/array
        else
          textureMap = getColourMap("texture", textureMap);

        if (textureMap)
        {
          //No texture provided, load the colourmap data as a texture
          if (!textureMap->texture)
            textureMap->loadTexture(tex->repeat); //Repeat enabled? switch by prop
          if (textureMap->texture)
            return textureMap->texture->use();
        }

        if (texfn.length() > 0)
          debug_print("Texture File: %s not found!\n", texfn.c_str());
        //If load failed, skip from now on
        properties.data["texture"] = "";
      }
    }
    //else
    //  printf("Not empty, but found nothing to load!\n");
  }
  //else
  //  printf("Not loading, is empty\n");
  tex->repeat = properties["repeat"];
  tex->filter = properties["texturefilter"];
  tex->flip = properties["fliptexture"];
  //printf("Loaded texture, repeat %d\n", tex->repeat);

  GL_Error_Check;
  return tex->use();
}


