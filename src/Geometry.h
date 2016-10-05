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

#include "DrawState.h"
#include "Util.h"
#include "GraphicsUtil.h"
#include "DrawingObject.h"
#include "ColourMap.h"
#include "View.h"
#include "ViewerTypes.h"
#include "Shaders.h"
#include "TimeStep.h"
#include "base64.h"

#ifndef Geometry__
#define Geometry__

#define SORT_DIST_MAX 65535

typedef struct
{
  unsigned int dataIdx;
  float minimum;
  float maximum;
  bool map;
  bool out;
  bool inclusive;
} Filter;

//Types based on triangle renderer
#define TriangleBased(type) (type == lucShapeType || type == lucVectorType || type == lucTracerType)

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
  GLuint index[3]; //global indices
  float* vertex; //Pointer to vertex to calc distance from (usually centroid)
} TIndex;

//Geometry object data store
#define MAX_DATA_ARRAYS 64
class GeomData
{
public:
  static std::string names[lucMaxType];
  DrawingObject* draw; //Parent drawing object
  unsigned int count;  //Number of vertices
  unsigned int width;
  unsigned int height;
  unsigned int depth;
  char* labelptr;
  bool opaque;   //Flag for opaque geometry, render first, don't depth sort
  int texIdx;    //Texture index to use
  unsigned int fixedOffset; //Offset to end of fixed value data
  std::vector<Filter> filterCache;

  float distance;

  //Bounding box of content
  float min[3];
  float max[3];

  std::vector<std::string> labels;      //Optional vertex labels

  //Geometry data
  Coord3DValues vertices;
  Coord3DValues vectors;
  Coord3DValues normals;
  UIntValues indices;
  UIntValues colours;
  Coord2DValues texCoords;
  UCharValues luminance;

  std::vector<DataContainer*> data;
  std::vector<FloatValues*> values;

  GeomData(DrawingObject* draw) : draw(draw), count(0), width(0), height(0), depth(0), labelptr(NULL), opaque(false)
  {
    //opaque = false; //true; //Always true for now (need to check colourmap, opacity and global opacity)
    data.resize(MAX_DATA_ARRAYS); //Maximum increased to allow predefined data plus generic value data arrays
    data[lucVertexData] = &vertices;
    data[lucVectorData] = &vectors;
    data[lucNormalData] = &normals;
    data[lucIndexData] = &indices;
    data[lucRGBAData] = &colours;
    data[lucTexCoordData] = &texCoords;
    data[lucLuminanceData] = &luminance;

    texIdx = -1;

    for (int i=0; i<3; i++)
    {
      min[i] = HUGE_VAL;
      max[i] = -HUGE_VAL;
    }

    fixedOffset = 0;
  }

  ~GeomData()
  {
    if (labelptr) free(labelptr);
    labelptr = NULL;

    //Delete value data containers (exclude fixed additions)
    for (unsigned int i=fixedOffset; i<values.size(); i++)
      delete values[i];
  }

  void checkPointMinMax(float *coord);
  void calcBounds();

  void label(std::string& labeltext);
  std::string getLabels();
  void colourCalibrate();
  void mapToColour(Colour& colour, float value);
  int colourCount();
  void getColour(Colour& colour, unsigned int idx);
  unsigned int valuesLookup(const std::string& label);
  bool filter(unsigned int idx);
  FloatValues* colourData();
  float colourData(unsigned int idx);
  FloatValues* valueData(lucGeometryDataType type);
  float valueData(lucGeometryDataType type, unsigned int idx);
};


