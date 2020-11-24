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

//Line centre for depth sorting
#define linecentre(v1,v2) {centres.emplace_back((v1[0]+v2[0])/2., (v1[1]+v2[1])/2., (v1[2]+v2[2])/2.);}

LinesSorted::LinesSorted(Session& session) : Lines(session)
{
  idxcount = 0;
}

LinesSorted::~LinesSorted()
{
  Lines::close();
  sorter.clear();
}

void LinesSorted::close()
{
}

void LinesSorted::update()
{
  // Update lines...
  unsigned int drawelements = lineCount();
  if (drawelements == 0) return;

  //Only reload the vbo data when required
  //Not needed when objects hidden/shown but required if colours changed
  //if (centres.size() != total/2 || vbo == 0 || (reload && (!allVertsFixed || internal)))
  {
    //Load & optimise the mesh data (including updating centroids)
    loadLines();
    redraw = true;
  }

  if (redraw) //Vertices possibly not updated but colours or properties?
  {
    //Send the data to the GPU via VBO
    loadBuffers();
  }

  //Always reload indices if reload flagged
  if (reload)
    sorter.changed = true;

  //Reload the sort array?
  //if (sorter.size != total/2 || !allVertsFixed || counts.size() != geom.size())
    loadList();
}

void LinesSorted::loadLines()
{
  // Load lines, calculating centre positions
  clock_t t1,tt;
  tt=clock();

  debug_print("Loading %d lines...\n", total/2);

  elements = 0;
  //Reset centre data
  centres.clear();
  centres.reserve(total/2);
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    if (geom[index]->count() == 0) continue;

    //Has index data, simply load the triangles
    if (geom[index]->render->indices.size() > 0)
    {
      unsigned i1, i2;
      for (unsigned int j=0; j < geom[index]->render->indices.size()-1; j += 2)
      {
        i1 = geom[index]->render->indices[j];
        i2 = geom[index]->render->indices[j+1];

        linecentre(geom[index]->render->vertices[i1],
                   geom[index]->render->vertices[i2]);

        elements += 2;
      }
    }
    else
    {
      //Load non-indexed... create indices and get centre points
      std::vector<GLuint> indices;

      for (unsigned int j=0; j < geom[index]->count()-1; j += 2)
      {
        linecentre(geom[index]->render->vertices[j],
                   geom[index]->render->vertices[j+1]);

        indices.push_back(j);
        indices.push_back(j+1);

        elements += 2;
      }

      //Read the indices for loading sort list and later use (json export etc)
      geom[index]->render->indices.read(indices.size(), &indices[0]);
    }
  }

  t1 = clock();
  debug_print("  %.4lf seconds to load line data\n", (t1-tt)/(double)CLOCKS_PER_SEC);
}

void LinesSorted::loadList()
{
  assert(view);
  clock_t t2,tt;
  tt=clock();

  debug_print("Loading up to %d lines into list...\n", total/2);

  //Create sorting array
  sorter.allocate(total/2, 2);

  //Element counts to actually plot (exclude filtered/hidden) per geom entry
  counts.clear();
  counts.resize(geom.size());

  //Index data for all vertices
  unsigned int voffset = 0;
  unsigned int offset = 0; //Offset into centroid list, include all hidden/filtered
  linecount = 0;
  for (unsigned int index = 0; index < geom.size(); voffset += geom[index]->count(), index++)
  {
    counts[index] = 0;
    if (!drawable(index)) 
    {
      offset += geom[index]->render->indices.size()/2; //Need to include hidden in centres offset
      continue;
    }

    //Calibrate colour maps on range for this surface
    //(also required for filtering by map)
    geom[index]->colourCalibrate();

    bool filter = geom[index]->draw->filterCache.size();
    bool opaque = geom[index]->opaqueCheck();
    for (unsigned int t = 0; t < geom[index]->render->indices.size()-2 && geom[index]->render->indices.size() > 2; t+=2, offset++)
    {
      //voffset is offset of the last vertex added to the vbo from the previous object
      assert(offset < total/2);
      if (!internal && filter)
      {
        //If any vertex filtered, skip whole tri
        if (geom[index]->filter(geom[index]->render->indices[t]) ||
            geom[index]->filter(geom[index]->render->indices[t+1]))
          continue;
      }
      sorter.buffer[linecount].index[0] = geom[index]->render->indices[t] + voffset;
      sorter.buffer[linecount].index[1] = geom[index]->render->indices[t+1] + voffset;
      sorter.buffer[linecount].distance = 0;

      //Create the default un-sorted index list
      memcpy(&sorter.indices[linecount*2], &sorter.buffer[linecount].index, sizeof(GLuint) * 2);

      //All opaque triangles at start
      if (opaque)
      {
        sorter.buffer[linecount].distance = USHRT_MAX;
        sorter.buffer[linecount].vertex = NULL;
      }
      else
      {
        //Line centre for depth sorting
        assert(offset < centres.size());
        sorter.buffer[linecount].vertex = centres[offset].ref();
      }
      linecount++;
      counts[index] += 2; //Element count

    }
    //printf("INDEX %d LINES %d ELS %d offset = %d, linecount = %d VOFFSET = %d\n", index, counts[index]/3, counts[index], offset, linecount, voffset);
  }

  t2 = clock();
  debug_print("  %.4lf seconds to load line list (%d)\n", (t2-tt)/(double)CLOCKS_PER_SEC, linecount);

  updateBoundingBox();

  if (session.global("sort"))
    sort();
}

