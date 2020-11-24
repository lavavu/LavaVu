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

#include "ColourMap.h"

//Safe log function for scaling
#define LOG10(val) (val > FLT_MIN ? log10(val) : log10(FLT_MIN))

//This can stay global as never actually modified, if it needs to be then move to State
int ColourMap::samples = 4096;

std::ostream & operator<<(std::ostream &os, const ColourVal& cv)
{
  return os << cv.value << " --> " << cv.position << "=" << cv.colour;
}

ColourMap::ColourMap(Session& session, std::string name, std::string props)
  : name(name), properties(session.globals, session.defaults)
{
  precalc = new Colour[samples];
  background.value = 0xff000000;

  session.parseSet(properties, props);

  if (properties.has("colours"))
  {
    loadPaletteJSON(properties["colours"]);
    //Erase the colour list? Or keep it for export?
    //properties.data.erase("colours");
  }
}

bool ColourMap::loadPaletteJSON(json& data)
{
  //Load from json object/array
  if (data.is_object())
  {
    //"background" + "colours"
    if (data.count("background") > 0)
    {
      colours.clear();
      add(data["background"]);
      background = colours[0].colour;
    }
    return loadPaletteJSON(data["colours"]);
  }
  else if (data.is_array())
  {
    colours.clear();
    noValues = false;
    for (auto el : data)
      add(el);
    return colours.size() > 1;
  }
  else if (data.is_string())
  {
    std::string s = data;
    return loadPalette(s);
  }
  return false;
}

bool ColourMap::loadPalette(std::string data)
{
  if (data.length() == 0) return false;
  //Types of data accepted
  // a) Name of predefined colour map, if preceded with @ position data is ignored
  //   and only colours loaded at even spacing
  // b) Space separated colour list
  // c) Position=rgba colour palette
  // d) JSON arrays of [r,g,b(,a)] or [position, [r,g,b(,a)]] values
  colours.clear();
  bool nopos = false;
  if (data.at(0) == '[' || data.at(0) == '{')
  {
    //Load from json array
    json j = json::parse(data);
    return loadPaletteJSON(j);
  }
  else
  {
    noValues = false;
    calibrated = false;

    if (data.at(0) == '@')
    {
      //Strip position data after loading
      data = data.substr(1);
      nopos = true;
    }

    std::string map = getDefaultMap(data);
    if (map.length() > 0) data = map;

    if (data.find("=") == std::string::npos)
    {
      //Parse a whitespace separated list of colours with optional preceeding values
      const char *breakChars = " \t\n;";
      char *charPtr;
      char colourStr[64];
      char *colourString = new char[data.size()+1];
      strcpy(colourString, data.c_str());
      char *colourMap_ptr = colourString;
      charPtr = strtok(colourMap_ptr, breakChars);
      while (charPtr != NULL)
      {
        float value = 0.0;

        // Parse out value if provided, otherwise assign a default
        // input string: (OPTIONAL-VALUE)colour
        if (sscanf(charPtr, "(%f)%s", &value, colourStr) == 2)
        {
          //Add parsed colour to the map with value
          add(colourStr, value);
        }
        else
        {
          //Add parsed colour to the map
          add(charPtr);
        }

        charPtr = strtok(NULL, breakChars);
      }
      delete[] colourString;
    }
    else
    {
      //Parse a list of position=value colours, newline or semi-colon separated
      //Currently only support loading palettes with literal position data, not values to scale
      noValues = true;
      //Parse palette string into key/value pairs
      std::replace(data.begin(), data.end(), ';', '\n'); //Allow semi-colon separators
      std::stringstream is(data);
      std::string line;
      while(std::getline(is, line))
      {
        std::istringstream iss(line);
        float pos;
        char delim;
        std::string value;
        if (iss >> pos && pos >= 0.0 && pos <= 1.0)
        {
          iss >> delim;
          std::getline(iss, value); //Read rest of stream into value
          Colour colour(value);
          //Add to colourmap
          addAt(colour, pos);
        }
        else
        {
          //Background?
          std::size_t pos = line.find("=") + 1;
          if (line.substr(0, pos) == "Background=")
          {
            Colour c(line.substr(pos));
            background = c;
          }
        }
      }
    }
  }

  //Ensure at least two colours
  bool valid = colours.size() > 1;
  if (colours.size() == 0)
    add(0xff000000);
  if (colours.size() == 1)
    add(colours[0].colour.value);

  //Strip positions
  if (nopos) noValues = true;
  return valid; //Data was a valid colourmap
}

void ColourMap::addAt(Colour& colour, float position)
{
  //Adds by position, not "value"
  colours.push_back(ColourVal(colour));
  colours[colours.size()-1].value = HUGE_VAL;
  colours[colours.size()-1].position = position;
  //Flag positions provided
  noValues = true;
}

void ColourMap::add(std::string colour)
{
  Colour c(colour);
  colours.push_back(ColourVal(c));
  //std::cerr << colour << " : " << colours[colours.size()-1] << std::endl;
}

void ColourMap::add(std::string colour, float pvalue)
{
  Colour c(colour);
  colours.push_back(ColourVal(c, pvalue));
}

void ColourMap::add(unsigned int colour)
{
  Colour c;
  c.value = colour;
  colours.push_back(ColourVal(c));
}

void ColourMap::add(unsigned int* colours, int count)
{
  for (int i=0; i<count; i++)
    add(colours[i]);
}

void ColourMap::add(unsigned int colour, float pvalue)
{
  Colour c;
  c.value = colour;
  colours.push_back(ColourVal(c, pvalue));
}

void ColourMap::add(json& entry, float pos)
{
  //std::cout << pos << " : " << entry << std::endl;
  if (entry.is_object())
  {
    float pos = entry["position"];
    Colour colour;
    colour.fromJSON(entry["colour"]);
    if (pos < 0.0)
      add(colour.value);
    else
      addAt(colour, pos);
  }
  else if (entry.size() == 1)
  {
    if (entry.is_number())
    {
      //Single numeric value, load as shade and opacity
      float f = entry;
      if (f <= 1.0) f *= 255;
      Colour colour(f, f, f, f);
      if (pos < 0.0)
        add(colour.value);
      else
        addAt(colour, pos);
    }
    else
    {
      //Parse colour string
      std::string s = entry;
      if (pos < 0.0)
        add(s);
      else
      {
        Colour colour(s);
        addAt(colour, pos);
      }
    }
  }
  else if (entry.size() == 2)
  {
    //Position, colour
    float pos = entry[0];
    //Recursive add json colour
    add(entry[1], pos);
  }
  else if (entry.size() >= 3)
  {
    Colour colour;
    colour.fromJSON(entry);
    if (pos < 0.0)
      add(colour.value);
    else
      addAt(colour, pos);
  }
}

void ColourMap::calc()
{
  if (!colours.size()) return;

  //Check for transparency
  opaque = true;
  for (unsigned int i=0; i<colours.size(); i++)
  {
    if (colours[i].colour.a < 255)
    {
      opaque = false;
      break;
    }
  }

  //Precalculate colours
  if (log)
    for (int cv=0; cv<samples; cv++)
      precalc[cv] = get(pow(10, log10(minimum) + range * (float)cv/(samples-1)));
  else
    for (int cv=0; cv<samples; cv++)
      precalc[cv] = get(minimum + range * (float)cv/(samples-1));
}

void ColourMap::calibrate(float min, float max)
{
  //Skip calibration if min/max unchanged
  if (!noValues && calibrated && min == minimum && max == maximum) return;
  //No colours?
  if (colours.size() == 0) return;
  //Skip calibration when locked
  if (properties["locked"]) return;

  if (min == HUGE_VAL) min = max;
  if (max == HUGE_VAL) max = min;

  discrete = properties["discrete"];
  interpolate = properties["interpolate"];
  if (discrete)
  {
    if (max - (int)max != 0.0) max = ceil(max);
    if (min - (int)min != 0.0) min = floor(min);
    debug_print("Discrete colour map %s min %.0f, max %.0f\n", name.c_str(), minimum, maximum);
  }

  minimum = min;
  maximum = max;
  log = properties["logscale"];
  if (log)
    range = LOG10(maximum) - LOG10(minimum);
  else
    range = maximum - minimum;
  irange = 1.0 / range;

  //Calculates positions based on field values over range
  if (!noValues)
  {
    colours[0].position = 0;
    colours.back().position = 1;
    colours[0].value = minimum;
    colours.back().value = maximum;

    // Get scaled positions for colours - Scale the values to find colour bar positions [0,1]
    float inc;
    for (unsigned int i=1; i < colours.size()-1; i++)
    {
      // Found an empty value
      if (colours[i].value == HUGE_VAL)
      {
        // Search for next provided value
        //printf("Empty value at %d ", i);
        unsigned int j;
        for (j=i+1; j < colours.size(); j++)
        {
          if (colours[j].value != HUGE_VAL)
          {
            // Scale to get new position, unless at max pos
            if (j < colours.size()-1)
              colours[j].position = scaleValue(colours[j].value);

            //printf(", next value found at %d, ", j);
            inc = (colours[j].position - colours[i - 1].position) / (j - i + 1);
            for (unsigned int k = i; k < j; k++)
            {
              colours[k].position = colours[k-1].position + inc;
              //printf("Interpolating at %d from %f by %f to %f\n", k, colours[k-1].position, inc, colours[k].position);
            }
            break;
          }
        }
        // Continue search from j
        i = j;
      }
      else
        // Value found, scale to get position
        colours[i].position = scaleValue(colours[i].value);
    }
  }

  //Ensure position data is valid - sorted and ranging [0,1]
  std::sort(colours.begin(), colours.end());
  if (colours[0].position != 0.0)
    colours[0].position = 0.0;
  if (colours.size() > 1 && colours.back().position != 1.0)
    colours.back().position = 1.0;

  //Calc values now colours have been added
  calc();

  debug_print("ColourMap %s calibrated min %f, max %f, range %f ==> %d colours\n", name.c_str(), minimum, maximum, range, colours.size());

  //for (int i=0; i < colours.size(); i++)
  //  std::cout << " colour " << colours[i].colour << " value " << colours[i].value << " pos " << colours[i].position << std::endl;

  calibrated = true;
}

