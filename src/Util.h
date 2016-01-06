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

#ifndef Utils__
#define Utils__
#include "Include.h"
#include "GraphicsUtil.h"
#include "Colours.h"

#define INBUF_SIZE 65535

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define frand (rand() / (float) RAND_MAX)

extern FILE* infostream;
void abort_program(const char * s, ...);
void debug_print(const char *fmt, ...);

//Class for handling filenames/paths
class FilePath
{
public:
  std::string full;
  std::string path;
  std::string file;
  std::string base;
  std::string ext;
  std::string type; //Same as ext but always lowercase

  FilePath() {};
  FilePath(std::string fp)
  {
    parse(fp);
  }

  void parse(std::string fp)
  {
    full = fp;
    //From: http://stackoverflow.com/a/8520815/866759
    // Remove directory if present.
    // Do this before extension removal incase directory has a period character.
    const size_t last_slash = full.find_last_of("\\/");
    if (std::string::npos != last_slash)
    {
      path = full.substr(0, last_slash + 1);
      file = full.substr(last_slash + 1);
    }
    else
    {
      file = full;
    }

    // Remove extension if present.
    const size_t period = file.rfind('.');
    if (std::string::npos != period)
    {
      base = file.substr(0, period);
      ext = file.substr(period + 1);
      type = ext;
      std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    }
    else
    {
      base = file;
    }
  }
};

std::string GetBinaryPath(const char* argv0, const char* progname);

//General purpose geometry data store types...
typedef union
{
  float val;
  unsigned int idx;
} floatidx; /* For index values stored in float container */

class FloatValues
{
public:
  std::vector<float> value;
  unsigned int next;
  float minimum;
  float maximum;
  int datasize;
  float dimCoeff;   //For scaling
  std::string units;      //Scaling units
  unsigned int offset;
  bool generated;
  static long membytes;  //Track memory usage
  static long mempeak;

  FloatValues() : next(0), minimum(0), maximum(0), datasize(1), dimCoeff(1.0), offset(0), generated(false) {}
  ~FloatValues()
  {
    if (value.size()) clear();
  }

  virtual void read(unsigned int n, const void* data)
  {
    int size = next + n;
    int oldsize = value.size();
    if (oldsize < size)
    {
      //Always at least double size for efficiency
      if (size < oldsize*2) size = oldsize*2;
      resize(size);
    }
    memcpy(&value[next], data, n * sizeof(float));
    next += n;
  }

  void setup(float min, float max, float dimFactor, const char* units)
  {
    //if (min < minimum) minimum = min;
    //if (max > maximum) maximum = max;
    minimum = min;
    maximum = max;
    dimCoeff = dimFactor;
    this->units = std::string(units ? units : "");
  }

  inline float operator[] (unsigned i)
  {
    //if (i >= value.size())
    //   abort_program("Out of bounds %d -- %d (max idx %d)\n", i, i, value.size()-1);
    return value[i];
  }

  Colour toColour(unsigned i)
  {
    //Interpret a float value as RGBA colour
    Colour c;
    c.fvalue = value[i];
    return c;
  }

  void resize(unsigned long size)
  {
    unsigned int oldsize = value.size();
    if (oldsize < size)
    {
      value.resize(size);
      FloatValues::membytes += 4*(size-oldsize);
      if (FloatValues::membytes > FloatValues::mempeak) FloatValues::mempeak = FloatValues::membytes;
      //printf("============== MEMORY total %.3f mb, added %d ==============\n", FloatValues::membytes/1000000.0f, 4*(size-oldsize));
    }
  }

  unsigned int size()
  {
    return next;
  }
  void clear()
  {
    int count = value.size();
    if (count == 0) return;
    value.clear();
    offset = 0;
    FloatValues::membytes -= 4*count;
    next = 0;
    //if (bytes) printf("============== MEMORY total %.3f mb, removed %d ==============\n", FloatValues::membytes/1000000.0f, bytes);
  }

  //Update saved position
  void setOffset()
  {
    offset = value.size();
  }

  void erase(int start, int end)
  {
    //erase elements:
    value.erase(value.begin()+start, value.begin()+end);
    if (offset > 0) offset -= start;
    FloatValues::membytes -= 4*(end - start);
    //printf("============== MEMORY total %.3f mb, erased %d ==============\n", FloatValues::membytes/1000000.0f, (end - start));
  }
};

class Coord3DValues : public FloatValues
{
public:
  Coord3DValues()
  {
    datasize = 3;
  }

  void read(unsigned int n, const void* data)
  {
    FloatValues::read(n * 3, data);
  }

  inline float* operator[] (unsigned i)
  {
    //if (i*3 >= value.size())
    //   abort_program("Out of bounds %d -- %d (max idx %d)\n", i, i*3, value.size()-1);
    return &value[i * 3];
  }
};

class Coord2DValues : public FloatValues
{
public:
  Coord2DValues()
  {
    datasize = 2;
  }

  void read(unsigned int n, const void* data)
  {
    FloatValues::read(n * 2, data);
  }

  inline float* operator[] (unsigned i)
  {
    //if (i*2 >= value.size())
    //   abort_program("Out of bounds %d -- %d (max idx %d)\n", i, i*2, value.size()-1);
    return &value[i * 2];
  }
};

