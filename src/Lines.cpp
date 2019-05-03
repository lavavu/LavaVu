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

Lines::Lines(Session& session) : Geometry(session)
{
  type = lucLineType;
  idxcount = 0;
  total = 0;
  primitive = GL_LINES;
}

Lines::~Lines()
{
}

void Lines::close()
{
  Geometry::close();
}

unsigned int Lines::lineCount()
{
  total = 0;
  unsigned int drawelements = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    unsigned int v = geom[index]->count();
    //Indexed?
    if (geom[index]->render->indices.size() > 0)
      v = geom[index]->render->indices.size();

    total += v;
    bool hidden = !drawable(index);
    if (!hidden) drawelements += v; //Count drawable
    debug_print(" %s, lines %d hidden? %s\n", geom[index]->draw->name().c_str(), v/2, (hidden ? "yes" : "no"));
  }

  //When objects hidden/shown drawable count changes, so need to reallocate
  if (elements != drawelements)
    counts.clear();

  elements = drawelements;

  return drawelements;
}

void Lines::update()
{
  unsigned int lastcount = total;
  //unsigned int drawelements = lineCount();

  //Only reload the vbo data when required
  //Not needed when objects hidden/shown but required if colours changed
  if (lastcount != total || reload || vbo == 0 || counts.size() == 0)
  {
    //Send the data to the GPU via VBO
    loadBuffers();
  }
}