//Calibration from set "range" property or geom data set
void ColourMap::calibrate(Range* dataRange)
{
  //Check has range property and is valid
  bool hasRange = properties.has("range");
  Range range;
  Properties::toArray<float>(properties["range"], range.data(), 2);
  //Ignore invalid range or where min == max (default for range property)
  if (!range.valid() || range.minimum == range.maximum)
    hasRange = false;

  //Has values and no fixed range, calibrate to data
  if (dataRange && !hasRange)
    calibrate(dataRange->minimum, dataRange->maximum);
  //Otherwise calibrate to fixed range if provided
  else if (hasRange)
    calibrate(range.minimum, range.maximum);
  //Otherwise calibrate with existing values
  else
    calibrate(minimum, maximum);
}

float ColourMap::scalefast(float value)
{
  if (log)
    return irange * ((LOG10(value) - LOG10(minimum)));
  else
    return irange * ((value - minimum));
}

Colour ColourMap::getfast(float value)
{
  //NOTE: value caching DOES NOT WORK for log scales!
  //If this is causing slow downs in future, need a better method
  int c = 0;
  if (log)
    c = (int)((samples-1) * irange * ((LOG10(value) - LOG10(minimum))));
  else
    c = (int)((samples-1) * irange * ((value - minimum)));
  if (c > samples - 1) c = samples - 1;
  if (c < 0) c = 0;
  //std::cerr << value << " range : " << range << " : min " << minimum << ", max " << maximum << ", pos " << c << ", Colour " << precalc[c] << " uncached " << get(value) << std::endl;
  //std::cerr << LOG10(value) << " range : " << range << " : Lmin " << LOG10(minimum) << ", max " << LOG10(maximum) << ", pos " << c << ", Colour " << precalc[c] << " uncached " << get(value) << std::endl;
  return precalc[c];
}

Colour ColourMap::get(float value)
{
  return getFromScaled(scaleValue(value));
}

float ColourMap::scaleValue(float value)
{
  float min = minimum, max = maximum;

  if (discrete) 
    value = round(value);

  if (max == min) return 0.5;   // Force central value if no range
  if (value <= min) return 0.0;
  if (value >= max) return 1.0;

  if (log)
  {
    value = LOG10(value);
    min = LOG10(minimum);
    max = LOG10(maximum);
  }

  //Scale to range [0,1]
  return (value - min) / (max - min);
}

Colour ColourMap::getFromScaled(float scaledValue)
{
  //printf(" scaled %f\n", scaledValue);
  if (colours.size() == 0) return Colour();
  if (colours.size() == 1) return colours[0].colour;
  // Check within range
  if (scaledValue >= 1.0)
    return colours.back().colour;
  else if (scaledValue >= 0)
  {
    // Find the colour/values our value lies between
    unsigned int i;
    for (i=0; i<colours.size(); i++)
    {
      if (fabs(colours[i].position - scaledValue) <= FLT_EPSILON)
        return colours[i].colour;

      if (colours[i].position > scaledValue) break;
    }

    if (i==0 || i==colours.size()) 
      abort_program("ColourMap %s (%d) Colour position %f not in range [%f,%f] min %f max %f", name.c_str(), (int)colours.size(), scaledValue, colours[0].position, colours.back().position, minimum, maximum);

    //Just return the nearest colour
    //if (scaledValue == 0.5) printf("%f -- %d\n", scaledValue, interpolate);
    if (!interpolate)
    {
      //printf("NOT INTERPOLATING for %f\n", scaledValue);
      float p1 = scaledValue - colours[i-1].position;
      float p2 = colours[i].position - scaledValue;
      if (p1 < p2)
        return colours[i-1].colour;
      else
        return colours[i].colour;
    }

    // Calculate interpolation factor [0,1] between colour at index and previous colour
    float mu = (scaledValue - colours[i-1].position) / (colours[i].position - colours[i-1].position);

    //Linear interpolation between colours
    //printf(" interpolate %f above %f below %f\n", interpolate, colours[i].position, colours[i-1].position);
    Colour colour0, colour1;
    colour0 = colours[i-1].colour;
    colour1 = colours[i].colour;

    for (int c=0; c<4; c++)
      colour0.rgba[c] += (colour1.rgba[c] - colour0.rgba[c]) * mu;

    return colour0;
  }

  Colour c;
  c.value = 0;
  return c;
}

#define VERT2D(x, y, c, swap) if (swap) vertices.push_back(ColourVert2d(y, x, c)); else vertices.push_back(ColourVert2d(x, y, c));
#define RECT2D(x0, y0, x1, y1, c, s) { VERT2D(x0, y1, c, s); VERT2D(x0, y0, c, s); VERT2D(x1, y1, c, s); VERT2D(x0, y0, c, s); VERT2D(x1, y0, c, s); VERT2D(x1, y1, c, s);}

