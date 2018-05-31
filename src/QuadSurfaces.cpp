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

QuadSurfaces::QuadSurfaces(Session& session) : TriSurfaces(session)
{
  type = lucGridType;
}

QuadSurfaces::~QuadSurfaces()
{
  TriSurfaces::close();
}

void QuadSurfaces::update()
{
  clock_t t1,t2,tt;
  t1 = tt = clock();
  // Update and depth sort surfaces..

  tt=clock();
  if (geom.size() == 0) return;

  //Get element/quad count
  debug_print("Reloading and sorting %d quad surfaces...\n", geom.size());
  total = 0;
  surf_sort.clear();

  //Calculate min/max distances from viewer
  if (reload) updateBoundingBox();
  float distanceRange[2], modelView[16];
  view->getMinMaxDistance(min, max, distanceRange, modelView);

  unsigned int quadverts = 0;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    unsigned int quads = geom[i]->gridElements2d();
    quadverts += quads * 4;
    if (quads == 0) quadverts += geom[i]->elementCount();
    unsigned int v = geom[i]->count();
    if (v < 4) continue;
    total += v; //Actual vertices

    bool hidden = !drawable(i); //Save flags
    debug_print("Surface %d, quads %d hidden? %s\n", i, quadverts/4, (hidden ? "yes" : "no"));

    //Get corners of strip
    float* posmin = geom[i]->render->vertices[0];
    float* posmax = geom[i]->render->vertices[v - 1];
    float pos[3] = {posmin[0] + (posmax[0] - posmin[0]) * 0.5f,
                    posmin[1] + (posmax[1] - posmin[1]) * 0.5f,
                    posmin[2] + (posmax[2] - posmin[2]) * 0.5f
                   };

    //Calculate distance from viewing plane
    geom[i]->distance = view->eyeDistance(modelView, pos);
    if (geom[i]->distance < distanceRange[0]) distanceRange[0] = geom[i]->distance;
    if (geom[i]->distance > distanceRange[1]) distanceRange[1] = geom[i]->distance;
    //printf("%d)  %f %f %f distance = %f\n", i, pos[0], pos[1], pos[2], geom[i]->distance);
    surf_sort.push_back(Distance(i, geom[i]->distance));
  }
  if (total == 0) return;
  t2 = clock();
  debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Sort
  std::sort(surf_sort.begin(), surf_sort.end());
  t2 = clock();
  debug_print("  %.4lf seconds to sort\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Only reload the vbo data when required
  //Not needed when objects hidden/shown but required if colours changed
  //To force, set geometry->reload = true
  if (reload || elements != quadverts)
  {
    elements = quadverts;
    //Load & optimise the mesh data
    if (sorter.size != total || !allVertsFixed || counts.size() != geom.size())
      render();
    //Send the data to the GPU via VBO
    loadBuffers();
  }
}

void QuadSurfaces::sort()
{
  if (view && view->is3d)
    redraw = true; //Recalc cross section order
}

