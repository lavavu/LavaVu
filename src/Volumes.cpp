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

Shader* Volumes::prog = NULL;

Volumes::Volumes() : Geometry()
{
  type = lucVolumeType;
}

Volumes::~Volumes()
{

}

void Volumes::close()
{
  Geometry::close();
  //Iterate geom and delete textures
  //"cachevolumes" property allows switching this behaviour off for faster switching
  //requries enough GPU ram to store all volumes
  if (Geometry::properties["cachevolumes"].ToBool(false)) return;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (geom[i]->texture)
    {
      delete geom[i]->texture;
      geom[i]->texture = NULL;
    }
  }
}

void Volumes::draw()
{
  //Draw
  Geometry::draw();
  if (drawcount == 0) return;

  //clock_t t1,t2,tt;
  //t1 = tt = clock();

  DrawingObject* current = NULL;
  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (!drawable(i)) continue;

    if (current != geom[i]->draw)
    {
      current = geom[i]->draw;

      setState(i, prog); //Set draw state settings for this object
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
  clock_t t2,tt;
  tt = clock();

  Geometry::update();

  int maxtex;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxtex);
  debug_print("Max 3D texture size %d\n", maxtex);

  //Count slices in each volume
  //printf("Total slices: %d\n", geom.size());
  if (!geom.size()) return;

  //Padding!
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  //TODO: filtering
  //Read all colourvalues, apply filter to each and store in filtered block before loading into texture

  //Single volume cube
  if (geom.size() == 1)
  {
    int i = 0;
    if (!drawable(i)) return;

    DrawingObject* current = geom[i]->draw;
    if (!geom[i]->texture || geom[i]->texture->width == 0)
    {
#if 0
      //Dump raw
      FILE* pFile;
      pFile = fopen("volume.raw", "wb");
      fwrite(geom[i]->colourData()->ref(), 1, sizeof(float) * geom[i]->colourData()->size(), pFile);
      fclose(pFile);
      std::cerr << "Wrote raw volume\n";
#endif
      //Determine type of data then load the texture
      unsigned int bpv = 0;
      if (geom[i]->colours.size() > 0)
      {
        bpv = (4 * geom[i]->colours.size()) / (float)(geom[i]->width * geom[i]->height * geom[i]->depth);
        if (bpv == 3)
          current->load3DTexture(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->colours.ref(), VOLUME_RGB);
        if (bpv == 4)
          current->load3DTexture(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->colours.ref(), VOLUME_RGBA);
      }
      else if (geom[i]->colourData())
      {
        bpv = (4 * geom[i]->colourData()->size()) / (float)(geom[i]->width * geom[i]->height * geom[i]->depth);
        if (bpv == 1)
          current->load3DTexture(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->colourData()->ref(), VOLUME_BYTE);
        if (bpv == 4)
          current->load3DTexture(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->colourData()->ref(), VOLUME_FLOAT);
      }
      debug_print("volume 0 width %d height %d depth %d (bpv %d)\n", geom[i]->width, geom[i]->height, geom[i]->depth, bpv);
      geom[i]->texture = current->defaultTexture;
      current->defaultTexture = NULL; ////
    }

    //Setup gradient texture from colourmap
    if (geom[i]->draw->colourMaps[lucColourValueData] && !geom[i]->draw->colourMaps[lucColourValueData]->texture)
      geom[i]->draw->colourMaps[lucColourValueData]->loadTexture();
  }
  else
  {
    //Collection of 2D slices
    slices.clear();
    unsigned int id = geom[0]->draw->id;
    unsigned int count = 0;
    for (unsigned int i=0; i<=geom.size(); i++)
    {
      //Force reload
      //if (i<geom.size() && geom[i]->texture)
      //  geom[i]->texture->width = 0;
      if (i<geom.size() && geom[i]->draw->defaultTexture)
        geom[i]->draw->defaultTexture->width = 0;
      if (i==geom.size() || id != geom[i]->draw->id)
      {
        slices[id] = count;
        debug_print("Reloading: %d slices in object %d\n", count, id);
        count = 0;
        if (i<geom.size()) id = geom[i]->draw->id;
      }
      count++;
    }

    for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw->id])
    {
      if (!drawable(i)) continue;

      DrawingObject* current = geom[i]->draw;
      if (!current->defaultTexture || current->defaultTexture->width == 0)
      {
        if (!geom[i]->height)
          //No height? Calculate from values data (assumes float data (4 bpv))
          geom[i]->height = geom[i]->colourData()->size() / geom[i]->width; // * 4;

        assert(geom[i]->width <= (unsigned)maxtex);
        assert(geom[i]->height <= (unsigned)maxtex);
        assert(slices[current->id] <= maxtex);

        //Init/allocate/bind texture
        unsigned int bpv = 0;
        GL_Error_Check;
        if (geom[i]->colours.size() > 0)
        {
          bpv = (4 * geom[i]->colours.size()) / (float)(geom[i]->width * geom[i]->height);
          if (bpv == 3)
          {
            current->load3DTexture(geom[i]->width, geom[i]->height, slices[current->id], NULL, VOLUME_RGB);
            for (unsigned int j=i; j<i+slices[current->id]; j++)
              glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, j-i, geom[i]->width, geom[i]->height, 1, GL_RGB, GL_UNSIGNED_BYTE, geom[j]->colours.ref());
          }
          if (bpv == 4)
          {
            current->load3DTexture(geom[i]->width, geom[i]->height, slices[current->id], NULL, VOLUME_RGBA);
            for (unsigned int j=i; j<i+slices[current->id]; j++)
              glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, j-i, geom[i]->width, geom[i]->height, 1, GL_RGBA, GL_UNSIGNED_BYTE, geom[j]->colours.ref());
          }
        }
        else if (geom[i]->colourData())
        {
          bpv = (4 * geom[i]->colourData()->size()) / (float)(geom[i]->width * geom[i]->height);
          if (bpv == 1)
          {
            current->load3DTexture(geom[i]->width, geom[i]->height, slices[current->id], NULL, VOLUME_BYTE);
            for (unsigned int j=i; j<i+slices[current->id]; j++)
              glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, j-i, geom[i]->width, geom[i]->height, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, geom[j]->colourData()->ref());
          }
          if (bpv == 4)
          {
            current->load3DTexture(geom[i]->width, geom[i]->height, slices[current->id], NULL, VOLUME_FLOAT);
            for (unsigned int j=i; j<i+slices[current->id]; j++)
              glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, j-i, geom[i]->width, geom[i]->height, 1, GL_LUMINANCE, GL_FLOAT, geom[j]->colourData()->ref());
          }
        }
        debug_print("current %d width %d height %d depth %d (bpv %d)\n", current->id, geom[i]->width, geom[i]->height, slices[current->id], bpv);

        geom[i]->texture = current->defaultTexture;
        GL_Error_Check;

        //Calibrate on data now so if colour bar drawn it will have correct range
        geom[i]->colourCalibrate();
      }

      //Setup gradient texture from colourmap
      if (geom[i]->draw->colourMaps[lucColourValueData] && !geom[i]->draw->colourMaps[lucColourValueData]->texture)
        geom[i]->draw->colourMaps[lucColourValueData]->loadTexture();
    }
  }
  //Restore padding
  glPopClientAttrib();
  GL_Error_Check;
  t2 = clock();
  debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void Volumes::render(int i)
{
  float dims[3] = {geom[i]->vertices[1][0] - geom[i]->vertices[0][0],
                   geom[i]->vertices[1][1] - geom[i]->vertices[0][1],
                   geom[i]->vertices[1][2] - geom[i]->vertices[0][2]
                  };

  assert(prog);
  GL_Error_Check;

  //Uniform variables
  float viewport[4];
  glGetFloatv(GL_VIEWPORT, viewport);
  TextureData* voltexture = geom[i]->texture;
  if (!voltexture) voltexture = geom[i]->draw->defaultTexture;
  float res[3] = {(float)voltexture->width, (float)voltexture->height, (float)voltexture->depth};
  glUniform3fv(prog->uniforms["uResolution"], 1, res);
  glUniform4fv(prog->uniforms["uViewport"], 1, viewport);

  //User settings
  json::Object props = geom[i]->draw->properties;
  bool hasColourMap = geom[i]->draw->colourMaps[lucColourValueData]
                      && geom[i]->draw->colourMaps[lucColourValueData]
                      && props["colourmap"].ToBool(true);
  //Use per-object clip box if set, otherwise use global clip
  /*
  float bbMin[3] = {props["xmin"].ToFloat(Geometry::properties["xmin"].ToFloat(0.01) * geom[i]->vertices[0][0]),
                    props["ymin"].ToFloat(Geometry::properties["ymin"].ToFloat(0.01) * geom[i]->vertices[0][1]),
                    props["zmin"].ToFloat(Geometry::properties["zmin"].ToFloat(0.01) * geom[i]->vertices[0][2])};
  float bbMax[3] = {props["xmax"].ToFloat(Geometry::properties["xmax"].ToFloat(0.99) * geom[i]->vertices[1][0]),
                    props["ymax"].ToFloat(Geometry::properties["ymax"].ToFloat(0.99) * geom[i]->vertices[1][1]),
                    props["zmax"].ToFloat(Geometry::properties["zmax"].ToFloat(0.99) * geom[i]->vertices[1][2])};
  */
  float bbMin[3] = {props["xmin"].ToFloat(Geometry::properties["xmin"].ToFloat(0.01)),
                    props["ymin"].ToFloat(Geometry::properties["ymin"].ToFloat(0.01)),
                    props["zmin"].ToFloat(Geometry::properties["zmin"].ToFloat(0.01))
                   };
  float bbMax[3] = {props["xmax"].ToFloat(Geometry::properties["xmax"].ToFloat(0.99)),
                    props["ymax"].ToFloat(Geometry::properties["ymax"].ToFloat(0.99)),
                    props["zmax"].ToFloat(Geometry::properties["zmax"].ToFloat(0.99))
                   };
  glUniform3fv(prog->uniforms["uBBMin"], 1, bbMin);
  glUniform3fv(prog->uniforms["uBBMax"], 1, bbMax);
  glUniform1i(prog->uniforms["uEnableColour"], hasColourMap ? 1 : 0);
  glUniform1f(prog->uniforms["uPower"], props["power"].ToFloat(1.0));
  glUniform1i(prog->uniforms["uSamples"], props["samples"].ToInt(256));
  glUniform1f(prog->uniforms["uDensityFactor"], props["density"].ToFloat(5.0) * props["opacity"].ToFloat(1.0));
  glUniform1f(prog->uniforms["uIsoValue"], props["isovalue"].ToFloat(0));
  Colour colour = Colour_FromJson(props, "colour", 220, 220, 200, 255);
  colour.a = 255.0 * props["isoalpha"].ToFloat(colour.a/255.0);
  Colour_SetUniform(prog->uniforms["uIsoColour"], colour);
  glUniform1f(prog->uniforms["uIsoSmooth"], props["isosmooth"].ToFloat(0.1));
  glUniform1i(prog->uniforms["uIsoWalls"], props["isowalls"].ToInt(0));
  glUniform1i(prog->uniforms["uFilter"], props["tricubicfilter"].ToInt(0));
   //density min max
   float dminmax[4] = { props["dmin"].ToFloat(0.0),
                        props["dminclip"].ToInt(0),
                        props["dmax"].ToFloat(1.0),
                        props["dmaxclip"].ToInt(0)};
   glUniform4fv(prog->uniforms["uDenMinMax"], 1, dminmax);
  GL_Error_Check;

  //Field data requires normalisation to [0,1]
  //Pass minimum,maximum in place of colourmap calibrate
  float range[2] = {0.0, 1.0};
  if (geom[i]->colourData())
  {
    range[0] = geom[i]->colourData()->minimum;
    range[1] = geom[i]->colourData()->maximum;
  }
  glUniform2fv(prog->uniforms["uRange"], 1, range);
  GL_Error_Check;

  //Gradient texture
  if (hasColourMap)
  {
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(prog->uniforms["uTransferFunction"], 0);
    glBindTexture(GL_TEXTURE_2D, geom[i]->draw->colourMaps[lucColourValueData]->texture->id);
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
#ifndef USE_OMEGALIB
  //Get modelview without focal point / rotation centre adjustment
  glLoadIdentity();
  view->apply(false);
#endif
  //printf("DIMS: %f,%f,%f TRANS: %f,%f,%f SCALE: %f,%f,%f\n", dims[0], dims[1], dims[2], -dims[0]*0.5, -dims[1]*0.5, -dims[2]*0.5, 1.0/dims[0], 1.0/dims[1], 1.0/dims[2]);
  glTranslatef(-dims[0]*0.5, -dims[1]*0.5, -dims[2]*0.5);  //Translate to origin
  glScalef(1.0/dims[0], 1.0/dims[1], 1.0/dims[2]);
  //glScalef(1.0/dims[0]*view->scale[0], 1.0/dims[1]*view->scale[1], 1.0/dims[2]*view->scale[2]);
  glScalef(1.0/(view->scale[0]*view->scale[0]), 1.0/(view->scale[1]*view->scale[1]), 1.0/(view->scale[2]*view->scale[2]));
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
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);  //No depth testing to allow multi-pass blend!
  glDisable(GL_MULTISAMPLE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  //Draw two triangles to fill screen
  glBegin(GL_TRIANGLES);
  glVertex2f(-1, -1);
  glVertex2f(-1, 1);
  glVertex2f(1, -1);
  glVertex2f(-1,  1);
  glVertex2f( 1, 1);
  glVertex2f(1, -1);
  glEnd();

  glPopAttrib();
  GL_Error_Check;
  glActiveTexture(GL_TEXTURE0);
}

GLubyte* Volumes::getTiledImage(unsigned int id, int& iw, int& ih, bool flip, int xtiles)
{
  //Note: update() must be called first to fill slices[]
  if (slices.size() == 0) return NULL;
  for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw->id])
  {
    if (geom[i]->draw->id == id && drawable(i))
    {
      int width = geom[i]->width;
      int height = geom[i]->colourData()->size() / width;
      iw = width * xtiles;
      ih = ceil(slices[id] / (float)xtiles) * height;
      if (ih == height) iw = width * slices[id];
      printf("Exporting Image: %d width %d height %d depth %d --> %d x %d\n", id, width, height, slices[id], iw, ih);
      GLubyte *image = new GLubyte[iw * ih];
      memset(image, 0, iw*ih*sizeof(GLubyte));
      int xoffset = 0, yoffset = 0;
      for (unsigned int j=i; j<i+slices[id]; j++)
      {
        //printf("%d %d < %d\n", i, j, i+slices[id]);
        //printf("SLICE %d OFFSETS %d,%d\n", j, xoffset, yoffset);
        float min = geom[j]->colourData()->minimum;
        float range = geom[j]->colourData()->maximum - min;
        for (int y=0; y<height; y++)
        {
          for (int x=0; x<width; x++)
          {
            float val = geom[j]->colourData(y * width + x);
            if (flip)
              image[iw * (ih - (y + yoffset) - 1) + x + xoffset] = (val - min) / range * 255;
            else
              image[iw * (y + yoffset) + x + xoffset] = (val - min) / range * 255;
          }
        }

        xoffset += width;
        if (xoffset > iw-width)
        {
          xoffset = 0;
          yoffset += height;
        }
      }
      //Will only return one volume per id
      return image;
    }
  }
  return NULL;
}

