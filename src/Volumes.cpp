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
#include "IsoSurface.h"

Volumes::Volumes(Session& session) : Imposter(session)
{
  type = lucVolumeType;
}

Volumes::~Volumes()
{
  close();
}

void Volumes::close()
{
  Imposter::close();

  slices.clear();

  //Iterate geom and delete textures
  //"gpucache" property allows switching this behaviour off for faster switching
  //(usually set globally but checked per object for volumes)
  //requries enough GPU ram to store all volumes
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (!geom[i]->draw->properties["gpucache"] && geom[i]->texture)
    {
      geom[i]->texture->clear();
      reload = true;
    }
  }
}

void Volumes::setup(View* vp, float* min, float* max)
{
  //Ensure data is loaded for this step
  merge(session.now, session.now);

  //Default volume bounding cube - load corner vertices if not provided
  DrawingObject* draw = NULL;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if ((geom[i]->depth > 1 || draw != geom[i]->draw) && geom[i]->count() < 2)
    {
      float volmin[3], volmax[3];
      Properties::toArray<float>(geom[i]->draw->properties["volmin"], volmin, 3);
      Properties::toArray<float>(geom[i]->draw->properties["volmax"], volmax, 3);
      float dims[3];
      Properties::toArray<float>(geom[i]->draw->properties["dims"], dims, 3);
      read(geom[i], 1, lucVertexData, volmin, dims[0], dims[1], dims[2]);
      read(geom[i], 1, lucVertexData, volmax, dims[0], dims[1], dims[2]);
    }
    draw = geom[i]->draw;
  }

  Geometry::setup(vp, min, max);
}

