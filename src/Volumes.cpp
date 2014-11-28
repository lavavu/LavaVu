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

Volumes::Volumes(bool hidden) : Geometry(hidden)
{
   type = lucVolumeType;
}

Volumes::~Volumes()
{

}

void Volumes::close()
{
   Geometry::close();
}

void Volumes::draw()
{
   //Draw, calls display list when available
   Geometry::draw();

   //Use shaders if available
   if (prog) prog->use();

   //clock_t t1,t2,tt;
   //t1 = tt = clock();

   DrawingObject* current = NULL;
   for (int i=0; i<geom.size(); i += 1)
   {
      if (!drawable(i)) continue;

      if (current != geom[i]->draw)
      {
         current = geom[i]->draw;

         render(i);

         GL_Error_Check;
      }
   }

   //t2 = clock(); debug_print("  Draw %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void Volumes::update()
{
   clock_t t1,t2,tt;
   t1 = tt = clock();

   Geometry::update();

   //Count slices in each volume
   printf("Total slices: %d\n", geom.size());
   if (!geom.size()) return;
   slices.clear();
   int id = geom[0]->draw->id;
   int count = 0;
   for (int i=0; i<=geom.size(); i++)
   {
      //Force reload
      if (i<geom.size() && geom[i]->draw->texture)
        geom[i]->draw->texture->width = 0;
      if (i==geom.size() || id != geom[i]->draw->id)
      {
         slices[id] = count;
         printf("%d slices in object %d\n", count, id);
         count = 0;
         if (i<geom.size()) id = geom[i]->draw->id;
      }
      count++;
   }

   for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw->id]) 
   {
      if (!drawable(i)) continue;

      DrawingObject* current = geom[i]->draw;
      if (!current->texture || current->texture->width == 0)
      {
         //Create the texture
         if (!current->texture) current->texture = new TextureData();
         //Height needs calculating from values data
         int height = geom[i]->colourValue.size() / geom[i]->width;
         printf("current %d width %d height %d depth %d\n", current->id, geom[i]->width, height, slices[current->id]);
         int sliceSize = geom[i]->width * height;
         float* volume = new float[sliceSize * slices[current->id]];

         size_t offset = 0;
         for (int j=i; j<i+slices[current->id]; j++)
         {
            size_t size = sliceSize * sizeof(float);
            memcpy(volume + offset, &geom[j]->colourValue.value[0], size);
            offset += sliceSize;
         }
          
         glActiveTexture(GL_TEXTURE1);
         GL_Error_Check;
         glBindTexture(GL_TEXTURE_3D, current->texture->id);
         GL_Error_Check;

         current->texture->width = geom[i]->width;
         current->texture->height = height;
         current->texture->depth = slices[current->id];
       
         // set the texture parameters
         glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
         glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
         glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
         glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         GL_Error_Check;

         glTexImage3D(GL_TEXTURE_3D, 0, GL_INTENSITY, geom[i]->width, height, slices[current->id], 0, GL_LUMINANCE, GL_FLOAT, volume);
         GL_Error_Check;
         delete volume;

         //Setup gradient texture from colourmap
         if (geom[i]->draw->colourMaps[lucColourValueData])
           geom[i]->draw->colourMaps[lucColourValueData]->loadTexture();
      }
   }

   t2 = clock(); debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void Volumes::render(int i)
{
   float dims[3] = {geom[i]->vertices[1][0] - geom[i]->vertices[0][0],
                    geom[i]->vertices[1][1] - geom[i]->vertices[0][1],
                    geom[i]->vertices[1][2] - geom[i]->vertices[0][2]};
   float pos[3] =  {geom[i]->vertices[0][0] + 0.5 * dims[0],
                    geom[i]->vertices[0][1] + 0.5 * dims[1],
                    geom[i]->vertices[0][2] + 0.5 * dims[2]};

   assert(prog);
   GL_Error_Check;
 
    //Uniform variables
    //TODO: Provide interface to set these parameters
    float bbMin[3] = {0.01,0.01,0.01};
    //float bbMax[3] = {dims[0], dims[1], dims[2]};
    float bbMax[3] = {0.99, 0.99, 0.99};
    float res[3] = {geom[i]->draw->texture->width, geom[i]->draw->texture->height, geom[i]->draw->texture->depth};
    float focalLength = 1.0 / tan(0.5 * view->fov * M_PI/180);
    float wdims[4] = {view->width, view->height, view->xpos, view->ypos};
    wdims[2] += view->eye_shift;
    //printf("View shift: %f\n", wdims[2]);
    float isocolour[4] = {0.922,0.886,0.823,1.0};
    //printf("PLOTTING %d width %d height %d depth\n", geom[i]->draw->texture->width, geom[i]->draw->texture->height, geom[i]->draw->texture->depth);
    //printf("dims %f,%f,%f pos %f,%f,%f res %f,%f,%f\n", dims[0], dims[1], dims[2], pos[0], pos[1], pos[2], res[0], res[1], res[2]);
    glUniform3fv(prog->uniforms["uBBMin"], 1, bbMin);
    glUniform3fv(prog->uniforms["uBBMax"], 1, bbMax);
    glUniform3fv(prog->uniforms["uResolution"], 1, res);
    glUniform1i(prog->uniforms["uEnableColour"], 1);
    glUniform1f(prog->uniforms["uBrightness"], 0);
    glUniform1f(prog->uniforms["uContrast"], 1);
    glUniform1f(prog->uniforms["uPower"], 1);
    glUniform1f(prog->uniforms["uFocalLength"], focalLength);
    glUniform4fv(prog->uniforms["uWindowSize"], 1, wdims);
    glUniform1i(prog->uniforms["uSamples"], 256);
    glUniform1f(prog->uniforms["uDensityFactor"], 5.0);
    glUniform1f(prog->uniforms["uIsoValue"], 0.0);
    glUniform4fv(prog->uniforms["uIsoColour"], 1, isocolour);
    glUniform1f(prog->uniforms["uIsoSmooth"], 0.1);
    glUniform1i(prog->uniforms["uIsoWalls"], 0);
    glUniform1i(prog->uniforms["uFilter"], 0);
   GL_Error_Check;
   
   //Field data requires normalisation to [0,1]
   //Pass minimum,maximum in place of colourmap calibrate
   float range[2] = {0.0, 1.0};
   if (geom[i]->colourValue.size() > 0)
   {
      range[0] = geom[i]->colourValue.minimum;
      range[1] = geom[i]->colourValue.maximum;
   }
   glUniform2fv(prog->uniforms["uRange"], 1, range);
   GL_Error_Check;
 
   //Gradient texture
   glActiveTexture(GL_TEXTURE0);
   glUniform1i(prog->uniforms["uTransferFunction"], 0);
   if (geom[i]->draw->colourMaps[lucColourValueData])
      glBindTexture(GL_TEXTURE_2D, geom[i]->draw->colourMaps[lucColourValueData]->texture->id);
 
   //Volume texture
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_3D, geom[i]->draw->texture->id);
   glUniform1i(prog->uniforms["uVolume"], 1);
   GL_Error_Check;

   //Get the matrices to send as uniform data
   float mvMatrix[16];
   float nMatrix[16];
   float pMatrix[16];
   glGetFloatv(GL_MODELVIEW_MATRIX, nMatrix);
   //Apply scaling to fit bounding box
   glPushMatrix();
   glScalef(1.0/dims[0], 1.0/dims[1], 1.0/dims[2]);
   glGetFloatv(GL_MODELVIEW_MATRIX, mvMatrix);
   glPopMatrix();
   glGetFloatv(GL_PROJECTION_MATRIX, pMatrix);
   GL_Error_Check;

   //Projection and modelview matrices
   glUniformMatrix4fv(prog->uniforms["uPMatrix"], 1, GL_FALSE, pMatrix);
   glUniformMatrix4fv(prog->uniforms["uMVMatrix"], 1, GL_FALSE, mvMatrix);
   //printMatrix(mvMatrix);
   nMatrix[12] = nMatrix[13] = nMatrix[14] = 0; //Removing translation works as long as no non-uniform scaling
   glUniformMatrix4fv(prog->uniforms["uNMatrix"], 1, GL_FALSE, nMatrix);
   GL_Error_Check;
 
   //State...
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_DEPTH_TEST);  //No depth testing to allow multi-pass blend!
   glDisable(GL_CULL_FACE);
   //glDisable(GL_MULTISAMPLE);
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
 
   GL_Error_Check;

   drawCuboid(pos, dims[0], dims[1], dims[2], true, 1);
   GL_Error_Check;

   //glDisable(GL_TEXTURE_2D);
   glEnable(GL_DEPTH_TEST);
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
         int height = geom[i]->colourValue.size() / width;
         iw = width * xtiles;
         ih = ceil(slices[id] / (float)xtiles) * height;
         if (ih == height) iw = width * slices[id];
         printf("Exporting Image: %d width %d height %d depth %d --> %d x %d\n", id, width, height, slices[id], iw, ih);
         GLubyte *image = new GLubyte[iw * ih];
         memset(image, 0, iw*ih*sizeof(GLubyte));
         int xoffset = 0, yoffset = 0;
         for (int j=i; j<i+slices[id]; j++)
         {
            //printf("%d %d < %d\n", i, j, i+slices[id]);
            //printf("SLICE %d OFFSETS %d,%d\n", j, xoffset, yoffset);
            float min = geom[j]->colourValue.minimum;
            float range = geom[j]->colourValue.maximum - min;
            for (int y=0; y<height; y++)
            {
               for (int x=0; x<width; x++)
               {
                  if (flip)
                     image[iw * (ih - (y + yoffset) - 1) + x + xoffset] = (geom[j]->colourValue[y * width + x] - min) / range * 255;
                  else
                     image[iw * (y + yoffset) + x + xoffset] = (geom[j]->colourValue[y * width + x] - min) / range * 255;
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

void Volumes::jsonWrite(unsigned int id, std::ostream* osp)
{
   update();  //Count slices etc...
   //Note: update() must be called first to fill slices[]
   if (slices.size() == 0) return;

   std::ostream& os = *osp;
   
   for (unsigned int i = 0; i < geom.size(); i += slices[geom[i]->draw->id]) 
   {
         printf("%d id == %d\n", i, geom[i]->draw->id);
      if (geom[i]->draw->id == id && drawable(i))
      {
         //Height needs calculating from values data
         int height = geom[i]->colourValue.size() / geom[i]->width;
         /* This is for exporting the floating point volume data cube, may use in future when WebGL supports 3D textures...
         printf("Exporting: %d width %d height %d depth %d\n", id, geom[i]->width, height, slices[id]);
         int sliceSize = geom[i]->width * height;
         float* volume = new float[sliceSize * slices[id]];
         size_t offset = 0;
         for (int j=i; j<i+slices[id]; j++)
         {
            size_t size = sliceSize * sizeof(float);
            memcpy(volume + offset, &geom[j]->colourValue.value[0], size);
            offset += sliceSize;
         }*/

         //Get a tiled image for WebGL to use as a 2D texture...
         int iw, ih;
         GLubyte *image = getTiledImage(id, iw, ih, false, 16); //16 * 256 = 4096^2 square texture
         if (!image) return;
         //For scaling factors
         float dims[3] = {geom[i]->vertices[1][0] - geom[i]->vertices[0][0],
                          geom[i]->vertices[1][1] - geom[i]->vertices[0][1],
                          geom[i]->vertices[1][2] - geom[i]->vertices[0][2]};
          
         os << "        {" << std::endl;
         os << "          \"res\" : \n          [" << std::endl;
         os << "            " << geom[i]->width << "," << std::endl;
         os << "            " << height << "," << std::endl;
         os << "            " << slices[id] << std::endl;
         os << "          ]," << std::endl;
         os << "          \"scale\" : \n          [" << std::endl;
         os << "            " << dims[0] << "," << std::endl;
         os << "            " << dims[1] << "," << std::endl;
         os << "            " << dims[2] << std::endl;
         os << "          ]," << std::endl;
         std::string vertices = base64_encode(reinterpret_cast<const unsigned char*>(&geom[i]->vertices.value[0]), geom[i]->vertices.size() * sizeof(float));
         os << "          \"vertices\" : \n          {" << std::endl;
         os << "            \"size\" : 3," << std::endl;
         os << "            \"data\" : \"" << vertices << "\"" << std::endl;
         os << "          }," << std::endl;
         //std::string vol = base64_encode(reinterpret_cast<const unsigned char*>(volume), sliceSize * slices[id] * sizeof(float)); //For 3D export
         std::string vol = base64_encode(reinterpret_cast<const unsigned char*>(image), iw * ih * sizeof(GLubyte));
         os << "          \"volume\" : \n          {" << std::endl;
         os << "            \"size\" : 1," << std::endl;
         os << "            \"data\" : \"" << vol << "\"" << std::endl;
         os << "          }" << std::endl;
         os << "        }";

         delete[] image;
         //pngWrite(id);
         //delete volume;
         return; //Only one volume per id
      }
   }
}
