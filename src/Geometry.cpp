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

//Init static, names list
std::string GeomData::names[lucMaxType] = {"labels", "points", "quads", "triangles", "vectors", "tracers", "lines", "shapes", "volume"};

std::string GeomData::datalabels[lucMaxDataType+1] = {"vertices", "normals", "vectors",
                                          "values", "opacities", "red", "green", "blue",
                                          "indices", "widths", "heights", "lengths",
                                          "colours", "texcoords", "sizes", 
                                          "luminance", "rgb", "values"
                                         };

//Track min/max coords
void GeomData::checkPointMinMax(float *coord)
{
  //std::cerr << coord[0] << "," << coord[1] << "," << coord[2] << std::endl;
  compareCoordMinMax(min, max, coord);
}

void GeomData::calcBounds()
{
  //Loop through vertices and calculate bounds automatically for all elements
  for (unsigned int j=0; j < count(); j++)
    checkPointMinMax(render->vertices[j]);
}

void GeomData::label(std::string& labeltext)
{
  //Adds a vertex label
  labels.push_back(labeltext);
}

std::string GeomData::getLabels()
{
  std::stringstream ss;
  if (labels.size())
  {
    //Get total length
    int length = 1;
    for (unsigned int i=0; i < labels.size(); i++)
      length += labels[i].size() + 1;
    //Copy labels
    for (unsigned int i=0; i < labels.size(); i++)
      ss << labels[i] << std::endl;
  }
  return ss.str();
}

//Utility functions, calibrate colourmaps and get colours
ColourLookup& GeomData::colourCalibrate()
{
  //Cache colour lookups
  draw->setup();

  //Get filter data indices
  for (unsigned int i=0; i < draw->filterCache.size(); i++)
    draw->filterCache[i].dataIdx = valuesLookup(draw->filterCache[i].by);

  //Calibrate opacity map if provided
  ColourMap* omap = draw->opacityMap;
  bool mappedOpacity = false;
  unsigned int opacityIdx = valuesLookup(draw->properties["opacityby"]);
  FloatValues* ovals = valueData(opacityIdx);
  if (omap && ovals)
  {
    omap->calibrate(ovals);
    //Init the mapped opacity lookup functor
    mappedOpacity = true;
  }

  //Get a colour lookup functor
  // - determine source of colour data
  // - return a colour lookup functor object to do the
  //   required lookup at high efficiency

  //Save the value index
  draw->colourIdx = valuesLookup(draw->properties["colourby"]);

  ColourMap* cmap = draw->colourMap;
  FloatValues* vals = colourData();
  if (cmap && vals)
  {
    //Calibrate the colour map
    cmap->calibrate(vals);
    if (mappedOpacity)
    {
      _getColourMappedOpacityMapped.init(draw, render, vals, ovals);
      _getColourMappedOpacityMapped(draw->colour, 0); //Cache a representative colour
      return _getColourMappedOpacityMapped;
    }
    _getColourMapped.init(draw, render, vals, NULL);
    _getColourMapped(draw->colour, 0); //Cache a representative colour
    return _getColourMapped;
  }
  else if (render->colours.size() > 0)
  {
    if (mappedOpacity)
    {
      _getColourRGBAOpacityMapped.init(draw, render, NULL, ovals);
      _getColourRGBAOpacityMapped(draw->colour, 0); //Cache a representative colour
      return _getColourRGBAOpacityMapped;
    }
    _getColourRGBA.init(draw, render, NULL, NULL);
    _getColourRGBA(draw->colour, 0); //Cache a representative colour
    return _getColourRGBA;
  }
  else if (render->rgb.size() > 0)
  {
    if (mappedOpacity)
    {
      _getColourRGBOpacityMapped.init(draw, render, NULL, ovals);
      _getColourRGBOpacityMapped(draw->colour, 0); //Cache a representative colour
      return _getColourRGBOpacityMapped;
    }
    _getColourRGB.init(draw, render, NULL, NULL);
    _getColourRGB(draw->colour, 0); //Cache a representative colour
    return _getColourRGB;
  }
  else if (render->luminance.size() > 0)
  {
    if (mappedOpacity)
    {
      _getColourLuminanceOpacityMapped.init(draw, render, NULL, ovals);
      _getColourLuminanceOpacityMapped(draw->colour, 0); //Cache a representative colour
    return _getColourLuminanceOpacityMapped;
    }
    _getColourLuminance.init(draw, render, NULL, NULL);
    _getColourLuminance(draw->colour, 0); //Cache a representative colour
    return _getColourLuminance;
  }
  else
  {
    if (mappedOpacity)
    {
      _getColourOpacityMapped.init(draw, render, NULL, ovals);
      return _getColourOpacityMapped;
    }
    _getColour.init(draw, render, NULL, NULL);
    return _getColour;
  }
}

void ColourLookup::operator()(Colour& colour, unsigned int idx) const
{
  colour = draw->colour;
  colour.a *= draw->opacity;
}

void ColourLookupMapped::operator()(Colour& colour, unsigned int idx) const
{
  //assert(idx < values->size());
  if (idx >= vals->size()) idx = vals->size() - 1;
  float val = (*vals)[idx];
  if (val == HUGE_VAL)
    colour.value = 0;
  else
    colour = draw->colourMap->getfast(val);

  colour.a *= draw->opacity;
}

void ColourLookupRGBA::operator()(Colour& colour, unsigned int idx) const
{
  if (render->colours.size() == 1) idx = 0;  //Single colour only provided
  if (idx >= render->colours.size()) idx = render->colours.size() - 1;
  //assert(idx < colours.size());
  colour.value = render->colours[idx];

  colour.a *= draw->opacity;
}

void ColourLookupRGB::operator()(Colour& colour, unsigned int idx) const
{
  if (idx >= render->rgb.size()/3) idx = render->rgb.size()/3 - 1;
  colour.r = render->rgb[idx*3];
  colour.g = render->rgb[idx*3+1];
  colour.b = render->rgb[idx*3+2];
  colour.a = 255*draw->opacity;
}

void ColourLookupLuminance::operator()(Colour& colour, unsigned int idx) const
{
  if (idx >= render->luminance.size()) idx = render->luminance.size() - 1;
  colour.r = colour.g = colour.b = render->luminance[idx];
  colour.a = 255*draw->opacity;
}

void ColourLookupOpacityMapped::operator()(Colour& colour, unsigned int idx) const
{
  colour = draw->colour;
  //Set opacity using own value map...
  const Colour& temp = draw->opacityMap->getfast((*ovals)[idx]);
  colour.a *= div255 * temp.a * draw->opacity;
}

void ColourLookupMappedOpacityMapped::operator()(Colour& colour, unsigned int idx) const
{
  //assert(idx < values->size());
  if (idx >= vals->size()) idx = vals->size() - 1;
  float val = (*vals)[idx];
  if (val == HUGE_VAL)
    colour.value = 0;
  else
    colour = draw->colourMap->getfast(val);

  //Set opacity using own value map...
  const Colour& temp = draw->opacityMap->getfast((*ovals)[idx]);
  colour.a *= div255 * temp.a * draw->opacity;
}

void ColourLookupRGBAOpacityMapped::operator()(Colour& colour, unsigned int idx) const
{
  if (render->colours.size() == 1) idx = 0;  //Single colour only provided
  if (idx >= render->colours.size()) idx = render->colours.size() - 1;
  //assert(idx < colours.size());
  colour.value = render->colours[idx];

  //Set opacity using own value map...
  const Colour& temp = draw->opacityMap->getfast((*ovals)[idx]);
  colour.a *= div255 * temp.a * draw->opacity;
}

void ColourLookupRGBOpacityMapped::operator()(Colour& colour, unsigned int idx) const
{
  if (idx >= render->rgb.size()/3) idx = render->rgb.size()/3 - 1;
  colour.r = render->rgb[idx*3];
  colour.g = render->rgb[idx*3+1];
  colour.b = render->rgb[idx*3+2];

  //Set opacity using own value map...
  const Colour& temp = draw->opacityMap->getfast((*ovals)[idx]);
  colour.a = temp.a * draw->opacity;
}

void ColourLookupLuminanceOpacityMapped::operator()(Colour& colour, unsigned int idx) const
{
  if (idx >= render->luminance.size()) idx = render->luminance.size() - 1;
  colour.r = colour.g = colour.b = render->luminance[idx];

  //Set opacity using own value map...
  const Colour& temp = draw->opacityMap->getfast((*ovals)[idx]);
  colour.a = temp.a * draw->opacity;
}

int GeomData::colourCount()
{
  //Return number of colour values or RGBA colours
  int hasColours = render->colours.size();
  if (render->rgb.size()) hasColours = render->rgb.size() / 3;
  if (values.size() > draw->colourIdx)
  {
    Values_Ptr fv = values[draw->colourIdx];
    hasColours = fv->size();
  }
  return hasColours;
}

