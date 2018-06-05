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

Triangles::Triangles(Session& session) : Geometry(session)
{
  type = lucTriangleType;
  vbo = 0;
  indexvbo = 0;
}

Triangles::~Triangles()
{
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (indexvbo)
    glDeleteBuffers(1, &indexvbo);
  vbo = 0;
  indexvbo = 0;
}


void Triangles::close()
{
  reload = true;
}

unsigned int Triangles::triCount()
{
  //Get triangle count
  total = 0;
  unsigned int drawelements = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    unsigned int tris;
    //Indexed
    if (geom[index]->render->indices.size() > 0)
    {
      //Un-structured tri indices
      tris = geom[index]->render->indices.size() / 3;
      if (tris * 3 != geom[index]->render->indices.size()) // || geom[index]->draw->properties["tristrip"])
        //Tri-strip indices
        tris = geom[index]->render->indices.size() - 2;
      debug_print("Surface (indexed) %d", index);
    }
    //Grid
    else if (geom[index]->width > 0 && geom[index]->height > 0)
    {
      tris = 2 * (geom[index]->width-1) * (geom[index]->height-1);
      debug_print("Grid Surface %d (%d x %d)", index, geom[index]->width, geom[index]->height);
    }
    else
    {
      //Un-structured tri vertices
      tris = geom[index]->count() / 3;
      if (tris * 3 != geom[index]->count()) // || geom[index]->draw->properties["tristrip"])
        //Tri-strip vertices
        tris =  geom[index]->count() - 2;
      debug_print("Surface %d ", index);
    }

    total += tris*3;
    bool hidden = !drawable(index);
    if (!hidden) drawelements += tris*3; //Count drawable
    debug_print(" %s, triangles %d hidden? %s\n", geom[index]->draw->name().c_str(), tris, (hidden ? "yes" : "no"));
  }

  //When objects hidden/shown drawable count changes, so need to reallocate
  if (elements != drawelements)
    counts.clear();

  elements = drawelements;

  return drawelements;
}

void Triangles::update()
{
  // Update triangles...
  unsigned int lastcount = total/3;
  unsigned int drawelements = triCount();
  if (drawelements == 0) return;

  //Only reload the vbo data when required
  //Not needed when objects hidden/shown but required if colours changed
  //if ((lastcount != total/3 && reload) || !tidx)
  if ((lastcount != total/3 && reload) || vbo == 0)
  {
    //Send the data to the GPU via VBO
    loadBuffers();

    //Initial render
    //render();
  }

  if (reload)
    counts.clear();
}

