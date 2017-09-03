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

Volumes::Volumes(DrawState& drawstate) : Imposter(drawstate)
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

  //Iterate geom and delete textures
  //"gpucache" property allows switching this behaviour off for faster switching
  //(usually set globally but checked per object for volumes)
  //requries enough GPU ram to store all volumes
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (!geom[i]->draw->properties["gpucache"] && geom[i]->texture)
    {
      delete geom[i]->texture;
      geom[i]->texture = NULL;
      reload = true;
    }
  }
}

void Volumes::draw()
{
  //clock_t t1,t2,tt;
  //t1 = tt = clock();

  //Each object can only have one volume,
  //but each volume can consist of separate slices or a single cube
  DrawingObject* current = NULL;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (!drawable(i)) continue;

    if (current != geom[i]->draw)
    {
      current = geom[i]->draw;

      setState(i, drawstate.prog[lucVolumeType]); //Set draw state settings for this object
      render(i);

      GL_Error_Check;
    }
  }

  glUseProgram(0);
  glBindTexture(GL_TEXTURE_3D, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  //t2 = clock(); debug_print("  Draw %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);

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

  for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw])
  {
    if (!drawable(i)) continue;

    //Required to cache colour value info
    geom[i]->colourCalibrate();

    //Single volume cube
    if (geom[i]->depth > 1)
    {
      DrawingObject* current = geom[i]->draw;
      bool texcompress = current->properties["compresstextures"];
      if (!geom[i]->texture || !geom[i]->texture->texture || geom[i]->texture->texture->width == 0) //Width set to 0 to flag reload
      {
        //Determine type of data then load the texture
        if (!geom[i]->texture) geom[i]->texture = new ImageLoader(); //Add a new texture container
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
      continue;
    }

    //Collection of 2D slices
    DrawingObject* current = geom[i]->draw;
    bool texcompress = current->properties["compresstextures"];
    if (!geom[i]->texture || !geom[i]->texture->texture || geom[i]->texture->texture->width == 0) //Width set to 0 to flag reload
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
      if (!geom[i]->texture) geom[i]->texture = new ImageLoader(); //Add a new texture container
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
}

void Volumes::render(int i)
{
  float dims[3] = {geom[i]->render->vertices[1][0] - geom[i]->render->vertices[0][0],
                   geom[i]->render->vertices[1][1] - geom[i]->render->vertices[0][1],
                   geom[i]->render->vertices[1][2] - geom[i]->render->vertices[0][2]
                  };

  GL_Error_Check;
  Shader* prog = drawstate.prog[lucVolumeType];

  //Uniform variables
  Properties& props = geom[i]->draw->properties;
  float viewport[4];
  glGetFloatv(GL_VIEWPORT, viewport);
  TextureData* voltexture = geom[i]->draw->useTexture(geom[i]->texture);
  if (!voltexture) 
  {
    fprintf(stderr, "No volume texture loaded for %s : %d!\n", geom[i]->draw->name().c_str(), i);
    return;
  }
  float res[3] = {(float)voltexture->width, (float)voltexture->height, (float)voltexture->depth};
  glUniform3fv(prog->uniforms["uResolution"], 1, res);
  glUniform4fv(prog->uniforms["uViewport"], 1, viewport);

  //User settings
  ColourMap* cmap = geom[i]->draw->colourMap;
  //if (cmap) cmap->calibrate(0, 1);
  //Setup gradient texture from colourmap if not yet loaded
  if (cmap && !cmap->texture) cmap->loadTexture();
  //Use per-object clip box if set, otherwise use global clip
  /*
  float bbMin[3] = {props.getFloat("xmin", 0.01) * geom[i]->render->vertices[0][0]),
                    props.getFloat("ymin", 0.01) * geom[i]->render->vertices[0][1]),
                    props.getFloat("zmin", 0.01) * geom[i]->render->vertices[0][2])};
  float bbMax[3] = {props.getFloat("xmax", 0.99) * geom[i]->render->vertices[1][0]),
                    props.getFloat("ymax", 0.99) * geom[i]->render->vertices[1][1]),
                    props.getFloat("zmax", 0.99) * geom[i]->render->vertices[1][2])};
  */
  float bbMin[3] = {props.getFloat("xmin", 0.),
                    props.getFloat("ymin", 0.),
                    props.getFloat("zmin", 0.)
                   };
  float bbMax[3] = {props.getFloat("xmax", 1.),
                    props.getFloat("ymax", 1.),
                    props.getFloat("zmax", 1.)
                   };

  glUniform3fv(prog->uniforms["uBBMin"], 1, bbMin);
  glUniform3fv(prog->uniforms["uBBMax"], 1, bbMax);
  glUniform1i(prog->uniforms["uEnableColour"], cmap ? 1 : 0);
  glUniform1f(prog->uniforms["uPower"], props["power"]);
  glUniform1i(prog->uniforms["uSamples"], props["samples"]);
  float opacity = props["opacity"], density = props["density"];
  glUniform1f(prog->uniforms["uDensityFactor"], density * opacity);
  Colour colour = geom[i]->draw->properties.getColour("colour", 220, 220, 200, 255);
  float isoval = props["isovalue"];
  float isoalpha = props["isoalpha"];
  if (isoval == FLT_MAX) isoalpha = 0.0; //Skip drawing isosurface if no isovalue set
  colour.a = 255.0 * isoalpha * (colour.a/255.0);
  colour.setUniform(prog->uniforms["uIsoColour"]);
  glUniform1f(prog->uniforms["uIsoSmooth"], props["isosmooth"]);
  glUniform1i(prog->uniforms["uIsoWalls"], (bool)props["isowalls"]);
  glUniform1i(prog->uniforms["uFilter"], (bool)props["tricubicfilter"]);
  float dminmax[2] = {props["dminclip"],
                      props["dmaxclip"]};
  glUniform2fv(prog->uniforms["uDenMinMax"], 1, dminmax);
  GL_Error_Check;

  //Field data requires normalisation to [0,1]
  //Pass minimum,maximum in place of colourmap calibrate
  float range[2] = {0.0, 1.0};
  if (geom[i]->colourData())
  {
    range[0] = geom[i]->colourData()->minimum;
    range[1] = geom[i]->colourData()->maximum;
    //For non float type, normalise isovalue to range [0,1] to match data
    if (geom[i]->texture->type != VOLUME_FLOAT)
      isoval = (isoval - range[0]) / (range[1] - range[0]);
    //std::cout << "IsoValue " << isoval << std::endl;
    //std::cout << "Range " << range[0] << " : " << range[1] << std::endl;
  }
  glUniform2fv(prog->uniforms["uRange"], 1, range);
  glUniform1f(prog->uniforms["uIsoValue"], isoval);
  GL_Error_Check;

  //Gradient texture
  if (cmap)
  {
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(prog->uniforms["uTransferFunction"], 0);
    glBindTexture(GL_TEXTURE_2D, cmap->texture->id);
  }

  //Volume texture
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_3D, voltexture->id);
  glUniform1i(prog->uniforms["uVolume"], 1);
  GL_Error_Check;

  //Get the matrices to send as uniform data
  float mvMatrix[16];
  float nMatrix[16];
  float pMatrix[16];
  float invPMatrix[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, nMatrix);

  //Apply scaling to fit bounding box (maps volume dimensions to [0,1] cube)
  glPushMatrix();

  //Get modelview without focal point / rotation centre adjustment
  bool rotatable = props["rotatable"]; //Object rotation by view flag
  if (rotatable)
    view->apply(false, false);
  else
    view->apply(false);

  //Object rotation/translation
  if (props.has("translate"))
  {
    float trans[3];
    Properties::toArray<float>(props["translate"], trans, 3);
    glTranslatef(trans[0], trans[1], trans[2]);
  }

  if (props.has("rotate"))
  {
    float rot[4];
    Quaternion qrot;
    json jrot = props["rotate"];
    if (jrot.size() == 4)
    {
      Properties::toArray<float>(jrot, rot, 4);
      qrot = Quaternion(rot[0], rot[1], rot[2], rot[3]);
    }
    else
    {
      Properties::toArray<float>(jrot, rot, 3);
      qrot.fromEuler(rot[0], rot[1], rot[2]);
    }
    qrot.apply();
  }
  //Rotate this object with view rotation setting
  if (rotatable)
    view->getRotation().apply();

  //printf("DIMS: %f,%f,%f TRANS: %f,%f,%f SCALE: %f,%f,%f\n", dims[0], dims[1], dims[2], -dims[0]*0.5, -dims[1]*0.5, -dims[2]*0.5, 1.0/dims[0], 1.0/dims[1], 1.0/dims[2]);
  glTranslatef(-dims[0]*0.5, -dims[1]*0.5, -dims[2]*0.5);  //Translate to origin
  glScalef(1.0/dims[0], 1.0/dims[1], 1.0/dims[2]);
  //glScalef(1.0/dims[0]*view->scale[0], 1.0/dims[1]*view->scale[1], 1.0/dims[2]*view->scale[2]);
  glScalef(1.0/(view->scale[0]*view->scale[0]), 1.0/(view->scale[1]*view->scale[1]), 1.0/(view->scale[2]*view->scale[2]));

  //Object scaling
  if (props.has("scale"))
  {
    float scale[3];
    Properties::toArray<float>(props["scale"], scale, 3);
    glScalef(1.0/scale[0], 1.0/scale[1], 1.0/scale[2]);
  }

  glGetFloatv(GL_MODELVIEW_MATRIX, mvMatrix);
  glPopMatrix();
  glGetFloatv(GL_PROJECTION_MATRIX, pMatrix);
  if (!gluInvertMatrixf(pMatrix, invPMatrix)) abort_program("Uninvertable matrix!");
  GL_Error_Check;

  //Projection and modelview matrices
  glUniformMatrix4fv(prog->uniforms["uPMatrix"], 1, GL_FALSE, pMatrix);
  glUniformMatrix4fv(prog->uniforms["uInvPMatrix"], 1, GL_FALSE, invPMatrix);
  glUniformMatrix4fv(prog->uniforms["uMVMatrix"], 1, GL_FALSE, mvMatrix);
  nMatrix[12] = nMatrix[13] = nMatrix[14] = 0; //Removing translation works as long as no non-uniform scaling
  glUniformMatrix4fv(prog->uniforms["uNMatrix"], 1, GL_FALSE, nMatrix);
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

ImageData* Volumes::getTiledImage(DrawingObject* draw, unsigned int index, int& iw, int& ih, int& channels, int xtiles)
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
          image->allocate(iw, ih, slice_image.channels);
          image->clear();
        }

        //Copy slice image to tile region of image
        for (int y=0; y<slice_image.height; y++)
          for (int x=0; x<slice_image.width; x++)
            image->pixels[iw * (y + yoffset) + x + xoffset] = slice_image.pixels[y * slice_image.width + x];

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
      int xoffset = 0, yoffset = 0;

      for (unsigned int j=i; j<i+slices[draw]; j++)
      {
        ImageData slice_image;
        getSliceImage(&slice_image, geom[j].get());

        if (j==i)
        {
          iw = slice_image.width * xtiles;
          ih = ceil(slices[draw] / (float)xtiles) * slice_image.height;
          if (ih == slice_image.height) iw = slice_image.width * slices[draw];
          debug_print("Exporting Image: %s width %d height %d depth %d --> %d x %d\n", draw->name().c_str(), slice_image.width, slice_image.height, slices[draw], iw, ih);
          image->allocate(iw, ih, slice_image.channels);
          image->clear();
        }

        //Copy slice image to tile region of image
        for (int y=0; y<slice_image.height; y++)
          for (int x=0; x<slice_image.width; x++)
            image->pixels[iw * (y + yoffset) + x + xoffset] = slice_image.pixels[y * slice_image.width + x];

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
      int iw, ih, channels;
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

ImageData* Volumes::getSliceImage(ImageData* image, GeomData* slice, int offset)
{
  int width = slice->width;
  int height = slice->height;
  int channels = 0;

  if (slice->colourData() != nullptr)
  {
    if (!height) height = slice->colourData()->size() / width;
    image->allocate(width, height, 1); //Luminance
    image->clear();

    float min = slice->colourData()->minimum;
    float range = slice->colourData()->maximum - min;
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
  else if (slice->render->colours.size() > 0)
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
      unsigned int xoffset = 0, yoffset = 0;
      for (unsigned int z=0; z<geom[i]->depth; z++)
      {
        getSliceImage(&image, geom[i].get(), z*size);
        //Write slice image
        char fn[256];
        sprintf(fn, "%s_%d.jpg", draw->name().c_str(), z);
        printf(fn);
        image.write(fn);
      }
      break;
    }
    //Slices: load selected index volume only
    else if (index == i && geom[i]->draw == draw)
    {
      int xoffset = 0, yoffset = 0;

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
  unsigned int inc = slices[draw];
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    if (geom[i]->draw == draw && drawable(i))
    {
      json data, vertices, volume;
      int height = geom[i]->height;
      //Height needs calculating from values data?
      if (!geom[i]->depth)
        height = geom[i]->colourData()->size() / geom[i]->width;

      /* This is for exporting the floating point volume data cube, may use in future when WebGL supports 3D textures...
      printf("Exporting: %d width %d height %d depth %d\n", id, geom[i]->width, height, slices[draw]);
      int sliceSize = geom[i]->width * height;
      float* vol = new float[sliceSize * slices[draw]];
      size_t offset = 0;
      for (int j=i; j<i+slices[draw]; j++)
      {
         size_t size = sliceSize * sizeof(float);
         memcpy(vol + offset, geom[j]->colourData()->ref(), size);
         offset += sliceSize;
      }*/

      //Get a tiled image for WebGL to use as a 2D texture...
      int iw, ih, channels; //TODO: Support other pixel formats
      ImageData *image = getTiledImage(draw, i, iw, ih, channels, 16); //16 * 256 = 4096^2 square texture
      if (!image) continue;
      std::string imagestr = getImageUrlString(image);
      delete image;
      json res, scale;
      res.push_back((int)geom[i]->width);
      res.push_back(height);
      if (slices[draw])
        res.push_back(slices[draw]);
      else
        res.push_back(geom[i]->depth);
      //Scaling factors
      scale.push_back(geom[i]->render->vertices[1][0] - geom[i]->render->vertices[0][0]);
      scale.push_back(geom[i]->render->vertices[1][1] - geom[i]->render->vertices[0][1]);
      scale.push_back(geom[i]->render->vertices[1][2] - geom[i]->render->vertices[0][2]);
      volume["res"] = res;
      volume["scale"] = scale;

      volume["size"] = 1;
      //volume["count"] = ;
      volume["url"] = imagestr;
      //volume["data"] = base64_encode(reinterpret_cast<const unsigned char*>(volume), sliceSize * slices[draw] * sizeof(float)); //For 3D export
      obj["volume"] = volume;

      //Write image to disk (for debugging)
      //saveImage(draw);

      //Only one volume per object supported
      break;
    }
  }
}

void Volumes::isosurface(Triangles* surfaces, DrawingObject* target, bool clearvol)
{
  //Isosurface extract
  Isosurface iso(geom, surfaces, target);

  //Clear the volume data, allows converting object from a volume to a surface
  if (clearvol) clear();

  //Optimise triangle vertices
  surfaces->loadMesh();
}

