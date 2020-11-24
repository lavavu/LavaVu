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
}

Triangles::~Triangles()
{
}

void Triangles::close()
{
  reload = true;
  Geometry::close();
}

unsigned int Triangles::triCount(unsigned int index)
{
  //Get triangle count for element
  unsigned int tris;
  //Indexed
  if (geom[index]->render->indices.size() > 0)
  {
    //Un-structured tri indices
    tris = geom[index]->render->indices.size() / 3;
    if (tris * 3 != geom[index]->render->indices.size()) // || geom[index]->draw->properties["tristrip"])
      //Tri-strip indices
      tris = geom[index]->render->indices.size() - 2;
    debug_print("Surface (indexed) %d\n", index);
  }
  //Grid
  else if (geom[index]->width > 0 && geom[index]->height > 0)
  {
    tris = 2 * (geom[index]->width-1) * (geom[index]->height-1);
    debug_print("Grid Surface %d (%d x %d)\n", index, geom[index]->width, geom[index]->height);
  }
  else
  {
    //Un-structured tri vertices
    tris = geom[index]->count() / 3;
    if (tris < 1) return 0;
    if (tris * 3 != geom[index]->count()) // || geom[index]->draw->properties["tristrip"])
      //Tri-strip vertices
      tris =  geom[index]->count() - 2;
    debug_print("Surface %d \n", index);
  }
  return tris;
}

unsigned int Triangles::triCount()
{
  //Get total triangle count
  total = 0;
  unsigned int drawelements = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    unsigned int tris = triCount(index);
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

  //Always trigger re-render (load indices) in case elements hidden/shown
  counts.clear();
}