void Triangles::loadBuffers()
{
  //Copy data to Vertex Buffer Object
  clock_t t1,t2,tt;
  tt=t2=clock();

  //Update VBO...
  debug_print("Reloading %d elements...\n", elements);

  // VBO - copy normals/colours/positions to buffer object
  unsigned char *p, *ptr;
  ptr = p = NULL;
  unsigned int datasize = sizeof(float) * 8 + sizeof(Colour);   //Vertex(3), normal(3), texCoord(2) and 32-bit colour
  unsigned int vcount = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
    vcount += geom[index]->count();
  unsigned int bsize = vcount * datasize;

  //Initialise vertex buffer
  if (!vbo) glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  if (glIsBuffer(vbo))
  {
    glBufferData(GL_ARRAY_BUFFER, bsize, NULL, GL_DYNAMIC_DRAW);
    debug_print("  %d byte VBO created, holds %d vertices\n", bsize, bsize/datasize);
    ptr = p = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    GL_Error_Check;
  }
  if (!p) abort_program("VBO setup failed");

  //Buffer data for all vertices
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    t1=tt=clock();

    //Calculate vertex normals?
    bool vnormals = geom[index]->draw->properties["vertexnormals"];
    debug_print("Mesh %d/%d has normals? %d == %d\n", index, geom.size(), geom[index]->render->normals.size(), geom[index]->render->vertices.size());
    if (geom[index]->render->normals.size() != geom[index]->render->vertices.size()) vnormals = false;

    //Colour values to texture coords
    ColourMap* texmap = geom[index]->draw->textureMap;
    FloatValues* vals = geom[index]->colourData();
    //Calibrate colour maps on range for this surface
    ColourLookup& getColour = geom[index]->colourCalibrate();
    unsigned int hasColours = geom[index]->colourCount();
    if (hasColours > geom[index]->count()) hasColours = geom[index]->count(); //Limit to vertices
    unsigned int colrange = hasColours ? geom[index]->count() / hasColours : 1;
    if (colrange < 1) colrange = 1;
    //if (hasColours) assert(colrange * hasColours == geom[index]->count());
    //if (hasColours && colrange * hasColours != geom[index]->count())
    //   debug_print("WARNING: Vertex Count %d not divisable by colour count %d\n", geom[index]->count(), hasColours);
    debug_print("Using 1 colour per %d vertices (%d : %d)\n", colrange, geom[index]->count(), hasColours);

    Colour colour;
    float zero[3] = {0,0,0};
    float shift = geom[index]->draw->properties["shift"];
    if (geom[index]->draw->name().length() == 0) shift = 0.0; //Skip shift for built in objects
    shift *= 0.0001 * view->model_size;
    std::array<float,3> shiftvert;
    float texCoord[2] = {0.0, 0.0};
    for (unsigned int v=0; v < geom[index]->count(); v++)
    {
      //Have colour values but not enough for per-vertex, spread over range (eg: per triangle)
      unsigned int cidx = v / colrange;
      if (!texmap && cidx * colrange == v)
        getColour(colour, cidx);

      float* vert = geom[index]->render->vertices[v];
      if (shift > 0)
      {
        //Shift vertices
        shiftvert = {vert[0] + shift, vert[1] + shift, vert[2] + shift};
        vert = shiftvert.data();
        if (v==0) debug_print("Shifting vertices %s (%d) by %f\n", geom[index]->draw->name().c_str(), index, shift);
      }

      //Write vertex data to vbo
      assert((unsigned int)(ptr-p) < bsize);
      //Copies vertex bytes
      memcpy(ptr, vert, sizeof(float) * 3);
      ptr += sizeof(float) * 3;
      //Copies normal bytes
      if (vnormals)
        memcpy(ptr, &geom[index]->render->normals[v][0], sizeof(float) * 3);
      else
        memcpy(ptr, zero, sizeof(float) * 3);
      ptr += sizeof(float) * 3;
      //Copies texCoord bytes
      if (geom[index]->render->texCoords.size() > v)
        memcpy(ptr, &geom[index]->render->texCoords[v][0], sizeof(float) * 2);
      else if (texmap && vals)
      {
        texCoord[0] = texmap->scalefast(geom[index]->colourData(v));
        //if (v%100==0 || texCoord[0] <= 0.0) printf("(%f - %f) %f ==> %f\n", texmap->minimum, texmap->maximum, geom[index]->colourData(v), texCoord[0]);
        memcpy(ptr, texCoord, sizeof(float) * 2);
      }

      ptr += sizeof(float) * 2;
      //Copies colour bytes
      memcpy(ptr, &colour, sizeof(Colour));
      ptr += sizeof(Colour);
    }
    t2 = clock();
    debug_print("  %.4lf seconds to reload %d vertices\n", (t2-t1)/(double)CLOCKS_PER_SEC, geom[index]->count());
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);
  GL_Error_Check;
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  debug_print("  Total %.4lf seconds to update triangle buffers\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

//Reloads triangle indices
void Triangles::render()
{
  clock_t t1,t2;
  t1 = clock();
  if (elements == 0) return;

  //Prepare the Index buffer
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
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

  //Element counts to actually plot (exclude filtered/hidden) per geom entry
  counts.clear();
  counts.resize(geom.size());

  //Upload vertex indices
  unsigned int offset = 0;
  unsigned int idxcount = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    unsigned int indices = geom[index]->render->indices.size();
    if (drawable(index))
    {
      if (indices > 0)
      {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof(GLuint), indices * sizeof(GLuint), geom[index]->render->indices.ref());
        //printf("%d upload %d indices\n", index, indices);
        counts[index] = indices;
        offset += indices;
        GL_Error_Check;
      }
      else
      {
        //No indices, just raw vertices
        counts[index] = geom[index]->count();
        //printf("%d upload NO indices, %d vertices\n", index, counts[index]);
      }
      idxcount += counts[index];
    }
  }

  GL_Error_Check;
  t2 = clock();
  debug_print("  %.4lf seconds to upload %d indices\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount);
  t1 = clock();
  //After render(), elements holds unfiltered count, idxcount is filtered
  elements = idxcount;
}

void Triangles::draw()
{
  GL_Error_Check;
  if (elements == 0) return;

  //Re-render the triangles if required
  if (counts.size() == 0)
    render();

  // Draw using vertex buffer object
  clock_t t0 = clock();
  clock_t t1 = clock();
  double time;
  int stride = 8 * sizeof(float) + sizeof(Colour);   //3+3+2 vertices, normals, texCoord + 32-bit colour
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (geom.size() > 0 && elements > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
  {
    intptr_t vstart = 0; //Vertex buffer offset
    unsigned int start = 0;
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    for (unsigned int index = 0; index < geom.size(); index++)
    {
      if (counts[index] > 0)
      {
        unsigned int offset = 0;
        glVertexPointer(3, GL_FLOAT, stride, (GLvoid*)vstart); // Load vertex x,y,z only
        offset += 3;
        glNormalPointer(GL_FLOAT, stride, (GLvoid*)(vstart + offset*sizeof(float))); // Load normal x,y,z, offset 3 float
        offset += 3;
        glTexCoordPointer(2, GL_FLOAT, stride, (GLvoid*)(vstart + offset*sizeof(float))); // Load texcoord x,y
        offset += 2;
        glColorPointer(4, GL_UNSIGNED_BYTE, stride, (GLvoid*)(vstart + offset*sizeof(float)));   // Load rgba, offset 6 float


        setState(index); //Set draw state settings for this object
        if (geom[index]->render->indices.size() > 0)
        {
          //Draw with index buffer
          glDrawElements(GL_TRIANGLES, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
          //printf("DRAW %d from %d by INDEX vs %d\n", counts[index], start, vstart);
        }
        else
        {
          //Draw directly from vertex buffer
          glDrawArrays(GL_TRIANGLES, start, geom[index]->count());
          //printf("DRAW %d from %d by VERTEX vs %d\n", geom[index]->count(), start, vstart);
        }
        start += counts[index];
      }

      //Vertex buffer offset (bytes) required because indices per object are zero based
      vstart += stride * geom[index]->count();
    }

    time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
    if (time > 0.005) debug_print("  %.4lf seconds to draw opaque triangles\n", time);
    t1 = clock();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
  if (time > 0.05)
    debug_print("  %.4lf seconds to draw triangles\n", time);
  GL_Error_Check;
}

void Triangles::jsonWrite(DrawingObject* draw, json& obj)
{
  jsonExportAll(draw, obj);
}