void Volumes::draw()
{
  //clock_t t1,t2,tt;
  //t1 = tt = clock();

  //Lock the update mutex and copy the sorted vector
  //Allows further sorting to continue in background while rendering
  std::vector<Distance> vol_sorted;
  {
    if (vol_sort.size() == 0)
      sort();
    std::lock_guard<std::mutex> guard(loadmutex);
    vol_sorted = vol_sort;
  }

  //Each object can only have one volume,
  //but each volume can consist of separate slices or a single cube

  //Render in reverse sorted order
  for (int i=vol_sorted.size()-1; i>=0; i--)
  {
    unsigned int id = vol_sorted[i].id;
    if (!drawable(id)) continue;

    setState(id); //Set draw state settings for this object
    render(id);

    GL_Error_Check;
  }

  glUseProgram(0);
  glBindTexture(GL_TEXTURE_3D, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  //t2 = clock(); debug_print("  Draw %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void Volumes::sort()
{
  clock_t t1,t2,tt;
  t1 = tt = clock();
  // depth sort volumes..

  tt=clock();
  if (slices.size() == 0 || geom.size() == 0) return;

  //Lock the update mutex
  std::lock_guard<std::mutex> guard(loadmutex);

  //Get element/quad count
  debug_print("Sorting %lu volume entries...\n", geom.size());
  vol_sort.clear();

  //Calculate min/max distances from viewer
  if (reload) updateBoundingBox();
  float distanceRange[2], modelView[16];
  view->getMinMaxDistance(min, max, distanceRange, modelView);

  unsigned int index = 0;
  for (unsigned int i=0; i<slices.size(); i++)
  {
    //Get corners of cube
    if (geom[index]->count() == 0) continue;
    float* posmin = geom[index]->render->vertices[0];
    float* posmax = geom[index]->render->vertices[1];



    float pos[3] = {posmin[0] + (posmax[0] - posmin[0]) * 0.5f,
                    posmin[1] + (posmax[1] - posmin[1]) * 0.5f,
                    posmin[2] + (posmax[2] - posmin[2]) * 0.5f
                   };

    //Object rotation/translation
    if (geom[index]->draw->properties.has("translate"))
    {
      float trans[3];
      Properties::toArray<float>(geom[index]->draw->properties["translate"], trans, 3);
      pos[0] += trans[0];
      pos[1] += trans[1];
      pos[2] += trans[2];
    }

    //Calculate distance from viewing plane
    geom[index]->distance = view->eyeDistance(modelView, pos);
    if (geom[index]->distance < distanceRange[0]) distanceRange[0] = geom[index]->distance;
    if (geom[index]->distance > distanceRange[1]) distanceRange[1] = geom[index]->distance;
    //printf("%d)  %f %f %f distance = %f (min %f, max %f)\n", i, pos[0], pos[1], pos[2], geom[index]->distance, distanceRange[0], distanceRange[1]);
    vol_sort.push_back(Distance(index, geom[index]->distance));

    index += slices[geom[i]->draw];
  }
  t2 = clock();
  debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Sort
  std::sort(vol_sort.begin(), vol_sort.end());
  t2 = clock();
  debug_print("  %.4lf seconds to sort\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

void Volumes::countSlices()
{
  //Count and group 2D slices
  slices.clear();
  DrawingObject* draw = geom[0]->draw;
  unsigned int count = 0;
  for (unsigned int i=0; i<=geom.size(); i++)
  {
    if (i==geom.size() || draw != geom[i]->draw)
    {
      slices[draw] = count;
      if (count == 1)
        debug_print("Reloading: cube in object %s\n", draw->name().c_str());
      else
        debug_print("Reloading: %d slices in object %s\n", count, draw->name().c_str());
      count = 0;
      if (i<geom.size()) draw = geom[i]->draw;
    }
    count++;
  }
}

void Volumes::update()
{
  //Use triangle renderer for two triangles to display volume shader output
  Imposter::update();

  if (geom.size() == 0)
    return; //No volume to render

  clock_t t2,tt;
  tt = clock();

  int maxtex;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxtex);
  debug_print("Volume slices: %d, Max 3D texture size %d\n", geom.size(), maxtex);

  //Padding!
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  //TODO: filtering
  //Read all colourvalues, apply filter to each and store in filtered block before loading into texture

  //Count and group 2D slices
  countSlices();

  for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw])
  {
    if (!drawable(i) || !geom[i]->width) continue;

    //Required to cache colour value info
    geom[i]->colourCalibrate();

    //Single volume cube
    if (geom[i]->depth > 1)
    {
      DrawingObject* current = geom[i]->draw;
      bool texcompress = current->properties["compresstextures"];
      //if (geom[i]->texture->empty())
      if (geom[i]->texture->fn.empty() || geom[i]->texture->empty())
      {
        //Determine type of data then load the texture
        unsigned int bpv = 4;
        if (geom[i]->render->colours.size() > 0)
        {
          int type = texcompress ? VOLUME_RGBA_COMPRESSED : VOLUME_RGBA;
          geom[i]->texture->load3D(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->render->colours.ref(), type);
        }
        else if (geom[i]->render->luminance.size() > 0)
        {
          bpv = 1;
          int type = texcompress ? VOLUME_BYTE_COMPRESSED : VOLUME_BYTE;
          assert(geom[i]->render->luminance.size() == geom[i]->width * geom[i]->height * geom[i]->depth);
          geom[i]->texture->load3D(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->render->luminance.ref(), type);
        }
        else if (geom[i]->colourData())
        {
          assert(geom[i]->colourData()->size() == geom[i]->width * geom[i]->height * geom[i]->depth);
          geom[i]->texture->load3D(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->colourData()->ref(), VOLUME_FLOAT);
        }
        debug_print("volume %d width %d height %d depth %d (bpv %d)\n", i, geom[i]->width, geom[i]->height, geom[i]->depth, bpv);
      }

      ColourMap* cmap = geom[i]->draw->colourMap;
      if (cmap) cmap->loadTexture();
      continue;
    }

    //Collection of 2D slices
    DrawingObject* current = geom[i]->draw;
    bool texcompress = current->properties["compresstextures"];
    //if (geom[i]->texture->empty())
    if (geom[i]->texture->fn.empty() || geom[i]->texture->empty())
    {
      if (!geom[i]->height)
      {
        //No height? Calculate from values data
        if (geom[i]->colourData())
          //(assumes float luminance data (4 bpv))
          geom[i]->height = geom[i]->colourData()->size() / geom[i]->width;
        else if (geom[i]->render->colours.size() > 0)
          geom[i]->height = geom[i]->render->colours.size() / geom[i]->width;
        else if (geom[i]->render->luminance.size() > 0)
          geom[i]->height = geom[i]->render->luminance.size() / geom[i]->width;
        else if (geom[i]->render->rgb.size() > 0)
          geom[i]->height = geom[i]->render->rgb.size() / geom[i]->width / 3;
      }

      //Texture crop?
      unsigned int texsize[3], texoffset[3];
      Properties::toArray<unsigned int>(current->properties["texturesize"], texsize, 3);
      Properties::toArray<unsigned int>(current->properties["textureoffset"], texoffset, 3);
      unsigned int dims[3] = {geom[i]->width, geom[i]->height, slices[current]};
      bool crop = false;
      for (int d=0; d<3; d++)
      {
        if (texsize[d] > 0 && texsize[d] < dims[d]) 
        {
          dims[d] = texsize[d];
          crop = true;
        }
        if (texoffset[d] > 0) crop = true;
        //Check within tex limits
        assert(dims[d] <= (unsigned)maxtex);
      }
      if (crop)
        debug_print("Cropping volume %d x %d ==> %d x %d @ %d,%d\n", geom[i]->width, geom[i]->height, dims[0], dims[1], (int)texoffset[0], (int)texoffset[1]);

      //Init/allocate/bind texture
      unsigned int bpv = 4;
      int type = 0;
      GL_Error_Check;
      if (geom[i]->render->colours.size() > 0)
      {
        //RGBA colours
        type = texcompress ? VOLUME_RGBA_COMPRESSED : VOLUME_RGBA;
        geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->render->colours.ref(), geom[i]->width, geom[i]->height, 4, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->render->colours.ref());
        }
      }
      else if (geom[i]->render->rgb.size() > 0)
      {
        //Byte RGB
        bpv = 3;
        type = texcompress ? VOLUME_RGB_COMPRESSED : VOLUME_RGB;
        assert(geom[i]->render->rgb.size() == 3*geom[i]->width * geom[i]->height);
        geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->render->rgb.ref(), geom[i]->width, geom[i]->height, 3, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->render->rgb.ref());
        }
      }
      else if (geom[i]->render->luminance.size() > 0)
      {
        //Byte luminance
        bpv = 1;
        type = texcompress ? VOLUME_BYTE_COMPRESSED : VOLUME_BYTE;
        assert(geom[i]->render->luminance.size() == geom[i]->width * geom[i]->height);
        geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->render->luminance.ref(), geom[i]->width, geom[i]->height, 1, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->render->luminance.ref());
        }
      }
      else if (geom[i]->colourData())
      {
        //Float data, interpret as either luminance or bytes packed into float container (legacy, still needed?)
        //TODO: Support RGB(A) float GL_RGBA16F (bpv=8) or GL_RGBA32F? (bpv=16)
        bpv = (4 * geom[i]->colourData()->size()) / (float)(geom[i]->width * geom[i]->height);
        if (bpv == 1)
        {
          type = texcompress ? VOLUME_BYTE_COMPRESSED : VOLUME_BYTE;
          geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        }
        else if (bpv == 4)
        {
          geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, VOLUME_FLOAT);
        }
        else
          abort_program("Invalid volume bpv %d", bpv);

        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->colourData()->ref(), geom[i]->width, geom[i]->height, bpv, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->colourData()->ref());
        }

      }

      debug_print("current %s width %d height %d depth %d (bpv %d type %d)\n", current->name().c_str(), geom[i]->width, geom[i]->height, slices[current], bpv, type);
      GL_Error_Check;

      //Calibrate on data now so if colour bar drawn it will have correct range
      geom[i]->colourCalibrate();
    }

    //Setup gradient texture from colourmap
    ColourMap* cmap = geom[i]->draw->colourMap;
    if (cmap) cmap->loadTexture();
  }

  //Restore padding
  glPopClientAttrib();
  GL_Error_Check;
  t2 = clock();
  debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);

  if (!session.global("sort"))
  {
    vol_sort.clear();
    //Create the default un-sorted list
    unsigned int index = 0;
    for (unsigned int i=0; i<slices.size(); i++)
    {
      vol_sort.push_back(Distance(index, 0));
      index += slices[geom[i]->draw];
    }
  }
  else
    sort();
}

