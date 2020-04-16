/*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
* LavaVu python interface
**~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
%module LavaVuPython

%{
#define SWIG_FILE_WITH_INIT
%}

%include <std_string.i>
%include <std_vector.i>
%include <numpy.i>
%include <std_shared_ptr.i>

%init %{
  import_array();
%}

%{
#include "version.h"
#include "LavaVu.h"
#include "ViewerTypes.h"
#include "Model.h"
#include "DrawingObject.h"
#include "ColourMap.h"
%}

%include "exception.i"
%exception {
  try {
    $action
  } catch (const std::runtime_error& e) {
    SWIG_exception(SWIG_RuntimeError, e.what());
  }
}

%shared_ptr(GeomData)

namespace std 
{
%template(Line)  vector <float>;
%template(Array) vector <vector <float> >;
%template(List) vector <string>;
%template(GeomList) vector < std::shared_ptr<GeomData>>;
%template(ByteArray) vector <unsigned char>;
}

/* Accept numpy arrays as inputs to C++ functions */
%apply (unsigned char* IN_ARRAY1, int DIM1) {(unsigned char* array, int len)};
%apply (unsigned int* IN_ARRAY1, int DIM1) {(unsigned int* array, int len)};
%apply (float* IN_ARRAY1, int DIM1) {(float* array, int len)};
/* Return numpy array views into memory owned and managed by C++ */
%apply (float** ARGOUTVIEW_ARRAY1, int* DIM1) {(float** array, int* len)};
%apply (unsigned char** ARGOUTVIEW_ARRAY1, int* DIM1) {(unsigned char** array, int* len)};
%apply (unsigned int** ARGOUTVIEW_ARRAY1, int* DIM1) {(unsigned int** array, int* len)};
/* Return numpy array views of memory allocated in C++ but will be released/managed in python (for imageFromFile) */
%apply (unsigned char** ARGOUTVIEWM_ARRAY3, int* DIM1, int* DIM2, int* DIM3) {(unsigned char** array, int* height, int* width, int* depth)};
/* Accept numpy array and allow modification in place in C++ */
%apply (unsigned char* INPLACE_ARRAY3, int DIM1, int DIM2, int DIM3) {(unsigned char* array, int height, int width, int depth)};

%include "ViewerTypes.h"
%include "Colours.h"

const std::string version;

class OpenGLViewer
{
public:
  bool quitProgram;
  bool isopen;
  bool postdisplay; //Flag to request a frame when animating
  bool nodisplay; //Flag to manage render loop externally
  bool visible;
  int width, height;
  int timer_animate = 50;

  virtual void open(int width=0, int height=0);
  virtual void init();
  virtual void display(bool redraw=true);

  virtual void show();
  virtual void hide();
  virtual void title(std::string title) {}
  virtual void execute();
  bool events();
  void loop(bool interactive=true);
  void downSample(int q);
  void multiSample(int q);
  void animateTimer(int msec=-1);
};

class DrawingObject
{
public:
  DrawingObject(Session& session, std::string name="", std::string props="", unsigned int id=0);
  ColourMap* colourMap;
  ColourMap* opacityMap;
  ColourMap* getColourMap(const std::string propname="colourmap", ColourMap* current=NULL);
  std::string name();
  float opacity;
  Colour colour;
};

class Model
{
public:
  std::vector<std::string> fignames;
  std::vector<std::string> figures;
  int figure;
  Model(Session& session);
};

class ColourMap
{
public:
  std::string name;
  ColourMap(Session& session, std::string name="", std::string props="");
  void flip();
  void monochrome();
  static std::vector<std::string> getDefaultMapNames();
  static std::string getDefaultMap(std::string);
};

typedef std::shared_ptr<GeomData> Geom_Ptr;

class GeomData
{
public:
  unsigned int width;
  unsigned int height;
  unsigned int depth;
  int step; //Timestep
  lucGeometryType type;   //Holds the object type
  GeomData(DrawingObject* draw, lucGeometryType type);
};

class LavaVu
{
public:
  OpenGLViewer* viewer;
  Model* amodel;
  View* aview;
  DrawingObject* aobject;
  std::string binpath;

  LavaVu(std::string binpath, bool havecontext=false, bool omegalib=false);
  ~LavaVu();
  void destroy();
  virtual void resize(int new_width, int new_height);

  void run(std::vector<std::string> args={});

  void printall(const std::string& str);
  bool loadFile(const std::string& file);
  bool parseProperty(std::string data, DrawingObject* obj=NULL);
  bool parseCommands(std::string cmd);
  std::string gl_version();
  std::string image(std::string filename="", int width=0, int height=0, int jpegquality=0, bool transparent=false);
  std::string web(bool tofile=false);
  std::string video(std::string filename, int fps=30, int width=0, int height=0, int start=0, int end=0, int quality=1);
  std::string encodeVideo(std::string filename="", int fps=30, int quality=1, int width=0, int height=0);
  void defaultModel();
  ColourMap* addColourMap(std::string name, std::string colours="", std::string properties="");
  void updateColourMap(ColourMap* colourMap, std::string colours, std::string properties="");
  ColourMap* getColourMap(unsigned int id);
  ColourMap* getColourMap(std::string name);
  void setColourMap(ColourMap* target, std::string properties);
  DrawingObject* colourBar(DrawingObject* obj=NULL);
  void setState(std::string state);
  std::string getState();
  std::string getTimeSteps();
  void addTimeStep(int step, std::string properties="");

