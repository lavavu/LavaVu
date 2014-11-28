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

Shader* TriSurfaces::prog = NULL;

TriSurfaces::TriSurfaces(bool hidden) : Geometry(hidden)
{
   type = lucTriangleType;
   wireframe = false;
   cullface = false;
   vbo = 0;
   indexvbo = 0;
   tidx = NULL;
}

TriSurfaces::~TriSurfaces()
{
   if (vbo) glDeleteBuffers(1, &vbo);
   if (indexvbo) glDeleteBuffers(1, &indexvbo);
   vbo = 0;
   indexvbo = 0;
   delete[] tidx;
}

void TriSurfaces::update()
{
   Geometry::update();

   // Update triangles...
   clock_t t1,t2,tt;
   tt=clock();
   static unsigned int last_total = 0;
   if (geom.size() == 0) return;

   total = 0;  //Get triangle count
   std::vector<bool> hiddenflags(geom.size());
   estimate = 0;
   for (unsigned int t = 0; t < geom.size(); t++) 
   {
      //Check if grid or unstructured mesh
      int tris = 0;
      if (geom[t]->width > 0 && geom[t]->height > 0)
      {
         tris = 2 * (geom[t]->width-1) * (geom[t]->height-1);
         estimate += geom[t]->width * geom[t]->height;   //Vertex count
      }
      else
      {
         tris = geom[t]->count / 3;
         estimate += tris*1.1;   //Vertex count estimate
      }
      total += tris;
      hiddenflags[t] = !drawable(t); //Save flags
      debug_print("Isosurface %d, triangles %d hidden? %s\n", t, tris, (hiddenflags[t] ? "yes" : "no"));
   }
   if (total == 0) return;

   //Ensure vbo recreated if total changed
   if (elements < 0 || total != last_total)
   {
      if (!loadVertices())
      {
         debug_print("WARNING: buffer estimate too small, retrying with max size");
         estimate = total*3;
         loadVertices(); //On buffer size fail, load with max buffer size
      }
   }
   else
   {
      //Set flag in index array based on hidden/shown
      t1=clock();
      for (unsigned int t = 0; t < total; t++) 
         tidx[t].hidden = hiddenflags[tidx[t].geomid];
      t2 = clock(); debug_print("  %.4lf seconds to update hidden flags\n", (t2-t1)/(double)CLOCKS_PER_SEC);
   }
   last_total = total;

   t2 = clock(); debug_print("  %.4lf seconds to update triangles into sort array\n", (t2-tt)/(double)CLOCKS_PER_SEC);

   //Check total
   if (total == 0) 
      return; 

   //Initial depth sort & render
   render();
}

