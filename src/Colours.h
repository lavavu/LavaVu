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

#ifndef Colours__
#define Colours__
#include "Include.h"

//Defines a 32bit colour accessible in multiple ways:
// - rgba 4-byte array
// - r,g,b,a component struct
// - integer (hex AABBGGRR little endian)
// - float for storing in a float data block
union Colour
{
  GLubyte rgba[4];
  int value;
  float fvalue;
  struct
  {
    GLubyte r;
    GLubyte g;
    GLubyte b;
    GLubyte a;
  };

  Colour() : r(0), g(0), b(0), a(255) {}
  Colour(GLubyte r, GLubyte g, GLubyte b, GLubyte a=255) : r(r), g(g), b(b), a(a) {}
  Colour(json& jvalue, GLubyte red=0, GLubyte grn=0, GLubyte blu=0, GLubyte alpha=255);
  Colour(const std::string& str);

  void fromRGBA(GLubyte R, GLubyte G, GLubyte B, GLubyte A=255) {r=R;g=G;b=B;a=A;}
  void fromJSON(json& jvalue);
  void fromString(const std::string& str);
  void fromX11Colour(std::string x11colour);
  void fromHex(const std::string& hexName);

  void invert();
  json toJson();
  std::string toString();
  void toArray(float* array);
};

#define printColour(c) printf("RGB:A %d,%d,%d:%d\n",c.r,c.g,c.b,c.a)

//Component conversions, char to float and back
#define _FTOC(c) ((int)round(c*255.f))
#define _CTOF(c) (round(c/2.55f)/100.f)

#endif //Colours__
