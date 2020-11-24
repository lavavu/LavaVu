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

Points::Points(Session& session) : Geometry(session)
{
  type = lucPointType;
}

Points::~Points()
{
  sorter.clear();
}

void Points::close()
{
  Geometry::close();
}

void Points::update()
{
  //Get point count
  total = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    //Un-structured vertices
    total += geom[index]->count();
    debug_print(" %s, points %d hidden? %s\n", geom[index]->draw->name().c_str(), geom[index]->count(), (!drawable(index) ? "yes" : "no"));
  }
  if (total == 0) return;

  //Ensure vbo recreated if total changed
  //To force update, set geometry->reload = true
  if (reload || vbo == 0)
    loadVertices();

  //Always reload indices if redraw flagged
  if (reload)
    sorter.changed = true;

  //Reload the sort array?
  if (sorter.size != total || !allVertsFixed || counts.size() != geom.size() || redraw)
    loadList();
}

void Points::loadVertices()
{
  debug_print("Reloading %d particles...\n", total);
  // Update points...
  clock_t t1,t2;

  // VBO - copy normals/colours/positions to buffer object for quick display
  int datasize;
  bool attribs = session.global("pointattribs");
  if (attribs)
    datasize = sizeof(float) * 5 + sizeof(Colour);   //Vertex(3), two flags and 32-bit colour
  else
    datasize = sizeof(float) * 3 + sizeof(Colour);   //Vertex(3) and 32-bit colour

  //Check for use of texture, if any elements use textures then entire buffer requires elements
  if (geom.size() == 0) return;
  for (unsigned int s = 0; s < geom.size(); s++)
    anyHasTexture = anyHasTexture || (geom[s]->hasTexture() && geom[s]->render->texCoords.size()/2 == geom[s]->count());
  if (anyHasTexture)
    datasize += 2 * sizeof(float); //TexCoord 2 * float

  //Create intermediate buffer
  unsigned char* buffer = new unsigned char[total * datasize];
  unsigned char *ptr = buffer;

//////////////////////////////////////////////////
  t1 = clock();
  //debug_print("Reloading %d particles...(size %f)\n", total);

  //Get eye distances and copy all particles into sorting array
  for (unsigned int s = 0; s < geom.size(); s++)
  {
    debug_print("Swarm %d, points %d hidden? %s\n", s, geom[s]->count(), (hidden[s] ? "yes" : "no"));

    //Calibrate colourMap
    ColourLookup& getColour = geom[s]->colourCalibrate();

    unsigned int hasColours = geom[s]->colourCount();
    if (hasColours > geom[s]->count()) hasColours = geom[s]->count(); //Limit to vertices
    unsigned int colrange = hasColours ? geom[s]->count() / hasColours : 1;
    if (colrange < 1) colrange = 1;
    debug_print("Using 1 colour per %d vertices (%d : %d)\n", colrange, geom[s]->count(), hasColours);

    Properties& props = geom[s]->draw->properties;
    float psize0 = props["pointsize"];
    float scaling = props["scaling"];
    //printf("psize %f * scaling %f = %f\n", psize0, scaling, psize0 * scaling);
    psize0 *= scaling;
    float ptype = getPointType(s); //Default (-1) is to use the global (uniform) value
    unsigned int sizeidx = geom[s]->valuesLookup(geom[s]->draw->properties["sizeby"]);
    bool usesize = geom[s]->valueData(sizeidx) != NULL;
    //std::cout << geom[s]->draw->properties["sizeby"] << " : " << sizeidx << " : " << usesize << std::endl;
    Colour colour;
    bool hasTexture = geom[s]->hasTexture();
    bool hasTexCoords = geom[s]->render->texCoords.size()/2 == geom[s]->count();
    float nullTexCoord[2] = {0.0, -1.0};
    for (unsigned int i = 0; i < geom[s]->count(); i ++)
    {
      //Copy data to VBO entry
      if (ptr)
      {
        assert((unsigned int)(ptr-buffer) < total * datasize);
        //Copies vertex bytes
        memcpy(ptr, geom[s]->render->vertices[i], sizeof(float) * 3);
        ptr += sizeof(float) * 3;
        //getColour(c, i);
        //Have colour values but not enough for per-vertex, spread over range (eg: per triangle)
        unsigned int cidx = i / colrange;
        if (cidx * colrange == i)
          getColour(colour, cidx);
        memcpy(ptr, &colour, sizeof(Colour));
        ptr += sizeof(Colour);
        //Optional texcoord
        if (anyHasTexture)
        {
          if (hasTexture && hasTexCoords)
            memcpy(ptr, geom[s]->render->texCoords[i], sizeof(float) * 2);
          else
            memcpy(ptr, nullTexCoord, sizeof(float) * 2);
          ptr += sizeof(float) * 2;
        }
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

  //Initialise vertex array object for OpenGL 3.2+
  if (!vao) glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  if (!vbo)
    glGenBuffers(1, &vbo);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  //Initialise point buffer
  if (glIsBuffer(vbo))
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, total * datasize, buffer, GL_DYNAMIC_DRAW);
    debug_print("  %d byte VBO created, for %d vertices\n", (int)(total * datasize), total);
  }
  else
    debug_print("  VBO creation failed!\n");

  GL_Error_Check;

  delete[] buffer;

  if (!glIsBuffer(vbo))
    abort_program("VBO load failed");
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;
}

void Points::loadList()
{
  // Update points sorting list...
  clock_t t1,t2;
  t1 = clock();

  //Create sorting array
  sorter.allocate(total);

  if (geom.size() == 0) return;
  int voffset = 0;

  //Element counts to actually plot (exclude filtered/hidden) per geom entry
  counts.clear();
  counts.resize(geom.size());

  unsigned int maxCount = session.global("pointmaxcount");
  unsigned int subSample = session.global("pointsubsample");
  //Auto-sub-sample if maxcount set
  if (maxCount > 0 && elements > maxCount)
    subSample = elements / maxCount + 0.5; //Rounded up
  elements = 0;
  uint32_t SEED;
  for (unsigned int s = 0; s < geom.size(); voffset += geom[s]->count(), s++)
  {
    counts[s] = 0;
    if (!drawable(s)) continue;

    //Calibrate colourMap - required to re-cache filter settings (TODO: split filter reload into another function?)
    geom[s]->colourCalibrate();

    //Override opaque if pointtype requires opacity (1/2) unless explicitly set
    if (geom[s]->opaqueCheck() && (int)geom[s]->draw->properties["pointtype"] < 2 && !geom[s]->draw->properties["opaque"])
      geom[s]->opaque = false;

    bool filter = geom[s]->draw->filterCache.size();
    for (unsigned int i = 0; i < geom[s]->count(); i ++)
    {
      if (filter && geom[s]->filter(i)) continue;
      // If subSampling, use a pseudo random distribution to select which particles to draw
      // If we just draw every n'th particle, we end up with a whole bunch in one region / proc
      SEED = i; //Reset the seed for determinism based on index
      if (subSample > 1 && SHR3(SEED) % subSample > 0) continue;

      sorter.buffer[elements].index = sorter.indices[elements] = voffset + i;
      sorter.buffer[elements].vertex = geom[s]->render->vertices[i];
      sorter.buffer[elements].distance = 0;

      if (geom[s]->opaque)
      {
        //All opaque points at start
        sorter.buffer[elements].distance = USHRT_MAX;
        sorter.buffer[elements].vertex = NULL;
      }

      elements++;
      counts[s] ++; //Element count
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds to update %d/%d particles into sort array\n", (t2-t1)/(double)CLOCKS_PER_SEC, elements, total);
  t1 = clock();

  updateBoundingBox();

  if (session.global("sort"))
    sort();
}

//Depth sort the particles before drawing, called whenever the viewing angle has changed
void Points::sort()
{
  //List not yet loaded, wait
  //if (!sorter.buffer || !total == 0 || elements == 0 || !view->is3d) return;
  if (!sorter.buffer || total == 0) return;

  clock_t t1,t2;
  t1 = clock();

  //Calculate min/max distances from view plane
  float distanceRange[2];
  view->getMinMaxDistance(min, max, distanceRange, true);

  //Update eye distances, clamping distance to integer between 0 and USHRT_MAX-1
  //float multiplier = (float)USHRT_MAX / (distanceRange[1] - distanceRange[0]);
  float multiplier = (USHRT_MAX-1.0) / (distanceRange[1] - distanceRange[0]);
  float fdistance;
  unsigned int opaqueCount = 0;
  for (unsigned int i = 0; i < elements; i++)
  {
    //Max dist 65535 reserved for opaque triangles
    if (sorter.buffer[i].distance < USHRT_MAX)
    {
      //Distance from viewing plane is -eyeZ
      fdistance = view->eyePlaneDistance(sorter.buffer[i].vertex);
      //fdistance = view->eyeDistance(sorter.buffer[i].vertex);
      //float d = floor(multiplier * (fdistance - distanceRange[0])) + 0.5;
      //assert(d < USHRT_MAX);
      //assert(d >= 0);
      sorter.buffer[i].distance = (unsigned short)(multiplier * (fdistance - distanceRange[0]));
    }
    else
      opaqueCount++;
  }
  t2 = clock();
  debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Skip sort if all opaque
  if (opaqueCount == elements)
  {
    debug_print("No sort necessary\n");
    return;
  }

  //Depth sort using 2-byte key radix sort, 10 times faster than equivalent quicksort
  if (view->is3d)
  {
    sorter.sort(elements);
    t2 = clock();
    debug_print("  %.4lf seconds to sort %d points\n", (t2-t1)/(double)CLOCKS_PER_SEC, elements);
  }

  //Re-map vertex indices in sorted order
  t1 = clock();
  //Lock the update mutex, to allow updating the indexlist and prevent access while drawing
  LOCK_GUARD(loadmutex);
  //Reverse order farthest to nearest
  //int distSample = session.global("pointdistsample");
  //uint32_t SEED;
  int idxcount = 0;
  for(int i=elements-1; i>=0; i--)
  {
    /*/Distance based sub-sampling - disabled
    if (distSample > 0)
    {
      SEED = sorter.buffer[i].index; //Reset the seed for determinism based on index
      int subSample = 1 + distSample * sorter.buffer[i].distance / (USHRT_MAX-1.0); //[1,distSample]
      if (subSample > 1 && SHR3(SEED) % subSample > 0) continue;
    }*/
    sorter.indices[idxcount] = sorter.buffer[i].index;
    idxcount ++;
  }

  t2 = clock();
  //if (distSample)
  //  debug_print("  %.4lf seconds to load %d indices (dist-sub-sampled %d)\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount, distSample);
  //else
    debug_print("  %.4lf seconds to load %d indices)\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount);
  t1 = clock();

  //Force update after sort
  sorter.changed = true;
}

//Reloads points into display list or VBO, required after data update and depth sort
void Points::render()
{
  clock_t t1,t2,tt;
  if (total == 0 || elements == 0) return;
  assert(sorter.indices.size());

  tt = t1 = clock();

  // Index buffer object for quick display
  glBindVertexArray(vao);
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  GL_Error_Check;
  //Initialise particle buffer
  if (glIsBuffer(indexvbo))
  {
    //Lock the update mutex, to wait for any updates to the sorter.indices to finish
    LOCK_GUARD(loadmutex);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sorter.indices.size() * sizeof(GLuint), sorter.indices.data(), GL_DYNAMIC_DRAW);
    debug_print("  %d byte IBO uploaded %d indices\n", sorter.indices.size() * sizeof(GLuint), sorter.indices.size());
  }
  else
    abort_program("IBO creation failed!\n");
  GL_Error_Check;

  t2 = clock();
  debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

int Points::getPointType(int index)
{
  if (index != -1)
  {
    if (geom[index]->draw->properties.has("pointtype"))
      return geom[index]->draw->properties["pointtype"];
    else
      return -1; //Use global 
  }
  return session.global("pointtype");
}

void Points::draw()
{
  if (elements == 0) return;
  clock_t t0 = clock();
  double time;
  GL_Error_Check;

  setState(0); //Set global draw state (using first object)
  Shader_Ptr prog = session.shaders[lucPointType];

  //Re-render the particles if view has rotated
  if (sorter.changed) render();

  glDepthFunc(GL_LEQUAL); //Ensure points at same depth both get drawn
  //Required for OpenGL < 3.2 or compatibility mode
#ifndef __EMSCRIPTEN__
  if (!session.context.core)
  {
#ifdef GL_POINT_SPRITE
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
  }
  else
  {
    glEnable(GL_PROGRAM_POINT_SIZE);
  }
#endif
  GL_Error_Check;


  //Point size distance attenuation (disabled for 2d models)
  float scale0 = (float)geom[0]->draw->properties["scalepoints"];
  int pointspixels = session.global("pointpixelscale");
  if (pointspixels == 1)
    pointspixels = view->base_height; //Saved height at first render
  //Adjust scaling by viewport aspect ratio?
  if (pointspixels > 0)
    scale0 *= view->height / (float)pointspixels;
  else
    scale0 *= session.context.scale2d; //Include 2d scale factor

  if (view->is3d && session.global("pointattenuate")) //Adjust scaling by model size when using distance size attenuation
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

  // Draw using vertex buffer object
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (sorter.size > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
  {
    //Setup vertex attributes
    long offset = 0;
    GLint aPosition = prog->attribs["aVertexPosition"];
    GLint aColour = prog->attribs["aVertexColour"];
    GLint aTexCoord = prog->attribs["aVertexTexCoord"];
    GLint aSize = prog->attribs["aSize"];
    GLint aPointType = prog->attribs["aPointType"];

    int stride = 3 * sizeof(float) + sizeof(Colour);
    if (anyHasTexture)
      stride += 2 * sizeof(float);
    bool attribs = session.global("pointattribs");
    if (attribs)
      stride += 2 * sizeof(float);

    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offset)); // Vertex x,y,z
    offset += 3*sizeof(float);
    glEnableVertexAttribArray(aColour);
    glVertexAttribPointer(aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(offset));   // rgba, offset 3 float
    offset += sizeof(Colour);

    //Generic vertex attributes, "aTexCoord", "aSize", "aPointType"
    if (anyHasTexture)
    {
      glEnableVertexAttribArray(aTexCoord);
      glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_TRUE, stride, (GLvoid*)(offset));
      offset += 2*sizeof(float);
    }
    else
    {
      //Flag unused tex coord
      glVertexAttrib2f(aTexCoord, -1.0f, 0.0f);
    }

    if (attribs)
    {
      glEnableVertexAttribArray(aSize);
      glVertexAttribPointer(aSize, 1, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offset));
      offset += 1*sizeof(float);
      glEnableVertexAttribArray(aPointType);
      glVertexAttribPointer(aPointType, 1, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offset));
      offset += 1*sizeof(float);
    }
    else
    {
      glVertexAttrib1f(aSize, 1.0f);
      glVertexAttrib1f(aPointType, getPointType());
    }
    GL_Error_Check;

    unsigned int start = 0;
    int defidx = 0;
    //for (int index = 0; index<geom.size(); index++)
    for (int index = geom.size()-1; index >= 0; index--)
    {
      if (counts[index] == 0) continue;
      if (geom[index]->opaque)
      {
        setState(index); //Set draw state settings for this object
        //fprintf(stderr, "(%d, %s) DRAWING OPAQUE POINTS: %d (%d to %d)\n", index, geom[index]->draw->name().c_str(), counts[index], start, (start+counts[index]));
        glDrawElements(GL_POINTS, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
        start += counts[index];
      }
      else
        defidx = index;
    }

    //Draw remaining elements (transparent, depth sorted)
    if (start < (unsigned int)elements)
    {
      //Set draw state settings for first non-opaque object
      //NOTE: per-object properties do not work with transparency!
      setState(defidx);

      //Render all remaining points - elements is the number of indices. 3 indices needed to make a single triangle
      //fprintf(stderr, "(*) DRAWING TRANSPARENT POINTS: %d\n", (elements-start));
      glDrawElements(GL_POINTS, elements-start, GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
    }

    if (aTexCoord >= 0) glDisableVertexAttribArray(aTexCoord);
    if (attribs)
    {
      if (aSize >= 0) glDisableVertexAttribArray(aSize);
      if (aPointType >= 0) glDisableVertexAttribArray(aPointType);
    }

    glDisableVertexAttribArray(aPosition);
    glDisableVertexAttribArray(aColour);
  }
  GL_Error_Check;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  GL_Error_Check;
  //Restore state
  glBindTexture(GL_TEXTURE_2D, 0);
#ifndef __EMSCRIPTEN__
  if (!session.context.core)
  {
#ifdef GL_POINT_SPRITE
    glDisable(GL_POINT_SPRITE);
#endif
  }
  else
  {
    glDisable(GL_PROGRAM_POINT_SIZE);
  }
#endif
  GL_Error_Check;
  glDepthFunc(GL_LESS);
  GL_Error_Check;

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