void Volumes::render(int i)
{
  Properties& props = geom[i]->draw->properties;

  float dims[3] = {geom[i]->render->vertices[1][0] - geom[i]->render->vertices[0][0],
                   geom[i]->render->vertices[1][1] - geom[i]->render->vertices[0][1],
                   geom[i]->render->vertices[1][2] - geom[i]->render->vertices[0][2]
                  };

  //Object scaling
  if (props.has("scale"))
  {
    float scale[3];
    Properties::toArray<float>(props["scale"], scale, 3);
    dims[0] *= scale[0];
    dims[1] *= scale[1];
    dims[2] *= scale[2];
  }

  GL_Error_Check;
  Shader_Ptr prog = session.shaders[lucVolumeType];

  //Uniform variables
  float viewport[4];
  glGetFloatv(GL_VIEWPORT, viewport);
  TextureData* voltexture = geom[i]->draw->useTexture(geom[i]->texture);
  if (!voltexture) 
  {
    fprintf(stderr, "No volume texture loaded for %s : %d!\n", geom[i]->draw->name().c_str(), i);
    return;
  }
  float res[3] = {(float)voltexture->width, (float)voltexture->height, (float)voltexture->depth};
  prog->setUniform3f("uResolution", res);
  prog->setUniform4f("uViewport", viewport);

  //User settings
  ColourMap* cmap = geom[i]->draw->colourMap;
  //if (cmap) cmap->calibrate(0, 1);
  //Setup gradient texture from colourmap if not yet loaded
  if (cmap && !cmap->texture) cmap->loadTexture();
  //Use per-object clip box if set, otherwise use global clip
  float bbMin[3] = {props.getFloat("xmin", 0.),
                    props.getFloat("ymin", 0.),
                    props.getFloat("zmin", 0.)
                   };
  float bbMax[3] = {props.getFloat("xmax", 1.),
                    props.getFloat("ymax", 1.),
                    props.getFloat("zmax", 1.)
                   };

  prog->setUniform3f("uBBMin", bbMin);
  prog->setUniform3f("uBBMax", bbMax);
  prog->setUniformi("uEnableColour", cmap ? 1 : 0);
  prog->setUniformf("uPower", props["power"]);
  prog->setUniformi("uSamples", props["samples"]);
  float opacity = props["opacity"], density = props["density"];
  prog->setUniformf("uDensityFactor", density * opacity);
  Colour colour = geom[i]->draw->properties.getColour("colour", 220, 220, 200, 255);
  bool hasiso = props.has("isovalue");
  float isoval = props["isovalue"];
  float isoalpha = props["isoalpha"];
  if (!hasiso) isoalpha = 0.0; //Skip drawing isosurface if no isovalue set
  colour.a = 255.0 * isoalpha * (colour.a/255.0);
  prog->setUniform("uIsoColour", colour);
  prog->setUniformf("uIsoSmooth", props["isosmooth"]);
  prog->setUniformi("uIsoWalls", (bool)props["isowalls"]);
  prog->setUniformi("uFilter", (bool)props["tricubicfilter"]);
  float dminmax[2] = {props["minclip"],
                      props["maxclip"]};
  prog->setUniform2f("uDenMinMax", dminmax);
  GL_Error_Check;

  //Field data requires normalisation to [0,1]
  //Pass minimum,maximum in place of colourmap calibrate
  Range range(0, 1);
  if (cmap && cmap->properties.has("range"))
  {
    float r[2] = {0.0, 1.0};
    Properties::toArray<float>(cmap->properties["range"], r, 2);
    if (r[0] < r[1])
    {
      range = Range(r[0], r[1]);
      //std::cout << "USING CMAP RANGE: " << range << std::endl;
    }
    //cmap->calibrate(&range);
  }
  else if (geom[i]->colourData()) // && (!cmap || !cmap->properties.has("range") || cmap->properties["range"].is_null()))
  {
    range = geom[i]->draw->ranges[geom[i]->colourData()->label];
    //For non float type, normalise isovalue to range [0,1] to match data
    //if (geom[i]->texture->type != VOLUME_FLOAT)
    //  isoval = (isoval - range.minimum) / (range.maximum - range.minimum);
    //prog->setUniform2f("uRange", range.data());
    //std::cout << "USING DATA RANGE: " << range << std::endl;
  }

  //std::cout << "Range " << range.minimum << " : " << range.maximum << std::endl;
  //Normalise provided isovalue to match data range
  isoval = (isoval - range.minimum) / (range.maximum - range.minimum);
  prog->setUniform2f("uRange", range.data());
  prog->setUniformf("uIsoValue", isoval);
  GL_Error_Check;

  //Gradient texture
  if (cmap)
  {
    TextureData* tex = cmap->texture->use();
    prog->setUniformi("uTransferFunction", tex->unit);
  }

  //Volume texture
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_3D, voltexture->id);
  prog->setUniformi("uVolume", 1);
  GL_Error_Check;

  //Get the matrices to send as uniform data
  float mvMatrix[16];
  float nMatrix[16];
  float pMatrix[16];
  float invPMatrix[16];
  float tmvMatrix[16];
  float matrix[16];
  float mvpMatrix[16];
  float invMVPMatrix[16];

  //Apply scaling to fit bounding box (maps volume dimensions to [0,1] cube)
  glPushMatrix();

  //Object rotation/translation
  Quaternion* qrot = NULL;
  Quaternion orot, vrot;
  if (props.has("rotate"))
  {
    float rot[4];
    json jrot = props["rotate"];
    if (jrot.size() == 4)
    {
      Properties::toArray<float>(jrot, rot, 4);
      orot = Quaternion(rot[0], rot[1], rot[2], rot[3]);
    }
    else
    {
      Properties::toArray<float>(jrot, rot, 3);
      orot.fromEuler(rot[0], rot[1], rot[2]);
    }

    qrot = &orot;
  }

  Vec3d translate;
  if (props.has("translate"))
  {
    float trans[3];
    Properties::toArray<float>(props["translate"], trans, 3);
    //glTranslatef(trans[0], trans[1], trans[2]);
    translate = trans;
  }

  //Get modelview without focal point / rotation centre adjustment
  bool rotatable = props["rotatable"]; //Object rotation by view flag
  if (rotatable)
  {
    //Rotate this object with view rotation setting as well as its own rotation
    vrot = view->getRotation() * orot;
    qrot = &vrot;
  }

  //Apply model view
  view->apply(rotatable, qrot, &translate);

  //Translate to our origin
  glTranslatef(geom[i]->render->vertices[0][0],
               geom[i]->render->vertices[0][1],
               geom[i]->render->vertices[0][2]);

  //Get the mvMatrix scaled by volume size
  //(used for depth calculations)
  glPushMatrix();
    glScalef(dims[0], dims[1], dims[2]);
    glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
  glPopMatrix();

  //Invert the scaling for raymarching
  glScalef(1.0/dims[0], 1.0/dims[1], 1.0/dims[2]);

  //Get the normal matrix
  glGetFloatv(GL_MODELVIEW_MATRIX, nMatrix);
  //Invert and transpose to get correct normal matrix even when non-uniform scaling used
  if (!gluInvertMatrixf(nMatrix, nMatrix)) abort_program("Uninvertable matrix!");
  transposeMatrixf(nMatrix);

  //Apply model scaling, inverse squared
  glScalef(1.0/(view->scale[0]*view->scale[0]), 1.0/(view->scale[1]*view->scale[1]), 1.0/(view->scale[2]*view->scale[2]));

  //Get the mv matrix used for ray marching at this point
  glGetFloatv(GL_MODELVIEW_MATRIX, mvMatrix);

  //Restore matrix
  glPopMatrix();

  //Get the projection matrix and invert
  glGetFloatv(GL_PROJECTION_MATRIX, pMatrix);
  GL_Error_Check;

  //Raymarching modelview and normal matrices
  prog->setUniformMatrixf("uMVMatrix", mvMatrix);

  //Pass transpose too
  copyMatrixf(mvMatrix, tmvMatrix);
  transposeMatrixf(tmvMatrix);
  prog->setUniformMatrixf("uTMVMatrix", tmvMatrix);

  prog->setUniformMatrixf("uNMatrix", nMatrix);

  //Calculate the combined modelview projection matrix
  multMatrixf(mvpMatrix, pMatrix, matrix);

  //Calculate the combined inverse modelview projection matrix
  if (!gluInvertMatrixf(pMatrix, invPMatrix)) abort_program("Uninvertable matrix!");
  transposeMatrixf(mvMatrix);
  multMatrixf(invMVPMatrix, mvMatrix, invPMatrix);

  //Combined projection and modelview matrices and inverses
  prog->setUniformMatrixf("uInvMVPMatrix", invMVPMatrix);
  prog->setUniformMatrixf("uMVPMatrix", mvpMatrix);
  GL_Error_Check;

  //State...
  glPushAttrib(GL_ENABLE_BIT);
  glEnable(GL_BLEND);
  //Blending for premultiplied alpha
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  //Draw two triangles to fill screen
  Imposter::draw();

  glPopAttrib();
  GL_Error_Check;
  glActiveTexture(GL_TEXTURE0);
  //Restore default blending mode
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  //Calibrate colourmap on data now so if colour bar drawn it will have correct range
  geom[i]->colourCalibrate();
}

