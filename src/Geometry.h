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

#include "Session.h"
#include "Util.h"
#include "GraphicsUtil.h"
#include "DrawingObject.h"
#include "ColourMap.h"
#include "View.h"
#include "ViewerTypes.h"
#include "Shaders.h"
#include "TimeStep.h"
#include "Contour.h"
#include "base64.h"

#ifndef Geometry__
#define Geometry__

//Types based on triangle renderer
#define TriangleBased(type) (type == lucTriangleType || type == lucGridType || type == lucShapeType || type == lucVectorType || type == lucTracerType)

typedef struct
{
   float value;
   float pos[3];
   float colourval;
} IVertex;

// Point indices + distance for sorting
typedef struct
{
  unsigned short distance;
  GLuint index; //global index
  float* vertex; //Pointer to vertex
} PIndex;

typedef struct
{
  unsigned short distance;
  GLuint index[2]; //global indices
  float* vertex; //Pointer to vertex to calc distance from (usually centre)
} LIndex;

typedef struct
{
  unsigned short distance;
  GLuint index[3]; //global indices
  float* vertex; //Pointer to vertex to calc distance from (usually centroid)
} TIndex;

template <typename T>
void radix(char byte, long N, T *source, T *dest)
//void radix(char byte, char size, long N, unsigned char *src, unsigned char *dst)
{
  // Radix counting sort of 1 byte, 8 bits = 256 bins
  int size = sizeof(T);
  long count[256], index[256];
  int i;
  memset(count, 0, sizeof(count)); //Clear counts
  unsigned char* src = (unsigned char*)source;
  //unsigned char* dst = (unsigned char*)dest;

  //Create histogram, count occurences of each possible byte value 0-255
  for (i=0; i<N; i++)
    count[src[i*size+byte]]++;

  //Calculate number of elements less than each value (running total through array)
  //This becomes the offset index into the sorted array
  //(eg: there are 5 smaller values so place me in 6th position = 5)
  index[0]=0;
  for (i=1; i<256; i++) index[i] = index[i-1] + count[i-1];

  //Finally, re-arrange data by index positions
  for (i=0; i<N; i++ )
  {
    int val = src[i*size + byte];  //Get value
    memcpy(&dest[index[val]], &source[i], size);
    //memcpy(&dst[index[val]*size], &src[i*size], size);
    index[val]++; //Increment index to push next element with same value forward one
  }
}

template <typename T>
void radix_sort(T *source, T* swap, long N, char bytes)
{
  assert(bytes % 2 == 0);
  //debug_print("Radix X sort: %d items %d bytes. Byte: ", N, size);
  // Sort bytes from least to most significant
  for (char x = 0; x < bytes; x += 2)
  {
    radix<T>(x, N, source, swap);
    radix<T>(x+1, N, swap, source);
    //radix_sort_byte(x, N, (unsigned char*)source, (unsigned char*)temp, size);
    //radix_sort_byte(x+1, N, (unsigned char*)temp, (unsigned char*)source, size);
  }
}

template <typename T>
void radix_sort(T *source, long N, char bytes)
{
  T* temp = (T*)malloc(N*sizeof(T));
  radix_sort(source, temp, N, bytes);
  free(temp);
}

template <class T>
class SortData
{
  public:
    T* buffer = NULL;
    T* swap = NULL;
    unsigned int size = 0;
    unsigned int order = 1; //Points=1, Tris=3
    std::vector<unsigned int> indices;
    bool changed;

    SortData() {}
    ~SortData() {clear();}

    void clear()
    {
      changed = true;
      if (buffer) delete[] buffer;
      if (swap) delete[] swap;
      buffer = swap = NULL;
      size = 0;
      indices.clear();
    }

    void allocate(unsigned int newsize, unsigned int order=1)
    {
      if (newsize == size) return;
      clear();
      size = newsize;
      this->order = order;
      buffer = new T[newsize];
      swap = new T[newsize];
      indices.resize(newsize*order);
      if (buffer == NULL || swap == NULL)
        abort_program("Memory allocation error (failed to allocate %d bytes)", sizeof(T) * size * 2);
      changed = true;
    }

    void sort(unsigned int N)
    {
      if (N > size) abort_program("Sort count out of range");
      //Sort by each byte of 2 byte index
      radix<T>(0, N, buffer, swap);
      radix<T>(1, N, swap, buffer);
    }
};