  void resetViews(bool autozoom=false);

  void setObject(DrawingObject* target, std::string properties);
  DrawingObject* createObject(std::string properties);
  DrawingObject* getObject(const std::string& name);
  DrawingObject* getObject(int id=-1);
  void reloadObject(DrawingObject* target);
  void appendToObject(DrawingObject* target);

  void loadTriangles(DrawingObject* target, std::vector< std::vector <float> > array, int split=0);
  void loadColours(DrawingObject* target, std::vector <std::string> list);
  void loadLabels(DrawingObject* target, std::vector <std::string> labels);

  void clearAll(bool objects=false, bool colourmaps=false);
  void clearObject(DrawingObject* target);
  void clearValues(DrawingObject* target, std::string label="");
  void clearData(DrawingObject* target, lucGeometryDataType type);

  std::string getObjectDataLabels(DrawingObject* target);
  void populateData(DrawingObject* target, std::string label);

  Geom_Ptr arrayUChar(DrawingObject* target, unsigned char* array, int len, lucGeometryDataType type=lucRGBData, int width=0, int height=0, int depth=0);
  Geom_Ptr arrayUInt(DrawingObject* target, unsigned int* array, int len, lucGeometryDataType type=lucRGBAData, int width=0, int height=0, int depth=0);
  Geom_Ptr arrayFloat(DrawingObject* target, float* array, int len, lucGeometryDataType type=lucVertexData, int width=0, int height=0, int depth=0);
  Geom_Ptr arrayFloat(DrawingObject* target, float* array, int len, std::string label, int width=0, int height=0, int depth=0);
  void clearTexture(DrawingObject* target);
  void setTexture(DrawingObject* target, std::string texpath);
  void textureUChar(DrawingObject* target, unsigned char* array, int len, unsigned int width, unsigned int height, unsigned int channels, bool flip=true, int filter=2, bool bgr=false);
  void textureUInt(DrawingObject* target, unsigned int* array, int len, unsigned int width, unsigned int height, unsigned int channels, bool flip=true, int filter=2, bool bgr=false);

  std::vector<Geom_Ptr> getGeometry(DrawingObject* target);
  std::vector<Geom_Ptr> getGeometryAt(DrawingObject* target, int timestep=-2);
  std::vector<float> getBoundingBox(DrawingObject* target, bool allsteps=false);
  void geometryArrayUChar(Geom_Ptr geom, unsigned char* array, int len, lucGeometryDataType type);
  void geometryArrayUInt(Geom_Ptr geom, unsigned int* array, int len, lucGeometryDataType type);
  void geometryArrayFloat(Geom_Ptr geom, float* array, int len, lucGeometryDataType type);
  void geometryArrayFloat(Geom_Ptr geom, float* array, int len, std::string label);
  void colourArrayFloat(std::string colour, float* array, int len);

  void geometryArrayViewFloat(Geom_Ptr geom, lucGeometryDataType dtype, float** array, int* len);
  void geometryArrayViewFloat(Geom_Ptr geom, float** array, int* len, std::string label);
  void geometryArrayViewUInt(Geom_Ptr geom, lucGeometryDataType dtype, unsigned int** array, int* len);
  void geometryArrayViewUChar(Geom_Ptr geom, lucGeometryDataType dtype, unsigned char** array, int* len);

  void imageBuffer(unsigned char* array, int height, int width, int depth);
  void imageFromFile(std::string filename, unsigned char** array, int* height, int* width, int* depth);
  std::vector<unsigned char> imageJPEG(int width, int height, int quality=95);
  std::vector<unsigned char> imagePNG(int width, int height, int depth);

  DrawingObject* contour(DrawingObject* target, DrawingObject* source, std::string properties, bool labels=false, bool clearsurf=false);
  DrawingObject* isoSurface(DrawingObject* target, DrawingObject* source, std::string properties, bool clearvol=false);
  void update(DrawingObject* target, bool compress=true);
  void update(DrawingObject* target, lucGeometryType type, bool compress=true);

  void close();
  std::vector<float> imageArray(std::string path="", int width=0, int height=0, int channels=3);
  float imageDiff(std::string path1, std::string path2="", int downsample=4);
  void queueCommands(std::string cmds);

  std::string helpCommand(std::string cmd="", bool heading=true);
  std::vector<std::string> commandList(std::string category="");
  std::string propertyList();
};

std::string rawImageWrite(unsigned char* array, int height, int width, int depth, std::string path, int jpegquality=0);

class VideoEncoder
{
public:
  std::string filename = "";
  VideoEncoder(const std::string& fn, int fps, int quality=3);
  ~VideoEncoder();

  //OutputInterface
  virtual void open(unsigned int w, unsigned int h);
  virtual void close();
  virtual void resize(unsigned int new_width, unsigned int new_height);
  virtual void display();

  //Custom frame write
  void copyframe(unsigned char* array, int len);
};

