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

Lines::Lines(DrawState& drawstate) : Geometry(drawstate)
{
  type = lucLineType;
  vbo = 0;
  linetotal = 0;
}

Lines::~Lines()
{
}

void Lines::close()
{
  if (!drawstate.global("gpucache"))
  {
    if (vbo)
      glDeleteBuffers(1, &vbo);

    reload = true;
  }
}

void Lines::update()
{
  //Skip update if count hasn't changed
  //To force update, set geometry->reload = true
  if (reload) elements = 0;
  if (elements > 0 && (linetotal == (unsigned int)elements || total == 0)) return;

  //Count lines
  linetotal = 0;
  for (unsigned int i=0; i<geom.size(); i++)
    linetotal += geom[i]->count;
  if (linetotal == 0) return;

  //Copy data to Vertex Buffer Object
  // VBO - copy normals/colours/positions to buffer object
  unsigned char *p, *ptr;
  ptr = p = NULL;
  int datasize = sizeof(float) * 3 + sizeof(Colour);   //Vertex(3), and 32-bit colour
  int bsize = linetotal * datasize;
  //Initialise vertex buffer
  if (!vbo) glGenBuffers(1, &vbo);
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
  counts.clear();
  counts.resize(geom.size());
  for (unsigned int i=0; i<geom.size(); i++)
  {
    t1=tt=clock();
    Properties& props = geom[i]->draw->properties;

    //Calibrate colour maps on range for this object
    geom[i]->colourCalibrate();

    int hasColours = geom[i]->colourCount();
    int colrange = hasColours ? geom[i]->count / hasColours : 1;
    if (colrange < 1) colrange = 1;
    debug_print("Using 1 colour per %d vertices (%d : %d)\n", colrange, geom[i]->count, hasColours);

    Colour colour;
    bool fastCol = hasColours == geom[i]->colours.size() && hasColours > 0 && !geom[i]->draw->opacityMap;
    for (unsigned int v=0; v < geom[i]->count; v++)
    {
      if (!internal && geom[i]->filter(v)) continue;

      //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
      int cidx = v / colrange;
      if (cidx >= hasColours) cidx = hasColours - 1;
      //Fast lookup for prepared colour data
      //TODO: Replace this (see Geometry::colourCalibrate) with colour lookup function ptr
      if (fastCol)
      {
        colour.value = geom[i]->colours[cidx];
        colour.a *= geom[i]->draw->opacity;
      }
      else
        geom[i]->getColour(colour, cidx);
      //if (cidx%100 ==0) printf("COLOUR %d => %d,%d,%d\n", cidx, colour.r, colour.g, colour.b);

      //Write vertex data to vbo
      assert((int)(ptr-p) < bsize);
      //Copies vertex bytes
      memcpy(ptr, &geom[i]->vertices[v][0], sizeof(float) * 3);
      ptr += sizeof(float) * 3;
      //Copies colour bytes
      memcpy(ptr, &colour, sizeof(Colour));
      ptr += sizeof(Colour);

      //Count of vertices actually plotted
      counts[i]++;
    }
    t2 = clock();
    debug_print("  %.4lf seconds to reload %d vertices\n", (t2-t1)/(double)CLOCKS_PER_SEC, counts[i]);
    t1 = clock();
    elements += counts[i];
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;

  t1 = clock();
  debug_print("Plotted %d lines in %.4lf seconds\n", linetotal, (t1-tt)/(double)CLOCKS_PER_SEC);
}

void Lines::draw()
{
  // Draw using vertex buffer object
  glPushAttrib(GL_ENABLE_BIT);
  clock_t t0 = clock();
  double time;
  int stride = 3 * sizeof(float) + sizeof(Colour);   //3+3+2 vertices, normals, texCoord + 32-bit colour
  int offset = 0;
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

    for (unsigned int i=0; i<geom.size(); i++)
    {
      Properties& props = geom[i]->draw->properties;
      if (drawable(i))
      {
        //Set draw state
        setState(i, drawstate.prog[lucLineType]);

        //Lines specific state
        float scaling = props["scalelines"];
        //Don't apply object scaling to internal lines objects
        if (!internal) scaling *= (float)props["scaling"];
        float lineWidth = (float)props["linewidth"] * scaling * view->scale2d; //Include 2d scale factor
        if (lineWidth <= 0) lineWidth = scaling;
        glLineWidth(lineWidth);

        if (props["link"])
          glDrawArrays(GL_LINE_STRIP, offset, counts[i]);
        else
          glDrawArrays(GL_LINES, offset, counts[i]);
      }

      offset += counts[i];
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;

  //Restore state
  glPopAttrib();
  glBindTexture(GL_TEXTURE_2D, 0);

  time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
  if (time > 0.05)
    debug_print("  %.4lf seconds to draw %d lines\n", time, offset);
  GL_Error_Check;
}

void Lines::jsonWrite(DrawingObject* draw, json& obj)
{
  jsonExportAll(draw, obj);
}
