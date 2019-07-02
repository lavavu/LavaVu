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
#if defined HAVE_LIBPNG
#include <png.h>
#include <zlib.h>
#else
#include "png/lodepng.h"
#endif

#include "GraphicsUtil.h"
#include "base64.h"
#include <string.h>
#include <math.h>

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif

#ifdef USE_FONTS
#include  "font.h"
#include  "FontSans.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

void FontManager::init(std::string& path, RenderContext* context)
{
  this->context = context;
  // Load fonts
  std::vector<float> vertices;
  GenerateFontCharacters(vertices, path + "font.bin");
  //Initialise vertex array object for OpenGL 3.2+
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  //Initialise vertex buffer
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;
  //std::cout << vertices.size() * sizeof(GLfloat) << " bytes, " << vertices.size() << " float buffer loaded for vector font\n";

  rasterSetupFonts();
}

Colour FontManager::setFont(Properties& properties, std::string def, float scaling, float multiplier2d)
{
  //fixed, small, sans, serif, vector
  std::string fonttype = def;
  if (properties["vectorfont"])
    //Always use 3D vector font
    fonttype = "vector";
  if (properties.has("font"))
    fonttype = properties["font"];

  fontscale = properties.getFloat("fontscale", scaling);

  //Colour
  colour = Colour(properties["fontcolour"]);

  //Bitmap fonts
  if (fonttype == "fixed")
    charset = FONT_FIXED;
  else if (fonttype == "sans")
    charset = FONT_NORMAL;
  else if (fonttype == "serif")
    charset = FONT_SERIF;
  else if (fonttype == "vector")
    charset = FONT_VECTOR;
  else  //Default (small)
    charset = FONT_SMALL;

  //For non-vector fonts
  if (charset > FONT_VECTOR)
    fontscale *= multiplier2d;

  return colour;
}

void FontManager::printString(const char* str)
{
  glDisable(GL_CULL_FACE);

  //Render the characters in loop
  //1) Create index buffer data for each char
  //2) Load and render index buffer
  //3) Translate by char width
  if (!ibo) glGenBuffers(1, &ibo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  context->fontshader->use();
  GLint aPosition = context->fontshader->attribs["aVertexPosition"];
  GLint aTexCoord = context->fontshader->attribs["aVertexTexCoord"];
  //Send the matrices as uniform data
  context->fontshader->setUniformMatrixf("uMVMatrix", context->MV);
  context->fontshader->setUniformMatrixf("uPMatrix", context->P);
  context->fontshader->setUniform("uColour", colour);
  context->fontshader->setUniformi("uTextured", 0);

  int stride = 2 * sizeof(GLfloat);
  GL_Error_Check;
  glEnableVertexAttribArray(aPosition);
  glDisableVertexAttribArray(aTexCoord);
  glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0); // Vertex x,y
  GL_Error_Check;

  for (unsigned int c=0; c<strlen(str); c++)
  {
    //Render character
    int i = (int)str[c] - 32;
    std::vector<unsigned int> indices;
    unsigned int offset = font_offsets[i]; //Vertex offsets of 2d vertices
    //Load the tris
    for (unsigned int t=0; t<font_tricounts[i]; t++)
    {
      assert(offset + t*3+2 < (unsigned int)font_vertex_total);
      //Tri: 3 * vertex indices
      indices.push_back(offset + t*3);
      indices.push_back(offset + t*3 + 1);
      indices.push_back(offset + t*3 + 2);
    }

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_DYNAMIC_DRAW);
    GL_Error_Check;
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, (GLvoid*)0);
    GL_Error_Check;
    
    // Shift right width of character
    context->translate3(font_charwidths[i], 0, 0);
    context->fontshader->setUniformMatrixf("uMVMatrix", context->MV);
  }

  GL_Error_Check;
  glDisableVertexAttribArray(aPosition);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;
}

void FontManager::printf(int x, int y, const char *fmt, ...)
{
  GET_VAR_ARGS(fmt, buffer);
  print(x, y, buffer);   // FontManager::print result string
}

void FontManager::print(int x, int y, const char *str, bool scale2d)
{
  if (charset > FONT_VECTOR) return rasterPrint(x, y, str);
  context->MV = linalg::identity;
  if (scale2d)
    context->scale3(context->scale2d, context->scale2d, context->scale2d);
  context->translate3(x, y, 0);
  context->scale3(fontscale, fontscale, 1.0);
  printString(str);
}

void FontManager::print3d(float x, float y, float z, const char *str)
{
  if (charset > FONT_VECTOR) return rasterPrint3d(x, y, z, str);
  context->translate3(x, y, z);
  context->scale3(fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D);
  printString(str);
}

void FontManager::print3dBillboard(float x, float y, float z, const char *str, int align, float* scale)
{
  if (charset > FONT_VECTOR) return rasterPrint3d(x, y, z, str, align > -1);
  int i,j;
  float scaledef[3] = {1.0, 1.0, 1.0};
  if (!scale) scale = scaledef;

  // save the current modelview matrix
  context->push();
  float sw = FONT_SCALE_3D * printWidth(str);
  //Default align = -1 (Left)
  //(scalex is to undo any x axis scaling on position adjustments)
  if (align == 1) x -= sw/scale[0];     //Right
  if (align == 0) x -= sw*0.5/scale[0]; //Centre
  z -= 0.025 / scale[2] * fontscale;

  context->translate3(x, y, z);

  // undo all rotations
  // beware all scaling is lost as well
  for( i=0; i<3; i++ )
    for( j=0; j<3; j++ )
    {
      if ( i==j )
        context->MV[j][i] = 1.0;
      else
        context->MV[j][i] = 0.0;
    }

  context->scale3(fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D, fontscale * FONT_SCALE_3D);
  printString(str);
  context->pop();
}

