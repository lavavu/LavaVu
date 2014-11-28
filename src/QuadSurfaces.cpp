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

Shader* QuadSurfaces::prog = NULL;

QuadSurfaces::QuadSurfaces(bool hidden) : Geometry(hidden)
{
   type = lucGridType;
   wireframe = false;
   cullface = false;
   triangles = true;
}

QuadSurfaces::~QuadSurfaces()
{
}

void QuadSurfaces::draw()
{
   //Re-render if view has rotated
   if (view->sort && geom.size() > 1) redraw = true;

   //Use shaders if available
   if (prog) prog->use();

   //Draw, calls display list when available
   Geometry::draw();
}

void QuadSurfaces::update()
{
   clock_t t1,t2,tt;
   t1 = tt = clock();
   // Update and depth sort surfaces..
   //Calculate distances from view plane
   float modelView[16];
   float maxdist = -1000000000, mindist = 1000000000; 

   Geometry::update();

   debug_print("Reloading and sorting %d quad surfaces...\n", geom.size());
   std::vector<Distance> surf_sort;
   glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      //Get corners of strip
      float* posmin = geom[i]->vertices[0];
      float* posmax = geom[i]->vertices[geom[i]->count - 1];
      float pos[3] = {posmin[0] + (posmax[0] - posmin[0]) * 0.5, 
                      posmin[1] + (posmax[1] - posmin[1]) * 0.5, 
                      posmin[2] + (posmax[2] - posmin[2]) * 0.5};

      //Calculate distance from viewing plane
      geom[i]->distance = eyeDistance(modelView, pos); 
      if (geom[i]->distance < mindist) mindist = geom[i]->distance;
      if (geom[i]->distance > maxdist) maxdist = geom[i]->distance;
      //printf("%d)  %f %f %f distance = %f\n", i, pos[0], pos[1], pos[2], geom[i]->distance);
      surf_sort.push_back(Distance(i, geom[i]->distance));
   }
   t2 = clock(); debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

   //Sort
   std::sort(surf_sort.begin(), surf_sort.end());
   t2 = clock(); debug_print("  %.4lf seconds to sort\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

   // Render into display list (using multiple lists won't work with sorting so just use the first)
   glNewList(displaylists[0], GL_COMPILE);
   //Render in reverse sorted order
   for (int i=geom.size()-1; i>=0; i--)
   {
      int id = surf_sort[i].id;
      if (!drawable(id)) continue;
      //int id = i; //Sorting disabled
      render(id);
      //printf("%d) rendered, distance = %f (%f)\n", id, geom[id]->distance, surf_sort[i].distance);
   }
   glEndList();

   t2 = clock(); debug_print("  Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
}

void QuadSurfaces::render(int i)
{
   //Calculate lighting normals to surface if not yet done
   bool vertNormals = false;
   if (geom[i]->normals.size() == 0) calcNormals(i);
   if (geom[i]->normals.size() == geom[i]->vertices.size()) vertNormals = true;

   //Calibrate colour maps on range for this surface
   geom[i]->colourCalibrate();

   //Set draw state
   setState(i, prog);

   //Single normal
   if (!vertNormals)
      glNormal3fv(geom[i]->normals[0]);

   // Shift value for adjusting fields drawn at min & max of mesh
   //double shift[3];
   //for (int d=0; d<3; d++) shift[d] = (view->max[d] - view->min[d]) / 5000.0;
   int offset0 = 0, offset1 = 0;
   for (int j=0; j < geom[i]->height-1; j++)  //Iterate over geom[i]->height-1 strips
   {
      //Use quad strip if drawing a wireframe to show the grid lines correctly
      if (!triangles || wireframe || geom[i]->draw->wireframe)
         glBegin(GL_QUAD_STRIP);
      else                    //Otherise: Triangle strips are faster & smoother looking
         glBegin(GL_TRIANGLE_STRIP);

      for (int k=0; k < geom[i]->width; k++)  //Iterate width of strips, geom[i]->width vertices
      {
         ///Colour colour;
         offset0 = j * geom[i]->width + k;
         offset1 = (j+1) * geom[i]->width + k;
         //printf("offset0 %d offset1 %d vertex0 %f,%f,%f vertex1 %f,%f,%f\n", offset0, offset1, geom[i]->vertices[offset0][0], geom[i]->vertices[offset0][1], geom[i]->vertices[offset0][2], geom[i]->vertices[offset1][0], geom[i]->vertices[offset1][1], geom[i]->vertices[offset1][2]);
         
               // Push edges out a little if drawing on border to prevent 
               // position clashes with other objects */
               //if (vertices[i][j][d] == view->min[d])
               //   vertices[i][j][d] -= shift[d];
               //if (vertices[i][j][d] == view->max[d])
               //   vertices[i][j][d] += shift[d];

         // Plot vertex 0
         geom[i]->setColour(offset0);
         if (vertNormals && !geom[i]->draw->wireframe && !wireframe)
            glNormal3fv(geom[i]->normals[offset0]);

         if (geom[i]->texCoords.size())
            glTexCoord2fv(geom[i]->texCoords[offset0]);
         else
            glTexCoord2f(k/(float)geom[i]->width, j/(float)geom[i]->height);

         glVertex3fv(geom[i]->vertices[offset0]);
            //Geometry::checkPointMinMax(geom[i]->vertices[offset0]);

         // Plot vertex 1
         geom[i]->setColour(offset1);
         if (vertNormals && !geom[i]->draw->wireframe && !wireframe)
            glNormal3fv(geom[i]->normals[offset1]);

         if (geom[i]->texCoords.size())
            glTexCoord2fv(geom[i]->texCoords[offset1]);
         else
            glTexCoord2f(k/(float)geom[i]->width, (j+1)/(float)geom[i]->height);

         glVertex3fv(geom[i]->vertices[offset1]);
            //Geometry::checkPointMinMax(geom[i]->vertices[offset1]);
      }
      glEnd();
      //printf("----End strip\n");
   }

   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   glDisable(GL_CULL_FACE);
   glDisable(GL_TEXTURE_2D);
}

void QuadSurfaces::calcNormals(int i)
{
   //Normals: calculate from surface geometry
   clock_t t1,t2;
   t1=clock();
   debug_print("Calculating normals for quad surface %d... ", i);

   // Calc pre-vertex normals for irregular meshes by averaging four surrounding triangle facet normals
   geom[i]->normals.resize(geom[i]->width * geom[i]->height * 3);
   for (int j = 0 ; j < geom[i]->height; j++ )
   {
      for (int k = 0 ; k < geom[i]->width; k++ )
      {
         Vec3d normal;
         // Get sum of normal vectors
         if (j > 0)
         {
            if (k > 0)
            {
               // Look back
               normal += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                          geom[i]->vertices[geom[i]->width * (j-1) + k], geom[i]->vertices[geom[i]->width * j + k-1]);
            }

            if (k < geom[i]->width - 1)
            {
               // Look back in x, forward in y
               normal += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                              geom[i]->vertices[geom[i]->width * j + k+1], geom[i]->vertices[geom[i]->width * (j-1) + k]);
            }
         }

         if (j <  geom[i]->height - 1)
         {
            if (k > 0)
            {
               // Look forward in x, back in y
               normal += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                              geom[i]->vertices[geom[i]->width * j + k-1], geom[i]->vertices[geom[i]->width * (j+1) + k]);
            }

            if (k < geom[i]->width - 1)
            {
               // Look forward
               normal += vectorNormalToPlane(geom[i]->vertices[geom[i]->width * j + k], 
                              geom[i]->vertices[geom[i]->width * (j+1) + k], geom[i]->vertices[geom[i]->width * j + k+1]);
            }
         }

         //Normalise to average
         normal.normalise();
         //Copy directly into normal block
         memcpy(geom[i]->normals[j * geom[i]->width + k], normal.ref(), sizeof(float) * 3);
      }
   }
   //Flag generated data
   geom[i]->normals.generated = true;
   t2 = clock(); debug_print("  %.4lf seconds\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();
}


