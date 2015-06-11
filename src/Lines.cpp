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

Lines::Lines()
{
   type = lucLineType;
   vbo = 0;
}

Lines::~Lines()
{
}

void Lines::update()
{
   //Skip update if count hasn't changed
   if (elements > 0 && total == elements) return;

   //Copy data to Vertex Buffer Object
   // VBO - copy normals/colours/positions to buffer object 
   unsigned char *p, *ptr;
   ptr = p = NULL;
   int datasize = sizeof(float) * 3 + sizeof(Colour);   //Vertex(3), and 32-bit colour
   int bsize = total * datasize;

   //Initialise vertex buffer
   if (vbo) glDeleteBuffers(1, &vbo);
   
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   if (glIsBuffer(vbo))
   {
      glBufferData(GL_ARRAY_BUFFER, bsize, NULL, GL_STATIC_DRAW);
      debug_print("  %d byte VBO created for LINES, holds %d vertices\n", bsize, bsize/datasize);
      ptr = p = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
      GL_Error_Check;
   }
   if (!p) abort_program("VBO setup failed");

   clock_t t1,t2,tt;
   tt=clock();
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      vertex_index = 0; //Reset current index

       t1=tt=clock();

       //Calibrate colour maps on range for this surface
       geom[i]->colourCalibrate();
       int hasColours = geom[i]->colourCount();
       int colrange = hasColours ? geom[i]->count / hasColours : 1;
       bool vertColour = hasColours && colrange > 1;
       debug_print("Using 1 colour per %d vertices (%d : %d)\n", colrange, geom[i]->count, hasColours);

       Colour colour;
       float zero[3] = {0,0,0};
       for (unsigned int v=0; v < geom[i]->count; v++)
       {
          //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
          int cidx = v / colrange;
          if (cidx >= hasColours) cidx = hasColours - 1;
          geom[i]->getColour(colour, cidx);
          //Write vertex data to vbo
          assert((int)(ptr-p) < bsize);
          //Copies vertex bytes
          memcpy(ptr, &geom[i]->vertices[v][0], sizeof(float) * 3);
          ptr += sizeof(float) * 3;
          //Copies colour bytes
          memcpy(ptr, &colour, sizeof(Colour));
          ptr += sizeof(Colour);
       }
       t2 = clock(); debug_print("  %.4lf seconds to reload %d vertices\n", (t2-t1)/(double)CLOCKS_PER_SEC, geom[i]->count); t1 = clock();
       elements += geom[i]->count; 
   }

   glUnmapBuffer(GL_ARRAY_BUFFER);
   GL_Error_Check;
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   t1 = clock(); debug_print("Plotted %d lines in %.4lf seconds\n", total, (t1-tt)/(double)CLOCKS_PER_SEC);
}

void Lines::draw()
{
   //Draw, calls update when required
   Geometry::draw();
   if (drawcount == 0) return;

   GL_Error_Check;
   // Draw using vertex buffer object
   clock_t t0 = clock();
   double time;
   int stride = 3 * sizeof(float) + sizeof(Colour);   //3+3+2 vertices, normals, texCoord + 32-bit colour
   if (geom.size() > 0 && elements > 0 && glIsBuffer(vbo))
   {
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glVertexPointer(3, GL_FLOAT, stride, (GLvoid*)0); // Load vertex x,y,z only
      glColorPointer(4, GL_UNSIGNED_BYTE, stride, (GLvoid*)(3*sizeof(float)));   // Load rgba, offset 3 float
      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_COLOR_ARRAY);

      //Disable depth test on 2d models
      if (view->is3d)
         glEnable(GL_DEPTH_TEST);
      else
         glDisable(GL_DEPTH_TEST);

      int offset = 0;
      for (unsigned int i=0; i<geom.size(); i++) 
      {
         if (drawable(i))
         {
            //Set draw state
            setState(i);
            glPushAttrib(GL_ENABLE_BIT);
         
            glDisable(GL_LIGHTING); //Turn off lighting (for databases without properties exported)

            if (geom[i]->draw->properties["link"].ToBool(false))
               glDrawArrays(GL_LINE_STRIP, offset, geom[i]->count);
            else
               glDrawArrays(GL_LINES, offset, geom[i]->count);
         }

         offset += geom[i]->count;
      }

      glDisableClientState(GL_VERTEX_ARRAY);
      glDisableClientState(GL_COLOR_ARRAY);
   }
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   GL_Error_Check;

   //Restore state
   glPopAttrib();
   //glEnable(GL_LIGHTING);
   //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   //glDisable(GL_CULL_FACE);
   glBindTexture(GL_TEXTURE_2D, 0);

   time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
   if (time > 0.05)
     debug_print("  %.4lf seconds to draw lines\n", time);
   GL_Error_Check;
}

