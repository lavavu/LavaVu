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
#include "Colours.h"

#define INBUF_SIZE 65535

//Fast random int with fixed seed for deterministic random sample (SEED should be uint32_t)
#define SHR3(SEED) (SEED^=(SEED<<13), SEED^=(SEED>>17), SEED^=(SEED<<5))

#define GET_VAR_ARGS(fmt, buffer) \
{ \
  va_list ap;                 /* Pointer to arguments list         */ \
  assert(fmt != NULL);        /* No format string                  */ \
  va_start(ap, fmt);          /* Parse format string for variables */ \
  vsprintf(buffer, fmt, ap);  /* Convert symbols                   */ \
  va_end(ap);                                                         \
} 

//Skip lock/guard when pthreads not supported
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#define LOCK_GUARD(m) std::lock_guard<std::mutex> guard(m);
#else
#define LOCK_GUARD(m) ;
#endif

extern FILE* infostream;
void abort_program(const char * s, ...);
void debug_print(const char *fmt, ...);

bool FileExists(const std::string& name);

//Class for handling filenames/paths
#define FILE_PATH_MAX 4096
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
    if (fp.length() == 0) return;
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

  bool empty() {return full.empty();}

  static std::vector<std::string> paths;
};

std::string GetBinaryPath(const char* argv0, const char* progname);

class Range
{
 public:
  //This class defines a min/max range
  //default constructor initialises a null range correctly for comparisons
  float minimum;
  float maximum;
  Range(const float& min=HUGE_VALF, const float& max=-HUGE_VALF) : minimum(min), maximum(max) {}
  bool valid() const {return minimum <= maximum;}
  float* data() {return &minimum;}
  bool update(const float& min, const float& max);
  friend std::ostream& operator<<(std::ostream& stream, const Range& range);
};

typedef struct
{
  json by;
  unsigned int dataIdx;
  float minimum;
  float maximum;
  bool map;
  bool out;
  bool inclusive;
  int elements;
} Filter;

//General purpose geometry data store types...
extern long membytes__;
extern long mempeak__;

class DataContainer
{
protected:
  unsigned int next;
public:
  unsigned int datasize;
  float minimum;
  float maximum;
  std::string label;

  DataContainer() : next(0), datasize(1), minimum(0), maximum(0), label("") {}

  //Pure virtual methods
  virtual unsigned int bytes() = 0;
  virtual void read(unsigned int n, const void* data) = 0;
  virtual void resize(unsigned long size) = 0;
  virtual void clear() = 0;
  virtual void erase(unsigned int start, unsigned int end) = 0;
  virtual void* ref(unsigned i=0) = 0;

  unsigned int size()
  {
    //Returns size in base data type
    return next;
  }

  unsigned int count()
  {
    //Returns size in data units
    return next / datasize;
  }

  unsigned int unitsize()
  {
    //Returns size of data units
    return datasize;
  }
};

template <class dtype> class DataValues : public DataContainer
{
public:
  std::vector<dtype> value;

  DataValues() {}
  virtual ~DataValues() {membytes__ -= sizeof(dtype)*value.size();}

  unsigned int bytes() {return sizeof(dtype)*size();}

  virtual void read1(const dtype& data) = 0;

  virtual void read(unsigned int n, const void* data)
  {
    unsigned int size = next + n;
    unsigned int oldsize = value.size();
    if (oldsize < size)
    {
      //Always at least double size for efficiency when reading 1 value at a time
      if (n == 1 && size < oldsize*2)
        size = oldsize*2;
      else if (n > 1)
        size = oldsize + n;
      resize(size);
    }
    memcpy(&value[next], data, n * sizeof(dtype));
    next += n;
  }

  inline dtype operator[] (unsigned i)
  {
    //if (i >= value.size())
    //   abort_program("Out of bounds %d -- %d (max idx %d)\n", i, i, value.size()-1);
    return value[i];
  }

  void* ref (unsigned i=0)
  {
    return (void*)&value[i];
  }

  void resize(unsigned long size)
  {
    unsigned int oldsize = value.size();
    if (oldsize < size)
    {
      value.resize(size);
      membytes__ += sizeof(dtype)*(size-oldsize);
      if (membytes__ > mempeak__) mempeak__ = membytes__;
      //printf("============== MEMORY total %.3f mb, added %d ==============\n", membytes__/1000000.0f, (size-oldsize));
    }
  }

  void clear()
  {
    unsigned int count = value.size();
    if (count == 0) return;
    value.clear();
    membytes__ -= sizeof(dtype)*count;
    next = 0;
    //printf("============== MEMORY total %.3f mb, removed %d ==============\n", membytes__/1000000.0f, count);
  }

  void erase(unsigned int start, unsigned int end)
  {
    //erase elements:
    value.erase(value.begin()+start, value.begin()+end);
    membytes__ -= sizeof(dtype)*(end - start);
    //printf("============== MEMORY total %.3f mb, erased %d ==============\n", membytes__/1000000.0f, (end - start));
  }
};

