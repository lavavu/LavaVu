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

//Init static data
float Geometry::min[3] = {HUGE_VAL, HUGE_VAL, HUGE_VAL};
float Geometry::max[3] = {-HUGE_VAL, -HUGE_VAL, -HUGE_VAL};
std::vector<TimeStep> Geometry::timesteps; //Active model timesteps
float GeomData::opacity = 0;

void GeomData::label(std::string& labeltext)
{
   //Adds a vertex label
   labels.push_back(labeltext);
}

const char* GeomData::getLabels()
{
   if (labelptr) free(labelptr);
   labelptr = NULL;
   if (labels.size())
   {
      //Get total length
      int length = 1;
      for (unsigned int i=0; i < labels.size(); i++)
         length += labels[i].size() + 1;
      labelptr = (char*)malloc(sizeof(char) * length);
      labelptr[0] = '\0';
      //Copy labels
      for (unsigned int i=0; i < labels.size(); i++)
         sprintf(labelptr, "%s%s\n", labelptr, labels[i].c_str());
   }
   return (const char*)labelptr;
}

//Utility functions, calibrate colourmaps and get colours
void GeomData::colourCalibrate()
{
   //Calibrate colour maps on ranges for related data
   if (draw->colourMaps[lucColourValueData])
      draw->colourMaps[lucColourValueData]->calibrate(colourValue);
   if (draw->colourMaps[lucOpacityValueData])
      draw->colourMaps[lucOpacityValueData]->calibrate(opacityValue);
   if (draw->colourMaps[lucRedValueData])
      draw->colourMaps[lucRedValueData]->calibrate(redValue);
   if (draw->colourMaps[lucGreenValueData])
      draw->colourMaps[lucGreenValueData]->calibrate(greenValue);
   if (draw->colourMaps[lucBlueValueData])
      draw->colourMaps[lucBlueValueData]->calibrate(blueValue);
}

//Get colour using specified colourValue
void GeomData::mapToColour(Colour& colour, float value)
{
   colour = draw->colourMaps[lucColourValueData]->getfast(value);

   //Set opacity to drawing object override level if set
   if (draw->opacity > 0.0 && draw->opacity < 1.0)
      colour.a = draw->opacity * 255;
}

//Sets the colour for specified vertex index, looks up all provided colourmaps
void GeomData::getColour(Colour& colour, int idx)
{
   //Lookup using base colourmap first, use base colour if no map
   if (draw->colourMaps[lucColourValueData] && colourValue.size() > 0)
   {
      if (colourValue.size() == 1) idx = 0;  //Single colour value only provided
      colour = draw->colourMaps[lucColourValueData]->getfast(colourValue[idx]);
   }
   else if (colours.size() > 0)
   {
      if (colours.size() == 1) idx = 0;  //Single colour only provided
      colour = colours.toColour(idx);
   }
   else
      colour = draw->colour;

   //Set components using component colourmaps...
   Colour cc;
   if (draw->colourMaps[lucRedValueData] && redValue.size() > 0)
   {
      cc = draw->colourMaps[lucRedValueData]->getfast(redValue[idx]);
      colour.r = cc.r;
   }
   if (draw->colourMaps[lucGreenValueData] && greenValue.size() > 0)
   {
      cc = draw->colourMaps[lucGreenValueData]->getfast(greenValue[idx]);
      colour.g = cc.g;
   }
   if (draw->colourMaps[lucBlueValueData] && blueValue.size() > 0)
   {
      cc = draw->colourMaps[lucBlueValueData]->getfast(blueValue[idx]);
      colour.b = cc.b;
   }
   if (draw->colourMaps[lucOpacityValueData] && opacityValue.size() > 0)
   {
      cc = draw->colourMaps[lucOpacityValueData]->getfast(opacityValue[idx]);
      colour.a = cc.a;
   }

   //Set opacity to drawing object override level if set
   if (draw->opacity > 0.0 && draw->opacity < 1.0)
      colour.a = draw->opacity * 255;
}

void GeomData::setColour(int idx)
{
   Colour colour;
   getColour(colour, idx);
   //Multiply opacity by global override level if set
   //(We don't do this in getColour because it is used by shader drawn objects,
   // which do this in their shader code instead)
   if (GeomData::opacity > 0.0)
      colour.a *= GeomData::opacity;
   glColor4ubv(colour.rgba);
}

