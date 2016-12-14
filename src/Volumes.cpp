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

Volumes::Volumes(DrawState& drawstate) : Geometry(drawstate)
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
  if (drawstate.global("cachevolumes")) return;
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
        printf("Reloading: cube in object %s\n", draw->name().c_str());
      else
        printf("Reloading: %d slices in object %s\n", count, draw->name().c_str());
      count = 0;
      if (i<geom.size()) draw = geom[i]->draw;
    }
    count++;
  }

  for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw])
  {
    if (!drawable(i)) continue;

    //Single volume cube
    if (geom[i]->depth > 1)
    {
      DrawingObject* current = geom[i]->draw;
      bool texcompress = current->properties["compresstextures"];
      if (!geom[i]->texture || geom[i]->texture->texture->width == 0) //Width set to 0 to flag reload
      {
        //Determine type of data then load the texture
        if (!geom[i]->texture) geom[i]->texture = new ImageLoader(); //Add a new texture container
        unsigned int bpv = 4;
        if (geom[i]->colours.size() > 0)
        {
          int type = texcompress ? VOLUME_RGBA_COMPRESSED : VOLUME_RGBA;
          geom[i]->texture->load3D(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->colours.ref(), type);
        }
        else if (geom[i]->luminance.size() > 0)
        {
          bpv = 1;
          int type = texcompress ? VOLUME_BYTE_COMPRESSED : VOLUME_BYTE;
          assert(geom[i]->luminance.size() == geom[i]->width * geom[i]->height * geom[i]->depth);
          geom[i]->texture->load3D(geom[i]->width, geom[i]->height, geom[i]->depth, geom[i]->luminance.ref(), type);
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
    if (!geom[i]->texture || geom[i]->texture->texture->width == 0) //Width set to 0 to flag reload
    {
      if (!geom[i]->height)
      {
        //No height? Calculate from values data
        if (geom[i]->colourData())
          //(assumes float luminance data (4 bpv))
          geom[i]->height = geom[i]->colourData()->size() / geom[i]->width;
        else if (geom[i]->colours.size() > 0)
          geom[i]->height = geom[i]->colours.size() / geom[i]->width;
        else if (geom[i]->luminance.size() > 0)
          geom[i]->height = geom[i]->luminance.size() / geom[i]->width;
        else if (geom[i]->rgb.size() > 0)
          geom[i]->height = geom[i]->rgb.size() / geom[i]->width / 3;
      }

      //Texture crop?
      json texsize = current->properties["texturesize"];
      json texoffset = current->properties["textureoffset"];
      unsigned int dims[3] = {geom[i]->width, geom[i]->height, (unsigned int)slices[current]};
      bool crop = false;
      for (int d=0; d<3; d++)
      {
        if ((int)texsize[d] > 0 && (int)texsize[d] < dims[d]) 
        {
          dims[d] = texsize[d];
          crop = true;
        }
        if ((int)texoffset[d] > 0) crop = true;
        //Check within tex limits
        assert(dims[d] <= (unsigned)maxtex);
      }
      if (crop)
        printf("Cropping volume %d x %d ==> %d x %d @ %d,%d\n", geom[i]->width, geom[i]->height, dims[0], dims[1], (int)texoffset[0], (int)texoffset[1]);

      //Init/allocate/bind texture
      if (!geom[i]->texture) geom[i]->texture = new ImageLoader(); //Add a new texture container
      unsigned int bpv = 4;
      int type = 0;
      GL_Error_Check;
      if (geom[i]->colours.size() > 0)
      {
        //RGBA colours
        type = texcompress ? VOLUME_RGBA_COMPRESSED : VOLUME_RGBA;
        geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->colours.ref(), geom[i]->width, geom[i]->height, 4, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->colours.ref());
        }
      }
      else if (geom[i]->rgb.size() > 0)
      {
        //Byte RGB
        bpv = 3;
        type = texcompress ? VOLUME_RGB_COMPRESSED : VOLUME_RGB;
        assert(geom[i]->rgb.size() == 3*geom[i]->width * geom[i]->height);
        geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->rgb.ref(), geom[i]->width, geom[i]->height, 3, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->rgb.ref());
        }
      }
      else if (geom[i]->luminance.size() > 0)
      {
        //Byte luminance
        bpv = 1;
        type = texcompress ? VOLUME_BYTE_COMPRESSED : VOLUME_BYTE;
        assert(geom[i]->luminance.size() == geom[i]->width * geom[i]->height);
        geom[i]->texture->load3D(dims[0], dims[1], dims[2], NULL, type);
        for (unsigned int j=i; j<i+slices[current]; j++)
        {
          if (crop) 
          {
            GLubyte* ptr = RawImageCrop(geom[j]->luminance.ref(), geom[i]->width, geom[i]->height, 1, dims[0], dims[1], texoffset[0], texoffset[1]);
            geom[i]->texture->load3Dslice(j-i, ptr);
            delete ptr;
          }
          else
            geom[i]->texture->load3Dslice(j-i, geom[j]->luminance.ref());
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
  float dims[3] = {geom[i]->vertices[1][0] - geom[i]->vertices[0][0],
                   geom[i]->vertices[1][1] - geom[i]->vertices[0][1],
                   geom[i]->vertices[1][2] - geom[i]->vertices[0][2]
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
    fprintf(stderr, "No volume texture loaded for %d!\n", i);
    return;
  }
  float res[3] = {(float)voltexture->width, (float)voltexture->height, (float)voltexture->depth};
  glUniform3fv(prog->uniforms["uResolution"], 1, res);
  glUniform4fv(prog->uniforms["uViewport"], 1, viewport);

  //User settings
  int cmapid = props["colourmap"];
  ColourMap* cmap = geom[i]->draw->colourMap;
  //if (cmap) cmap->calibrate(0, 1);
  //Setup gradient texture from colourmap if not yet loaded
  if (cmap && !cmap->texture) cmap->loadTexture();
  bool hasColourMap = cmap && cmapid >= 0;
  //Use per-object clip box if set, otherwise use global clip
  /*
  float bbMin[3] = {props.getFloat("xmin", 0.01) * geom[i]->vertices[0][0]),
                    props.getFloat("ymin", 0.01) * geom[i]->vertices[0][1]),
                    props.getFloat("zmin", 0.01) * geom[i]->vertices[0][2])};
  float bbMax[3] = {props.getFloat("xmax", 0.99) * geom[i]->vertices[1][0]),
                    props.getFloat("ymax", 0.99) * geom[i]->vertices[1][1]),
                    props.getFloat("zmax", 0.99) * geom[i]->vertices[1][2])};
  */
  float bbMin[3] = {props.getFloat("xmin", 0.01),
                    props.getFloat("ymin", 0.01),
                    props.getFloat("zmin", 0.01)
                   };
  float bbMax[3] = {props.getFloat("xmax", 0.99),
                    props.getFloat("ymax", 0.99),
                    props.getFloat("zmax", 0.99)
                   };

  glUniform3fv(prog->uniforms["uBBMin"], 1, bbMin);
  glUniform3fv(prog->uniforms["uBBMax"], 1, bbMax);
  glUniform1i(prog->uniforms["uEnableColour"], hasColourMap ? 1 : 0);
  glUniform1f(prog->uniforms["uPower"], props["power"]);
  glUniform1i(prog->uniforms["uSamples"], props["samples"]);
  float opacity = props["opacity"], density = props["density"];
  glUniform1f(prog->uniforms["uDensityFactor"], density * opacity);
  Colour colour = geom[i]->draw->properties.getColour("colour", 220, 220, 200, 255);
  float isoalpha = props["isoalpha"];
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
  float isoval = props["isovalue"];
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
  if (hasColourMap)
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
  view->apply(false);
  //Object rotation/translation
  if (geom[i]->draw->properties.has("translate"))
  {
    float trans[3];
    Properties::toFloatArray(geom[i]->draw->properties["translate"], trans, 3);
    glTranslatef(trans[0], trans[1], trans[2]);
  }

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
  //Blending for premultiplied alpha
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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

  glEnable(GL_MULTISAMPLE);
  glPopAttrib();
  GL_Error_Check;
  glActiveTexture(GL_TEXTURE0);
  //Restore default blending mode
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  //Calibrate colourmap on data now so if colour bar drawn it will have correct range
  geom[i]->colourCalibrate();
}

GLubyte* Volumes::getTiledImage(DrawingObject* draw, unsigned int index, int& iw, int& ih, int& channels, int xtiles)
{
  GLubyte *image = NULL;
  unsigned int inc = slices[draw];
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    if (geom.size() == 1) //Single volume cube
    {
      int bpv;
      float min = 0.f, range = 0.f;
      if (geom[i]->colours.size() > 0)
      {
        //RGB/RGBA
        bpv = (4 * geom[i]->colours.size()) / (float)(geom[i]->width * geom[i]->height * geom[i]->depth);
      }
      else if (geom[i]->colourData())
      {
        //LUM BYTE/FLOAT
        bpv = (4 * geom[i]->colourData()->size()) / (float)(geom[i]->width * geom[i]->height * geom[i]->depth);
        min = geom[i]->colourData()->minimum;
        range = geom[i]->colourData()->maximum - min;
      }
      iw = geom[i]->width * xtiles;
      ih = ceil(geom[i]->depth / (float)xtiles) * geom[i]->height;
      if (ih == geom[i]->height) iw = geom[i]->width * geom[i]->depth;
      int size = geom[i]->width * geom[i]->height;
      printf("Exporting Image: %s width %d height %d depth %d --> %d x %d\n", draw->name().c_str(), geom[i]->width, geom[i]->height, geom[i]->depth, iw, ih);
      channels = bpv;
      image = new GLubyte[iw * ih * channels];
      memset(image, 0, iw*ih*sizeof(GLubyte));
      int xoffset = 0, yoffset = 0;
      for (unsigned int z=0; z<geom[i]->depth; z++)
      {
        for (int y=0; y<geom[i]->height; y++)
        {
          for (int x=0; x<geom[i]->width; x += 4/bpv)
          {
            /*if (bpv == 1) //Byte, DEPRECATED, now stored in own type as uchar
            {
              Colour c;
              c.fvalue = geom[i]->colourData(((z * size) + y * geom[i]->width + x)/4);
              for (int p=0; p<4; p++)
                image[iw * (y + yoffset) + x + xoffset + p] = ((c.rgba[p]/255.0) - min) / range * 255;
            }
            else*/ if (bpv == 4) //Float
            {
              float val = geom[i]->colourData((z * size) + y * geom[i]->width + x);
              image[iw * (y + yoffset) + x + xoffset] = (val - min) / range * 255;
            }
          }
        }

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
      int width = geom[i]->width;
      bool hasColourVals = geom[i]->colourData();
      int height = 0;
      if (hasColourVals)
      {
        height = geom[i]->colourData()->size() / width;
        channels = 1; //Luminance
      }
      else
      {
        height = geom[i]->colours.size() / width;
        channels = 4; //RGBA
      }
      iw = width * xtiles;
      ih = ceil(slices[draw] / (float)xtiles) * height;
      if (ih == height) iw = width * slices[draw];
      printf("Exporting Image: %s width %d height %d depth %d --> %d x %d\n", draw->name().c_str(), width, height, slices[draw], iw, ih);
      image = new GLubyte[iw * ih * channels];
      memset(image, 0, iw*ih*channels*sizeof(GLubyte));
      int xoffset = 0, yoffset = 0;
      for (unsigned int j=i; j<i+slices[draw]; j++)
      {
        //printf("%d %d < %d\n", i, j, i+slices[draw]);
        //printf("SLICE %d OFFSETS %d,%d\n", j, xoffset, yoffset);
        if (hasColourVals)
        {
          float min = geom[j]->colourData()->minimum;
          float range = geom[j]->colourData()->maximum - min;
          for (int y=0; y<height; y++)
          {
            for (int x=0; x<width; x++)
            {
              float val = geom[j]->colourData(y * width + x);
              image[iw * (y + yoffset) + x + xoffset] = (val - min) / range * 255;
            }
          }
        }
        else
        {
          Colour c;
          for (int y=0; y<height; y++)
          {
            for (int x=0; x<width; x++)
            {
              c.value = geom[j]->colours[y * width + x];
              image[(iw * (y + yoffset) + x + xoffset)*4] = c.r;
              image[(iw * (y + yoffset) + x + xoffset)*4+1] = c.g;
              image[(iw * (y + yoffset) + x + xoffset)*4+2] = c.b;
              image[(iw * (y + yoffset) + x + xoffset)*4+3] = c.a;
            }
          }
        }

        xoffset += width;
        if (xoffset > iw-width)
        {
          xoffset = 0;
          yoffset += height;
        }
      }
      break;
    }
  }
  return image;
}

void Volumes::saveImage(DrawingObject* draw, int xtiles)
{
  int count = 0;
  unsigned int inc = slices[draw];
  if (inc <= 0) inc = 1;
  for (unsigned int i = 0; i < geom.size(); i += inc)
  {
    if (geom[i]->draw == draw && drawable(i))
    {
      int iw, ih, channels;
      GLubyte *image = getTiledImage(draw, i, iw, ih, channels, xtiles);
      if (!image) return;
      char path[FILE_PATH_MAX];
      sprintf(path, "%s_%d", geom[i]->draw->name().c_str(), count++);
      writeImage(image, iw, ih, path, channels);
      delete[] image;
      break;  //Done
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
      GLubyte *image = getTiledImage(draw, i, iw, ih, channels, 16); //16 * 256 = 4096^2 square texture
      if (!image) continue;
      std::string imagestr = getImageString(image, iw, ih, channels);
      delete[] image;
      json res, scale;
      res.push_back((int)geom[i]->width);
      res.push_back(height);
      if (slices[draw])
        res.push_back(slices[draw]);
      else
        res.push_back(geom[i]->depth);
      //Scaling factors
      scale.push_back(geom[i]->vertices[1][0] - geom[i]->vertices[0][0]);
      scale.push_back(geom[i]->vertices[1][1] - geom[i]->vertices[0][1]);
      scale.push_back(geom[i]->vertices[1][2] - geom[i]->vertices[0][2]);
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
