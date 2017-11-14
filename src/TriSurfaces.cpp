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

TriSurfaces::TriSurfaces(DrawState& drawstate) : Triangles(drawstate)
{
  tricount = 0;
  tidx = swap = NULL;
  indexlist = NULL;
}

void TriSurfaces::close()
{
  if (!drawstate.global("gpucache"))
    Triangles::close();

  if (tidx)
    delete[] tidx;
  if (swap)
    delete[] swap;
  if (indexlist)
    delete[] indexlist;

  tidx = swap = NULL;
  indexlist = NULL;
}

void TriSurfaces::update()
{
  // Update triangles...
  unsigned int lastcount = total;
  unsigned int drawelements = triCount();
  if (drawelements == 0) return;

  //Only reload the vbo data when required
  //Not needed when objects hidden/shown but required if colours changed
  //if ((lastcount != total && reload) || !tidx)
  if (lastcount != total || reload || vbo == 0)
  {
    //Load & optimise the mesh data (on first load and if total changes)
    if (!tidx || lastcount != total)
      loadMesh();

    //Send the data to the GPU via VBO
    loadBuffers();
  }

  //Reload the list if count changes
  if (reload || !tidx || tricount == 0 || tricount*3 != idxcount)
    loadList();

  if (reload)
    idxcount = 0;
}

