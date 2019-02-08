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

std::ostream & operator<<(std::ostream &os, const Vertex& v)
{
  return os << "[" << v.vert[0] << "," << v.vert[1] << "," << v.vert[2] << "]";
}

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
  draw->opacityIdx = valuesLookup(draw->properties["opacityby"]);
  FloatValues* ovals = valueData(draw->opacityIdx);
  if (omap && ovals)
  {
    auto range = draw->ranges[ovals->label];
    omap->calibrate(&range);

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
    auto range = draw->ranges[vals->label];
    cmap->calibrate(&range);

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
  if (!values.size()) return valueIdx;
  if (by.is_string())
  {
    //Lookup index from label
    std::string label = by;
    if (label.length() == 0) return valueIdx;
    for (unsigned int j=0; j < values.size(); j++)
    {
      if (values[j]->label == label)
      {
        valueIdx = j;
        break;
      }
    }
    if (valueIdx > MAX_DATA_ARRAYS)
      debug_print("Label: %s not found!\n", label.c_str());
  }
  //Numerical index, check in range
  else if (by.is_number() && (int)by < values.size())
    valueIdx = by;

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
      //Have values but not enough for per-vertex? spread over range (eg: per triangle)
      range = count() / size;
      ridx = idx;
      if (range > 1) ridx = idx / range;

      //if (idx%1000==0) std::cout << "Filtering on index: " << draw->filterCache[i].dataIdx << " " << size << " values" << std::endl;
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
        else if (v != nullptr)
        {
          auto range = draw->ranges[v->label];
          value = range.maximum - range.minimum;
          min = range.minimum + min * value;
          max = range.minimum + max * value;
          value = (*v)[ridx];
        }
      }
      else
        value = (*v)[ridx];

      //Always filter nan/inf
      if (std::isnan(value) || std::isinf(value)) return true;

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
  FloatValues* fv = values[draw->colourIdx].get();
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

bool GeomData::opaqueCheck()
{
  //Return Opacity flag - default transparency enabled
  if (draw->properties.has("opaque"))
  {
    //If explicitly set, just use set value
    opaque = draw->properties["opaque"];
  }
  else
  {
    //Check for any properties that preclude transparency

    //Per-object wireframe works only when drawing opaque objects
    //(can't set per-objects properties when all triangles collected and sorted)
    opaque = (draw->properties["wireframe"] || draw->properties["opaque"]);

    //If using a colourmap without transparency, and no opacity prop, flag opaque
    if (draw->colourMap && values.size() > draw->colourIdx &&
        draw->colourMap->opaque &&
        !draw->properties.has("opacityby") &&
        (float)draw->properties["opacity"] == 1.0 &&
        (float)draw->properties["alpha"] == 1.0)
    {
      opaque = true;
    }
  }

  //Value is cached for future lookups
  return opaque;
}

Geometry::Geometry(Session& session) : view(NULL), elements(0),
                       cached(NULL), session(session),
                       allhidden(false), internal(false),
                       type(lucMinType), total(0), redraw(true), reload(true)
{
  drawcount = 0;
  type = lucMinType;
}

Geometry::~Geometry()
{
  //Free GeomData elements
  clear(true);
}

//Virtuals to implement
void Geometry::close() //Called on quit or gl context destroy
{
  //Ensure cache cleared
  cached = NULL;
}

void Geometry::clear(bool fixed)
{
  total = 0;
  reload = true;
  if (fixed)
  {
    records.clear();
  }
  else
  {
    for (int i = records.size()-1; i>=0; i--)
    {
      if (records[i]->step >= 0 && records[i]->type != lucTracerType)
      {
        //Now using shared_ptr so no need to delete
        records.erase(records.begin()+i);
      }
    }
  }
  
  //Ensure cache cleared
  cached = NULL;
  //Ensure temporal data gets reloaded
  geom.clear();
}

void Geometry::remove(DrawingObject* draw)
{
  //Same as clear but for specific drawing object
  reload = true;
  for (int i = records.size()-1; i>=0; i--)
  {
    if (draw == records[i]->draw)
    {
      //Now using shared_ptr so no need to delete
      records.erase(records.begin()+i);
    }
  }

  //Ensure temporal data gets reloaded
  geom.clear();
}

void Geometry::clearValues(DrawingObject* draw, std::string label)
{
  reload = true;
  for (auto g : records)
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
  for (auto g : records)
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
  for (unsigned int i = 0; i < records.size(); i++)
  {
    compareCoordMinMax(min, max, records[i]->min);
    compareCoordMinMax(min, max, records[i]->max);
    //Also update global min/max
    compareCoordMinMax(session.min, session.max, records[i]->min);
    compareCoordMinMax(session.min, session.max, records[i]->max);
  }
  //Update global bounding box size
  getCoordRange(session.min, session.max, session.dims);
}