void Triangles::loadBuffers()
{
  //Copy data to Vertex Buffer Object
  clock_t t1,t2,tt;
  tt=t2=clock();

  //Update VBO...
  debug_print("Reloading %d elements...\n", elements);

  //Transform grids to triangles...
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    debug_print("Mesh %d/%d has normals? %d == %d\n", index, geom.size(), geom[index]->render->normals.size(), geom[index]->render->vertices.size());
    if (!geom[index]->count()) continue;
    bool grid = (geom[index]->width * geom[index]->height == geom[index]->count());
    bool vnormals = geom[index]->draw->properties["vertexnormals"];
    bool flat = geom[index]->draw->properties["flat"];
    if (grid)
    {
      //Structured mesh grid, 2 triangles per element, 3 indices per tri
      if (vnormals && geom[index]->render->normals.size() == 0)
        calcGridNormals(index);

      //For flat colouring, don't use shared vertices (with index array)
      if (flat)
        calcGridVertices(index);
      //Otherwise calculate indices using existing grid vertices
      else if (geom[index]->render->indices.size() == 0)
        calcGridIndices(index);
    }
    else
    {
      //Calculate normals for irregular mesh
      if (vnormals && geom[index]->render->normals.size() != geom[index]->render->vertices.size())
        calcTriangleNormals(index);
    }
  }

  // VBO - copy normals/colours/positions to buffer object
  unsigned int datasize = sizeof(float) * 8 + sizeof(Colour);   //Vertex(3), normal(3), texCoord(2) and 32-bit colour
  unsigned int vcount = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
    vcount += geom[index]->count();
  unsigned int bsize = vcount * datasize;

  //Create intermediate buffer
  unsigned char* buffer = new unsigned char[bsize];
  unsigned char *ptr = buffer;

  //Buffer data for all vertices
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    t1=tt=clock();

    //Have vertex normals?
    bool vnormals = geom[index]->draw->properties["vertexnormals"];
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
    //Get largest dimension for auto-texcoord calculation
    float dims[3] = {geom[index]->max[0] - geom[index]->min[0],
                     geom[index]->max[1] - geom[index]->min[1],
                     geom[index]->max[2] - geom[index]->min[2]
                    };
    bool hasTexture = geom[index]->hasTexture();
    //Get the indices of the two largest dimensions
    int i0 = 0, i1 = 1;
    if (dims[2] > dims[0] || dims[2] > dims[1])
    {
      i1 = 2;
      if (dims[1] > dims[0])
        i0 = 1;
    }

    float zero[3] = {0,0,0};
    float shift = geom[index]->draw->properties["shift"];
    if (geom[index]->draw->name().length() == 0) shift = 0.0; //Skip shift for built in objects
    //Shift by index, helps prevent z-clashing
    shift *= index * 10e-7 * view->model_size;
    std::array<float,3> shiftvert;
    //TODO: instead of writing blank normal, skip normal and texcoord if no data
    //      will require adjustment to draw() code, attrib pointers etc
    float texCoord[2] = {0.0, 0.0};
    float nullTexCoord[2] = {-1.0, -1.0};
    if (index > 0)
      geom[index]->voffset = geom[index-1]->voffset + geom[index-1]->count();
    for (unsigned int v=0; v < geom[index]->count(); v++)
    {
      //Have colour values but not enough for per-vertex, spread over range (eg: per triangle)
      unsigned int cidx = v / colrange;
      if (!texmap && cidx * colrange == v)
        getColour(colour, cidx);

      float* vert = geom[index]->render->vertices[v];
      if (view->is3d && shift > 0)
      {
        //Shift vertices
        shiftvert = {vert[0] + shift, vert[1] + shift, vert[2] + shift};
        vert = shiftvert.data();
        if (v==0) debug_print("Shifting vertices %s (%d) by %f\n", geom[index]->draw->name().c_str(), index, shift);
      }

      //Write vertex data to vbo
      assert((unsigned int)(ptr-buffer) < bsize);
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
      {
        memcpy(ptr, &geom[index]->render->texCoords[v][0], sizeof(float) * 2);
      }
      else if (texmap && vals)
      {
        texCoord[0] = texmap->scalefast(geom[index]->colourData(v));
        //if (v%100==0 || texCoord[0] <= 0.0) printf("(%f - %f) %f ==> %f\n", texmap->minimum, texmap->maximum, geom[index]->colourData(v), texCoord[0]);
        memcpy(ptr, texCoord, sizeof(float) * 2);
      }
      else if (hasTexture)
      {
        //Auto texcoord : take objects largest dimensions, texture over that range
        texCoord[0] = (vert[i0] - geom[index]->min[i0]) / dims[i0];
        texCoord[1] = (vert[i1] - geom[index]->min[i1]) / dims[i1];
        memcpy(ptr, texCoord, sizeof(float) * 2);
        //printf("Autotexcoord %d %f,%f (i0 %d, i1 %d)\n", v, texCoord[0], texCoord[1], i0, i1);
      }
      else
      {
        //No texcoord: -1,-1 ==> don't use texture!
        memcpy(ptr, nullTexCoord, sizeof(float) * 2);
      }

      ptr += sizeof(float) * 2;
      //Copies colour bytes
      memcpy(ptr, &colour, sizeof(Colour));
      ptr += sizeof(Colour);
    }
    t2 = clock();
    debug_print("  %.4lf seconds to reload %d vertices\n", (t2-t1)/(double)CLOCKS_PER_SEC, geom[index]->count());
  }

  //Initialise vertex array object for OpenGL 3.2+
  if (!vao) glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  //Initialise vertex buffer
  if (!vbo) glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  if (glIsBuffer(vbo))
  {
    glBufferData(GL_ARRAY_BUFFER, bsize, buffer, GL_DYNAMIC_DRAW);
    debug_print("  %d byte VBO created, holds %d vertices\n", bsize, bsize/datasize);
    //ptr = p = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    GL_Error_Check;
  }

  delete[] buffer;
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

  //Element counts to actually plot (exclude filtered/hidden) per geom entry
  counts.clear();
  counts.resize(geom.size());

  //Upload vertex indices
  unsigned int offset = 0;
  unsigned int idxcount = 0;
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    //printf("%d triCount %d drawable %d\n", index, triCount(index), drawable(index));
    unsigned int indices = geom[index]->render->indices.size();
    if (drawable(index))
    {
      if (indices > 0)
      {
        GL_Error_Check;
        assert(offset+indices <= elements);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof(GLuint), indices * sizeof(GLuint), geom[index]->render->indices.ref());
        //printf("%d upload %d indices, offset %d\n", index, indices, offset);
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

  setState(0); //Set global draw state (using first object)
  Shader_Ptr prog = session.shaders[lucTriangleType];

  // Draw using vertex buffer object
  clock_t t0 = clock();
  clock_t t1 = clock();
  double time;
  int stride = 8 * sizeof(float) + sizeof(Colour);   //3+3+2 vertices, normals, texCoord + 32-bit colour
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexvbo);
  if (geom.size() > 0 && elements > 0 && glIsBuffer(vbo) && glIsBuffer(indexvbo))
  {
    unsigned int start = 0;
    //Setup vertex attributes
    GLint aPosition = prog->attribs["aVertexPosition"];
    GLint aNormal = prog->attribs["aVertexNormal"];
    GLint aColour = prog->attribs["aVertexColour"];
    GLint aTexCoord = prog->attribs["aVertexTexCoord"];
    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0); // Vertex x,y,z
    glEnableVertexAttribArray(aNormal);
    glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(3*sizeof(float))); // Normal x,y,z
    glEnableVertexAttribArray(aTexCoord);
    glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(6*sizeof(float))); //Tex coord s,t
    glEnableVertexAttribArray(aColour);
    glVertexAttribPointer(aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(8*sizeof(float)));   // rgba, offset 3 float
    for (unsigned int index = 0; index < geom.size(); index++)
    {
      if (counts[index] > 0)
      {
        setState(index); //Set draw state settings for this object
        if (geom[index]->render->indices.size() > 0)
        {
          //Draw with index buffer
          //glDrawElements(primitive, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
          //printf("  DRAW %d from %d by INDEX (voffset %d)\n", counts[index], start, voffset);
#ifdef __EMSCRIPTEN__ //All GLES2/3 ?
          unsigned int offset = geom[index]->voffset * stride;
          glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offset); // Vertex x,y,z
          glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offset+3*sizeof(float))); // Normal x,y,z
          glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(offset+6*sizeof(float))); //Tex coord s,t
          glVertexAttribPointer(aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (GLvoid*)(offset+8*sizeof(float)));   // rgba, offset 3 float
          glDrawElements(primitive, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)));