unsigned int GeomData::valuesLookup(const json& by)
{
  //Gets a valid value index by property, either actual index or string label
  unsigned int valueIdx = MAX_DATA_ARRAYS+1;
  if (by.is_string())
  {
    //Lookup index from label
    std::string label = by;
    for (unsigned int j=0; j < values.size(); j++)
    {
      if (values[j]->label == label)
      {
        valueIdx = j;
        break;
      }
    }
    if (valueIdx > MAX_DATA_ARRAYS) debug_print("Label: %s not found!\n", label.c_str());
  }
  else if (by.is_number())
    valueIdx = by;

  //Check for sane range
  if (valueIdx >= values.size()) valueIdx = MAX_DATA_ARRAYS+1;

  return valueIdx;
}

//Returns true if vertex/voxel is to be filtered (don't display)
bool GeomData::filter(unsigned int idx)
{
  //Iterate all the filters,
  // - if a value matches a filter condition return true (filtered)
  // - if no filters hit, returns false
  float value, min, max;
  int size, range;
  unsigned int ridx;
  for (unsigned int i=0; i < draw->filterCache.size(); i++)
  {
    if (values.size() <= draw->filterCache[i].dataIdx || !values[draw->filterCache[i].dataIdx]) continue;
    size = values[draw->filterCache[i].dataIdx]->size();
    if (draw->filterCache[i].dataIdx < MAX_DATA_ARRAYS && size > 0)
    {
      //Tracer filter? check if enough data for per step filter, if not assume per tracer
      if (type == lucTracerType && draw->filterCache[i].elements > 0 && size == draw->filterCache[i].elements)
      {
        //debug_print("FILTER PER-TRACER by %d : %d ==> %d\n", draw->filterCache[i].elements, idx, idx % draw->filterCache[i].elements);
        idx %= draw->filterCache[i].elements;
      }

      //Have values but not enough for per-vertex? spread over range (eg: per triangle)
      range = count() / size;
      ridx = idx;
      if (range > 1) ridx = idx / range;

      //std::cout << "Filtering on index: " << draw->filterCache[i].dataIdx << " " << size << " values" << std::endl;
      min = draw->filterCache[i].minimum;
      max = draw->filterCache[i].maximum;
      Values_Ptr v = values[draw->filterCache[i].dataIdx];
      if (draw->filterCache[i].map)
      {
        //Range type filters map over available values on [0,1] => [min,max]
        //If a colourmap is provided, that is used to get the values (allows log maps)
        //Otherwise they come directly from the data 
        ColourMap* cmap = draw->colourMap;
        if (cmap)
          value = cmap->scaleValue((*v)[ridx]);
        else
        {
          value = values[draw->filterCache[i].dataIdx]->maximum - values[draw->filterCache[i].dataIdx]->minimum;
          min = values[draw->filterCache[i].dataIdx]->minimum + min * value;
          max = values[draw->filterCache[i].dataIdx]->minimum + max * value;
          value = (*v)[ridx];
        }
      }
      else
        value = (*v)[ridx];

      //if (idx%10000==0) std::cout << min << " < " << value << " < " << max << std::endl;
      //std::cout << min << " < " << value << " < " << max << std::endl;
      
      //"out" flag indicates values between the filter range are skipped - exclude
      if (draw->filterCache[i].out)
      {
        //- return TRUE if VALUE inside RANGE
        if (min == max)
        {
          if (min == value)
            return true;
          continue;
        }
        //Filters out values between specified ranges (allows filtering separate sections)
        if (draw->filterCache[i].inclusive && value >= min && value <= max)
          return true;
        if (value > min && value < max) 
          return true;
      }
      //Default is to filter values NOT in the filter range - include those that are
      else
      {
        //- return TRUE if VALUE outside RANGE
        if (min == max)
        {
          if (min != value)
            return true;
          continue;
        }
        //Filters out values not between specified ranges (allows combining filters)
        if (draw->filterCache[i].inclusive && (value <= min || value >= max))
          return true;
        if (value < min || value > max)
          return true;
      }
    }
  }
  //std::cout << "(Not filtered)\n";
  return false;
}

FloatValues* GeomData::colourData()
{
  return values.size() && values.size() > draw->colourIdx && values[draw->colourIdx]->size() ? values[draw->colourIdx].get() : NULL;
}

float GeomData::colourData(unsigned int idx) 
{
  if (values.size() == 0 || values.size() <= draw->colourIdx) return HUGE_VALF;
  Values_Ptr fv = values[draw->colourIdx];
  return (*fv)[idx];
}

FloatValues* GeomData::valueData(unsigned int vidx)
{
  if (values.size() <= vidx || !values[vidx]->size()) return NULL;
  return values[vidx].get();
}

float GeomData::valueData(unsigned int vidx, unsigned int idx)
{
  FloatValues* fv = valueData(vidx);
  return fv ? (*fv)[idx] : HUGE_VALF;
}

Geometry::Geometry(DrawState& drawstate) : view(NULL), elements(0),
                       cached(NULL), drawstate(drawstate),
                       allhidden(false), internal(false), unscale(false),
                       type(lucMinType), total(0), redraw(true), reload(true)
{
  drawcount = 0;
  type = lucMinType;
}

Geometry::~Geometry()
{
  //Free GeomData elements
  clear();
}

//Virtuals to implement
void Geometry::close() //Called on quit or gl context destroy
{
  //Ensure cache cleared
  cached = NULL;
}

void Geometry::clear()
{
  total = 0;
  reload = true;
  geom.clear();
  
  //Ensure cache cleared
  cached = NULL;
}

void Geometry::remove(DrawingObject* draw)
{
  //Same as clear but for specific drawing object
  reload = true;
  for (int i = geom.size()-1; i>=0; i--)
  {
    if (draw == geom[i]->draw)
    {
      total -= geom[i]->count();
      //Now using shared_ptr so no need to delete
      geom.erase(geom.begin()+i);
      if (hidden.size() > (unsigned int)i) hidden.erase(hidden.begin()+i);
    }
  }
}

void Geometry::clearValues(DrawingObject* draw, std::string label)
{
  reload = true;
  for (auto g : geom)
  {
    if (draw == g->draw)
    {
      if (label == "labels")
      {
        //Also used to clear labels
        g->labels.clear();
        continue;
      }

      for (auto vals : g->values)
        if (label.length() == 0 || vals->label == label)
          vals->clear();
    }
  }
}

void Geometry::clearData(DrawingObject* draw, lucGeometryDataType dtype)
{
  reload = true;
  for (auto g : geom)
  {
    if (draw == g->draw)
    {
      g->dataContainer(dtype)->clear();
    }
  }
  //std::cout << "CLEARED " << dtype << std::endl;
}

void Geometry::compareMinMax(float* min, float* max)
{
  //Compare passed min/max with min/max of all geometry
  //(Used by parent to get bounds of sub-renderer objects)
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    compareCoordMinMax(min, max, geom[i]->min);
    compareCoordMinMax(min, max, geom[i]->max);
    //Also update global min/max
    compareCoordMinMax(drawstate.min, drawstate.max, geom[i]->min);
    compareCoordMinMax(drawstate.min, drawstate.max, geom[i]->max);
  }
  //Update global bounding box size
  getCoordRange(drawstate.min, drawstate.max, drawstate.dims);
}

void Geometry::dump(std::ostream& csv, DrawingObject* draw)
{
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    if (geom[i]->draw == draw)
    {
      if (type == lucVolumeType)
      {
        //Dump colourValue data only
        std::cout << "Collected " << geom[i]->colourData()->size() << " values (" << i << ")" << std::endl;
        for (unsigned int c=0; c < geom[i]->colourData()->size(); c++)
        {
          csv << geom[i]->colourData(c) << std::endl;
        }

      }
      else
      {
        std::cout << "Collected " << geom[i]->count() << " vertices/values (" << i << ")" << std::endl;
        //Only supports dump of vertex, vector and scalar colour value
        for (unsigned int v=0; v < geom[i]->count(); v++)
        {
          csv << geom[i]->render->vertices[v][0] << ',' <<  geom[i]->render->vertices[v][1] << ',' << geom[i]->render->vertices[v][2];

          if (geom[i]->colourData() && geom[i]->colourData()->size() == geom[i]->count())
            csv << ',' << geom[i]->colourData(v);
          if (geom[i]->render->vectors.size() > v)
            csv << ',' << geom[i]->render->vectors[v][0] << ',' <<  geom[i]->render->vectors[v][1] << ',' << geom[i]->render->vectors[v][2];

          csv << std::endl;
        }
      }
    }
  }
}

void Geometry::jsonWrite(DrawingObject* draw, json& obj)
{
  //Export geometry to json
}