void Geometry::dump(std::ostream& csv, DrawingObject* draw)
{
  //Just dumps currently loaded data, not all timesteps
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

          if (geom[i]->colourData() && geom[i]->colourData()->size() >= geom[i]->count())
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
  //Just dumps currently loaded data, not all timesteps
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
        Data_Ptr datp = geom[index]->dataContainer((lucGeometryDataType)data_type);
        //Quads are not supported by webgl, skip indices and new triangle indices will be generated instead
        if (type == lucGridType && data_type == lucIndexData) continue;
        DataContainer* dat = datp.get();
        FloatValues* val_ptr = NULL;
        if (!dat)
        {
          //TODO: export all values with labels separately
          //Check in values and use if label matches
          for (auto vals : geom[index]->values)
          {
            //std::cout << vals->label << " == " << GeomData::datalabels[data_type] << std::endl;
            if (vals->label == GeomData::datalabels[data_type])
            {
              dat = val_ptr = (FloatValues*)vals.get();
              break;
            }
          }
          //Use default colour values for "values" until multiple data sets supported in WebGL viewer
          if (!dat && data_type == lucColourValueData)
            dat = val_ptr = geom[index]->colourData();

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

          //Use cached min max for applicable data label
          if (val_ptr)
          {
            el["minimum"] = geom[index]->draw->ranges[val_ptr->label].minimum;
            el["maximum"] = geom[index]->draw->ranges[val_ptr->label].maximum;
          }
          //Use the local min/max data block
          else if (dat->minimum < dat->maximum)
          {
            el["minimum"] = dat->minimum;
            el["maximum"] = dat->maximum;
          }

          //Copy final data
          data[GeomData::datalabels[data_type]] = el;
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
  expandHidden();
  if (hidden[idx]) return false;
  hidden[idx] = true;
  redraw = true;
  return true;
}

void Geometry::hideShowAll(bool hide)
{
  expandHidden();
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
  expandHidden();
  if (!hidden[idx]) return false;
  hidden[idx] = false;
  redraw = true;
  return true;
}

void Geometry::showObj(DrawingObject* draw, bool state)
{
  expandHidden();
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
  for (auto g : records)
  {
    if (g->colourData() && (!draw || g->draw == draw) && g->draw->colourMap)
    {
      auto range = g->draw->ranges[g->colourData()->label];
      g->draw->colourMap->properties.data["range"] = json::array({range.minimum, range.maximum});
    }
  }
}

void Geometry::redrawObject(DrawingObject* draw, bool reload)
{
  for (unsigned int i = 0; i < records.size(); i++)
  {
    if (records[i]->draw == draw)
    {
      debug_print("Reloading object: %s\n", draw->name().c_str());
      this->reload = reload;
      redraw = true;
      return;
    }
  }
}

void Geometry::init() //Called on GL init
{
  reload = true;
}

void Geometry::merge(int start, int end)
{
  if (type == lucTracerType)
  {
    start = 0;
    end = 0;
    if (session.timesteps.size())
      end = session.timesteps.back()->step;
  }
  //This function writes the "geom" list containing
  //all renderable geometry
  // - All fixed (render+value) data
  // - All variable (render+value) data at the specified timestep range
  // - All mixed (fixed render + variable value) data at the specified timestep range
  geom.clear();
  int fixedVertices = 0;

  std::vector<Geom_Ptr> fixed;
  
  //First locate fixed records
  for (unsigned int i=0; i<records.size(); i++)
  {
    //printf("STEP %d (%d - %d)\n", records[i]->step, start, end);
    if (records[i]->step == -1)
    {
      fixed.push_back(records[i]);
      //printf("%d (%s) Found fixed record for %s, complete? %d\n", i, GeomData::names[type].c_str(),
      //       records[i]->draw->name().c_str(), records[i]->count() > 0);

      fixedVertices += records[i]->count();
    }
  }

  allDataFixed = true;

  //Now process each step in turn
  for (int step=start; step<=end; step++)
  {
    for (unsigned int i=0; i<records.size(); i++)
    {
      if (step >= 0 && records[i]->step == step)
      {
        allDataFixed = false;
        //Three possible cases:
        //- Data is complete:
        //  - check previous loaded record with same drawing object
        //    - Not incomplete? Load as completely variable
        //    - Found incomplete? Load and merge with incomplete fixed data
        //- Data is incomplete:
        //  - Find previous loaded record for same drawing object
        //    Merge with the fixed record

        //Find the previous record with this object in the fixed list
        int pos = -1;
        Geom_Ptr merge_source = nullptr;
        Geom_Ptr merge_dest = nullptr;
        for (pos=fixed.size()-1; pos>=0; pos--)
        {
          if (fixed[pos]->draw == records[i]->draw)
          {
            //Vertices in fixed?
            if (fixed[pos]->count() > 0)
            {
              merge_dest = fixed[pos];
              merge_source = records[i];
              //printf("%d (%s) BASE: Added fixed record for %s, complete? %d\n", i, GeomData::names[type].c_str(),
              //       fixed[pos]->draw->name().c_str(), fixed[pos]->count() > 0);
              //Use fixed entry as base, copy its reference
              geom.push_back(fixed[pos]);
            }
            else
            {
              //Use as source and copy records to varying entry
              merge_source = fixed[pos];
            }

            //Erase from fixed list
            fixed.erase(fixed.begin()+pos);
            break;
          }
        }
        
        //Vertices in varying? (Or no fixed record to merge with)
        if (records[i]->count() > 0 || !merge_dest)
        {
          //Use time-varying entry as base, copy its reference
          geom.push_back(records[i]);
          //printf("%d (%s) BASE: Added variable record for %s, complete? %d\n", i, GeomData::names[type].c_str(),
          //       records[i]->draw->name().c_str(), records[i]->count() > 0);
          //printf("%d/%d Copy record @ %d for %s, complete? %d\n", i, pos, step, records[i]->draw->name().c_str(), records[i]->count() > 0);
          merge_dest = records[i];
        }

        if (merge_source && merge_dest)
        {

          //Merge with previous entry
          //printf("%d/%d Merging record @ %d for %s, complete? %d/%d\n", i, pos, step, merge_source->draw->name().c_str(),
          //       merge_source->count() > 0, merge_dest->count() > 0);
          for (auto vals : merge_source->values)
          {
            //Only insert if not already done
            json by = vals->label;
            std::string labl = by;
            unsigned int idx = merge_dest->valuesLookup(by);
            if (idx <= MAX_DATA_ARRAYS)
            {
              //Replace
              //printf(" - REPLACE %s\n", labl.c_str());
              merge_dest->values[idx] = vals;
            }
            else
            {
              //Append (none existing)
              //printf(" - APPEND %s\n", labl.c_str());
              merge_dest->values.push_back(vals);
            }
          }

          //Copy hard coded render data types
          // - data in source always overwrites data in dest if present
          if (merge_source->_vertices->size() > 0)
            merge_dest->_vertices = merge_source->_vertices;
          if (merge_source->_normals->size() > 0)
            merge_dest->_normals = merge_source->_normals;
          if (merge_source->_vectors->size() > 0)
            merge_dest->_vectors = merge_source->_vectors;
          if (merge_source->_indices->size() > 0)
            merge_dest->_indices = merge_source->_indices;
          if (merge_source->_colours->size() > 0)
            merge_dest->_colours = merge_source->_colours;
          if (merge_source->_texCoords->size() > 0)
            merge_dest->_texCoords = merge_source->_texCoords;
          if (merge_source->_luminance->size() > 0)
            merge_dest->_luminance = merge_source->_luminance;
          if (merge_source->_rgb->size() > 0)
            merge_dest->_rgb = merge_source->_rgb;

          //Update references in render container
          merge_dest->setRenderData();
        }

      }
    }
  }

  //Add any remaining fixed entries
  for (auto f : fixed)
  {
    //printf("(%s) FIXED: Added fixed record for %s, complete? %d\n", GeomData::names[type].c_str(),
    //       f->draw->name().c_str(), f->count() > 0);
    geom.push_back(f);
  }

  //Update total vertex count
  int total = 0;
  for (unsigned int i=0; i<geom.size(); i++)
    total += geom[i]->count();

  allVertsFixed = (total == fixedVertices);
  //printf("(STEP %d) %d records, %d geom entries, %d verts, All fixed? %d All verts fixed? %d (%s : %s)\n", start, (int)records.size(),
  //       (int)geom.size(), total, allDataFixed, allVertsFixed, GeomData::names[type].c_str(), geom.size() > 0 ? geom[0]->draw->name().c_str() : "??");
}

Shader_Ptr Geometry::getShader(DrawingObject* draw)
{
  //Use existing custom shader if found
  if (draw->shader)
    return draw->shader;

  //Uses a custom shader?
  if (draw->properties.has("shaders"))
  {
    //Init from property
    json shaders = draw->properties["shaders"];
    std::cout << shaders << std::endl;
    if (shaders.size() == 1)
    {
      std::string f = shaders[0];
      draw->shader = std::make_shared<Shader>(f);
    }
    else if (shaders.size() == 2)
    {
      std::string v = shaders[0];
      std::string f = shaders[1];
      draw->shader = std::make_shared<Shader>(v, f);
    }
    else if (shaders.size() == 3)
    {
      std::string v = shaders[0];
      std::string g = shaders[1];
      std::string f = shaders[2];
      draw->shader = std::make_shared<Shader>(v, g, f);
    }

    //Get uniforms/attribs
    draw->shader->loadUniforms();
    draw->shader->loadAttribs();

    return draw->shader;
  }

  //Get the base type for default shader
  lucGeometryType btype;
  switch (type)
  {
    case lucPointType:
    case lucLineType:
    case lucVolumeType:
      btype = type;
      break;
    case lucGridType:
    case lucTriangleType:
    case lucVectorType:
    case lucTracerType:
    case lucShapeType:
    case lucScreenType:
      //Everything else uses the triangle shader
      btype = lucTriangleType;
      break;
    default:
      //Fallback shader, passthrough colour only
      btype = lucMinType;
      break;
  }

  //Already initialised?
  if (session.shaders[btype])
    return session.shaders[btype];

  //Not found? init the default
  if (btype == lucPointType)
  {
    //Point shaders
    session.shaders[lucPointType] = std::make_shared<Shader>("pointShader.vert", "pointShader.frag");
    session.shaders[lucPointType]->loadUniforms();
    session.shaders[lucPointType]->loadAttribs();
  }

  if (btype == lucLineType)
  {
    //Line shaders
    session.shaders[lucLineType] = std::make_shared<Shader>("lineShader.vert", "lineShader.frag");
    session.shaders[lucLineType]->loadUniforms();
  }

  if (btype == lucTriangleType)
  {
    //Triangle shaders
    session.shaders[lucTriangleType] = std::make_shared<Shader>("triShader.vert", "triShader.frag");
    session.shaders[lucTriangleType]->loadUniforms();
  }

  if (btype == lucVolumeType)
  {
    //Volume ray marching shaders
    session.shaders[lucVolumeType] = std::make_shared<Shader>("volumeShader.vert", "volumeShader.frag");
    session.shaders[lucVolumeType]->loadUniforms();
    session.shaders[lucVolumeType]->loadAttribs();
  }

  if (btype == lucMinType)
  {
    //Default colour only shader
    session.shaders[lucMinType] = std::make_shared<Shader>("default.vert", "default.frag");
    session.shaders[lucMinType]->loadUniforms();
  }

  return session.shaders[btype];
}

void Geometry::setState(unsigned int i)
{
  //NOTE: Transparent triangle surfaces/points are drawn as a single object so 
  //      no per-object state settings work, state applied is that of first in list
  GL_Error_Check;
  if (geom.size() <= i) return;
  DrawingObject* draw = geom[i]->draw;
  Properties& props = geom[i]->draw->properties;

  //Textured? - can be per element, so always execute
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

  //Only set rest of object state when object changes
  if (draw == cached) return;
  cached = draw;

  //printf("SETSTATE %s\n", geom[i]->draw->name().c_str());
  bool lighting = props["lit"];
  //Don't light surfaces in 2d models
  if ((type == lucTriangleType || type == lucGridType) && !view->is3d && !internal) lighting = false;

  //Global/Local draw state
  if (props["cullface"])
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  //Surface specific options
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  if (TriangleBased(type))
  {
    //Disable lighting and polygon faces in wireframe mode
    if (props["wireframe"])
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      lighting = false;
      glDisable(GL_CULL_FACE);
    }

    //TODO: replace with shader version for gl 3.2+
    if (props["flat"])
      glShadeModel(GL_FLAT);
    else
      glShadeModel(GL_SMOOTH);
  }
  else
  {
    //Flat disables lighting for non surface types
    if (props["flat"]) lighting = false;
    glEnable(GL_BLEND);
    glShadeModel(GL_SMOOTH);
  }

  //Default line width
  float lineWidth = (float)props["linewidth"] * session.scale2d; //Include 2d scale factor
  glLineWidth(lineWidth);

  //Disable depth test by default for 2d lines, otherwise enable
  bool depthTestDefault = (view->is3d || type != lucLineType);
  if (props.getBool("depthtest", depthTestDefault))
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);

  if (props["depthwrite"])
    glDepthMask(GL_TRUE);
  else
    glDepthMask(GL_FALSE);

  //Replace the default colour with a json value if present
  draw->colour = draw->properties.getColour("colour", draw->colour.r, draw->colour.g, draw->colour.b, draw->colour.a);

  //Uniforms for shader programs
  Shader_Ptr prog = getShader(geom[i]->draw);
  assert(prog && prog->program > 0); //Should always get a shader now
  prog->use();

  //Custom uniforms?
  if (draw->properties.has("uniforms"))
  {
    json uniforms = draw->properties["uniforms"];
    //std::cout << uniforms << std::endl;
    for (json::iterator it = uniforms.begin(); it != uniforms.end(); ++it)
    {
      //Attempt to find in properties and use that value if found
      json prop;
      std::string label = it.key();
      if (draw->properties.has(label))
        prop = draw->properties[label];
      else
        prop = it.value();
      //std::cout << label << " ==> " << prop << std::endl;

      //Special case: string data -> treat as colourmap!
      if (prop.is_string())
      {
        if (geom[i]->draw->textureMap && geom[i]->draw->textureMap->texture)
        {
          prog->setUniformi(label, geom[i]->draw->textureMap->texture->texture->unit);
          //std::cerr << label << " : Colourmap texture available on unit " << geom[i]->draw->colourMap->texture->texture->unit << std::endl;
        }
        else
          std::cerr << label << " : No colourmap texture available! " << prop << std::endl;
      }
      else if (!prop.is_null())
      {
        prog->setUniform(label, prop);
      }
    }
  }

  //Per-object "opacity" overrides global default if set
  //"alpha" is multiplied to affect all objects
  float opacity = (float)props["alpha"];
  //Apply global 'opacity' only if no per-object setting (which is applied with colour)
  if (!props.has("opacity"))
    opacity *= (float)session.global("opacity");
  bool allopaque = !session.global("sort");
  if (allopaque) opacity = 1.0;
  prog->setUniformf("uOpacity", opacity);
  prog->setUniformi("uLighting", lighting);
  prog->setUniformf("uBrightness", props["brightness"]);
  prog->setUniformf("uContrast", props["contrast"]);
  prog->setUniformf("uSaturation", props["saturation"]);
  prog->setUniformf("uAmbient", props["ambient"]);
  prog->setUniformf("uDiffuse", props["diffuse"]);
  prog->setUniformf("uSpecular", props["specular"]);
  prog->setUniformf("uShininess", props["shininess"]);
  prog->setUniform("uLightPos", props["lightpos"]);
  prog->setUniformi("uTextured", texture && texture->unit >= 0);
  prog->setUniformf("uOpaque", allopaque || geom[i]->opaque);
  //std::cout << i << " OPAQUE: " << allopaque << " || " << geom[i]->opaque << std::endl;

  if (texture)
    prog->setUniform("uTexture", (int)texture->unit);

  if (geom[i]->render->normals.size() == 0 && TriangleBased(type))
    prog->setUniform("uCalcNormal", 1);
  else
    prog->setUniform("uCalcNormal", 0);

  if (prog->uniforms["uClipMin"])
  {
    Vec3d clipMin = Vec3d(-HUGE_VALF, -HUGE_VALF, -HUGE_VALF);
    Vec3d clipMax = Vec3d(HUGE_VALF, HUGE_VALF, HUGE_VALF);
    if (props["clip"])
    {
      bool xminset = props.has("xmin") || props.hasglobal("xmin");
      bool yminset = props.has("ymin") || props.hasglobal("ymin");
      bool zminset = props.has("zmin") || props.hasglobal("zmin");
      bool xmaxset = props.has("xmax") || props.hasglobal("xmax");
      bool ymaxset = props.has("ymax") || props.hasglobal("ymax");
      bool zmaxset = props.has("zmax") || props.hasglobal("zmax");
      clipMin = Vec3d(xminset ? (float)props["xmin"] : -HUGE_VALF,
                      yminset ? (float)props["ymin"] : -HUGE_VALF,
                      zminset && view->is3d ? (float)props["zmin"] : -HUGE_VALF);
      clipMax = Vec3d(xmaxset ? (float)props["xmax"] : HUGE_VALF,
                      ymaxset ? (float)props["ymax"] : HUGE_VALF,
                      zmaxset && view->is3d ? (float)props["zmax"] : HUGE_VALF);

      if (props["clipmap"])
      {
        Vec3d dims(session.dims);
        Vec3d dmin(session.min);
        clipMin *= dims;
        clipMin += dmin;
        clipMax *= dims;
        clipMax += dmin;
      }
      //printf("Dimensions %f,%f,%f - %f,%f,%f\n", session.min[0], session.min[1],
      //       session.min[2], session.max[0], session.max[1], session.max[2]);
      //printf("Clipping %s %f,%f,%f - %f,%f,%f\n", geom[i]->draw->name().c_str(),
      //       clipMin[0], clipMin[1], clipMin[2], clipMax[0], clipMax[1], clipMax[2]);
    }

    prog->setUniform3f("uClipMin", clipMin.ref());
    prog->setUniform3f("uClipMax", clipMax.ref());
  }
  GL_Error_Check;
}


