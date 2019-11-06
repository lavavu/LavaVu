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
** THIS SOFTWARE IS PROVIDED BY THE COPY_EDGE_RIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
** THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPY_EDGE_RIGHT OWNER OR CONTRIBUTORS
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

#include "Contour.h"

// Given a grid dataset and an isovalue, calculate the line segments required to represent the contour through the data.
Contour::Contour(std::vector<Geom_Ptr>& geom, Geometry* lines, DrawingObject* draw, DrawingObject* target, Geometry* source)
  : lines(lines), target(target)
{
  //Generate a contour from a data slice - input source must have width/height set (eg: grid, QuadSurface)
  clock_t t1,t2,tt;
  tt = t1 = clock();
  if (geom.size() == 0) return;

  for (unsigned int index = 0; index < geom.size(); index++)
  {
    if (geom[index]->draw != draw || !geom[index]->width) continue;
    //Get isovalues array, or scalar, or isovalue property as fallback
    json isovalues = target->properties["isovalues"];
    labelformat = target->properties["format"];
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

    DrawingObject* draw = geom[index]->draw;
    debug_print("Extracting contours from grid: %s (%d %d)\n", draw->name().c_str(), geom[index]->width, geom[index]->height);

    geom[index]->colourCalibrate();

    nI = geom[index]->width;
    nJ = geom[index]->height;

    if (nI == 0 || nJ == 0)
      abort_program("Invalid grid dimensions %d %d %d\n", nI, nJ);

    vertex = new vertices2(nI, nJ);

    //Save colour values reference
    colourVals = geom[index]->colourData();
    if (colourVals != NULL && colourVals->size() != nI*nJ)
      colourVals = NULL;

    // Sample in regular grid
    Colour c;
    unsigned int next = 0;
    for (unsigned int j = 0; j < nJ; j++)
    {

      for (unsigned int i = 0; i < nI; i++)
      {
        memcpy(vertex->at(i,j).pos, geom[index]->render->vertices[nI * j + i], sizeof(float)*3);

        next = (j*nI + i);
        //std::cout << x << "," << y << "," << " : " << next << std::endl;

        //Set the contour value
        if (geom[index]->values.size() > 0) //Use first values entry
        {
          assert(geom[index]->valueData(0)->size() > next);
          vertex->at(i,j).value = geom[index]->valueData(0, next);
        }

        //Set the colour value
        if (colourVals)
          vertex->at(i,j).colourval = geom[index]->colourData(next);
        else
          vertex->at(i,j).colourval = vertex->at(i,j).value;

        //vertex->at(x,y,z).colourval = (x*y)/(float)(nI*nJ); //vertex->at(x,y).pos[1] - start[1]; //geom[i+j]->colourData(next);
      }
    }

    debug_print(" %s width %d height %d sampled %d %d\n", draw->name().c_str(), geom[index]->width, geom[index]->height, nI, nJ);
    t2 = clock(); debug_print("  Vertex load took %.4lf seconds.\n", (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

    printedIndex = -1;
    coordIndex = 0;

    for (auto isoval : isovalues)
    {
      isovalue = isoval;
      //printf("GEOM %d using isovalue %f\n", index, isovalue);

      //Create a new data store for output geometry
      lines->add(target);

      // Find Lines with Marching Rectangles
      MarchingRectangles();

      t2 = clock(); debug_print("  Surface extraction (%d triangles) took %.4lf seconds.\n", lines->getObjectStore(target)->count()/3, (t2-t1)/(double)CLOCKS_PER_SEC); t1 = clock();

      //Adjust bounding box
      lines->compareMinMax(geom[index]->min, geom[index]->max);

      t2 = clock();
      debug_print("Total %.4lf seconds.\n", (t2-tt)/(double)CLOCKS_PER_SEC);

      coordIndex++;

    }

    delete vertex;
  }
}

/* Linearly interpolate the position where an isosurface cuts
   an edge between two vertices, each with their own scalar value */
void Contour::VertexInterp(IVertex* point, IVertex* vertex1, IVertex* vertex2 )
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

#define _EDGE_LEFT   0
#define _EDGE_RIGHT  1
#define _EDGE_BOTTOM 2
#define _EDGE_TOP    3

void Contour::MarchingRectangles()
{
  for (unsigned int i = 0 ; i < nI-1; i++)
  {
    for (unsigned int j = 0 ; j < nJ-1; j++)
    {
      /* Assign a unique number to the element type from 0 to 15 */
      unsigned int elementType = 0;
      if (vertex->at(i,j).value     > isovalue) 	elementType += 1;
      if (vertex->at(i+1,j).value   > isovalue) 	elementType += 2;
      if (vertex->at(i,j+1).value   > isovalue) 	elementType += 4;
      if (vertex->at(i+1,j+1).value > isovalue) 	elementType += 8;

      switch ( elementType )
      {
        case 0:
          /*  @@  */
          /*  @@  */
          break;
        case 1:
          /*  @@  */
          /*  #@  */
          addVertex(_EDGE_LEFT, i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 2:
          /*  @@  */
          /*  @#  */
          addVertex(_EDGE_RIGHT, i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 3:
          /*  @@  */
          /*  ##  */
          addVertex(_EDGE_LEFT, i, j);
          addVertex(_EDGE_RIGHT, i, j);
          break;
        case 4:
          /*  #@  */
          /*  @@  */
          addVertex(_EDGE_LEFT  , i, j);
          addVertex(_EDGE_TOP   , i, j);
          break;
        case 5:
          /*  #@  */
          /*  #@  */
          addVertex(_EDGE_TOP   , i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 6:
          /*  #@  */
          /*  @#  */
          addVertex(_EDGE_LEFT, i, j);
          addVertex(_EDGE_TOP , i, j);

          addVertex(_EDGE_RIGHT , i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 7:
          /*  #@  */
          /*  ##  */
          addVertex(_EDGE_TOP, i, j);
          addVertex(_EDGE_RIGHT, i, j);
          break;
        case 8:
          /*  @#  */
          /*  @@  */
          addVertex(_EDGE_TOP, i, j);
          addVertex(_EDGE_RIGHT, i, j);
          break;
        case 9:
          /*  @#  */
          /*  #@  */
          addVertex(_EDGE_TOP, i, j);
          addVertex(_EDGE_RIGHT, i, j);

          addVertex(_EDGE_BOTTOM, i, j);
          addVertex(_EDGE_LEFT, i, j);
          break;
        case 10:
          /*  @#  */
          /*  @#  */
          addVertex(_EDGE_TOP, i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 11:
          /*  @#  */
          /*  ##  */
          addVertex(_EDGE_TOP, i, j);
          addVertex(_EDGE_LEFT, i, j);
          break;
        case 12:
          /*  ##  */
          /*  @@  */
          addVertex(_EDGE_LEFT, i, j);
          addVertex(_EDGE_RIGHT, i, j);
          break;
        case 13:
          /*  ##  */
          /*  #@  */
          addVertex(_EDGE_RIGHT, i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 14:
          /*  ##  */
          /*  @#  */
          addVertex(_EDGE_LEFT, i, j);
          addVertex(_EDGE_BOTTOM, i, j);
          break;
        case 15:
          /*  ##  */
          /*  ##  */
          break;
        default:
          abort_program("In func %s: Cannot understand element type %d\n", __func__, elementType );
      }
    }
  }
}

void Contour::addVertex(char edge, int aIndex, int bIndex)
{
   IVertex* v1 = &vertex->at(aIndex,bIndex);
   IVertex* v2 = &vertex->at(aIndex+1,bIndex);
   IVertex* v3 = &vertex->at(aIndex,bIndex+1);
   IVertex* v4 = &vertex->at(aIndex+1,bIndex+1);
   IVertex vert;
   switch (edge)
   {
   case _EDGE_BOTTOM:
      VertexInterp(&vert, v1, v2);
      break;
   case _EDGE_TOP:
      VertexInterp(&vert, v3, v4);
      break;
   case _EDGE_LEFT:
      VertexInterp(&vert, v1, v3);
      break;
   case _EDGE_RIGHT:
      VertexInterp(&vert, v2, v4);
      break;
   }

   //Write vertex
   lines->read(target, 1, lucVertexData, vert.pos);
   if (colourVals)
     lines->read(target, 1, &vert.colourval, colourVals->label);

   //Labels
   char label[64] = "";
   if (labelformat.length() && aIndex > 0 && bIndex > 0)
   //if (labelformat.length() && aIndex > 0 && bIndex > 0 && aIndex < (int)nI-2 && bIndex < (int)nJ-2)
   //if (labelformat.length() && coordIndex % 4 == edge) 
   { 
     if (printedIndex < coordIndex)
     {
       //Print label data
       snprintf(label, 63, labelformat.c_str(), isovalue);
       printedIndex = coordIndex;
     }
   }
   lines->label(target, label);
}