void QuadSurfaces::jsonWrite(unsigned int id, std::ostream* osp)
{
   std::ostream& os = *osp;
   bool first = true;
   
   for (unsigned int i = 0; i < geom.size(); i++) 
   {
      if (geom[i]->draw->id == id && drawable(i))
      {
         bool vertNormals = false;
         if (geom[i]->normals.size() == 0) calcNormals(i);
         if (geom[i]->normals.size() == geom[i]->vertices.size()) vertNormals = true;

         std::cerr << "Collected " << geom[i]->count << " vertices/values (" << i << ")" << std::endl;
         //Only supports dump of vertex, normal and colourValue at present
         if (!first) os << "," << std::endl;
         first = false;
         os << "        {" << std::endl;
         os << "          \"width\" : " << geom[i]->width << "," << std::endl;
         os << "          \"height\" : " << geom[i]->height << "," << std::endl;
         std::string vertices = base64_encode(reinterpret_cast<const unsigned char*>(&geom[i]->vertices.value[0]), geom[i]->vertices.size() * sizeof(float));
         os << "          \"vertices\" : \n          {" << std::endl;
         os << "            \"size\" : 3," << std::endl;
         os << "            \"data\" : \"" << vertices << "\"" << std::endl;
         os << "          }," << std::endl;
         std::string normals = base64_encode(reinterpret_cast<const unsigned char*>(&geom[i]->normals.value[0]), geom[i]->normals.size() * sizeof(float));
         os << "          \"normals\" : \n          {" << std::endl;
         os << "            \"size\" : 3," << std::endl;
         os << "            \"data\" : \"" << normals << "\"" << std::endl;
         
         if (geom[i]->colourValue.size() == geom[i]->count)
         {
            std::string values = base64_encode(reinterpret_cast<const unsigned char*>(&geom[i]->colourValue.value[0]), geom[i]->colourValue.size() * sizeof(float));
            geom[i]->colourCalibrate(); //Calibrate on data range
            os << "          }," << std::endl;
            os << "          \"values\" : \n          {" << std::endl;
            os << "            \"size\" : 1," << std::endl;
            os << "            \"minimum\" : " << geom[i]->colourValue.minimum << "," << std::endl;
            os << "            \"maximum\" : " << geom[i]->colourValue.maximum << "," << std::endl;
            os << "            \"data\" : \"" << values << "\"" << std::endl;
         }
         os << "          }" << std::endl;
         os << "        }";
      }
   }
}