void Geometry::jsonExportAll(DrawingObject* draw, json& obj, bool encode)
{
  //Export all geometry to json
  //TODO: json model needs to store value data separately by label
  std::string& typelabel = GeomData::names[type];
  if (typelabel == "quads") typelabel = "triangles";
  json array;
  //Append to existing if found
  if (obj.count(typelabel) > 0 && obj[typelabel].type() == json::value_t::array)
    array = obj[typelabel];

  int dsizes[lucMaxDataType+1] = {3, 3, 3,
                                1, 1, 1, 1, 1,
                                1, 1, 1, 1,
                                1, 2, 1, 1, 1
                               };
  for (unsigned int index = 0; index < geom.size(); index++)
  {
    if (geom[index]->draw == draw && drawable(index))
    {
      std::cerr << "Collecting data for " << draw->name() << ", " << geom[index]->count() << " vertices (" << index << ")" << std::endl;
      json data;
      for (int data_type=lucMinDataType; data_type<=lucMaxDataType; data_type++)
      {
        DataContainer* dat = geom[index]->dataContainer((lucGeometryDataType)data_type);
        if (!dat)
        {
          //TODO: export all values with labels separately
          //Check in values and use if label matches
          for (auto vals : geom[index]->values)
          {
            if (vals->label == GeomData::datalabels[data_type])
              dat = (FloatValues*)vals.get();
          }
          //Use default colour values for "values" until multiple data sets supported in WebGL viewer
          if (!dat && data_type == lucColourValueData)
            dat = geom[index]->colourData();

        }
        if (!dat) continue;
        json el;

        unsigned int length = dat->size() * sizeof(float);

        if (length > 0)
        {
          el["size"] = dsizes[data_type];
          el["count"] = (int)dat->size();
          if (encode)
            el["data"] = base64_encode(reinterpret_cast<const unsigned char*>(dat->ref(0)), length);
          else
          {
            //TODO: Support export of custom value data (not with pre-defined label)
            //      include minimum/maximum/label fields
            json values;
            for (unsigned int j=0; j<dat->size(); j++)
            {
              if (data_type == lucIndexData || data_type == lucRGBAData)
                values.push_back((int)*reinterpret_cast<unsigned int*>(dat->ref(j)));
              else
                values.push_back((float)*reinterpret_cast<float*>(dat->ref(j)));
            }
            el["data"] = values;
          }
          data[GeomData::datalabels[data_type]] = el;
          std::cout << " -- " <<  GeomData::datalabels[data_type] << " * " << length << " : " << dat->minimum << " - " << dat->maximum << std::endl;
          if (dat->minimum < dat->maximum)
          {
            el["minimum"] = dat->minimum;
            el["maximum"] = dat->maximum;
          }
        }
      }

      //for grid surfaces... TODO: Should just use dims prop, directly exported
      if (geom[index]->width) data["width"] = (int)geom[index]->width;
      if (geom[index]->height) data["height"] = (int)geom[index]->height;

      array.push_back(data);
    }
  }

  if (array.size() > 0)
    obj[typelabel] = array;
}

bool Geometry::hide(unsigned int idx)
{
  if (idx >= geom.size()) return false;
  if (hidden[idx]) return false;
  hidden[idx] = true;
  redraw = true;
  return true;
}

void Geometry::hideShowAll(bool hide)
{
  for (unsigned int i=0; i<hidden.size(); i++)
  {
    hidden[i] = hide;
    //geom[i]->draw->properties.data["visible"] = false;
  }
  allhidden = hide;
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

void Geometry::showObj(DrawingObject* draw, bool state)
{
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    //std::cerr << i << " owned by object " << geom[i]->draw->id << std::endl;
    if (!draw || geom[i]->draw == draw)
    {
      if (draw) hidden[i] = !state;
      geom[i]->draw->properties.data["visible"] = state;
    }
  }
}

void Geometry::setValueRange(DrawingObject* draw)
{
  for (auto g : geom)
  {
    if (g->colourData() && (!draw || g->draw == draw))
    {
      //Get local min and max for each element from colourValues
      g->colourData()->minimum = HUGE_VAL;
      g->colourData()->maximum = -HUGE_VAL;
      for (unsigned int v=0; v < g->colourData()->size(); v++)
      {
        // Check min/max against each value
        if (g->colourData(v) > g->colourData()->maximum) g->colourData()->maximum = g->colourData(v);
        if (g->colourData(v) < g->colourData()->minimum) g->colourData()->minimum = g->colourData(v);
      }
    }
  }
}

void Geometry::redrawObject(DrawingObject* draw)
{
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    if (geom[i]->draw == draw)
    {
      debug_print("Reloading object: %s\n", draw->name().c_str());
      //Flag reload of texture
      if (geom[i]->texture) geom[i]->texture->texture->width = 0;
      reload = true;
      return;
    }
  }
}

void Geometry::init() //Called on GL init
{
  reload = true;
}

void Geometry::setState(unsigned int i, Shader* prog)
{
  //NOTE: Transparent triangle surfaces/points are drawn as a single object so 
  //      no per-object state settings work, state applied is that of first in list
  GL_Error_Check;
  if (geom.size() <= i) return;
  DrawingObject* draw = geom[i]->draw;

  //Only set state when object changes
  if (draw == cached) return;
  cached = draw;

  bool lighting = geom[i]->draw->properties["lit"];
  //Don't light surfaces in 2d models
  if ((type == lucTriangleType || type == lucGridType) && !view->is3d && !internal) lighting = false;

  //Global/Local draw state
  if (geom[i]->draw->properties["cullface"])
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  //Surface specific options
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  if (type == lucTriangleType || type == lucGridType || TriangleBased(type))
  {
    //Disable lighting and polygon faces in wireframe mode
    if (geom[i]->draw->properties["wireframe"])
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      lighting = false;
      glDisable(GL_CULL_FACE);
    }

    if (geom[i]->draw->properties["flat"])
      glShadeModel(GL_FLAT);
    else
      glShadeModel(GL_SMOOTH);
  }
  else
  {
    //Flat disables lighting for non surface types
    if (geom[i]->draw->properties["flat"]) lighting = false;
    glEnable(GL_BLEND);
  }

  //Default line width
  float lineWidth = (float)geom[i]->draw->properties["linewidth"] * drawstate.scale2d; //Include 2d scale factor
  glLineWidth(lineWidth);

  //Disable depth test by default for 2d lines, otherwise enable
  bool depthTestDefault = (view->is3d || type != lucLineType);
  if (geom[i]->draw->properties.getBool("depthtest", depthTestDefault))
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  
  if (!lighting)
    glDisable(GL_LIGHTING);
  else
    glEnable(GL_LIGHTING);

  //Textured?
  TextureData* texture = draw->useTexture(geom[i]->texture);
  GL_Error_Check;
  if (texture)
  {
    //Combine texture with colourmap: Requires modulate mode
    //GL_MODULATE/BLEND/REPLACE/DECAL
    if (geom[i]->colourCount() > 0)
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    else
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);  
  }

  //Replace the default colour with a json value if present
  draw->colour = draw->properties.getColour("colour", draw->colour.r, draw->colour.g, draw->colour.b, draw->colour.a);

  //Uniforms for shader programs
  if (prog && prog->program > 0)
  {
    prog->use();
    //Per-object "opacity" overrides global default if set
    //"alpha" is multiplied to affect all objects
    float opacity = (float)geom[i]->draw->properties["alpha"];
    //Apply global 'opacity' only if no per-object setting (which is applied with colour)
    if (!geom[i]->draw->properties.has("opacity"))
      opacity *= (float)drawstate.global("opacity");
    bool allopaque = !drawstate.global("sort");
    if (allopaque) opacity = 1.0;
    prog->setUniformf("uOpacity", opacity);
    prog->setUniformi("uLighting", lighting);
    prog->setUniformf("uBrightness", geom[i]->draw->properties["brightness"]);
    prog->setUniformf("uContrast", geom[i]->draw->properties["contrast"]);
    prog->setUniformf("uSaturation", geom[i]->draw->properties["saturation"]);
    prog->setUniformf("uAmbient", geom[i]->draw->properties["ambient"]);
    prog->setUniformf("uDiffuse", geom[i]->draw->properties["diffuse"]);
    prog->setUniformf("uSpecular", geom[i]->draw->properties["specular"]);
    prog->setUniform3f("uLightPos", geom[i]->draw->properties["lightpos"]);
    prog->setUniformi("uTextured", texture && texture->unit >= 0);
    prog->setUniformf("uOpaque", allopaque);

    if (texture)
      prog->setUniform("uTexture", (int)texture->unit);

    if (geom[i]->render->normals.size() == 0 && (type == lucTriangleType || TriangleBased(type)))
      prog->setUniform("uCalcNormal", 1);
    else
      prog->setUniform("uCalcNormal", 0);

    if (prog->uniforms["uClipMin"])
    {
      Vec3d clipMin = Vec3d(-HUGE_VALF, -HUGE_VALF, -HUGE_VALF);
      Vec3d clipMax = Vec3d(HUGE_VALF, HUGE_VALF, HUGE_VALF);
      if (geom[i]->draw->properties["clip"])
      {
        clipMin = Vec3d((float)geom[i]->draw->properties["xmin"],
                        (float)geom[i]->draw->properties["ymin"],
                        view->is3d ? (float)geom[i]->draw->properties["zmin"] : -HUGE_VALF);
        clipMax = Vec3d((float)geom[i]->draw->properties["xmax"],
                        (float)geom[i]->draw->properties["ymax"],
                        view->is3d ? (float)geom[i]->draw->properties["zmax"] : HUGE_VALF);
        if (geom[i]->draw->properties["clipmap"])
        {
          Vec3d dims(drawstate.dims);
          Vec3d dmin(drawstate.min);
          clipMin *= dims;
          clipMin += dmin;
          clipMax *= dims;
          clipMax += dmin;
        }
        //printf("Dimensions %f,%f,%f - %f,%f,%f\n", drawstate.min[0], drawstate.min[1], 
        //       drawstate.min[2], drawstate.max[0], drawstate.max[1], drawstate.max[2]);
        //printf("Clipping %s %f,%f,%f - %f,%f,%f\n", geom[i]->draw->name().c_str(), 
        //       clipMin[0], clipMin[1], clipMin[2], clipMax[0], clipMax[1], clipMax[2]);
      }

      glUniform3fv(prog->uniforms["uClipMin"], 1, clipMin.ref());
      glUniform3fv(prog->uniforms["uClipMax"], 1, clipMax.ref());
    }
  }
  GL_Error_Check;
}

