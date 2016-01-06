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

FILE* infostream = NULL;

long FloatValues::membytes = 0;
long FloatValues::mempeak = 0;

std::string GetBinaryPath(const char* argv0, const char* progname)
{
  //Try the PATH env var if argv0 contains no path info
  FilePath xpath;
  if (!argv0 || strlen(argv0) == 0 || strcmp(argv0, progname) == 0)
  {
    std::stringstream path(getenv("PATH"));
    std::string line;
    while (std::getline(path, line, ':'))
    {
      std::stringstream pathentry;
      pathentry << line << "/" << argv0;
      std::string pstr = pathentry.str();
      const char* pathstr = pstr.c_str();
#ifdef _WIN32
      if (strstr(pathstr, ".exe"))
#else
      if (access(pathstr, X_OK) == 0)
#endif
      {
        xpath.parse(pathstr);
        break;
      }
    }
  }
  else
  {
    xpath.parse(argv0);
  }
  return xpath.path;
}

//Parse multi-line string
void jsonParseProperties(std::string& properties, json::Object& object)
{
  //Process all lines
  std::stringstream ss(properties);
  std::string line;
  while (std::getline(ss, line))
    jsonParseProperty(line, object);
}

//Property containers now using json
void jsonParseProperty(std::string& data, json::Object& object)
{
  //Parse a key=value property where value is a json object
  std::string key, value;
  std::istringstream iss(data);
  std::getline(iss, key, '=');
  std::getline(iss, value, '=');

  if (value.length() > 0)
  {
    //Ignore case
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    //std::cerr << "Key " << key << " == " << value << std::endl;

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
        if (prev == '+')
          object[mkey] = object[mkey].ToFloat() + parsedval;
        else if (prev == '-')
          object[mkey] = object[mkey].ToFloat() - parsedval;

      }
      else if (value.find("[") == std::string::npos && value.find("{") == std::string::npos)
      {
        //This JSON parser only accepts objects or arrays as base element
        std::string nvalue = "[" + value + "]";
        object[key] = json::Deserialize(nvalue).ToArray()[0];
      }
      else
      {
        object[key] = json::Deserialize(value);
      }
    }
    catch (std::exception& e)
    {
      //std::cerr << "[" << key << "] " << data << " : " << e.what();
      //Treat as a string value
      object[key] = json::Value(value);
    }
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