void QuadSurfaces::render()
{
  //Update quad index buffer
  clock_t t1,t2;

  //Prepare the Index buffer
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  GL_Error_Check;
  if (glIsBuffer(indexvbo))
  {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    debug_print("  %d byte IBO created for %d indices\n", elements * sizeof(GLuint), elements);
  }
  else
    abort_program("IBO creation failed!\n");
  GL_Error_Check;

  elements = 0;
  int offset = 0;
  int voffset = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    t1=clock();

    std::vector<Vec3d> normals(geom[index]->count());
    std::vector<GLuint> indices;

    //Quad indices
    unsigned int quads = geom[index]->gridElements2d();
    tricount += quads; //For debug messages
    bool vnormals = geom[index]->draw->properties["vertexnormals"];
    debug_print("%d x %d grid, quads %d, offset %d\n", geom[index]->width, geom[index]->height, quads, elements);
    if (vnormals && geom[index]->render->normals.size()/3 < geom[index]->count())
    {
      calcGridNormals(index, normals);
      geom[index]->render->normals.clear();
      geom[index]->render->normals.read(normals.size(), normals[0].ref());
    }

    //Special case: colour count == grid elements
    unsigned int vcount = geom[index]->count();
    unsigned int cc = geom[index]->colourCount();
    if (quads == 0)
    {
      //printf("Using existing indices/vertices %d/%d\n", geom[index]->render->indices.size(), geom[index]->count());
      quads = geom[index]->elementCount() / 4;
    }
    else if (cc > 0 && cc == (geom[index]->width-1) * (geom[index]->height-1))
    {
      if (vcount < quads*4)
      {
        //Re-vertex to separate elements - required to plot colours per grid quad
        std::vector<Vec3d> vertices(quads*4);
        calcGridVertices(index, vertices);
        geom[index]->render->vertices.clear();
        geom[index]->render->vertices.read(vertices.size(), &vertices[0]);
        //Wipe any indices as we have generated full vertex list
        geom[index]->render->indices.clear();
      }
    }
    //Require calculated indices if using shared vertices (grid order rather than anti-clockwise per-quad order)
    else if (quads && geom[index]->render->indices.size() == 0)
    //else if ((vcount < 4 || vcount < quads*4) && geom[index]->render->indices.size() != quads*4)
    {
      indices.resize(quads*4);
      calcGridIndices(index, indices, voffset);
      //Read new data and continue
      geom[index]->render->indices.clear();
      geom[index]->render->indices.read(indices.size(), &indices[0]);
    }

    //Vertex index offset
    voffset += vcount;
    //Index offset
    elements += quads*4;

    t1 = clock();
    unsigned int isize = geom[index]->render->indices.size();
    unsigned int bytes = isize*sizeof(GLuint);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, bytes, geom[index]->render->indices.ref());
    t2 = clock();
    debug_print("  %.4lf seconds to upload %d quad indices (%d - %d)\n", (t2-t1)/(double)CLOCKS_PER_SEC, isize, offset, bytes);
    t1 = clock();
    offset += bytes;
    GL_Error_Check;
  }
}

