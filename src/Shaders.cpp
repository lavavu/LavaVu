#include "Shaders.h"

#ifndef SHADER_PATH
std::string Shader::path = "";
#else
std::string Shader::path = SHADER_PATH;
#endif
std::string Shader::gl_version = "";
bool Shader::supported = false;

//Default shaders
const char *vertexShader = R"(
void main(void)
{
  gl_TexCoord[0] = gl_MultiTexCoord0;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  gl_FrontColor = gl_Color;
}
)";

const char *fragmentShader = R"(
void main(void)
{
  gl_FragColor = gl_Color;
}
)";

Shader::Shader()
{
  //If default constructor used, must manually call init, passing own shader source strings
}

Shader::Shader(const std::string& fshader)
{
  //This constructor for a fragment shader only
  std::string fsrc = read_file(fshader);
  init("", "", fsrc);
}

Shader::Shader(const std::string& vshader, const std::string& fshader)
{
  //This constructor is for a single vertex and fragment shader only
  std::string vsrc = read_file(vshader);
  std::string fsrc = read_file(fshader);
  init(vsrc, "", fsrc);
}

Shader::Shader(const std::string& vshader, const std::string& gshader, const std::string& fshader)
{
  //This constructor is for a geometry, vertex and fragment shader
  std::string vsrc = read_file(vshader);
  std::string gsrc = read_file(gshader);
  std::string fsrc = read_file(fshader);
  init(vsrc, gsrc, fsrc);
}
/*
Shader::Shader(const std::string& shader, GLenum shader_type)
{
  //This constructor is for a custom shader of specified type (eg: GL_COMPUTE_SHADER)
  std::string src = read_file(shader);
  program = 0;
  for (auto s : shaders)
    glDeleteShader(s);
  shaders.clear();
  supported = version();
  if (!supported) return;
  //Attempts to load and build shader programs
  if (compile(src.c_str(), shader_type)) build();
}
*/

//Hack for no geom shader support
#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER GL_VERTEX_SHADER
#endif

void Shader::init(std::string vsrc, std::string gsrc, std::string fsrc)
{
  program = 0;
  for (auto s : shaders)
    glDeleteShader(s);
  shaders.clear();
  supported = version();
  if (!supported) return;
  //Default shaders
  if (fsrc.length() == 0) fsrc = std::string(fragmentShader);
  if (vsrc.length() == 0) vsrc = std::string(vertexShader);
  //Prepend GLSL version
  if (vsrc.length()) vsrc = "#version 120\n" + vsrc;
  if (fsrc.length()) fsrc = "#version 120\n" + fsrc;
  //Attempts to load and build shader programs
  if (compile(vsrc.c_str(), GL_VERTEX_SHADER) &&
      (gsrc.length() == 0 || compile(gsrc.c_str(), GL_GEOMETRY_SHADER)) &&
      compile(fsrc.c_str(), GL_FRAGMENT_SHADER))
  {
    //Compile succeeded, link program
    build();
  }
}

bool Shader::version()
{
  const char* gl_v = (const char*)glGetString(GL_VERSION);
  if (!gl_v) return false;
  gl_version = std::string(gl_v);
  return true;
}

//Read a fragment or vertex shader from a file into a shader object
std::string Shader::read_file(const std::string& fname)
{
  if (!fname.length()) return std::string("");

  //If shader found locally in working directory, use it instead
  std::string filepath;
  if (Shader::path.length() > 0 && !FileExists(fname))
    filepath = Shader::path;

  filepath += fname;
  debug_print("Shader loading: %s\n", filepath.c_str());

  std::ifstream ifs(filepath);
  std::stringstream buffer;
  if (ifs.is_open())
    buffer << ifs.rdbuf();
  else
    std::cerr << "Error opening shader: " << filepath << std::endl;
  return buffer.str();
}

bool Shader::compile(const char *src, GLuint type)
{
  GLint compiled;
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled)
    print_log("Shader Compile", shader);
  else
    shaders.push_back(shader);
  GL_Error_Check;
  return compiled;
}

//Attach and link shaders into program
bool Shader::build()
{
  GLint linked;
  if (!supported) return false;

  if (program) glDeleteProgram(program);
  program = glCreateProgram();
  assert(glIsProgram(program));

  for (auto s : shaders)
  {
    if (glIsShader(s))
      glAttachShader(program, s);
  }

  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if(!linked)
  {
    print_log("Shader Link", program);
    return false;
  }
  return true;
}

//Report shader compile/link errors
void Shader::print_log(const char *action, GLuint obj)
{
  if (!supported) return;
  int infologLength = 0;
  int maxLength;

  if(glIsShader(obj))
    glGetShaderiv(obj,GL_INFO_LOG_LENGTH,&maxLength);
  else
    glGetProgramiv(obj,GL_INFO_LOG_LENGTH,&maxLength);

  char* infoLog = new char[maxLength];

  if (glIsShader(obj))
    glGetShaderInfoLog(obj, maxLength, &infologLength, infoLog);
  else
    glGetProgramInfoLog(obj, maxLength, &infologLength, infoLog);

  if (infologLength > 0)
    fprintf(stderr, "%s:\n%s\n", action, infoLog);

  delete[] infoLog;
}