ImageData* Volumes::getTiledImage(DrawingObject* draw, unsigned int index, unsigned int& iw, unsigned int& ih, unsigned int& channels, int xtiles)
{
  ImageData* image = new ImageData();
  unsigned int inc = slices[draw];
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    //Single volume cube
    if (geom[i]->depth > 1)
    {
      iw = geom[i]->width * xtiles;
      ih = ceil(geom[i]->depth / (float)xtiles) * geom[i]->height;
      if ((unsigned int)ih == geom[i]->height) iw = geom[i]->width * geom[i]->depth;
      int size = geom[i]->width * geom[i]->height;
      debug_print("Exporting Image: %s width %d height %d depth %d --> %d x %d\n", draw->name().c_str(), geom[i]->width, geom[i]->height, geom[i]->depth, iw, ih);
      unsigned int xoffset = 0, yoffset = 0;
      for (unsigned int z=0; z<geom[i]->depth; z++)
      {
        ImageData slice_image;
        getSliceImage(&slice_image, geom[i].get(), z*size);

        if (z==0)
        {
          channels = slice_image.channels;
          image->allocate(iw, ih, channels);
          image->clear();
        }

        //Copy slice image to tile region of image
        for (unsigned int y=0; y<slice_image.height; y++)
          for (unsigned int x=0; x<slice_image.width; x++)
            for (unsigned int z=0; z<channels; z++)
              image->pixels[iw*channels * (y + yoffset) + channels*(x + xoffset) + z] = slice_image.pixels[y*channels * slice_image.width + x*channels + z];

        xoffset += geom[i]->width;
        if (xoffset > iw-geom[i]->width)
        {
          xoffset = 0;
          yoffset += geom[i]->height;
        }
      }
      break;
    }
    //Slices: load selected index volume only
    else if (index == i && geom[i]->draw == draw)
    {
      unsigned int xoffset = 0, yoffset = 0;

      for (unsigned int j=i; j<i+slices[draw]; j++)
      {
        ImageData slice_image;
        getSliceImage(&slice_image, geom[j].get());

        if (j==i)
        {
          iw = slice_image.width * xtiles;
          ih = ceil(slices[draw] / (float)xtiles) * slice_image.height;
          channels = slice_image.channels;
          if (ih == slice_image.height) iw = slice_image.width * slices[draw];
          debug_print("Exporting Image: %s width %d height %d depth %d --> %d x %d\n", draw->name().c_str(), slice_image.width, slice_image.height, slices[draw], iw, ih);
          image->allocate(iw, ih, channels);
          image->clear();
        }

        //Copy slice image to tile region of image
        for (unsigned int y=0; y<slice_image.height; y++)
          for (unsigned int x=0; x<slice_image.width; x++)
            for (unsigned int z=0; z<channels; z++)
              image->pixels[iw*channels * (y + yoffset) + channels*(x + xoffset) + z] = slice_image.pixels[y*channels * slice_image.width + x*channels + z];

        xoffset += slice_image.width;
        if (xoffset > iw-slice_image.width)
        {
          xoffset = 0;
          yoffset += slice_image.height;
        }
      }
      break;
    }
  }
  return image;
}