void Geometry::display()
{
  //Skip if view not open or nothing to draw
  if (!view || !view->width || !total) return;

  //Draw data, then labels
  GL_Error_Check;

  //Default to no shaders
  glUseProgram(0);

  //Clear cached object
  cached = NULL;

  //Anything to draw?
  unsigned int newcount = 0;
  for (unsigned int i=0; i < geom.size(); i++)
  {
    if (drawable(i))
      newcount++;

    //Opacity flag - default transparency enabled
    geom[i]->opaque = false;

    //Per-object wireframe works only when drawing opaque objects
    //(can't set per-objects properties when all triangles collected and sorted)
    geom[i]->opaque = (geom[i]->draw->properties["wireframe"] || 
                       geom[i]->draw->properties["opaque"]);

    //If using a colourmap without transparency, and no opacity prop, flag opaque
    if (geom[i]->draw->colourMap && geom[i]->values.size() > geom[i]->draw->colourIdx && 
        geom[i]->draw->colourMap->opaque && 
        (float)geom[i]->draw->properties["opacity"] == 1.0 &&
        (float)geom[i]->draw->properties["alpha"] == 1.0)
    {
      geom[i]->opaque = true;
    }

  }

  if (reload || redraw || newcount != drawcount)
  {
    //Prevent update while sorting
    std::lock_guard<std::mutex> guard(sortmutex);
    update();
    reload = false;
  }

  //Skip draw for internal sub-renderers, will be done by parent renderer
  if (!internal && newcount)
  {
    draw();

    labels();
  }

  drawcount = newcount;
  redraw = false;
  GL_Error_Check;
}

void Geometry::update()
{
  //Update virtual to be implemented
}

void Geometry::draw()
{
  //Draw virtual to be implemented
}

void Geometry::sort()
{
  //Threaded sort virtual to be implemented
}

void Geometry::labels()
{
  //Print labels
  glPushAttrib(GL_ENABLE_BIT);
  glDisable(GL_DEPTH_TEST);  //No depth testing
  glDisable(GL_LIGHTING);  //No lighting
  glUseProgram(0);
  for (unsigned int i=0; i < geom.size(); i++)
  {
    if (drawable(i) && geom[i]->labels.size() > 0)
    {
      std::string font = geom[i]->draw->properties["font"];
      if (drawstate.scale2d != 1.0 && font != "vector")
        geom[i]->draw->properties.data["font"] = "vector"; //Force vector if downsampling
      //Default to object colour (if fontcolour provided will replace)
      Colour colour = Colour(geom[i]->draw->properties["colour"]);
      glColor3ubv(colour.rgba);
      drawstate.fonts.setFont(geom[i]->draw->properties, "small", 1.0, drawstate.scale2d);

      for (unsigned int j=0; j < geom[i]->labels.size(); j++)
      {
        float* p = geom[i]->render->vertices[j];
        //debug_print("Labels for %d - %d : %s\n", i, j, geom[i]->labels[j].c_str());
        std::string labstr = geom[i]->labels[j];
        if (labstr.length() == 0) continue;
        //Preceed with ! for right align, | for centre
        float shift = drawstate.fonts.printWidth("XX")*FONT_SCALE_3D/view->scale[1]; //Vertical shift
        char alignchar = ' ';
        char alignchar2 = ' ';
        if (labstr.length() > 1) alignchar = labstr.at(0);
        if (labstr.length() > 2) alignchar2 = labstr.at(1);
        //Left/right/centre
        int align = -1;
        if (alignchar == '!') align = 1;
        if (alignchar == '|') align = 0;

        //Reverse v-shift
        if (alignchar == '^' || alignchar2 == '^')
          shift = -0.5*shift;
        //Zero vshift
        if (alignchar == '_' || alignchar2 == '_')
          shift = 0.0;

        //Strip align chars
        if (alignchar2 == '^' || alignchar2 == '_')
          labstr = labstr.substr(2);
        else if (alignchar == '!' || alignchar == '|' || alignchar == '^' || alignchar == '_')
          labstr = labstr.substr(1);

        if (labstr.length() > 0)
        {
          drawstate.fonts.print3dBillboard(p[0], p[1]-shift, p[2], labstr.c_str(), align, view->scale);
        }
      }
    }
  }
  glPopAttrib();
}

//Returns true if passed geometry element index is drawable
//ie: has data, in range, not hidden and in viewport object list
bool Geometry::drawable(unsigned int idx)
{
  if (!geom[idx]->draw->properties["visible"]) return false;
  //Within bounds and not hidden
  if (idx < geom.size() && geom[idx]->count() > 0 && !hidden[idx])
  {
    //Not filtered by viewport?
    if (!view->filtered) return true;

    //Search viewport object set
    if (view->hasObject(geom[idx]->draw))
      return true;

  }
  return false;
}

std::vector<Geom_Ptr> Geometry::getAllObjects(DrawingObject* draw)
{
  //Get passed object's data store
  std::vector<Geom_Ptr> geomlist;
  for (unsigned int i=0; i<geom.size(); i++)
    if (geom[i]->draw == draw)
      geomlist.push_back(geom[i]);
  return geomlist;
}

Geom_Ptr Geometry::getObjectStore(DrawingObject* draw)
{
  //Get passed object's most recently added data store
  for (int i=geom.size()-1; i>=0; i--)
    if (geom[i]->draw == draw) return geom[i];
  return nullptr;
}

Geom_Ptr Geometry::add(DrawingObject* draw)
{
  Geom_Ptr geomdata = std::make_shared<GeomData>(draw, type);
  geom.push_back(geomdata);
  if (hidden.size() < geom.size()) hidden.push_back(allhidden);
  //if (allhidden) draw->properties.data["visible"] = false;
  //debug_print("%d NEW %s DATA STORE CREATED FOR %s size %d ptr %p hidden %d\n", geom.size(), GeomData::names[type].c_str(), draw->name().c_str(), geom.size(), geomdata, allhidden);
  return geomdata;
}

void Geometry::setup(View* vp, float* min, float* max)
{
  view = vp;

  if (!min || !max) return;

  //Iterate the selected viewport's drawing objects
  //Apply geometry bounds from all object data within this viewport
  for (unsigned int o=0; o<view->objects.size(); o++)
    if (view->objects[o]->properties["visible"])
      objectBounds(view->objects[o], min, max);
  //printf("Final bounding dims...%f,%f,%f - %f,%f,%f\n", min[0], min[1], min[2], max[0], max[1], max[2]);
}

void Geometry::calcDistanceRange(bool eyePlane)
{
  //Calculate min/max distances from viewer
  float min[3], max[3];
  //float min[3] = {HUGE_VALF, HUGE_VALF, HUGE_VALF}, max[3] = {-HUGE_VALF, -HUGE_VALF, -HUGE_VALF};
  Properties::toArray<float>(view->properties["min"], min, 3);
  Properties::toArray<float>(view->properties["max"], max, 3);

  //Iterate the selected viewport's drawing objects
  //Apply geometry bounds from all object data within this viewport
  for (unsigned int o=0; o<view->objects.size(); o++)
    if (view->objects[o]->properties["visible"])
      objectBounds(view->objects[o], min, max);

  //Calculate viewer distance using current modelview
  view->getMinMaxDistance(min, max, eyePlane);

  //printf("Final bounding dims...%f,%f,%f - %f,%f,%f\n", min[0], min[1], min[2], max[0], max[1], max[2]);
  //printf("Final view range...%f - %f\n", view->mindist, view->maxdist);
}