//All the fixed data types for rendering
class RenderData
{
 public:
  Coord3DValues& vertices;
  Coord3DValues& vectors;
  Coord3DValues& normals;
  UIntValues& indices;
  UIntValues& colours;
  Coord2DValues& texCoords;
  UCharValues& luminance;
  UCharValues& rgb;

  RenderData(Coord3DValues& vertices, Coord3DValues& vectors, Coord3DValues& normals, UIntValues& indices, UIntValues& colours, Coord2DValues& texCoords, UCharValues& luminance, UCharValues& rgb)
   : vertices(vertices), vectors(vectors), normals(normals), indices(indices), colours(colours), texCoords(texCoords), luminance(luminance), rgb(rgb) {}

};

//Shared pointer so we can pass these around without issues
typedef std::shared_ptr<RenderData> Render_Ptr;
typedef std::shared_ptr<DataContainer> Data_Ptr;
typedef std::shared_ptr<Coord3DValues> Float3_Ptr;
typedef std::shared_ptr<Coord2DValues> Float2_Ptr;
typedef std::shared_ptr<UIntValues> UInt_Ptr;
typedef std::shared_ptr<UCharValues> UChar_Ptr;

//Colour lookup functors
class ColourLookup
{
public:
  DrawingObject* draw;
  Render_Ptr render;
  FloatValues* vals;
  FloatValues* ovals;
  float div255;

  ColourLookup() {}

