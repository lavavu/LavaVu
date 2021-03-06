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

#include "ColourMap.h"
#include "GraphicsUtil.h"
#include "ViewerTypes.h"
#include "Session.h"

#ifndef DrawingObject__
#define DrawingObject__

typedef std::shared_ptr<ImageLoader> Texture_Ptr;

//Holds parameters for a drawing object
class DrawingObject
{
  Session& session;
public:
  unsigned int dbid;
  bool skip;
  Shader_Ptr shader; //Custom shader

  //Cached values for faster lookup
  bool visible = true;
  float dims[3] = {0., 0., 0.};
  std::string renderer = "";
  float opacity;
  Colour colour;
  unsigned int colourIdx;
  unsigned int opacityIdx;
  float radius_default;
  ColourMap* colourMap; //Cached references
  ColourMap* opacityMap;
  ColourMap* textureMap;
  std::vector<Filter> filterCache;

  //Data min/max values
  std::map<std::string, Range> ranges;

  //Object properties data...
  Properties properties;
  //Default texture
  Texture_Ptr texture;

  DrawingObject(Session& session, std::string name="", std::string props="", unsigned int id=0);
  ~DrawingObject();

  void updateRange(const std::string& label, const Range& newRange);
  ColourMap* getColourMap(const std::string propname="colourmap", ColourMap* current=NULL);
  void setup();
  TextureData* useTexture(Texture_Ptr tex=nullptr);
  std::string name() {return properties["name"];}
};

#endif //DrawingObject__