#else
          glDrawElementsBaseVertex(primitive, counts[index], GL_UNSIGNED_INT, (GLvoid*)(start*sizeof(GLuint)), geom[index]->voffset);
#endif
        }
        else
        {
          //Draw directly from vertex buffer
          glDrawArrays(primitive, geom[index]->voffset, geom[index]->count());
          //printf("  DRAW %d from %d by VERTEX\n", geom[index]->count(), voffset);
        }
        start += counts[index];
      }

      //Vertex buffer offset (bytes) required because indices per object are zero based
      //vstart += stride * geom[index]->count();
      //vstart += stride * counts[index];
    }

    time = ((clock()-t1)/(double)CLOCKS_PER_SEC);
    if (time > 0.005) debug_print("  %.4lf seconds to draw opaque triangles\n", time);
    t1 = clock();

    glDisableVertexAttribArray(aPosition);
    glDisableVertexAttribArray(aNormal);
    glDisableVertexAttribArray(aTexCoord);
    glDisableVertexAttribArray(aColour);
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

void Triangles::calcTriangleNormals(unsigned int index)
{
  clock_t t1,t2;
  t1 = clock();
  std::vector<Vec3d> normals(geom[index]->count());
  Vec3d normal;

  //Has index data, simply load the triangles
  if (geom[index]->render->indices.size() > 2)
  {
    //TODO: this doesn't appear to be working, normals not smoothed
    debug_print("Calculating normals (indexed) for triangle surface %d size %d\n", index, geom[index]->render->indices.size()/3);
    //Calculate face normals for each triangle and copy to each face vertex
    for (unsigned int i=0; i<geom[index]->render->indices.size()-2 && geom[index]->render->indices.size() > 2; i += 3)
    {
      //Copies for each vertex
      GLuint i1 = geom[index]->render->indices[i];
      GLuint i2 = geom[index]->render->indices[i+1];
      GLuint i3 = geom[index]->render->indices[i+2];

      normal = vectorNormalToPlane(geom[index]->render->vertices[i1],
                                   geom[index]->render->vertices[i2],
                                   geom[index]->render->vertices[i3]);

      normals[i1] += normal;
      normals[i2] += normal;
      normals[i3] += normal;
    }
  }
  else if (geom[index]->count() > 2)
  {
    //Calculate face normals for each triangle and copy to each face vertex
    debug_print("Calculating normals for triangle surface %d size %d\n", index, geom[index]->count());
    for (unsigned int v=0; v<geom[index]->count()-2; v += 3)
    {
      //Copies for each vertex
      normal = vectorNormalToPlane(geom[index]->render->vertices[v],
                                   geom[index]->render->vertices[v+1],
                                   geom[index]->render->vertices[v+2]);
      normals[v] += normal;
      normals[v+1] += normal;
      normals[v+2] += normal;
    }
  }

  t2 = clock();
  debug_print("  %.4lf seconds to calc normals\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Normalise to combine and load normal data
  //geom[index]->render->normals.clear();
  for (unsigned int n=0; n<normals.size(); n++)
    normals[n].normalise();

  geom[index]->_normals = std::make_shared<Coord3DValues>();
  read(geom[index], normals.size(), lucNormalData, &normals[0]);

  //Update the rendering references as some containers have been replaced
  geom[index]->setRenderData();

  t2 = clock();
  debug_print("  %.4lf seconds to normalise (%d) \n", (t2-t1)/(double)CLOCKS_PER_SEC, (int)normals.size());
}

void Triangles::calcGridNormals(unsigned int i)
{
  //Normals: calculate from surface geometry
  std::vector<Vec3d> normals(geom[i]->count());
  clock_t t1,t2;
  t1 = clock();
  debug_print("Calculating normals for grid surface %d... ", i);
  bool hasTexture = geom[i]->hasTexture();
  bool genTexCoords = hasTexture && geom[i]->render->texCoords.size() == 0;
  bool flip = geom[i]->draw->properties["flip"];

  // Calc per-vertex normals for irregular meshes by averaging four surrounding triangle facet normals
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
        unsigned int j0 = flip ? j-1 : j;
        unsigned int j1 = flip ? j : j-1;
        if (k > 0)
        {
          // Look back
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j0 + k],
                                            geom[i]->render->vertices[geom[i]->width * j1 + k],
                                            geom[i]->render->vertices[geom[i]->width * j0 + k-1]);
        }

        if (k < geom[i]->width - 1)
        {
          // Look back in x, forward in y
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j0 + k],
                                            geom[i]->render->vertices[geom[i]->width * j0 + k+1],
                                            geom[i]->render->vertices[geom[i]->width * j1 + k]);
        }
      }

      if (j <  geom[i]->height - 1)
      {
        unsigned int j0 = flip ? j+1 : j;
        unsigned int j1 = flip ? j : j+1;
        if (k > 0)
        {
          // Look forward in x, back in y
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j0 + k],
                                            geom[i]->render->vertices[geom[i]->width * j0 + k-1],
                                            geom[i]->render->vertices[geom[i]->width * j1 + k]);
        }

        if (k < geom[i]->width - 1)
        {
          // Look forward
          normals[n] += vectorNormalToPlane(geom[i]->render->vertices[geom[i]->width * j0 + k],
                                            geom[i]->render->vertices[geom[i]->width * j1 + k],
                                            geom[i]->render->vertices[geom[i]->width * j0 + k+1]);
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

  geom[i]->_normals = std::make_shared<Coord3DValues>();
  read(geom[i], normals.size(), lucNormalData, &normals[0]);
  //Update the rendering references as some containers have been replaced
  geom[i]->setRenderData();
}