// String width calc
int FontManager::printWidth(const char *string)
{
  if (charset > FONT_VECTOR) return rasterPrintWidth(string);
  // Sum character widths in string
  int i, len = 0, slen = strlen(string);
  for (i = 0; i < slen; i++)
    len += font_charwidths[string[i]-32];

  // Additional pixel of spacing for each character
  float w = len + slen;
  return fontscale * w;
}

//Bitmap fonts
void FontManager::rasterPrintString(const char* str)
{
  assert(r_vbo);
  if (charset > FONT_SERIF || charset < FONT_FIXED) // Character set valid?
    charset = FONT_FIXED;

  // First save state of enable flags
  glDisable(GL_CULL_FACE);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glBindTexture(GL_TEXTURE_2D, fonttexture);
  if (fontscale >= 1.0) //Don't allow downscaling bitmap fonts
    context->scale3(fontscale, fontscale, fontscale);

  if (!r_ibo) glGenBuffers(1, &r_ibo);
  glBindVertexArray(r_vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_ibo);
  glBindBuffer(GL_ARRAY_BUFFER, r_vbo);

  GL_Error_Check;
  glBindTexture(GL_TEXTURE_2D, fonttexture);
  GL_Error_Check;

  context->fontshader->use();
  GLint aPosition = context->fontshader->attribs["aVertexPosition"];
  GLint aTexCoord = context->fontshader->attribs["aVertexTexCoord"];

  int stride = 4 * sizeof(GLfloat);
  glEnableVertexAttribArray(aPosition);
  glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0); // Vertex x,y
  glEnableVertexAttribArray(aTexCoord);
  glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(2*sizeof(float))); //Tex coord s,t
  GL_Error_Check;

  //Send the matrices as uniform data
  context->fontshader->setUniformMatrixf("uMVMatrix", context->MV);
  context->fontshader->setUniformMatrixf("uPMatrix", context->P);

  context->fontshader->setUniform("uColour", colour);
  context->fontshader->setUniformi("uTextured", 1);

  for (unsigned int c=0; c<strlen(str); c++)
  {
    //Render character
    unsigned int startidx = 0;
    unsigned int stopidx = 384;
    unsigned int i = (unsigned int)(str[c] - 32 + (96 * charset));      // Choose the font and charset
    assert(4 * i + 3 < stopidx * 4 * 2 * 2);
    GLuint buffer[4] = {4 * i, 4 * i + 1, 4 * i + 3, 4 * i + 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(GLuint), buffer, GL_DYNAMIC_DRAW);
    GL_Error_Check;
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, (GLvoid*)0);
    GL_Error_Check;
    
    // Shift right width of character + 1
    context->translate3(bmpfont_charwidths[startidx + i]+1, 0, 0);
    context->fontshader->setUniformMatrixf("uMVMatrix", context->MV);
    GL_Error_Check;
  }

  GL_Error_Check;

  glDisableVertexAttribArray(aPosition);
  glDisableVertexAttribArray(aTexCoord);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;

  //glDisable(GL_CULL_FACE);
}

void FontManager::rasterPrint(int x, int y, const char *str, bool scale2d)
{
  context->MV = linalg::identity;
  if (scale2d)
    context->scale3(context->scale2d, context->scale2d, context->scale2d);
  context->translate3(x, y-bmpfont_charheights[charset], 0);
  rasterPrintString(str);
}

void FontManager::rasterPrint3d(float x, float y, float z, const char *str, bool alignRight)
{
  /* Calculate projected screen coords in viewport */
  float pos[3];
  GLint viewportArray[4];
  glGetIntegerv(GL_VIEWPORT, viewportArray);
  context->project(x, y, z, pos);

  /* Switch to ortho view with 1 unit = 1 pixel and print using calculated screen coords */
  context->viewport2d(viewportArray[2], viewportArray[3]);

  GL_Error_Check;
  glDepthFunc(GL_ALWAYS);
  GL_Error_Check;

  /* FontManager::print at calculated position, compensating for viewport offset */
  int xs, ys;
  xs = (int)(pos[0]) - viewportArray[0];
  if (alignRight) xs -= rasterPrintWidth(str);
  ys = (int)(pos[1]) - viewportArray[1]; //(viewportArray[3] - (yPos - viewportArray[1]));
  rasterPrint(xs, ys, str);

  /* Restore state */
  context->viewport2d(0, 0);
  /* Put back settings */
  glDepthFunc(GL_LESS);
  GL_Error_Check;
}

/* String width calc */
int FontManager::rasterPrintWidth(const char *string)
{
  /* Sum character widths in string */
  int i, len = 0, slen = strlen(string);
  for (i = 0; i < slen; i++)
    len += bmpfont_charwidths[string[i]-32 + (96 * charset)];
  /* Additional pixel of spacing for each character */
  float w = len + slen;
  if (fontscale >= 1.0) return fontscale * w;
  return w;
}