  void init(DrawingObject* draw, Render_Ptr render, FloatValues* vals, FloatValues* ovals)
  {
    div255 = 1.0/255;
    this->draw = draw;
    this->render = render;
    this->vals = vals;
    this->ovals = ovals;
  }

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupMapped : public ColourLookup
{
 public:
  ColourLookupMapped() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupRGBA : public ColourLookup
{
 public:
  ColourLookupRGBA() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupRGB : public ColourLookup
{
 public:
  ColourLookupRGB() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupLuminance : public ColourLookup
{
 public:
  ColourLookupLuminance() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

//Same as above with mapped opacity lookup
class ColourLookupOpacityMapped : public ColourLookup
{
 public:
  ColourLookupOpacityMapped() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupMappedOpacityMapped : public ColourLookup
{
 public:
  ColourLookupMappedOpacityMapped() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupRGBAOpacityMapped : public ColourLookup
{
 public:
  ColourLookupRGBAOpacityMapped() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupRGBOpacityMapped : public ColourLookup
{
 public:
  ColourLookupRGBOpacityMapped() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

class ColourLookupLuminanceOpacityMapped : public ColourLookup
{
 public:
  ColourLookupLuminanceOpacityMapped() {}

  virtual void operator()(Colour& colour, unsigned int idx) const;
};

//Geometry object data store
#define MAX_DATA_ARRAYS 64
class GeomData
{
public:
  bool internal = false;
  static std::string names[lucMaxType];
  static std::string datalabels[lucMaxDataType+1];
  DrawingObject* draw; //Parent drawing object
  unsigned int width;
  unsigned int height;
  unsigned int depth;
  bool opaque;   //Flag for opaque geometry, render first, don't depth sort
  Texture_Ptr texture;
  lucGeometryType type;   //Holds the object type
  int step = -1; //Holds the timestep
  unsigned int voffset = 0; //Vertex offset in VBO

  //Colour/Opacity lookup functors
  ColourLookup _getColour;
  ColourLookupMapped _getColourMapped;
  ColourLookupRGBA _getColourRGBA;
  ColourLookupRGB _getColourRGB;
  ColourLookupLuminance _getColourLuminance;
  ColourLookupOpacityMapped _getColourOpacityMapped;
  ColourLookupMappedOpacityMapped _getColourMappedOpacityMapped;
  ColourLookupRGBAOpacityMapped _getColourRGBAOpacityMapped;
  ColourLookupRGBOpacityMapped _getColourRGBOpacityMapped;
  ColourLookupLuminanceOpacityMapped _getColourLuminanceOpacityMapped;

  float distance; //For depth sorting

  //Bounding box of content
  float min[3] = {HUGE_VALF, HUGE_VALF, HUGE_VALF};
  float max[3] = {-HUGE_VALF, -HUGE_VALF, -HUGE_VALF};

  std::vector<std::string> labels;      //Optional vertex labels

  std::vector<Values_Ptr> values;
  Float3_Ptr _vertices, _vectors, _normals;
  UInt_Ptr _indices, _colours;
  Float2_Ptr _texCoords;
  UChar_Ptr _luminance, _rgb;

  Render_Ptr render;

  void readVertex(float* data)
  {
    //Shortcut to read single vertex and with bounding box update
    _vertices->read(1, data);
    checkPointMinMax(data);
  }

  static unsigned int byteSize(lucGeometryDataType type)
  {
    if (type == lucIndexData || type == lucRGBAData)
      return sizeof(unsigned int);
    if (type == lucLuminanceData || type == lucRGBData)
      return sizeof(unsigned char);
    return sizeof(float);
  }

  GeomData(DrawingObject* draw, lucGeometryType type, int step=-1)
    : draw(draw), width(0), height(0), depth(0), opaque(false), type(type), step(step)
  {
    texture = std::make_shared<ImageLoader>(); //Add a new empty texture container

    _vertices = std::make_shared<Coord3DValues>();
    _vectors = std::make_shared<Coord3DValues>();
    _normals = std::make_shared<Coord3DValues>();
    _indices = std::make_shared<UIntValues>();
    _colours = std::make_shared<UIntValues>();
    _texCoords = std::make_shared<Coord2DValues>();
    _luminance = std::make_shared<UCharValues>();
    _rgb = std::make_shared<UCharValues>();

    setRenderData();
  }

  ~GeomData()
  {
  }

  void init();

  unsigned int count() {return render->vertices.count();}  //Number of vertices

  void vertexColours(Colour* colour, unsigned int startcount)
  {
    //Load per vertex colours given initial count before vertices generated
    int diff = count() - startcount;
    for (int c=0; c<diff; c++)
      _colours->read1(colour->value);
  }

  void setRenderData()
  {
    //Required if any of the render data containers modified
    render = std::make_shared<RenderData>(*_vertices, *_vectors, *_normals, *_indices, *_colours, *_texCoords, *_luminance, *_rgb);
  }

  Data_Ptr dataContainer(lucGeometryDataType type)
  {
    switch (type)
    {
      case lucVertexData:
        return std::static_pointer_cast<DataContainer>(_vertices);
      case lucVectorData:
        return std::static_pointer_cast<DataContainer>(_vectors);
      case lucNormalData:
        return std::static_pointer_cast<DataContainer>(_normals);
      case lucIndexData:
        return std::static_pointer_cast<DataContainer>(_indices);
      case lucRGBAData:
        return std::static_pointer_cast<DataContainer>(_colours);
      case lucTexCoordData:
        return std::static_pointer_cast<DataContainer>(_texCoords);
      case lucLuminanceData:
        return std::static_pointer_cast<DataContainer>(_luminance);
      case lucRGBData:
        return std::static_pointer_cast<DataContainer>(_rgb);
      default:
        return nullptr;
    }
  }

  //Find labelled value store
  Values_Ptr valueContainer(const std::string& label)
  {
    Values_Ptr store = nullptr;
    for (auto vals : values)
    {
      if (vals->label == label)
        store = vals;
    }
    return store;
  }

  Coord3DValues& vertices()   {return *_vertices;}
  Coord3DValues& vectors()    {return *_vectors;}
  Coord3DValues& normals()    {return *_normals;}
  UIntValues& indices()       {return *_indices;}
  UIntValues& colours()       {return *_colours;}
  Coord2DValues& texCoords()  {return *_texCoords;}
  UCharValues& luminance()    {return *_luminance;}
  UCharValues& rgb()          {return *_rgb;}

  void checkPointMinMax(float *coord);
  void calcBounds();

  void label(const std::string& labeltext);
  std::string getLabels();
  ColourLookup& colourCalibrate();
  int colourCount();
  unsigned int valuesLookup(const json& by);
  bool filter(unsigned int idx);
  FloatValues* colourData();
  float colourData(unsigned int idx);
  FloatValues* valueData(unsigned int vidx);
  float valueData(unsigned int vidx, unsigned int idx);

  unsigned int gridElements2d()
  {
    //Return number of quad elements in a 2d grid
    //with underflow prevention check
    if (width == 0 || height == 0) return 0;
    return (width-1) * (height-1);
  }

  unsigned int elementCount()
  {
    return _indices->size() ? _indices->size() : count();
  }

  bool opaqueCheck(); //Return true if object does not require transparency

  bool hasTexture()
  {
    if (texture->loaded || texture->source || !texture->fn.empty()) return true;
    if (draw->texture) return true;
    if (draw->properties.has("texture"))
    {
      std::string tex = draw->properties["texture"];
      return tex.length() > 0;
    }
    return false;
  }
};

//Shared pointer for GeomData
typedef std::shared_ptr<GeomData> Geom_Ptr;

class GeomPtrCompare
{
public:
  //For sorting in descending order by distance
  bool operator()(const Geom_Ptr& l, const Geom_Ptr& r) const
  {
    return l->distance > r->distance;
  }
};

//Class for surface vertices, sorting, collating & averaging normals
class Vertex
{
public:
  Vertex() : vert(NULL), ref(-1), vcount(1) {}

  float* vert;
  int ref;
  int id;
  int vcount;

  bool operator<(const Vertex &rhs) const
  {
    //Comparison for vertex sort
    if (vert[0] != rhs.vert[0]) return vert[0] < rhs.vert[0];
    if (vert[1] != rhs.vert[1]) return vert[1] < rhs.vert[1];
    return vert[2] < rhs.vert[2];
  }

  bool operator==(const Vertex &rhs) const
  {
    //Comparison for equality
    return fabs(vert[0] - rhs.vert[0]) < VERT_EPSILON && fabs(vert[1] - rhs.vert[1]) < VERT_EPSILON && fabs(vert[2] - rhs.vert[2]) < VERT_EPSILON;
  }

  friend std::ostream& operator<<(std::ostream& stream, const Vertex& v);

  static float VERT_EPSILON; //Minimum distance before vertices will be merged
};

//Comparison for sort by id
struct vertexIdSort
{
  bool operator()(const Vertex &a, const Vertex &b)
  {
    return (a.id < b.id);
  }
};

//Container class for a list of geometry objects
class Geometry
{
  friend class Model; //Allow private access from Model
protected:
  GLuint indexvbo, vbo, vao;
  View* view;
  std::vector<Geom_Ptr> records;
  std::vector<Geom_Ptr> geom;
  std::vector<bool> hidden;
  unsigned int elements;
  unsigned int drawcount;
  DrawingObject* cached;
  std::vector<unsigned int> counts;
  bool allVertsFixed = false;
  bool allDataFixed = false;

public:
  int timestep = -2;
  Session& session;
  std::string name = "";
  std::string renderer = "";
  GLenum primitive = GL_TRIANGLES; //Some renderers allow custom primitive

  //Maximum bounding box of all content
  float min[3] = {HUGE_VALF, HUGE_VALF, HUGE_VALF};
  float max[3] = {-HUGE_VALF, -HUGE_VALF, -HUGE_VALF};

  bool allhidden, internal;
  lucGeometryType type;   //Holds the renderer type
  lucGeometryType parentType = lucMinType;   //Holds the parent renderer type
  unsigned int total;     //Total vertices renderable of all objects in container at current step
  bool redraw;    //Redraw flag
  bool reload;    //Reload and redraw flag
  std::mutex sortmutex;
  std::mutex loadmutex;

  Geometry(Session& session);
  virtual ~Geometry();

  void clear(bool fixed=false); //Called before new data loaded
  virtual void remove(DrawingObject* draw);
  void clearValues(DrawingObject* draw=NULL, std::string label="");
  void clearData(DrawingObject* draw, lucGeometryDataType dtype);
  virtual void close(); //Called on quit & before gl context recreated

  void compareMinMax(float* min, float* max);
  void dump(std::ostream& csv, DrawingObject* draw=NULL);
  virtual void jsonWrite(DrawingObject* draw, json& obj);
  void jsonExportAll(DrawingObject* draw, json& obj, bool encode=true);
  bool hide(unsigned int idx);
  void hideShowAll(bool hide);
  bool show(unsigned int idx);
  void showObj(DrawingObject* draw, bool state);
  void redrawObject(DrawingObject* draw, bool reload=false);
  void setValueRange(DrawingObject* draw, float* min=NULL, float* max=NULL);
  bool drawable(unsigned int idx);
  virtual void init(); //Called on GL init
  void merge(int start=-2, int end=-2);
  Shader_Ptr getShader(DrawingObject* draw=NULL);
  Shader_Ptr getShader(lucGeometryType type);
  void setState(unsigned int i);
  void setState(Geom_Ptr g);
  void convertColours(int step=-1, DrawingObject* obj=NULL);
  void colourMapTexture(DrawingObject* obj=NULL);
  void updateBoundingBox();
  virtual void display(bool refresh=false); //Display saved geometry
  virtual void update();  //Implementation should create geometry here...
  virtual void draw();    //Implementation should draw geometry here...
  virtual void sort();    //Threaded sort function
  void labels();  //Draw labels
  std::vector<Geom_Ptr> getAllObjects(DrawingObject* draw);
  std::vector<Geom_Ptr> getAllObjectsAt(DrawingObject* draw, int step);
  Geom_Ptr getObjectStore(DrawingObject* draw, bool stepfilter=true);
  Geom_Ptr add(DrawingObject* draw);
  Geom_Ptr read(DrawingObject* draw, unsigned int n, lucGeometryDataType dtype, const void* data, int width=0, int height=0, int depth=0);
  void read(Geom_Ptr geomdata, unsigned int n, lucGeometryDataType dtype, const void* data, int width=0, int height=0, int depth=0);
  Geom_Ptr read(DrawingObject* draw, unsigned int n, const void* data, std::string label, int width=0, int height=0, int depth=0);
  Geom_Ptr read(Geom_Ptr geom, unsigned int n, const void* data, std::string label, int width=0, int height=0, int depth=0);
  void addTriangle(DrawingObject* obj, float* a, float* b, float* c, int level, bool texCoords=false, float trilimit=0, float* normal=NULL);
  void scanDataRange(DrawingObject* draw);
  void setupObject(DrawingObject* draw);
  void label(DrawingObject* draw, const char* labels);
  void label(DrawingObject* draw, std::vector<std::string> labels);
  void print(std::ostream& os);
  json getDataLabels(DrawingObject* draw);
  int size() {return records.size();}
  virtual void setup(View* vp, float* min=NULL, float* max=NULL);
  void clearBounds(DrawingObject* draw=NULL, bool allsteps=false);
  void objectBounds(DrawingObject* draw, float* min, float* max, bool allsteps=false);
  void move(Geometry* other);
  void toImage(unsigned int idx);
  void setTexture(DrawingObject* draw, Texture_Ptr tex);
  void clearTexture(DrawingObject* draw);
  void loadTexture(DrawingObject* draw, GLubyte* data, GLuint width, GLuint height, GLuint channels, bool flip=true, int filter=2, bool bgr=false);
  Quaternion vectorRotation(Vec3d rvector);
  void drawVector(DrawingObject *draw, const Vec3d& translate, const Vec3d& vector, bool scale3d, float scale, float radius0, float radius1, float head_scale, int segment_count=24, Colour* colour=NULL);
  void drawTrajectory(DrawingObject *draw, float coord0[3], float coord1[3], float radius0, float radius1, float arrowHeadSize, float scale[3], float maxLength=0.f, int segment_count=24, Colour* colour=NULL);
  void drawCuboid(DrawingObject *draw, Vec3d& min, Vec3d& max, Quaternion& rot, bool scale3d=false, Colour* colour=NULL);
  void drawCuboidAt(DrawingObject *draw, Vec3d& pos, Vec3d& dims, Quaternion& rot, bool scale3d=false, Colour* colour=NULL);
  void drawSphere(DrawingObject *draw, Vec3d& centre, bool scale3d=false, float radius=1.0f, bool texCoords=false, int segment_count=24, Colour* colour=NULL);
  void drawEllipsoid(DrawingObject *draw, Vec3d& centre, Vec3d& radii, Quaternion& rot, bool scale3d=false, bool texCoords=false, int segment_count=24, Colour* colour=NULL);

  //Return total vertex count
  unsigned int getVertexCount(DrawingObject* draw)
  {
    unsigned int count = 0;
    for (unsigned int i=0; i<records.size(); i++)
      if (!draw || draw == records[i]->draw) count += records[i]->count();
    return count;
  }

  void expandHidden()
  {
    //Ensure enough hidden flag entries
    while (hidden.size() < geom.size())
      hidden.push_back(allhidden);
  }

  void contour(Geometry* lines, DrawingObject* source, DrawingObject* target,  bool clearsurf);
};

class Triangles : public Geometry
{
protected:
  unsigned int tricount;
public:
  Triangles(Session& session);
  virtual ~Triangles();
  virtual void close();
  unsigned int triCount(unsigned int index);
  unsigned int triCount();
  virtual void update();
  virtual void loadMesh() {}
  void loadBuffers();
  void calcTriangleNormals(unsigned int index);
  void calcGridNormals(unsigned int i);
  void calcGridIndices(unsigned int i);
  void calcGridVertices(unsigned int i);
  virtual void render();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Mesh : public Triangles
{
public:
  Mesh(Session& session) : Triangles(session) {}
};

class TriSurfaces : public Triangles
{
  SortData<TIndex> sorter;
  std::vector<Vec3d> centroids;
public:
  TriSurfaces(Session& session);
  virtual ~TriSurfaces();
  virtual void close();
  virtual void update();
  virtual void loadMesh();
  void smoothMesh(int index, std::vector<Vertex> &verts, std::vector<Vec3d> &normals, bool optimise=true);
  void calcCentroids();
  virtual void sort();    //Threaded sort function
  void loadList();
  virtual void render();
  virtual void draw();
};

class Lines : public Geometry
{
protected:
  unsigned int idxcount;
public:
  Lines(Session& session);
  virtual ~Lines();
  virtual void close();
  unsigned int lineCount();
  virtual void update();
  void loadBuffers();
  virtual void render();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class LinesSorted : public Lines
{
  SortData<TIndex> sorter;
  unsigned int linecount;
  std::vector<Vec3d> centres;
public:
  LinesSorted(Session& session);
  virtual ~LinesSorted();
  virtual void close();
  virtual void update();
  void loadLines();
  virtual void sort();    //Threaded sort function
  void loadList();
  virtual void render();
  virtual void draw();
};

class Points : public Geometry
{
  SortData<PIndex> sorter;
  bool anyHasTexture = false;
public:
  Points(Session& session);
  virtual ~Points();
  virtual void close();
  virtual void update();
  void loadVertices();
  void loadList();
  virtual void sort();    //Threaded sort function
  void render();
  int getPointType(int index=-1);
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);

  void dumpJSON();
};

class Glyphs : public Geometry
{
  friend class Model; //Allow private access from Model
protected:
  //Sub-renderers
  Lines* lines;
  Triangles* tris;
  Points* points;
public:
  Glyphs(Session& session);
  virtual ~Glyphs();
  virtual void close();
  virtual void remove(DrawingObject* draw);
  virtual void setup(View* vp, float* min=NULL, float* max=NULL);
  virtual void display(bool refresh=false);
  virtual void sort();    //Threaded sort function
  virtual void update();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Links : public Glyphs
{
  bool any3d;
public:
  Links(Session& session);
  ~Links() {close();}
  virtual void update();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Vectors : public Glyphs
{
public:
  Vectors(Session& session);
  virtual void update();
};

class Tracers : public Glyphs
{
public:
  Tracers(Session& session);
  virtual void update();
};

class Shapes : public Glyphs
{
protected:
  int defaultshape = 0;
public:
  Shapes(Session& session);
  virtual void update();
};

class QuadSurfaces : public Triangles
{
  std::vector<Geom_Ptr> sorted;
public:
  QuadSurfaces(Session& session);
  virtual ~QuadSurfaces();
  virtual void update();
  virtual void sort();    //Threaded sort function
  virtual void draw();
};

class Spheres : public Shapes
{
  //Render points as spheres
public:
  Spheres(Session& session) : Shapes(session) {defaultshape = 0;}
};

class Cuboids : public Shapes
{
  //Render points as cuboids
public:
  Cuboids(Session& session) : Shapes(session) {defaultshape = 1;}
};

class Imposter : public Geometry
{
public:
  Imposter(Session& session);
  virtual ~Imposter();
  virtual void close();
  virtual void update();
  virtual void draw();
};

class FullScreen : public Imposter
{
public:
  FullScreen(Session& session) : Imposter(session) {}
  virtual void draw();
};

class Volumes : public Imposter
{
  std::vector<Geom_Ptr> sorted;
public:
  GLuint colourTexture;
  std::map<DrawingObject*, unsigned int> slices;
  void countSlices();

  Volumes(Session& session);
  ~Volumes();
  virtual void close();
  virtual void setup(View* vp, float* min=NULL, float* max=NULL);
  virtual void update();
  virtual void sort();    //Threaded sort function
  virtual void draw();
  void render(Geom_Ptr g);
  ImageData* getTiledImage(DrawingObject* draw, unsigned int index, unsigned int& iw, unsigned int& ih, unsigned int& channels, int xtiles=16);
  void saveTiledImage(DrawingObject* draw, int xtiles=16);
  void getSliceImage(ImageData* image, GeomData* slice, int offset=0);
  void saveSliceImages(DrawingObject* draw, unsigned int index);
  virtual void jsonWrite(DrawingObject* draw, json& obj);
  void isosurface(Triangles* surfaces, DrawingObject* source, DrawingObject* target, bool clearvol=false);
};

//Sorting util functions
int compareXYZ(const void *a, const void *b);
int comparePoint(const void *a, const void *b);

Geometry* createRenderer(Session& session, const std::string& what);

#endif //Geometry__
