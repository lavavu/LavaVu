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

#include "GraphicsUtil.h"
#include "DrawingObject.h"
#include "Session.h"

#ifndef View__
#define View__

#define EYE_CENTRE 0
#define EYE_LEFT -1
#define EYE_RIGHT 1

#define INERTIA_ON  0.1f
#define INERTIA_OFF 0.0f

//Coordinate systems
#define RIGHT_HANDED 1
#define LEFT_HANDED -1

class Camera
{
public:
  float rotate_origin[3];    // Centre of rotation
  float focal_point[3];      // Focal point

  float model_trans[3];
  Quaternion rotation;

  Camera(Camera* cam=NULL)
  {
    for (int i=0; i<3; i++)
    {
      rotate_origin[i]     = cam ? cam->rotate_origin[i] : 0.0;
      focal_point[i]       = cam ? cam->focal_point[i]   : FLT_MIN;
      model_trans[i]       = cam ? cam->model_trans[i]   : 0.0;
      rotation[i]          = cam ? cam->rotation[i]      : 0.0;
    }

    rotation[3]            = cam ? cam->rotation[3]      : 1.0;
  }
};

Quaternion rotationFromProperty(const json& jsonprop);

class View
{
public:
  // Settings
  bool stereo;
  bool autozoom;
  bool filtered; //Filter objects in view by object list
  bool scaled;
  bool rotated;  //Flags whether view has rotated since last redraw
  bool updated = false;

  // view params
  float x;          // X offset [0,1]
  float y;          // Y offset [0,1]
  float w;          // Width ratio [0,1]
  float h;          // Height ratio [0,1]
  int xpos;         // X coord
  int ypos;         // Y coord
  int width;        // Viewport height
  int height;       // Viewport width
  int base_height;  // Initial height

  //View properties data...
  Properties properties;

  // Scene scaling
  float scale[3];
  Vec3d iscale; //Factors for un-scaling

  float model_size;   //Scalar magnitude of model dimensions = length of diagonal of bounding box

  // Dimensions
  float dims[3];
  float min[3], max[3];
  bool initialised;

  Colour background;
  Colour inverse;
  Colour textColour;

private:
  Session& session;
  Camera* localcam;
  float* rotate_origin;      // Centre of rotation
  float* focal_point;        // Focal point

  float default_focus[3];    // Default Focal point

  float* model_trans;
  Quaternion* rotation;

  float focal_length;        // Stereo zero parallex distance
  float scene_shift;         // Stereo projection shift (calculated from eye sep)
  float focal_length_adj;    // User adjust to focal length

  float nearclip, farclip;

  std::mutex matrix_lock;

public:
  std::vector<DrawingObject*> objects;     // Contains these objects
  float fov;                 // Field of view
  bool is3d;
  float eye_shift;           // Stereo eye shift factor
  float eye_sep_ratio;       // Eye separation ratio to focal length
  mat4 modelView;

  View(Session& session, float xf = 0, float yf = 0);

  ~View();

  void clear(bool objects=true)
  {
    if (objects) this->objects.clear();
    initialised = false;
    reset();
  }

  void addObject(DrawingObject* obj);
  bool hasObject(DrawingObject* obj);

  bool init(bool force=false, float* newmin=NULL, float* newmax=NULL);
  void checkClip(float& near_clip, float& far_clip);
  void getMinMaxDistance(float* min, float* max, float range[2], bool eyePlane=false);
  float eyeDistance(const Vec3d& vec);
  float eyePlaneDistance(const Vec3d& vec);
  void autoRotate();
  std::string rotateString();
  std::string translateString();
  void getCamera(float rotate[4], float translate[3], float focus[3]);
  std::string adjustStereo(float aperture, float focal_len, float eye_sep);
  void focus(float x, float y, float z, float aperture=0, bool setdefault=false);
  void origin(float x, float y, float z);
  void translate(float x, float y, float z);
  void setTranslation(float x, float y, float z);
  void setRotation(float x, float y, float z, float w);
  void setRotation(float x, float y, float z);
  Quaternion getRotation() {return *rotation;}
  void rotate(float degrees, Vec3d axis);
  void rotate(float degreesX, float degreesY, float degreesZ);
  void applyRotation();

  void setScale(float x, float y, float z, bool replace=true);
  std::string zoom(float factor);
  std::string zoomClip(float factor);
  void reset();
  void print();

  void port(int x, int y, int width, int height);
  void port(int win_width, int win_height);
  bool hasPixel(int x, int y);

  void projection(int eye);
  void apply(Properties* objprops=NULL);
  int switchCoordSystem();
  void zoomToFit();
  void exportProps();
  void importProps(bool force=false);

  //Utility functions
  void drawOverlay();
  void setBackground();
};

#endif //View__