void Geometry::objectBounds(DrawingObject* draw, float* min, float* max)
{
  if (!min || !max) return;
  //Get geometry bounds from all object data
  for (unsigned int g=0; g<geom.size(); g++)
  {
    //If no range, must calculate
    for (int i=0; i<3; i++)
    {
      if (std::isinf(geom[g]->max[i]) || std::isinf(geom[g]->min[i]))
      {
        geom[g]->calcBounds();
        //printf("No bounding dims provided for object %s (el %d), calculated ...%f,%f,%f - %f,%f,%f\n", geom[g]->draw->name().c_str(), g, 
        //        geom[g]->min[0], geom[g]->min[1], geom[g]->min[2], geom[g]->max[0], geom[g]->max[1], geom[g]->max[2]);
        break;
      }
    }

    if (geom[g]->draw == draw)
    {
      compareCoordMinMax(min, max, geom[g]->min);
      compareCoordMinMax(min, max, geom[g]->max);
      //printf("Applied bounding dims from object %s...%f,%f,%f - %f,%f,%f\n", geom[g]->draw->name().c_str(), 
      //       geom[g]->min[0], geom[g]->min[1], geom[g]->min[2], geom[g]->max[0], geom[g]->max[1], geom[g]->max[2]);
    }

    //Apply object rotation/translation?
  }
}

Geom_Ptr Geometry::read(DrawingObject* draw, unsigned int n, lucGeometryDataType dtype, const void* data, int width, int height, int depth)
{
  draw->skip = false;  //Enable object (has geometry now)
  Geom_Ptr geomdata = nullptr;
  //Get passed object's most recently added data store
  geomdata = getObjectStore(draw);

  //If dimensions specified, check if full dataset loaded
  assert(dtype <= lucMaxDataType);
  if (geomdata && geomdata->width > 0 && geomdata->height > 0)
  {
    DataContainer* container = geomdata->dataContainer(dtype);
    unsigned int size = geomdata->width * geomdata->height * (geomdata->depth > 0 ? geomdata->depth : 1);
    if (size == container->count())
      geomdata = nullptr;
    //if (!geomdata) printf("LOAD COMPLETE dtype %d size %u ==  %u / %u == %u\n", dtype, size, container->size(), 
    //                       container->unitsize(), container->count());
  }

  //Allow spec width/height/depth in properties
  if (!geomdata || geomdata->count() == 0)
  {
    float dims[3];
    Properties::toArray<float>(draw->properties["dims"], dims, 3);
    if (width == 0) width = dims[0];
    if (height == 0) height = dims[1];
    if (depth == 0) depth = dims[2];
  }

  //Create new data store if required, save in drawing object and Geometry list
  if (!geomdata)
    geomdata = add(draw);

  //Now ready to load geometry data into store
  read(geomdata, n, dtype, data, width, height, depth);

  return geomdata; //Return data store pointer
}

void Geometry::read(Geom_Ptr geomdata, unsigned int n, lucGeometryDataType dtype, const void* data, int width, int height, int depth)
{
  //Set width & height if provided
  if (width) geomdata->width = width;
  if (height) geomdata->height = height;
  if (depth) geomdata->depth = depth;

  //Update the default type property on first read
  if (geomdata->count() == 0 && !geomdata->draw->properties.has("geometry"))
    geomdata->draw->properties.data["geometry"] = GeomData::names[type];

  //Read the data
  if (n > 0)
    geomdata->dataContainer(dtype)->read(n, data);

  if (dtype == lucVertexData)
  {
    total += n;

    //Update bounds on single vertex reads (except labels)
    if (n == 1 && type != lucLabelType) // && !internal)
    {
      if (unscale)
      {
        //Some internal geom is pre-scaled by any global scaling factors to avoid distortion,
        //so we need to undo scaling before applying to bounding box.
        float* arr = (float*)data;
        std::array<float,3> unscaled;
        unscaled = {arr[0]*iscale[0], arr[1]*iscale[1], arr[2]*iscale[2]};
        geomdata->checkPointMinMax(unscaled.data());
      }
      else
        geomdata->checkPointMinMax((float*)data);
    }
  }
}

Geom_Ptr Geometry::read(DrawingObject* draw, unsigned int n, const void* data, std::string label)
{
  //Read into given label - for value data only

  //Get passed object's most recently added data store
  Geom_Ptr geomdata = getObjectStore(draw);
  //Create new data store if required, save in drawing object and Geometry list
  if (!geomdata)
    geomdata = add(draw);

  return read(geomdata, n, data, label);
}

Geom_Ptr Geometry::read(Geom_Ptr geom, unsigned int n, const void* data, std::string label)
{
  //Read into given label - for value data only

  //Find labelled value store
  Values_Ptr store = nullptr;
  for (auto vals : geom->values)
  {
    if (vals->label == label)
      store = vals;
  }

  //Create value store if required
  if (!store)
  {
    store = std::make_shared<FloatValues>();
    geom->values.push_back(store);
    store->label = label;
    //debug_print(" -- NEW VALUE STORE CREATED FOR %s label %s count %d N %d ptr %p\n", geom->draw->name().c_str(), label.c_str(), geom->values.size(), n, store);
  }

  //Read the data
  if (n > 0) store->read(n, data);

  return geom; //Return data store pointer
}

//Read a triangle with optional resursive splitting and y/z swap
void Geometry::addTriangle(DrawingObject* obj, float* a, float* b, float* c, int level, bool swapY)
{
  level--;
  //Experimental: splitting triangles by size
  //float a_b[3], a_c[3], b_c[3];
  //vectorSubtract(a_b, a, b);
  //vectorSubtract(a_c, a, c);
  //vectorSubtract(b_c, b, c);
  //float max = 100000; //aview->model_size / 100.0;
  //printf("%f\n", max); getchar();

  if (level <= 0) // || (dotProduct(a_b,a_b) < max && dotProduct(a_c,a_c) < max && dotProduct(b_c,b_c) < max))
  {
    float A[3] = {a[0], a[2], a[1]};
    float B[3] = {b[0], b[2], b[1]};
    float C[3] = {c[0], c[2], c[1]};
    if (swapY)
    {
      a = A;
      b = B;
      c = C;
    }

    //Read the triangle
    read(obj, 1, lucVertexData, a);
    read(obj, 1, lucVertexData, b);
    read(obj, 1, lucVertexData, c);
  }
  else
  {
    //Process a triangle into 4 sub-triangles
    float ab[3] = {0.5f*(a[0]+b[0]), 0.5f*(a[1]+b[1]), 0.5f*(a[2]+b[2])};
    float ac[3] = {0.5f*(a[0]+c[0]), 0.5f*(a[1]+c[1]), 0.5f*(a[2]+c[2])};
    float bc[3] = {0.5f*(b[0]+c[0]), 0.5f*(b[1]+c[1]), 0.5f*(b[2]+c[2])};

    addTriangle(obj, a, ab, ac, level);
    addTriangle(obj, ab, b, bc, level);
    addTriangle(obj, ac, bc, c, level);
    addTriangle(obj, ab, bc, ac, level);
  }
}

void Geometry::setupObject(DrawingObject* draw)
{
  //Scan all data for min/max
  std::vector<float> minimums;
  std::vector<float> maximums;

  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (geom[i]->draw == draw)
    {
      for (unsigned int d=0; d<geom[i]->values.size(); d++)
      {
        //Store max/min per value index
        if (minimums.size() <= d)
        {
          minimums.push_back(HUGE_VAL);
          maximums.push_back(-HUGE_VAL);
        }

        Values_Ptr flvals = geom[i]->values[d];
        FloatValues& fvals = *flvals;
        for (unsigned int c=0; c < fvals.size(); c++)
        {
          if (fvals[c] < minimums[d])
            minimums[d] = fvals[c];
          if (fvals[c] > maximums[d])
            maximums[d] = fvals[c];
        }
      }
    }
  }

  for (unsigned int i=0; i<geom.size(); i++)
  {
    if (geom[i]->draw == draw)
    {
      for (unsigned int d=0; d<geom[i]->values.size(); d++)
      {
        if (minimums[d] > maximums[d]) continue;
        //std::cout << "Updating data range for " << draw->name() << " == " << minimums[d] << " - " << maximums[d] << std::endl;
        Values_Ptr fvals = geom[i]->values[d];
        fvals->setup(minimums[d], maximums[d]); //, label);
      }
    }
  }
}

void Geometry::insertFixed(Geometry* fixed)
{
  if (geom.size() > 0) return; //Not permitted to load fixed data if any existing data loaded already

  for (unsigned int i=0; i<fixed->geom.size(); i++)
  {
    if (geom.size() == i)
      add(fixed->geom[i]->draw); //Insert new if not enough records

    //Create a shallow copy of member content
    *geom[i] = *fixed->geom[i];

    //std::cout << i << ") IMPORTED FIXED DATA RECORDS FOR " << fixed->geom[i]->draw->name() << ", " << fixed->geom[i]->count() << " verts, value entries = " << fixed->geom[i]->values.size() << std::endl;
  }

  //Update total
  total += fixed->total;
  //std::cout << fixed->total << " + NEW TOTAL == " << total << std::endl;
}

