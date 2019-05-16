/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
** Copyright (c) 2016, Monash University
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

#include "IsoSurface.h"

// Given a grid dataset and an isovalue, calculate the triangular
//  facets required to represent the isosurface through the data.
Isosurface::Isosurface(std::vector<Geom_Ptr>& geom, Triangles* tris, DrawingObject* draw, DrawingObject* target, Volumes* vol, unsigned int subsample)
  : subsample(subsample), surfaces(tris), target(target)
{
  //Generate an isosurface from a set of volume slices or a cube
  clock_t t1,t2,tt;
  tt = t1 = clock();
  if (geom.size() == 0) return;

  for (unsigned int i = 0; i < geom.size(); i += vol->slices[geom[i]->draw])
  {
    if (geom[i]->draw != draw || !geom[i]->width) continue;
    //Get isovalues array, or scalar, or isovalue property as fallback
    json isovalues = target->properties["isovalues"];
    isovalues = target->properties["isovalues"];
    if (isovalues.is_null() || !isovalues.size())
      isovalues = target->properties["isovalue"];
    if (!isovalues.size())
    {
      json arr;
      arr.push_back(isovalues);
      isovalues = arr;
    }
    if (!isovalues.size())
    {
      std::cerr << "Invalid isovalues property\n";
      continue;
    }

    DrawingObject* current = geom[i]->draw;
    if (vol->slices[current] == 1)
      debug_print("Extracting isosurface from: cube, volume: %s\n", draw->name().c_str());
    else
      debug_print("Extracting isosurface from: %d vol->slices, volume: %s\n", vol->slices[current], draw->name().c_str());

    geom[i]->colourCalibrate();
    if (!geom[i]->height)
    {
      //No height? Calculate from values data
      if (geom[i]->colourData())
        //(assumes float luminance data (4 bpv))
        geom[i]->height = geom[i]->colourData()->size() / geom[i]->width;
      else if (geom[i]->render->luminance.size() > 0)
        geom[i]->height = geom[i]->render->luminance.size() / geom[i]->width;
      else if (geom[i]->render->colours.size() > 0)
        geom[i]->height = geom[i]->render->colours.size() / geom[i]->width;
    }

    unsigned int depth = geom[i]->depth;
    if (vol->slices[current] > depth) depth = vol->slices[current];

    nx = geom[i]->width;
    ny = geom[i]->height;
    nz = depth;

    if (nx == 0 || ny == 0 || nz == 0)
      abort_program("Invalid volume dimensions %d %d %d\n", nx, ny, nz);

    //Apply subsampling and allocate grid vertex array
    nx /= subsample;
    ny /= subsample;
    nz /= subsample;

    vertex = new vertices(nx, ny, nz);

    //Corners
    Vec3d start = Vec3d(geom[i]->render->vertices[0]);
    Vec3d end = Vec3d(geom[i]->render->vertices[1]);
    Vec3d inc = end-start;
    inc[0] = inc[0] / (nx-1);
    inc[1] = inc[1] / (ny-1);
    inc[2] = inc[2] / (nz-1);

    //Save colour values reference
    colourVals = geom[i]->colourData();
    if (colourVals != NULL && colourVals->size() != (geom[i]->depth <= 1 ? nx*ny : nx*ny*nz))
      colourVals = NULL;

    // Sample in regular grid
    Colour c;
    unsigned int next = 0;
    for (unsigned int z = 0; z < nz; z++)
    //for (int x = 0; x < nx; x++)
    {
      //if (geom[i]->depth <= 1) next = 0; //Reset value index if sliced

      for (unsigned int y = 0; y < ny; y++)
      {

        for (unsigned int x = 0; x < nx; x++)
        //for (int z = 0; z < nz; z++)
        {
          vertex->at(x,y,z).pos[0] = start[0]+x*inc[0];
          vertex->at(x,y,z).pos[1] = start[1]+y*inc[1];
          vertex->at(x,y,z).pos[2] = start[2]+z*inc[2];

          int j = 0;
          //Loading slices? get slice index
          if (geom[i]->depth <= 1)
          {
            j = z * subsample;
            next = (y*nx + x) * subsample;
          }
          else
            next = ((z*ny + y)*nx + x) * subsample;

          //std::cout << x << "," << y << "," << z << " : " << next << std::endl;

          //Set the contour value
          if (geom[i]->render->luminance.size() > 0)
          {
            //Byte luminance
            assert(geom[i+j]->render->luminance.size() > next);
            vertex->at(x,y,z).value = geom[i+j]->render->luminance[next]/255.0;
          }
          else if (geom[i+j]->render->colours.size() > 0)
          {
            assert(geom[i+j]->render->colours.size() > next);
            //RGBA - just use red channel
            c.value = geom[i+j]->render->colours[next];
            vertex->at(x,y,z).value = c.r/255.0;
          }
          else if (geom[i+j]->values.size() > 0) //Use first values entry
          {
            assert(geom[i+j]->valueData(0)->size() > next);
            //Float 
            vertex->at(x,y,z).value = geom[i+j]->valueData(0, next);
          }

          //Set the colour value
          if (colourVals)
            vertex->at(x,y,z).colourval = geom[i+j]->colourData(next);
          else
            vertex->at(x,y,z).colourval = vertex->at(x,y,z).value;

          //vertex->at(x,y,z).colourval = (x*y*z)/(float)(nx*ny*nz); //vertex->at(x,y,z).pos[1] - start[1]; //geom[i+j]->colourData(next);
          //next += subsample;
        }
      }
    }

    debug_print(" %s width %d height %d depth %d, sampled %d %d %d\n", current->name().c_str(), geom[i]->width, geom[i]->height, depth, nx, ny, nz);
    t2 = clock(); debug_print("  Vertex load took %.4lf seconds.\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

    for (auto isoval : isovalues)
    {
      isovalue = isoval;

      //Create a new data store for output geometry
      surfaces->add(target);

      // Find Surface with Marching Cubes
      MarchingCubes();

      t2 = clock(); debug_print("  Surface extraction (%d triangles) took %.4lf seconds.\n", surfaces->getObjectStore(target)->count()/3, (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

      if (target->properties["isowalls"])
      {
        //Create a new data store for walls
        surfaces->add(target);
        DrawWalls();
        t2 = clock(); debug_print("  Surface wall extraction (%d triangles) took %.4lf seconds.\n", surfaces->getObjectStore(target)->count()/3, (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();
      }

      //Adjust bounding box
      surfaces->compareMinMax(geom[i]->min, geom[i]->max);

      t2 = clock();
      debug_print("Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);
    }

    delete vertex;
  }
}

/* This algorithm for constructing an isosurface is taken from:
Lorensen, William and Harvey E. Cline. Marching Cubes: A High Resolution 3D Surface Construction Algorithm. Computer Graphics (SIGGRAPH 87 Proceedings) 21(4) July 1987, p. 163-170) http://www.cs.duke.edu/education/courses/fall01/cps124/resources/p163-lorensen.pdf
The lookup table is taken from http://astronomy.swin.edu.au/~pbourke/modelling/polygonise/
*/
void Isosurface::MarchingCubes()
{
   int cubeindex;
   IVertex points[12];

   int edgeTable[256]=
   {
      0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
      0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
      0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
      0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
      0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
      0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
      0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
      0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
      0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
      0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
      0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
      0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
      0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
      0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
      0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
      0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
      0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
      0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
      0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
      0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
      0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
      0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
      0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
      0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
      0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
      0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
      0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
      0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
      0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
      0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
      0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
      0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
   };
   int triTable[256][16] =
   {{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
      {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
      {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
      {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
      {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
      {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
      {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
      {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
      {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
      {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
      {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
      {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
      {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
      {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
      {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
      {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
      {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
      {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
      {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
      {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
      {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
      {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
      {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
      {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
      {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
      {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
      {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
      {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
      {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
      {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
      {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
      {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
      {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
      {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
      {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
      {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
      {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
      {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
      {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
      {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
      {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
      {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
      {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
      {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
      {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
      {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
      {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
      {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
      {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
      {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
      {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
      {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
      {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
      {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
      {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
      {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
      {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
      {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
      {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
      {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
      {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
      {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
      {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
      {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
      {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
      {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
      {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
      {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
      {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
      {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
      {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
      {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
      {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
      {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
      {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
      {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
      {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
      {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
      {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
      {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
      {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
      {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
      {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
      {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
      {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
      {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
      {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
      {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
      {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
      {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
      {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
      {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
      {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
      {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
      {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
      {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
      {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
      {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
      {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
      {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
      {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
      {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
      {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
      {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
      {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
      {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
      {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
      {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
      {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
      {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
      {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
      {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
      {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
      {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
      {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
      {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
      {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
      {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
      {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
      {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
      {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
      {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
      {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
      {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
      {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
      {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
      {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
      {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
      {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
      {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
      {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
      {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
      {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
      {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
      {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
      {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
      {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
      {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
      {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
      {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
      {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
      {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
      {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
      {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
      {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
      {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
      {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
      {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
      {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
      {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
      {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
      {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
      {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
      {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
      {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
      {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
      {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
      {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
      {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
      {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
      {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
      {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
      {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
      {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
      {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
      {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
      {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
      {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
      {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
   };


   for (unsigned int i = 0 ; i < nx - 1  ; i++ )
   {
      for (unsigned int j = 0 ; j < ny - 1 ; j++ )
      {
         for (unsigned int k = 0 ; k < nz - 1 ; k++ )
         {
            /* Determine the index into the edge table which tells us which vertices are inside of the surface */
            cubeindex = 0;
            if (vertex->at(i,j,k).value       < isovalue) cubeindex |= 1;
            if (vertex->at(i+1,j,k).value     < isovalue) cubeindex |= 2;
            if (vertex->at(i+1,j,k+1).value   < isovalue) cubeindex |= 4;
            if (vertex->at(i,j,k+1).value     < isovalue) cubeindex |= 8;
            if (vertex->at(i,j+1,k).value     < isovalue) cubeindex |= 16;
            if (vertex->at(i+1,j+1,k).value   < isovalue) cubeindex |= 32;
            if (vertex->at(i+1,j+1,k+1).value < isovalue) cubeindex |= 64;
            if (vertex->at(i,j+1,k+1).value   < isovalue) cubeindex |= 128;

            /* Cube is entirely in/out of the surface */
            if (edgeTable[cubeindex] == 0) continue;

            /* Find the vertices where the surface intersects the cube */
            if (edgeTable[cubeindex] & 1)
               VertexInterp( &points[0], &vertex->at(i,j,k) , &vertex->at(i+1,j,k));
            if (edgeTable[cubeindex] & 2)
               VertexInterp( &points[1], &vertex->at(i+1,j,k) , &vertex->at(i+1,j,k+1) );
            if (edgeTable[cubeindex] & 4)
               VertexInterp( &points[2], &vertex->at(i+1,j,k+1) , &vertex->at(i,j,k+1) );
            if (edgeTable[cubeindex] & 8)
               VertexInterp( &points[3], &vertex->at(i,j,k+1) , &vertex->at(i,j,k) );
            if (edgeTable[cubeindex] & 16)
               VertexInterp( &points[4], &vertex->at(i,j+1,k) , &vertex->at(i+1,j+1,k) );
            if (edgeTable[cubeindex] & 32)
               VertexInterp( &points[5], &vertex->at(i+1,j+1,k) , &vertex->at(i+1,j+1,k+1) );
            if (edgeTable[cubeindex] & 64)
               VertexInterp( &points[6], &vertex->at(i+1,j+1,k+1) , &vertex->at(i,j+1,k+1) );
            if (edgeTable[cubeindex] & 128)
               VertexInterp( &points[7], &vertex->at(i,j+1,k+1) , &vertex->at(i,j+1,k) );
            if (edgeTable[cubeindex] & 256)
               VertexInterp( &points[8], &vertex->at(i,j,k) , &vertex->at(i,j+1,k) );
            if (edgeTable[cubeindex] & 512)
               VertexInterp( &points[9], &vertex->at(i+1,j,k) , &vertex->at(i+1,j+1,k) );
            if (edgeTable[cubeindex] & 1024)
               VertexInterp( &points[10], &vertex->at(i+1,j,k+1) , &vertex->at(i+1,j+1,k+1) );
            if (edgeTable[cubeindex] & 2048)
               VertexInterp( &points[11], &vertex->at(i,j,k+1) , &vertex->at(i,j+1,k+1) );

            /* Create the triangles */
            for (unsigned int n = 0 ; triTable[cubeindex][n] != -1 ; n += 3 )
            {
               CreateTriangle( &points[ triTable[cubeindex][n  ] ], 
                               &points[ triTable[cubeindex][n+1] ], 
                               &points[ triTable[cubeindex][n+2] ]);
            }
         }
      }
   }
}

/* Linearly interpolate the position where an isosurface cuts
   an edge between two vertices, each with their own scalar value */
void Isosurface::VertexInterp(IVertex* point, IVertex* vertex1, IVertex* vertex2 )
{
   float mu;
   /* Don't try and interpolate if isovalue not between vertices */
   if (vertex1->value > isovalue && vertex2->value > isovalue)
   {
      if (vertex1->value < vertex2->value)
         memcpy( point, vertex1, sizeof(IVertex) );
      else
         memcpy( point, vertex2, sizeof(IVertex) );
      return;
   }
   if (vertex1->value < isovalue && vertex2->value < isovalue) 
   {
      if (vertex1->value > vertex2->value)
         memcpy( point, vertex1, sizeof(IVertex) );
      else
         memcpy( point, vertex2, sizeof(IVertex) );
      return;
   }

   memcpy( point, vertex1, sizeof(IVertex) );
   mu = (isovalue - vertex1->value) / (vertex2->value - vertex1->value);
   point->pos[0] = vertex1->pos[0] + mu * (vertex2->pos[0] - vertex1->pos[0]);
   point->pos[1] = vertex1->pos[1] + mu * (vertex2->pos[1] - vertex1->pos[1]);
   point->pos[2] = vertex1->pos[2] + mu * (vertex2->pos[2] - vertex1->pos[2]);
   if (colourVals)
     point->colourval = vertex1->colourval + mu * (vertex2->colourval - vertex1->colourval);
}

void Isosurface::CreateTriangle(IVertex* point1, IVertex* point2, IVertex* point3)
{
  //Read in order 1,3,2 for counter-clockwise winding
  surfaces->read(target, 1, lucVertexData, &point1->pos);
  surfaces->read(target, 1, lucVertexData, &point3->pos);
  surfaces->read(target, 1, lucVertexData, &point2->pos);
  if (colourVals)
  {
    surfaces->read(target, 1, &point1->colourval, colourVals->label);
    surfaces->read(target, 1, &point3->colourval, colourVals->label);
    surfaces->read(target, 1, &point2->colourval, colourVals->label);
  }
}

#define LEFT_BOTTOM     0
#define RIGHT_BOTTOM    1
#define LEFT_TOP        2
#define RIGHT_TOP       3
#define LEFT            4
#define RIGHT           5
#define TOP             6
#define BOTTOM          7

void Isosurface::DrawWalls()
{
   unsigned int i, j, k;
   IVertex * points[8];
   IVertex midVertices[4];
   points[LEFT] = &midVertices[0];
   points[RIGHT] = &midVertices[1];
   points[TOP] = &midVertices[2];
   points[BOTTOM] = &midVertices[3];

   /* Check boundaries of area this vertex array covers, 
    * only draw walls if aligned to outer edges of global domain */ 
   unsigned int min[3], max[3], range[3] = {nx-1, ny-1, nz-1};
   int axis;

      for (axis=0; axis<3; axis++)
      {
         min[axis] = 0;
         max[axis] = range[axis];
         if (range[axis] == 0) range[axis] = 1;
      }


   /* Generate front/back walls (Z=min/max, sample XY) */
   if (range[K_AXIS] > 0) 
   {
      for ( i = 0 ; i < nx - 1 ; i++ )
      {
         for ( j = 0 ; j < ny - 1 ; j++ )
         {
            for ( k = min[K_AXIS]; k <= max[K_AXIS]; k += range[K_AXIS])
            {
               points[LEFT_BOTTOM]  = &vertex->at( i , j ,k);
               points[RIGHT_BOTTOM] = &vertex->at(i+1, j ,k);
               points[LEFT_TOP]     = &vertex->at( i ,j+1,k);
               points[RIGHT_TOP]    = &vertex->at(i+1,j+1,k);
               WallElement( points );
            }
         }
      }
   }

   /* Generate left/right walls (X=min/max, sample YZ) */
   if (range[I_AXIS] > 0) 
   {
      for ( k = 0 ; k < nz - 1 ; k++ )
      {
         for ( j = 0 ; j < ny - 1 ; j++ )
         {
            for ( i = min[I_AXIS]; i <= max[I_AXIS]; i += range[I_AXIS])
            {
               points[LEFT_BOTTOM]  = &vertex->at(i, j , k );
               points[RIGHT_BOTTOM] = &vertex->at(i,j+1, k );
               points[LEFT_TOP]     = &vertex->at(i, j ,k+1);
               points[RIGHT_TOP]    = &vertex->at(i,j+1,k+1);
               WallElement( points );
            }
         }
      }
   }

   /* Generate top/bottom walls (Y=min/max, sample XZ) */
   if (range[J_AXIS] > 0) 
   {
      for ( i = 0 ; i < nx - 1 ; i++ )
      {
         for ( k = 0 ; k < nz - 1 ; k++ )
         {
            for ( j = min[J_AXIS]; j <= max[J_AXIS]; j += range[J_AXIS])
            {
               points[LEFT_BOTTOM]  = &vertex->at( i ,j, k );
               points[RIGHT_BOTTOM] = &vertex->at(i+1,j, k );
               points[LEFT_TOP]     = &vertex->at( i ,j,k+1);
               points[RIGHT_TOP]    = &vertex->at(i+1,j,k+1);
               WallElement( points );
            }
         }
      }
   }
}

void Isosurface::WallElement( IVertex** points )
{
   char   squareType = 0;

   /* find cube type */
   if (points[LEFT_BOTTOM]->value  > isovalue) squareType += 1;
   if (points[RIGHT_BOTTOM]->value > isovalue) squareType += 2;
   if (points[LEFT_TOP]->value     > isovalue) squareType += 4;
   if (points[RIGHT_TOP]->value    > isovalue) squareType += 8;

   if (squareType == 0) return; /* No triangles here */

   /* Create Intermediate Points */
   VertexInterp( points[ LEFT ], points[ LEFT_BOTTOM ] , points[LEFT_TOP]);
   VertexInterp( points[ RIGHT ], points[ RIGHT_BOTTOM ] , points[RIGHT_TOP]);
   VertexInterp( points[ BOTTOM ], points[ LEFT_BOTTOM ] , points[RIGHT_BOTTOM]);
   VertexInterp( points[ TOP ], points[ LEFT_TOP ] , points[RIGHT_TOP]);

   MarchingRectangles( points, squareType );
}


void Isosurface::MarchingRectangles( IVertex** points, char squareType)
{
   switch (squareType)
   {
   case 0:
      /*  @@  */
      /*  @@  */
      break;
   case 1:
      /*  @@  */
      /*  #@  */
      CreateTriangle( points[LEFT_BOTTOM], points[LEFT], points[BOTTOM]);
      break;
   case 2:
      /*  @@  */
      /*  @#  */
      CreateTriangle( points[BOTTOM], points[RIGHT], points[RIGHT_BOTTOM]);
      break;
   case 3:
      /*  @@  */
      /*  ##  */
      CreateTriangle( points[LEFT_BOTTOM], points[LEFT], points[RIGHT]);
      CreateTriangle( points[LEFT_BOTTOM], points[RIGHT], points[RIGHT_BOTTOM]);
      break;
   case 4:
      /*  #@  */
      /*  @@  */
      CreateTriangle( points[LEFT_TOP], points[TOP], points[LEFT]);
      break;
   case 5:
      /*  #@  */
      /*  #@  */
      CreateTriangle( points[LEFT_TOP], points[TOP], points[LEFT_BOTTOM]);
      CreateTriangle( points[TOP], points[BOTTOM], points[LEFT_BOTTOM]);
      break;
   case 6:
      /*  #@  */
      /*  @#  */
      CreateTriangle( points[LEFT_TOP], points[TOP], points[LEFT]);
      CreateTriangle( points[BOTTOM], points[RIGHT], points[RIGHT_BOTTOM]);
      break;
   case 7:
      /*  #@  */
      /*  ##  */
      CreateTriangle( points[RIGHT], points[RIGHT_BOTTOM], points[TOP]);
      CreateTriangle( points[TOP], points[RIGHT_BOTTOM], points[LEFT_TOP]);
      CreateTriangle( points[LEFT_TOP], points[RIGHT_BOTTOM], points[LEFT_BOTTOM]);
      break;
   case 8:
      /*  @#  */
      /*  @@  */
      CreateTriangle( points[TOP], points[RIGHT_TOP], points[RIGHT]);
      break;
   case 9:
      /*  @#  */
      /*  #@  */
      CreateTriangle( points[TOP], points[RIGHT_TOP], points[RIGHT]);
      CreateTriangle( points[LEFT_BOTTOM], points[LEFT], points[BOTTOM]);
      break;
   case 10:
      /*  @#  */
      /*  @#  */
      CreateTriangle( points[TOP], points[RIGHT_TOP], points[RIGHT_BOTTOM]);
      CreateTriangle( points[BOTTOM], points[TOP], points[RIGHT_BOTTOM]);

      break;
   case 11:
      /*  @#  */
      /*  ##  */
      CreateTriangle( points[TOP], points[LEFT_BOTTOM], points[LEFT]);
      CreateTriangle( points[RIGHT_TOP], points[LEFT_BOTTOM], points[TOP]);
      CreateTriangle( points[RIGHT_BOTTOM], points[LEFT_BOTTOM], points[RIGHT_TOP]);
      break;
   case 12:
      /*  ##  */
      /*  @@  */
      CreateTriangle( points[LEFT], points[LEFT_TOP], points[RIGHT]);
      CreateTriangle( points[RIGHT], points[LEFT_TOP], points[RIGHT_TOP]);
      break;
   case 13:
      /*  ##  */
      /*  #@  */
      CreateTriangle( points[BOTTOM], points[RIGHT_TOP], points[RIGHT]);
      CreateTriangle( points[LEFT_BOTTOM], points[RIGHT_TOP], points[BOTTOM]);
      CreateTriangle( points[LEFT_TOP], points[RIGHT_TOP], points[LEFT_BOTTOM]);
      break;
   case 14:
      /*  ##  */
      /*  @#  */
      CreateTriangle( points[LEFT], points[LEFT_TOP], points[BOTTOM]);
      CreateTriangle( points[BOTTOM], points[LEFT_TOP], points[RIGHT_BOTTOM]);
      CreateTriangle( points[RIGHT_BOTTOM], points[LEFT_TOP], points[RIGHT_TOP]);
      break;
   case 15:
      /*  ##  */
      /*  ##  */
      CreateTriangle( points[LEFT_TOP], points[RIGHT_TOP], points[RIGHT_BOTTOM]);
      CreateTriangle( points[RIGHT_BOTTOM], points[LEFT_BOTTOM], points[LEFT_TOP]);
      break;
   default:
      abort_program("In func %s: Cannot understand square type %d\n", __func__, squareType );
   }
}


