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

#ifndef GraphicsUtil__
#define GraphicsUtil__
#include "Include.h"
#include "Util.h"
#include "Colours.h"
#include "RenderContext.h"
#include "GLUtils.h"

#include "stb_image_resize.h"

#define BLEND_NONE -1
#define BLEND_NORMAL 0
#define BLEND_PNG 1
#define BLEND_ADD 2
#define BLEND_PRE 3
#define BLEND_DEF 4

#define FONT_VECTOR -1
#define FONT_LINE    0

#define FONT_DEFAULT FONT_VECTOR

#define EPSILON 0.000001
#ifndef M_PI
#define M_PI 3.1415926536
#endif

#define DEG2RAD (M_PI/180.0)
#define RAD2DEG (180.0/M_PI)

#define crossProduct(a,b,c) \
   (a)[0] = (b)[1] * (c)[2] - (c)[1] * (b)[2]; \
   (a)[1] = (b)[2] * (c)[0] - (c)[2] * (b)[0]; \
   (a)[2] = (b)[0] * (c)[1] - (c)[0] * (b)[1];

#define dotProduct(v,q) \
   ((v)[0] * (q)[0] + \
   (v)[1] * (q)[1] + \
   (v)[2] * (q)[2])

#define vectorAdd(a, b, c) \
   (a)[0] = (b)[0] + (c)[0]; \
   (a)[1] = (b)[1] + (c)[1]; \
   (a)[2] = (b)[2] + (c)[2]; \
 
#define vectorSubtract(a, b, c) \
   (a)[0] = (b)[0] - (c)[0]; \
   (a)[1] = (b)[1] - (c)[1]; \
   (a)[2] = (b)[2] - (c)[2]; \
 
#define vectorMagnitude(v) sqrt(dotProduct(v,v));

//Get eye pos vector z by multiplying vertex by modelview matrix
#define eyePlaneDistance_(M,V) -(M[0][2] * V[0] + M[1][2] * V[1] + M[2][2] * V[2] + M[3][2]);

#define printRGB(v) printf("[%d,%d,%d]\n",v[0],v[1],v[2]);
#define printRGBA(v) printf("[%d,%d,%d,%d]\n",v[0],v[1],v[2],v[3]);
#define printVertex(v) printf("%9f,%9f,%9f\n",v[0],v[1],v[2]);
// Print out a matrix
#define printMatrix(mat) {              \
        int r, p;                       \
        fprintf(stderr, "--------- --------- --------- ---------\n"); \
        for (r=0; r<4; r++) {           \
            for (p=0; p<4; p++)         \
                fprintf(stderr, "[%d][%d] %9f ", p, r, mat[p][r]); \
            fprintf(stderr, "\n");               \
        } fprintf(stderr, "--------- --------- --------- ---------\n"); }


#define identityMatrix {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}

void compareCoordMinMax(float* min, float* max, float *coord);
void clearMinMax(float* min, float* max);
void getCoordRange(float* min, float* max, float* dims);

void RawImageFlip(void* image, int width, int height, int channels);

class ImageData  //Raw Image data
{
public:
  GLuint   width = 0;       // Image Width
  GLuint   height = 0;      // Image Height
  GLuint   channels = 4;    // Image Depth
  GLubyte* pixels = NULL;   // Image data
  bool allocated = false;
  bool flipped = false;

  ImageData(int w=0, int h=0, int c=4)
  {
    if (w && h && c)
      allocate(w, h, c);
  }

  ImageData(int w, int h, int c, GLubyte* data, bool allocated=false) : allocated(allocated)
  {
    //Provided data, pass allocated=true if it needs to be freed when in destructor
    width = w;
    height = h;
    channels = c;
    pixels = data;
  }

  ~ImageData()
  {
    release();
  }

  void allocate(int w, int h, int c=4)
  {
    release();
    width = w;
    height = h;
    channels = c;
    allocate();
  }

  void allocate()
  {
    if (width * height * channels <= 0) return;
    release();
    pixels = new GLubyte[size()];
    allocated = true;
  }

  void release()
  {
    if (allocated)
    {
      if (pixels)
        delete[] pixels;
      pixels = NULL;
      allocated = false;
    }
  }

