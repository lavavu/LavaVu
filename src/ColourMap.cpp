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

ColourMap::ColourMap(DrawState& drawstate, const char* name,
                     float min, float max, std::string props)
  : properties(drawstate.globals, drawstate.defaults),
    minimum(min), maximum(max),
    calibrated(false), noValues(false)
{
  precalc = new Colour[samples];
  this->name = std::string(name);
  texture = NULL;
  background.value = 0xff000000;

  properties.parseSet(props);

  if (properties.has("colours"))
  {
    loadPalette(properties["colours"]);
    //Erase the colour list? Keep it for now for export
    //properties.data.erase("colours");
  }
}

void ColourMap::parse(std::string colourMapString)
{
  noValues = false;
  const char *breakChars = " \t\n;";
  char *charPtr;
  char colourStr[64];
  char *colourString = new char[colourMapString.size()+1];
  strcpy(colourString, colourMapString.c_str());
  char *colourMap_ptr = colourString;
  colours.clear();
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

  //Ensure at least two colours
  while (colours.size() < 2) add(0xff000000);
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

void ColourMap::add(float *components, float pvalue)
{
  Colour colour;
  for (int c=0; c<4; c++)  //Convert components from floats to bytes
    colour.rgba[c] = 255 * components[c];

  add(colour.value, pvalue);
}

void ColourMap::calc()
{
  if (!colours.size()) return;
  //Precalculate colours
  if (properties["logscale"])
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

  minimum = min;
  maximum = max;
  if (properties["logscale"])
    range = LOG10(maximum) - LOG10(minimum);
  else
    range = maximum - minimum;

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

  //Calc values now colours have been added
  calc();

  debug_print("ColourMap calibrated min %f, max %f, range %f ==> %d colours\n", minimum, maximum, range, colours.size());
  //for (int i=0; i < colours.size(); i++)
  //   printf(" colour %d value %f pos %f\n", colours[i].colour, colours[i].value, colours[i].position);
  calibrated = true;
}

//Calibration from a geom data set, also copies scaling data fields
void ColourMap::calibrate(FloatValues* dataValues)
{
  float range[2];
  Properties::toFloatArray(properties["range"], range, 2);
  if (range[0] == range[1])
    calibrate(dataValues->minimum, dataValues->maximum);
  else
    calibrate(range[0], range[1]);
}

void ColourMap::calibrate()
{
  calibrate(minimum, maximum);
}

Colour ColourMap::getfast(float value)
{
  //NOTE: value caching DOES NOT WORK for log scales!
  //If this is causing slow downs in future, need a better method
  int c = 0;
  if (properties["logscale"])
    c = (int)((samples-1) * ((LOG10(value) - LOG10(minimum)) / range));
  else
    c = (int)((samples-1) * ((value - minimum) / range));
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
  if (max == min) return 0.5;   // Force central value if no range
  if (value <= min) return 0.0;
  if (value >= max) return 1.0;

  if (properties["logscale"])
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
  //printf(" scaled %f ", scaledValue);
  if (colours.size() == 0) return Colour();
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

    if (i==0 || i==colours.size()) abort_program("Colour position %f not in range [%f,%f]", scaledValue, colours[0].position, colours.back().position);

    // Calculate interpolation factor [0,1] between colour at index and previous colour
    float interpolate = (scaledValue - colours[i-1].position) / (colours[i].position - colours[i-1].position);

    //printf(" interpolate %f above %f below %f\n", interpolate, colours[i].position, colours[i-1].position);
    if (properties["discrete"])
    {
      //No interpolation
      if (interpolate < 0.5)
        return colours[i-1].colour;
      else
        return colours[i].colour;
    }
    else
    {
      //Linear interpolation between colours
      Colour colour0, colour1;
      colour0 = colours[i-1].colour;
      colour1 = colours[i].colour;

      for (int c=0; c<4; c++)
        colour0.rgba[c] += (colour1.rgba[c] - colour0.rgba[c]) * interpolate;

      return colour0;
    }
  }

  Colour c;
  c.value = 0;
  return c;
}

#define RECT2(x0, y0, x1, y1, swap) swap ? glRecti(y0, x0, y1, x1) : glRecti(x0, y0, x1, y1);
#define VERT2(x, y, swap) swap ? glVertex2i(y, x) : glVertex2i(x, y);