void Triangles::calcGridIndices(unsigned int i)
{
  int els = geom[i]->gridElements2d();
  int triverts = els * 6;
  if (triverts < 3) return;
  std::vector<GLuint> indices(triverts);
  clock_t t1,t2;
  t1=clock();
  debug_print("Calculating indices for grid tri surface %d... ", i);
  bool flip = geom[i]->draw->properties["flip"];

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
      indices[o++] = offset0;
      indices[o++] = offset1;
      indices[o++] = offset2;
      //Tri 2
      indices[o++] = offset1;
      indices[o++] = offset3;
      indices[o++] = offset2;
    }
  }

  //geom[i]->_indices = std::make_shared<UIntValues>();
  geom[i]->render->indices.read(indices.size(), &indices[0]);
  //Update the rendering references as some containers have been replaced
  geom[i]->setRenderData();

  t2 = clock();
  debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

void Triangles::calcGridVertices(unsigned int i)
{
  int els = geom[i]->gridElements2d();
  int triverts = els * 6;
  std::vector<Vec3d> vertices(triverts);
  clock_t t1,t2;
  t1=clock();
  debug_print("Calculating vertices for grid tri surface %d... ", i);
  bool flip = geom[i]->draw->properties["flip"];

  unsigned int o = 0;
  for (unsigned int j = 0 ; j < geom[i]->height-1; j++ )
  {
    for (unsigned int k = 0 ; k < geom[i]->width-1; k++ )
    {
      //Add vertices for two triangles per grid element
      unsigned int j0 = flip ? j+1 : j;
      unsigned int j1 = flip ? j : j+1;
      unsigned int offset0 = j0 * geom[i]->width + k;
      unsigned int offset1 = j1 * geom[i]->width + k;
      unsigned int offset2 = j0 * geom[i]->width + k + 1;
      unsigned int offset3 = j1 * geom[i]->width + k + 1;
      assert(o <= vertices.size()-6);
      //Tri 1
      vertices[o++] = geom[i]->render->vertices[offset0];
      vertices[o++] = geom[i]->render->vertices[offset1];
      vertices[o++] = geom[i]->render->vertices[offset2];
      //Tri 2
      vertices[o++] = geom[i]->render->vertices[offset1];
      vertices[o++] = geom[i]->render->vertices[offset3];
      vertices[o++] = geom[i]->render->vertices[offset2];
    }
  }

  geom[i]->_indices = std::make_shared<UIntValues>(); //Clear any indices
  geom[i]->_vertices = std::make_shared<Coord3DValues>();
  read(geom[i], vertices.size(), lucVertexData, &vertices[0]);
  //Update the rendering references as some containers have been replaced
  geom[i]->setRenderData();

  t2 = clock();
  debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}



