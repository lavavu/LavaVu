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

Shapes::Shapes() : TriSurfaces()
{
   type = lucShapeType;
}

Shapes::~Shapes()
{
}

void Shapes::drawCuboid(GeomData* geom, float pos[3], float width, float height, float depth, Quaternion& rot)
{
   float min[3] = {-0.5 * width, -0.5 * height, -0.5 * depth};
   float max[3] = {min[0] + width, min[1] + height, min[2] + depth};

   //Corner vertices
   Vec3d verts[8] = 
   {
      Vec3d(min[0], min[1], max[2]),
      Vec3d(max[0], min[1], max[2]),
      Vec3d(max[0], max[1], max[2]),
      Vec3d(min[0], max[1], max[2]),
      Vec3d(min[0], min[1], min[2]),
      Vec3d(max[0], min[1], min[2]),
      Vec3d(max[0], max[1], min[2]),
      Vec3d(min[0], max[1], min[2])
   };

   for (int i=0; i<8; i++)
   {
      /* Multiplying a quaternion q with a vector v applies the q-rotation to v */
      verts[i] = rot * verts[i];
      verts[i] += Vec3d(pos);
      checkPointMinMax(verts[i].ref());
   }

   //Triangle indices
   int indices[36] = {
				0+idx, 1+idx, 2+idx, 2+idx, 3+idx, 0+idx, 
				3+idx, 2+idx, 6+idx, 6+idx, 7+idx, 3+idx, 
				7+idx, 6+idx, 5+idx, 5+idx, 4+idx, 7+idx, 
				4+idx, 0+idx, 3+idx, 3+idx, 7+idx, 4+idx, 
				0+idx, 1+idx, 5+idx, 5+idx, 4+idx, 0+idx,
				1+idx, 5+idx, 6+idx, 6+idx, 2+idx, 1+idx 
			};

   read(geom, 8, lucVertexData, verts[0].ref());
   read(geom, 36, lucIndexData, indices);
}

// Create a 3d ellipsoid given centre point, 3 radii and number of triangle segments to use
// Based on algorithm and equations from:
// http://local.wasp.uwa.edu.au/~pbourke/texture_colour/texturemap/index.html
// http://paulbourke.net/geometry/sphere/
void Shapes::drawEllipsoid(GeomData* geom, Vec3d& centre, Vec3d& radii, int segment_count, Quaternion& rot)
{
   int i,j;
   Vec3d edge, pos;
   float tex[2];

   if (radii.x < 0) radii.x = -radii.x;
   if (radii.y < 0) radii.y = -radii.y;
   if (radii.z < 0) radii.z = -radii.z;
   if (segment_count < 0) segment_count = -segment_count;
   calcCircleCoords(segment_count);

   std::vector<int> indices;
   for (j=0; j<=segment_count/2; j++)
   {
      //Triangle strip vertices
      for (i=0; i<=segment_count; i++)
      {
         // Get index from pre-calculated coords which is back 1/4 circle from j+1 (same as forward 3/4circle)
         int cidx = ((int)(1 + j + 0.75 * segment_count) % segment_count);
         edge = Vec3d(y_coords_[cidx] * y_coords_[i], x_coords_[cidx], y_coords_[cidx] * x_coords_[i]);
         pos = rot * (centre + radii * edge);

         // Flip for normal
         edge = -edge;

         tex[0] = i/(float)segment_count;
         tex[1] = 2*(j+1)/(float)segment_count;
      
         //Read triangle vertex, normal, texcoord
         read(geom, 1, lucVertexData, pos.ref());
         read(geom, 1, lucNormalData, edge.ref());
         read(geom, 1, lucTexCoordData, tex);
         checkPointMinMax(pos.ref());

         // Get index from pre-calculated coords which is back 1/4 circle from j (same as forward 3/4circle)
         cidx = ((int)(j + 0.75 * segment_count) % segment_count);
         edge = Vec3d(y_coords_[cidx] * y_coords_[i], x_coords_[cidx], y_coords_[cidx] * x_coords_[i]);
         pos = rot * (centre + radii * edge);

         // Flip for normal
         edge = -edge;

         tex[0] = i/(float)segment_count;
         tex[1] = 2*j/(float)segment_count;

         //Read triangle vertex, normal, texcoord
         read(geom, 1, lucVertexData, pos.ref());
         read(geom, 1, lucNormalData, edge.ref());
         read(geom, 1, lucTexCoordData, tex);
         checkPointMinMax(pos.ref());

         //Triangle strip indices
         if (i > 0)
         {
            //First tri
            indices.push_back(idx);
            indices.push_back(idx+1);
            indices.push_back(idx+2);
            //Second tri
            indices.push_back(idx+1);
            indices.push_back(idx+3);
            indices.push_back(idx+2);
            idx += 2;
         }
      }
   }

   //Read the triangle indices
   read(geom, indices.size(), lucIndexData, &indices[0]);
}

