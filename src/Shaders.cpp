#include "Shaders.h"

#ifndef SHADER_PATH
std::string Shader::path = "";
#else
std::string Shader::path = SHADER_PATH;
#endif

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
  //Use both default shaders
  init("", "");
}

Shader::Shader(const std::string& fshader)
{
  //This constructor for a fragment shader only
  std::string fsrc = read_file(fshader);
  init("", fsrc);
}

Shader::Shader(const std::string& vshader, const std::string& fshader)
{
  //This constructor is for a single vertex and/or fragment shader only
  std::string vsrc = read_file(vshader);
  std::string fsrc = read_file(fshader);
  init(vsrc, fsrc);
}

Shader::Shader(const std::string& shader, GLenum shader_type)
{
  //This constructor is for a custom shader of specified type (eg: GL_COMPUTE_SHADER)
  std::string src = read_file(shader);
  program = 0;
  for (auto s : shaders)
    glDeleteShader(s.second);
  shaders.clear();
  supported = version();
  if (!supported) return;
  //Attempts to load and build shader programs
  if (compile(src.c_str(), shader_type)) build();
}

void Shader::init(std::string vsrc, std::string fsrc)
{
  program = 0;
  for (auto s : shaders)
    glDeleteShader(s.second);
  shaders.clear();
  supported = version();
  if (!supported) return;
  //Default shaders
  if (fsrc.length() == 0) fsrc = std::string(fragmentShader);
  if (vsrc.length() == 0) vsrc = std::string(vertexShader);
  //Attempts to load and build shader programs
  if (compile(vsrc.c_str(), GL_VERTEX_SHADER) &&
      compile(fsrc.c_str(), GL_FRAGMENT_SHADER)) build();
}

bool Shader::version()
{
  gl_version = (const char*)glGetString(GL_VERSION);
  if (!gl_version) return false;
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
    shaders[type] = shader;
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
    if (glIsShader(s.second))
      glAttachShader(program, s.second);
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

void Shader::loadUniforms(const char** names, int count)
{
  if (!supported || !program) return;
  for (int i=0; i<count; i++)
  {
    GLint loc = glGetUniformLocation(program, names[i]);
    if (loc < 0)
      debug_print("Uniform '%s' not found\n", names[i]);
    uniforms[names[i]] = loc;
  }
}

void Shader::loadAttribs(const char** names, int count)
{
  if (!supported || !program) return;
  for (int i=0; i<count; i++)
  {
    GLint loc = glGetAttribLocation(program, names[i]);
    if (loc < 0)
      debug_print("Attrib '%s' not found\n", names[i]);
    attribs[names[i]] = loc;
  }
}

void Shader::setUniform(const char* name, int value)   {setUniformi(name, value);}
void Shader::setUniform(const char* name, float value) {setUniformf(name, value);}

void Shader::setUniformi(const char* name, int value)
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

void Shader::setUniformf(const char* name, float value)
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

void Shader::setUniform3f(const char* name, json value)
{
  if (!supported || !program) return;
  std::map<std::string,int>::iterator it = uniforms.find(name);
  if (it != uniforms.end())
  {
    GLint loc = uniforms[name];
    if (loc >= 0)
    {
      float fval[3];
      Properties::toArray<float>(value, fval, 3);
      glUniform3fv(loc, 1, fval);
    }
    GL_Error_Check;
  }
}