Geometry::Geometry(bool hidden) : view(NULL), elements(-1), allhidden(hidden), total(0), scale(1.0f), redraw(true), wireframe(false), cullface(false), flat(false), lit(true)
{
}

Geometry::~Geometry()
{
   clear();
}

//Virtuals to implement
void Geometry::close() //Called on quit or gl context destroy
{
   elements = -1;
   for (unsigned int i=0; i<displaylists.size(); i++)
   {
      if (displaylists[i]) glDeleteLists(displaylists[i], 1);
      displaylists[i] = 0;
   }
}

void Geometry::clear(bool all)
{
   total = 0;
   elements = -1;
   redraw = true;
   //iterate geom and delete all GeomData entries
   for (int i = geom.size()-1; i>=0; i--) 
   {
      unsigned int idx = i;
      if (all || !geom[idx]->draw->persistent)
      {
         delete geom[idx]; 
         if (!all) geom.erase(geom.begin()+idx);
      }
   }
   if (all) geom.clear();

   for (int i=0; i<3; i++)
   {
      Geometry::min[i] = HUGE_VAL;
      Geometry::max[i] = -HUGE_VAL;
   }
}

void Geometry::dumpById(std::ostream& csv, unsigned int id)
{
   for (unsigned int i = 0; i < geom.size(); i++) 
   {
      if (geom[i]->draw->id == id)
      {
         std::cout << "Collected " << geom[i]->count << " vertices/values (" << i << ")" << std::endl;
         //Only supports dump of vertex and colourValue at present
         for (int v=0; v < geom[i]->count; v++)
         {
            csv << geom[i]->vertices[v][0] << ',' <<  geom[i]->vertices[v][1] << ',' << geom[i]->vertices[v][2];

            if (geom[i]->colourValue.size() == geom[i]->count)
               csv << ',' << geom[i]->colourValue[v];

            csv << std::endl;
         }
      }
   }
}

void Geometry::jsonWrite(unsigned int id, std::ostream* osp)
{
   //Placeholder virtual
}

bool Geometry::hide(unsigned int idx)
{
   if (idx >= geom.size()) return false;
   if (hidden[idx]) return false;
   hidden[idx] = true;
   redraw = true;
   return true;
}

void Geometry::hideAll()
{
   if (hidden.size() == 0) return;
   for (unsigned int i=0; i<hidden.size(); i++)
   {
      hidden[i] = true;
      geom[i]->draw->visible = false;
   }
   allhidden = true;
   redraw = true;
}

bool Geometry::show(unsigned int idx)
{
   if (idx >= geom.size()) return false;
   if (!hidden[idx]) return false;
   hidden[idx] = false;
   redraw = true;
   return true;
}

void Geometry::showAll()
{
   if (hidden.size() == 0) return;
   for (unsigned int i=0; i<hidden.size(); i++)
   {
      hidden[i] = false;
      geom[i]->draw->visible = true;
   }
   allhidden = false;
   redraw = true;
}

void Geometry::showById(unsigned int id, bool state)
{
   for (unsigned int i = 0; i < geom.size(); i++)
   {
      if (geom[i]->draw->id == id)
      {
         hidden[i] = !state;
         geom[i]->draw->visible = state;
      }
   }
   redraw = true;
}

void Geometry::localiseColourValues()
{
   for (unsigned int i = 0; i < geom.size(); i++) 
   {
      //Get local min and max for each element from colourValues
      if (geom[i]->draw->colourMaps[lucColourValueData])
      {
         geom[i]->colourValue.minimum = HUGE_VAL;
         geom[i]->colourValue.maximum = -HUGE_VAL;
         for (int v=0; v < geom[i]->colourValue.size(); v++)
         {
            // Check min/max against each value
            if (geom[i]->colourValue[v] > geom[i]->colourValue.maximum) geom[i]->colourValue.maximum = geom[i]->colourValue[v];
            if (geom[i]->colourValue[v] < geom[i]->colourValue.minimum) geom[i]->colourValue.minimum = geom[i]->colourValue[v];
         }
      }
   }
}