void QuadSurfaces::calcGridIndices(int i, std::vector<GLuint> &indices, unsigned int vertoffset)
{
  //Converts a set of grid vertices into quad indices
  if (geom[i]->height == 0 || geom[i]->width == 0) return;
  clock_t t1,t2;
  t1=clock();
  debug_print("Calculating indices for grid quad surface %d... ", i);
  bool flip = geom[i]->draw->properties["flip"];

  unsigned int o = 0;
  for (unsigned int j = 0 ; j < geom[i]->height-1; j++ )
  {
    for (unsigned int k = 0 ; k < geom[i]->width-1; k++ )
    {
      //Add indices for quad per grid element
      //Asssumes vertices arranged in a grid, top down, left-right (row 0 : 0,1,2... row 1 : 0,1,2 )
      //Counter-clockwise travel order to define face (U shape)
      unsigned int j0 = flip ? j+1 : j;
      unsigned int j1 = flip ? j : j+1;
      unsigned int offset0 = j0 * geom[i]->width + k;
      unsigned int offset1 = j1 * geom[i]->width + k;
      unsigned int offset2 = j1 * geom[i]->width + k + 1;
      unsigned int offset3 = j0 * geom[i]->width + k + 1;

      assert(offset2 + vertoffset < total);
      assert(o <= indices.size()-4);

      indices[o++] = offset0 + vertoffset;
      indices[o++] = offset1 + vertoffset;
      indices[o++] = offset2 + vertoffset;
      indices[o++] = offset3 + vertoffset;
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

void QuadSurfaces::calcGridVertices(int i, std::vector<Vec3d> &vertices)
{
  //Converts a set of grid vertices into quad vertices
  if (geom[i]->height == 0 || geom[i]->width == 0) return;
  clock_t t1,t2;
  t1=clock();
  debug_print("Calculating non-shared vertices for grid quad surface %d... ", i);
  bool flip = geom[i]->draw->properties["flip"];

  // Calculate vertices to render each grid element without shared vertices
  unsigned int o = 0;
  for (unsigned int j = 0 ; j < geom[i]->height-1; j++ )
  {
    for (unsigned int k = 0 ; k < geom[i]->width-1; k++ )
    {
      //Add vertices for quad per grid element
      //Asssumes vertices arranged in a grid, top down, left-right (row 0 : 0,1,2... row 1 : 0,1,2 )
      //Counter-clockwise travel order to define face (U shape)
      unsigned int j0 = flip ? j+1 : j;
      unsigned int j1 = flip ? j : j+1;
      unsigned int offset0 = j0 * geom[i]->width + k;
      unsigned int offset1 = j1 * geom[i]->width + k;
      unsigned int offset2 = j1 * geom[i]->width + k + 1;
      unsigned int offset3 = j0 * geom[i]->width + k + 1;

      //assert(offset2 + vertoffset < total);
      assert(o <= vertices.size()-4);

      //Quads...
      vertices[o++] = geom[i]->render->vertices[offset0];
      vertices[o++] = geom[i]->render->vertices[offset1];
      vertices[o++] = geom[i]->render->vertices[offset2];
      vertices[o++] = geom[i]->render->vertices[offset3];
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

void QuadSurfaces::draw()
{
  GL_Error_Check;
  // Draw using vertex buffer object
  clock_t t0 = clock();
  double time;
  int stride = 8 * sizeof(float) + sizeof(Colour);   //3+3+2 vertices, normals, texCoord + 32-bit colour
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (geom.size() > 0 && elements > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
  {
    glVertexPointer(3, GL_FLOAT, stride, (GLvoid*)0); // Load vertex x,y,z only
    glNormalPointer(GL_FLOAT, stride, (GLvoid*)(3*sizeof(float))); // Load normal x,y,z, offset 3 float
    glTexCoordPointer(2, GL_FLOAT, stride, (GLvoid*)(6*sizeof(float))); // Load texcoord x,y
    glColorPointer(4, GL_UNSIGNED_BYTE, stride, (GLvoid*)(8*sizeof(float)));   // Load rgba, offset 6 float
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    //Render in reverse sorted order
    for (int i=geom.size()-1; i>=0; i--)
    {
      unsigned int id = surf_sort[i].id;
      //if (!drawable(id)) continue;
      if (!drawable(id)) continue;
      unsigned int els = 0;
      //Get the offset
      unsigned int vstart = 0;
      unsigned int estart = 0;
      for (unsigned int g=0; g<geom.size(); g++)
      {
        els = geom[g]->gridElements2d() * 4;
        if (!els) els = geom[g]->elementCount();
        if (g == id) break;
        //Calculate offsets
        if (geom[g]->render->indices.size() > 0)
        {
          estart += els;                //Element index offset
          vstart += geom[g]->count();  //Vertex index offset
        }
        else
          vstart += els;                //Vertex index offset
      }

      //int id = i; //Sorting disabled
      setState(id); //Set draw state settings for this object

      //fprintf(stderr, "(%d, %s) DRAWING QUADS: %d (%d to %d) elements: %d\n", i, geom[i]->draw->name().c_str(), geom[i]->render->indices.size()/4, start/4, (start+geom[i]->render->indices.size())/4, elements);
      //printf("%d) rendered, distance = %f (%f)\n", id, geom[id]->distance, surf_sort[i].distance);

      if (geom[id]->render->indices.size() > 0)
      {
        //Draw with index buffer
        glDrawElements(GL_QUADS, els, GL_UNSIGNED_INT, (GLvoid*)(estart*sizeof(GLuint)));
        //printf("DRAW %d from %d by INDEX\n", els, estart);
      }
      else
      {
        //Draw directly from vertex buffer
        glDrawArrays(GL_QUADS, vstart, els);
        //printf("DRAW %d from %d by VERTEX\n", els, vstart);
      }
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;

  glBindTexture(GL_TEXTURE_2D, 0);

  time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
  if (time > 0.05)
    debug_print("  %.4lf seconds to draw quads\n", time);
  GL_Error_Check;
}