void ColourMap::drawVertices(Session& session, std::vector<ColourVert2d>& vertices, GLenum primitive, bool flat)
{
  //Initialise vertex array object for OpenGL 3.2+
  GL_Error_Check;
  if (!vao) glGenVertexArrays(1, &vao);
  GL_Error_Check;
  glBindVertexArray(vao);
  if (!vbo) glGenBuffers(1, &vbo);
  int stride = 2 * sizeof(float) + sizeof(Colour);   //2d vertices + 32-bit colour
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  session.context.useDefaultShader(true, flat);
  if (glIsBuffer(vbo))
  {
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * stride, vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(primitive, 0, vertices.size());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
  GL_Error_Check;

  //Reset
  vertices.clear();
}

void ColourMap::draw(Session& session, Properties& colourbarprops, int startx, int starty, int length, int breadth, Colour& printColour, bool vertical)
{
  session.context.push();
#ifndef __EMSCRIPTEN__
  glDisable(GL_MULTISAMPLE);
#endif
  glDisable(GL_CULL_FACE);

  std::vector<ColourVert2d> vertices;

  if (!calibrated) calibrate();

  // Draw larger background box for border, use font colour
  Colour colour = session.fonts.setFont(colourbarprops);
  if (colour.a == 0.0) colour = printColour; //Use the user defined font colour if valid, otherwise default print colour
  int border = colourbarprops["outline"];
  if (border > 0)
    RECT2D(startx - border, starty - border, startx + length + border, starty + breadth + border, colour, vertical);

  //Draw background (checked)
  for (int pixel_J = 0; pixel_J < breadth; pixel_J += 5)
  {
    colour.value = 0xff666666;
    if (pixel_J % 2 == 1) colour.invert();
    for (int pixel_I = 0 ; pixel_I <= length ; pixel_I += 5)
    {
      int end = startx + pixel_I + breadth;
      if (end > startx + length) end = startx + length;
      int endy = starty + pixel_J + 5;
      if (endy > starty + breadth) endy = starty + breadth;

      RECT2D(startx + pixel_I, starty + pixel_J, end, endy, colour, vertical);
      colour.invert();
    }
  }

  drawVertices(session, vertices, GL_TRIANGLES);

  // Draw Colour Bar
  int count = colours.size();
  int idx, xpos;
  int discretevals = 0;
  float midpos;
  if (discrete)
  {
    discretevals =  maximum - minimum + 1;
    xpos = startx;
    colour = getFromScaled(0);
    VERT2D(xpos, starty, colour, vertical);
    VERT2D(xpos, starty + breadth, colour, vertical);
    for (idx = 0; idx < discretevals+1; idx++)
    {
      int oldx = xpos;
      float dval = minimum + idx - 1;
      colour = get(dval);
      xpos = startx + length * idx/(float)(discretevals);
      VERT2D(oldx + (xpos - oldx), starty, colour, vertical);
      VERT2D(oldx + (xpos - oldx), starty + breadth, colour, vertical);
    }
  }
  else if (!interpolate)
  {
    colour = getFromScaled(0);
    VERT2D(startx, starty, colour, vertical);
    VERT2D(startx, starty + breadth, colour, vertical);
    for (idx = 1; idx < count; idx++)
    {
      midpos = (colours[idx-1].position + 0.5 * (colours[idx].position - colours[idx-1].position));
      xpos = startx + length * midpos;
      colour = colours[idx-1].colour;
      VERT2D(xpos, starty, colour, vertical);
      VERT2D(xpos, starty + breadth, colour, vertical);

      colour = colours[idx].colour;
      VERT2D(xpos+1, starty, colour, vertical);
      VERT2D(xpos+1, starty + breadth, colour, vertical);
    }
    colour = getFromScaled(1);
    VERT2D(startx+length, starty, colour, vertical);
    VERT2D(startx+length, starty + breadth, colour, vertical);
  }
  else
  {
    for (idx = 0; idx < count; idx++)
    {
      colour = getFromScaled(colours[idx].position);
      xpos = startx + length * colours[idx].position;
      VERT2D(xpos, starty, colour, vertical);
      VERT2D(xpos, starty + breadth, colour, vertical);
    }
  }

  //Send the buffer
  drawVertices(session, vertices, GL_TRIANGLE_STRIP, discrete);

  //Labels / tick marks
  colour = session.fonts.setFont(colourbarprops);
  if (colour.a == 0.0) colour = printColour; //Use the user defined font colour if valid, otherwise default print colour
  session.fonts.colour = colour;
  float tickValue;
  unsigned int ticks = colourbarprops["ticks"];
  json tickValues = colourbarprops["tickvalues"];
  if (!tickValues.is_array()) tickValues = json::array({tickValues});
  if (tickValues.size() > ticks) ticks = tickValues.size();
  bool printTicks = colourbarprops["printticks"];
  std::string format = colourbarprops["format"];
  float scaleval = colourbarprops["scalevalue"];
  bool binLabels = colourbarprops["binlabels"];

  //Always show at one tick on a log scale
  if (log && ticks < 1) ticks = 1;
  //Label interval bins
  if (discrete && binLabels) ticks = discretevals-1;
  // No ticks if no range
  if (minimum == maximum) ticks = 0;
  for (unsigned int i = 0; i < ticks+2; i++)
  {
    /* Get tick value */
    float scaledPos = 0.0;
    if (i==0)
    {
      /* Start tick */
      tickValue = minimum;
      scaledPos = 0;
      if (minimum == maximum) scaledPos = 0.5;
    }
    else if (i==ticks+1)
    {
      /* End tick */
      tickValue = maximum;
      scaledPos = 1;
      if (minimum == maximum) continue;
    }
    else
    {
      /* Calculate tick position */
      if (i > tickValues.size())  /* No fixed value provided */
      {
        /* First get scaled position 0-1 */
        if (log)
        {
          /* Space ticks based on a logarithmic scale of log(1) to log(11)
             shows non-linearity while keeping ticks spaced apart enough to read labels */
          float tickpos = 1.0f + (float)i * (10.0f / (ticks+1));
          scaledPos = (LOG10(tickpos) / LOG10(11.0f));
        }
        else
          /* Default linear scale evenly spaced ticks */
          scaledPos = (float)i / (ticks+1);

        /* Compute the tick value */
        if (log)
        {
          /* Reverse calc to find tick value at calculated position 0-1: */
          tickValue = LOG10(minimum) + scaledPos
                      * (LOG10(maximum) - LOG10(minimum));
          tickValue = pow( 10.0f, tickValue );
        }
        else
        {
          /* Reverse scale calc and find value of tick at position 0-1 */
          /* (Using even linear scale) */
          tickValue = minimum + scaledPos * (maximum - minimum);
        }
      }
      else if (tickValues[i-1].is_number())
      {
        /* User specified value */
        /* Calculate scaled position from value */
        tickValue = tickValues[i-1];
        scaledPos = scaleValue(tickValue);
      }

      /* Skip middle ticks if not inside range */
      if (scaledPos == 0 || scaledPos == 1) continue;
    }

    /* Calculate pixel position */
    int xpos = startx + length * scaledPos;

    /* Draws the tick */
    int offset = 0;
    if (vertical) offset = breadth+5;
    int ts = starty-5+offset;
    int te = starty+offset;
    //Full breadth ticks at ends
    if (scaledPos != 0.5 && (i==0 || i==ticks+1))
    {
      if (vertical) ts-=breadth; else te+=breadth;
    }
    //Outline tweak
    if (i==0) xpos -= border;
    if (i==ticks+1) xpos += border-1; //-1 tweak or will be offset from edge

    RECT2D(xpos, ts, xpos+1, te, colour, vertical);

    //Discrete: label bin not tick
    if (binLabels)
    {
      if (i==ticks+1) break; //Draw one less label
      xpos += 0.5*length/(float)ticks;
      scaledPos = (float)i / (ticks);
      tickValue = minimum + scaledPos * (maximum - minimum);
    }

    /* Always print end values, print others if flag set */
    char string[20];
    if (printTicks || i == 0 || i == ticks+1)
    {
      /* Apply any scaling factor  and show units on output */
      if (fabs(tickValue) <= FLT_MIN )
        sprintf( string, "0" );
      else
      {
        // For display purpose, scales the printed values if needed
        tickValue = scaleval * tickValue;
        sprintf(string, format.c_str(), tickValue);
      }

      session.context.push();
#ifndef __EMSCRIPTEN__
      glEnable(GL_MULTISAMPLE);
#endif
      float shiftf = (float)session.fonts.printWidth("T");
      if (vertical)
        session.fonts.print(starty + breadth + 8 + 0.25 * shiftf, xpos - (int) (0.75 * shiftf),  string);
      else
        session.fonts.print(xpos - (int) (0.5 * (float)session.fonts.printWidth(string)), starty - 6 - 1.25 * shiftf, string);
#ifndef __EMSCRIPTEN__
      glDisable(GL_MULTISAMPLE);
#endif
      session.context.pop();
    }
  }

  drawVertices(session, vertices, GL_TRIANGLES);

#ifndef __EMSCRIPTEN__
  glEnable(GL_MULTISAMPLE);
#endif

  session.context.pop();
}

void ColourMap::setComponent(int component_index)
{
  //Clear other components of colours
  for (unsigned int i=0; i<colours.size(); i++)
    for (int c=0; c<3; c++)
      if (c != component_index) colours[i].colour.rgba[c] = 0;
}

void ColourMap::loadTexture(bool repeat)
{
  if (!texture) texture = new ImageLoader();
  texture->filter = 0; //Nearest neighbour filtering
  texture->repeat = repeat;
  ImageData* paletteData = toImage(repeat);
  texture->load(paletteData);
  delete paletteData;
}

ImageData* ColourMap::toImage(bool repeat)
{
  ImageData* paletteData = new ImageData(samples, 1, 4);
  Colour col;
  if (!calibrated) calibrate();
  for (int i=0; i<samples; i++)
  {
    col = getFromScaled(i / (float)(samples-1));
    //if (i%64==0) printf("RGBA %d %d %d %d\n", col.r, col.g, col.b, col.a);
    memcpy(&paletteData->pixels[i*4], col.rgba, 4);
  }

  return paletteData;
}

json ColourMap::toJSON()
{
  json cmap = properties.data;
  json cj;

  if (!calibrated)
    calibrate();

  for (unsigned int c=0; c < colours.size(); c++)
  {
    json colour;
    //Hack to clean up unnecessary float precision rubbish in json output of positions
    std::stringstream ps;
    ps << colours[c].position;
    colour["position"] = json::parse(ps.str());
    colour["colour"] = colours[c].colour.toString();
    cj.push_back(colour);
  }

  cmap["name"] = name;
  cmap["colours"] = cj;

  return cmap;
}

std::string ColourMap::toString()
{
  std::stringstream ss;
  for (unsigned int idx = 0; idx < colours.size(); idx++)
    ss << colours[idx].position << "=" << colours[idx].colour << std::endl;
  return ss.str();
}

void ColourMap::print()
{
  std::cout << toString() << std::endl;
}

void ColourMap::flip()
{
  std::reverse(colours.begin(), colours.end());
  for (unsigned int idx = 0; idx < colours.size(); idx++)
    colours[idx].position = 1.0 - colours[idx].position;
}

void ColourMap::monochrome()
{
  for (unsigned int i=0; i<colours.size(); i++)
  {
    //Convert RGBA to perceived greyscale luminance
    //cdf. http://jakevdp.github.io/blog/2014/10/16/how-bad-is-your-colormap/
    //cf. http://alienryderflex.com/hsp.html
    Colour col = colours[i].colour;
    float luminance = sqrt(0.299*col.r*col.r + 0.587*col.g*col.g + 0.114*col.b*col.b);
    for (int c=0; c<3; c++)
       colours[i].colour.rgba[c] = luminance;
  }
}

std::string ColourMap::getDefaultMap(std::string name)
{
  for (unsigned int i=0; i<ColourMap::defaultMapNames.size(); i++)
    if (ColourMap::defaultMapNames[i] == name)
      return ColourMap::defaultMaps[i];
  return "";
}

/* Some preset colourmaps
*   aim to reduce banding artifacts by being either 
* - isoluminant
* - smoothly increasing in luminance
* - diverging in luminance about centre value
* - others from GMT colour tables
*/
std::vector<std::string> ColourMap::defaultMapNames = { 
"diverge", "isolum", "isorainbow", "cubelaw", "cubelaw2", "smoothheat", "coolwarm", "spectral",
"drywet", "elevation", "dem1", "dem2", "dem3", "dem4", "ocean", "bathy", "seafloor", "abyss", "ibcso", "gebco",
"topo", "sealand", "nighttime", "world", "geo", "terra", "relief", "globe", "earth", "etopo1", 
"cubhelix", "hot", "cool", "copper", "gray", "split", "polar", "red2green", 
"paired", "categorical", 
"haxby", "jet", "panoply", "no_green", "wysiwyg", "seis", "rainbow", "nih"
};

std::vector<std::string> ColourMap::defaultMaps = { 
//Diverging blue-yellow-orange
"#288FD0 #fbfb9f #FF7520",
//Isoluminant blue-orange
"#288FD0 #50B6B8 #989878 #C68838 #FF7520",
//Isoluminant rainbow blue-green-orange
"#5ed3ff #6fd6de #7ed7be #94d69f #b3d287 #d3ca7b #efc079 #ffb180",
//CubeLaw indigo-blue-green-yellow
"#440088 #831bb9 #578ee9 #3db6b6 #6ce64d #afeb56 #ffff88",
//CubeLaw indigo-blue-green-orange-yellow
"#440088 #1b83b9 #6cc35b #ebbf56 #ffff88",
//CubeLaw heat blue-magenta-yellow
"#440088 #831bb9 #c66f5d #ebbf56 #ffff88",
//Paraview cool-warm (diverging blue-red)
"#3b4cc0 #7396f5 #b0cbfc #dcdcdc #f6bfa5 #ea7b60 #b50b27",
//Spectral
"#9e0142 #d53e4f #f46d43 #fdae61 #fee08b #ffffbf #e6f598 #abdda4 #66c2a5 #3288bd #5e4fa2",
//drywet
"#86612a #eec764 #b4ee87 #32eeeb #0c78ee #2601b7 #083371 ",
//elevation
"0.00000=rgb(105,152,133); 0.00714=rgb(118,169,146); 0.00814=rgb(118,169,146); 0.02857=rgb(131,181,155); 0.02957=rgb(131,181,155); 0.08571=rgb(165,192,167); 0.08671=rgb(165,192,167); 0.14286=rgb(211,201,179); 0.14386=rgb(211,201,179); 0.28571=rgb(212,184,164); 0.28671=rgb(212,184,164); 0.42857=rgb(212,192,181); 0.42957=rgb(212,192,181); 0.57143=rgb(214,209,206); 0.57243=rgb(214,209,206); 0.71429=rgb(222,221,220); 0.71529=rgb(222,221,220); 0.85714=rgb(238,237,236); 0.85814=rgb(238,237,236); 1.00000=rgb(247,246,245); ",
//dem1
"#336600 #81c31f #ffffcc #f4bd45 #66330c #663300 #fffefd ",
//dem2
"0.00000=rgb(0,97,71); 0.01020=rgb(16,122,47); 0.01120=rgb(16,122,47); 0.10204=rgb(232,215,125); 0.10304=rgb(232,215,125); 0.24490=rgb(161,67,0); 0.24590=rgb(161,67,0); 0.34694=rgb(100,50,25); 0.34794=rgb(100,50,25); 0.57143=rgb(110,109,108); 0.57243=rgb(110,109,108); 0.81633=rgb(255,254,253); 0.81733=rgb(255,254,253); 1.00000=rgb(255,254,253); ",
//dem3
"mediumseagreen burlywood darkorange3 saddlebrown bisque2 ivory2 snow ",
//dem4
"#aeefd5 #aff0d3 #b0f2d0 #b0f2ca #b1f2c4 #b0f3be #b0f4ba #b2f6b5 #b5f6b2 #baf7b2 #c0f7b2 #c6f8b2 #ccf9b2 #d2fab1 #d9fab2 #e0fbb2 #e7fcb2 #eefcb3 #f5fcb3 #fafcb2 #f8f9ac #eef4a2 #e2f097 #d5eb8c #c6e480 #b8de76 #aad86c #9ad362 #8ccd59 #7dc752 #6ec24a #5ebc42 #4db639 #3eb032 #31ab2c #27a52a #1ea02b #189a2e #129431 #0e8e34 #098938 #07843c #0c823f #18823f #28843d #34883c #408c3b #4c8e3b #579238 #639436 #6e9634 #789a32 #809c30 #89a02e #93a22b #9ca429 #a6a627 #b0aa24 #bbad22 #c5b01e #cfb11c #dab318 #e4b414 #eeb60e #f6b608 #f8b004 #f4a602 #ee9b02 #e89002 #e28402 #dc7a02 #d86f02 #d36602 #ce5c02 #c85402 #c04a02 #ba4202 #b43a02 #ae3102 #a92a02 #a32402 #9d1e02 #971702 #921201 #8d0e01 #870800 #820500 #7d0400 #7a0802 #770d02 #761002 #751204 #751404 #751504 #741604 #741805 #721a06 #721d06 #701f07 #6f2108 #6e2308 #6e2408 #6d2609 #6c280a #6c2a0a #6b2c0b #6a2c0c #6a2e0c #6b300e #6e3412 #713917 #743e1c #764220 #794625 #7d4a2b #804f32 #835538 #875a3f #8a6045 #8c654c #906a54 #936f5a #967460 #987a68 #9c8170 #9e8778 #a08d82 #a3938b #a69a93 #a7a09c #aaa7a4 #acabaa #aeadac #b2b1b0 #b5b4b3 #b8b7b6 #bcbbba #c0bfbe #c4c3c2 #c8c7c6 #cccbca #d0cfce #d4d3d2 #d8d7d6 #dad9d8 #dddcdb #e1e0df #e5e4e3 #e9e8e7 #ebeae9 ",
//ocean
"black #000519 #000a32 #00507d #0096c8 #56c5b8 #acf5a8 #d3fad3 #faffff ",
//bathy
"black #1f284f #263c6a #3563a0 #4897d3 mediumaquamarine #8dd2ef #f5ffff ",
//seafloor
"#6900b6 #6e00bc #7300c1 #7800c9 #6d0ecc #611ccf #542ad2 #4739d6 #3a48d9 #2c58dc #1e68df #0f79e2 #008ae6 #1996e9 #32a2ec #4caeef #67bbf2 #81c8f5 #9dd6f9 #bae3fc #d7f1ff #f1fcff ",
//abyss
"black #141e35 #263c6a #2e5085 #3563a0 #4897d3 #5ab9e9 #8dd2ef #f5ffff ",
//ibcso
"#1f284f #263c6a #2e5085 #3563a0 #3c77bc #4897d3 #5ab9e9 #5fc6f2 #72caee #8dd2ef ",
//gebco
"0.00000=rgb(0,240,255); 0.14286=rgb(35,255,255); 0.28571=rgb(90,255,255); 0.42857=rgb(140,255,230); 0.57143=rgb(165,255,215); 0.71429=rgb(195,255,215); 0.85714=rgb(210,255,215); 0.92857=rgb(230,255,240); 0.97143=rgb(235,255,255); 1.00000=rgb(235,255,255); ",
//topo
"0.00000=rgb(200,119,216); 0.03571=rgb(159,119,216); 0.03621=rgb(175,137,229); 0.07143=rgb(137,137,229); 0.07193=rgb(137,137,229); 0.10714=rgb(137,168,229); 0.10764=rgb(137,168,229); 0.14286=rgb(137,200,229); 0.14336=rgb(137,200,229); 0.17857=rgb(145,242,234); 0.17907=rgb(145,242,234); 0.21429=rgb(133,242,187); 0.21479=rgb(133,242,187); 0.25000=rgb(133,242,142); 0.25050=rgb(133,242,142); 0.28571=rgb(171,242,133); 0.28621=rgb(171,242,133); 0.32143=rgb(214,242,133); 0.32193=rgb(214,242,133); 0.35714=rgb(242,224,133); 0.35764=rgb(242,224,133); 0.39286=rgb(242,178,133); 0.39336=rgb(242,178,133); 0.46429=rgb(216,153,140); 0.46479=rgb(216,162,162); 0.50000=rgb(204,153,153); 0.50050=rgb(116,162,178); 0.51429=rgb(107,178,154); 0.51479=rgb(107,178,154); 0.52857=rgb(98,178,104); 0.52907=rgb(98,178,104); 0.54286=rgb(144,204,112); 0.54336=rgb(144,204,112); 0.57143=rgb(181,204,112); 0.57193=rgb(181,204,112); 0.60714=rgb(229,216,149); 0.60764=rgb(229,216,149); 0.75000=rgb(255,240,229); 0.75050=rgb(255,247,242); 1.00000=rgb(255,255,255); ",
//sealand
"0.00000=rgb(140,102,255); 0.04167=rgb(102,102,255); 0.04217=rgb(102,102,255); 0.08333=rgb(102,140,255); 0.08383=rgb(102,140,255); 0.12500=rgb(102,178,255); 0.12550=rgb(102,178,255); 0.16667=rgb(102,216,255); 0.16717=rgb(102,216,255); 0.20833=rgb(102,255,255); 0.20883=rgb(102,255,255); 0.25000=rgb(102,255,216); 0.25050=rgb(102,255,216); 0.29167=rgb(102,255,178); 0.29217=rgb(102,255,178); 0.33333=rgb(102,255,140); 0.33383=rgb(102,255,140); 0.37500=rgb(102,255,102); 0.37550=rgb(102,255,102); 0.41667=rgb(140,255,102); 0.41717=rgb(140,255,102); 0.45833=rgb(178,255,102); 0.45883=rgb(178,255,102); 0.50000=rgb(216,255,102); 0.50050=rgb(255,255,165); 0.58333=rgb(255,225,165); 0.58383=rgb(255,225,165); 0.66667=rgb(255,195,165); 0.66717=rgb(255,195,165); 0.75000=rgb(255,165,165); 0.75050=rgb(255,165,165); 0.83333=rgb(255,178,197); 0.83383=rgb(255,178,197); 0.91667=rgb(255,191,223); 0.91717=rgb(255,191,223); 1.00000=rgb(255,204,242); ",
//nighttime
"0.00000=rgb(8,0,25); 0.50000=rgb(63,120,140); 0.50100=rgb(133,140,63); 1.00000=rgb(255,229,229); ",
//world
"0.00000=rgb(0,240,255); 0.07143=rgb(0,240,255); 0.07193=rgb(35,255,255); 0.14286=rgb(35,255,255); 0.14336=rgb(90,255,255); 0.21429=rgb(90,255,255); 0.21479=rgb(140,255,230); 0.28571=rgb(140,255,230); 0.28621=rgb(165,255,215); 0.35714=rgb(165,255,215); 0.35764=rgb(195,255,215); 0.42857=rgb(195,255,215); 0.42907=rgb(210,255,215); 0.46429=rgb(210,255,215); 0.46479=rgb(230,255,240); 0.48571=rgb(230,255,240); 0.48621=rgb(235,255,255); 0.50000=rgb(235,255,255); 0.50050=rgb(51,102,0); 0.56250=rgb(129,195,31); 0.56300=rgb(129,195,31); 0.62500=rgb(255,255,204); 0.62550=rgb(255,255,204); 0.75000=rgb(244,189,69); 0.75050=rgb(244,189,69); 0.81250=rgb(102,51,12); 0.81300=rgb(102,51,12); 0.87500=rgb(102,51,0); 0.87550=rgb(102,51,0); 1.00000=rgb(255,254,253); ",
//geo
"0.00000=black; 0.06250=rgb(20,30,53); 0.06300=rgb(20,30,53); 0.12500=rgb(38,60,106); 0.12550=rgb(38,60,106); 0.18750=rgb(46,80,133); 0.18800=rgb(46,80,133); 0.25000=rgb(53,99,160); 0.25050=rgb(53,99,160); 0.31250=rgb(72,151,211); 0.31300=rgb(72,151,211); 0.37500=rgb(90,185,233); 0.37550=rgb(90,185,233); 0.43750=rgb(141,210,239); 0.43800=rgb(141,210,239); 0.50000=rgb(245,255,255); 0.50050=rgb(0,97,71); 0.50510=rgb(16,122,47); 0.50560=rgb(16,122,47); 0.55102=rgb(232,215,125); 0.55152=rgb(232,215,125); 0.62245=rgb(161,67,0); 0.62295=rgb(161,67,0); 0.67347=rgb(100,50,25); 0.67397=rgb(100,50,25); 0.78571=rgb(110,109,108); 0.78621=rgb(110,109,108); 0.90816=rgb(255,254,253); 0.90866=rgb(255,254,253); 1.00000=rgb(255,254,253); ",
//terra
"0.00000=rgb(105,0,182); 0.02083=rgb(110,0,188); 0.02133=rgb(110,0,188); 0.04167=rgb(115,0,193); 0.04217=rgb(115,0,193); 0.06250=rgb(120,0,201); 0.06300=rgb(120,0,201); 0.08333=rgb(109,14,204); 0.08383=rgb(109,14,204); 0.10417=rgb(97,28,207); 0.10467=rgb(97,28,207); 0.14583=rgb(84,42,210); 0.14633=rgb(84,42,210); 0.16667=rgb(71,57,214); 0.16717=rgb(71,57,214); 0.18750=rgb(58,72,217); 0.18800=rgb(58,72,217); 0.20833=rgb(44,88,220); 0.20883=rgb(44,88,220); 0.22917=rgb(30,104,223); 0.22967=rgb(30,104,223); 0.27083=rgb(15,121,226); 0.27133=rgb(15,121,226); 0.29167=rgb(0,138,230); 0.29217=rgb(0,138,230); 0.31250=rgb(25,150,233); 0.31300=rgb(25,150,233); 0.33333=rgb(50,162,236); 0.33383=rgb(50,162,236); 0.35417=rgb(76,174,239); 0.35467=rgb(76,174,239); 0.37500=rgb(103,187,242); 0.37550=rgb(103,187,242); 0.41667=rgb(129,200,245); 0.41717=rgb(129,200,245); 0.43750=rgb(157,214,249); 0.43800=rgb(157,214,249); 0.45833=rgb(186,227,252); 0.45883=rgb(186,227,252); 0.47917=rgb(215,241,255); 0.47967=rgb(215,241,255); 0.50000=rgb(241,252,255); 0.50050=rgb(105,152,133); 0.50357=rgb(118,169,146); 0.50407=rgb(118,169,146); 0.51429=rgb(131,181,155); 0.51479=rgb(131,181,155); 0.54286=rgb(165,192,167); 0.54336=rgb(165,192,167); 0.57143=rgb(211,201,179); 0.57193=rgb(211,201,179); 0.64286=rgb(212,184,164); 0.64336=rgb(212,184,164); 0.71429=rgb(212,192,181); 0.71479=rgb(212,192,181); 0.78571=rgb(214,209,206); 0.78621=rgb(214,209,206); 0.85714=rgb(222,221,220); 0.85764=rgb(222,221,220); 0.92857=rgb(238,237,236); 0.92907=rgb(238,237,236); 1.00000=rgb(247,246,245); ",
//relief
"0.00000=black; 0.06250=rgb(0,5,25); 0.06300=rgb(0,5,25); 0.12500=rgb(0,10,50); 0.12550=rgb(0,10,50); 0.18750=rgb(0,80,125); 0.18800=rgb(0,80,125); 0.25000=rgb(0,150,200); 0.25050=rgb(0,150,200); 0.31250=rgb(86,197,184); 0.31300=rgb(86,197,184); 0.37500=rgb(172,245,168); 0.37550=rgb(172,245,168); 0.43750=rgb(211,250,211); 0.43800=rgb(211,250,211); 0.50000=rgb(250,255,255); 0.50050=rgb(70,120,50); 0.53125=rgb(120,100,50); 0.53175=rgb(120,100,50); 0.56250=rgb(146,126,60); 0.56300=rgb(146,126,60); 0.62500=rgb(198,178,80); 0.62550=rgb(198,178,80); 0.68750=rgb(250,230,100); 0.68800=rgb(250,230,100); 0.75000=rgb(250,234,126); 0.75050=rgb(250,234,126); 0.81250=rgb(252,238,152); 0.81300=rgb(252,238,152); 0.87500=rgb(252,243,177); 0.87550=rgb(252,243,177); 0.93750=rgb(253,249,216); 0.93800=rgb(253,249,216); 1.00000=white; ",
//globe
"0.00000=rgb(153,0,255); 0.02500=rgb(153,0,255); 0.02550=rgb(153,0,255); 0.05000=rgb(153,0,255); 0.05050=rgb(153,0,255); 0.07500=rgb(153,0,255); 0.07550=rgb(136,17,255); 0.10000=rgb(136,17,255); 0.10050=rgb(119,34,255); 0.12500=rgb(119,34,255); 0.12550=rgb(102,51,255); 0.15000=rgb(102,51,255); 0.15050=rgb(85,68,255); 0.17500=rgb(85,68,255); 0.17550=rgb(68,85,255); 0.20000=rgb(68,85,255); 0.20050=rgb(51,102,255); 0.22500=rgb(51,102,255); 0.22550=rgb(34,119,255); 0.25000=rgb(34,119,255); 0.25050=rgb(17,136,255); 0.27500=rgb(17,136,255); 0.27550=rgb(0,153,255); 0.30000=rgb(0,153,255); 0.30050=rgb(27,164,255); 0.32500=rgb(27,164,255); 0.32550=rgb(54,175,255); 0.35000=rgb(54,175,255); 0.35050=rgb(81,186,255); 0.37500=rgb(81,186,255); 0.37550=rgb(108,197,255); 0.40000=rgb(108,197,255); 0.40050=rgb(134,208,255); 0.42500=rgb(134,208,255); 0.42550=rgb(161,219,255); 0.45000=rgb(161,219,255); 0.45050=rgb(188,230,255); 0.47500=rgb(188,230,255); 0.47550=rgb(215,241,255); 0.49000=rgb(215,241,255); 0.49050=rgb(241,252,255); 0.50000=rgb(241,252,255); 0.50050=rgb(51,102,0); 0.50500=rgb(51,204,102); 0.50550=rgb(51,204,102); 0.51000=rgb(187,228,146); 0.51050=rgb(187,228,146); 0.52500=rgb(255,220,185); 0.52550=rgb(255,220,185); 0.55000=rgb(243,202,137); 0.55050=rgb(243,202,137); 0.57500=rgb(230,184,88); 0.57550=rgb(230,184,88); 0.60000=rgb(217,166,39); 0.60050=rgb(217,166,39); 0.62500=rgb(168,154,31); 0.62550=rgb(168,154,31); 0.65000=rgb(164,144,25); 0.65050=rgb(164,144,25); 0.67500=rgb(162,134,19); 0.67550=rgb(162,134,19); 0.70000=rgb(159,123,13); 0.70050=rgb(159,123,13); 0.72500=rgb(156,113,7); 0.72550=rgb(156,113,7); 0.75000=rgb(153,102,0); 0.75050=rgb(153,102,0); 0.77500=rgb(162,89,89); 0.77550=rgb(162,89,89); 0.80000=rgb(178,118,118); 0.80050=rgb(178,118,118); 0.82500=rgb(183,147,147); 0.82550=rgb(183,147,147); 0.85000=rgb(194,176,176); 0.85050=rgb(194,176,176); 0.87500=gray80; 0.90000=gray90; 0.92500=gray95; 0.95000=white; 0.97500=white; 1.00000=white; ",
//earth
"0.00000=black; 0.06250=rgb(31,40,79); 0.06300=rgb(31,40,79); 0.12500=rgb(38,60,106); 0.12550=rgb(38,60,106); 0.25000=rgb(53,99,160); 0.25050=rgb(53,99,160); 0.28125=rgb(72,151,211); 0.28175=rgb(72,151,211); 0.34375=mediumaquamarine; 0.43750=rgb(141,210,239); 0.43800=rgb(141,210,239); 0.50000=rgb(245,255,255); 0.50050=rgb(174,239,213); 0.50333=rgb(175,240,211); 0.50383=rgb(175,240,211); 0.50667=rgb(176,242,208); 0.50717=rgb(176,242,208); 0.51000=rgb(176,242,202); 0.51050=rgb(176,242,202); 0.51333=rgb(177,242,196); 0.51383=rgb(177,242,196); 0.51667=rgb(176,243,190); 0.51717=rgb(176,243,190); 0.52000=rgb(176,244,186); 0.52050=rgb(176,244,186); 0.52333=rgb(178,246,181); 0.52383=rgb(178,246,181); 0.52667=rgb(181,246,178); 0.52717=rgb(181,246,178); 0.53000=rgb(186,247,178); 0.53050=rgb(186,247,178); 0.53333=rgb(192,247,178); 0.53383=rgb(192,247,178); 0.53667=rgb(198,248,178); 0.53717=rgb(198,248,178); 0.54000=rgb(204,249,178); 0.54050=rgb(204,249,178); 0.54333=rgb(210,250,177); 0.54383=rgb(210,250,177); 0.54667=rgb(217,250,178); 0.54717=rgb(217,250,178); 0.55000=rgb(224,251,178); 0.55050=rgb(224,251,178); 0.55333=rgb(231,252,178); 0.55383=rgb(231,252,178); 0.55667=rgb(238,252,179); 0.55717=rgb(238,252,179); 0.56000=rgb(245,252,179); 0.56050=rgb(245,252,179); 0.56333=rgb(250,252,178); 0.56383=rgb(250,252,178); 0.56667=rgb(248,249,172); 0.56717=rgb(248,249,172); 0.57000=rgb(238,244,162); 0.57050=rgb(238,244,162); 0.57333=rgb(226,240,151); 0.57383=rgb(226,240,151); 0.57667=rgb(213,235,140); 0.57717=rgb(213,235,140); 0.58000=rgb(198,228,128); 0.58050=rgb(198,228,128); 0.58333=rgb(184,222,118); 0.58383=rgb(184,222,118); 0.58667=rgb(170,216,108); 0.58717=rgb(170,216,108); 0.59000=rgb(154,211,98); 0.59050=rgb(154,211,98); 0.59333=rgb(140,205,89); 0.59383=rgb(140,205,89); 0.59667=rgb(125,199,82); 0.59717=rgb(125,199,82); 0.60000=rgb(110,194,74); 0.60050=rgb(110,194,74); 0.60333=rgb(94,188,66); 0.60383=rgb(94,188,66); 0.60667=rgb(77,182,57); 0.60717=rgb(77,182,57); 0.61000=rgb(62,176,50); 0.61050=rgb(62,176,50); 0.61333=rgb(49,171,44); 0.61383=rgb(49,171,44); 0.61667=rgb(39,165,42); 0.61717=rgb(39,165,42); 0.62000=rgb(30,160,43); 0.62050=rgb(30,160,43); 0.62333=rgb(24,154,46); 0.62383=rgb(24,154,46); 0.62667=rgb(18,148,49); 0.62717=rgb(18,148,49); 0.63000=rgb(14,142,52); 0.63050=rgb(14,142,52); 0.63333=rgb(9,137,56); 0.63383=rgb(9,137,56); 0.63667=rgb(7,132,60); 0.63717=rgb(7,132,60); 0.64000=rgb(12,130,63); 0.64050=rgb(12,130,63); 0.64333=rgb(24,130,63); 0.64383=rgb(24,130,63); 0.64667=rgb(40,132,61); 0.64717=rgb(40,132,61); 0.65000=rgb(52,136,60); 0.65050=rgb(52,136,60); 0.65333=rgb(64,140,59); 0.65383=rgb(64,140,59); 0.65667=rgb(76,142,59); 0.65717=rgb(76,142,59); 0.66000=rgb(87,146,56); 0.66050=rgb(87,146,56); 0.66333=rgb(99,148,54); 0.66383=rgb(99,148,54); 0.66667=rgb(110,150,52); 0.66717=rgb(110,150,52); 0.67000=rgb(120,154,50); 0.67050=rgb(120,154,50); 0.67333=rgb(128,156,48); 0.67383=rgb(128,156,48); 0.67667=rgb(137,160,46); 0.67717=rgb(137,160,46); 0.68000=rgb(147,162,43); 0.68050=rgb(147,162,43); 0.68333=rgb(156,164,41); 0.68383=rgb(156,164,41); 0.68667=rgb(166,166,39); 0.68717=rgb(166,166,39); 0.69000=rgb(176,170,36); 0.69050=rgb(176,170,36); 0.69333=rgb(187,173,34); 0.69383=rgb(187,173,34); 0.69667=rgb(197,176,30); 0.69717=rgb(197,176,30); 0.70000=rgb(207,177,28); 0.70050=rgb(207,177,28); 0.70333=rgb(218,179,24); 0.70383=rgb(218,179,24); 0.70667=rgb(228,180,20); 0.70717=rgb(228,180,20); 0.71000=rgb(238,182,14); 0.71050=rgb(238,182,14); 0.71333=rgb(246,182,8); 0.71383=rgb(246,182,8); 0.71667=rgb(248,176,4); 0.71717=rgb(248,176,4); 0.72000=rgb(244,166,2); 0.72050=rgb(244,166,2); 0.72333=rgb(238,155,2); 0.72383=rgb(238,155,2); 0.72667=rgb(232,144,2); 0.72717=rgb(232,144,2); 0.73000=rgb(226,132,2); 0.73050=rgb(226,132,2); 0.73333=rgb(220,122,2); 0.73383=rgb(220,122,2); 0.73667=rgb(216,111,2); 0.73717=rgb(216,111,2); 0.74000=rgb(211,102,2); 0.74050=rgb(211,102,2); 0.74333=rgb(206,92,2); 0.74383=rgb(206,92,2); 0.74667=rgb(200,84,2); 0.74717=rgb(200,84,2); 0.75000=rgb(192,74,2); 0.75050=rgb(192,74,2); 0.75333=rgb(186,66,2); 0.75383=rgb(186,66,2); 0.75667=rgb(180,58,2); 0.75717=rgb(180,58,2); 0.76000=rgb(174,49,2); 0.76050=rgb(174,49,2); 0.76333=rgb(169,42,2); 0.76383=rgb(169,42,2); 0.76667=rgb(163,36,2); 0.76717=rgb(163,36,2); 0.77000=rgb(157,30,2); 0.77050=rgb(157,30,2); 0.77333=rgb(151,23,2); 0.77383=rgb(151,23,2); 0.77667=rgb(146,18,1); 0.77717=rgb(146,18,1); 0.78000=rgb(141,14,1); 0.78050=rgb(141,14,1); 0.78333=rgb(135,8,0); 0.78383=rgb(135,8,0); 0.78667=rgb(130,5,0); 0.78717=rgb(130,5,0); 0.79000=rgb(125,4,0); 0.79050=rgb(125,4,0); 0.79333=rgb(122,8,2); 0.79383=rgb(122,8,2); 0.79667=rgb(119,13,2); 0.79717=rgb(119,13,2); 0.80000=rgb(118,16,2); 0.80050=rgb(118,16,2); 0.80333=rgb(117,18,4); 0.80383=rgb(117,18,4); 0.80667=rgb(117,20,4); 0.80717=rgb(117,20,4); 0.81000=rgb(117,21,4); 0.81050=rgb(117,21,4); 0.81333=rgb(116,22,4); 0.81383=rgb(116,22,4); 0.81667=rgb(116,24,5); 0.81717=rgb(116,24,5); 0.82000=rgb(114,26,6); 0.82050=rgb(114,26,6); 0.82333=rgb(114,29,6); 0.82383=rgb(114,29,6); 0.82667=rgb(112,31,7); 0.82717=rgb(112,31,7); 0.83000=rgb(111,33,8); 0.83050=rgb(111,33,8); 0.83333=rgb(110,35,8); 0.83383=rgb(110,35,8); 0.83667=rgb(110,36,8); 0.83717=rgb(110,36,8); 0.84000=rgb(109,38,9); 0.84050=rgb(109,38,9); 0.84333=rgb(108,40,10); 0.84383=rgb(108,40,10); 0.84667=rgb(108,40,10); 0.84717=rgb(108,40,10); 0.85000=rgb(108,42,10); 0.85050=rgb(108,42,10); 0.85333=rgb(107,44,11); 0.85383=rgb(107,44,11); 0.85667=rgb(106,44,12); 0.85717=rgb(106,44,12); 0.86000=rgb(106,46,12); 0.86050=rgb(106,46,12); 0.86333=rgb(107,48,14); 0.86383=rgb(107,48,14); 0.86667=rgb(110,52,18); 0.86717=rgb(110,52,18); 0.87000=rgb(113,57,23); 0.87050=rgb(113,57,23); 0.87333=rgb(116,62,28); 0.87383=rgb(116,62,28); 0.87667=rgb(118,66,32); 0.87717=rgb(118,66,32); 0.88000=rgb(121,70,37); 0.88050=rgb(121,70,37); 0.88333=rgb(125,74,43); 0.88383=rgb(125,74,43); 0.88667=rgb(128,79,50); 0.88717=rgb(128,79,50); 0.89000=rgb(131,85,56); 0.89050=rgb(131,85,56); 0.89333=rgb(135,90,63); 0.89383=rgb(135,90,63); 0.89667=rgb(138,96,69); 0.89717=rgb(138,96,69); 0.90000=rgb(140,101,76); 0.90050=rgb(140,101,76); 0.90333=rgb(144,106,84); 0.90383=rgb(144,106,84); 0.90667=rgb(147,111,90); 0.90717=rgb(147,111,90); 0.91000=rgb(150,116,96); 0.91050=rgb(150,116,96); 0.91333=rgb(152,122,104); 0.91383=rgb(152,122,104); 0.91667=rgb(156,129,112); 0.91717=rgb(156,129,112); 0.92000=rgb(158,135,120); 0.92050=rgb(158,135,120); 0.92333=rgb(160,141,130); 0.92383=rgb(160,141,130); 0.92667=rgb(163,147,139); 0.92717=rgb(163,147,139); 0.93000=rgb(166,154,147); 0.93050=rgb(166,154,147); 0.93333=rgb(167,160,156); 0.93383=rgb(167,160,156); 0.93667=rgb(170,167,164); 0.93717=rgb(170,167,164); 0.94000=rgb(172,171,170); 0.94050=rgb(172,171,170); 0.94333=rgb(174,173,172); 0.94383=rgb(174,173,172); 0.94667=rgb(178,177,176); 0.94717=rgb(178,177,176); 0.95000=rgb(181,180,179); 0.95050=rgb(181,180,179); 0.95333=rgb(184,183,182); 0.95383=rgb(184,183,182); 0.95667=rgb(188,187,186); 0.95717=rgb(188,187,186); 0.96000=rgb(192,191,190); 0.96050=rgb(192,191,190); 0.96333=rgb(196,195,194); 0.96383=rgb(196,195,194); 0.96667=rgb(200,199,198); 0.96717=rgb(200,199,198); 0.97000=rgb(204,203,202); 0.97050=rgb(204,203,202); 0.97333=rgb(208,207,206); 0.97383=rgb(208,207,206); 0.97667=rgb(212,211,210); 0.97717=rgb(212,211,210); 0.98000=rgb(216,215,214); 0.98050=rgb(216,215,214); 0.98333=rgb(218,217,216); 0.98383=rgb(218,217,216); 0.98667=rgb(221,220,219); 0.98717=rgb(221,220,219); 0.99000=rgb(225,224,223); 0.99050=rgb(225,224,223); 0.99333=rgb(229,228,227); 0.99383=rgb(229,228,227); 0.99667=rgb(233,232,231); 0.99717=rgb(233,232,231); 1.00000=rgb(235,234,233); ",
//etopo1
"0.00000=rgb(10,0,121); 0.02273=rgb(26,0,137); 0.02323=rgb(26,0,137); 0.04545=rgb(38,0,152); 0.04596=rgb(38,0,152); 0.06818=rgb(27,3,166); 0.06868=rgb(27,3,166); 0.09091=rgb(16,6,180); 0.09141=rgb(16,6,180); 0.11364=rgb(5,9,193); 0.11414=rgb(5,9,193); 0.13636=rgb(0,14,203); 0.13686=rgb(0,14,203); 0.15909=rgb(0,22,210); 0.15959=rgb(0,22,210); 0.18182=rgb(0,30,216); 0.18232=rgb(0,30,216); 0.20455=rgb(0,39,223); 0.20505=rgb(0,39,223); 0.22727=rgb(12,68,231); 0.22777=rgb(12,68,231); 0.25000=rgb(26,102,240); 0.25050=rgb(26,102,240); 0.27273=rgb(19,117,244); 0.27323=rgb(19,117,244); 0.29545=rgb(14,133,249); 0.29596=rgb(14,133,249); 0.31818=rgb(21,158,252); 0.31868=rgb(21,158,252); 0.34091=rgb(30,178,255); 0.34141=rgb(30,178,255); 0.36364=rgb(43,186,255); 0.36414=rgb(43,186,255); 0.38636=rgb(55,193,255); 0.38686=rgb(55,193,255); 0.40909=rgb(65,200,255); 0.40959=rgb(65,200,255); 0.43182=rgb(79,210,255); 0.43232=rgb(79,210,255); 0.45455=rgb(94,223,255); 0.45505=rgb(94,223,255); 0.47727=rgb(138,227,255); 0.47777=rgb(138,227,255); 0.50000=rgb(188,230,255); 0.50050=rgb(51,102,0); 0.50588=rgb(51,204,102); 0.50638=rgb(51,204,102); 0.51176=rgb(187,228,146); 0.51226=rgb(187,228,146); 0.52941=rgb(255,220,185); 0.52991=rgb(255,220,185); 0.55882=rgb(243,202,137); 0.55932=rgb(243,202,137); 0.58824=rgb(230,184,88); 0.58873=rgb(230,184,88); 0.61765=rgb(217,166,39); 0.61815=rgb(217,166,39); 0.64706=rgb(168,154,31); 0.64756=rgb(168,154,31); 0.67647=rgb(164,144,25); 0.67697=rgb(164,144,25); 0.70588=rgb(162,134,19); 0.70638=rgb(162,134,19); 0.73529=rgb(159,123,13); 0.73579=rgb(159,123,13); 0.76471=rgb(156,113,7); 0.76521=rgb(156,113,7); 0.79412=rgb(153,102,0); 0.79462=rgb(153,102,0); 0.82353=rgb(162,89,89); 0.82403=rgb(162,89,89); 0.85294=rgb(178,118,118); 0.85344=rgb(178,118,118); 0.88235=rgb(183,147,147); 0.88285=rgb(183,147,147); 0.91176=rgb(194,176,176); 0.91226=rgb(194,176,176); 0.94118=rgb(205,204,203); 0.94168=rgb(205,204,203); 0.97059=rgb(230,229,228); 0.97109=rgb(230,229,228); 1.00000=rgb(255,254,253); ",
//cubhelix
"black #010001 #030103 #040104 #060206 #080208 #090309 #0a040b #0c040d #0d050f #0e0611 #0f0613 #110715 #120817 #130919 #140a1b #140b1d #150b1f #160c21 #170d23 #170e25 #180f27 #181129 #19122b #19132d #19142f #1a1530 #1a1632 #1a1834 #1a1936 #1a1a38 #1a1c39 #1a1d3b #1a1f3c #1a203e #1a223f #1a2341 #192542 #192643 #192845 #192946 #182b47 #182d48 #182e49 #17304a #17324a #17344b #17354c #16374c #16394d #163a4d #153c4d #153e4e #15404e #15424e #15434e #15454e #14474e #14494e #144a4d #154c4d #154e4d #154f4c #15514c #15534b #16544b #16564a #165849 #175949 #175b48 #185c47 #195e46 #1a5f45 #1b6144 #1b6243 #1c6342 #1e6542 #1f6641 #206740 #21683f #236a3d #246b3c #266c3b #276d3a #296e3a #2b6f39 #2d7038 #2f7137 #317236 #337235 #357334 #377433 #397433 #3c7532 #3e7631 #417631 #437730 #467730 #48782f #4b782f #4e782f #51792e #53792e #56792e #59792e #5c7a2e #5f7a2f #627a2f #657a2f #687a30 #6b7a30 #6e7a31 #717a32 #747a32 #787a33 #7b7a34 #7e7a35 #817a37 #847a38 #877a39 #8a793b #8d793c #90793e #937940 #967941 #997943 #9b7945 #9e7947 #a1794a #a4784c #a6784e #a97851 #ab7853 #ae7856 #b07858 #b2785b #b5785e #b77860 #b97863 #bb7966 #bd7969 #bf796c #c1796f #c27972 #c47a75 #c67a78 #c77a7c #c97b7f #ca7b82 #cb7c85 #cc7c88 #cd7d8c #ce7d8f #cf7e92 #d07f95 #d17f99 #d1809c #d2819f #d382a2 #d383a5 #d383a9 #d484ac #d485af #d487b2 #d488b5 #d489b8 #d48aba #d48bbd #d48cc0 #d38ec3 #d38fc5 #d390c8 #d292cb #d293cd #d295cf #d196d2 #d098d4 #d09ad6 #cf9bd8 #cf9dda #ce9edc #cda0de #cda2e0 #cca4e2 #cba5e3 #cba7e5 #caa9e6 #c9abe7 #c9ace9 #c8aeea #c7b0eb #c7b2ec #c6b4ed #c5b6ee #c5b7ef #c4b9ef #c4bbf0 #c3bdf1 #c3bff1 #c2c1f2 #c2c2f2 #c2c4f2 #c1c6f3 #c1c8f3 #c1caf3 #c1cbf3 #c1cdf3 #c1cff3 #c1d0f3 #c1d2f3 #c1d4f3 #c1d5f3 #c2d7f2 #c2d8f2 #c3daf2 #c3dbf2 #c4ddf1 #c4def1 #c5e0f1 #c6e1f1 #c7e2f0 #c8e4f0 #c8e5f0 #cae6ef #cbe7ef #cce8ef #cde9ef #ceebef #d0ecee #d1edee #d2eeee #d4efee #d5f0ee #d7f0ee #d9f1ee #daf2ee #dcf3ef #def4ef #dff4ef #e1f5f0 #e3f6f0 #e5f7f0 #e7f7f1 #e8f8f2 #eaf8f2 #ecf9f3 #eefaf4 #f0faf5 #f2fbf6 #f4fbf7 #f5fcf8 #f7fcf9 #f9fdfa #fbfdfc #fdfefd white ",

//hot
"0.00000=black; 0.37500=red; 0.75000=yellow; 1.00000=white; ",
//cool
"cyan magenta ",
//copper
"0.00000=black; 0.79688=rgb(255,159,102); 0.79788=rgb(255,159,102); 1.00000=rgb(255,200,128); ",
//gray
"black white ",
//split
"#8080ff #000080 #000000 #800000 #ff8080 ",
//polar
"blue white red ",
//red2green
"red white green ",

//paired
"#a6cee3 #1f78b4 #b2df8a #33a02c #fb9a99 #e31a1c #fdbf6f darkorange1 #cab2d6 #6a3d9a #ffff99 #b15928 ",
//categorical
"green blue red cyan #ff99ee #ffdd66 #006600 #000066 #aa5544 #0088cc #ff11ff #ffeeee #ff0077 #88ff99 #8866ff #558877 #660044 #ddff00 #ff8800 #776600 #bb0099 #997799 #88aa00 #223366 #223300 #aabb77 #00bb22 #ff5555 #ffaa77 #441100 #ff99aa #00bb88 #9999ff #88ddff #0011bb #990000 #774499 #bbffdd #bb9988 #aa1144 #554444 #9911ee #ddff77 #aa5500 #005566 #ddccff #99ff44 #0044bb #bb5588 #dd66ee #ffdd00 #770099 #ffddaa #00ff77 #cc9900 #55aa44 #33bbbb #220022 #99aabb #00ffcc #eeffaa #ff00cc #dd4411 #775533 #ff55aa #bb9955 #0055aa #005533 #ff6688 #667744 #884455 #0099bb #bb00cc #440055 #88bbff #7777bb #88dd55 #0088ff #aa0066 #660022 #bb77dd #ee7744 #00cc66 #bbccbb #eebbcc #338855 #88dd99 #557799 #668822 #553355 #773300 119 #5544ff #77bb99 #002211 #445500 #aadd22 #dd99cc #881177 #cccc22 #000044 #aadd77 #cc8833 #ff8877 #ffbbaa #aaaa44 #ff0055 #0044dd #002233 #332200 #bb55ff #88eeee #ff0033 #004400 #331199 #ee66cc #aa66aa #bb7777 #7744bb #441122 #771111 #ffaa00 #445544 #6666cc #ff11aa #33dd11 #ffbb55 #ffffdd #7722cc #55aa00 #333388 #554400 #ddaaff #00ffaa #008800 #996622 #cc0033 #ffff55 #cceeff #bb5566 #00bbff #005588 #cc8866 #ff88bb #5577ff #66eecc #4400dd #ddcc77 #ff55ff #aa44bb #ff9944 #dd3377 #ff6600 #889977 #bb4499 #998800 #001133 #775588 #bb4433 #00ddff #9999cc #bb77ff #66ee55 #66cc00 #552288 #009988 #bbddaa #6699ee #77ff00 #555577 #880044 #eebb00 #332233 #441144 #887744 #77aa66 #ccbbbb #88bbbb #ff55dd #ccaa44 #bbffaa #007777 #dd8800 #773322 #eebb77 #cc0011 #775566 #bb99aa #ccbb99 #ffff88 #008833 #dd6611 #88bb44 #bbaa11 #cc7744 #773366 #bbccee #993300 #11dd99 #113333 #cc4455 #220000 #ee3366 #990022 #cc0088 #bbcc55 #334455 #444422 #cc33ff #ddffee #bbff66 #663333 #335522 #6699cc #00ff55 #447733 #997777 #dd7777 #888833 #dd77bb #aa88cc #44aa66 #ee0088 #554488 #77ffbb #55cc77 #aa6688 #ff88ff #114433 #7700ff yellow #1177cc #aa00aa #ff6644 #bbff22 #5555dd #bbff88 #aa88ff #00aa33 #996655 #776655 #ddff55 #553311 #6699aa #003355 #337788 #002266 #00ccbb #55ee77 #22aaff #ffaacc #dd22cc #002288 #ffdd44 #003311 #446666 ",

//haxby
"#0a0079 #280096 #1405af #000ac8 #0019d4 #0028e0 #1a66f0 #0d81f8 #19afff #32beff #44caff #61e1f0 #6aebe1 #7cebc8 #8aecae #acf5a8 #cdffa2 #dff58d #f0ec79 #f7d768 #ffbd57 #ffa045 #f4754b #ee504e #ff5a5a #ff7c7c #ff9e9e #f5b3ae #ffc4c4 #ffd7d7 #ffebeb #fffefd ",
//jet
"0.00000=rgb(0,0,127); 0.12500=blue; 0.37500=cyan; 0.62500=yellow; 0.87500=red; 1.00000=rgb(127,0,0); ",
//panoply
"#040ed8 #2050ff #4196ff #6dc1ff #86d9ff #9ceeff #aff5ff #ceffff #fffe47 #ffeb00 #ffc400 #ff9000 #ff4800 red #d50000 #9e0000 ",
//no_green
"#2060ff #209fff #20bfff #00cfff #2affff #55ffff #7fffff #aaffff #ffff54 #fff000 #ffbf00 #ffa800 #ff8a00 #ff7000 #ff4d00 red ",
//wysiwyg
"#400040 #4000c0 #0040ff #0080ff #00a0ff #40c0ff #40e0ff #40ffff #40ffc0 #40ff40 #80ff40 #c0ff40 #ffff40 #ffe040 #ffa040 #ff6040 #ff2040 #ff60c0 #ffa0ff #ffe0ff ",
//seis
"#aa0000 #ff0000 #ff5500 #ffaa00 #ffff00 #5aff1e #00f06e #0050ff #0000cd ",
//rainbow
"magenta blue cyan green yellow red ",
//nih
"black rgb(85,0,170) rgb(5,0,90) rgb(0,0,159) rgb(0,0,239) rgb(0,63,255) rgb(0,143,196) rgb(0,223,170) rgb(0,255,74) rgb(42,255,42) rgb(159,255,47) rgb(255,223,0) rgb(255,143,0) rgb(255,71,0) rgb(255,22,0) rgb(237,0,0) rgb(203,0,0) black)"
};