void ColourMap::draw(Properties& colourbarprops, int startx, int starty, int length, int height, Colour& printColour, bool vertical)
{
  glPushAttrib(GL_ENABLE_BIT);
  int pixel_I;
  Colour colour;
  glDisable(GL_MULTISAMPLE);

  if (!calibrated) calibrate(minimum, maximum);

  //Draw background (checked)
  colour.value = 0xff444444;
  for ( pixel_I = 0 ; pixel_I < length ; pixel_I += height*0.5 )
  {
    int end = startx + pixel_I + height;
    if (end > startx + length) end = startx + length;

    glColor4ubv(colour.rgba);
    RECT2( startx + pixel_I, starty, end, starty + height*0.5, vertical);
    colour.invert();
    glColor4ubv(colour.rgba);
    RECT2( startx + pixel_I, starty + height*0.5, end, starty + height, vertical);
  }

  // Draw Colour Bar
  glDisable(GL_CULL_FACE);
  int count = colours.size();
  int idx, xpos;
  if (properties["discrete"])
  {
    glShadeModel(GL_FLAT);
    glBegin(GL_QUAD_STRIP);
    xpos = startx;
    VERT2(xpos, starty, vertical);
    VERT2(xpos, starty + height, vertical);
    for (idx = 1; idx < count+1; idx++)
    {
      int oldx = xpos;
      colour = getFromScaled(colours[idx-1].position);
      glColor4ubv(colour.rgba);
      if (idx == count)
      {
        VERT2(xpos, starty, vertical);
        VERT2(xpos, starty + height, vertical);
      }
      else
      {
        xpos = startx + length * colours[idx].position;
        VERT2(oldx + 0.5 * (xpos - oldx), starty, vertical);
        VERT2(oldx + 0.5 * (xpos - oldx), starty + height, vertical);
      }
    }
    glEnd();
    glShadeModel(GL_SMOOTH);
  }
  else
  {
    glShadeModel(GL_SMOOTH);
    glBegin(GL_QUAD_STRIP);
    for (idx = 0; idx < count; idx++)
    {
      colour = getFromScaled(colours[idx].position);
      glColor4ubv(colour.rgba);
      xpos = startx + length * colours[idx].position;
      VERT2(xpos, starty, vertical);
      VERT2(xpos, starty + height, vertical);
    }
    glEnd();
  }

  //Labels / tick marks
  glColor4ubv(printColour.rgba);
  float tickValue;
  int ticks = colourbarprops["ticks"];
  json tickValues = colourbarprops["tickvalues"];
  if (tickValues.size() > ticks) ticks = tickValues.size();
  bool printTicks = colourbarprops["printticks"];
  std::string units = colourbarprops["units"];
  bool scientific = colourbarprops["scientific"];
  int precision = colourbarprops["precision"];
  float scaleval = colourbarprops["scalevalue"];
  float border = colourbarprops.getFloat("border", 1.0); //Use getFloat or will load global border prop as default
  if (border > 0) glLineWidth(border-0.5);
  else glLineWidth(0.5);

  //Always show at least two ticks on a log scale
  if (properties["logscale"] && ticks < 2) ticks = 2;
  // No ticks if no range
  if (minimum == maximum) ticks = 0;
  float fontscale = PrintSetFont(colourbarprops);
  for (int i = 0; i < ticks+2; i++)
  {
    /* Get tick value */
    float scaledPos;
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
      if (minimum == maximum) scaledPos = 0.5;
    }
    else
    {
      /* Calculate tick position */
      if (i > tickValues.size())  /* No fixed value provided */
      {
        /* First get scaled position 0-1 */
        if (properties["logscale"])
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
        if (properties["logscale"])
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
      else
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
    if (vertical) offset = height+5;
    glBegin(GL_LINES);
    VERT2(xpos, starty-5+offset, vertical);
    VERT2(xpos, starty+offset, vertical);
    glEnd();

    /* Always print end values, print others if flag set */
    char string[20];
    if (printTicks || i == 0 || i == ticks+1)
    {
      /* Apply any scaling factor  and show units on output */
      if ( !scientific && fabs(tickValue) <= FLT_MIN )
        sprintf( string, "0" );
      else
      {
        // For display purpose, scales the printed values if needed
        tickValue = scaleval * tickValue;
        char format[10];
        sprintf(format, "%%.%d%c%s", precision, scientific ? 'e' : 'g', units.c_str());
        sprintf(string, format, tickValue);
      }

      if (fontscale > 0.0)
      {
        if (vertical)
          lucPrint(starty + height + 10, xpos,  string);
        else
          lucPrint(xpos - (int) (0.5 * (float)lucPrintWidth(string)),  starty - 10, string);
      }
      else
      {
        glEnable(GL_MULTISAMPLE);
        if (vertical)
          Print(starty + height + 10, xpos - (int) (0.5 * (float)PrintWidth("W")),  string);
        else
          Print(xpos - (int) (0.5 * (float)PrintWidth(string)),  starty - 5 - PrintWidth("W"), string);
        glDisable(GL_MULTISAMPLE);
      }
    }
  }

  // Draw Box around colour bar
  if (border > 0)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    RECT2(startx, starty, startx + length, starty + height, vertical);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  glEnable(GL_MULTISAMPLE);
  glPopAttrib();
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
  if (!texture) texture = new TextureData();
  calibrate(0, 1);
  unsigned char paletteData[4*samples];
  Colour col;
  for (int i=0; i<samples; i++)
  {
    col = get(i / (float)(samples-1));
    //if (i%64==0) printf("RGBA %d %d %d %d\n", col.r, col.g, col.b, col.a);
    paletteData[i*4] = col.r;
    paletteData[i*4+1] = col.g;
    paletteData[i*4+2] = col.b;
    paletteData[i*4+3] = col.a;
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, samples, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, paletteData);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  if (repeat)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ColourMap::loadPalette(std::string data)
{
  //Two types of palette data accepted
  // - space separated colour list (process with parse())
  // - position=rgba colour palette (process here)
  if (data.find("=") == std::string::npos)
  {
     parse(data);
     return;
  }

  //Currently only support loading palettes with literal position data, not values to scale
  noValues = true;
  //Parse palette string into key/value pairs
  std::replace(data.begin(), data.end(), ';', '\n'); //Allow semi-colon separators
  std::stringstream is(data);
  colours.clear();
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

  if (texture)
  {
    loadTexture();
    //delete texture;
    //texture = NULL;
  }

  //Ensure at least two colours
  while (colours.size() < 2) add(0xff000000);
}

void ColourMap::print()
{
  for (unsigned int idx = 0; idx < colours.size(); idx++)
  {
    //Colour colour = getFromScaled(colours[idx].position);
    std::cout << idx << " : " << colours[idx] << std::endl;
  }
}

