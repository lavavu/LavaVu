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

#include "Util.h"
#include <string.h>
#include <math.h>
#include <sys/stat.h>

FILE* infostream = NULL;

long membytes__ = 0;
long mempeak__ = 0;

void abort_program(const char * s, ...)
{
  char buffer[2048];
  va_list args;
  va_start(args, s);
  vsprintf(buffer, s, args);
  va_end(args);
  strcat(buffer, "\n");
  fputs(buffer, stderr);
  throw std::runtime_error(buffer);
}

bool FileExists(const std::string& name)
{
  FILE *file = fopen(name.c_str(), "r");
  if (file)
  {
    fclose(file);
    return true;
  }
  return false;
}

std::string GetBinaryPath(const char* argv0, const char* progname)
{
  //Try the PATH env var if argv0 contains no path info
  std::string bpath = "";
  if (!argv0 || strlen(argv0) == 0 || strcmp(argv0, progname) == 0)
  {
    std::stringstream path(getenv("PATH"));
    std::string pathentry;
    while (std::getline(path, pathentry, ':'))
    {
      std::string pstr = pathentry + "/" + progname;
#ifdef _WIN32
      pstr += ".exe";
#endif
      if (access(pstr.c_str(), X_OK) == 0)
      {
        //Need to make sure it's a regular file, not directory
        struct stat st_buf;
        if (stat(pstr.c_str(), &st_buf) == 0 && S_ISREG(st_buf.st_mode))
          bpath = pathentry + "/";
      }
    }
  }
  else
  {
    FilePath xpath;
    xpath.parse(argv0);
    bpath = xpath.path;
  }

  //Convert to absolute path
  char *real_path = realpath(bpath.c_str(), NULL);
  if (real_path) 
  {
    bpath = std::string(real_path) + "/";
    free(real_path);
  }
  return bpath;
}

void Properties::toFloatArray(const json& val, float* array, unsigned int size)
{
  //Convert to a float array
  for (unsigned int i=0; i<size; i++)
  {
    if (i >= val.size())
      array[i] = 0.0; //Zero pad if too short
    else
      array[i] = val[i];
  }
}

bool Properties::has(const std::string& key) {return data.count(key) > 0 && !data[key].is_null();}

json& Properties::operator[](const std::string& key)
{
  //std::cout << key << " : " << data.count(key) << std::endl;
  //std::cout << key << " :: DATA\n" << data << std::endl;
  //std::cout << key << " :: DEFAULTS\n" << defaults << std::endl;
  if (data.count(key))
    return data[key];
  else if (globals.count(key))
    return globals[key];
  return defaults[key];
}

//Functions to get values with provided defaults
Colour Properties::getColour(const std::string& key, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
  Colour colour = {red, green, blue, alpha};
  if (data.count(key) == 0) return colour;
  return Colour(data[key], red, green, blue, alpha);
}

float Properties::getFloat(const std::string& key, float def)
{
  if (data.count(key) == 0) return def;
  return data[key];
}

int Properties::getInt(const std::string& key, int def)
{
  if (data.count(key) == 0) return def;
  return data[key];
}

bool Properties::getBool(const std::string& key, bool def)
{
  if (data.count(key) == 0) return def;
  return data[key];
}

//Parse multi-line string
void Properties::parseSet(const std::string& properties)
{
  //Properties can be provided as valid json object {...}
  //in which case, parse directly
  if (properties.length() > 2 && properties.at(0) == '{')
  {
    json props = json::parse(properties);
    merge(props);
  }
  //Otherwise, provided as single prop=value per line 
  //where value is a parsable as json
  else
  {
    std::stringstream ss(properties);
    std::string line;
    while (std::getline(ss, line))
      parse(line);
  }
}

//Property containers now using json
void Properties::parse(const std::string& property, bool global)
{
  //Parse a key=value property where value is a json object
  json& dest = global ? globals : data; //Parse into data by default
  std::string key, value;
  size_t pos = property.find("=");
  key = property.substr(0,pos);
  value = property.substr(pos+1);
  if (value.length() > 0)
  {
    //Ignore case
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    //std::cerr << "Key " << key << " == " << value << std::endl;
    std::string valuel = value;
    std::transform(valuel.begin(), valuel.end(), valuel.begin(), ::tolower);
    
    try
    {
      //Parse simple increments and decrements
      int end = key.length()-1;
      char prev = key.at(end);
      if (prev == '+' || prev == '-')
      {
        std::string mkey = key.substr(0,end);
        std::stringstream ss(value);
        float parsedval;
        ss >> parsedval;
        float val = dest[mkey];
        if (prev == '+')
          dest[mkey] = val + parsedval;
        else if (prev == '-')
          dest[mkey] = val - parsedval;

      }
      else if (valuel == "true")
      {
        dest[key] = true;
      }
      else if (valuel == "false")
      {
        dest[key] = false;
      }
      else
      {
        dest[key] = json::parse(value);
      }
    }
    catch (std::exception& e)
    {
      //std::cerr << e.what() << " : [" << key << "] => " << value;
      //Treat as a string value
      dest[key] = value;
    }
  }
}

void Properties::merge(json& other)
{
  //Merge: keep existing values, replace any imported
  for (json::iterator it = other.begin() ; it != other.end(); ++it)
  {
    if (!it.value().is_null())
      data[it.key()] = it.value();
  }
}

void debug_print(const char *fmt, ...)
{
  if (fmt == NULL || infostream == NULL) return;
  va_list args;
  va_start(args, fmt);
  //vprintf(fmt, args);
  vfprintf(infostream, fmt, args);
  //debug_print("\n");
  va_end(args);
}