void FontManager::rasterSetupFonts()
{
  /* Load font bitmaps and Convert To Textures */
  int i, j;
  unsigned char* pixel_data = new unsigned char[IMAGE_HEIGHT * IMAGE_WIDTH * IMAGE_BYTES_PER_PIXEL];
  unsigned char fontdata[IMAGE_HEIGHT][IMAGE_WIDTH];   /* font texture data */

  /* Get font pixels from source data - interpret RGB (greyscale) as alpha channel */
  IMAGE_RUN_LENGTH_DECODE(pixel_data, IMAGE_RLE_PIXEL_DATA, IMAGE_WIDTH * IMAGE_HEIGHT, IMAGE_BYTES_PER_PIXEL);
  for (i = 0; i < IMAGE_HEIGHT; i++)
    for (j = 0; j < IMAGE_WIDTH; j++)
      fontdata[ i ][ j ] = 255 - pixel_data[ IMAGE_BYTES_PER_PIXEL * (IMAGE_WIDTH * i + j) ];

  GL_Error_Check;
  /* create and bind texture */
  glGenTextures(1, &fonttexture);
  glBindTexture(GL_TEXTURE_2D, fonttexture);
  //glEnable(GL_COLOR_MATERIAL);
  /* use linear filtering */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  /* generate the texture from bitmap alpha data */
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, IMAGE_WIDTH, IMAGE_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, fontdata);
  GL_Error_Check;

  /* Build font data */
  rasterBuildFont(16, 16, 0, 384);      /* 16x16 glyphs, 16 columns - 4 fonts */
  delete [] pixel_data;
}