void Volumes::pngWrite(unsigned int id, int xtiles)
{
#ifdef HAVE_LIBPNG
  for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw->id])
  {
    if (geom[i]->draw->id == id && drawable(i))
    {
      int iw, ih;
      GLubyte *image = getTiledImage(id, iw, ih, true, xtiles);
      if (!image) return;
      char path[256];
      sprintf(path, "%s.png", geom[i]->draw->name.c_str());
      std::ofstream file(path, std::ios::binary);
      write_png(file, 1, iw, ih, image);
      delete[] image;
      return; //Only one volume per id
    }
  }
#endif
}

void Volumes::jsonWrite(unsigned int id, json::Object& obj)
{
  update();  //Count slices etc...
  //Note: update() must be called first to fill slices[]
  if (slices.size() == 0) return;

  json::Array volumes;
  if (obj.HasKey("volumes")) volumes = obj["volumes"].ToArray();
  for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw->id])
  {
    //   printf("%d id == %d\n", i, geom[i]->draw->id);
    if (geom[i]->draw->id == id && drawable(i))
    {
      json::Object data, vertices, volume;
      //Height needs calculating from values data
      int height = geom[i]->colourData()->size() / geom[i]->width;
      /* This is for exporting the floating point volume data cube, may use in future when WebGL supports 3D textures...
      printf("Exporting: %d width %d height %d depth %d\n", id, geom[i]->width, height, slices[id]);
      int sliceSize = geom[i]->width * height;
      float* volume = new float[sliceSize * slices[id]];
      size_t offset = 0;
      for (int j=i; j<i+slices[id]; j++)
      {
         size_t size = sliceSize * sizeof(float);
         memcpy(volume + offset, geom[j]->colourData()->ref(), size);
         offset += sliceSize;
      }*/

      //Get a tiled image for WebGL to use as a 2D texture...
      int iw, ih;
      GLubyte *image = getTiledImage(id, iw, ih, false, 16); //16 * 256 = 4096^2 square texture
      if (!image) continue;
      json::Array res, scale;
      res.push_back((int)geom[i]->width);
      res.push_back(height);
      res.push_back(slices[id]);
      //Scaling factors
      scale.push_back(geom[i]->vertices[1][0] - geom[i]->vertices[0][0]);
      scale.push_back(geom[i]->vertices[1][1] - geom[i]->vertices[0][1]);
      scale.push_back(geom[i]->vertices[1][2] - geom[i]->vertices[0][2]);
      data["res"] = res;
      data["scale"] = scale;

      vertices["size"] = 3;
      vertices["count"] = (int)geom[i]->vertices.size();
      vertices["data"] = base64_encode(reinterpret_cast<const unsigned char*>(&geom[i]->vertices.value[0]), geom[i]->vertices.size() * sizeof(float));
      data["vertices"] = vertices;

      volume["size"] = 1;
      //volume["count"] = ;
      volume["data"] = base64_encode(reinterpret_cast<const unsigned char*>(image), iw * ih * sizeof(GLubyte));
      //volume["data"] = base64_encode(reinterpret_cast<const unsigned char*>(volume), sliceSize * slices[id] * sizeof(float)); //For 3D export
      data["volume"] = volume;

      delete[] image;
      //pngWrite(id);

      volumes.push_back(data);
    }
  }

  if (volumes.size() > 0) obj["volumes"] = volumes;
}