void Shader::use()
{
  if (!supported || !program) return;
  glUseProgram(program);
  GL_Error_Check;
}

void Shader::loadUniforms()
{
  if (!supported || !program) return;
  GLint uniform_count;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
  GLsizei len;
  GLint size;
  GLenum type;
  char name[1024];
  for (GLint i=0; i<uniform_count; i++)
  {
    glGetActiveUniform(program, i, 1023, &len, &size, &type, name);
    GLint location = glGetUniformLocation(program, name);
    //NVidia returns name[0] for arrays, need to strip after [
    char* nname = strchr(name, '[');
    if (nname) nname[0] = '\0';
    uniforms[name] = location;
    uniform_types[name] = type;
    //std::cout << "UNIFORM : " << name << " @ " << i << " type " << type << std::endl;
  }
}

void Shader::loadAttribs()
{
  if (!supported || !program) return;
  GLint attrib_count;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attrib_count);
  GLsizei len;
  GLint size;
  GLenum type;
  char name[1024];
  for (GLint i=0; i<attrib_count; i++)
  {
    glGetActiveAttrib(program, i, 1023, &len, &size, &type, name);
    GLint location = glGetAttribLocation(program, name);
    attribs[name] = location;
    //attrib_types[name] = type;
    //std::cout << "ATTRIB : " << name << " @ " << i << " type " << type << std::endl;
  }
}

void Shader::setUniform(const std::string& name, json& value)
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it == uniforms.end()) return;

  //Handle integer/float uniforms, singular, vector values (2,3 or 4) or scalar arrays (>4)
  //NOTE: this is a bit error prone, probably need to flag int/float by parsing shader rather than
  //detecting from the json dict
  //std::cout << name << " => " << value << " " << " @ " << uniforms[name] << std::endl;
  int dims = 1;
  float* fdat = NULL;
  int* idat = NULL;
  if (value.is_array())
  {
    //Array values
    dims = value.size();
    fdat = new float[dims];
    idat = new int[dims];
    Properties::toArray<int>(value, idat, dims);
    Properties::toArray<float>(value, fdat, dims);
  }

  GL_Error_Check

  switch (uniform_types[name])
  {
    case GL_FLOAT:
      if (dims == 1)
        glUniform1f(uniforms[name], (float)value);
      else //Array of scalars
        glUniform1fv(uniforms[name], dims, fdat);
      GL_Error_Print
      break;
    case GL_INT:
    case GL_BOOL:
      if (dims == 1)
        glUniform1i(uniforms[name], (int)value);
      else //Array of scalars
        glUniform1iv(uniforms[name], dims, idat);
      GL_Error_Print
      break;
    case GL_FLOAT_VEC2:
      assert(dims>=2);
      glUniform2fv(uniforms[name], 1, fdat);
      GL_Error_Print
      break;
    case GL_INT_VEC2:
      glUniform2iv(uniforms[name], 1, idat);
      GL_Error_Print
      break;
    case GL_FLOAT_VEC3:
      glUniform3fv(uniforms[name], 1, fdat);
      GL_Error_Print
      break;
    case GL_INT_VEC3:
      glUniform3iv(uniforms[name], 1, idat);
      GL_Error_Print
      break;
    case GL_FLOAT_VEC4:
      glUniform4fv(uniforms[name], 1, fdat);
      GL_Error_Print
      break;
    case GL_INT_VEC4:
      glUniform4iv(uniforms[name], 1, idat);
      GL_Error_Print
      break;
  }

  GL_Error_Check;
  if (fdat) delete[] fdat;
  if (idat) delete[] idat;
}

void Shader::setUniform(const std::string& name, int value)   {setUniformi(name, value);}
void Shader::setUniform(const std::string& name, float value) {setUniformf(name, value);}

void Shader::setUniform(const std::string& name, Colour& colour)
{
  float array[4];
  colour.toArray(array);
  setUniform4f(name, array);
}

void Shader::setUniformi(const std::string& name, int value)
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0) glUniform1i(loc, value);
    GL_Error_Check;
  }
}

void Shader::setUniformf(const std::string& name, float value)
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0) glUniform1f(loc, value);
    GL_Error_Check;
  }
}

void Shader::setUniform2f(const std::string& name, float value[2])
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0)
      glUniform2fv(loc, 1, value);
    GL_Error_Check;
  }
}

void Shader::setUniform3f(const std::string& name, float value[3])
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0)
      glUniform3fv(loc, 1, value);
    GL_Error_Check;
  }
}

void Shader::setUniform4f(const std::string& name, float value[4])
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0)
      glUniform4fv(loc, 1, value);
    GL_Error_Check;
  }
}

void Shader::setUniformMatrixf(const std::string& name, float matrix[16], bool transpose)
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0)
      glUniformMatrix4fv(loc, 1, transpose ? GL_TRUE : GL_FALSE, matrix);
    GL_Error_Check;
  }
}