bool TriSurfaces::loadVertices(unsigned int filter, std::ostream* json)
{
   //TODO: Possibly separate GL calls from sorting algorithm in here,
   //will require storing verts/normals arrays at least temporarily
   //May be worth caching this anyway for faster loading of new objects from db

   // Update triangles...
   clock_t t1,t2,tt;
   if (!json)
   {
      if (vbo)
         glDeleteBuffers(1, &vbo);
      if (indexvbo)
         glDeleteBuffers(1, &indexvbo);
      vbo = indexvbo = 0;
   }
   if (geom.size() == 0) return true;

   debug_print("Reloading %d triangles...\n", total);

   //Calculates normals, deletes duplicate verts, adds triangles to sorting array

   // VBO - copy normals/colours/positions to buffer object for quick display 
   unsigned char *p, *ptr;
   ptr = p = NULL;
   int datasize = sizeof(float) * 6 + sizeof(Colour);   //Vertex(3), normal(3) and 32-bit colour
   //3=max size, buffer large enough for all triangles with no shared vertices, 33% has been large enough in all testing
   int bsize = estimate * datasize; //3 / bufferdiv * total * datasize;
   if (!json)
   {
      //Create sorting array
      if (tidx) delete[] tidx;
      tidx = new TIndex[total];
      if (tidx == NULL) abort_program("Memory allocation error (failed to allocate %d bytes)", sizeof(TIndex) * total);

      glGenBuffers(1, &vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      //Initialise triangle buffer
      if (glIsBuffer(vbo))
      {
         glBufferData(GL_ARRAY_BUFFER, bsize, NULL, GL_STATIC_DRAW);
         debug_print("  %d byte VBO created, holds %d of %d max tri vertices\n", bsize, bsize/datasize, 3*total);
      }
      else 
         debug_print("  VBO creation failed\n");
      GL_Error_Check;

      // Index buffer object for quick display 
      glGenBuffers(1, &indexvbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
      GL_Error_Check;
      if (glIsBuffer(indexvbo))
      {
         glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * total * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
         debug_print("  %d byte IBO created for %d indices\n", 3 * total * sizeof(GLuint), total * 3);
      }
      else 
         debug_print("  IBO creation failed\n");
      GL_Error_Check;

      if (glIsBuffer(vbo))
      {
         ptr = p = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
         GL_Error_Check;
         //if (!p) abort_program("glMapBuffer failed");
      }
      //else abort_program("vbo fail");
   }

   //Index data for all vertices
   GLuint unique = 0;
   tricount = 0;
   bool first = true;
   for (unsigned int index = 0; index < geom.size(); index++) 
   {
      if (filter > 0 && geom[index]->draw->id != filter) continue; //Skip filtered
      bool grid = (geom[index]->width > 0 && geom[index]->height > 0);

      //Save initial offset into VBO
      GLuint voffset = unique;
      //Add vertices to a vector with indices
      //Sort vertices vector with std::sort & custom compare sort( vec.begin(), vec.end() );
      //Iterate, for duplicates replace indices with index of first
      //Remove duplicate vertices Triangles stored as list of indices
      bool avgColour = (geom[index]->colourValue.size() > 0);
      t1=tt=clock();

      //Add vertices to vector
      std::vector<Vertex> verts(geom[index]->count);
      std::vector<Vec3d> normals(geom[index]->count);
      std::vector<GLuint> indices;
      for (int j=0; j < geom[index]->count; j++)
      {
         verts[j].id = verts[j].ref = j;
         verts[j].vert = geom[index]->vertices[j];
      }
      t2 = clock(); debug_print("  %.4lf seconds to add to sort vector\n", (t2-t1)/(double)CLOCKS_PER_SEC);

      int triverts = 0;
      if (grid)
      {
         //Structured mesh grid, 2 triangles per element, 3 indices per tri 
         int elements = (geom[index]->width-1) * (geom[index]->height-1);
         triverts = elements * 6;
         indices.resize(triverts);
         calcGridNormalsAndIndices(index, normals, indices);
      }
      else
      {
         //Unstructured mesh, 1 index per vertex 
         triverts = geom[index]->count;
         indices.resize(triverts);
         calcTriangleNormals(index, verts, normals, json != NULL);
      }

      //Now have list of vertices sorted by vertex pos with summed normals and references of duplicates replaced
      t1 = clock();

      //Calibrate colour maps on range for this surface
      geom[index]->colourCalibrate();

      int i = 0;
      Colour colour;
      for (unsigned int v=0; v<verts.size(); v++)
      {
         if (verts[v].id == verts[v].ref)
         {
            //Reference id == self, not a duplicate
            //Normalise final vector
            normals[verts[v].id].normalise();
            //Average final colour
            if (avgColour && verts[v].vcount > 1)
               geom[index]->colourValue.value[verts[v].id] /= verts[v].vcount;

            geom[index]->getColour(colour, verts[v].id);

            //Write vertex data to vbo
            if (ptr)
            {
               if ((int)(ptr-p) >= bsize) return false;
               //Copies vertex bytes
               memcpy(ptr, verts[v].vert, sizeof(float) * 3);
               ptr += sizeof(float) * 3;
               //Copies normal bytes
               memcpy(ptr, normals[verts[v].id].ref(), sizeof(float) * 3);
               ptr += sizeof(float) * 3;
               //Copies colour bytes
               memcpy(ptr, &colour, sizeof(Colour));
               ptr += sizeof(Colour);
            }

            //Save an index lookup entry (Grid indices loaded in previous step)
            if (!grid) indices[verts[v].id] = i++;
            unique++;
         }
         else
         {
            //Duplicate vertex, replace with a copy
            indices[verts[v].id] = indices[verts[v].ref];
            if (!ptr) //When using index buffer don't need this as data already saved, is necessary to output to json though
            {
               normals[verts[v].id] = normals[verts[v].ref];
               if (avgColour) geom[index]->colourValue.value[verts[v].id] = geom[index]->colourValue.value[verts[v].ref];

            }
         }
      }
      t2 = clock(); debug_print("  %.4lf seconds to normalise (and re-buffer)\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

      //Replace normals in float data block
      if (!ptr && !json) //When using index buffer don't need this as data already saved
      {
         geom[index]->normals.clear();
         geom[index]->normals.read(normals.size(), normals[0].ref());   //Read data
         //Flag generated data
         geom[index]->normals.generated = true;
      }

      //Save json data buffers for export
      if (json && drawable(index))
      {
         //New contiguous buffer of unique vertices only
         std::ostream& os = *json;
         std::vector<Vec3d> vertices;
         std::vector<Vec3d> norms;
         std::vector<float> values;
         for (unsigned int v=0; v<verts.size(); v++)
         {
            if (verts[v].id == verts[v].ref)
            {
               vertices.push_back(Vec3d(verts[v].vert));
               norms.push_back(Vec3d(normals[verts[v].id]));
               if (geom[index]->colourValue.size() == geom[index]->count)
                 values.push_back(geom[index]->colourValue.value[verts[v].id]);
            }
         }
         std::string indices_str = base64_encode(reinterpret_cast<const unsigned char*>(&indices[0]), indices.size() * sizeof(GLuint));
         std::string vertices_str = base64_encode(reinterpret_cast<const unsigned char*>(vertices[0].ref()), vertices.size() * sizeof(float) * 3);
         std::string normals_str = base64_encode(reinterpret_cast<const unsigned char*>(norms[0].ref()), norms.size() * sizeof(float) * 3);

         std::cerr << "Collected " << geom[index]->count << " vertices/values (" << index << ")" << std::endl;
         //Only supports dump of vertex, normal and colourValue at present
         if (!first) os << "," << std::endl;
         first = false;
         os << "        {" << std::endl;
         os << "          \"indices\" : \n          {" << std::endl;
         os << "            \"size\" : 1," << std::endl;
         os << "            \"count\" : " << indices.size() << "," << std::endl;
         os << "            \"data\" : \"" << indices_str << "\"" << std::endl;
         os << "          }," << std::endl;
         os << "          \"vertices\" : \n          {" << std::endl;
         os << "            \"size\" : 3," << std::endl;
         os << "            \"count\" : " << vertices.size() << "," << std::endl;
         os << "            \"data\" : \"" << vertices_str << "\"" << std::endl;
         os << "          }," << std::endl;
         os << "          \"normals\" : \n          {" << std::endl;
         os << "            \"size\" : 3," << std::endl;
         os << "            \"count\" : " << norms.size() << "," << std::endl;
         os << "            \"data\" : \"" << normals_str << "\"" << std::endl;
         
         if (values.size())
         {
            std::string values_str = base64_encode(reinterpret_cast<const unsigned char*>(&values[0]), values.size() * sizeof(float));
            os << "          }," << std::endl;
            os << "          \"values\" : \n          {" << std::endl;
            os << "            \"size\" : 1," << std::endl;
            os << "            \"count\" : " << values.size() << "," << std::endl;
            os << "            \"minimum\" : " << geom[index]->colourValue.minimum << "," << std::endl;
            os << "            \"maximum\" : " << geom[index]->colourValue.maximum << "," << std::endl;
            os << "            \"data\" : \"" << values_str << "\"" << std::endl;
         }
         os << "          }" << std::endl;
         os << "        }";
      }

      if (ptr)
      {
         t1 = clock();
         int i=0;
         //Loop from previous tricount to current tricount
         for (unsigned int t = (tricount - triverts / 3); t < tricount; t++)
         {
            //voffset is offset of the last vertex added to the vbo from the previous object
            tidx[t].index[0] = indices[i++] + voffset;
            tidx[t].index[1] = indices[i++] + voffset;
            tidx[t].index[2] = indices[i++] + voffset;
         }
         t2 = clock(); debug_print("  %.4lf seconds to re-index\n", (t2-t1)/(double)CLOCKS_PER_SEC);
      }

      //Empty
      verts.clear();
      normals.clear();

      t2 = clock(); debug_print("  %.4lf seconds to reload & clean up\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();
      debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
   }

   if (ptr)
   {
      glUnmapBuffer(GL_ARRAY_BUFFER);
      GL_Error_Check;
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }
   debug_print("  *** There were %d unique vertices out of %d total. Buffer allocated for %d\n", unique, total*3, bsize/datasize);
   return true;
}

void TriSurfaces::setTriangle(int index, bool hidden, float* v1, float* v2, float* v3)
{
   tidx[tricount].distance = 0;
   tidx[tricount].geomid = index;
   tidx[tricount].index[0] = 0;
   tidx[tricount].index[1] = 0;
   tidx[tricount].index[2] = 0;
   tidx[tricount].hidden = hidden;

        //TODO: hack for now to get wireframe working, should detect/switch opacity
        if (geom[index]->draw->wireframe) geom[index]->opaque = true;

   //All opaque triangles at start
   if (geom[index]->opaque) 
      tidx[tricount].distance = 65535; //0
   else
   {
      //Triangle centroid for depth sorting
      float centroid[3] = {(v1[0]+v2[0]+v3[0])/3, (v1[1]+v2[1]+v3[1])/3, view->is3d ? (v1[2]+v2[2]+v3[2])/3 : 0.0};
      assert(centroid[2] >= Geometry::min[2] && centroid[2] <= Geometry::max[2]);
      memcpy(tidx[tricount].centroid, centroid, sizeof(float)*3);
   }
   tricount++;
}

void TriSurfaces::calcTriangleNormals(int index, std::vector<Vertex> &verts, std::vector<Vec3d> &normals, bool skipList)
{
   clock_t t1,t2;
   t1 = clock();
   debug_print("Calculating normals for triangle surface %d\n", index);
   bool hiddenflag = !drawable(index);
   bool avgColour = (geom[index]->colourValue.size() > 0);
   //Calculate face normals for each triangle and copy to each face vertex
   for (unsigned int v=0; v<verts.size(); v += 3)
   {
      //Copies for each vertex
      normals[v] = Vec3d(vectorNormalToPlane(verts[v].vert, verts[v+1].vert, verts[v+2].vert));
      normals[v+1] = Vec3d(normals[v]);
      normals[v+2] = Vec3d(normals[v]);

      if (skipList) continue; //Skip triangle index list for json export

      //Add to triangle index list for sorting
      setTriangle(index, hiddenflag, verts[v].vert, verts[v+1].vert, verts[v+2].vert);
   }
   t2 = clock(); debug_print("  %.4lf seconds to calc facet normals\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

   //Sort by vertex
   std::sort(verts.begin(), verts.end());
   t2 = clock(); debug_print("  %.4lf seconds to sort %d verts\n", (t2-t1)/(double)CLOCKS_PER_SEC, verts.size()); t1 = clock();

   //Now have list of vertices sorted by vertex pos
   //Search for duplicates and replace references to normals
   int match = 0;
   for(unsigned int v=1; v<verts.size(); v++)
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
         float angle = RAD2DEG * normals[verts[v].id].angle(normals[verts[match].id]);
         //debug_print("angle %f ", angle);
         //Don't include vertices in the sum if angle between normals too sharp
         if (angle < 90)
         {
            //Found a duplicate, replace reference idx (original retained in "id")
            verts[v].ref = verts[match].ref;

            //Add this normal to matched normal
            normals[verts[match].id] += normals[verts[v].id];

            //Colour value, add to matched
            if (avgColour)
               geom[index]->colourValue.value[verts[match].id] += geom[index]->colourValue.value[verts[v].id];

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
   t2 = clock(); debug_print("  %.4lf seconds to replace duplicates\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();
}

void TriSurfaces::calcGridNormalsAndIndices(int i, std::vector<Vec3d> &normals, std::vector<GLuint> &indices)
{
   //Normals: calculate from surface geometry
   clock_t t1,t2;
   t1=clock();
   debug_print("Calculating normals/indices for grid tri surface %d... ", i);

   // Calc pre-vertex normals for irregular meshes by averaging four surrounding triangle facet normals
   bool hiddenflag = !drawable(i);
   int n = 0, o = 0;
   for (int j = 0 ; j < geom[i]->height; j++ )
   {
      for (int k = 0 ; k < geom[i]->width; k++ )
      {
         // Get sum of normal vectors
         if (j > 0)
         {
            if (k > 0)
            {
               // Look back
               normals[n] += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                             geom[i]->vertices[geom[i]->width * (j-1) + k], geom[i]->vertices[geom[i]->width * j + k-1]);
            }

            if (k < geom[i]->width - 1)
            {
               // Look back in x, forward in y
               normals[n] += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                             geom[i]->vertices[geom[i]->width * j + k+1], geom[i]->vertices[geom[i]->width * (j-1) + k]);
            }
         }

         if (j <  geom[i]->height - 1)
         {
            if (k > 0)
            {
               // Look forward in x, back in y
               normals[n] += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                             geom[i]->vertices[geom[i]->width * j + k-1], geom[i]->vertices[geom[i]->width * (j+1) + k]);
            }

            if (k < geom[i]->width - 1)
            {
               // Look forward
               normals[n] += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                             geom[i]->vertices[geom[i]->width * (j+1) + k], geom[i]->vertices[geom[i]->width * j + k+1]);
            }
         }

         //Normalise to average (done later now)
         //normals[n].normalise();
         //Copy directly into normal block
         //memcpy(geom[i]->normals[j * geom[i]->width + k], normal.ref(), sizeof(float) * 3);

         //Add indices for two triangles per grid element
         if (j < geom[i]->height-1 && k < geom[i]->width-1)
         {
            int offset0 = j * geom[i]->width + k;
            int offset1 = (j+1) * geom[i]->width + k;
            int offset2 = j * geom[i]->width + k + 1;
            int offset3 = (j+1) * geom[i]->width + k + 1;

            //Tri 1
            setTriangle(i, hiddenflag, geom[i]->vertices[offset0], geom[i]->vertices[offset1], geom[i]->vertices[offset2]);
            indices[o++] = offset0;
            indices[o++] = offset1;
            indices[o++] = offset2;
            //Tri 2
            setTriangle(i, hiddenflag, geom[i]->vertices[offset1], geom[i]->vertices[offset3], geom[i]->vertices[offset2]);
            indices[o++] = offset1;
            indices[o++] = offset3;
            indices[o++] = offset2;
         }
         n++;
      }
   }
   t2 = clock(); debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();
}

//Depth sort the triangles before drawing, called whenever the viewing angle has changed
void TriSurfaces::depthSort()
{
   clock_t t1,t2;
   t1 = clock();

   //Sort is much faster without allocate, so keep buffer until size changes
   static long last_size = 0;
   static TIndex* swap = NULL;
   int size = total*sizeof(TIndex);
   if (size != last_size)
   {
      if (swap) delete[] swap;
      swap = new TIndex[total];
      if (swap == NULL) abort_program("Memory allocation error (failed to allocate %d bytes)", size);
   }
   last_size = size;

   //Calculate min/max distances from view plane
   float maxdist, mindist; 
   float modelView[16];
   glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
   Geometry::getMinMaxDistance(modelView, &mindist, &maxdist);
   //printMatrix(modelView);
   //printf("MINDIST %f MAXDIST %f\n", mindist, maxdist);

   //Update eye distances, clamping int distance to integer between 1 and 65534
   float multiplier = 65534.0 / (maxdist - mindist);
   if (total == 0) return;
   for (unsigned int i = 0; i < total; i++)
   {
      //Distance from viewing plane is -eyeZ
      //Max dist 65535 reserved for opaque triangles
      if (tidx[i].distance < 65535) 
      //if (tidx[i].distance > 0) 
      {
         tidx[i].fdistance = eyeDistance(modelView, tidx[i].centroid);
         tidx[i].distance = (int)(multiplier * (tidx[i].fdistance - mindist));
         assert(tidx[i].distance >= 0 && tidx[i].distance <= 65534);
             //Shift by id hack
             tidx[i].distance += tidx[i].geomid;
         //Reverse as radix sort is ascending and we want to draw by distance descending
         //tidx[i].distance = 65535 - (int)(multiplier * (tidx[i].fdistance - mindist));
         //assert(tidx[i].distance >= 1 && tidx[i].distance <= 65535);
      }
   }
   t2 = clock(); debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

   //Depth sort using 2-byte key radix sort, 10 times faster than equivalent quicksort
   radix_sort<TIndex>(tidx, swap, total, 2);
   t2 = clock(); debug_print("  %.4lf seconds to sort\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();
}

//Reloads triangles into display list, required after data update and depth sort
void TriSurfaces::render()
{
   clock_t t1,t2;
   if (total == 0) return;


   //First, depth sort the triangles
   if (view->is3d)
   {
      debug_print("Depth sorting %d triangles...\n", total);
      depthSort();
   }

   t1 = clock();

   //Re-map vertex indices in sorted order
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
   if (glIsBuffer(indexvbo))
   {
      unsigned char *p, *ptr;
      t1 = clock();
      ptr = p = (unsigned char*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
      GL_Error_Check;
      if (!p) abort_program("glMapBuffer failed");
      //Reverse order farthest to nearest 
      elements = 0;
      for(int i=total-1; i>=0; i--) 
      //for(int i=0; i<total; i++) 
      {
         if (tidx[i].hidden) continue;
         elements += 3;
         assert((int)(ptr-p) < 3 * total * sizeof(GLuint));
         //Copies index bytes
         memcpy(ptr, tidx[i].index, sizeof(GLuint) * 3);
         ptr += sizeof(GLuint) * 3;
      }
      glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
      GL_Error_Check;
      t2 = clock(); debug_print("  %.4lf seconds to upload %d indices\n", (t2-t1)/(double)CLOCKS_PER_SEC, elements); t1 = clock();
   }
}

void TriSurfaces::draw()
{
   //Draw, calls display list when available
   Geometry::draw();

   //Re-render the triangles if view has rotated
   if (view->sort) render();

   // Draw using vertex buffer object
   clock_t t1 = clock();
   int stride = 6 * sizeof(float) + sizeof(Colour);   //3+3 vertices, normals + 32-bit colour
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
   if (elements > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
   {
      glVertexPointer(3, GL_FLOAT, stride, (GLvoid*)0); // Load vertex x,y,z only
      glNormalPointer(GL_FLOAT, stride, (GLvoid*)(3*sizeof(float))); // Load normal x,y,z, offset 3 float
      glColorPointer(4, GL_UNSIGNED_BYTE, stride, (GLvoid*)(6*sizeof(float)));   // Load rgba, offset 6 float
      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_NORMAL_ARRAY);
      glEnableClientState(GL_COLOR_ARRAY);

      unsigned int start = 0;
      //Reverse order of objects to match index array layout
      for (int index = geom.size()-1; index >= 0; index--) 
      //for (int index = 0; index < geom.size(); index++) 
      {
         setState(index, prog); //Set draw state settings for this object
         if (geom[index]->opaque)
         {
            //fprintf(stderr, "(%d) DRAWING OPAQUE TRIANGLES: %d (%d to %d)\n", index, geom[index]->count/3, start/3, (start+geom[index]->count)/3);
            glDrawRangeElements(GL_TRIANGLES, 0, elements, geom[index]->count, GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
            start += geom[index]->count;
         }
      }
      //Draw remaining elements (transparent, depth sorted)
      if (start > 0 && start < elements)
      {
         //fprintf(stderr, "(*) DRAWING TRANSPARENT TRIANGLES: %d\n", elements-start);
         glDrawRangeElements(GL_TRIANGLES, 0, elements, elements-start, GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
      }
      else
      {
         //Render all triangles - elements is the number of indices. 3 indices needed to make a single triangle
         //(If there is no separate opaque/transparent geometry)
         glDrawElements(GL_TRIANGLES, elements, GL_UNSIGNED_INT, (GLvoid*)0);
      }

      glDisableClientState(GL_VERTEX_ARRAY);
      glDisableClientState(GL_NORMAL_ARRAY);
      glDisableClientState(GL_COLOR_ARRAY);
   }
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   //Restore state
   glEnable(GL_LIGHTING);
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   glDisable(GL_CULL_FACE);

   double time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
   if (time > 0.05)
     debug_print("  %.4lf seconds to draw triangles\n", time);
   GL_Error_Check;
}

void TriSurfaces::jsonWrite(unsigned int id, std::ostream* osp)
{
   loadVertices(id, osp);
}