class FloatValues : public DataValues<float>
{
 public:
  FloatValues() {}
  void read1(const float& data) {read(1, &data);}

  void minmax();
};

class UIntValues : public DataValues<unsigned int>
{
 public:
  UIntValues() {}
  void read1(const unsigned int& data) {read(1, &data);}
};

class UCharValues : public DataValues<unsigned char>
{
 public:
  UCharValues() {}
  void read1(const unsigned char& data) {read(1, &data);}
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

//Shared pointer for FloatValues
typedef std::shared_ptr<FloatValues> Values_Ptr;

//JSON Property set wrapper class
class Properties
{
public:
  json& globals;
  json& defaults;
  json data;

  Properties(json& globals, json& defaults) : globals(globals), defaults(defaults)
  {
    data = json::object();
  }

  template<typename T>
  static void toArray(const json& val, T* array, unsigned int size);
  static void mergeJSON(json& dest, json& src);

  bool has(const std::string& key);
  bool hasglobal(const std::string& key);
  json& operator[](const std::string& key);
  Colour getColour(const std::string& key, unsigned char red=0, unsigned char green=0, unsigned char blue=0, unsigned char alpha=255);
  float getFloat(const std::string& key, float def);
  int getInt(const std::string& key, int def);
  bool getBool(const std::string& key, bool def);
  void merge(json& other);
  void checkall(bool strict=true);
  void clear();
  void replace(json src);
  static void check(json& props, json& defaults, bool strict=true);
  static bool typecheck(json& val, json& defaults, const std::string& key, bool strict=true);

};

template<typename T>
void Properties::toArray(const json& val, T* array, unsigned int size)
{
  //Convert to a array of type T
  try
  {
    if (val.is_number())
    {
      array[0] = (T)val;
      for (unsigned int i=1; i<size; i++)
        array[i] = (T)0.0; //Zero pad if too short
    }
    else
    {
      for (unsigned int i=0; i<size; i++)
      {
        array[i] = (T)0.0; //Zero pad if too short
        if (i < val.size())
          array[i] = (T)val[i];
      }
    }
  }
  catch (const std::domain_error& e)
  {
    std::cout << "Invalid value in property: " << val << std::endl;
  }
}

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
    std::string key;

    iss >> key;
    if (ignoreCase)
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    props[key] = prop_values();

    //Read values into vector until stream empty
    size_t last = 0;
    while (iss.good())
    {
      std::string value;
      iss >> value;
      //Detect quotes, if found read until end quote if any
      if (value.length() > 0 && value.at(0) == '"')
      {
        size_t start = line.find('"', last);
        size_t end = line.find('"', start+1);
        if (end <= start) continue;
        iss.seekg(end+2, std::ios_base::beg);
        value = line.substr(start+1, end-start-1);
        last = end+2;
      }

      props[key].push_back(value);
      //std::cerr << key << " => " << value << std::endl;
    }
  }

  std::string get(std::string key, unsigned int idx=0)
  {
    try
    {
      if (ignoreCase)
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
      if (props.find(key) != props.end())
      {
        if (idx < props[key].size())
          return props[key][idx];
      }
    }
    catch (std::exception& e)
    {
      std::cout << "Error : " << e.what() << std::endl;
    }
    return std::string("");
  }

  std::string getall(std::string key, unsigned int idx=0)
  {
    if (ignoreCase)
      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    std::string all = "";
    if (props.find(key) != props.end())
    {
      for (; idx < props[key].size(); idx++)
      {
        if (all.length() > 0) all += " ";
        all += props[key][idx];
      }
    }
    return all;
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

  float Float(std::string key, float def=0.0, unsigned int idx=0)
  {
    std::stringstream parsess(get(key, idx));
    float val;
    if (!(parsess >> val))
      return def;
    return val;
  }

  bool Boolean(std::string key, bool def=false, unsigned int idx=0)
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
