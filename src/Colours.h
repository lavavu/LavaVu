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

//Some predefined rgba colour constants
#define LUC_BLACK          0xff000000
#define LUC_WHITE          0xffffffff
#define LUC_GREEN          0xff00ff00
#define LUC_BLUE           0xffff0000
#define LUC_CYAN           0xffffff00
#define LUC_YELLOW         0xff00ffff
#define LUC_RED            0xff0000ff
#define LUC_GREY           0xff888888
#define LUC_SILVER         0xffcccccc
#define LUC_MIDBLUE        0xffff3333
#define LUC_STRAW          0xff77ffff
#define LUC_ORANGE         0xff0088ff
#define LUC_DARKGREEN      0xff006400
#define LUC_DARKBLUE       0xff00008B
#define LUC_SEAGREEN       0xff2E8B57
#define LUC_YELLOWGREEN    0xff9ACD32
#define LUC_OLIVE          0xff808000
#define LUC_LIGHTBLUE      0xff1E90FF
#define LUC_DARKTURQUOISE  0xff00CED1
#define LUC_BISQUE         0xffFFE4C4
#define LUC_ORANGERED      0xffFF4500
#define LUC_BRICK          0xffB22222
#define LUC_DARKRED        0xff8B0000

//Defines a 32bit colour accessible in multiple ways:
// - rgba 4-byte array
// - r,g,b,a component struct
// - integer (hex AABBGGRR little endian)
// - float for storing in a float data block
typedef union
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
} Colour;

#define printColour(c) printf("RGB:A %d,%d,%d:%d\n",c.r,c.g,c.b,c.a)

Colour Colour_RGBA(int r, int g, int b, int a=255);
Colour parseRGBA(std::string value);

void Colour_SetColour(Colour* colour);
void Colour_Invert(Colour& colour);
Colour Colour_FromJson(json& value, GLubyte red=0, GLubyte green=0, GLubyte blue=0, GLubyte alpha=255);
json Colour_ToJson(int colourval);
json Colour_ToJson(Colour& colour);
void Colour_ToArray(Colour colour, float* array);
void Colour_SetUniform(GLint uniform, Colour colour);

Colour Colour_FromString(std::string str);
Colour Colour_FromX11Colour(std::string x11colour);
Colour Colour_FromHex(std::string hexName);

#endif //Colours__