void Volumes::saveTiledImage(DrawingObject* draw, int xtiles)
{
  int count = 0;
  unsigned int inc = slices[draw];
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    if (geom[i]->draw == draw && drawable(i))
    {
      unsigned int iw, ih, channels;
      ImageData *image = getTiledImage(draw, i, iw, ih, channels, xtiles);
      if (!image) return;
      char path[FILE_PATH_MAX];
      sprintf(path, "%s_%d", geom[i]->draw->name().c_str(), count++);
      image->write(path);
      delete image;
      break;  //Done
    }
  }
}

void Volumes::getSliceImage(ImageData* image, GeomData* slice, int offset)
{
  int width = slice->width;
  int height = slice->height;

  if (slice->render->colours.size() > 0)
  {
    if (!height) height = slice->render->colours.size() / width;
    image->allocate(width, height, 4); //RGBA
    image->clear();

    memcpy(image->pixels, slice->render->colours.ref(offset), image->width*image->height*4);
  }
  else if (slice->render->rgb.size() > 0)
  {
    if (!height) height = slice->render->rgb.size() / width;
    image->allocate(width, height, 3); //RGB
    image->clear();

    memcpy(image->pixels, slice->render->rgb.ref(offset), image->width*image->height*3);
  }
  else if (slice->render->luminance.size() > 0)
  {
    if (!height) height = slice->render->luminance.size() / width;
    image->allocate(width, height, 1); //Luminance
    image->clear();

    memcpy(image->pixels, slice->render->luminance.ref(offset), image->width*image->height);
  }
  else if (slice->colourData() != nullptr)
  {
    if (!height) height = slice->colourData()->size() / width;
    image->allocate(width, height, 1); //Luminance
    image->clear();
    auto dataRange = slice->draw->ranges[slice->colourData()->label];
    float min = dataRange.minimum;
    float range = dataRange.maximum - min;
    for (int y=0; y<height; y++)
    {
      for (int x=0; x<width; x++)
      {
        float val = slice->colourData(offset + y * width + x);
        val = (val - min) / range * 255;
        image->pixels[y * width + x] = (unsigned int)val;
      }
    }
  }
}