bool Geometry::inFixed(DataContainer* block0)
{
  //Return true if a data block found in fixed data set
  for (Geom_Ptr p : geom)
  {
    for (unsigned int data_type=0; data_type <= lucMaxDataType; data_type++)
    {
      //Get the data entries and compare
      DataContainer* block1 = p->dataContainer((lucGeometryDataType)data_type);
      if (block1 && block1 == block0) return true;
    }
    for (unsigned int j=0; j<p->values.size(); j++)
    {
      DataContainer* block1 = (DataContainer*)p->values[j].get();
      if (block1 && block1 == block0) return true;
    }
  }
  return false;
}

void Geometry::label(DrawingObject* draw, const char* labels)
{
  //Get passed object's most recently added data store and add vertex labels (newline separated)
  Geom_Ptr geomdata = getObjectStore(draw);
  if (!geomdata) return;

  //Clear if NULL
  if (labels == NULL)
  {
    geomdata->labels.clear();
  }
  else
  {
    //Split by newlines
    std::istringstream iss(labels);
    std::string line;
    while(std::getline(iss, line))
      geomdata->label(line);
  }
}

void Geometry::label(DrawingObject* draw, std::vector<std::string> labels)
{
  //Get passed object's most recently added data store and add vertex labels (newline separated)
  Geom_Ptr geomdata = getObjectStore(draw);
  if (!geomdata) return;

  //Load from vector 
  for (auto line : labels)
  {
    geomdata->label(line);
  }
}

void Geometry::print()
{
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    std::cout << GeomData::names[type] << " [" << i << "] - "
              << (drawable(i) ? "shown" : "hidden") << std::endl;
  }
}

json Geometry::getDataLabels(DrawingObject* draw)
{
  //Iterate through all geometry of given object(id) and print
  //the index and label of the associated value data sets
  //(used for colouring and filtering)
  json list = json::array();
  DrawingObject* last = NULL;
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    if ((!draw || geom[i]->draw == draw) && geom[i]->draw != last)
    {
      for (unsigned int v = 0; v < geom[i]->values.size(); v++)
      {
        std::stringstream ss;
        json entry;
        entry["label"] = geom[i]->values[v]->label;
        entry["minimum"] = geom[i]->values[v]->minimum;
        entry["maximum"] = geom[i]->values[v]->maximum;
        entry["size"] = geom[i]->values[v]->size();
        list.push_back(entry);
      }
      //No need to repeat for every element as they will all have the same data sets per object
      last = geom[i]->draw;
    }
  }
  return list;
}

//Dumps colourmapped data to image
void Geometry::toImage(unsigned int idx)
{
  ColourLookup& getColour = geom[idx]->colourCalibrate();
  int width = geom[idx]->width;
  if (width == 0) width = 256;
  int height = geom[idx]->height;
  if (height == 0) height = geom[idx]->colourData()->size() / width;
  char path[FILE_PATH_MAX];
  int pixel = 3;
  ImageData image(width, height, pixel);
  // Read the pixels
  for (int y=0; y<height; y++)
  {
    for (int x=0; x<width; x++)
    {
      //printf("%f\n", geom[idx]->colourData()[y * width + x]);
      Colour c;
      getColour(c, y * width + x);
      image.pixels[y * width*pixel + x*pixel + 0] = c.r;
      image.pixels[y * width*pixel + x*pixel + 1] = c.g;
      image.pixels[y * width*pixel + x*pixel + 2] = c.b;
    }
  }
  //Write data to image file
  sprintf(path, "%s.%05d", geom[idx]->draw->name().c_str(), idx);
  image.write(path);
}

void Geometry::setTexture(DrawingObject* draw, ImageLoader* tex)
{
  Geom_Ptr geomdata = getObjectStore(draw);
  geomdata->texture = tex;
  //std::cout << "SET TEXTURE ON " << draw->name() << std::endl;
  //Must be opaque to draw with own texture
  geomdata->opaque = true;
}