void Lines::loadBuffers()
{
  //Skip update if count hasn't changed
  //To force update, set geometry->reload = true
  if (reload) elements = 0;
  //if (elements > 0 && (total == (unsigned int)elements)) return;

  //Count lines
  unsigned int drawelements = lineCount();
  if (drawelements == 0) return;

  if (reload) idxcount = 0;

  //Copy data to Vertex Buffer Object
  // VBO - copy normals/colours/positions to buffer object
  int datasize = sizeof(float) * 3 + sizeof(Colour);   //Vertex(3), and 32-bit colour
  int bsize = total * datasize;

  //Create intermediate buffer
  unsigned char* buffer = new unsigned char[bsize];
  unsigned char *ptr = buffer;

  clock_t t1,t2,tt;
  tt=clock();
  //Element counts to actually plot (exclude filtered/hidden) per geom entry
  counts.clear();
  counts.resize(geom.size());
  for (unsigned int i=0; i<geom.size(); i++)
  {
    t1=tt=clock();

    //Calibrate colour maps on range for this object
    ColourLookup& getColour = geom[i]->colourCalibrate();

    Properties& props = geom[i]->draw->properties;
    float limit = props["limit"];
    bool linked = props["link"];
    if (linked) limit = 0.f;

    unsigned int hasColours = geom[i]->colourCount();
    unsigned int colrange = hasColours ? geom[i]->count() / hasColours : 1;
    if (colrange < 1) colrange = 1;
    debug_print("Using 1 colour per %d vertices (%d : %d)\n", colrange, geom[i]->count(), hasColours);

    Colour colour;
    bool filter = geom[i]->draw->filterCache.size();
    for (unsigned int v=0; v < geom[i]->count(); v++)
    {
      if (filter && !internal && geom[i]->filter(v)) continue;

      //Check length limit if applied (used for periodic boundary conditions)
      //Works only if not plotting linked lines
      if (limit > 0.f && v%2 == 0 && v < geom[i]->count()-1)
      {
        Vec3d line;
        vectorSubtract(line, geom[i]->render->vertices[v+1], geom[i]->render->vertices[v]);
        if (line.magnitude() > limit)
        {
          //Skip next segment (two vertices)
          v++;
          continue;
        }
      }

      //Have colour values but not enough for per-vertex, spread over range (eg: per segment)
      unsigned int cidx = v / colrange;
      if (cidx >= hasColours) cidx = hasColours - 1;
      getColour(colour, cidx);
      //if (cidx%100 ==0) printf("COLOUR %d => %d,%d,%d\n", cidx, colour.r, colour.g, colour.b);

      //Write vertex data to vbo
      assert((int)(ptr-buffer) < bsize);
      //Copies vertex bytes
      memcpy(ptr, &geom[i]->render->vertices[v][0], sizeof(float) * 3);
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
    //Indexed
    if (geom[i]->render->indices.size() > 0)
      elements += geom[i]->render->indices.size();
    else
      elements += counts[i]; //geom[i]->count();
  }

  //Initialise vertex array object for OpenGL 3.2+
  if (!vao) glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  //Initialise vertex buffer
  if (!vbo) glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  if (glIsBuffer(vbo))
  {
    glBufferData(GL_ARRAY_BUFFER, bsize, buffer, GL_STATIC_DRAW);
    debug_print("  %d byte VBO created for LINES, holds %d vertices\n", bsize, bsize/datasize);
    //ptr = p = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    GL_Error_Check;
  }

  delete[] buffer;
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;

  t1 = clock();
  debug_print("Plotted %d lines in %.4lf seconds\n", total, (t1-tt)/(double)CLOCKS_PER_SEC);
}

//Reloads line indices
void Lines::render()
{
  clock_t t1,t2;
  t1 = clock();
  if (elements == 0 || counts.size() == 0) return;

  //Prepare the Index buffer
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
  glBindVertexArray(vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  GL_Error_Check;
  if (glIsBuffer(indexvbo))
  {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
    debug_print("  %d byte IBO prepared for %d indices\n", elements * sizeof(GLuint), elements);
  }
  else
    abort_program("IBO creation failed\n");
  GL_Error_Check;

  //Upload vertex indices
  unsigned int offset = 0;
  unsigned int voffset = 0;
  idxcount = 0;
  assert(counts.size() == geom.size());
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    unsigned int indices = geom[index]->render->indices.size();
    if (indices) counts[index] = 0; //Reset count
    if (drawable(index))
    {
      if (indices > 0)
      {
        //Create the index list, adding offset from previous element vertices
        std::vector<unsigned int> indexlist(indices);
        for (unsigned int i=0; i<indices; i++)
          indexlist[i] = voffset + geom[index]->render->indices[i];

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof(GLuint), indices * sizeof(GLuint), indexlist.data());
        //printf("%d upload %d indices, voffset %d\n", index, indices, voffset);
        counts[index] = indices;
        offset += indices;
        GL_Error_Check;
      }
      //For vertices only, just use the existing vertex count
      idxcount += counts[index];
    }

    //Vertex index offset
    voffset += geom[index]->count();
  }

  GL_Error_Check;
  t2 = clock();
  debug_print("  %.4lf seconds to upload %d indices\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount);
  t1 = clock();
  //After render(), elements holds unfiltered count, idxcount is filtered
  elements = idxcount;
}

void Lines::draw()
{
  //Re-render if count changes
  if (idxcount != elements) render();

  setState(0); //Set global draw state (using first object)
  Shader_Ptr prog = session.shaders[lucLineType];

  // Draw using vertex buffer object
  clock_t t0 = clock();
  double time;
  int stride = 3 * sizeof(float) + sizeof(Colour);   //3d vertices + 32-bit colour
  int offset = 0;
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (geom.size() > 0 && elements > 0 && glIsBuffer(vbo))
  {
    //Setup vertex attributes
    GLint aPosition = prog->attribs["aVertexPosition"];
    GLint aColour = prog->attribs["aVertexColour"];
    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0); // Vertex x,y,z
    glEnableVertexAttribArray(aColour);
    glVertexAttribPointer(aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(3*sizeof(float)));   // rgba, offset 3 float

    for (unsigned int i=0; i<geom.size(); i++)
    {
      Properties& props = geom[i]->draw->properties;
      if (drawable(i))
      {
        //Set draw state
        setState(i);

        //Lines specific state
        float scaling = props["scalelines"];
        //Don't apply object scaling to internal lines objects
        if (!internal) scaling *= (float)props["scaling"];
        float lineWidth = (float)props["linewidth"] * scaling * session.context.scale2d; //Include 2d scale factor
        if (lineWidth <= 0) lineWidth = scaling;
        if (session.context.core && lineWidth > 1.0) lineWidth = 1.0;
        glLineWidth(lineWidth);

        if (props["loop"])
          primitive = GL_LINE_LOOP;
        else if (props["link"])
          primitive = GL_LINE_STRIP;

        if (geom[i]->render->indices.size() > 0)
        {
          //Draw with index buffer
          glDrawElements(primitive, counts[i], GL_UNSIGNED_INT, (GLvoid*)(offset*sizeof(GLuint)));
        }
        else
        {
          //Draw directly from vertex buffer
          glDrawArrays(primitive, offset, counts[i]);
        }
      }

      offset += counts[i];
    }
    glDisableVertexAttribArray(aPosition);
    glDisableVertexAttribArray(aColour);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  GL_Error_Check;

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
