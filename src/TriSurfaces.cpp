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

//Triangle centroid for depth sorting
#define centroid(v1,v2,v3) {centroids.emplace_back((v1[0]+v2[0]+v3[0])/3, (v1[1]+v2[1]+v3[1])/3, (v1[2]+v2[2]+v3[2])/3);}

TriSurfaces::TriSurfaces(Session& session) : Triangles(session)
{
  tricount = 0;
}

TriSurfaces::~TriSurfaces()
{
  Triangles::close();
  sorter.clear();
}

void TriSurfaces::close()
{
}

void TriSurfaces::update()
{
  // Update triangles...
  unsigned int drawelements = triCount();
  if (drawelements == 0) return;

  //Only reload the vbo data when required
  //Not needed when objects hidden/shown but required if colours changed
  if (centroids.size() != total/3 || vbo == 0 || (reload && (!allVertsFixed || internal)))
  {
    //Load & optimise the mesh data (including updating centroids)
    tricount = 0;
    loadMesh();
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
  if (sorter.size != total/3 || !allVertsFixed || counts.size() != geom.size())
    loadList();
}

void TriSurfaces::loadMesh()
{
  // Load & optimise triangle meshes...
  // If indices & normals provided, this simply calcs triangle centroids
  // If not, also optimises the mesh, removes duplicate vertices, calcs vertex normals and colours
  clock_t t1,t2,tt;
  tt=clock();

  debug_print("Loading %d triangles...\n", total/3);

  //Calculate normals, delete duplicate verts, calc indices
  GLuint unique = 0;
  elements = 0;
  //Reset triangle centroid data
  centroids.clear();
  centroids.reserve(total/3);
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    bool vnormals = geom[index]->draw->properties["vertexnormals"];
    if (geom[index]->count() == 0) continue;

    //Has index data, simply load the triangles
    if (geom[index]->render->indices.size() > 0)
    {
      unsigned i1, i2, i3;
      for (unsigned int j=0; j < geom[index]->render->indices.size()-2; j += 3)
      {
        i1 = geom[index]->render->indices[j];
        i2 = geom[index]->render->indices[j+1];
        i3 = geom[index]->render->indices[j+2];

        centroid(geom[index]->render->vertices[i1],
                 geom[index]->render->vertices[i2],
                 geom[index]->render->vertices[i3]);

        elements += 3;
      }

      //Increment by vertex count (all vertices are unique as mesh is pre-optimised)
      //elements += counts[index]; //geom[index]->render->indices.size();
      unique += geom[index]->render->vertices.size() / 3;

      if (vnormals && geom[index]->render->normals.size() == 0)
        calcTriangleNormalsWithIndices(index);
      continue;
    }

    //Add vertices to a vector with indices
    //Sort vertices vector with std::sort & custom compare sort( vec.begin(), vec.end() );
    //Iterate, for duplicates replace indices with index of first
    //Remove duplicate vertices Triangles stored as list of indices
    unsigned int hasColours = geom[index]->colourCount();
    bool vertColour = hasColours && (hasColours >= geom[index]->count());
    t1=tt=clock();

    //Add vertices to vector
    std::vector<Vertex> verts(geom[index]->count());
    std::vector<Vec3d> normals(vnormals ? geom[index]->count() : 0);
    std::vector<GLuint> indices;
    for (unsigned int j=0; j < geom[index]->count(); j++)
    {
      verts[j].id = verts[j].ref = j;
      verts[j].vert = geom[index]->render->vertices[j];
    }
    t2 = clock();
    debug_print("  %.4lf seconds to add to sort vector\n", (t2-t1)/(double)CLOCKS_PER_SEC);

    int triverts = 0;
    bool grid = (geom[index]->width * geom[index]->height == geom[index]->count());

    //Mesh optimisation (duplicate vertex replacement) must be disabled with tex coords
    //(Vertices may be shared by triangles, but each needs it's own texcoord and texcoords are stored with vertices)
    bool hasTexCoords = geom[index]->render->texCoords.size()/2 == geom[index]->count();
    bool optimise = !hasTexCoords && vnormals && geom[index]->draw->properties["optimise"];

    if (grid)
    {
      //Structured mesh grid, 2 triangles per element, 3 indices per tri
      int els = geom[index]->gridElements2d();
      triverts = els * 6;
      indices.resize(triverts);
      if (vnormals && geom[index]->render->normals.size() == 0)
        calcGridNormals(index, normals);
      calcGridIndices(index, indices);
      unique += geom[index]->count(); //For calculating index offset (voffset)
      elements += triverts;
    }
    else
    {
      //Unstructured mesh, 1 index per vertex
      triverts = geom[index]->count();
      indices.resize(triverts);
      if (vnormals) calcTriangleNormals(index, verts, normals, optimise);
      elements += triverts;
    }

    //Now have list of vertices sorted by vertex pos with summed normals and references of duplicates replaced
    t1 = clock();

    if (grid)
    {
      //Replace normals
      if (vnormals)
      {
        geom[index]->_normals = std::make_shared<Coord3DValues>();
        read(geom[index], normals.size(), lucNormalData, &normals[0]);
      }
    }
    else
    {
      //Switch out the optimised vertices and normals with the old data stores
      // - must maintain a copy of old containers to read from in optimisation
      //   otherwise will get destroyed when new containers replace them
      Float3_Ptr old_vertices = geom[index]->_vertices;
      Float3_Ptr old_normals = geom[index]->_normals;
      Float2_Ptr old_texCoords = geom[index]->_texCoords;
      UInt_Ptr old_indices = geom[index]->_indices;

      //Create a new stores for replaced values
      geom[index]->_vertices = std::make_shared<Coord3DValues>();
      geom[index]->_indices = std::make_shared<UIntValues>();

      if (hasTexCoords)
        geom[index]->_texCoords = std::make_shared<Coord2DValues>();

      //Vertices, normals, indices (and colour values) may be replaced
      //Rest just get copied over
      if (vnormals)
        geom[index]->_normals = std::make_shared<Coord3DValues>();

      //Update references
      geom[index]->setRenderData();

      //Recreate value data as optimised version is smaller, re-load necessary values
      FloatValues* oldvalues = geom[index]->colourData();
      Values_Ptr newvalues = NULL;
      if (optimise && oldvalues)
      {
        newvalues = Values_Ptr(new FloatValues());
        newvalues->label = oldvalues->label;
        newvalues->minimum = oldvalues->minimum;
        newvalues->maximum = oldvalues->maximum;
      }

      for (unsigned int v=0; v<verts.size(); v++)
      {
        //Re-write optimised data with unique vertices only
        if (!optimise || verts[v].id == verts[v].ref)
        {
          //Reference id == self, not a duplicate

          //Average final colour value
          if (vertColour && newvalues)
          {
            if (verts[v].vcount > 1)
              oldvalues->value[verts[v].id] /= verts[v].vcount;
            newvalues->read(1, &oldvalues->value[verts[v].id]);
          }

          //Save an index lookup entry (Grid indices loaded in previous step)
          indices[verts[v].id] = geom[index]->count();

          //Replace verts & normals
          read(geom[index], 1, lucVertexData, verts[v].vert);
          if (vnormals)
            read(geom[index], 1, lucNormalData, normals[verts[v].ref].ref());

          if (hasTexCoords)
            read(geom[index], 1, lucTexCoordData, &((*old_texCoords)[verts[v].id][0]));

          unique++;
        }
        else
        {
          //Duplicate vertex, use index reference
          indices[verts[v].id] = indices[verts[v].ref];
        }
      }

      if (newvalues)
        geom[index]->values[geom[index]->draw->colourIdx] = newvalues;
    }

    //Update the rendering references as some containers have been replaced
    geom[index]->setRenderData();

    t2 = clock();
    debug_print("  %.4lf seconds to normalise (and re-buffer)\n", (t2-t1)/(double)CLOCKS_PER_SEC);
    t1 = clock();

    //Read the indices for loading sort list and later use (json export etc)
    geom[index]->render->indices.read(indices.size(), &indices[0]);

    t2 = clock();
    debug_print("  %.4lf seconds to reload & clean up\n", (t2-t1)/(double)CLOCKS_PER_SEC);
    t1 = clock();
    debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
  }

  //debug_print("  *** There were %d unique vertices out of %d total.\n", unique, total);
  t2 = clock();
  debug_print("  %.4lf seconds to optimise triangle mesh\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void TriSurfaces::loadList()
{
  assert(view);
  clock_t t2,tt;
  tt=clock();

  debug_print("Loading up to %d triangles into list...\n", total/3);

  //Create sorting array
  sorter.allocate(total/3, 3);

  //Element counts to actually plot (exclude filtered/hidden) per geom entry
  counts.clear();
  counts.resize(geom.size());

  //Index data for all vertices
  unsigned int voffset = 0;
  unsigned int offset = 0; //Offset into centroid list, include all hidden/filtered
  tricount = 0;
  for (unsigned int index = 0; index < geom.size(); voffset += geom[index]->count(), index++)
  {
    counts[index] = 0;
    if (!drawable(index)) 
    {
      offset += geom[index]->render->indices.size()/3; //Need to include hidden in centroid offset
      continue;
    }

    //Calibrate colour maps on range for this surface
    //(also required for filtering by map)
    geom[index]->colourCalibrate();

    bool filter = geom[index]->draw->filterCache.size();
    bool opaque = geom[index]->opaqueCheck();
    for (unsigned int t = 0; t < geom[index]->render->indices.size()-2 && geom[index]->render->indices.size() > 2; t+=3, offset++)
    {
      //voffset is offset of the last vertex added to the vbo from the previous object
      assert(offset < total/3);
      if (!internal && filter)
      {
        //If any vertex filtered, skip whole tri
        if (geom[index]->filter(geom[index]->render->indices[t]) ||
            geom[index]->filter(geom[index]->render->indices[t+1]) ||
            geom[index]->filter(geom[index]->render->indices[t+2]))
          continue;
      }
      sorter.buffer[tricount].index[0] = geom[index]->render->indices[t] + voffset;
      sorter.buffer[tricount].index[1] = geom[index]->render->indices[t+1] + voffset;
      sorter.buffer[tricount].index[2] = geom[index]->render->indices[t+2] + voffset;
      sorter.buffer[tricount].distance = 0;

      //Create the default un-sorted index list
      memcpy(&sorter.indices[tricount*3], &sorter.buffer[tricount].index, sizeof(GLuint) * 3);

      //All opaque triangles at start
      if (opaque)
      {
        sorter.buffer[tricount].distance = USHRT_MAX;
        sorter.buffer[tricount].vertex = NULL;
      }
      else
      {
        //Triangle centroid for depth sorting
        assert(offset < centroids.size());
        sorter.buffer[tricount].vertex = centroids[offset].ref();
      }
      tricount++;
      counts[index] += 3; //Element count

    }
    //printf("INDEX %d TRIS %d ELS %d offset = %d, tricount = %d VOFFSET = %d\n", index, counts[index]/3, counts[index], offset, tricount, voffset);
  }

  t2 = clock();
  debug_print("  %.4lf seconds to load triangle list (%d)\n", (t2-tt)/(double)CLOCKS_PER_SEC, tricount);

  updateBoundingBox();

  if (session.global("sort"))
    sort();
}

void TriSurfaces::calcTriangleNormals(int index, std::vector<Vertex> &verts, std::vector<Vec3d> &normals, bool optimise)
{
  clock_t t1,t2;
  t1 = clock();
  debug_print("Calculating normals for triangle surface %d size %d\n", index, geom[index]->render->vertices.size()/3);
  //Vertex elimination currently only works for per-vertex colouring, 
  // if less colour values provided, must precalc own indices to skip this step 
  unsigned int hasColours = geom[index]->colourCount();
  bool vertColour = (hasColours && hasColours >= geom[index]->render->vertices.size()/3);
  if (hasColours && !vertColour) debug_print("WARNING: Not enough colour values for per-vertex normalisation! %d < %d\n", hasColours, geom[index]->render->vertices.size()/3);
  bool normal = geom[index]->draw->properties["vertexnormals"];
  //Calculate face normals for each triangle and copy to each face vertex
  for (unsigned int v=0; v<verts.size()-2 && verts.size() > 2; v += 3)
  {
    //Copies for each vertex
    if (normal)
    {
      normals[v] = vectorNormalToPlane(verts[v].vert, verts[v+1].vert, verts[v+2].vert);
      normals[v+1] = Vec3d(normals[v]);
      normals[v+2] = Vec3d(normals[v]);
    }

    //Calc triangle centroid for sorting
    centroid(verts[v].vert, verts[v+1].vert, verts[v+2].vert);
  }
  t2 = clock();
  debug_print("  %.4lf seconds to calc facet normals\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Sort by vertex
  std::sort(verts.begin(), verts.end());
  t2 = clock();
  debug_print("  %.4lf seconds to sort %d verts\n", (t2-t1)/(double)CLOCKS_PER_SEC, verts.size());
  t1 = clock();

  //Now have list of vertices sorted by vertex pos
  //Search for duplicates and replace references to normals
  int match = 0;
  int dupcount = 0;
  for (unsigned int v=1; v<verts.size(); v++)
  {
    if (verts[match] == verts[v])
    {
      // If the angle between a given face normal and the face normal
      // associated with the first triangle in the list of triangles for the
      // current vertex is greater than a specified angle, normal is not added
      // to average normal calculation and the corresponding vertex is given
      // the facet normal. This preserves hard edges, specific angle to
      // use depends on the model, but 90 degrees is usually a good start.

      // cosine of angle between vectors = (v1 . v2) / |v1|.|v2|
      float angle = normal ? RAD2DEG * normals[verts[v].id].angle(normals[verts[match].id]) : 0;
      //debug_print("angle %f ", angle);
      //Don't include vertices in the sum if angle between normals too sharp
      if (angle < 90)
      {
        //Found a duplicate, replace reference idx (original retained in "id")
        verts[v].ref = verts[match].ref;
        dupcount++;

        //Add this normal to matched normal
        if (normal)
          normals[verts[match].id] += normals[verts[v].id];

        //Colour value, add to matched
        if (optimise && vertColour && geom[index]->colourData())
          geom[index]->colourData()->value[verts[match].id] += geom[index]->colourData()->value[verts[v].id];

        verts[match].vcount++;
        verts[v].vcount = 0;
      }
    }
    else
    {
      //First occurrence, following comparisons will be against this vertex...
      match = v;
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds to replace duplicates (%d/%d) \n", (t2-t1)/(double)CLOCKS_PER_SEC, dupcount, verts.size());
  t1 = clock();

  for (unsigned int v=0; v<verts.size(); v++)
  {
    //Normalise final vectors
    if (verts[v].id == verts[v].ref)
      normals[verts[v].id].normalise();
  }

  t2 = clock();
  debug_print("  %.4lf seconds to normalise\n", (t2-t1)/(double)CLOCKS_PER_SEC);


}

void TriSurfaces::calcTriangleNormalsWithIndices(int index)
{
  clock_t t1,t2;
  t1 = clock();
  debug_print("Calculating normals for triangle surface %d size %d\n", index, geom[index]->render->indices.size()/3);
  //Calculate face normals for each triangle and copy to each face vertex
  std::vector<Vec3d> normals(geom[index]->count());
  for (unsigned int i=0; i<geom[index]->render->indices.size()-2 && geom[index]->render->indices.size() > 2; i += 3)
  {
    //Copies for each vertex
    GLuint i1 = geom[index]->render->indices[i];
    GLuint i2 = geom[index]->render->indices[i+1];
    GLuint i3 = geom[index]->render->indices[i+2];

    Vec3d normal = vectorNormalToPlane(geom[index]->render->vertices[i1], 
                                       geom[index]->render->vertices[i2],
                                       geom[index]->render->vertices[i3]);

    normals[i1] += normal;
    normals[i2] += normal;
    normals[i3] += normal;
  }
  t2 = clock();
  debug_print("  %.4lf seconds to calc facet normals\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Normalise to combine and load normal data
  for (unsigned int n=0; n<normals.size(); n++)
  {
    normals[n].normalise();
    read(geom[index], 1, lucNormalData, normals[n].ref());
  }
  t2 = clock();
  debug_print("  %.4lf seconds to normalise (%d) \n", (t2-t1)/(double)CLOCKS_PER_SEC, normals.size());
}

void TriSurfaces::calcGridNormals(int i, std::vector<Vec3d> &normals)
{
  //Normals: calculate from surface geometry
  clock_t t1,t2;
  t1=clock();
  debug_print("Calculating normals for grid surface %d... ", i);
  bool genTexCoords = (geom[i]->draw->useTexture(geom[i]->texture) && geom[i]->render->texCoords.size() == 0);

  // Calc pre-vertex normals for irregular meshes by averaging four surrounding triangle facet normals
  int n = 0;
  for (unsigned int j = 0 ; j < geom[i]->height; j++ )
  {
    for (unsigned int k = 0 ; k < geom[i]->width; k++ )
    {
      //Tex coords
      if (genTexCoords)
      {
        float texCoord[2] = {k / (float)(geom[i]->width-1), j / (float)(geom[i]->height-1)};
        read(geom[i], 1, lucTexCoordData, texCoord);
      }

      // Get sum of normal vectors
      if (j > 0)
      {
        if (k > 0)
        {
          // Look back
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j + k],
                                            geom[i]->render->vertices[geom[i]->width * (j-1) + k],
                                            geom[i]->render->vertices[geom[i]->width * j + k-1]);
        }

        if (k < geom[i]->width - 1)
        {
          // Look back in x, forward in y
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j + k],
                                            geom[i]->render->vertices[geom[i]->width * j + k+1],
                                            geom[i]->render->vertices[geom[i]->width * (j-1) + k]);
        }
      }

      if (j <  geom[i]->height - 1)
      {
        if (k > 0)
        {
          // Look forward in x, back in y
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j + k],
                                            geom[i]->render->vertices[geom[i]->width * j + k-1],
                                            geom[i]->render->vertices[geom[i]->width * (j+1) + k]);
        }

        if (k < geom[i]->width - 1)
        {
          // Look forward
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j + k],
                                            geom[i]->render->vertices[geom[i]->width * (j+1) + k],
                                            geom[i]->render->vertices[geom[i]->width * j + k+1]);
        }
      }

      //Normalise to average
      normals[n].normalise();
      //Copy directly into normal block
      //memcpy(geom[i]->render->normals[j * geom[i]->width + k], normal.ref(), sizeof(float) * 3);
      n++;
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

void TriSurfaces::calcGridIndices(int i, std::vector<GLuint> &indices)
{
  //Normals: calculate from surface geometry
  clock_t t1,t2;
  t1=clock();
  debug_print("Calculating indices for grid tri surface %d... ", i);
  bool flip = geom[i]->draw->properties["flip"];

  // Calc pre-vertex normals for irregular meshes by averaging four surrounding triangle facet normals
  unsigned int o = 0;
  for (unsigned int j = 0 ; j < geom[i]->height-1; j++ )
  {
    for (unsigned int k = 0 ; k < geom[i]->width-1; k++ )
    {
      //Add indices for two triangles per grid element
      unsigned int j0 = flip ? j+1 : j;
      unsigned int j1 = flip ? j : j+1;
      unsigned int offset0 = j0 * geom[i]->width + k;
      unsigned int offset1 = j1 * geom[i]->width + k;
      unsigned int offset2 = j0 * geom[i]->width + k + 1;
      unsigned int offset3 = j1 * geom[i]->width + k + 1;
      assert(o <= indices.size()-6);
      //Tri 1
      centroid(geom[i]->render->vertices[offset0], geom[i]->render->vertices[offset1], geom[i]->render->vertices[offset2]);
      indices[o++] = offset0;
      indices[o++] = offset1;
      indices[o++] = offset2;
      //Tri 2
      centroid(geom[i]->render->vertices[offset1], geom[i]->render->vertices[offset3], geom[i]->render->vertices[offset2]);
      indices[o++] = offset1;
      indices[o++] = offset3;
      indices[o++] = offset2;
    }
  }
  t2 = clock();
  debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

//Depth sort the triangles before drawing, called whenever the viewing angle has changed
void TriSurfaces::sort()
{
  //Skip if nothing to render or in 2d
  if (!sorter.buffer || tricount == 0 || elements == 0) return;
  clock_t t1,t2;
  t1 = clock();
  assert(sorter.buffer);

  //Calculate min/max distances from view plane
  float distanceRange[2], modelView[16];
  view->getMinMaxDistance(min, max, distanceRange, modelView, true);

  //Update eye distances, clamping int distance to integer between 1 and 65534
  float multiplier = (USHRT_MAX-1.0) / (distanceRange[1] - distanceRange[0]);
  unsigned int opaqueCount = 0;
  float fdistance;
  for (unsigned int i = 0; i < tricount; i++)
  {
    //Distance from viewing plane is -eyeZ
    //Max dist 65535 reserved for opaque triangles
    if (sorter.buffer[i].distance < USHRT_MAX)
    {
      assert(sorter.buffer[i].vertex);
      fdistance = eyePlaneDistance(modelView, sorter.buffer[i].vertex);
      //fdistance = view->eyeDistance(modelView, sorter.buffer[i].vertex);
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
  if (opaqueCount == tricount) 
  {
    debug_print("No sort necessary\n");
    return;
  }

  if (tricount > total/3)
  {
    //Will overflow sorter.buffer buffer (this should not happen!)
    fprintf(stderr, "Too many triangles! %d > %d\n", tricount, total/3);
    tricount = total/3;
  }

  if (view->is3d)
  {
    //Depth sort using 2-byte key radix sort, 10 times faster than equivalent quicksort
    sorter.sort(tricount);
    t2 = clock();
    debug_print("  %.4lf seconds to sort %d triangles\n", (t2-t1)/(double)CLOCKS_PER_SEC, tricount);
  }

  //Lock the update mutex, to allow updating the indexlist and prevent access while drawing
  t1 = clock();
  std::lock_guard<std::mutex> guard(loadmutex);
  unsigned int idxcount = 0;
  for(int i=tricount-1; i>=0; i--)
  {
    assert(idxcount < 3 * tricount * sizeof(unsigned int));
    //Copy index bytes
    memcpy(&sorter.indices[idxcount], sorter.buffer[i].index, sizeof(GLuint) * 3);
    idxcount += 3;
    //if (i%100==0) printf("%d ==> %d,%d,%d\n", i, sorter.buffer[i].index[0], sorter.buffer[i].index[1], sorter.buffer[i].index[2]);
  }

  t2 = clock();
  debug_print("  %.4lf seconds to save %d triangle indices\n", (t2-t1)/(double)CLOCKS_PER_SEC, tricount*3);

  //Force update after sort
  sorter.changed = true;
}

//Reloads triangle indices, required after data update and depth sort
void TriSurfaces::render()
{
  clock_t t1,t2;
  if (tricount == 0 || elements == 0) return;
  assert(sorter.buffer);

  t1 = clock();

  //Prepare the Index buffer
  if (!indexvbo)
    glGenBuffers(1, &indexvbo);

  //Always set data size again in case changed
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  GL_Error_Check;
  if (glIsBuffer(indexvbo))
  {
    //Lock the update mutex, to wait for any updates to the indexlist to finish
    std::lock_guard<std::mutex> guard(loadmutex);
    //NOTE: tricount holds the filtered count of triangles to actually render as opposed to total in buffer
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, tricount * 3 * sizeof(GLuint), sorter.indices.data(), GL_DYNAMIC_DRAW);
    debug_print("  %d byte IBO uploaded %d indices (%d tris)\n", tricount*3 * sizeof(GLuint), tricount*3, tricount);
  }
  else
    abort_program("IBO creation failed\n");
  GL_Error_Check;

  t2 = clock();
  debug_print("  %.4lf seconds to upload %d indices (%d tris)\n", (t2-t1)/(double)CLOCKS_PER_SEC, sorter.indices.size(), tricount);
  t1 = clock();
  //After render(), copy filtered count to elements, indices.size() is unfiltered
  elements = tricount * 3;
}

void TriSurfaces::draw()
{
  GL_Error_Check;
  if (elements == 0) return;

  //Re-render the triangles if view has rotated
  if (sorter.changed)
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
    int offset = 0;
    glVertexPointer(3, GL_FLOAT, stride, (GLvoid*)0); // Load vertex x,y,z only
    glEnableClientState(GL_VERTEX_ARRAY);
    offset += 3;
    glNormalPointer(GL_FLOAT, stride, (GLvoid*)(offset*sizeof(float))); // Load normal x,y,z, offset 3 float
    glEnableClientState(GL_NORMAL_ARRAY);
    offset += 3;
    glTexCoordPointer(2, GL_FLOAT, stride, (GLvoid*)(offset*sizeof(float))); // Load texcoord x,y
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    offset += 2;
    glColorPointer(4, GL_UNSIGNED_BYTE, stride, (GLvoid*)(offset*sizeof(float)));   // Load rgba, offset 6 float
    glEnableClientState(GL_COLOR_ARRAY);

    unsigned int start = 0;
    int tridx = 0;
    for (int index = 0; index<geom.size(); index++)
    {
      if (counts[index] == 0) continue;
      if (geom[index]->opaque)
      {
        setState(index); //Set draw state settings for this object
        //fprintf(stderr, "(%d %s) DRAWING OPAQUE TRIANGLES: %d (%d to %d)\n", index, geom[index]->draw->name().c_str(), counts[index]/3, start/3, (start+counts[index])/3);
        glDrawElements(GL_TRIANGLES, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
        start += counts[index];
      }
      else
        tridx = index;
    }

    time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
    if (time > 0.005) debug_print("  %.4lf seconds to draw %d opaque triangles\n", time, start);
    t1 = clock();

    //Draw remaining elements (transparent, depth sorted)
    if (start < (unsigned int)elements)
    {
      //fprintf(stderr, "(*) DRAWING TRANSPARENT TRIANGLES: %d\n", (elements-start)/3);
      //Set draw state settings for first non-opaque object
      //NOTE: per-object textures do not work with transparency!
      setState(tridx);

      //Render all remaining triangles - elements is the number of indices. 3 indices needed to make a single triangle
      glDrawElements(GL_TRIANGLES, elements-start, GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));

      time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
      if (time > 0.005) debug_print("  %.4lf seconds to draw %d transparent triangles\n", time, (elements-start)/3);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  //Restore state
  //glEnable(GL_LIGHTING);
  //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  //glDisable(GL_CULL_FACE);
  glBindTexture(GL_TEXTURE_2D, 0);

  time = ((clock()-t0)/(double)CLOCKS_PER_SEC);
  if (time > 0.05)
    debug_print("  %.4lf seconds to draw triangles\n", time);
  GL_Error_Check;
}