//Depth sort the triangles before drawing, called whenever the viewing angle has changed
void LinesSorted::sort()
{
  //Skip if nothing to render or in 2d
  if (!sorter.buffer || linecount == 0 || elements == 0) return;
  clock_t t1,t2;
  t1 = clock();
  assert(sorter.buffer);

  //Calculate min/max distances from view plane
  float distanceRange[2];
  view->getMinMaxDistance(min, max, distanceRange, true);

  //Update eye distances, clamping int distance to integer between 1 and 65534
  float multiplier = (USHRT_MAX-1.0) / (distanceRange[1] - distanceRange[0]);
  unsigned int opaqueCount = 0;
  float fdistance;
  for (unsigned int i = 0; i < linecount; i++)
  {
    //Distance from viewing plane is -eyeZ
    //Max dist 65535 reserved for opaque triangles
    if (sorter.buffer[i].distance < USHRT_MAX)
    {
      assert(sorter.buffer[i].vertex);
      fdistance = view->eyePlaneDistance(sorter.buffer[i].vertex);
      //fdistance = view->eyeDistance(sorter.buffer[i].vertex);
      fdistance = std::min(distanceRange[1], std::max(distanceRange[0], fdistance)); //Clamp to range
      sorter.buffer[i].distance = (unsigned short)(multiplier * (fdistance - distanceRange[0]));
      //if (i%10000==0) printf("%d : centroid %f %f %f\n", i, sorter.buffer[i].vertex[0], sorter.buffer[i].vertex[1], sorter.buffer[i].vertex[2]);
      //Reverse as radix sort is ascending and we want to draw by distance descending
      //sorter.buffer[i].distance = USHRT_MAX - (unsigned short)(multiplier * (fdistance - mindist));
      //assert(sorter.buffer[i].distance >= 1 && sorter.buffer[i].distance <= USHRT_MAX);
    }
    else
      opaqueCount++;
  }
  t2 = clock();
  debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Skip sort if all opaque
  if (opaqueCount == linecount) 
  {
    debug_print("No sort necessary\n");
    return;
  }

  if (linecount > total/2)
  {
    //Will overflow sorter.buffer buffer (this should not happen!)
    fprintf(stderr, "Too many lines! %d > %d\n", linecount, total/2);
    linecount = total/2;
  }

  if (view->is3d)
  {
    //Depth sort using 2-byte key radix sort, 10 times faster than equivalent quicksort
    sorter.sort(linecount);
    t2 = clock();
    debug_print("  %.4lf seconds to sort %d lines\n", (t2-t1)/(double)CLOCKS_PER_SEC, linecount);
  }

  //Lock the update mutex, to allow updating the indexlist and prevent access while drawing
  t1 = clock();
  LOCK_GUARD(loadmutex);
  unsigned int idxcount = 0;
  for(int i=linecount-1; i>=0; i--)
  {
    assert(idxcount < 2 * linecount * sizeof(unsigned int));
    //Copy index bytes
    memcpy(&sorter.indices[idxcount], sorter.buffer[i].index, sizeof(GLuint) * 2);
    idxcount += 2;
    //if (i%100==0) printf("%d ==> %d,%d,%d\n", i, sorter.buffer[i].index[0], sorter.buffer[i].index[1]);
  }

  t2 = clock();
  debug_print("  %.4lf seconds to save %d line indices\n", (t2-t1)/(double)CLOCKS_PER_SEC, linecount*2);

  //Force update after sort
  sorter.changed = true;
}