  void copy(GLubyte* data)
  {
    memcpy(pixels, data, size());
  }

  void paste(GLubyte* data)
  {
    memcpy(data, pixels, size());
  }


  void outflip(bool png=false);

  void flip()
  {
    RawImageFlip(pixels, width, height, channels);
    flipped = !flipped;
  }

  void clear()
  {
    memset(pixels, 0, size());
  }

  void rgba2rgb()
  {
    if (channels != 4) return;
    GLubyte* dst = pixels;
    GLubyte* src = pixels;
    for (unsigned int i=0; i<width*height*4; i++)
    {
      memcpy(dst, src, 3);
      dst += 3;
      src += 4;
    }
    channels = 3;
  }

  unsigned int size()
  {
    return width*height*channels*sizeof(GLubyte);
  }

  std::string write(const std::string& path, int jpegquality=95);

  unsigned char* getBytes(unsigned int* outsize, int jpegquality);
  std::string getString(int jpegquality=0);
  std::string getBase64(int jpegquality=0);
  std::string getURIString(int jpegquality=0);
};

//Generic 3d vector
class Vec3d
{
public:
  float x;
  float y;
  float z;
  float* ref()
  {
    return &x;
  }

  Vec3d() : x(0), y(0), z(0) {}
  Vec3d(const Vec3d& copy) : x(copy.x), y(copy.y), z(copy.z) {}
  Vec3d(Vec3d* copy) : x(copy->x), y(copy->y), z(copy->z) {}
  Vec3d(float val) : x(val), y(val), z(val) {}
  Vec3d(float x, float y, float z) : x(x), y(y), z(z) {}
  Vec3d(float pos[3]) : x(pos[0]), y(pos[1]), z(pos[2]) {}

  void check()
  {
    if (std::isnan(x) || std::isinf(x)) x = 0;
    if (std::isnan(y) || std::isinf(y)) y = 0;
    if (std::isnan(z) || std::isinf(z)) z = 0;
  }

  float& operator[] (unsigned int i)
  {
    if (i==0) return x;
    if (i==1) return y;
    return z;
  }

  Vec3d operator-() const
  {
    return Vec3d(-x, -y, -z);
  }

  Vec3d operator+(const Vec3d& rhs) const
  {
    return Vec3d(x + rhs.x, y + rhs.y, z + rhs.z);
  }

  Vec3d& operator+=(const Vec3d& rhs)
  {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }

  Vec3d operator-(const Vec3d& rhs) const
  {
    return Vec3d(x - rhs.x, y - rhs.y, z - rhs.z);
  }

  Vec3d& operator-=(const Vec3d& rhs)
  {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }

  Vec3d& operator=(const Vec3d& rhs)
  {
    x = rhs.x;
    y = rhs.y;
    z = rhs.z;
    return *this;
  }

  Vec3d operator*(const Vec3d& rhs) const
  {
    return Vec3d(x * rhs.x, y * rhs.y, z * rhs.z);
  }

  Vec3d& operator*=(const Vec3d& rhs)
  {
    x *= rhs.x;
    y *= rhs.y;
    z *= rhs.z;
    return *this;
  }

  Vec3d operator*(const float& scalar) const
  {
    return Vec3d(x * scalar, y * scalar, z * scalar);
  }

  Vec3d& operator*=(const float& scalar)
  {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
  }

  Vec3d cross(const Vec3d& rhs)
  {
    return Vec3d(y * rhs.z - rhs.y * z, z * rhs.x - rhs.z * x, x * rhs.y - rhs.x * y);
  }

  float dot(const Vec3d& rhs) const
  {
    return x * rhs.x + y * rhs.y + z * rhs.z;
  }

  float magnitude() const
  {
    return sqrt(dot(*this));
  }

  void normalise()
  {
    float mag = magnitude();
    if (mag == 0.0) return;
    x /= mag;
    y /= mag;
    z /= mag;
  }

  //Returns angle in radians between this and another vector
  // cosine of angle between vectors = (v1 . v2) / |v1|.|v2|
  float angle(const Vec3d& other)
  {
    float result = dot(other) / (magnitude() * other.magnitude());
    if (result >= -1.0 && result <= 1.0)
      return acos(result);
    else
      return 0;
  }