void Shapes::update()
{
   //Convert shapes to triangles
   for (unsigned int i=0; i<geom.size(); i++) 
   {
      //Clear existing vertex related data
      geom[i]->count = 0;
      geom[i]->data[lucVertexData]->clear();
      geom[i]->data[lucIndexData]->clear();
      geom[i]->data[lucTexCoordData]->clear();
      geom[i]->data[lucNormalData]->clear();

      idx = 0; //Reset current index

      float scaling = geom[i]->draw->properties["scaling"].ToFloat(1.0);

      //Load constant scaling factors from properties
      float dims[3];
      dims[0] = geom[i]->draw->properties["width"].ToFloat(FLT_MIN);
      dims[1] = geom[i]->draw->properties["height"].ToFloat(FLT_MIN);
      dims[2] = geom[i]->draw->properties["length"].ToFloat(FLT_MIN);
      int shape = geom[i]->draw->properties["shape"].ToInt(0);
      int quality = geom[i]->draw->properties["glyphs"].ToInt(24);
      //Points drawn as shapes?
      if (!geom[i]->draw->properties.HasKey("shape"))
      {
         dims[0] = dims[1] = dims[2] = geom[i]->draw->properties["pointsize"].ToFloat(1.0) / 8.0;
         quality = 16;
      }

      if (scaling <= 0) scaling = 1.0;

      for (int v=0; v < geom[i]->positions.size()/3; v++) 
      {
         //Scale the dimensions by variables (dynamic range options? by setting max/min?)
         float sdims[3] = {dims[0], dims[1], dims[2]};
         if (geom[i]->xWidths.size() > 0) sdims[0] = geom[i]->xWidths[v];
         if (geom[i]->yHeights.size() > 0) sdims[1] = geom[i]->yHeights[v]; else sdims[1] = sdims[0];
         if (geom[i]->zLengths.size() > 0) sdims[2] = geom[i]->zLengths[v]; else sdims[2] = sdims[1];

         //Multiply by constant scaling factors if present
         for (int c=0; c<3; c++)
         {
            if (dims[c] != FLT_MIN) sdims[c] *= dims[c];
            //Apply scaling, also inverse of model scaling to avoid distorting glyphs
            sdims[c] *= scaling * scale / view->scale[c];
         }

         //Setup orientation using alignment vector
         Quaternion qrot;
         if (geom[i]->vectors.size() > 0)
         {
            Vec3d vec(geom[i]->vectors[v]);
            //vec *= Vec3d(view->scale); //Scale

            // Rotate to orient the shape
            //...Want to align our z-axis to point along arrow vector:
            // axis of rotation = (z x vec)
            // cosine of angle between vector and z-axis = (z . vec) / |z|.|vec| *
            vec.normalise();
            float rangle = RAD2DEG * vec.angle(Vec3d(0.0, 0.0, 1.0));
            //Axis of rotation = vec x [0,0,1] = -vec[1],vec[0],0
            Vec3d rvec = Vec3d(-vec.y, vec.x, 0);
            qrot.fromAxisAngle(rvec, rangle);
         }

         //Create shape
         Vec3d posv = Vec3d(geom[i]->positions[v]);
         Vec3d radii = Vec3d(sdims);
         if (shape == 1)
            drawCuboid(geom[i], geom[i]->positions[v], sdims[0], sdims[1], sdims[2], qrot);
         else
            drawEllipsoid(geom[i], posv, radii, quality, qrot);
         idx = geom[i]->count; //Reset current index hack for spheres
      }
      //printf("%d Shapes: %d Vertices: %d Indices: %d\n", i, geom[i]->positions.size()/3, geom[i]->count, geom[i]->indices.size());
   }

   elements = -1;
   TriSurfaces::update();
}