void FontManager::rasterBuildFont(int glyphsize, int columns, int startidx, int stopidx)
{
  // Build font buffers
  //GLfloat buffer[stopidx][4][2][2]; //character, 4 vertices, vertex + texcoord, 2d(x,y)
  GLfloat buffer[384][4][2][2]; //character, 4 vertices, vertex + texcoord, 2d(x,y)

  int i;
  float divX = IMAGE_WIDTH / (float)glyphsize;
  float divY = IMAGE_HEIGHT / (float)glyphsize;
  float glyphX = 1 / divX;   /* Width & height of a glyph in texture coords */
  float glyphY = 1 / divY;
  GLfloat cx = 0, cy = 0;         /* the character coordinates in our texture */
  glBindTexture(GL_TEXTURE_2D, fonttexture);
  for (i = 0; i < (stopidx - startidx); i++)
  {
    cx = (float) (i % columns) / divX;
    cy = (float) (i / columns) / divY;
    //Vertices
    buffer[i][0][0][0] = 0;
    buffer[i][0][0][1] = 0;
    buffer[i][1][0][0] = glyphsize;
    buffer[i][1][0][1] = 0;
    buffer[i][2][0][0] = glyphsize;
    buffer[i][2][0][1] = glyphsize;
    buffer[i][3][0][0] = 0;
    buffer[i][3][0][1] = glyphsize;
    //Tex coords
    buffer[i][0][1][0] = cx;
    buffer[i][0][1][1] = cy + glyphY;
    buffer[i][1][1][0] = cx + glyphX;
    buffer[i][1][1][1] = cy + glyphY;
    buffer[i][2][1][0] = cx + glyphX;
    buffer[i][2][1][1] = cy;
    buffer[i][3][1][0] = cx;
    buffer[i][3][1][1] = cy;

  }

  GL_Error_Check;
  //Initialise vertex array object for OpenGL 3.2+
  glGenVertexArrays(1, &r_vao);
  glBindVertexArray(r_vao);
  //Initialise vertex buffer
  glGenBuffers(1, &r_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, r_vbo);
  assert(glIsBuffer(r_vbo));
  int size = stopidx * 4 * 2 * 2;
  glBufferData(GL_ARRAY_BUFFER, size * sizeof(GLfloat), buffer, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  GL_Error_Check;
  //std::cout << size * sizeof(GLfloat) << " bytes, " << size << " float buffer loaded for raster font\n";
}


#else //USE_FONTS
void FontManager::init(std::string& path, RenderContext* context) {this->context = context;}
Colour FontManager::setFont(Properties& properties, std::string def, float scaling, float multiplier2d) {return Colour();}
void FontManager::printString(const char* str) {}
void FontManager::printf(int x, int y, const char *fmt, ...) {}
void FontManager::print(int x, int y, const char *str, bool scale2d) {}
void FontManager::print3d(float x, float y, float z, const char *str) {}
void FontManager::print3dBillboard(float x, float y, float z, const char *str, int align, float* scale) {}
int FontManager::printWidth(const char *string)
{
  return 0;
}
void FontManager::rasterPrintString(const char* str) {}
void FontManager::rasterPrint(int x, int y, const char *str, bool scale2d) {}
void FontManager::rasterPrint3d(float x, float y, float z, const char *str, bool alignRight) {}
int FontManager::rasterPrintWidth(const char *string)
{
  return 0;
}
void FontManager::rasterSetupFonts() {}
void FontManager::rasterBuildFont(int glyphsize, int columns, int startidx, int stopidx) {}
#endif //USE_FONTS



void compareCoordMinMax(float* min, float* max, float *coord)
{
  for (int i=0; i<3; i++)
  {
    //assert(!std::isnan(coord[i]));
    if (std::isnan(coord[i])) return;
    if (std::isinf(coord[i])) return;
    if (coord[i] > max[i] && coord[i] < HUGE_VAL)
    {
      max[i] = coord[i];
      //std::cerr << "Updated MAX: " << Vec3d(max) << std::endl;
    }
    if (coord[i] < min[i] && coord[i] > -HUGE_VAL)
    {
      min[i] = coord[i];
      //std::cerr << "Updated MIN: " << Vec3d(min) << std::endl;
    }
  }
}

void clearMinMax(float* min, float* max)
{
  for (int i=0; i<3; i++)
  {
    min[i] = HUGE_VAL;
    max[i] = -HUGE_VAL;
  }
}

void getCoordRange(float* min, float* max, float* dims)
{
  for (int i=0; i<3; i++)
  {
    dims[i] = max[i] - min[i];
  }
}

//Vector ops

// vectorNormalise calculates the magnitude of a vector
// \hat v = frac{v} / {|v|}
// This function uses function dotProduct to calculate v . v
void vectorNormalise(float vector[3])
{
  float mag;
  mag = sqrt(dotProduct(vector,vector));
  vector[2] = vector[2]/mag;
  vector[1] = vector[1]/mag;
  vector[0] = vector[0]/mag;
}

std::ostream & operator<<(std::ostream &os, const Vec3d& vec)
{
  return os << "[" << vec.x << "," << vec.y << "," << vec.z << "]";
}

std::ostream& operator<<(std::ostream& stream, const Quaternion& q)
{
  return stream << q.x << "," << q.y << "," << q.z << "," << q.w; 
}

// Given three points which define a plane, returns a vector which is normal to that plane
Vec3d vectorNormalToPlane(float pos0[3], float pos1[3], float pos2[3])
{
  Vec3d vector0 = Vec3d(pos0);
  Vec3d vector1 = Vec3d(pos1);
  Vec3d vector2 = Vec3d(pos2);

  //Clear invalid components (nan/inf)
  vector0.check();
  vector1.check();
  vector2.check();

  vector1 -= vector0;
  vector2 -= vector0;

  return vector1.cross(vector2);
}

// Given three points which define a plane, NormalToPlane will give the unit vector which is normal to that plane
// Uses vectorSubtract, crossProduct and VectorNormalise
void normalToPlane( float normal[3], float pos0[3], float pos1[3], float pos2[3])
{
  float vector1[3], vector2[3];

//printf(" PLANE: %f,%f,%f - %f,%f,%f - %f,%f,%f\n", pos0[0], pos0[1], pos0[2], pos1[0], pos1[1], pos1[2], pos2[0], pos2[1], pos2[2]);
  vectorSubtract(vector1, pos1, pos0);
  vectorSubtract(vector2, pos2, pos0);

  crossProduct(normal, vector1, vector2);
//printf(" %f,%f,%f x %f,%f,%f == %f,%f,%f\n", vector1[0], vector1[1], vector1[2], vector2[0], vector2[1], vector2[2], normal[0], normal[1], normal[2]);

  //vectorNormalise( normal);
}

// Given 3 x 3d vertices defining a triangle, calculate the inner angle at the first vertex
float triAngle(float v0[3], float v1[3], float v2[3])
{
  //Returns angle at v0 in radians for triangle defined by v0,v1,v2

  //Get lengths of each side of triangle adjacent to this vertex
  float e0[3], e1[3]; //Triangle edge vectors
  vectorSubtract(e0, v1, v0);
  vectorSubtract(e1, v2, v0);

  //Normalise to simplify dot product calc
  vectorNormalise(e0);
  vectorNormalise(e1);
  //Return triangle angle (in radians)
  return acos(dotProduct(e0,e1));
}

void RawImageFlip(void* image, int width, int height, int channels)
{
  int scanline = channels * width;
  GLubyte* ptr1 = (GLubyte*)image;
  GLubyte* ptr2 = ptr1 + scanline * (height-1);
  GLubyte* temp = new GLubyte[scanline];
  for (int y=0; y<height/2; y++)
  {
    memcpy(temp, ptr1, scanline);
    memcpy(ptr1, ptr2, scanline);
    memcpy(ptr2, temp, scanline);
    ptr1 += scanline;
    ptr2 -= scanline;
  }
  delete[] temp;
}

GLubyte* RawImageCrop(void* image, int width, int height, int channels, int outwidth, int outheight, int offsetx, int offsety)
{
  int scanline = channels * width;
  int outscanline = channels * outwidth;
  GLubyte* crop = new GLubyte[outscanline*outheight];
  GLubyte* ptr1 = (GLubyte*)image + offsety*scanline + offsetx*channels;
  GLubyte* ptr2 = crop;
  for (int y=offsety; y<offsety+outheight; y++)
  {
    memcpy(ptr2, ptr1, outscanline);
    ptr1 += scanline;
    ptr2 += outscanline;
  }
  return crop;
}

TextureData* ImageLoader::use()
{
  //If have data but texture not loaded, load it
    GL_Error_Check;
  if (empty() && source)
    build();
    GL_Error_Check;

  if (!empty())
  {
    GL_Error_Check;
    GLenum ttype = GL_TEXTURE_2D;
    if (texture->depth > 0)
      ttype = GL_TEXTURE_3D;

    GL_Error_Check;
    glActiveTexture(GL_TEXTURE0 + texture->unit);
    GL_Error_Check;
    glBindTexture(ttype, texture->id);
    GL_Error_Check;
    //printf("USE TEXTURE: (id %d unit %d)\n", texture->id, texture->unit);

    if (repeat)
    {
      glTexParameteri(ttype, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(ttype, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    else
    {
      glTexParameteri(ttype, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(ttype, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    return texture;
  }

  return NULL;
}

void ImageLoader::load()
{
  //Load texture from internal data
  loaded = true;

  //Already loaded
  if (texture) return;

  //No file, requires source data
  if (!source)
  {
    if (fn.empty()) return;

    //Load texture file
    read();
  }

  //Build texture
  build(source);
}

void ImageLoader::load(ImageData* image)
{
  //Load image from provided external data
  if (!image) abort_program("NULL image data\n");
  loaded = true;

  //Requires flip on load for OpenGL
  if (flip) image->flip();

  //Build texture
  build(image);
}

void ImageLoader::loadData(GLubyte* data, GLuint width, GLuint height, GLuint channels, bool flip)
{
  //Load new raw data
  loaded = true;
  if (texture)
    texture->width = 0; //Flag empty rather than delete, avoids OpenGL calls for use in other threads

  if (source && (source->width != width || source->height != height || source->channels != channels))
    clearSource();
  if (!source)
  {
    newSource();
    source->allocate(width, height, channels);
  }
  source->copy(data);
  if (flip) source->flip();
}

void ImageLoader::read()
{
  //Load image file
  clear();
  if (fn.type == "jpg" || fn.type == "jpeg")
    loadJPEG();
  if (fn.type == "png")
    loadPNG();
  if (fn.type == "ppm")
    loadPPM();
  if (fn.type == "tif" || fn.type == "tiff")
    loadTIFF();

  //Requires flip on load for OpenGL
  if (source && flip) source->flip();
}

// Loads a PPM image
void ImageLoader::loadPPM()
{
  bool readTag = false, readWidth = false, readHeight = false, readColourCount = false;
  char stringBuffer[241];
  int ppmType, colourCount;
  newSource();

  FILE* imageFile = fopen(fn.full.c_str(), "rb");
  if (imageFile == NULL)
  {
    clearSource();
    debug_print("Cannot open '%s'\n", fn.full.c_str());
    return;
  }

  while (!readTag || !readWidth || !readHeight || !readColourCount)
  {
    // Read in a new line from file
    char* charPtr = fgets( stringBuffer, 240, imageFile );
    assert ( charPtr );

    for (charPtr = stringBuffer ; charPtr < stringBuffer + 240 ; charPtr++ )
    {
      // Check if we should go to a new line - this will happen for comments, line breaks and terminator characters
      if ( *charPtr == '#' || *charPtr == '\n' || *charPtr == '\0' )
        break;

      // Check if this is a space - if this is the case, then go to next line
      if ( *charPtr == ' ' || *charPtr == '\t' )
        continue;

      if ( !readTag )
      {
        sscanf( charPtr, "P%d", &ppmType );
        readTag = true;
      }
      else if ( !readWidth )
      {
        sscanf( charPtr, "%u", &source->width );
        readWidth = true;
      }
      else if ( !readHeight )
      {
        sscanf( charPtr, "%u", &source->height );
        readHeight = true;
      }
      else if ( !readColourCount )
      {
        sscanf( charPtr, "%d", &colourCount );
        readColourCount = true;
      }

      // Go to next white space
      charPtr = strpbrk( charPtr, " \t" );

      // If there are no more characters in line then go to next line
      if ( charPtr == NULL )
        break;
    }
  }

  // Only allow PPM images of type P6 and with 256 colours
  if ( ppmType != 6 || colourCount != 255 ) abort_program("Unable to load PPM Texture file, incorrect format");

  source->channels = 3;
  source->allocate();

  for (unsigned int j = 0; j<source->height; j++)
    if (fread(&source->pixels[source->width * j * source->channels], source->channels, source->width, imageFile) < source->width) 
      abort_program("PPM Read Error");
  fclose(imageFile);
}

void ImageLoader::loadPNG()
{
  std::ifstream file(fn.full.c_str(), std::ios::binary);
  if (!file)
  {
    debug_print("Cannot open '%s'\n", fn.full.c_str());
    return;
  }
  newSource();
  source->pixels = (GLubyte*)read_png(file, source->channels, source->width, source->height);
  source->allocated = true; //Allocated by read_png()

  file.close();
}

void ImageLoader::loadJPEG(int req_channels)
{
  newSource();
  int width, height, channels;
  source->pixels = (GLubyte*)jpgd::decompress_jpeg_image_from_file(fn.full.c_str(), &width, &height, &channels, req_channels);

  source->width = width;
  source->height = height;
  source->channels = req_channels ? req_channels : channels;
  source->allocated = true; //Allocated by decompress_jpeg_image_from_file()
}

void ImageLoader::loadTIFF()
{
  newSource();
#ifdef HAVE_LIBTIFF
  TIFF* tif = TIFFOpen(fn.full.c_str(), "r");
  if (tif)
  {
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &source->width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &source->height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &source->channels);
    //source->channels = 4;
    source->allocate();   // Reserve Memory
    if (source->pixels)
    {
      if (TIFFReadRGBAImage(tif, source->width, source->height, (uint32*)source->pixels, 0))
      {
        //Succeeded
      }
      else
        clear();
    }
    TIFFClose(tif);
  }
#else
  abort_program("[Load Texture] Require libTIFF to load TIFF images\n");
#endif
}

int ImageLoader::build(ImageData* image)
{
  if (!image && !source) return 0;
  if (!image) image = source;
  if (!texture)
    texture = new TextureData();

  //Build texture from raw data
  glActiveTexture(GL_TEXTURE0 + texture->unit);
  glBindTexture(GL_TEXTURE_2D, texture->id);

  if (filter == 2)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }
  else if (filter == 1)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  GL_Error_Check;

  //Load the texture data based on bits per pixel
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GL_Error_Check;
  switch (image->channels)
  {
  case 1:
    //Luminance using ARB_texture_swizzle (core in 3.3)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, image->width, image->height, 0, GL_RED, GL_UNSIGNED_BYTE, image->pixels);
    //All components take the value from red, no alpha
#ifndef GL_TEXTURE_SWIZZLE_R
#define GL_TEXTURE_SWIZZLE_R  GL_TEXTURE_SWIZZLE_R_EXT
#define GL_TEXTURE_SWIZZLE_G  GL_TEXTURE_SWIZZLE_G_EXT
#define GL_TEXTURE_SWIZZLE_B  GL_TEXTURE_SWIZZLE_B_EXT
#define GL_TEXTURE_SWIZZLE_A  GL_TEXTURE_SWIZZLE_A_EXT
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    break;
  case 2:
    //Luminance+Alpha using ARB_texture_swizzle (core in 3.3)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, image->width, image->height, 0, GL_RG, GL_UNSIGNED_BYTE, image->pixels);
    //Colour components take the value from red, alpha from green
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
    break;
  case 3:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->width, image->height, 0, bgr ? GL_BGR : GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
    break;
  case 4:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 0, bgr ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    break;
  }

  //Copy metadata
  texture->width = image->width;
  texture->height = image->height;
  texture->channels = image->channels;

  if (filter == 2)
    glGenerateMipmap(GL_TEXTURE_2D);
  return 1;
}

void ImageLoader::load3D(int width, int height, int depth, void* data, int voltype)
{
  //Save the type
  type = voltype;
  GL_Error_Check;
  //Create the texture
  if (!texture) texture = new TextureData();
  GL_Error_Check;
  //Hard coded unit for 3d textures for now
  texture->unit = 1;

  glActiveTexture(GL_TEXTURE0 + texture->unit);
  GL_Error_Check;
  glBindTexture(GL_TEXTURE_3D, texture->id);
  GL_Error_Check;

  texture->width = width;
  texture->height = height;
  texture->depth = depth;

  // set the texture parameters
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  GL_Error_Check;

  //Load based on type
  debug_print("Volume Texture: width %d height %d depth %d type %d\n", width, height, depth, type);
  switch (type)
  {
  case VOLUME_FLOAT:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_RED, GL_FLOAT, data);
    break;
  case VOLUME_BYTE:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, width, height, depth, 0, GL_RED, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_BYTE_COMPRESSED:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RED, width, height, depth, 0, GL_RED, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGB:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, width, height, depth, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGB_COMPRESSED:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RGB, width, height, depth, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGBA:
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGBA_COMPRESSED:
    glTexImage3D(GL_TEXTURE_3D, 0,  GL_COMPRESSED_RGBA, width, height, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    break;
  }
  GL_Error_Check;
}

void ImageLoader::load3Dslice(int slice, void* data)
{
  GL_Error_Check;
  switch (type)
  {
  case VOLUME_FLOAT:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_RED, GL_FLOAT, data);
    break;
  case VOLUME_BYTE:
  case VOLUME_BYTE_COMPRESSED:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_RED, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGB:
  case VOLUME_RGB_COMPRESSED:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_RGB, GL_UNSIGNED_BYTE, data);
    break;
  case VOLUME_RGBA:
  case VOLUME_RGBA_COMPRESSED:
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, slice, texture->width, texture->height, 1, 
                    GL_RGBA, GL_UNSIGNED_BYTE, data);
    break;
  }
  GL_Error_Check;
}

void ImageData::outflip(bool png)
{
  //Prepare the image buffer so the Y axis is as expected by the output library
  //JPEG, LodePNG: Flip Y if sourced from OpenGL (Y origin should be at top, opposite of OpenGL)
  //LibPNG: Flip Y if NOT sourced from OpenGL (Y origin should be at bottom as OpenGL)
  if (png)
  {
#ifndef HAVE_LIBPNG
    //Flip data sourced from framebuffer for LodePNG
    if (flipped)
      flip();
#else
    //LIBPNG expects flipped Y, leave OpenGL image as is, if NOT sourced from framebuffer, flip
    if (!flipped)
      flip();
#endif
  }
  else
  {
    //Always flip data sourced from framebuffer for JPEG
    if (flipped)
      flip();
  }
}

std::string ImageData::write(const std::string& path, int jpegquality)
{
  FilePath filepath(path);
  if (filepath.type == "png")
  {
    //Write data to image file
    std::ofstream file(filepath.full, std::ios::binary);
    outflip(true);  //Y-flip as necessary
    write_png(file, channels, width, height, pixels);
  }
  else if (filepath.type == "jpeg" || filepath.type == "jpg")
  {
    //JPEG support with built in encoder
    // Fill in the compression parameter structure.
    outflip(false);  //Y-flip as necessary
    jpge::params params;
    params.m_quality = jpegquality;
    params.m_subsampling = jpge::H1V1;   //H2V2/H2V1/H1V1-none/0-grayscale
    if (!compress_image_to_jpeg_file(filepath.full.c_str(), width, height, channels, pixels, params))
    {
      fprintf(stderr, "[write_jpeg] File %s could not be saved\n", filepath.full.c_str());
      return "";
    }
  }
  else
  {
    std::string newpath = path + ".png";
    return write(newpath);
  }
  debug_print("[%s] File successfully written\n", filepath.full.c_str());
  return path;
}

unsigned char* ImageData::getBytes(unsigned int* outsize, int jpegquality)
{
  //Returns encoded image as binary data, caller must delete returned buffer!
  int sz = size();
  unsigned char* buffer = NULL;
  if (jpegquality <= 0)
  {
    outflip(true);  //Y-flip as necessary
    // Write png to stringstream
    std::stringstream ss;
    write_png(ss, channels, width, height, pixels);
    //Base64 encode!
    std::string str = ss.str();
    buffer = new unsigned char[str.length()];
    memcpy(buffer, str.c_str(), str.length());
    *outsize = str.length();
  }
  else
  {
    outflip(false);  //Y-flip as necessary
    // Writes JPEG image to memory buffer.
    // On entry, jpeg_bytes is the size of the output buffer pointed at by jpeg, which should be at least ~1024 bytes.
    // If return value is true, jpeg_bytes will be set to the size of the compressed data.
    int jpeg_bytes = sz;
    // Fill in the compression parameter structure.
    jpge::params params;
    params.m_quality = jpegquality;
    params.m_subsampling = jpge::H1V1;   //H2V2/H2V1/H1V1-none/0-grayscale
    //Additional space for rare cases of tiny images where compressed output may require larger buffer
    buffer = new unsigned char[sz + 4096];
    if (compress_image_to_jpeg_file_in_memory(buffer, jpeg_bytes, width, height, channels, (const unsigned char *)pixels, params))
      debug_print("JPEG compressed, size %d\n", jpeg_bytes);
    else
      abort_program("JPEG compress error\n");
    *outsize = jpeg_bytes;
  }

  return buffer;
}

std::string ImageData::getString(int jpegquality)
{
  //Gets encoded image as binary string
  unsigned int outsize;
  const char* buffer = (const char*)getBytes(&outsize, jpegquality);
  std::string copy = std::string(buffer, outsize);
  delete[] buffer;
  return copy;
}

std::string ImageData::getBase64(int jpegquality)
{
  //Gets encoded image as base64 string
  std::string encoded;
  unsigned int outsize;
  unsigned char* buffer = getBytes(&outsize, jpegquality);
  //Base64 encode
  encoded = base64_encode(reinterpret_cast<const unsigned char*>(buffer), outsize);
  delete[] buffer;
  return encoded;
}

std::string ImageData::getURIString(int jpegquality)
{
  //Gets encoded image as base64 data url
  std::string encoded = getBase64(jpegquality);
  if (jpegquality > 0)
    return "data:image/jpeg;base64," + encoded;
  return "data:image/png;base64," + encoded;
}

#ifdef HAVE_LIBPNG
//PNG image read/write support
static void png_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
  std::istream* stream = (std::istream*)png_get_io_ptr(png_ptr);
  stream->read((char*)data, length);
  if (stream->fail() || stream->eof()) png_error(png_ptr, "Read Error");
}

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
  std::ostream* stream = (std::ostream*)png_get_io_ptr(png_ptr);
  stream->write((const char*)data, length);
  if (stream->bad()) png_error(png_ptr, "Write Error");
}