void Geometry::loadTexture(DrawingObject* draw, GLubyte* data, GLuint width, GLuint height, GLuint channels, bool flip)
{
  Geom_Ptr geomdata = getObjectStore(draw);
  if (!geomdata->texture) geomdata->texture = new ImageLoader(flip);
  geomdata->texture->load(data, width, height, channels);
  //std::cout << "LOAD TEXTURE " << width << " x " << height << " x " << channels << " ON " << draw->name() << std::endl;
  //Must be opaque to draw with own texture (TODO: obj properties in shader)
  geomdata->opaque = true;
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

Quaternion Geometry::vectorRotation(Vec3d rvector)
{
  // Get Rotattion quaternion to orient along rvector
  //...Want to align our z-axis to point along arrow vector:
  // axis of rotation = (z x vec)
  // cosine of angle between vector and z-axis = (z . vec) / |z|.|vec| *
  rvector.normalise();
  float rangle = RAD2DEG * rvector.angle(Vec3d(0.0, 0.0, 1.0));
  Quaternion rot;
  //Axis of rotation = vec x [0,0,1] = -vec[1],vec[0],0
  if (rangle == 180.0)
  {
    rot.y = 1;
    rot.w = 0.0;
  }
  else if (rangle > 0.0)
  {
    rot.fromAxisAngle(Vec3d(-rvector.y, rvector.x, 0), rangle);
  }
  return rot;
}

//////////////////////////////////
// Draws a 3d vector
// pos: centre position at which to draw vector
// scale: scaling factor for entire vector
// radius0: radius of cylinder at arrow base,
//          if zero a default value is automatically calculated based on length & scale multiplied by radius1
// radius1: radius of cylinder at arrow head, or ratio for calculating radius from length if radius0==0
// head_scale: scaling factor for head radius compared to shaft, if zero then no arrow head is drawn
// segment_count: number of primitives to draw circular geometry with, 16 is usually a good default
#define RADIUS_DEFAULT_RATIO 0.02   // Default radius as a ratio of length
void Geometry::drawVector(DrawingObject *draw, const Vec3d& translate, const Vec3d& vector, float scale, float radius0, float radius1, float head_scale, int segment_count)
{
  //Setup orientation using alignment vector
  Quaternion rot = vectorRotation(vector);

  //Scale vector
  Vec3d vec = vector * scale;

  std::vector<unsigned int> indices;

  // Get circle coords
  drawstate.cacheCircleCoords(segment_count);

  // Render a 3d arrow, cone with base for head, cylinder for shaft

  // Length of the drawn vector = vector magnitude * scaling factor
  float length = vec.magnitude();
  float halflength = length*0.5;
  if (length < FLT_EPSILON || std::isinf(length)) return;

  // Default shaft radius based on length of vector (2%)
  if (radius0 == 0)
  {
    if (radius1 == 0) radius1 = RADIUS_DEFAULT_RATIO;
    radius0 = radius1 = length * radius1;
  }
  if (radius1 == 0) radius1 = radius0;

  // Head radius based on shaft radius or length
  float head_radius = 0.0;
  // If [0,1], ratio to length (diameter to length, so * 0.5)
  // otherwise ratio to radius (> 1)
  if (head_scale > 0 && head_scale < 1.0)
    head_radius = 0.5 * head_scale * length;
  else if (head_scale > 0)
    head_radius = head_scale * radius1;

  // Vector is centered on pos[x,y,z]
  // Translate to the point of arrow -> position + vector/2
  float headD = head_radius*2;
  //Output is lines only if using very low quality setting
  if (segment_count < 4)
  {
    // Draw Line
    Vec3d vertex0 = Vec3d(0,0,-halflength);
    Vec3d vertex = translate + rot * vertex0;
    read(draw, 1, lucVertexData, vertex.ref());
    vertex0.z = halflength;
    vertex = translate + rot * vertex0;
    read(draw, 1, lucVertexData, vertex.ref());
    return;
  }
  else if (length > headD)
  {
    int v;
    for (v=0; v <= segment_count; v++)
    {
      int vertex_index = getVertexIdx(draw);

      // Base of shaft
      Vec3d vertex0 = Vec3d(radius0 * drawstate.x_coords[v], radius0 * drawstate.y_coords[v], -halflength); // z = Shaft length to base of head
      Vec3d vertex = translate + rot * vertex0;

      //Read triangle vertex, normal
      read(draw, 1, lucVertexData, vertex.ref());
      Vec3d normal = rot * Vec3d(drawstate.x_coords[v], drawstate.y_coords[v], 0);
      //normal.normalise();
      read(draw, 1, lucNormalData, normal.ref());

      // Top of shaft
      Vec3d vertex1 = Vec3d(radius1 * drawstate.x_coords[v], radius1 * drawstate.y_coords[v], -headD+halflength);
      vertex = translate + rot * vertex1;

      //Read triangle vertex, normal
      read(draw, 1, lucVertexData, vertex.ref());
      read(draw, 1, lucNormalData, normal.ref());

      //Triangle strip indices
      if (v > 0)
      {
        //First tri
        indices.push_back(vertex_index-2);
        indices.push_back(vertex_index-1);
        indices.push_back(vertex_index);
        //Second tri
        indices.push_back(vertex_index-1);
        indices.push_back(vertex_index+1);
        indices.push_back(vertex_index);
      }
    }
  }
  else
  {
    headD = length; //Limit max arrow head diameter
    head_radius = length * 0.5;
  }

  // Render the arrowhead cone and base with two triangle fans
  // Don't bother drawing head very low quality settings
  if (segment_count >= 3 && head_scale > 0 && head_radius > 1.0e-7 )
  {
    int v;
    // Pinnacle vertex is at point of arrow
    int pt = getVertexIdx(draw);
    Vec3d pinnacle = translate + rot * Vec3d(0, 0, halflength);

    // First pair of vertices on circle define a triangle when combined with pinnacle
    // First normal is between first and last triangle normals 1/|\seg-1

    //Read triangle vertex, normal
    //Vec3d vertex = translate + pinnacle;;
    Vec3d vertex = pinnacle;;

    Vec3d normal = rot * Vec3d(0.0f, 0.0f, 1.0f);
    normal.normalise();

    // Subsequent vertices describe outer edges of cone base
    for (v=segment_count; v >= 0; v--)
    {
      int vertex_index = getVertexIdx(draw);

      // Calc next vertex from unit circle coords
      Vec3d vertex1 = translate + rot * Vec3d(head_radius * drawstate.x_coords[v], head_radius * drawstate.y_coords[v], -headD+halflength);

      //Calculate normal at base
      Vec3d normal1 = rot * Vec3d(head_radius * drawstate.x_coords[v], head_radius * drawstate.y_coords[v], 0);
      normal1.normalise();

      //Duplicate pinnacle vertex as each facet needs a different normal
      read(draw, 1, lucVertexData, vertex.ref());
      //Balance between smoothness (normal) and highlighting angle of cone (normal1)
      Vec3d avgnorm = normal * 0.4 + normal1 * 0.6;
      avgnorm.normalise();
      read(draw, 1, lucNormalData, avgnorm.ref());

      //Read triangle vertex, normal
      read(draw, 1, lucVertexData, vertex1.ref());
      read(draw, 1, lucNormalData, avgnorm.ref());

      //Triangle fan indices
      if (v < segment_count)
      {
        indices.push_back(vertex_index);    //Pinnacle vertex
        indices.push_back(vertex_index-1);  //Previous vertex
        indices.push_back(vertex_index+1);  //Current vertex
      }
    }

    // Flatten cone for circle base -> set common point to share z-coord
    // Centre of base circle, normal facing back along arrow
    pt = getVertexIdx(draw);
    pinnacle = rot * Vec3d(0,0,-headD+halflength);
    vertex = translate + pinnacle;
    normal = rot * Vec3d(0.0f, 0.0f, -1.0f);
    //Read triangle vertex, normal
    read(draw, 1, lucVertexData, vertex.ref());
    read(draw, 1, lucNormalData, normal.ref());

    // Repeat vertices for outer edges of cone base
    for (v=0; v<=segment_count; v++)
    {
      int vertex_index = getVertexIdx(draw);
      // Calc next vertex from unit circle coords
      Vec3d vertex1 = rot * Vec3d(head_radius * drawstate.x_coords[v], head_radius * drawstate.y_coords[v], -headD+halflength);

      vertex1 = translate + vertex1;

      //Read triangle vertex, normal
      read(draw, 1, lucVertexData, vertex1.ref());
      read(draw, 1, lucNormalData, normal.ref());

      //Triangle fan indices
      indices.push_back(pt);
      indices.push_back(vertex_index-1);
      indices.push_back(vertex_index);
    }
  }

  //Read the triangle indices
  read(draw, indices.size(), lucIndexData, &indices[0]);
}

// Draws a trajectory vector between two coordinates,
// uses spheres and cylinder sections.
// coord0: start coord1: end
// radius: radius of cylinder/sphere sections to draw
// arrowHeadSize: if > 0 then finishes with arrowhead in vector direction at coord1
// segment_count: number of primitives to draw circular geometry with, 16 is usally a good default
// scale: scaling factor for each direction
// maxLength: length limit, sections exceeding this will be skipped
void Geometry::drawTrajectory(DrawingObject *draw, float coord0[3], float coord1[3], float radius0, float radius1, float arrowHeadSize, float scale[3], float maxLength, int segment_count)
{
  float length = 0;
  Vec3d vector, pos;

  assert(coord0 && coord1);

  //Scale start/end coords
  Vec3d start = Vec3d(coord0[0] * scale[0], coord0[1] * scale[1], coord0[2] * scale[2]);
  Vec3d end   = Vec3d(coord1[0] * scale[0], coord1[1] * scale[1], coord1[2] * scale[2]);

  // Obtain a vector between the two points
  vector = end - start;

  // Get centre position on vector between two coords
  pos = start + vector * 0.5;

  // Get length
  length = vector.magnitude();

  //Exceeds max length? Draw endpoint only
  if (maxLength > 0.f && length > maxLength)
  {
    drawSphere(draw, end, radius0, segment_count);
    return;
  }

  // Draw
  if (arrowHeadSize > 0)
  {
    // Head_scale as a ratio of length [0,1] makes no sense for trajectory,
    // convert to ratio of radius (> 1) using default conversion ratio
    if (arrowHeadSize < 1.0)
      arrowHeadSize = 0.5 * arrowHeadSize / RADIUS_DEFAULT_RATIO; // Convert from fraction of length to multiple of radius

    // Draw final section as arrow head
    // Position so centred on end of tube adjusted for arrowhead radius (tube radius * head size)
    // Too small a section to fit arrowhead? expand so length is at least 2*r ...
    if (length < 2.0 * radius1 * arrowHeadSize)
    {
      // Adjust length
      float length_adj = arrowHeadSize * radius1 * 2.0 / length;
      vector *= length_adj;
      // Adjust to centre position
      pos = start + vector * 0.5;
    }

    // Draw the vector arrow
    drawVector(draw, pos.ref(), vector.ref(), 1.0, radius0, radius1, arrowHeadSize, segment_count);

  }
  else
  {
    // Check segment length large enough to warrant joining points with cylinder section ...
    // Skip any section smaller than 0.3 * radius, draw sphere only for continuity
    //if (length > radius1 * 0.30)
    {
      // Join last set of points with this set
      drawVector(draw, pos.ref(), vector.ref(), 1.0, radius0, radius1, 0.0, segment_count);
//         if (segment_count < 3 || radius1 < 1.0e-3 ) return; //Too small for spheres
//          Vec3d centre(pos);
//         drawSphere(geom, centre, radius, segment_count);
    }
    // Finish with sphere, closes gaps in angled joins
//          Vec3d centre(coord1);
//      if (length > radius * 0.10)
//         drawSphere(geom, centre, radius, segment_count);
  }

}

void Geometry::drawCuboid(DrawingObject *draw, Vec3d& min, Vec3d& max, Quaternion& rot, bool quads)
{
  //float pos[3] = {min[0] + 0.5f*(max[0] - min[0]), min[1] + 0.5f*(max[1] - min[1]), min[2] + 0.5f*(max[2] - min[2])};
  Vec3d dims = max - min;
  Vec3d pos = min + dims * 0.5f;
  drawCuboidAt(draw, pos, dims, rot, quads);
}

void Geometry::drawCuboidAt(DrawingObject *draw, Vec3d& pos, Vec3d& dims, Quaternion& rot, bool quads)
{
   Vec3d min = dims * -0.5f; //Vec3d(-0.5f * width, -0.5f * height, -0.5f * depth);
   Vec3d max = min + dims; //Vec3d(min[0] + width, min[1] + height, min[2] + depth);
  
  //Corner vertices
  Vec3d verts[8] =
  {
    Vec3d(min[0], min[1], max[2]),
    Vec3d(max[0], min[1], max[2]),
    max,
    Vec3d(min[0], max[1], max[2]),
    min,
    Vec3d(max[0], min[1], min[2]),
    Vec3d(max[0], max[1], min[2]),
    Vec3d(min[0], max[1], min[2])
  };

  for (int i=0; i<8; i++)
  {
    /* Multiplying a quaternion q with a vector v applies the q-rotation to v */
    verts[i] = rot * verts[i];
    verts[i] += Vec3d(pos);
    //geom->checkPointMinMax(verts[i].ref());
  }

  if (quads)
  {
    //Back
    read(draw, 1, lucVertexData, verts[0].ref(), 2, 2);
    read(draw, 1, lucVertexData, verts[3].ref());
    read(draw, 1, lucVertexData, verts[1].ref());
    read(draw, 1, lucVertexData, verts[2].ref());

    //Front
    read(draw, 1, lucVertexData, verts[4].ref(), 2, 2);
    read(draw, 1, lucVertexData, verts[5].ref());
    read(draw, 1, lucVertexData, verts[7].ref());
    read(draw, 1, lucVertexData, verts[6].ref());

    //Bottom
    read(draw, 1, lucVertexData, verts[0].ref(), 2, 2);
    read(draw, 1, lucVertexData, verts[1].ref());
    read(draw, 1, lucVertexData, verts[4].ref());
    read(draw, 1, lucVertexData, verts[5].ref());

    //Top
    read(draw, 1, lucVertexData, verts[7].ref(), 2, 2);
    read(draw, 1, lucVertexData, verts[6].ref());
    read(draw, 1, lucVertexData, verts[3].ref());
    read(draw, 1, lucVertexData, verts[2].ref());

    //Left
    read(draw, 1, lucVertexData, verts[4].ref(), 2, 2);
    read(draw, 1, lucVertexData, verts[7].ref());
    read(draw, 1, lucVertexData, verts[0].ref());
    read(draw, 1, lucVertexData, verts[3].ref());

    //Right
    read(draw, 1, lucVertexData, verts[5].ref(), 2, 2);
    read(draw, 1, lucVertexData, verts[1].ref());
    read(draw, 1, lucVertexData, verts[6].ref());
    read(draw, 1, lucVertexData, verts[2].ref());
  }
  else
  {
    //Triangle indices
    unsigned vertex_index = (unsigned)getVertexIdx(draw);
    unsigned int indices[36] =
    {
      0+vertex_index, 1+vertex_index, 2+vertex_index, 2+vertex_index, 3+vertex_index, 0+vertex_index,
      3+vertex_index, 2+vertex_index, 6+vertex_index, 6+vertex_index, 7+vertex_index, 3+vertex_index,
      7+vertex_index, 6+vertex_index, 5+vertex_index, 5+vertex_index, 4+vertex_index, 7+vertex_index,
      4+vertex_index, 0+vertex_index, 3+vertex_index, 3+vertex_index, 7+vertex_index, 4+vertex_index,
      0+vertex_index, 1+vertex_index, 5+vertex_index, 5+vertex_index, 4+vertex_index, 0+vertex_index,
      1+vertex_index, 5+vertex_index, 6+vertex_index, 6+vertex_index, 2+vertex_index, 1+vertex_index
    };

    read(draw, 8, lucVertexData, verts[0].ref());
    read(draw, 36, lucIndexData, indices);
  }
}

void Geometry::drawSphere(DrawingObject *draw, Vec3d& centre, float radius, int segment_count)
{
  //Case of ellipsoid where all 3 radii are equal
  Vec3d radii = Vec3d(radius, radius, radius);
  Quaternion qrot;
  drawEllipsoid(draw, centre, radii, qrot, segment_count);
}

// Create a 3d ellipsoid given centre point, 3 radii and number of triangle segments to use
// Based on algorithm and equations from:
// http://local.wasp.uwa.edu.au/~pbourke/texture_colour/texturemap/index.html
// http://paulbourke.net/geometry/sphere/
void Geometry::drawEllipsoid(DrawingObject *draw, Vec3d& centre, Vec3d& radii, Quaternion& rot, int segment_count)
{
  int i,j;
  Vec3d edge, pos, normal;
  float tex[2];

  if (radii.x < 0) radii.x = -radii.x;
  if (radii.y < 0) radii.y = -radii.y;
  if (radii.z < 0) radii.z = -radii.z;
  if (segment_count < 0) segment_count = -segment_count;
  drawstate.cacheCircleCoords(segment_count);

  std::vector<unsigned int> indices;
  for (j=0; j<segment_count/2; j++)
  {
    //Triangle strip vertices
    for (i=0; i<=segment_count; i++)
    {
      int vertex_index = getVertexIdx(draw);
      // Get index from pre-calculated coords which is back 1/4 circle from j+1 (same as forward 3/4circle)
      int circ_index = ((int)(1 + j + 0.75 * segment_count) % segment_count);
      edge = Vec3d(drawstate.y_coords[circ_index] * drawstate.y_coords[i], drawstate.x_coords[circ_index], drawstate.y_coords[circ_index] * drawstate.x_coords[i]);
      pos = centre + rot * (radii * edge);

      tex[0] = i/(float)segment_count;
      tex[1] = 2*(j+1)/(float)segment_count;

      //Read triangle vertex, normal, texcoord
      read(draw, 1, lucVertexData, pos.ref());
      normal = rot * -edge;
      read(draw, 1, lucNormalData, normal.ref());
      read(draw, 1, lucTexCoordData, tex);

      // Get index from pre-calculated coords which is back 1/4 circle from j (same as forward 3/4circle)
      circ_index = ((int)(j + 0.75 * segment_count) % segment_count);
      edge = Vec3d(drawstate.y_coords[circ_index] * drawstate.y_coords[i], drawstate.x_coords[circ_index], drawstate.y_coords[circ_index] * drawstate.x_coords[i]);
      pos = centre + rot * (radii * edge);

      tex[0] = i/(float)segment_count;
      tex[1] = 2*j/(float)segment_count;

      //Read triangle vertex, normal, texcoord
      read(draw, 1, lucVertexData, pos.ref());
      normal = rot * -edge;
      read(draw, 1, lucNormalData, normal.ref());
      read(draw, 1, lucTexCoordData, tex);

      //Triangle strip indices
      if (i > 0)
      {
        //First tri
        indices.push_back(vertex_index-2);
        indices.push_back(vertex_index-1);
        indices.push_back(vertex_index);
        //Second tri
        indices.push_back(vertex_index-1);
        indices.push_back(vertex_index+1);
        indices.push_back(vertex_index);
      }
    }
  }

  //Read the triangle indices
  read(draw, indices.size(), lucIndexData, &indices[0]);
}

//Class to handle geometry types with sub-geometry glyphs drawn
//at vertices consisting of lines and/or triangles
Glyphs::Glyphs(DrawState& drawstate) : Geometry(drawstate)
{
  //Create sub-renderers
  lines = new Lines(drawstate);
  tris = new TriSurfaces(drawstate);
  points = new Points(drawstate);
  tris->internal = lines->internal = points->internal = true;
}

Glyphs::~Glyphs()
{
  close();
  delete lines;
  delete tris;
  delete points;
}

void Glyphs::close()
{
  if (!drawstate.global("gpucache"))
  {
    //Clear all generated geometry
    lines->clear();
    tris->clear();
    points->clear();
  }
  lines->close();
  tris->close();
  points->close();
  Geometry::close();
}

void Glyphs::setup(View* vp, float* min, float* max)
{
  //Only pass min/max to the master object,
  //otherwise sub-renderer geometry will expand bounding box
  Geometry::setup(vp, min, max);
  lines->setup(vp);
  tris->setup(vp);
  points->setup(vp);
}

void Glyphs::sort()
{
  {
    std::lock_guard<std::mutex> guard(lines->sortmutex);
    lines->sort();
  }
  {
    std::lock_guard<std::mutex> guard(tris->sortmutex);
    tris->sort();
  }
  {
    std::lock_guard<std::mutex> guard(points->sortmutex);
    points->sort();
  }
}

void Glyphs::display()
{
  tris->redraw = redraw;
  tris->reload = reload;
  lines->redraw = redraw;
  lines->reload = reload;
  points->redraw = redraw;
  points->reload = reload;

  //Need to call to clear the cached object
  lines->display();
  tris->display();
  points->display();

  if (!reload && drawstate.global("gpucache"))
  {
    //Skip re-gen of internal geometry shapes
    redraw = false;
  }

  //Render in parent display call
  Geometry::display();
}

void Glyphs::update()
{
  tris->update();
  lines->update();
  points->update();
}

void Glyphs::draw()
{
  if (lines->total)
    lines->draw();

  if (points->total)
    points->draw();

  if (tris->total)
  {
    // Undo any scaling factor for glyph drawing...
    glPushMatrix();
    if (tris->unscale)
      glScalef(tris->iscale[0], tris->iscale[1], tris->iscale[2]);

    tris->draw();

    // Re-Apply scaling factors
    glPopMatrix();
  }
}

void Glyphs::jsonWrite(DrawingObject* draw, json& obj)
{
  tris->jsonWrite(draw, obj);
  lines->jsonWrite(draw, obj);
  points->jsonWrite(draw, obj);
}

Imposter::Imposter(DrawState& drawstate) : Geometry(drawstate)
{
  vbo = 0;
}

void Imposter::close()
{
  if (vbo)
    glDeleteBuffers(1, &vbo);
  vbo = 0;
}

void Imposter::draw()
{
  //State...
  glDisable(GL_MULTISAMPLE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  //Draw two triangles to fill screen
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), (GLvoid*)0); // Load vertex x,y,z only
  glEnableClientState(GL_VERTEX_ARRAY);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableClientState(GL_VERTEX_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;
  
  glEnable(GL_MULTISAMPLE);
  GL_Error_Check;
}

void Imposter::update()
{
  //Use triangle renderer for two triangles to display shader output
  if (!vbo)
  {
    //Load 2 fullscreen triangles
    float verts[12] = {1,1,0,  -1,1,0,  1,-1,0,  -1,-1,0};
    //Initialise vertex buffer
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (glIsBuffer(vbo))
      glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  }
}

