#ifndef Shaders__
#define Shaders__

#include "Util.h"
#include "GLUtils.h"

class Shader
{
private:
  std::vector<GLuint> shaders;
  void print_log(const char *action, GLuint obj);

public:
  GLuint program;

  Shader();
  Shader(const std::string& fshader);
  Shader(const std::string& vshader, const std::string& fshader);
  Shader(const std::string& vshader, const std::string& gshader, const std::string& fshader);
  //Shader(const std::string& shader, GLenum shader_type);
  void init(std::string vsrc, std::string gsrc, std::string fsrc);

  std::string read_file(const std::string& fname);
  bool compile(const char *src, GLuint type);
  bool build();
  void use();
  void loadUniforms();
  void loadAttribs();
  void setUniform(const std::string& name, json& value);
  void setUniform(const std::string& name, int value);
  void setUniform(const std::string& name, float value);
  void setUniform(const std::string& name, Colour& colour);
  void setUniformi(const std::string& name, int value);
  void setUniformf(const std::string& name, float value);
  void setUniform2f(const std::string& name, float value[2]);
  void setUniform3f(const std::string& name, float value[3]);
  void setUniform4f(const std::string& name, float value[4]);
  void setUniformMatrixf(const std::string& name, mat4& matrix, bool transpose=false);

  std::map<std::string, GLint> uniforms;
  std::map<std::string, GLenum> uniform_types;
  std::map<std::string, GLint> attribs;
};

typedef std::shared_ptr<Shader> Shader_Ptr;

#endif //Shaders__
