/*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
* LavaVu python interface
**~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

%module LavaVuPython
%include <std_string.i>
%include <std_vector.i>

%{
#include "src/LavaVu.h"
#include "src/ViewerTypes.h"
%}

%include "exception.i"
%exception {
    try {
        $action
    } catch (const std::runtime_error& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    }
}

namespace std 
{
%template(Line)  vector <float>;
%template(ULine) vector <unsigned int> ;
%template(Array) vector <vector <float> >;
%template(List) vector <string>;
}

%include "src/ViewerTypes.h"

class OpenGLViewer
{
  public:
    bool quitProgram;
};

class LavaVu
{
public:
  OpenGLViewer* viewer;
  Model* amodel;
  View* aview;
  DrawingObject* aobject;
  std::string binpath;

  LavaVu(std::string binpath);
  ~LavaVu();

  void run(std::vector<std::string> args={});

  bool loadFile(const std::string& file);
  bool parseCommands(std::string cmd);
  void render();
  void init();
  std::string image(std::string filename="", int width=0, int height=0, bool frame=false);
  std::string web(bool tofile=false);
  std::string video(std::string filename, int fps=30, int width=0, int height=0, int start=0, int end=0);
  void defaultModel();
  void setObject(unsigned int id, std::string properties);
  void setObject(std::string name, std::string properties);
  int colourMap(std::string name, std::string colours="", std::string properties="");
  std::string colourBar(std::string objname);
  void setState(std::string state);
  std::string getState();
  std::string getFigures();
  std::string getTimeSteps();
  void loadVectors(std::vector< std::vector <float> > array, lucGeometryDataType type=lucVertexData, const std::string& name="");
  void loadValues(std::vector <float> array, std::string label="", const std::string& name="");
  void loadUnsigned(std::vector <unsigned int> array, lucGeometryDataType type=lucIndexData, const std::string& name="");
  void loadColours(std::vector <std::string> list, const std::string& name);
  void labels(std::vector <std::string> labels, const std::string& name="");
  void close();
  std::vector<float> imageArray(std::string path="", int width=0, int height=0, int channels=3);
  float imageDiff(std::string path1, std::string path2="", int downsample=4);
  void queueCommands(std::string cmds);

};