void TriSurfaces::loadMesh()
{
  // Load & optimise triangle meshes...
  // If indices & normals provided, this simply calcs triangle centroids
  // If not, also optimises the mesh, removes duplicate vertices, calcs vertex normals and colours
  clock_t t1,t2,tt;
  tt=clock();

  debug_print("Loading %d triangles...\n", total);

  //Calculate normals, delete duplicate verts, calc indices
  GLuint unique = 0;
  tricount = 0;
  elements = 0;
  //Reset triangle centroid data
  centroids.clear();
  centroids.reserve(total);
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
      calcTriangleNormals(index, verts, normals);
      elements += triverts;
    }

    //Now have list of vertices sorted by vertex pos with summed normals and references of duplicates replaced
    t1 = clock();

    if (grid)
    {
      //Replace normals
      if (vnormals)
      {
        geom[index]->render->normals = Coord3DValues();
        read(geom[index], normals.size(), lucNormalData, &normals[0]);
      }
    }
    else
    {
      //Switch out the optimised vertices and normals with the old data stores
      // - must maintain a copy of old RenderData to read from in optimisation
      //   otherwise it will have been destroyed when new RenderData created
      Render_Ptr olddata = geom[index]->render;
      //Create a new store and copy original values
      geom[index]->render = std::make_shared<RenderData>();

      //Vertices, normals, indices (and colour values) may be replaced
      //Rest just get copied over
      if (!vnormals)
        geom[index]->render->normals = olddata->normals;
      geom[index]->render->vectors = olddata->vectors;
      geom[index]->render->texCoords = olddata->texCoords;
      geom[index]->render->colours = olddata->colours;
      geom[index]->render->luminance = olddata->luminance;
      geom[index]->render->rgb = olddata->rgb;

      //Recreate value data as optimised version is smaller, re-load necessary values
      FloatValues* oldvalues = geom[index]->colourData();
      Values_Ptr newvalues = NULL;
      if (oldvalues)
      {
        newvalues = Values_Ptr(new FloatValues());
        newvalues->label = oldvalues->label;
        newvalues->minimum = oldvalues->minimum;
        newvalues->maximum = oldvalues->maximum;
      }

      bool optimise = vnormals && geom[index]->draw->properties["optimise"];
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
          {
            //Normalise final vector first
            normals[verts[v].id].normalise();
            read(geom[index], 1, lucNormalData, normals[verts[v].id].ref());
          }
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

  //debug_print("  *** There were %d unique vertices out of %d total. Buffer allocated for %d\n", unique, total*3, bsize/datasize);
  t2 = clock();
  debug_print("  %.4lf seconds to optimise triangle mesh\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void TriSurfaces::loadList()
{
  assert(view);
  clock_t t2,tt;
  tt=clock();

  debug_print("Loading up to %d triangles into list...\n", total);

  //Create sorting array
  if (tidx) delete[] tidx;
  tidx = new TIndex[total];
  if (swap) delete[] swap;
  swap = new TIndex[total];
  if (indexlist) delete[] indexlist;
  indexlist = new unsigned int[total*3];
  if (tidx == NULL || swap == NULL || indexlist == NULL) abort_program("Memory allocation error (failed to allocate %d bytes)", sizeof(TIndex) * total * 2 + sizeof(unsigned int) * total*3);

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
    for (unsigned int t = 0; t < geom[index]->render->indices.size()-2 && geom[index]->render->indices.size() > 2; t+=3, offset++)
    {
      //voffset is offset of the last vertex added to the vbo from the previous object
      assert(offset < total);
      if (!internal && filter)
      {
        //If any vertex filtered, skip whole tri
        if (geom[index]->filter(geom[index]->render->indices[t]) ||
            geom[index]->filter(geom[index]->render->indices[t+1]) ||
            geom[index]->filter(geom[index]->render->indices[t+2]))
          continue;
      }
      tidx[tricount].index[0] = geom[index]->render->indices[t] + voffset;
      tidx[tricount].index[1] = geom[index]->render->indices[t+1] + voffset;
      tidx[tricount].index[2] = geom[index]->render->indices[t+2] + voffset;
      tidx[tricount].distance = 0;

      //Create the default un-sorted index list
      memcpy(&indexlist[tricount*3], &tidx[tricount].index, sizeof(GLuint) * 3);

      //All opaque triangles at start
      if (geom[index]->opaque)
      {
        tidx[tricount].distance = USHRT_MAX;
        tidx[tricount].vertex = NULL;
      }
      else
      {
        //Triangle centroid for depth sorting
        assert(offset < centroids.size());
        tidx[tricount].vertex = centroids[offset].ref();
      }
      tricount++;
      counts[index] += 3; //Element count

    }
    //printf("INDEX %d TRIS %d ELS %d offset = %d, tricount = %d VOFFSET = %d\n", index, counts[index]/3, counts[index], offset, tricount, voffset);
  }

  t2 = clock();
  debug_print("  %.4lf seconds to load triangle list (%d)\n", (t2-tt)/(double)CLOCKS_PER_SEC, tricount);

  if (drawstate.global("sort"))
    sort();
}

void TriSurfaces::calcTriangleNormals(int index, std::vector<Vertex> &verts, std::vector<Vec3d> &normals)
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
  bool optimise = normal && geom[index]->draw->properties["optimise"];
  for (unsigned int v=1; v<verts.size(); v++)
  {
    if (optimise && verts[match] == verts[v])
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
        if (vertColour && geom[index]->colourData())
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

  // Calc pre-vertex normals for irregular meshes by averaging four surrounding triangle facet normals
  unsigned int o = 0;
  for (unsigned int j = 0 ; j < geom[i]->height-1; j++ )
  {
    for (unsigned int k = 0 ; k < geom[i]->width-1; k++ )
    {
      //Add indices for two triangles per grid element
      unsigned int offset0 = j * geom[i]->width + k;
      unsigned int offset1 = (j+1) * geom[i]->width + k;
      unsigned int offset2 = j * geom[i]->width + k + 1;
      unsigned int offset3 = (j+1) * geom[i]->width + k + 1;
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
  if (!tidx || tricount == 0 || elements == 0) return;
  clock_t t1,t2;
  t1 = clock();
  assert(tidx);

  //Calculate min/max distances from view plane
  calcDistanceRange(true);

  //Update eye distances, clamping int distance to integer between 1 and 65534
  float multiplier = (USHRT_MAX-1.0) / (view->maxdist - view->mindist);
  unsigned int opaqueCount = 0;
  float fdistance;
  for (unsigned int i = 0; i < tricount; i++)
  {
    //Distance from viewing plane is -eyeZ
    //Max dist 65535 reserved for opaque triangles
    if (tidx[i].distance < USHRT_MAX)
    {
      assert(tidx[i].vertex);
      fdistance = eyePlaneDistance(view->modelView, tidx[i].vertex);
      //fdistance = view->eyeDistance(tidx[i].vertex);
      fdistance = std::min(view->maxdist, std::max(view->mindist, fdistance)); //Clamp to range
      tidx[i].distance = (unsigned short)(multiplier * (fdistance - view->mindist));
      //if (i%10000==0) printf("%d : centroid %f %f %f\n", i, tidx[i].vertex[0], tidx[i].vertex[1], tidx[i].vertex[2]);
      //Reverse as radix sort is ascending and we want to draw by distance descending
      //tidx[i].distance = USHRT_MAX - (unsigned short)(multiplier * (fdistance - mindist));
      //assert(tidx[i].distance >= 1 && tidx[i].distance <= USHRT_MAX);
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

  if (tricount > total)
  {
    //Will overflow tidx buffer (this should not happen!)
    fprintf(stderr, "Too many triangles! %d > %d\n", tricount, total);
    tricount = total;
  }

  if (view->is3d)
  {
    //Depth sort using 2-byte key radix sort, 10 times faster than equivalent quicksort
    radix_sort<TIndex>(tidx, swap, tricount, 2);
    t2 = clock();
    debug_print("  %.4lf seconds to sort %d triangles\n", (t2-t1)/(double)CLOCKS_PER_SEC, tricount);
  }

  //Lock the update mutex, to allow updating the indexlist and prevent access while drawing
  t1 = clock();
  std::lock_guard<std::mutex> guard(loadmutex);
  unsigned int *ptr = indexlist;
  for(int i=tricount-1; i>=0; i--)
  {
    idxcount += 3;
    assert((unsigned int)(ptr-indexlist) < 3 * tricount * sizeof(unsigned int));
    //Copy index bytes
    memcpy(ptr, tidx[i].index, sizeof(GLuint) * 3);
    ptr += 3;
  }

  t2 = clock();
  debug_print("  %.4lf seconds to save %d triangle indices\n", (t2-t1)/(double)CLOCKS_PER_SEC, tricount*3);

  //Force update after sort
  idxcount = 0;
}

//Reloads triangle indices, required after data update and depth sort
void TriSurfaces::render()
{
  clock_t t1,t2;
  if (tricount == 0 || elements == 0) return;
  assert(tidx);

  if (idxcount == elements)
  {
    //Nothing has changed, skip
    debug_print("Redraw skipped, cached %d == %d\n", idxcount, elements);
    return;
  }
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
    idxcount = tricount*3;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxcount * sizeof(GLuint), indexlist, GL_DYNAMIC_DRAW);
    debug_print("  %d byte IBO uploaded %d indices\n", idxcount * sizeof(GLuint), idxcount);
  }
  else
    abort_program("IBO creation failed\n");
  GL_Error_Check;

  t2 = clock();
  debug_print("  %.4lf seconds to upload %d indices (%d tris)\n", (t2-t1)/(double)CLOCKS_PER_SEC, idxcount, tricount);
  t1 = clock();
  //After render(), elements holds unfiltered count, idxcount is filtered
  elements = idxcount;
}

void TriSurfaces::draw()
{
  GL_Error_Check;
  if (elements == 0) return;

  //Re-render the triangles if view has rotated
  if (idxcount != elements) render();

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
    //Reverse order of objects to match index array layout (opaque objects last)
    int tridx = 0;
    for (int index = geom.size()-1; index >= 0; index--)
    {
      if (counts[index] == 0) continue;
      if (geom[index]->opaque)
      {
        setState(index, drawstate.prog[lucTriangleType]); //Set draw state settings for this object
        //fprintf(stderr, "(%d) DRAWING OPAQUE TRIANGLES: %d (%d to %d)\n", index, counts[index]/3, start/3, (start+counts[index])/3);
        glDrawElements(GL_TRIANGLES, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
        start += counts[index];
      }
      else
        tridx = index;
    }

    time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
    if (time > 0.005) debug_print("  %.4lf seconds to draw opaque triangles\n", time);
    t1 = clock();

    //Set draw state settings for first non-opaque object
    //NOTE: per-object textures do not work with transparency!
    setState(tridx, drawstate.prog[lucTriangleType]);

    //Draw remaining elements (transparent, depth sorted)
    //fprintf(stderr, "(*) DRAWING TRANSPARENT TRIANGLES: %d\n", (elements-start)/3);
    if (start < (unsigned int)elements)
    {
      //Render all remaining triangles - elements is the number of indices. 3 indices needed to make a single triangle
      glDrawElements(GL_TRIANGLES, elements-start, GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
    }

    time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
    if (time > 0.005) debug_print("  %.4lf seconds to draw %d transparent triangles\n", time, (elements-start)/3);

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