void Geometry::redrawObject(unsigned int id)
{
   for (unsigned int i = 0; i < geom.size(); i++)
   {
      if (geom[i]->draw->id == id)
      {
         elements = -1; //Force reload data
         redraw = true;
         return;
      }
   }
}

void Geometry::init() //Called on GL init
{
   for (unsigned int i = 0; i<displaylists.size(); i++)
   {
      if (displaylists[i]) glDeleteLists(displaylists[i], 1);
      displaylists[i] = 0;
      //displaylists[i] = glGenLists(1);
   }
   redraw = true;
}

void Geometry::setState(int index, Shader* prog)
{
   DrawingObject* draw = NULL;
   int texunit = -1;
   bool lighting = lit && !flat;
   if (index >= 0) draw = geom[index]->draw;
   if (draw) lighting = lighting && (draw->lit && !draw->flat);

   //Global/Local draw state
   if (cullface || (draw && draw->cullface))
      glEnable(GL_CULL_FACE);
   else
      glDisable(GL_CULL_FACE);

   //Surface specific options
   if (type == lucTriangleType || type == lucGridType)
   {
      //Don't light surfaces in 3d models
      if (!view->is3d) lighting = false;
      //Disable lighting and polygon faces in wireframe mode
      if (wireframe || (draw && draw->wireframe))
      {
         glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
         lighting = false;
         glDisable(GL_CULL_FACE);
      }
      else
         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   }

   if (!lighting)
      glDisable(GL_LIGHTING);
   else
      glEnable(GL_LIGHTING);

   if (draw)
   {
      if (draw->lineWidth <= 0) draw->lineWidth = 1.0;
      glLineWidth(draw->lineWidth);
      //Textured?
      texunit = draw->useTexture();
   }
   GL_Error_Check;

   //Uniforms for shader programs
   if (prog && prog->program > 0)
   {
      prog->use();
      prog->setUniform("uOpacity", GeomData::opacity);
      prog->setUniform("uLighting", lighting);
      prog->setUniform("uTextured", texunit >= 0);
      if (texunit >= 0)
         prog->setUniform("uTexture", texunit);
   }
   GL_Error_Check;
}

void Geometry::update()
{
   //Default implementation, ensures display lists are generated
   for (unsigned int i = 0; i<geom.size(); i++)
   {
      if (i == displaylists.size()) displaylists.push_back(0);
      if (displaylists[i]) glDeleteLists(displaylists[i], 1);
      displaylists[i] = glGenLists(1);
   }
}

void Geometry::draw()  //Display saved geometry (default uses display list)
{
   GL_Error_Check;

   //Default to no shaders
   if (glUseProgram) glUseProgram(0);

   if (geom.size())
   {
      if (redraw) update();
         GL_Error_Check;
      redraw = false;
      //Draw using display lists if available
      for (unsigned int i=0; i<geom.size(); i++)
      {
         //Because of quad surface sorting, have to check drawable when creating lists
         //When quads moved to triangle renderer can re-enable this and won't have to
         //recreate display lists when hiding/showing/switching viewports
         //if (drawable(i) && displaylists[i] && glIsList(displaylists[i]))
         if (displaylists[i] && glIsList(displaylists[i]))
            glCallList(displaylists[i]);
         GL_Error_Check;
      }
   }
   GL_Error_Check;

   labels();
}

void Geometry::labels()
{
   //Print labels
   glColor3f(0,0,0);
   lucSetFontCharset(FONT_SMALL); //Bitmap fonts
   for (unsigned int i=0; i < geom.size(); i++)
   {
      if (drawable(i) && geom[i]->labels.size() > 0) 
      {
         for (unsigned int j=0; j < geom[i]->labels.size(); j++)
         {
            float* p = geom[i]->vertices[j];
            if (geom[i]->labels[j].size() > 0)
               lucPrint3d(p[0], p[1], p[2], geom[i]->labels[j].c_str());
         }
      }
   }
}

//Returns true if passed geometry element index is drawable
//ie: has data, in range, not hidden and in viewport object list 
bool Geometry::drawable(unsigned int idx)
{
   //Within bounds and not hidden
   if (idx < geom.size() && geom[idx]->count > 0 && !hidden[idx])
   {
      //Not filtered by viewport?
      if (!view->filtered) return true;

      //Search viewport object set
      if (view->hasObject(geom[idx]->draw))
         return true;

   }
   return false;
}

