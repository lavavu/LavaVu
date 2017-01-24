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

#include "GraphicsUtil.h"
#include "Geometry.h"

Points::Points(DrawState& drawstate) : Geometry(drawstate)
{
  type = lucPointType;
  pidx = swap = NULL;
  idxcount = 0;
  indexvbo = 0;
  vbo = 0;
}

Points::~Points()
{
  close();
}

void Points::close()
{
  if (!drawstate.global("gpucache"))
  {
    if (vbo)
      glDeleteBuffers(1, &vbo);
    if (indexvbo)
      glDeleteBuffers(1, &indexvbo);
    vbo = 0;
    indexvbo = 0;

    reload = true;
  }

  if (pidx)
    delete[] pidx;
  if (swap)
    delete[] swap;
  pidx = swap = NULL;
}

void Points::init()
{
  Geometry::init();
}

void Points::update()
{
  //Ensure vbo recreated if total changed
  //To force update, set geometry->reload = true
  if (reload || vbo == 0)
    loadVertices();

  //Initial depth sort & render
  view->sort = true;

  //Always reload indices if redraw flagged
  if (redraw) idxcount = 0;

  //Reload the sort array
  loadList(); 
}

void Points::loadVertices()
{
  debug_print("Reloading %d particles...\n", total);
  // Update points...
  clock_t t1,t2;

  // VBO - copy normals/colours/positions to buffer object for quick display
  int datasize;
  if (drawstate.global("pointattribs"))
    datasize = sizeof(float) * 5 + sizeof(Colour);   //Vertex(3), two flags and 32-bit colour
  else
    datasize = sizeof(float) * 3 + sizeof(Colour);   //Vertex(3) and 32-bit colour
  if (!vbo) glGenBuffers(1, &vbo);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  //Initialise point buffer
  if (glIsBuffer(vbo))
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, total * datasize, NULL, GL_STREAM_DRAW);
    debug_print("  %d byte VBO created, for %d vertices\n", (int)(total * datasize), total);
  }
  else
    debug_print("  VBO creation failed!\n");

  GL_Error_Check;

  unsigned char *p = NULL, *ptr = NULL;
  if (glIsBuffer(vbo))
  {
    ptr = p = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    GL_Error_Check;
    if (!p) abort_program("glMapBuffer failed");
  }
  else ptr = NULL;
  //else abort_program("vbo fail");

