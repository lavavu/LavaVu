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

QuadSurfaces::QuadSurfaces(Session& session) : Triangles(session)
{
  type = lucGridType;
  primitive = GL_TRIANGLES;
}

QuadSurfaces::~QuadSurfaces()
{
  Triangles::close();
}

void QuadSurfaces::update()
{
  // Actual quads can no longer be rendered in modern OpenGL
  // This renderer now just replaces the per-triangle sorting in TriSurfaces
  // with simpler version that sorts by centre of each geometry element
  //
  // Functions to turn grid vertices into triangles are already provided by Triangles() class
  sort();

  // Update triangles...
  Triangles::update();
}

void QuadSurfaces::sort()
{
  clock_t t1,t2,tt;
  t1 = tt = clock();
  // Update and depth sort surfaces..
  tt=clock();
  if (geom.size() == 0) return;

  //Get element/quad count
  debug_print("Reloading and sorting %d quad surfaces...\n", geom.size());
  total = 0;

  //Add key elements to sort list
  sorted.clear();
  sorted = geom;

  //Calculate min/max distances from viewer
  if (reload) updateBoundingBox();
  float distanceRange[2];
  view->getMinMaxDistance(min, max, distanceRange);

  unsigned int quadverts = 0;
  for (unsigned int i=0; i<sorted.size(); i++)
  {
    unsigned int quads = sorted[i]->gridElements2d();
    quadverts += quads * 4;
    if (quads == 0) quadverts += sorted[i]->elementCount();
    unsigned int v = sorted[i]->count();
    if (v < 4) continue;
    total += v; //Actual vertices

    bool hidden = !drawable(i); //Save flags
    debug_print("Surface %d, quads %d hidden? %s\n", i, quads, (hidden ? "yes" : "no"));

    //Get corners of strip
    float* posmin = sorted[i]->render->vertices[0];
    float* posmax = sorted[i]->render->vertices[v - 1];
    float pos[3] = {posmin[0] + (posmax[0] - posmin[0]) * 0.5f,
                    posmin[1] + (posmax[1] - posmin[1]) * 0.5f,
                    posmin[2] + (posmax[2] - posmin[2]) * 0.5f
                   };

    //Calculate distance from viewing plane
    sorted[i]->distance = view->eyeDistance(pos);
    if (sorted[i]->distance < distanceRange[0]) distanceRange[0] = sorted[i]->distance;
    if (sorted[i]->distance > distanceRange[1]) distanceRange[1] = sorted[i]->distance;
    //printf("%d) %p %f %f %f distance = %f\n", i, sorted[i].get(), pos[0], pos[1], pos[2], sorted[i]->distance);
  }
  if (total == 0) return;
  t2 = clock();
  debug_print("  %.4lf seconds to calculate distances\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();

  //Sort (descending)
  std::sort(sorted.begin(), sorted.end(), GeomPtrCompare());
  t2 = clock();
  debug_print("  %.4lf seconds to sort\n", (t2-t1)/(double)CLOCKS_PER_SEC);
  t1 = clock();
}

void QuadSurfaces::draw()
{
  //Lock the update mutex and copy the sorted vector
  //Allows further sorting to continue in background while rendering
  {
    LOCK_GUARD(loadmutex);
    geom = sorted;
  }
  //Elements at same depth: draw both
  //(mainly for backwards compatibility)
  glDepthFunc(GL_LEQUAL);
  Triangles::draw();
  glDepthFunc(GL_LESS);
}