  bool operator==(const Vec3d &rhs) const
  {
    return equals(rhs, 1e-8f);
  }

  bool equals(const Vec3d &rhs, float epsilon) const
  {
    //Comparison for equality
    return fabs(x - rhs.x) < epsilon && fabs(y - rhs.y) < epsilon && fabs(z - rhs.z) < epsilon;
  }

  bool operator<(const Vec3d &rhs) const
  {
    //Comparison for vertex sort
    if (x != rhs.x) return x < rhs.x;
    if (y != rhs.y) return y < rhs.y;
    return z < rhs.z;
  }

  friend std::ostream& operator<<(std::ostream& stream, const Vec3d& vec);
};

std::ostream & operator<<(std::ostream &os, const Colour& colour);
std::ostream & operator<<(std::ostream &os, const Vec3d& vec);

Vec3d vectorNormalToPlane(float pos0[3], float pos1[3], float pos2[3]);

/* Quaternion utility functions -
 * easily store rotations and apply to vectors
 * will be used for new camera functions
 * and wherever rotations need to be saved
 */
class Quaternion
{
  float matrix[16];
public:
  float x;
  float y;
  float z;
  float w;

  Quaternion()
  {
    identity();
  }
  Quaternion(const Quaternion& q) : x(q.x), y(q.y), z(q.z), w(q.w) {}
  Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

  float& operator[] (unsigned int i)
  {
    if (i==0) return x;
    if (i==1) return y;
    if (i==2) return z;
    return w;
  }

  operator bool()
  {
    return (fabs(x) > EPSILON || fabs(y) > EPSILON || fabs(z) > EPSILON);
  }

  void identity()
  {
    x = y = z = 0.0f;
    w = 1.0f;
  }

  void set(float x, float y, float z, float w)
  {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
  }

  /* Convert from Axis Angle */
  void fromAxisAngle(const Vec3d& v, float angle)
  {
    angle *= 0.5f * DEG2RAD;
    float sinAngle = sin(angle);
    Vec3d vn(v);
    vn.normalise();
    vn *= sinAngle;

    x = vn.x;
    y = vn.y;
    z = vn.z;
    w = cos(angle);
  }

  /* Convert to Axis Angle */
  void getAxisAngle(Vec3d& axis, float& angle)
  {
    float scale = sqrt(x * x + y * y + z * z);
    axis.x = x / scale;
    axis.y = y / scale;
    axis.z = z / scale;
    angle = acos(w) * 2.0f * RAD2DEG;
  }

  /* Multiplying q1 with q2 applies the rotation q2 to q1 */
  Quaternion operator*(const Quaternion& rhs) const
  {
    return Quaternion(
             w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
             w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
             w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
             w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
           );
  }

  /* We need to get the inverse of a quaternion to properly apply a quaternion-rotation to a vector
  * The conjugate of a quaternion is the same as the inverse, as long as the quaternion is unit-length */
  void conjugate()
  {
    x = -x;
    y = -y;
    z = -z;
  }

  /* Multiplying a quaternion q with a vector v applies the q-rotation to v */
  Vec3d operator*(const Vec3d &vec) const
  {
    //https://molecularmusings.wordpress.com/2013/05/24/a-faster-quaternion-vector-multiplication/
    //t = 2 * cross(q.xyz, v)
    //v' = v + q.w * t + cross(q.xyz, t)
    Vec3d q = Vec3d(x, y, z);
    Vec3d t = q.cross(vec) * 2.0f;
    return vec + (t * w) + q.cross(t);
  }

  float magnitude()
  {
    return sqrt(x * x + y * y + z * z + w * w);
  }

  /* Quaternions store scale as well as rotation, normalizing removes scaling */
  void normalise()
  {
    float length = magnitude();
    if (length > 0.0 && length != 1.0 )
    {
      float scale = (1.0f / length);
      x *= scale;
      y *= scale;
      z *= scale;
      w *= scale;
    }
  }