static void png_flush(png_structp png_ptr)
{
  std::ostream* stream = (std::ostream*)png_get_io_ptr(png_ptr);
  stream->flush();
}

void* read_png(std::istream& stream, GLuint& channels, GLuint& width, GLuint& height)
{
  char header[8];   // 8 is the maximum size that can be checked
  unsigned int y;

  png_byte color_type;

  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep * row_pointers;

  // open file and test for it being a png
  stream.read(header, 8);
  if (png_sig_cmp((png_byte*)&header, 0, 8))
    abort_program("[read_png_file] File is not recognized as a PNG file");

  // initialize stuff
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
    abort_program("[read_png_file] png_create_read_struct failed");

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_program("[read_png_file] png_create_info_struct failed");

  //if (setjmp(png_jmpbuf(png_ptr)))
  //   abort_program("[read_png_file] Error during init_io");

  // initialize png I/O
  png_set_read_fn(png_ptr, (png_voidp)&stream, png_read_data);
  png_set_sig_bytes(png_ptr, 8);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 imgWidth =  png_get_image_width(png_ptr, info_ptr);
  png_uint_32 imgHeight = png_get_image_height(png_ptr, info_ptr);
  //Number of channels
  channels   = png_get_channels(png_ptr, info_ptr);
  width = imgWidth;
  height = imgHeight;
  //Row bytes
  png_uint_32 rowbytes  = png_get_rowbytes(png_ptr, info_ptr);

  color_type = png_get_color_type(png_ptr, info_ptr);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
  {
    //Convert paletted to RGB
    png_set_palette_to_rgb(png_ptr);
    channels = 3;
  }

  debug_print("Reading PNG: %d x %d, colour type %d, channels %d\n", width, height, color_type, channels);

  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);

  // read file
  //if (setjmp(png_jmpbuf(png_ptr)))
  //   abort_program("[read_png_file] Error during read_image");

  row_pointers = new png_bytep[height];
  //for (y=0; y<height; y++)
  //   row_pointers[y] = (png_byte*) malloc(rowbytes);
  png_bytep pixels = new png_byte[width * height * channels];
  for (y=0; y<height; y++)
    row_pointers[y] = (png_bytep)&pixels[rowbytes * y];

  png_read_image(png_ptr, row_pointers);

  png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)0);
  png_destroy_info_struct(png_ptr, &info_ptr);

  delete[] row_pointers;

  return pixels;
}