GeomData* Geometry::getObjectStore(DrawingObject* draw)
{
   //Get passed object's most recently added data store
   GeomData* geomdata = NULL;
   for (int i=geom.size()-1; i>=0; i--)
   {
      if (geom[i]->draw == draw)
      {
         geomdata = geom[i];
         break;
      }
   }
   return geomdata;
}

GeomData* Geometry::add(DrawingObject* draw)
{
   GeomData* geomdata = new GeomData(draw);
   geom.push_back(geomdata);
   if (hidden.size() < geom.size()) hidden.push_back(allhidden);
   if (allhidden) draw->visible = false;
   //debug_print("NEW DATA STORE CREATED FOR %s size %d\n", draw->name.c_str(), geom.size());
   return geomdata;
}

void Geometry::move(Geometry* other)
{
   //Copy GeomData objects
   clear(true); //Clear all existing entries
   geom = other->geom;  //Copies all elements
   //Copy required fields
   total = other->total;
   elements = other->elements;
   //Clear references from other (or will be deleted when objects cleared!)
   other->geom.clear();
}

//Read geometry data from storage
void Geometry::read(DrawingObject* draw, int n, lucGeometryDataType type, const void* data, int width, int height, int index)
{
   draw->skip = false;  //Enable object (has data now)
   GeomData* geomdata;
   if (index < 0)
      //Get passed object's most recently added data store
      geomdata = getObjectStore(draw);
   else
      geomdata = geom[index];
   

   //Objects with a specified width & height: detect new data store when required (full)
   if (!geomdata || (type == lucVertexData && 
       geomdata->width > 0 && geomdata->height > 0 && 
       geomdata->width * geomdata->height == geomdata->count))
   {
      //No store yet or loading vertices and already have required amount, new object required...
      //Create new data store, save in drawing object and Geometry list
      geomdata = add(draw);
   }

   //Set width & height if provided
   if (width) geomdata->width = width;
   if (height) geomdata->height = height;

   //Read the data
   if (n > 0) geomdata->data[type]->read(n, data);

   if (type == lucVertexData)
   {
      geomdata->count += n;
      total += n;
   }
}

void Geometry::setup(DrawingObject* draw, lucGeometryDataType type, float minimum, float maximum, float dimFactor, const char* units)
{
   //Get passed object's most recently added data store and setup draw data
   GeomData* geomdata = getObjectStore(draw);
   if (!geomdata) return;
   geomdata->data[type]->setup(minimum, maximum, dimFactor, units);
}

void Geometry::label(DrawingObject* draw, const char* labels)
{
   //Get passed object's most recently added data store and add vertex labels (newline separated)
   GeomData* geomdata = getObjectStore(draw);
   if (!geomdata) return;

   //Split by newlines
   std::istringstream iss(labels);
   std::string line;
   while(std::getline(iss, line))
      geomdata->label(line);
}

//Track min/max coords
void Geometry::checkPointMinMax(float *coord)
{
   if (coord[0] > max[0]) max[0] = coord[0];
   if (coord[0] < min[0]) min[0] = coord[0];
   if (coord[1] > max[1]) max[1] = coord[1];
   if (coord[1] < min[1]) min[1] = coord[1];
   if (coord[2] > max[2]) max[2] = coord[2];
   if (coord[2] < min[2]) min[2] = coord[2];
}

void Geometry::getMinMaxDistance(float modelView[16], float* mindist, float* maxdist)
{
   //Save min/max distance
   float vert[3], dist;
   *maxdist = -HUGE_VAL;
   *mindist = HUGE_VAL; 
   for (int i=0; i<2; i++)
   {
      vert[0] = i==0 ? min[0] : max[0];
      for (int j=0; j<2; j++)
      {
         vert[1] = j==0 ? min[1] : max[1];
         for (int k=0; k<2; k++)
         {
            vert[2] = k==0 ? min[2] : max[2];
            dist = eyeDistance(modelView, vert);
            if (dist < *mindist) *mindist = dist;
            if (dist > *maxdist) *maxdist = dist;
         }
      }
   }
   if (*maxdist == *mindist) *maxdist += 0.0000001;
   //printf("DISTANCE MIN %f MAX %f\n", *mindist, *maxdist);
}