//Property containers now using json
void jsonParseProperties(std::string& properties, json::Object& object);
void jsonParseProperty(std::string& data, json::Object& object);

//Utility class for parsing property,value strings
typedef std::vector<std::string> prop_values;
typedef std::map<std::string, prop_values> prop_value_map;
typedef std::map<std::string, prop_values>::iterator prop_iterator_type;
class PropertyParser
{
  prop_value_map props;
  bool ignoreCase;

public:
  PropertyParser() : ignoreCase(true) {};

  PropertyParser(std::istream& is, char delim, bool ic=true) : ignoreCase(ic)
  {
    parse(is, delim);
  }
  PropertyParser(std::istream& is, bool ic=true) : ignoreCase(ic)
  {
    parse(is);
  }

  void stringify(std::ostream& os, char delim='=')
  {
    for (prop_iterator_type iterator = props.begin(); iterator != props.end(); iterator++)
      os << iterator->first << delim << get(iterator->first) << std::endl;
  }

  //Parse lines with delimiter, ie: key=value
  void parse(std::istream& is, char delim)
  {
    std::string line;
    while(std::getline(is, line))
    {
      parseLine(line, delim);
    }
  }

  //Parse lines as whitespace separated eg: " key   value"
  void parse(std::istream& is)
  {
    std::string line;
    while(std::getline(is, line))
    {
      parseLine(line);
    }
  }

  //Parse lines with delimiter, ie: key=value
  void parseLine(std::string& line, char delim)
  {
    std::istringstream iss(line);
    std::string temp, key, value;

    std::getline(iss, temp, delim);

    std::istringstream isskey(temp);
    isskey >> key;

    //Read values into vector until stream empty
    props[key] = prop_values();
    do
    {
      std::getline(iss, temp, delim);

      std::istringstream issval(temp);
      issval >> value;

      if (ignoreCase)
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

      props[key].push_back(value);
      //std::cerr << "Key " << key << " == " << value << std::endl;
    }
    while (iss.good());
    //std::cerr << props[key].size() << " values added to key: " << key << " [0] = " << props[key][0] << std::endl;
  }

  //Parse lines as whitespace separated eg: " key   value"
  void parseLine(std::string& line)
  {
    std::istringstream iss(line);
    std::string key, value;

    iss >> key;
    if (ignoreCase)
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    props[key] = prop_values();

    //Read values into vector until stream empty
    while (iss.good())
    {
      iss >> value;
      //Detect quotes, if found read until end quote if any
      if (value.length() > 0 && value.at(0) == '"')
      {
        size_t start = line.find('"');
        size_t end = line.find('"', start+1);
        if (end <= start) continue;
        iss.seekg(end+2, std::ios_base::beg);
        value = line.substr(start+1, end-start-1);
      }
      props[key].push_back(value);
      //std::cerr << key << " => " << value << std::endl;
    }
  }

  std::string get(std::string key, unsigned int idx=0)
  {
    if (ignoreCase)
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    if (props.find(key) != props.end())
    {
      if (idx < props[key].size())
        return props[key][idx];
    }
    return std::string("");
  }

  bool exists(std::string key)
  {
    if (ignoreCase)
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    if (props.find(key) != props.end()) return true;
    return false;
  }

  std::string operator[] (std::string key)
  {
    return get(key);
  }

  int Int(std::string key, int def=0, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    int val;
    if (!(parsess >> val))
      return def;
    return val;
  }

  unsigned int Hex(std::string key, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    unsigned int val;
    parsess >> std::hex >> val;
    return val;
  }

  unsigned int Colour(std::string key, unsigned int idx=0)
  {
    //Parse colour as RRGGBBAA hex string:
    //reverse order to get (AA)RRGGBB little endian hex, convert to ARGB int
    std::string str = get(key, idx);
    int len = str.length();
    if (len != 6 && len != 8) return LUC_BLACK;
    if (str.length() == 6) str += "ff";

    //Reverse components but preserve order of each 2 letter code
    if (str.length() == 6) //Alpha optional
      str = "ff" + str.substr(4,2) + str.substr(2,2) + str.substr(0,2);
    else
      str = str.substr(6,2) + str.substr(4,2) + str.substr(2,2) + str.substr(0,2);

    std::stringstream parsess(str);
    unsigned int val;
    parsess >> std::hex >> val;

    return val;
  }

  float Float(std::string key, float def=0.0, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    float val;
    if (!(parsess >> val))
      return def;
    return val;
  }

  bool Bool(std::string key, bool def=false, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    bool val;
    //Bools only parsed as integer, not "true" "false"
    //if (!(parsess >> std::boolalpha >> val)
    if (!(parsess >> val)) return def;
    return val;
  }

  bool has(int& val, std::string key, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    parsess >> val;
    return !parsess.fail();
  }

  bool has(float& val, std::string key, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    parsess >> val;
    return !parsess.fail();
  }

  bool has(bool& val, std::string key, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    parsess >> val;
    return !parsess.fail();
    //return (parsess >> std::boolalpha >> val);
  }


};

#endif //Utils__
