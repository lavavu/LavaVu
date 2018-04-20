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
  Shader(const std::string& vshader, const std::string& fshader);
  Shader(const std::string& gshader, const std::string& vshader, const std::string& fshader);
  //Shader(const std::string& shader, GLenum shader_type);
  void init(std::string gsrc, std::string vsrc, std::string fsrc);

  bool version();
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
  void setUniformMatrixf(const std::string& name, float matrix[16], bool transpose=false);

  std::map<std::string, GLint> uniforms;
  std::map<std::string, GLenum> uniform_types;
  std::map<std::string, GLint> attribs;

  static std::string path;
};

typedef std::shared_ptr<Shader> Shader_Ptr;

#endif //Shaders__
