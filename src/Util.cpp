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

std::vector<std::string> FilePath::paths;

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
  std::string bpath = "";
  FilePath xpath;
#if defined(__EMSCRIPTEN__)
  bpath = ".";
#elif defined(_WIN32)
  TCHAR dest[MAX_PATH];
  DWORD length = GetModuleFileName(NULL, dest, MAX_PATH);
  xpath.parse(std::string(dest));
  bpath = xpath.path;
#else
  //Get from /proc on Linux
  char result[FILE_PATH_MAX] = "\0";
  ssize_t count = readlink("/proc/self/exe", result, FILE_PATH_MAX);
  if (count > 0)
  {
    xpath.parse(result);
    bpath = xpath.path;
  }
  //Try the PATH env var if argv0 contains no path info
  else if (!argv0 || strlen(argv0) == 0 || strcmp(argv0, progname) == 0)
  {
    std::stringstream path(getenv("PATH"));
    std::string pathentry;
    while (std::getline(path, pathentry, ':'))
    {
      std::string pstr = pathentry + "/" + progname;
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
    //Resolve argv[0] with readlink in case it's a symlink
    count = readlink(argv0, result, FILE_PATH_MAX);
    if (count > 0)
      xpath.parse(result);
    else
      xpath.parse(argv0);
    bpath = xpath.path;
  }

  //Convert to absolute path
  char *real_path = realpath(bpath.c_str(), NULL);
  if (real_path) 
  {
    bpath = real_path;
    free(real_path);
  }
#endif
  return bpath;
}

std::ostream & operator<<(std::ostream &os, const Range& range)
{
  return os << "[" << range.minimum << "," << range.maximum << "]";
}

bool Range::update(const float& min, const float& max)
{
  bool modified = false;
  if (min < minimum)
  {
    minimum = min;
    modified = true;
  }
  if (max > maximum)
  {
    maximum = max;
    modified = true;
  }
  return modified;
}

void FloatValues::minmax()
{
  if (minimum < maximum) return;
  auto minmax = std::minmax_element(value.begin(), value.begin()+next);
  minimum = *minmax.first;
  maximum = *minmax.second;
}

bool Properties::has(const std::string& key) {return data.count(key) > 0 && !data[key].is_null();}

bool Properties::hasglobal(const std::string& key) {return globals.count(key) > 0 && !globals[key].is_null();}

json& Properties::operator[](const std::string& key)
{
  //std::cout << key << " : " << data.count(key) << std::endl;
  //std::cout << key << " :: DATA\n" << data << std::endl;
  //std::cout << key << " :: DEFAULTS\n" << defaults << std::endl;
  if (data.count(key) && !data[key].is_null())
    return data[key];
  else if (globals.count(key) && !globals[key].is_null())
    return globals[key];
  return defaults[key];
}

//Functions to get values with provided defaults instead of system defaults
Colour Properties::getColour(const std::string& key, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
  if (data.count(key) || globals.count(key))
  {
    json val = (*this)[key];
    return Colour(val, red, green, blue, alpha);
  }
  Colour colour = {red, green, blue, alpha};
  return colour;
}

float Properties::getFloat(const std::string& key, float def)
{
  if (data.count(key) || globals.count(key))
    return (*this)[key];
  return def;
}

int Properties::getInt(const std::string& key, int def)
{
  if (data.count(key) || globals.count(key))
    return (*this)[key];
  return def;
}

bool Properties::getBool(const std::string& key, bool def)
{
  if (data.count(key) || globals.count(key))
    return (*this)[key];
  return def;
}

void Properties::mergeJSON(json& dest, json& src)
{
  //Merge: keep existing values, replace any imported
  for (json::iterator it = src.begin(); it != src.end(); ++it)
  {
    if (!it.value().is_null())
      dest[it.key()] = it.value();
  }
}

void Properties::merge(json& other)
{
  //Merge: keep existing values, replace any imported
  mergeJSON(data, other);
  //Run a type check
  checkall();
}

void Properties::checkall(bool strict)
{
  //Ensure all loaded values are of correct types by checking against default types
  check(data, defaults, strict);
  check(globals, defaults, strict);
}

void Properties::check(json& props, json& defaults, bool strict)
{
  //Ensure all loaded values are of correct types by checking against default types
  for (json::iterator it = props.begin(); it != props.end(); ++it)
  {
    if (!it.value().is_null())
    {
      if (!typecheck(props[it.key()], defaults, it.key(), strict))
        debug_print("DATA key: %s had incorrect type\n", it.key().c_str());
    }
  }
}

bool Properties::typecheck(json& val, json& defaults, const std::string& key, bool strict)
{
  try
  {
    if (key == "colourby" || key == "opacityby" || key == "sizeby" ||
        key == "widthby" || key == "heightby" || key == "lengthby")
    {
        //Skip these, can be int or string
        return true;
    }
    json& def = defaults[key];
    if (val.type() == def.type()) return true;
    if (val.is_number() && def.is_number()) return true;
    if (def.is_array()) return true; //Skip check on arrays

    //Convert a json type to the correct type
    //Only do this where a sensible conversion is possible
    if (def.is_boolean())
    {
      //Bool conversion
      debug_print("Attempting to coerce value to BOOLEAN\n");
      val = val != 0;
    }
    if (def.is_number())
    {
      //Num conversion
      debug_print("Attempting to coerce value to NUMBER\n");
      if (val.is_boolean())
        val = val ? 1 : 0;
      else if (val.is_string())
      {
        std::string sval = val;
        //Check for alias string values:
        if (sval == "blur" || sval == "ellipsoid")
          val = 0;
        else if (sval == "smooth" || sval == "cuboid")
          val = 1;
        else if (sval == "sphere")
          val = 2;
        else if (sval == "shiny")
          val = 3;
        else if (sval == "flat")
          val = 4;
        else
        {
          std::stringstream ss;
          ss << sval;
          float v = 0.0;
          ss >> v;
          val = v;
          if (v - (int)v == 0.0) val = (int)v;
        }
      }
      else
        val = (float)val;
    }
    //Only convert to string if strict checking enabled (some props can be string or number)
    if (strict && def.is_string() && !val.is_string())
    {
      //String conversion
      debug_print("Attempting to coerce value to STRING\n");
      std::stringstream ss;
      ss << val;
      val = ss.str();
    }
  }
  catch (nlohmann::detail::type_error& e)
  {
    std::cerr << "Type check error on property: " << e.what() << std::endl;
    val = defaults[key];
  }

  return false;
}


void Properties::clear()
{
  data = json();
}

void Properties::replace(json src)
{
  data = src;
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