  /* Returns Quaternion to aim the Z-Axis along the vector v */
  void aimZAxis(Vec3d& v)
  {
    Vec3d vn(v);
    vn.normalise();

    set(vn.y, -vn.x, 0, 1.0f + vn.z);

    if (x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f )
    {
      x = 0;
      y = 1.0f;
      w = 0; /* If we can't normalize, just set it */
    }
    else
      normalise();
  }

  /* quaternion to aim vector from --> to */
  void aim(Vec3d& from, Vec3d& to)
  {
    /* get axis of rotation */
    Vec3d axis, cr(from);
    axis = cr.cross(to);
    float dot = from.dot(to);

    /* get scaled cos of angle between vectors and set initial quaternion */
    set(axis.x, axis.y, axis.z, dot);

    /* normalize to get cos theta, sin theta r */
    normalise();

    /* set up for half angle calculation */
    w += 1.0f;

    /* if vectors are opposing */
    if (w <= 0.000001f)
    {
      /* find orthogonal vector
       * take cross product with x axis */
      if (from.z*from.z > from.x*from.z)
        set(0.0f, 0.0f, from.z, -from.y);
      /* or take cross product with z axis */
      else
        set(0.0f, from.y, -from.x, 0.0f);
    }

    /* normalize again to get rotation quaternion */
    normalise();
  }

  // Convert to Matrix
  float* getMatrix()
  {
    // This calculation would be a lot more complicated for non-unit length quaternions
    // Note: expects the matrix in column-major format like expected by OpenGL
    float x2 = x + x;
    float y2 = y + y;
    float z2 = z + z;

    float xx2 = x * x2;
    float xy2 = x * y2;
    float xz2 = x * z2;
    float yy2 = y * y2;
    float yz2 = y * z2;
    float zz2 = z * z2;
    float wx2 = w * x2;
    float wy2 = w * y2;
    float wz2 = w * z2;

    matrix[0] = 1 - (yy2 + zz2);
    matrix[1] = xy2 + wz2;
    matrix[2] = xz2 - wy2;
    matrix[3] = 0;

    matrix[4] = xy2 - wz2;
    matrix[5] = 1 - (xx2 + zz2);
    matrix[6] = yz2 + wx2;
    matrix[7] = 0;

    matrix[8] = xz2 + wy2;
    matrix[9] = yz2 - wx2;
    matrix[10] = 1 - (xx2 + yy2);
    matrix[11] = 0;

    matrix[12] = 0;
    matrix[13] = 0;
    matrix[14] = 0;
    matrix[15] = 1;

    return matrix;
  }

  void apply(mat4& M)
  {
    M = linalg::mul(M, mat4(getMatrix()));
  }

  //Convert to/from Euler angles
  //https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
  void fromEuler(float X, float Y, float Z)
  {
    X = DEG2RAD*X*0.5;
    Y = DEG2RAD*Y*0.5;
    Z = DEG2RAD*Z*0.5;
    double sinx = std::sin(X);
    double siny = std::sin(Y);
    double sinz = std::sin(Z);
    double cosx = std::cos(X);
    double cosy = std::cos(Y);
    double cosz = std::cos(Z);

    w = cosz * cosx * cosy + sinz * sinx * siny;
    x = cosz * sinx * cosy - sinz * cosx * siny;
    y = cosz * cosx * siny + sinz * sinx * cosy;
    z = sinz * cosx * cosy - cosz * sinx * siny;

    normalise();
  }

  void toEuler(float& roll, float& pitch, float& yaw)
  {
    double ysqr = y*y;
    // roll (x-axis rotation)
    double t0 = + 2.0 * (w * x + y * z);
    double t1 = + 1.0 - 2.0 * (x * x + ysqr);
    roll = RAD2DEG * std::atan2(t0, t1);

    // pitch (y-axis rotation)
    double t2 = + 2.0 * (w * y - z * x);
    t2 = t2 > 1.0 ? 1.0 : t2;
    t2 = t2 < -1.0 ? -1.0 : t2;
    pitch = RAD2DEG * std::asin(t2);

    // yaw (z-axis rotation)
    double t3 = +2.0 * (w * z + x * y);
    double t4 = + 1.0 - 2.0 * (ysqr + z * z);
    yaw = RAD2DEG * std::atan2(t3, t4);
  }