void write_png(std::ostream& stream, int channels, int width, int height, void* data)
{
  int colour_type;
  png_bytep      pixels       = (png_bytep) data;
  int            rowStride;
  png_bytep*     row_pointers = new png_bytep[height];
  png_structp    pngWrite;
  png_infop      pngInfo;
  int            pixel_I;
  int            flip = 1;   //Flip input data vertically (for OpenGL framebuffer data)

  pngWrite = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!pngWrite)
  {
    fprintf(stderr, "[write_png_file] create PNG write struct failed");
    return;
  }

  //Setup for different write modes
  if (channels > 3)
  {
    colour_type = PNG_COLOR_TYPE_RGB_ALPHA;
    rowStride = width * 4;
  }
  else if (channels == 3)
  {
    colour_type = PNG_COLOR_TYPE_RGB;
    rowStride = width * 3;  // Don't need to pad lines! pack alignment is set to 1
  }
  else
  {
    colour_type = PNG_COLOR_TYPE_GRAY;
    rowStride = width;
  }

  // Set pointers to start of each scanline
  for ( pixel_I = 0 ; pixel_I < height ; pixel_I++ )
  {
    if (flip)
      row_pointers[pixel_I] = (png_bytep) &pixels[rowStride * (height - pixel_I - 1)];
    else
      row_pointers[pixel_I] = (png_bytep) &pixels[rowStride * pixel_I];
  }

  pngInfo = png_create_info_struct(pngWrite);
  if (!pngInfo)
  {
    fprintf(stderr, "[write_png_file] create PNG info struct failed");
    return;
  }
  //if (setjmp(png_jmpbuf(pngWrite)))
  //   abort_program("[write_png_file] Error during init_io");

  // initialize png I/O
  png_set_write_fn(pngWrite, (png_voidp)&stream, png_write_data, png_flush);
  png_set_compression_level(pngWrite, 6); //Much faster, not much larger

  //if (setjmp(png_jmpbuf(pngWrite)))
  //   abort_program("[write_png_file] Error writing header");

  png_set_IHDR(pngWrite, pngInfo,
               width, height,
               8,
               colour_type,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(pngWrite, pngInfo);
  png_write_image(pngWrite, row_pointers);
  png_write_end(pngWrite, pngInfo);

  // Clean Up
  png_destroy_info_struct(pngWrite, &pngInfo);
  png_destroy_write_struct(&pngWrite, NULL);
  delete[] row_pointers;
}