void Geometry::updateBoundingBox()
{
    //view->rotated = true;
    //Save min/max bounding box including sub-renderer geometry
    //including view bounds
    Properties::toArray<float>(view->properties["min"], this->min, 3);
    Properties::toArray<float>(view->properties["max"], this->max, 3);
    for (unsigned int o=0; o<view->objects.size(); o++)
    {
      if (view->objects[o]->properties["visible"])
      {
        for (auto g : geom)
        {
          if (g->draw == view->objects[o])
          {
            compareCoordMinMax(this->min, this->max, g->min);
            compareCoordMinMax(this->min, this->max, g->max);
            //printf("Applied bounding dims from object %s...%f,%f,%f - %f,%f,%f\n", g->draw->name().c_str(), 
            //       g->min[0], g->min[1], g->min[2], g->max[0], g->max[1], g->max[2]);
          }
        }
      }
    }
    //printf("(%s) Final bounding dims...%f,%f,%f - %f,%f,%f\n", GeomData::names[type].c_str(), min[0], min[1], min[2], max[0], max[1], max[2]);
}

void Geometry::display(bool refresh)
{
  //Skip if view not open or nothing to draw
  if (!view || !view->width) return;

  //TimeStep changed or step refresh requested
  if (refresh || timestep != session.now || (geom.size() == 0 && records.size() > 0))
  {
    merge(session.now, session.now);
    timestep = session.now;
  }

  //Default to no shaders
  GL_Error_Check;
  glUseProgram(0);

  //Clear cached object
  cached = NULL;

  //Anything to draw?
  unsigned int newcount = 0;
  for (unsigned int i=0; i < geom.size(); i++)
  {
    if (drawable(i))
      newcount++;
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
      if (session.scale2d != 1.0 && font != "vector")
        geom[i]->draw->properties.data["font"] = "vector"; //Force vector if downsampling
      //Default to object colour (if fontcolour provided will replace)
      Colour colour;
      ColourLookup& getColour = geom[i]->colourCalibrate();
      session.fonts.setFont(geom[i]->draw->properties, "small", 1.0, session.scale2d);

      for (unsigned int j=0; j < geom[i]->labels.size(); j++)
      {
        float* p = geom[i]->render->vertices[j];
        getColour(colour, j);
        glColor3ubv(colour.rgba);
        //debug_print("Labels for %d - %d : %s\n", i, j, geom[i]->labels[j].c_str());
        std::string labstr = geom[i]->labels[j];
        if (labstr.length() == 0) continue;
        //Preceed with ! for right align, | for centre
        float shift = session.fonts.printWidth("XX")*FONT_SCALE_3D/view->scale[1]; //Vertical shift
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
          session.fonts.print3dBillboard(p[0], p[1]-shift, p[2], labstr.c_str(), align, view->scale);
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
  expandHidden();
  if (idx >= geom.size()) return false;
  if (!geom[idx]->draw->visible) return false;
  //Within bounds and not hidden
  if (geom[idx]->count() > 0 && !hidden[idx])
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
  //Return all data from active geom list (fixed + current timestep)
  merge(session.now, session.now);
  std::vector<Geom_Ptr> geomlist;
  for (unsigned int i=0; i<geom.size(); i++)
    if (geom[i]->draw == draw)
      geomlist.push_back(geom[i]);
  return geomlist;
}

std::vector<Geom_Ptr> Geometry::getAllObjectsAt(DrawingObject* draw, int step)
{
  //Return all data from records list (at specified timestep, or -2 for all)
  merge(session.now, session.now);
  std::vector<Geom_Ptr> geomlist;
  for (unsigned int i=0; i<records.size(); i++)
    if (records[i]->draw == draw && (step < -1  || records[i]->step == step))
      geomlist.push_back(records[i]);
  return geomlist;
}

Geom_Ptr Geometry::getObjectStore(DrawingObject* draw)
{
  //Get passed object's most recently added data store (iff timestep matches or all fixed)
  for (int i=records.size()-1; i>=0; i--)
    if (records[i]->draw == draw && (draw->fixed || records[i]->step == session.now))
      return records[i];
  return nullptr;
}

Geom_Ptr Geometry::add(DrawingObject* draw)
{
  int timestep = session.now;
  if (draw->fixed) timestep = -1;
  Geom_Ptr geomdata = std::make_shared<GeomData>(draw, type, timestep);
  records.push_back(geomdata);
  //if (allhidden) draw->properties.data["visible"] = false;
  //printf("(TS %d) %d NEW %s DATA STORE CREATED FOR %s size %d ptr %p hidden %d\n", timestep, records.size(), GeomData::names[type].c_str(), draw->name().c_str(), records.size(), geomdata, allhidden);
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

  //printf("(%s) Final bounding dims...%f,%f,%f - %f,%f,%f\n", GeomData::names[type].c_str(), min[0], min[1], min[2], max[0], max[1], max[2]);
}

void Geometry::objectBounds(DrawingObject* draw, float* min, float* max, bool allsteps)
{
  if (!min || !max) return;
  //Get geometry bounds from all object data
  for (auto g : records)
  {
    if (!g->count()) continue;
    if (!allsteps && g->step >= 0 && g->step != session.now) continue;

    //If no range, must calculate
    for (int i=0; i<3; i++)
    {
      if (std::isinf(g->max[i]) || std::isinf(g->min[i]))
      {
        g->calcBounds();
        //printf("No bounding dims provided for object %s, calculated ...%f,%f,%f - %f,%f,%f\n", g->draw->name().c_str(), 
        //        g->min[0], g->min[1], g->min[2], g->max[0], g->max[1], g->max[2]);
        break;
      }
    }

    if (g->draw == draw)
    {
      compareCoordMinMax(min, max, g->min);
      compareCoordMinMax(min, max, g->max);
      //printf("Applied bounding dims from object %s...%f,%f,%f - %f,%f,%f\n", g->draw->name().c_str(), 
      //       g->min[0], g->min[1], g->min[2], g->max[0], g->max[1], g->max[2]);
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

  //Convert older tracer databases, where all data is in a single fixed timestep block
  if (geomdata && type == lucTracerType && width > 0 && width != n && n%width == 0 && n > 1)
  {
    Data_Ptr container = geomdata->dataContainer(dtype);
    int steps = n / width;
    //printf("WIDTH %d/%d = %d TS %d (%d)\n", n, width, steps, timestep, session.now);
    int old = session.now;
    for (int step=0; step < steps; step++)
    {
      session.now = step;
      read(draw, width, dtype, (float*)data + (step*width*container->datasize), width, height, depth);
    }
    session.now = old;
    return geomdata;
  }

  //If dimensions specified, check if full dataset loaded
  assert(dtype <= lucMaxDataType);
  if (geomdata && geomdata->width > 0 && geomdata->height > 0)
  {
    Data_Ptr container = geomdata->dataContainer(dtype);
    unsigned int size = geomdata->width * geomdata->height * (geomdata->depth > 0 ? geomdata->depth : 1);
    if (size == container->count())
      geomdata = nullptr;
    //if (!geomdata) printf("LOAD COMPLETE dtype %d size %u ==  %u / %u == %u\n", dtype, size, container->size(), 
    //                       container->unitsize(), container->count());
  }

  //Allow spec width/height/depth in properties
  if (!geomdata || geomdata->count() == 0 || geomdata->width * geomdata->height == 0)
  {
    if (width == 0) width = draw->dims[0];
    if (height == 0) height = draw->dims[1];
    if (depth == 0) depth = draw->dims[2];
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
  if (geomdata->count() == 0 && !geomdata->draw->geometry.length())
    geomdata->draw->properties.data["geometry"] = geomdata->draw->geometry = GeomData::names[type];

  //Read the data
  if (n > 0)
    geomdata->dataContainer(dtype)->read(n, data);

  if (dtype == lucVertexData)
  {
    //Update bounds on single vertex reads (except labels)
    if (n == 1 && type != lucLabelType) // && !internal)
      geomdata->checkPointMinMax((float*)data);
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
  Values_Ptr store = geom->valueContainer(label);

  //Create value store if required
  if (store == nullptr)
  {
    store = std::make_shared<FloatValues>();
    geom->values.push_back(store);
    store->label = label;
    //printf(" -- NEW VALUE STORE CREATED FOR %s label %s count %d N %d ptr %p\n", geom->draw->name().c_str(), label.c_str(), geom->values.size(), n, store);
  }

  //Read the data
  if (n > 0) store->read(n, data);

  return geom; //Return data store pointer
}

//Read a triangle with optional resursive splitting and y/z swap
void Geometry::addTriangle(DrawingObject* obj, float* a, float* b, float* c, int level, bool swapY, bool texCoords)
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
    float A[5] = {a[0], a[2], a[1], 0.f, 0.f};
    float B[5] = {b[0], b[2], b[1], 0.f, 0.f};
    float C[5] = {c[0], c[2], c[1], 0.f, 0.f};

    if (swapY)
    {
      a = A;
      b = B;
      c = C;
      if (texCoords)
      {
        A[3] = a[3];
        A[4] = a[4];
        B[3] = b[3];
        B[4] = b[4];
        C[3] = c[3];
        C[4] = c[4];
      }
    }

    //Read the triangle
    read(obj, 1, lucVertexData, a);
    read(obj, 1, lucVertexData, b);
    read(obj, 1, lucVertexData, c);

    if (texCoords)
    {
      read(obj, 1, lucTexCoordData, &a[3]);
      read(obj, 1, lucTexCoordData, &b[3]);
      read(obj, 1, lucTexCoordData, &c[3]);
    }
  }
  else
  {
    //Process a triangle into 4 sub-triangles
    if (texCoords)
    {
      float ab[5] = {0.5f*(a[0]+b[0]), 0.5f*(a[1]+b[1]), 0.5f*(a[2]+b[2]), 0.5f*(a[3]+b[3]), 0.5f*(a[4]+b[4])};
      float ac[5] = {0.5f*(a[0]+c[0]), 0.5f*(a[1]+c[1]), 0.5f*(a[2]+c[2]), 0.5f*(a[3]+c[3]), 0.5f*(a[4]+c[4])};
      float bc[5] = {0.5f*(b[0]+c[0]), 0.5f*(b[1]+c[1]), 0.5f*(b[2]+c[2]), 0.5f*(b[3]+c[3]), 0.5f*(b[4]+c[4])};

      addTriangle(obj, a, ab, ac, level, swapY, true);
      addTriangle(obj, ab, b, bc, level, swapY, true);
      addTriangle(obj, ac, bc, c, level, swapY, true);
      addTriangle(obj, ab, bc, ac, level, swapY, true);
    }
    else
    {
      float ab[3] = {0.5f*(a[0]+b[0]), 0.5f*(a[1]+b[1]), 0.5f*(a[2]+b[2])};
      float ac[3] = {0.5f*(a[0]+c[0]), 0.5f*(a[1]+c[1]), 0.5f*(a[2]+c[2])};
      float bc[3] = {0.5f*(b[0]+c[0]), 0.5f*(b[1]+c[1]), 0.5f*(b[2]+c[2])};

      addTriangle(obj, a, ab, ac, level, swapY);
      addTriangle(obj, ab, b, bc, level, swapY);
      addTriangle(obj, ac, bc, c, level, swapY);
      addTriangle(obj, ab, bc, ac, level, swapY);
    }
  }
}

void Geometry::scanDataRange(DrawingObject* draw)
{
  //Scan all data for min/max
  std::map<std::string, Range> ranges;

  //if (records.size())
  //  std::cout << GeomData::names[type] << " @ " << session.now << ">> SCANNING: " << draw->name() << " : " << records.size() << std::endl;

  for (auto g : records)
  {
    if ((!draw || draw == g->draw) && (session.now < 0 || !g->draw->properties["steprange"] || g->step < 0 || g->step == session.now))
    {
      for (unsigned int d=0; d<g->values.size(); d++)
      {
        Values_Ptr vals = g->values[d];
        FloatValues& fvals = *vals;

        //If already defined, and valid, skip the range update
        auto range = g->draw->ranges[fvals.label];
        if (range.valid())
        {
          //std::cout << session.now << " Skip updating data range for " << fvals.label << " : " << g->draw->name() << std::endl; 
          continue; //Skip if defined already
        }

        //Store max/min per value label
        range = Range();
        if (ranges.find(fvals.label) != ranges.end())
          range = ranges[fvals.label];

        //Get local range, skip if already cached
        fvals.minmax();

        //Apply local element range to data range for object
        //std::cout << session.now << " *Updating data range for " << g->draw->name() << " : " 
        //          << fvals.label << " ==> " << range << std::endl;
        ranges[fvals.label].update(fvals.minimum, fvals.maximum);
      }
    }
  }

  //Once we have scanned full range, update with the final values
  for (auto g : records)
  {
    if ((!draw || draw == g->draw) && (session.now < 0 || !g->draw->properties["steprange"] || g->step < 0 || g->step == session.now))
    {
      for (unsigned int d=0; d<g->values.size(); d++)
      {
        Values_Ptr vals = g->values[d];
        FloatValues& fvals = *vals;

        if (ranges.find(fvals.label) == ranges.end())
          continue; //Skip if not set

        auto range = ranges[fvals.label];
        //std::cout << session.now << " Updating data range for " << g->draw->name() << " : " 
        //          << fvals.label << " ==> " << range << std::endl;
        g->draw->updateRange(fvals.label, range);
      }
    }
  }
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

void Geometry::print(std::ostream& os)
{
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    os << GeomData::names[type] << " [" << i << "] - "
       << (drawable(i) ? "shown" : "hidden") << std::endl;
  }
}

json Geometry::getDataLabels(DrawingObject* draw)
{
  //Iterate through all geometry of given object(id) and print
  //the index and label of the associated value data sets
  //(used for colouring and filtering)
  json list = json::array();
  if (geom.size() == 0)
    merge(session.now, session.now);
  int laststep = -1;
  for (unsigned int i = 0; i < geom.size(); i++)
  {
    //Skip all except first (for tracers which have all steps merged)
    if (geom[i]->step > 0 && laststep >= 0)
      break;
    laststep = geom[i]->step;
    if (!draw || geom[i]->draw == draw)
    {
      for (unsigned int v = 0; v < geom[i]->values.size(); v++)
      {
        json entry;
        auto range = geom[i]->draw->ranges[geom[i]->values[v]->label];
        entry["label"] = geom[i]->values[v]->label;
        entry["minimum"] = range.minimum;
        entry["maximum"] = range.maximum;
        entry["size"] = geom[i]->values[v]->size();
        list.push_back(entry);
      }
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

void Geometry::setTexture(DrawingObject* draw, Texture_Ptr tex)
{
  Geom_Ptr geomdata = getObjectStore(draw);
  if (geomdata)
  {
    //printf("Set texture on %p to %s\n", geomdata.get(), tex->fn.full.c_str());
    geomdata->texture = tex;
    //Must be opaque to draw with own texture
    geomdata->opaque = true;
  }
}

void Geometry::loadTexture(DrawingObject* draw, GLubyte* data, GLuint width, GLuint height, GLuint channels, bool flip, bool mipmaps, bool bgr)
{
  Geom_Ptr geomdata = getObjectStore(draw);
  if (geomdata)
  {
    //std::cout << "LOAD TEXTURE " << width << " x " << height << " x " << channels << " ON " << draw->name() << std::endl;
    //NOTE: must only load the data here, can't make OpenGL calls as can be called async
    geomdata->texture->loadData(data, width, height, channels, flip, mipmaps, bgr);
    //Must be opaque to draw with own texture (TODO: obj properties in shader)
    geomdata->opaque = true;
  }
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
void Geometry::drawVector(DrawingObject *draw, const Vec3d& translate, const Vec3d& vector, bool scale3d, float scale, float radius0, float radius1, float head_scale, int segment_count, Colour* colour)
{
  //Scale vector
  Vec3d vec = vector * scale;

  const Vec3d& scale3 = scale3d ? view->scale : Vec3d(1.0, 1.0, 1.0);
  const Vec3d& iscale = scale3d ? view->iscale : Vec3d(1.0, 1.0, 1.0);

  //Setup orientation using alignment vector
  Vec3d scaled = vec * scale3;
  Quaternion rot = vectorRotation(scaled);

  // Get circle coords
  session.cacheCircleCoords(segment_count);

  //Get the geom ptr
  Geom_Ptr g = read(draw, 0, lucVertexData, NULL);
  unsigned int voffset = g->count();

  // Render a 3d arrow, cone with base for head, cylinder for shaft

  // Length of the drawn vector = vector magnitude * scaling factor
  float length = scaled.magnitude();
  float halflength = length*0.5;
  if (length < FLT_EPSILON || std::isinf(length)) return;

  // Default shaft radius based on length of vector (2%) otherwise use an exact passed value
  if (radius0 == 0)
  {
    if (radius1 == 0) radius1 = length * draw->radius_default;
    radius0 = radius1;
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
  if (type == lucLineType)
  {
    // Draw Line
    Vec3d vertex0 = Vec3d(0,0,-halflength);
    Vec3d vertex = translate + rot * vertex0;
    if (!view->is3d && scale3d) vertex.z += 0.75*radius1; //Ensure has same 3d properties for depth testing
    g->readVertex(vertex.ref());
    vertex0.z = halflength;
    vertex = translate + rot * vertex0;
    if (!view->is3d && scale3d) vertex.z += 0.75*radius1; //Ensure has same 3d properties for depth testing
    g->readVertex(vertex.ref());
    segment_count = 0;
  }
  else if (length > headD)
  {
    int v;
    for (v=0; v <= segment_count; v++)
    {
      unsigned int vertex_index = g->count();

      // Base of shaft
      Vec3d vertex0 = Vec3d(radius0 * session.x_coords[v], radius0 * session.y_coords[v], -halflength); // z = Shaft length to base of head
      Vec3d vertex = translate + rot * vertex0 * iscale;

      //Read triangle vertex, normal
      g->readVertex(vertex.ref());
      Vec3d normal = rot * Vec3d(session.x_coords[v], session.y_coords[v], 0) * scale3;
      normal.normalise();
      g->_normals->read(1, normal.ref());

      // Top of shaft
      Vec3d vertex1 = Vec3d(radius1 * session.x_coords[v], radius1 * session.y_coords[v], -headD+halflength);
      vertex = translate + rot * vertex1 * iscale;

      //Read triangle vertex, normal
      g->readVertex(vertex.ref());
      g->_normals->read(1, normal.ref());

      //Triangle strip indices
      if (v > 0)
      {
        //First tri
        g->_indices->read1(vertex_index-2);
        g->_indices->read1(vertex_index-1);
        g->_indices->read1(vertex_index);
        //Second tri
        g->_indices->read1(vertex_index-1);
        g->_indices->read1(vertex_index+1);
        g->_indices->read1(vertex_index);
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
    unsigned int pt = g->count();
    Vec3d pinnacle = translate + rot * Vec3d(0, 0, halflength) * iscale;

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
      unsigned int vertex_index = g->count();

      // Calc next vertex from unit circle coords
      Vec3d vertex1 = translate + rot * Vec3d(head_radius * session.x_coords[v], head_radius * session.y_coords[v], -headD+halflength) * iscale;

      //Calculate normal at base
      Vec3d normal1 = rot * Vec3d(head_radius * session.x_coords[v], head_radius * session.y_coords[v], 0);
      normal1.normalise();

      //Duplicate pinnacle vertex as each facet needs a different normal
      g->readVertex(vertex.ref());
      //Balance between smoothness (normal) and highlighting angle of cone (normal1)
      Vec3d avgnorm = normal * 0.4 + normal1 * 0.6 * scale3;
      avgnorm.normalise();
      //avgnorm *= scale3;
      g->_normals->read(1, avgnorm.ref());

      //Read triangle vertex, normal
      g->readVertex(vertex1.ref());
      g->_normals->read(1, avgnorm.ref());

      //Triangle fan indices
      if (v < segment_count)
      {
        g->_indices->read1(vertex_index);    //Pinnacle vertex
        g->_indices->read1(vertex_index-1);  //Previous vertex
        g->_indices->read1(vertex_index+1);  //Current vertex
      }
    }

    // Flatten cone for circle base -> set common point to share z-coord
    // Centre of base circle, normal facing back along arrow
    pt = g->count();
    pinnacle = rot * Vec3d(0,0,-headD+halflength) * iscale;
    vertex = translate + pinnacle;
    normal = rot * Vec3d(0.0f, 0.0f, -1.0f) * scale3;
    //Read triangle vertex, normal
    g->readVertex(vertex.ref());
    g->_normals->read(1, normal.ref());

    // Repeat vertices for outer edges of cone base
    for (v=0; v<=segment_count; v++)
    {
      unsigned int vertex_index = g->count();
      // Calc next vertex from unit circle coords
      Vec3d vertex1 = rot * Vec3d(head_radius * session.x_coords[v], head_radius * session.y_coords[v], -headD+halflength) * iscale;

      vertex1 = translate + vertex1;

      //Read triangle vertex, normal
      g->readVertex(vertex1.ref());
      g->_normals->read(1, normal.ref());

      //Triangle fan indices
      g->_indices->read1(pt);
      g->_indices->read1(vertex_index-1);
      g->_indices->read1(vertex_index);
    }
  }

  if (colour) g->vertexColours(colour, voffset);
}

// Draws a trajectory vector between two coordinates,
// uses spheres and cylinder sections.
// coord0: start coord1: end
// radius: radius of cylinder/sphere sections to draw
// arrowHeadSize: if > 0 then finishes with arrowhead in vector direction at coord1
// segment_count: number of primitives to draw circular geometry with, 16 is usally a good default
// scale: scaling factor for each direction
// maxLength: length limit, sections exceeding this will be skipped
void Geometry::drawTrajectory(DrawingObject *draw, float coord0[3], float coord1[3], float radius0, float radius1, float arrowHeadSize, float scale[3], float maxLength, int segment_count, Colour* colour)
{
  float length = 0;
  Vec3d vector, pos;

  assert(coord0 && coord1);

  Vec3d start = coord0;
  Vec3d end   = coord1;

  // Obtain a vector between the two points
  vector = end - start;

  // Get centre position on vector between two coords
  pos = start + vector * 0.5;

  // Get length
  length = vector.magnitude();

  //Exceeds max length? Draw endpoint only
  if (maxLength > 0.f && length > maxLength)
  {
    //drawSphere(draw, end, radius0, segment_count);
    return;
  }

  // Draw
  if (arrowHeadSize > 0)
  {
    // Head_scale as a ratio of length [0,1] makes no sense for trajectory,
    // convert to ratio of radius (> 1) using default conversion ratio
    if (arrowHeadSize < 1.0)
      arrowHeadSize = 0.5 * arrowHeadSize / draw->radius_default; // Convert from fraction of length to multiple of radius

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
    drawVector(draw, pos.ref(), vector.ref(), true, 1.0, radius0, radius1, arrowHeadSize, segment_count, colour);

  }
  else
  {
    // Check segment length large enough to warrant joining points with cylinder section ...
    // Skip any section smaller than 0.3 * radius, draw sphere only for continuity
    //if (length > radius1 * 0.30)
    {
      // Join last set of points with this set
      drawVector(draw, pos, vector, true, 1.0, radius0, radius1, 0.0, segment_count, colour);
      //if (segment_count < 3 || radius1 < 1.0e-3 ) return; //Too small for spheres
      //  drawSphere(draw, pos, true, radius1, segment_count, colour);
    }
    // Finish with sphere, closes gaps in angled joins
    //Vec3d centre(coord1);
    //if (length > radius1 * 0.10)
    //  drawSphere(draw, centre, true, radius1, segment_count, colour);
  }

}

void Geometry::drawCuboid(DrawingObject *draw, Vec3d& min, Vec3d& max, Quaternion& rot, bool scale3d, Colour* colour)
{
  //float pos[3] = {min[0] + 0.5f*(max[0] - min[0]), min[1] + 0.5f*(max[1] - min[1]), min[2] + 0.5f*(max[2] - min[2])};
  Vec3d dims = max - min;
  Vec3d pos = min + dims * 0.5f;
  drawCuboidAt(draw, pos, dims, rot, scale3d, colour);
}

void Geometry::drawCuboidAt(DrawingObject *draw, Vec3d& pos, Vec3d& dims, Quaternion& rot, bool scale3d, Colour* colour)
{
  Vec3d min = dims * -0.5f; //Vec3d(-0.5f * width, -0.5f * height, -0.5f * depth);
  Vec3d max = min + dims; //Vec3d(min[0] + width, min[1] + height, min[2] + depth);
  
  //Get the geom ptr
  Geom_Ptr g = read(draw, 0, lucVertexData, NULL);
  unsigned int voffset = g->count();

  Vec3d iscale = Vec3d(1.0, 1.0, 1.0);
  if (scale3d)
    iscale = view->iscale;

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
    // Multiplying a quaternion q with a vector v applies the q-rotation to v
    verts[i] = rot * verts[i] * iscale;
    verts[i] += Vec3d(pos);
    g->checkPointMinMax(verts[i].ref());
  }

  unsigned int vertex_index = g->count();

  if (type == lucGridType)
  {
    //Front
    g->_indices->read1(vertex_index);
    g->_indices->read1(vertex_index+1);
    g->_indices->read1(vertex_index+2);
    g->_indices->read1(vertex_index+3);

    //Back
    g->_indices->read1(vertex_index+4);
    g->_indices->read1(vertex_index+7);
    g->_indices->read1(vertex_index+6);
    g->_indices->read1(vertex_index+5);

    //Bottom
    g->_indices->read1(vertex_index);
    g->_indices->read1(vertex_index+4);
    g->_indices->read1(vertex_index+5);
    g->_indices->read1(vertex_index+1);

    //Top
    g->_indices->read1(vertex_index+3);
    g->_indices->read1(vertex_index+2);
    g->_indices->read1(vertex_index+6);
    g->_indices->read1(vertex_index+7);

    //Left
    g->_indices->read1(vertex_index+4);
    g->_indices->read1(vertex_index);
    g->_indices->read1(vertex_index+3);
    g->_indices->read1(vertex_index+7);

    //Right
    g->_indices->read1(vertex_index+1);
    g->_indices->read1(vertex_index+5);
    g->_indices->read1(vertex_index+6);
    g->_indices->read1(vertex_index+2);
  }
  else
  {
    //Triangle indices

    //Front
    g->_indices->read1(vertex_index);
    g->_indices->read1(vertex_index+1);
    g->_indices->read1(vertex_index+2);

    g->_indices->read1(vertex_index+2);
    g->_indices->read1(vertex_index+3);
    g->_indices->read1(vertex_index);

    //Back
    g->_indices->read1(vertex_index+7);
    g->_indices->read1(vertex_index+6);
    g->_indices->read1(vertex_index+5);

    g->_indices->read1(vertex_index+5);
    g->_indices->read1(vertex_index+4);
    g->_indices->read1(vertex_index+7);

    //Bottom
    g->_indices->read1(vertex_index+4);
    g->_indices->read1(vertex_index+5);
    g->_indices->read1(vertex_index+1);

    g->_indices->read1(vertex_index+1);
    g->_indices->read1(vertex_index);
    g->_indices->read1(vertex_index+4);

    //Top
    g->_indices->read1(vertex_index+3);
    g->_indices->read1(vertex_index+2);
    g->_indices->read1(vertex_index+6);

    g->_indices->read1(vertex_index+6);
    g->_indices->read1(vertex_index+7);
    g->_indices->read1(vertex_index+3);

    //Left
    g->_indices->read1(vertex_index+4);
    g->_indices->read1(vertex_index);
    g->_indices->read1(vertex_index+3);

    g->_indices->read1(vertex_index+3);
    g->_indices->read1(vertex_index+7);
    g->_indices->read1(vertex_index+4);

    //Right
    g->_indices->read1(vertex_index+1);
    g->_indices->read1(vertex_index+5);
    g->_indices->read1(vertex_index+6);

    g->_indices->read1(vertex_index+6);
    g->_indices->read1(vertex_index+2);
    g->_indices->read1(vertex_index+1);

  }

  g->_vertices->read(8, verts[0].ref());

  if (colour) g->vertexColours(colour, voffset);
}

void Geometry::drawSphere(DrawingObject *draw, Vec3d& centre, bool scale3d, float radius, int segment_count, Colour* colour)
{
  //Case of ellipsoid where all 3 radii are equal
  Vec3d radii = Vec3d(radius, radius, radius);
  Quaternion qrot;
  drawEllipsoid(draw, centre, radii, qrot, scale3d, segment_count, colour);
}

// Create a 3d ellipsoid given centre point, 3 radii and number of triangle segments to use
// Based on algorithm and equations from:
// http://local.wasp.uwa.edu.au/~pbourke/texture_colour/texturemap/index.html
// http://paulbourke.net/geometry/sphere/
void Geometry::drawEllipsoid(DrawingObject *draw, Vec3d& centre, Vec3d& radii, Quaternion& rot, bool scale3d, int segment_count, Colour* colour)
{
  int i,j;
  Vec3d edge, pos, normal;
  float tex[2];

  if (radii.x < 0) radii.x = -radii.x;
  if (radii.y < 0) radii.y = -radii.y;
  if (radii.z < 0) radii.z = -radii.z;
  if (segment_count < 0) segment_count = -segment_count;
  session.cacheCircleCoords(segment_count);

  const Vec3d& scale3 = scale3d ? view->scale : Vec3d(1.0, 1.0, 1.0);
  const Vec3d& iscale = scale3d ? view->iscale : Vec3d(1.0, 1.0, 1.0);

  //Get the geom ptr
  Geom_Ptr g = read(draw, 0, lucVertexData, NULL);
  unsigned int voffset = g->count();

  for (j=0; j<segment_count/2; j++)
  {
    //Triangle strip vertices
    for (i=0; i<=segment_count; i++)
    {
      int vertex_index = g->count();
      // Get index from pre-calculated coords which is back 1/4 circle from j+1 (same as forward 3/4circle)
      int circ_index = ((int)(1 + j + 0.75 * segment_count) % segment_count);
      edge = Vec3d(session.y_coords[circ_index] * session.y_coords[i], session.x_coords[circ_index], session.y_coords[circ_index] * session.x_coords[i]);
      pos = centre + rot * (radii * edge) * iscale;

      tex[0] = i/(float)segment_count;
      tex[1] = 2*(j+1)/(float)segment_count;

      //Read triangle vertex, normal, texcoord
      g->readVertex(pos.ref());
      normal = rot * -edge * scale3;
      g->_normals->read(1, normal.ref());
      g->_texCoords->read(1, tex);

      // Get index from pre-calculated coords which is back 1/4 circle from j (same as forward 3/4circle)
      circ_index = ((int)(j + 0.75 * segment_count) % segment_count);
      edge = Vec3d(session.y_coords[circ_index] * session.y_coords[i], session.x_coords[circ_index], session.y_coords[circ_index] * session.x_coords[i]);
      pos = centre + rot * (radii * edge) * iscale;

      tex[0] = i/(float)segment_count;
      tex[1] = 2*j/(float)segment_count;

      //Read triangle vertex, normal, texcoord
      g->readVertex(pos.ref());
      normal = rot * -edge * scale3;
      g->_normals->read(1, normal.ref());
      g->_texCoords->read(1, tex);

      //Triangle strip indices
      if (i > 0)
      {
        //First tri
        g->_indices->read1(vertex_index-2);
        g->_indices->read1(vertex_index-1);
        g->_indices->read1(vertex_index);
        //Second tri
        g->_indices->read1(vertex_index-1);
        g->_indices->read1(vertex_index+1);
        g->_indices->read1(vertex_index);
      }
    }
  }

  if (colour) g->vertexColours(colour, voffset);
}

//Class to handle geometry types with sub-geometry glyphs drawn
//at vertices consisting of lines and/or triangles
Glyphs::Glyphs(Session& session) : Geometry(session)
{
  //Create sub-renderers
  //All renderers are switchable and user defined based on "glyphrenderlist" global property
  std::string renderlist = session.global("glyphrenderlist");
  std::istringstream iss(renderlist);
  std::string s;
  while (getline(iss, s, ' '))
  {
    //std::cout << "CREATING: " << s << std::endl;
    Geometry* renderer = createRenderer(session, s);
    renderer->internal = true;

    switch (renderer->type)
    {
      case lucLineType:
        lines = (Lines*)renderer;
        break;
      case lucTriangleType:
        tris = (Triangles*)renderer;
        break;
      case lucPointType:
        points = (Points*)renderer;
        break;
      default:
        delete renderer;
    }
  }

  //Placeholder null renderers
  if (!lines) lines = (Lines*)(new Geometry(session));
  if (!tris) tris = (Triangles*)(new Geometry(session));
  if (!points) points = (Points*)(new Geometry(session));

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
  if (!session.global("gpucache"))
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

void Glyphs::remove(DrawingObject* draw)
{
  lines->remove(draw);
  tris->remove(draw);
  points->remove(draw);
  Geometry::remove(draw);
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

void Glyphs::display(bool refresh)
{
  tris->redraw = redraw;
  tris->reload = reload;
  lines->redraw = redraw;
  lines->reload = reload;
  points->redraw = redraw;
  points->reload = reload;

  if (geom.size() > 0 && (geom[0]->texture->texture || geom[0]->texture->source))
  {
    tris->setTexture(geom[0]->draw, geom[0]->texture);
    lines->setTexture(geom[0]->draw, geom[0]->texture);
    points->setTexture(geom[0]->draw, geom[0]->texture);
  }

  //Need to call to clear the cached object
  lines->display();
  tris->display();
  points->display();

  if (!reload && session.global("gpucache"))
  {
    //Skip re-gen of internal geometry shapes
    redraw = false;
  }

  //Render in parent display call
  Geometry::display(refresh);
}

void Glyphs::update()
{
  //No fixed or time varying support
  tris->merge(session.now, session.now);
  lines->merge(session.now, session.now);
  points->merge(session.now, session.now);

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
    tris->draw();
}

void Glyphs::jsonWrite(DrawingObject* draw, json& obj)
{
  tris->jsonWrite(draw, obj);
  lines->jsonWrite(draw, obj);
  points->jsonWrite(draw, obj);
}

Imposter::Imposter(Session& session) : Geometry(session)
{
  vbo = 0;
  type = lucScreenType;
}

Imposter::~Imposter()
{
  close();
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
  glVertexPointer(3, GL_FLOAT, 5 * sizeof(float), (GLvoid*)0); // Load vertex x,y,z only
  glEnableClientState(GL_VERTEX_ARRAY);
  glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(float), (GLvoid*)(3*sizeof(float))); // Load texcoord x,y
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
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
    float verts_texcoords[20] = {1,1,0,  1,0,
                                -1,1,0,  0,0,
                                 1,-1,0, 1,1,
                                -1,-1,0, 0,1};
    //Initialise vertex buffer
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (glIsBuffer(vbo))
      glBufferData(GL_ARRAY_BUFFER, sizeof(verts_texcoords), verts_texcoords, GL_STATIC_DRAW);
  }
}

void FullScreen::draw()
{
  for (int i=0; i<geom.size(); i++)
  {
    if (!drawable(i)) continue;

    setState(i); //Set draw state settings for this object
    Imposter::draw();

    GL_Error_Check;
  }
}

Geometry* createRenderer(Session& session, const std::string& what)
{
  if (what == "points") //TODO: unsorted points base class
    return new Points(session);
  if (what == "sortedpoints" || what == "particles")
    return new Points(session);
  if (what == "spheres")
    return new Spheres(session);
  if (what == "labels")
    return new Geometry(session);
  if (what == "vectors" || what == "arrows")
    return new Vectors(session);
  if (what == "tracers" || what == "streamlines")
    return new Tracers(session);
  if (what == "mesh" || what == "surface")
    return new Triangles(session);
  if (what == "triangles" || what == "sortedtriangles") //NOTE: default is sorted, but will use any triangle renderer
    return new TriSurfaces(session);
  if (what == "quads" || what == "grid")
    return new QuadSurfaces(session);
  if (what == "shapes")
    return new Shapes(session);
  if (what == "lines")
    return new Lines(session);
  if (what == "sortedlines")
    return new LinesSorted(session);
  if (what == "links" || what == "tubes")
    return new Links(session);
  if (what == "volume" || what == "volumes")
    return new Volumes(session);
  if (what == "screen" || what == "fullscreen")
    return new FullScreen(session);
  //Default renderer, can just plot labels etc
  return new Geometry(session);
}