//Reloads triangle indices, required after data update and depth sort
void LinesSorted::render()
{
  clock_t t1,t2;
  if (linecount == 0 || elements == 0) return;
  assert(sorter.buffer);

  t1 = clock();

  //Prepare the Index buffer
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
  glBindVertexArray(vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  GL_Error_Check;
  if (glIsBuffer(indexvbo))
  {
    //Lock the update mutex, to wait for any updates to the indexlist to finish
    LOCK_GUARD(loadmutex);
    //NOTE: linecount holds the filtered count of triangles to actually render as opposed to total in buffer
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, linecount * 2 * sizeof(GLuint), sorter.indices.data(), GL_DYNAMIC_DRAW);
    debug_print("  %d byte IBO uploaded %d indices (%d tris)\n", linecount*2 * sizeof(GLuint), linecount*2, linecount);
  }
  else
    abort_program("IBO creation failed\n");
  GL_Error_Check;

  t2 = clock();
  debug_print("  %.4lf seconds to upload %d indices (%d tris)\n", (t2-t1)/(double)CLOCKS_PER_SEC, sorter.indices.size(), linecount);
  t1 = clock();
  //After render(), copy filtered count to elements, indices.size() is unfiltered
  elements = linecount * 2;
}

void LinesSorted::draw()
{
  GL_Error_Check;
  if (elements == 0) return;

  //Re-render the triangles if view has rotated
  if (sorter.changed)
    render();

  setState(0); //Set global draw state (using first object)
  Shader_Ptr prog = session.shaders[lucLineType];

  // Draw using vertex buffer object
  clock_t t0 = clock();
  clock_t t1 = clock();
  double time;
  int stride = 3 * sizeof(float) + sizeof(Colour);   //3 vertices, + 32-bit colour
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (geom.size() > 0 && elements > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
  {
    //Setup vertex attributes
    GLint aPosition = prog->attribs["aVertexPosition"];
    GLint aColour = prog->attribs["aVertexColour"];
    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0); // Vertex x,y,z
    glEnableVertexAttribArray(aColour);
    glVertexAttribPointer(aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(3*sizeof(float)));   // rgba, offset 3 float

    unsigned int start = 0;
    unsigned int lnidx = 0;
    for (unsigned int index = 0; index<geom.size(); index++)
    {
      if (counts[index] == 0) continue;
      if (geom[index]->opaque)
      {
        setState(index); //Set draw state settings for this object
        //fprintf(stderr, "(%d %s) DRAWING OPAQUE LINES: %d (%d to %d)\n", index, geom[index]->draw->name().c_str(), counts[index]/2, start/2, (start+counts[index])/2);
        glDrawElements(GL_LINES, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
        start += counts[index];
      }
      else
        lnidx = index;
    }

    time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
    if (time > 0.005) debug_print("  %.4lf seconds to draw %d opaque lines\n", time, start);
    t1 = clock();

    //Draw remaining elements (transparent, depth sorted)
    if (start < (unsigned int)elements)
    {
      //fprintf(stderr, "(*) DRAWING TRANSPARENT LINES: %d\n", (elements-start)/2);
      //Set draw state settings for first non-opaque object
      //NOTE: per-object textures do not work with transparency!
      setState(lnidx);

      //Render all remaining lines - elements is the number of indices. 2 indices needed to make a single line
      glDrawElements(GL_LINES, elements-start, GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));

      time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
      if (time > 0.005) debug_print("  %.4lf seconds to draw %d transparent lines\n", time, (elements-start)/2);
    }

    glDisableVertexAttribArray(aPosition);
    glDisableVertexAttribArray(aColour);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
  if (time > 0.05)
    debug_print("  %.4lf seconds to draw lines\n", time);
  GL_Error_Check;
}