//////////////////////////////////////////////////
  t1 = clock();
  //debug_print("Reloading %d particles...(size %f)\n", (int)ceil(total / (float)subSample), scale);

  //Get eye distances and copy all particles into sorting array
  for (unsigned int s = 0; s < geom.size(); s++)
  {
    debug_print("Swarm %d, points %d hidden? %s\n", s, geom[s]->count, (hidden[s] ? "yes" : "no"));

    //Calibrate colourMap
    geom[s]->colourCalibrate();

    Properties& props = geom[s]->draw->properties;
    float psize0 = props["pointsize"];
    float scaling = props["scaling"];
    psize0 *= scaling;
    float ptype = getPointType(s); //Default (-1) is to use the global (uniform) value
    bool attribs = drawstate.global("pointattribs");
    unsigned int sizeidx = geom[s]->valuesLookup(geom[s]->draw->properties["sizeby"]);
    bool usesize = geom[s]->valueData(sizeidx) != NULL;
    //std::cout << geom[s]->draw->properties["sizeby"] << " : " << sizeidx << " : " << usesize << std::endl;
    Colour c;

    for (unsigned int i = 0; i < geom[s]->count; i ++)
    {
      //Copy data to VBO entry
      if (ptr)
      {
        assert((unsigned int)(ptr-p) < total * datasize);
        //Copies vertex bytes
        memcpy(ptr, geom[s]->vertices[i], sizeof(float) * 3);
        ptr += sizeof(float) * 3;
        geom[s]->getColour(c, i);
        memcpy(ptr, &c, sizeof(Colour));
        ptr += sizeof(Colour);
        //Optional per-object size/type
        if (attribs)
        {
          //Copies settings (size + smooth)
          float psize = psize0;
          if (usesize) psize *= geom[s]->valueData(sizeidx, i);
          memcpy(ptr, &psize, sizeof(float));
          ptr += sizeof(float);
          memcpy(ptr, &ptype, sizeof(float));
          ptr += sizeof(float);
        }
      }
    }

  }
  t2 = clock();
  debug_print("  %.4lf seconds to update %d particles into vbo\n", total, (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  if (ptr) glUnmapBuffer(GL_ARRAY_BUFFER);
  GL_Error_Check;
}

void Points::loadList()
{
  // Update points sorting list...
  clock_t t1,t2;
  t1 = clock();

  //Create sorting array
  if (pidx) delete[] pidx;
  pidx = new PIndex[total];
  if (swap) delete[] swap;
  swap = new PIndex[total];
  if (pidx == NULL || swap == NULL) abort_program("Memory allocation error (failed to allocate %d bytes)", sizeof(PIndex) * total);
  if (geom.size() == 0) return;
  elements = 0;
  int offset = 0;
  int subSample = drawstate.global("pointsubsample");
  uint32_t SEED;
  for (unsigned int s = 0; s < geom.size(); offset += geom[s]->count, s++)
  {
    if (!drawable(s)) continue;
    for (unsigned int i = 0; i < geom[s]->count; i ++)
    {
      if (geom[s]->filter(i)) continue;
      // If subSampling, use a pseudo random distribution to select which particles to draw
      // If we just draw every n'th particle, we end up with a whole bunch in one region / proc
      SEED = i; //Reset the seed for determinism based on index
      if (subSample > 1 && SHR3(SEED) % subSample > 0) continue;

      pidx[elements].index = offset + i;
      pidx[elements].vertex = geom[s]->vertices[i];
      pidx[elements].distance = 0;
      elements++;
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds to update %d/%d particles into sort array\n", (t2-t1)/(double)CLOCKS_PER_SEC, elements, total);
printf("  %.4lf seconds to update %d/%d particles into sort array\n", (t2-t1)/(double)CLOCKS_PER_SEC, elements, total);
  t1 = clock();
}

//Depth sort the particles before drawing, called whenever the viewing angle has changed
void Points::depthSort()
{
  clock_t t1,t2;
  t1 = clock();
  if (elements == 0) return;
  int size = elements*sizeof(PIndex);

  //Calculate min/max distances from view plane
  float maxdist, mindist;
  view->getMinMaxDistance(&mindist, &maxdist);

  //Update eye distances, clamping distance to integer between 0 and USHRT_MAX
  float multiplier = (float)USHRT_MAX / (maxdist - mindist);
  float fdistance;
  for (unsigned int i = 0; i < elements; i++)
  {
    //Distance from viewing plane is -eyeZ
    fdistance = eyeDistance(view->modelView, pidx[i].vertex);
    pidx[i].distance = (unsigned short)(multiplier * (fdistance - mindist));
  }
  t2 = clock();
  debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Depth sort using 2-byte key radix sort, 10 times faster than equivalent quicksort
  radix_sort<PIndex>(pidx, swap, elements, 2);
  t2 = clock();
  debug_print("  %.4lf seconds to sort %d points\n", (t2-t1)/(double)CLOCKS_PER_SEC, elements);
  t1 = clock();
  GL_Error_Check;
}

//Reloads points into display list or VBO, required after data update and depth sort
void Points::render()
{
  clock_t t1,t2,tt;
  if (total == 0 || elements == 0) return;
  assert(pidx);

  //First, depth sort the particles
  //if (view->is3d && view->sort)
  if (view->sort)
  {
    debug_print("Depth sorting %d of %d particles...\n", elements, total);
    depthSort();
  }

  tt = t1 = clock();

  // Index buffer object for quick display
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  GL_Error_Check;
  //Initialise particle buffer
  if (glIsBuffer(indexvbo))
  {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, total * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    debug_print("  %d byte IBO created for %d indices\n", elements * sizeof(GLuint), elements);
  }
  else
    abort_program("IBO creation failed!\n");
  GL_Error_Check;

  //Re-map vertex indices in sorted order
  t1 = clock();
  GLuint *ptr = (GLuint*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
  if (!ptr) abort_program("glMapBuffer failed");
  //Reverse order farthest to nearest
  int distSample = drawstate.global("pointdistsample");
  uint32_t SEED;
  idxcount = 0;
  for(int i=elements-1; i>=0; i--)
  {
    //Distance based sub-sampling
    if (distSample > 0)
    {
      SEED = pidx[i].index; //Reset the seed for determinism based on index
      int subSample = 1 + distSample * pidx[i].distance / USHRT_MAX; //[1,distSample]
      if (subSample > 1 && SHR3(SEED) % subSample > 0) continue;
    }
    ptr[idxcount] = pidx[i].index;
    idxcount++;
  }

  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
  t2 = clock();
  if (distSample)
    debug_print("  %.4lf seconds to upload %d indices (dist-sub-sampled %d)\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount, distSample);
  else
    debug_print("  %.4lf seconds to upload %d indices)\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount);
  t1 = clock();
  GL_Error_Check;

  t2 = clock();
  debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

int Points::getPointType(int index)
{
  json& pointtype = drawstate.global("pointtype");
  int ptype = -1;
  if (index != -1)
  {
    if (geom[index]->draw->properties.has("pointtype"))
      pointtype = geom[index]->draw->properties["pointtype"];
    else
      return -1; //Use global 
  }

  if (pointtype.is_string())
  {
    std::string val = pointtype;
    if (pointtype == "blur")
      ptype = 0;
    else if (pointtype == "smooth")
      ptype = 1;
    else if (pointtype == "sphere")
      ptype = 2;
    else if (pointtype == "shiny")
      ptype = 3;
    else if (pointtype == "flat")
      ptype = 4;
  }
  else if (pointtype.is_number())
    ptype = pointtype;

  return ptype;
}

void Points::draw()
{
  //Draw, update
  Geometry::draw();
  if (drawcount == 0 || elements == 0) return;
  clock_t t0 = clock();
  double time;
  GL_Error_Check;

  Shader* prog = drawstate.prog[lucPointType];
  setState(0, prog); //Set global draw state (using first object)

  //Re-render the particles if view has rotated
  //if (view->sort || idxcount != elements) render();
  if (view->sort || idxcount == 0) render();
  //After render(), elements holds unfiltered count, idxcount is filtered
  //idxcount = idxcount;

  glDepthFunc(GL_LEQUAL); //Ensure points at same depth both get drawn
  glEnable(GL_POINT_SPRITE);
  GL_Error_Check;

  //Gen TexCoords for point sprites (for GLSL < 1.3 where gl_PointCoord not available)
  glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
  GL_Error_Check;


  glDepthFunc(GL_LEQUAL); //Ensure points at same depth both get drawn
  if (!view->is3d) glDisable(GL_DEPTH_TEST);

  //Point size distance attenuation (disabled for 2d models)
  float scale0 = (float)geom[0]->draw->properties["scalepoints"] * view->scale2d; //Include 2d scale factor
  if (view->is3d && drawstate.global("pointattenuate")) //Adjust scaling by model size when using distance size attenuation
  {
    prog->setUniform("uPointScale", scale0 * view->model_size);
    prog->setUniform("uPointDist", 1);
  }
  else
  {
    prog->setUniform("uPointScale", scale0);
    prog->setUniform("uPointDist", 0);
  }
  prog->setUniformi("uPointType", getPointType());

  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
  GL_Error_Check;

  // Draw using vertex buffer object
  int stride = 3 * sizeof(float) + sizeof(Colour);
  if (drawstate.global("pointattribs"))
    stride += 2 * sizeof(float);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (idxcount > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
  {
    //Built in attributes gl_Vertex & gl_Color (Note: for OpenGL 3.0 onwards, should define our own generic attributes)
    glVertexPointer(3, GL_FLOAT, stride, (GLvoid*)0); // Load vertex x,y,z only
    glColorPointer(4, GL_UNSIGNED_BYTE, stride, (GLvoid*)(3*sizeof(float)));   // Load rgba, offset 3 float
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    //Generic vertex attributes, "aSize", "aPointType"
    if (drawstate.global("pointattribs"))
    {
      GLint aSize = 0, aPointType = 0;
      aSize = prog->attribs["aSize"];
      if (aSize >= 0)
      {
        glEnableVertexAttribArray(aSize);
        glVertexAttribPointer(aSize, 1, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(3*sizeof(float)+sizeof(Colour)));
      }
      aPointType = prog->attribs["aPointType"];
      if (aPointType >= 0)
      {
        glEnableVertexAttribArray(aPointType);
        glVertexAttribPointer(aPointType, 1, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(4*sizeof(float)+sizeof(Colour)));
      }

      //Draw the points
      glDrawElements(GL_POINTS, idxcount, GL_UNSIGNED_INT, (GLvoid*)0);

      if (aSize >= 0) glDisableVertexAttribArray(aSize);
      if (aPointType >= 0) glDisableVertexAttribArray(aPointType);
    }
    else
    {
      //Use generic default values
      glVertexAttrib1f(prog->attribs["aPointType"], -1.0);
      glVertexAttrib1f(prog->attribs["aSize"], 1.0);
      GL_Error_Check;

      //Draw the points
      glDrawElements(GL_POINTS, idxcount, GL_UNSIGNED_INT, (GLvoid*)0);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  //Restore state
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_POINT_SPRITE);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_TEXTURE_2D);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);

  time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
  if (time > 0.05)
    debug_print("%.4lf seconds to draw points\n", time);
  GL_Error_Check;

  labels();
}

void Points::jsonWrite(DrawingObject* draw, json& obj)
{
  jsonExportAll(draw, obj);
}