void Geometry::print()
{
   for (unsigned int i = 0; i < geom.size(); i++) 
   {
      switch (type)
      {
      case lucLabelType:
         std::cout << "Labels ";
         break;
      case lucPointType:
         std::cout << "Point swarm ";
         break;
      case lucVectorType:
         std::cout << "Vectors";
         break;
      case lucTracerType:
         std::cout << "Tracer swarm";
         break;
      case lucGridType:
         std::cout << "Surface";
         break;
      case lucTriangleType:
         std::cout << "Isosurface";
         break;
      case lucLineType:
         std::cout << "Lines";
         break;
      case lucShapeType:
         std::cout << "Shapes";
         break;
      default:
         std::cout << "UNKNOWN";
      }

      std::cout << i << " - " << std::endl;
      //std::cout << i << " - " << (drawable(i) ? "shown" : "hidden") << std::endl;
   }
}

void Geometry::newData(DrawingObject* draw)
{
   //Prepare for new data appended to current objects by setting offsets
   //(Used by tracers so they can collate data rather than creating new objects every time)
   for (unsigned int i = 0; i < geom.size(); i++) 
   {
      int data_type;
      for (data_type=lucMinDataType; data_type<lucMaxDataType; data_type++)
      {
         if (geom[i]->draw == draw)
            geom[i]->data[data_type]->setOffset();
      }
   }
}

//Dumps colourmapped data to image
void Geometry::toImage(unsigned int idx)
{
   geom[idx]->colourCalibrate();
   int width = geom[idx]->width;
   if (width == 0) width = 256;
   int height = geom[idx]->colourValue.size() / width;
   char path[256];
   int pixel = 3;
   GLubyte *image = new GLubyte[width * height * pixel];
   // Read the pixels
   for (int y=0; y<height; y++)
   {
     for (int x=0; x<width; x++)
     {
       printf("%f\n", geom[idx]->colourValue[y * width + x]);
       Colour c;
       geom[idx]->getColour(c, y * width + x);
       image[y * width*pixel + x*pixel + 0] = c.r;
       image[y * width*pixel + x*pixel + 1] = c.g;
       image[y * width*pixel + x*pixel + 2] = c.b;
     }
   }
   //Write data to image file
   sprintf(path, "%s.%05d", geom[idx]->draw->name.c_str(), idx);
   writeImage(image, width, height, path, false);
   delete[] image;
}

/////////////////////////////////////////////////////////////////////////////////
// Sorting utility functions
/////////////////////////////////////////////////////////////////////////////////
// Comparison for X,Y,Z vertex sort
int compareXYZ(const void *a, const void *b)
{
   float *s1 = (float*)a;
   float *s2 = (float*)b;
   
   if (s1[0] != s2[0]) return s1[0] < s2[0];
   if (s1[1] != s2[1]) return s1[1] < s2[1];
   return s1[2] < s2[2];
}

int compareParticle(const void *a, const void *b)
{
    PIndex *p1 = (PIndex*)a;
    PIndex *p2 = (PIndex*)b;
    if (p1->distance == p2->distance) return 0;
    return p1->distance < p2->distance ? -1 : 1;
}

//Generic radix sorter - template free version
void radix_sort_byte(int byte, long N, unsigned char *source, unsigned char *dest, int size)
{
   // Radix counting sort of 1 byte, 8 bits = 256 bins
   long count[256], index[256];
   int i;
   memset(count, 0, sizeof(count)); //Clear counts

   //Create histogram, count occurences of each possible byte value 0-255
   for (i=0; i<N; i++)
      count[source[i*size+byte]]++;

   //Calculate number of elements less than each value (running total through array)
   //This becomes the offset index into the sorted array
   //(eg: there are 5 smaller values so place me in 6th position = 5)
   index[0]=0;
   for (i=1; i<256; i++) index[i] = index[i-1] + count[i-1];

   //Finally, re-arrange data by index positions
   for (i=0; i<N; i++)
   {
      int val = source[i*size + byte];  //Get value
      memcpy(&dest[index[val]*size], &source[i*size], size);
      index[val]++; //Increment index to push next element with same value forward one
   }
}


//////////////////////////////////