class Distance
{
public:
  int id;
  float distance;
  Distance(int i, float d) : id(i), distance(d) {}
  bool operator<(const Distance& rhs) const
  {
    return distance < rhs.distance;
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
    return fabs(vert[0] - rhs.vert[0]) < 0.001 && fabs(vert[1] - rhs.vert[1]) < 0.001 && fabs(vert[2] - rhs.vert[2]) < 0.001;
  }
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
  friend class TimeStep; //Allow private access from TimeStep
protected:
  View* view;
  std::vector<GeomData*> geom;
  std::vector<bool> hidden;
  int elements;
  int drawcount;
  bool flat2d; //Flag for flat surfaces in 2d

public:
  DrawState& drawstate;
  //Store the actual maximum bounding box
  bool allhidden, internal, unscale;
  Vec3d iscale; //Factors for un-scaling
  lucGeometryType type;   //Holds the object type
  unsigned int total;     //Total entries of all objects in container
  bool redraw;    //Redraw flag
  bool reload;    //Reload and redraw flag

  Geometry(DrawState& drawstate);
  virtual ~Geometry();

  void clear(bool all=false); //Called before new data loaded
  void remove(DrawingObject* draw);
  virtual void close(); //Called on quit & before gl context recreated

  void compareMinMax(float* min, float* max);
  void dump(std::ostream& csv, DrawingObject* draw=NULL);
  virtual void jsonWrite(DrawingObject* draw, json& obj);
  void jsonExportAll(DrawingObject* draw, json& array, bool encode=true);
  bool hide(unsigned int idx);
  void hideShowAll(bool hide);
  bool show(unsigned int idx);
  void showObj(DrawingObject* draw, bool state);
  void redrawObject(DrawingObject* draw);
  void setValueRange(DrawingObject* draw);
  bool drawable(unsigned int idx);
  virtual void init(); //Called on GL init
  void setState(unsigned int i, Shader* prog=NULL);
  virtual void update();  //Implementation should create geometry here...
  virtual void draw();  //Display saved geometry
  void labels();  //Draw labels
  std::vector<GeomData*> getAllObjects(DrawingObject* draw);
  GeomData* getObjectStore(DrawingObject* draw);
  GeomData* add(DrawingObject* draw);
  GeomData* read(DrawingObject* draw, int n, lucGeometryDataType dtype, const void* data, int width=0, int height=0, int depth=0);
  GeomData* read(DrawingObject* draw, int n, lucGeometryDataType dtype, const void* data, std::string label);
  void read(GeomData* geomdata, int n, lucGeometryDataType dtype, const void* data, int width=0, int height=0, int depth=0);
  void setup(DrawingObject* draw);
  void insertFixed(Geometry* fixed);
  void label(DrawingObject* draw, const char* labels);
  void label(DrawingObject* draw, std::vector<std::string> labels);
  void print();
  json getDataLabels(DrawingObject* draw);
  int size() {return geom.size();}
  void setView(View* vp, float* min=NULL, float* max=NULL);
  void objectBounds(DrawingObject* draw, float* min, float* max);
  void move(Geometry* other);
  void toImage(unsigned int idx);
  void setTexture(DrawingObject* draw, int idx);
  void drawVector(DrawingObject *draw, float pos[3], float vector[3], float scale, float radius0, float radius1, float head_scale, int segment_count=24);
  void drawTrajectory(DrawingObject *draw, float coord0[3], float coord1[3], float radius0, float radius1, float arrowHeadSize, float scale[3], float maxLength=0.f, int segment_count=24);
  void drawCuboid(DrawingObject *draw, Vec3d& min, Vec3d& max, Quaternion& rot, bool quads=false);
  void drawCuboidAt(DrawingObject *draw, Vec3d& pos, Vec3d& dims, Quaternion& rot, bool quads=false);
  void drawSphere(DrawingObject *draw, Vec3d& centre, float radius, int segment_count=24);
  void drawEllipsoid(DrawingObject *draw, Vec3d& centre, Vec3d& radii, Quaternion& rot, int segment_count=24);