void Volumes::saveSliceImages(DrawingObject* draw, unsigned int index)
{
  ImageData image;
  unsigned int inc = slices[draw];
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    //Single volume cube
    if (geom[i]->depth > 1)
    {
      int size = geom[i]->width * geom[i]->height;
      for (unsigned int z=0; z<geom[i]->depth; z++)
      {
        getSliceImage(&image, geom[i].get(), z*size);
        //Write slice image
        char fn[256];
        sprintf(fn, "%s_%d.jpg", draw->name().c_str(), z);
        image.write(fn);
      }
      break;
    }
    //Slices: load selected index volume only
    else if (index == i && geom[i]->draw == draw)
    {
      for (unsigned int j=i; j<i+slices[draw]; j++)
      {
        getSliceImage(&image, geom[j].get());

        //Write slice image
        char fn[256];
        sprintf(fn, "%s_%d.jpg", draw->name().c_str(), j-i);
        image.write(fn);
      }
      break;
    }
  }
}

void Volumes::jsonWrite(DrawingObject* draw, json& obj)
{
  update();  //Count slices etc...
  //Note: update() must be called first to fill slices[]
  auto it = slices.find(draw);
  if (it == slices.end()) return;
  unsigned int inc = it->second;
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    if (geom[i]->draw == draw && drawable(i))
    {
      json data, vertices, volume;
      int height = geom[i]->height;
      //Height needs calculating from values data?
      if (!geom[i]->height && geom[i]->colourData())
        height = geom[i]->colourData()->size() / geom[i]->width;

      /* This is for exporting the floating point volume data cube, may use in future when WebGL supports 3D textures...
      printf("Exporting: %d width %d height %d depth %d\n", id, geom[i]->width, height, inc);
      int sliceSize = geom[i]->width * height;
      float* vol = new float[sliceSize * inc];
      size_t offset = 0;
      for (int j=i; j<i+inc; j++)
      {
         size_t size = sliceSize * sizeof(float);
         memcpy(vol + offset, geom[j]->colourData()->ref(), size);
         offset += sliceSize;
      }*/

      //Get a tiled image for WebGL to use as a 2D texture...
      unsigned int iw, ih, channels; //TODO: Support other pixel formats
      ImageData *image = getTiledImage(draw, i, iw, ih, channels, 16); //16 * 256 = 4096^2 square texture
      if (!image) continue;
      std::string imagestr = image->getURIString();
      delete image;
      json res, scale;
      res.push_back((int)geom[i]->width);
      res.push_back(height);
      if (inc > 1)
        res.push_back(inc);
      else
        res.push_back(geom[i]->depth);
      //Object scaling
      float s[3] = {1.0, 1.0, 1.0};
      if (geom[i]->draw->properties.has("scale"))
        Properties::toArray<float>(geom[i]->draw->properties["scale"], s, 3);
      //Scaling factors
      scale.push_back((geom[i]->render->vertices[1][0] - geom[i]->render->vertices[0][0]) * s[0]);
      scale.push_back((geom[i]->render->vertices[1][1] - geom[i]->render->vertices[0][1]) * s[1]);
      scale.push_back((geom[i]->render->vertices[1][2] - geom[i]->render->vertices[0][2]) * s[2]);
      volume["res"] = res;
      volume["scale"] = scale;

      if (geom[i]->colourData())
      {
        auto range = geom[i]->draw->ranges[geom[i]->colourData()->label];
        volume["minimum"] = range.minimum;
        volume["maximum"] = range.maximum;
      }

      volume["size"] = 1;
      //volume["count"] = ;
      volume["url"] = imagestr;
      //volume["data"] = base64_encode(reinterpret_cast<const unsigned char*>(volume), sliceSize * inc * sizeof(float)); //For 3D export
      obj["volume"] = volume;

      //Write image to disk (for debugging)
      //saveImage(draw);

      //Only one volume per object supported
      break;
    }
  }
}

void Volumes::isosurface(Triangles* surfaces, DrawingObject* source, DrawingObject* target, bool clearvol)
{
  //Ensure data is loaded for this step
  setup(view);
  countSlices();

  //Isosurface extract
  Isosurface iso(geom, surfaces, source, target, this);

  //Clear the volume data, allows converting object from a volume to a surface
  if (clearvol)
    clear(true);

  reload = redraw = true;

  //Optimise triangle vertices
  surfaces->update();
  //surfaces->loadMesh();
  //reload = redraw = true;
}

