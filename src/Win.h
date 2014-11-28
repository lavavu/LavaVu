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
//Win - handles windows
#ifndef Win__
#define Win__

#include "GraphicsUtil.h"
#include "ColourMap.h"
#include "View.h"

class Win
{
  public:
   std::string name;
   unsigned int id;
   int width, height;
   Colour background;
   float min[3];
   float max[3];
   //List of viewports in this window
   std::vector<View*> views;
   std::vector<DrawingObject*> objects;     // Contains these objects

   Win(std::string name) : name(name), width(800), height(600)
   {
      this->id = ++Win::lastid;
      background.value = 0xff000000;
      min[0] = min[1] = min[2] = 0.0;
      max[0] = max[1] = max[2] = 0.0;
   }

   Win(unsigned int id, std::string name, int w, int h, int bg, float mmin[3], float mmax[3])
       : name(name), id(id), width(w), height(h)
   {
      if (width < 200) width = 200;
      if (height < 200) height = 200;
      if (id == 0) this->id = ++Win::lastid;
      background.value = bg;
      memcpy(min, mmin, 3 * sizeof(float));
      memcpy(max, mmax, 3 * sizeof(float));
   }

   View* addView(View* v);
   void addObject(DrawingObject* obj);
   void initViewports();
   void resizeViewports();

   static unsigned int lastid;
};

#endif //Win__