  //Return total vertex count
  unsigned int getVertexCount(DrawingObject* draw)
  {
    unsigned int count = 0;
    for (unsigned int i=0; i<geom.size(); i++)
      if (!draw || draw == geom[i]->draw) count += geom[i]->count;
    return count;
  }
  //Return vertex count of most recently used object
  unsigned int getVertexIdx(DrawingObject* draw)
  {
    GeomData* geom = getObjectStore(draw);
    if (geom) return geom->count;
    return 0;
  }
};

class TriSurfaces : public Geometry
{
  TIndex *tidx;
  TIndex *swap;
  unsigned int tricount;
  unsigned int idxcount;
  std::vector<unsigned int> counts;
  std::vector<Vec3d> centroids;
protected:
  std::vector<Distance> surf_sort;
public:
  GLuint indexvbo, vbo;

  TriSurfaces(DrawState& drawstate, bool flat2Dflag=false);
  ~TriSurfaces();
  virtual void close();
  virtual void update();
  void loadMesh();
  void loadBuffers();
  void loadList();
  void centroid(float* v1, float* v2, float* v3);
  void calcTriangleNormals(int index, std::vector<Vertex> &verts, std::vector<Vec3d> &normals);
  void calcGridNormals(int i, std::vector<Vec3d> &normals);
  void calcGridIndices(int i, std::vector<GLuint> &indices);
  void depthSort();
  virtual void render();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Lines : public Geometry
{
  TriSurfaces* tris;
  GLuint vbo;
  unsigned int linetotal;
  bool all2d, any3d;
  std::vector<unsigned int> counts;
public:
  Lines(DrawState& drawstate, bool all2Dflag=false);
  ~Lines();
  virtual void close();
  virtual void update();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Vectors : public Geometry
{
  Lines* lines;
  TriSurfaces* tris;
public:
  Vectors(DrawState& drawstate);
  ~Vectors();
  virtual void close();
  virtual void update();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Tracers : public Geometry
{
  Lines* lines;
  TriSurfaces* tris;
public:
  Tracers(DrawState& drawstate);
  ~Tracers();
  virtual void close();
  virtual void update();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class QuadSurfaces : public TriSurfaces
{
public:
  QuadSurfaces(DrawState& drawstate, bool flat2Dflag=false);
  ~QuadSurfaces();
  virtual void update();
  virtual void render();
  void calcGridIndices(int i, std::vector<GLuint> &indices, unsigned int vertoffset);
  virtual void draw();
};

class Shapes : public Geometry
{
  TriSurfaces* tris;
public:
  Shapes(DrawState& drawstate);
  ~Shapes();
  virtual void close();
  virtual void update();
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

class Points : public Geometry
{
  PIndex *pidx;
  PIndex *swap;
  unsigned int idxcount;
public:
  Points(DrawState& drawstate);
  ~Points();
  virtual void init();
  virtual void close();
  virtual void update();
  void loadVertices();
  void loadList();
  void depthSort();
  void render();
  int getPointType(int index=-1);
  virtual void draw();
  virtual void jsonWrite(DrawingObject* draw, json& obj);

  void dumpJSON();
};

class Volumes : public Geometry
{
public:
  GLuint colourTexture;
  std::map<DrawingObject*, int> slices;

  Volumes(DrawState& drawstate);
  ~Volumes();
  virtual void close();
  virtual void update();
  virtual void draw();
  void render(int i);
  GLubyte* getTiledImage(DrawingObject* draw, unsigned int index, int& iw, int& ih, int& bpp, int xtiles=16);
  void saveImage(DrawingObject* draw, int xtiles=16);
  virtual void jsonWrite(DrawingObject* draw, json& obj);
};

//Sorting util functions
int compareXYZ(const void *a, const void *b);
int comparePoint(const void *a, const void *b);
//void radix(int byte, long N, PIndex *source, PIndex *dest);
//void radixsort2(PIndex *source, long N);
//void radix_sort_byte(int byte, long N, unsigned char *source, unsigned char *dest, int size);
//void radix_sort(void *source, long N, int size, int bytes);

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

#endif //Geometry__