#else //HAVE_LIBPNG
void* read_png(std::istream& stream, GLuint& channels, GLuint& width, GLuint& height)
{
  //Read the stream
  std::string s(std::istreambuf_iterator<char>(stream), {});
  unsigned char* buffer = 0;
  channels = 4; //Always loads RGBA
  unsigned status = lodepng_decode32(&buffer, &width, &height, (const unsigned char*)s.c_str(), s.length());
  if (status != 0)
  {
    fprintf(stderr, "[read_png_file] decode failed");
    return NULL;
  }
  debug_print("Reading PNG: %d x %d, channels %d\n", width, height, channels);
  size_t size = width*height*channels;
  GLubyte* pixels = new GLubyte[size];
  memcpy(pixels, buffer, size);
  free(buffer);
  return pixels;
}

void write_png(std::ostream& stream, int channels, int width, int height, void* data)
{
  unsigned char* buffer;
  size_t buffersize;
  unsigned status = 0;
  if (channels == 3)
    status = lodepng_encode24(&buffer, &buffersize, (const unsigned char*)data, width, height);
  else if (channels == 4)
    status = lodepng_encode32(&buffer, &buffersize, (const unsigned char*)data, width, height);
  else if (channels == 1)
    status = lodepng_encode_memory(&buffer, &buffersize, (const unsigned char*)data, width, height, LCT_GREY, 8); 
  else
    abort_program("Invalid channels %d\n", channels);
  if (status != 0)
  {
    fprintf(stderr, "[write_png_file] encode failed");
    return;
  }
  debug_print("Writing PNG: %d x %d, channels %d\n", width, height, channels);
  stream.write((const char*)buffer, buffersize);
  free(buffer);
}

#endif //!HAVE_LIBPNG