  friend std::ostream& operator<<(std::ostream& stream, const Quaternion& q);
};

class FontManager
{
  GLuint l_vao = 0;
  GLuint l_vbo = 0;
  GLuint l_ibo = 0;
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ibo = 0;
  char buffer[4096];

  int font_vertex_total = 0;
  unsigned int font_offsets[97];

  int linefont_vertex_total = 0;

  float linefont_charwidths[95];
  unsigned linefont_counts[95];
  unsigned linefont_offsets[95];

public:
  RenderContext* context;

  int charset;
  float fontscale;
  float SCALE3D;
  Colour colour;

  FontManager()
  {
    clear();
  }

  ~FontManager()
  {
    clear();
  }

  void clear();
  void init(std::string& path, RenderContext* context);

  void GenerateFontCharacters(std::vector<float>& vertices, std::string fontfile);
  void GenerateLineFontCharacters(std::vector<float>& vertices);

  //3d fonts
  Colour setFont(Properties& properties, float scaling=1.0, bool print3d=false);
  void printString(const char* str);
  void printf(int x, int y, const char *fmt, ...);
  void print(int x, int y, const char *str, bool scale2d=true);
  void print3d(float x, float y, float z, const char *str);
  void print3dBillboard(float x, float y, float z, const char *str, int align=-1, float* scale=NULL);
  int printWidth(const char *string);
};

void vectorNormalise(float vector[3]);
void normalToPlane( float normal[3], float pos0[3], float pos1[3], float pos2[3]);
float triAngle(float v0[3], float v1[3], float v2[3]);

GLubyte* RawImageCrop(void* image, int width, int height, int channels, int outwidth, int outheight, int offsetx=0, int offsety=0);

//PNG utils
void write_png(std::ostream& stream, int channels, int width, int height, void* data);
void* read_png(std::istream& stream, GLuint& channels, GLuint& width, GLuint& height);

#define VOLUME_NONE 0
#define VOLUME_FLOAT 1
#define VOLUME_BYTE 2
#define VOLUME_RGB 3
#define VOLUME_RGBA 4
#define VOLUME_BYTE_COMPRESSED 5
#define VOLUME_RGB_COMPRESSED 6
#define VOLUME_RGBA_COMPRESSED 7

class TextureData  //Texture image data
{
public:
  GLuint   channels; // Image Colour Depth.
  GLuint   width;    // Image Width
  GLuint   height;   // Image Height
  GLuint   depth;    // Image Depth
  GLuint   id;       // Texture ID Used To Select A Texture
  int      unit;

  TextureData() : channels(0), width(0), height(0), depth(0), unit(0)
  {
    glGenTextures(1, &id);
  }
  ~TextureData()
  {
    glDeleteTextures(1, &id);
  }
};

class ImageLoader
{
public:
  FilePath fn;
  int filter=2; //0=nearest, 1=linear, 2=mipmap
  bool bgr = false;
  bool repeat = false;
  bool flip = true;
  TextureData* texture = NULL;
  ImageData* source = NULL;
  int type = VOLUME_NONE;
  bool loaded = false;

  ImageLoader(bool flip=true) : flip(flip) {}
  ImageLoader(const std::string& texfn, bool flip=true) : fn(texfn), flip(flip) {}

  TextureData* use();
  void load();
  void load(ImageData* image);
  void read();
  void loadPPM();
  void loadPNG();
  void loadJPEG(int reqChannels=0);
  void loadTIFF();
  int build(ImageData* image=NULL);
  void load3D(int width, int height, int depth, void* data=NULL, int voltype=VOLUME_FLOAT);
  void load3Dslice(int slice, void* data);
  bool empty() {return !texture || !texture->width;}
  void loadData(GLubyte* data, GLuint width, GLuint height, GLuint channels, bool flip=true);

  void clear()
  {
    clearTexture();
    clearSource();
  }

  void clearTexture()
  {
    if (texture) delete texture; 
    texture = NULL;
  }

  void clearSource()
  {
    if (source) delete source;
    source = NULL;
  }

  void newSource()
  {
    clearSource();
    source = new ImageData();
  }

  ~ImageLoader()
  {
    clear();
  }
};

#endif //GraphicsUtil__
