#ifndef Shaders__
#define Shaders__

#include "Util.h"
#include "GraphicsUtil.h"

class Shader
{
private:
  std::map<GLuint, GLuint> shaders;
  void print_log(const char *action, GLuint obj);

public:
  GLuint program;
  const char* gl_version;
  bool supported;

  Shader();
  Shader(const std::string& fshader);
  Shader(const std::string&, const std::string& fshader);
  Shader(const std::string& shader, GLenum shader_type);
  void init(std::string vsrc, std::string fsrc);

  bool version();
  std::string read_file(const std::string& fname);
  bool compile(const char *src, GLuint type);
  bool build();
  void use();
  void loadUniforms(const char** names, int count);
  void loadAttribs(const char** names, int count);
  void setUniform(const char* name, int value);
  void setUniform(const char* name, float value);
  void setUniformi(const char* name, int value);
  void setUniformf(const char* name, float value);
  void setUniform3f(const char* name, json value);

  std::map<std::string, GLint> uniforms;
  std::map<std::string, GLint> attribs;

  static std::string path;
};

#endif //Shaders__
