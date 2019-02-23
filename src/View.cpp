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

#include "View.h"
#include "Model.h"

View::View(Session& session, float xf, float yf, float nearc, float farc) : properties(session.globals, session.defaults), session(session)
{
  // default view params
  eye_sep_ratio = 0.03f;  //Eye separation ratio to focal length
  fov = 45.0f; //60.0     //Field of view - important to adjust for stereo viewing
  focal_length = focal_length_adj = 0.0; //Stereo zero parallex distance adjustment
  scene_shift = 0.0;      //Stereo projection shift
  rotated = false;

  near = nearc;
  far = farc;

  model_size = 0.0;       //Scalar magnitude of model dimensions
  width = 0;              //Viewport width
  height = 0;             //Viewport height

  x = xf;
  y = yf;
  w = h = 1.0f;

  stereo = false;
  autozoom = false;
  filtered = true;
  scaled = true;
  initialised = false;

  localcam = new Camera();

  //Use the local cam by default
  rotate_centre = localcam->rotate_centre;
  focal_point = localcam->focal_point;
  model_trans = localcam->model_trans;
  rotation = &localcam->rotation;

  for (int i=0; i<3; i++)
  {
    default_focus[i]     = FLT_MIN;  // Default focal point
    scale[i]             = 1.0;
    min[i]               = 0.0;
    max[i]               = 0.0;
  }

  iscale = Vec3d(1.0, 1.0, 1.0);

  is3d = true;
}

View::~View()
{
  delete localcam;
}

void View::addObject(DrawingObject* obj)
{ 
  if (!hasObject(obj))
    objects.push_back(obj);
  //debug_print("Object '%s' added to viewport\n", obj->name().c_str());
}

bool View::hasObject(DrawingObject* obj)
{
  for (unsigned int i=0; i < objects.size(); i++)
    if (objects[i] == obj) return true;
  return false;
}

//Setup camera/modelview to view a model with enclosing cuboid defined by min[X,Y,Z] and max[X,Y,Z]
bool View::init(bool force, float* newmin, float* newmax)
{
  if (!newmin) newmin = min;
  if (!newmax) newmax = max;

  for (int i=0; i<3; i++)
  {
    //Invalid bounds! Skip
    if (!std::isfinite(newmin[i]) || !std::isfinite(newmax[i]))
    {
      //Invalid bounds! Flag not initialised and wait until we have a model
      initialised = false;
      return false;
    }

    if (properties["follow"])
    {
      //If bounds changed, reset focal point to default
      //(causes jitter when switching timesteps as camera follows default focal point)
      if (min[i] != newmin[i] || max[i] != newmax[i]) focal_point[i] = default_focus[i];
    }

    min[i] = newmin[i];
    max[i] = newmax[i];
    dims[i] = fabs(max[i] - min[i]);

    //Fallback focal point and rotation centre = model bounding box centre point
    if (focal_point[i] == FLT_MIN) focal_point[i] = min[i] + dims[i] / 2.0f;
    rotate_centre[i] = focal_point[i];
  }

  //Save magnitude of dimensions
  model_size = sqrt(dotProduct(dims,dims));
  if (model_size == 0 || !std::isfinite(model_size))
  {
    //Invalid bounds! Flag not initialised and wait until we have a model
    initialised = false;
    return false;
  }

  //Check and calculate near/far clip planes
  near = properties["near"];
  far = properties["far"];
  checkClip(near, far);

  if (max[2] > min[2]+FLT_EPSILON) is3d = true;
  else is3d = false;
  debug_print("Model size %f dims: %f,%f,%f - %f,%f,%f (scale %f,%f,%f) 3d? %s CLIP %f : %f\n",
              model_size, min[0], min[1], min[2], max[0], max[1], max[2], scale[0], scale[1], scale[2], (is3d ? "yes" : "no"), near, far);

  //Auto-cam etc should only be processed once... and only when viewport size has been set
  if ((force || !initialised) && width > 0 && height > 0)
  {
    //Only flag correctly initialised after focal point set (have model bounds)
    initialised = true;
    updated = true;

    projection(EYE_CENTRE);

    //Default translation by model size
    if (model_trans[2] == 0)
      model_trans[2] = -model_size;

    // Initial zoom to fit
    // NOTE without (int) cast properties["zoomstep"] == 0 here always evaluated to false!
    if ((int)properties["zoomstep"] == 0) zoomToFit();

    debug_print("   Auto cam: (Viewport %d x %d) (Model: %f x %f x %f)\n", width, height, dims[0], dims[1], dims[2]);
    debug_print("   Looking At: %f,%f,%f\n", focal_point[0], focal_point[1], focal_point[2]);
    debug_print("   Rotate Origin: %f,%f,%f\n", rotate_centre[0], rotate_centre[1], rotate_centre[2]);
    debug_print("   Clip planes: near %f far %f. Focal length %f Eye separation ratio: %f\n", (float)properties["near"], (float)properties["far"], focal_length, eye_sep_ratio);
    debug_print("   Translate: %f,%f,%f\n", model_trans[0], model_trans[1], model_trans[2]);

    //Apply changes to view
    apply();
  }

  return true;
}

void View::checkClip(float& near_clip, float& far_clip)
{
  //Adjust clipping planes
  if (near_clip == 0 || far_clip == 0)
  {
    //NOTE: Too much clip plane range can lead to depth buffer precision problems
    //Near clip should be as far away as possible as greater precision reserved for near
    if (model_size == 0 || !std::isfinite(model_size)) model_size = 1.0;
    float min_dist = model_size / 100.0;  //Estimate of min dist between viewer and geometry
    float aspectRatio = 1.33;
    if (width && height)
      aspectRatio = width / (float)height;
    if (near_clip == 0)
      near_clip = min_dist / sqrt(1 + pow(tan(0.5*M_PI*fov/180), 2) * (pow(aspectRatio, 2) + 1));
    //near_clip = model_size / 5.0;
    if (far_clip == 0)
      far_clip = model_size * 20.0;
    assert(near_clip > 0.0 && far_clip > 0.0);
  }

  if (near_clip < model_size * 0.001) near_clip = model_size * 0.001; //Bounds check
}

void View::getMinMaxDistance(float* min, float* max, float range[2], float mv[16], bool eyePlane)
{
  //Preserve the modelView at time of calculations
  {
    std::lock_guard<std::mutex> guard(matrix_lock);
    memcpy(mv, &modelView, sizeof(float)*16);
  }

  //Save min/max distance
  float vert[3], dist;
  range[0] = HUGE_VAL;
  range[1] = -HUGE_VAL;
  for (int i=0; i<2; i++)
  {
    vert[0] = i==0 ? min[0] : max[0];
    for (int j=0; j<2; j++)
    {
      vert[1] = j==0 ? min[1] : max[1];
      for (int k=0; k<2; k++)
      {
        vert[2] = k==0 ? min[2] : max[2];
        if (eyePlane)
        {
          dist = eyePlaneDistance(mv, vert);
        }
        else
        {
          dist = eyeDistance(mv, vert);
        }
        if (dist < range[0]) range[0] = dist;
        if (dist > range[1]) range[1] = dist;
      }
    }
  }
  if (range[1] == range[0]) range[1] += 0.0000001;
  //printf("DISTANCE MIN %f MAX %f\n", range[0], range[1]);
}

void View::autoRotate()
{
  //If model is 2d plane on X or Y axis, rotate to face camera
  bool unrotated = (rotation->x == 0.0 && rotation->y == 0.0 && rotation->z == 0.0);
  if (unrotated && session.min[0] == session.max[0])
    rotate(0, 90, 0);
  if (unrotated && session.min[1] == session.max[1])
    rotate(-90, 0, 0);
}

std::string View::rotateString()
{
  //Convert rotation to command string...
  std::ostringstream ss;
  ss << "rotation " << rotation->x << " " << rotation->y << " " << rotation->z << " " << rotation->w;
  return ss.str();
}

std::string View::translateString()
{
  //Convert translation to command string...
  std::ostringstream ss;
  ss << "translation " << model_trans[0] << " " << model_trans[1] << " " << model_trans[2];
  return ss.str();
}

void View::getCamera(float rotate[4], float translate[3], float focus[3])
{
  //Convert translation & rotation to array...
  //rotation->toEuler(rotate[0], rotate[1], rotate[2]);
  rotate[0] = rotation->x;
  rotate[1] = rotation->y;
  rotate[2] = rotation->z;
  rotate[3] = rotation->w;
  memcpy(translate, model_trans, sizeof(float) * 3);
  memcpy(focus, focal_point, sizeof(float) * 3);
}

std::string View::adjustStereo(float aperture, float focal_len, float eye_sep)
{
  fov = properties["aperture"];
  if (aperture < 10)
    fov += aperture;
  else
    fov = aperture;
  if (fov < 10) fov = 10;
  if (fov > 170) fov = 170;

  focal_length_adj += focal_len;
  eye_sep_ratio += eye_sep;

  //if (eye_sep_ratio < 0) eye_sep_ratio = 0;
  debug_print("STEREO: Aperture %f Focal Length Adj %f Eye Separation %f\n", fov, focal_length_adj, eye_sep_ratio);
  std::ostringstream ss;
  if (aperture) ss << "aperture " << fov;
  if (focal_len) ss << "focallength " << focal_len;
  if (eye_sep) ss << "eyeseparation " << eye_sep;
  updated = true;
  return ss.str();
}

void View::focus(float x, float y, float z, float aperture, bool setdefault)
{
  focal_point[0] = rotate_centre[0] = x;
  focal_point[1] = rotate_centre[1] = y;
  focal_point[2] = rotate_centre[2] = z;
  if (aperture > 0)
    fov = aperture;
  //reset(); //reset view
  //Set as the default
  if (setdefault)
    memcpy(default_focus, focal_point, sizeof(float)*3);
  updated = true;
}

void View::setTranslation(float x, float y, float z)
{
  model_trans[0] = x;
  model_trans[1] = y;
  model_trans[2] = z;
  updated = true;
}

void View::translate(float x, float y, float z)
{
  model_trans[0] += x;
  model_trans[1] += y;
  model_trans[2] += z;
  updated = true;
}

void View::setRotation(float x, float y, float z, float w)
{
  rotation->x = x;
  rotation->y = y;
  rotation->z = z;
  rotation->w = w;
  rotated = true;
  updated = true;
}

void View::setRotation(float x, float y, float z)
{
  //Convert from Euler angles
  rotation->fromEuler(x, y, z);
  rotated = true;
  updated = true;
}

void View::rotate(float degrees, Vec3d axis)
{
  Quaternion nrot;
  nrot.fromAxisAngle(axis, degrees);
  nrot.normalise();
  *rotation = nrot * *rotation;
  rotated = true;
}

void View::rotate(float degreesX, float degreesY, float degreesZ)
{
  //std::cerr << "Rotate : " << degreesX << "," << degreesY << "," << degreesZ << std::endl;
  rotate(degreesX, Vec3d(1,0,0));
  rotate(degreesY, Vec3d(0,1,0));
  rotate(degreesZ, Vec3d(0,0,1));
}

void View::setScale(float x, float y, float z, bool replace)
{
  if (x <= 0.0) x = 1.0;
  if (y <= 0.0) y = 1.0;
  if (z <= 0.0) z = 1.0;
  if (replace)
  {
    scale[0] = x;
    scale[1] = y;
    scale[2] = z;
  }
  else
  {
    scale[0] *= x;
    scale[1] *= y;
    scale[2] *= z;
  }

  iscale = Vec3d(1.0/scale[0], 1.0/scale[1], 1.0/scale[2]);
  updated = true;
}

std::string View::zoom(float factor)
{
  float adj = factor * model_size;
  if (fabs(model_trans[2]) < model_size) adj *= 0.1;
  model_trans[2] += adj;
  if (model_trans[2] > model_size*0.3) model_trans[2] = model_size*0.3;
  std::ostringstream ss;
  ss << "translate z " << adj;
  updated = true;
  return ss.str();
}

std::string View::zoomClip(float factor)
{
  near += factor * model_size;
  checkClip(near, far);
  updated = true;
  //Returns command to set in history
  std::ostringstream ss;
  ss << "nearclip " << near;
  return ss.str();
}

void View::reset()
{
  for (int i=0; i<3; i++)
  {
    model_trans[i] = 0;
    //Default focal point and rotation centre = model bounding box centre point
    focal_point[i] = rotate_centre[i] = FLT_MIN;
    //if (focal_point[i] == FLT_MIN)
    //   focal_point[i] = min[i] + dims[i] / 2.0f;
  }
  rotation->identity();
  rotated = true;   //Flag rotation
  updated = true;
}

void View::print()
{
  float xrot, yrot, zrot;
  rotation->toEuler(xrot, yrot, zrot);
  printf("------------------------------\n");
  printf("Viewport %d,%d %d x %d\n", xpos, ypos, width, height);
  printf("Clip planes: near %f far %f\n", near, far);
  printf("Model size %f dims: %f,%f,%f - %f,%f,%f (scale %f,%f,%f)\n",
         model_size, min[0], min[1], min[2], max[0], max[1], max[2], scale[0], scale[1], scale[2]);
  printf("Focal Point %f,%f,%f\n", focal_point[0], focal_point[1], focal_point[2]);
  printf("Rotate Centre %f,%f,%f\n", rotate_centre[0], rotate_centre[1], rotate_centre[2]);
  printf("------------------------------\n");
  printf("%s\n", translateString().c_str());
  printf("%s\n", rotateString().c_str());
  printf("------------------------------\n");
#ifdef DEBUG
  GLfloat modelview[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
  printMatrix(modelview);
#endif
}

//Absolute viewport
void View::port(int x, int y, int width, int height)
{
  this->width = width;
  this->height = height;
  xpos = x;
  ypos = y;

  glViewport(x, y, width, height);
  glScissor(x, y, width, height);
  //debug_print("Viewport set at %d,%d %d x %d\n", x, y, width, height);
}

//Viewport calculated using saved position & dim values
void View::port(int win_width, int win_height)
{
  width = ceil(w * win_width);
  height = ceil(h * win_height);

  xpos = ceil(x * win_width);
  ypos = ceil((1.0 - y - h) * win_height); //Flip y-coord for gl

  //Ensure full window area filled
  if (abs(win_width - (xpos + width)) < 5)
    width = win_width - xpos;
  if (abs(win_height - (ypos + height)) < 5)
    height = win_height - ypos;

  glViewport(xpos, ypos, width, height);
  glScissor(xpos, ypos, width, height);
  //debug_print("-------Viewport (%f,%f %fx%f) set at %d,%d %d x %d\n", x,y,w,h, xpos, ypos, width, height);
}

bool View::hasPixel(int x, int y)
{
  if (x < xpos) return false;
  if (y < ypos) return false;
  if (x > xpos + width) return false;
  if (y > ypos + height) return false;
  return true;
}

void View::projection(int eye)
{
  if (!initialised) return;
  float aspectRatio = width / (float)height;

  // Perspective viewing frustum parameters
  float left, right, top, bottom;
  float eye_separation, frustum_shift;

  //Ensure clip planes valid, calculate if not provided in properties
  near = properties["near"];
  far = properties["far"];
  fov = properties["aperture"];
  bool ortho = properties["orthographic"];
  checkClip(near, far);
  //printf("VP %d / %d\nsetPerspective( %f, %f, %f, %f)\n", width, height, fov, aspectRatio, near, far);

  //This is zero parallax distance, objects closer than this will appear in front of the screen,
  //default is to set to distance to model front edge...
  float focal_length = fabs(model_trans[2]) - model_size * 0.5; //3/4 of model size - distance from eye to model centre
  focal_length += focal_length_adj;   //Apply user adjustment (default 0)
  if (focal_length < 0) focal_length = 0.1;

  //Calculate eye separation based on focal length
  eye_separation = focal_length * eye_sep_ratio;

  // Build the viewing frustum
  // Top of frustum calculated from field of view (aperture) and near clipping plane, camera->aperture = fov in degrees
  top = tan(0.5f * DEG2RAD * fov) * near;
  bottom = -top;
  // Account for aspect ratio (width/height) to get right edge of frustum
  right = aspectRatio * top;
  left = -right;

  //Shift frustum to the left/right to account for right/left eye viewpoint
  frustum_shift = eye * 0.5 * eye_separation * fabs(near / focal_length);  //Mutiply by eye (-1 left, 0 none, 1 right)
  //Viewport eye shift in pixels => for raycasting shader
  eye_shift = eye * eye_sep_ratio * height * 0.6 / tan(DEG2RAD * fov);

  // In Stereo, View vector for each camera is parallel
  // Need to adjust eye position and focal point to left/right to account for this...
  //Shift model by half of eye-separation in opposite direction of eye (equivalent to camera shift in same direction as eye)
  scene_shift = eye * eye_separation * -0.5f;

  if (eye) debug_print("STEREO %s: focalLen: %f eyeSep: %f frustum_shift: %f, scene_shift: %f eye_shift %f\n", (eye < 0 ? "LEFT (RED)  " : "RIGHT (BLUE)"),
                         focal_length, eye_separation, frustum_shift, scene_shift, eye_shift);
  //debug_print(" Ratio %f Left %f Right %f Top %f Bottom %f Near %f Far %f\n",
  //         aspectRatio, left, right, bottom, top, near, far);

  // Set up our projection transform
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  //Orthographic mode?
  if (ortho)
  {
    float x = aspectRatio * focal_length;
    float y = focal_length;
    glOrtho(-x, x, -y, y, near, far);
  }
  else
    glFrustum(left - frustum_shift, right - frustum_shift, bottom, top, near, far);

  // Return to model view
  glMatrixMode(GL_MODELVIEW);
  GL_Error_Check;
}

void View::apply(bool no_rotate, Quaternion* obj_rotation, Vec3d* obj_translation)
{
  // Right-handed (GL default) or Left-handed
  int orientation = properties["coordsystem"];
  if (properties["globalcam"])
  {
    if (!session.globalcam) 
      session.globalcam = new Camera(localcam);

    //Global camera override
    rotate_centre = session.globalcam->rotate_centre;
    focal_point = session.globalcam->focal_point;
    model_trans = session.globalcam->model_trans;
    rotation = &session.globalcam->rotation;
  }
  else
  {
    //Use the local cam by default
    rotate_centre = localcam->rotate_centre;
    focal_point = localcam->focal_point;
    model_trans = localcam->model_trans;
    rotation = &localcam->rotation;
  }

  // Setup view transforms
  glMatrixMode(GL_MODELVIEW);
  if (!session.omegalib)
  {
    glLoadIdentity();
    GL_Error_Check;

    // Translate to cancel stereo parallax
    glTranslatef(scene_shift, 0.0, 0.0);
    GL_Error_Check;

    // Translate model away from eye by camera zoom/pan translation
    //debug_print("APPLYING VIEW '%s': trans %f,%f,%f\n", title.c_str(), model_trans[0], model_trans[1], model_trans[2]);
    glTranslatef(model_trans[0], model_trans[1], model_trans[2]);
    GL_Error_Check;

  }

  // Adjust centre of rotation, default is same as focal point so this does nothing...
  float adjust[3] = {(focal_point[0]-rotate_centre[0]), (focal_point[1]-rotate_centre[1]), (focal_point[2]-rotate_centre[2])};
  glTranslatef(-adjust[0], -adjust[1], -adjust[2]);
  GL_Error_Check;

  // Rotate model
  if (!no_rotate)
    rotation->apply();

  // Translate object
  if (obj_translation)
    glTranslatef(obj_translation->x, obj_translation->y, obj_translation->z);

  // Rotate object
  if (obj_rotation)
    obj_rotation->apply();

  GL_Error_Check;

  // Apply scaling factors
  if (scale[0] != 1.0 || scale[1] != 1.0 || scale[2] != 1.0)
  {
    glScalef(scale[0], scale[1], scale[2]);
    iscale = Vec3d(1.0/scale[0], 1.0/scale[1], 1.0/scale[2]);
  }
  GL_Error_Check;

  // Adjust back for rotation centre
  glTranslatef(adjust[0], adjust[1], adjust[2]);
  GL_Error_Check;

  // Translate to align eye with model centre - view focal point
  //glTranslatef(-rotate_centre[0], -rotate_centre[1], -rotate_centre[2]);
  //if (use_fp) glTranslatef(-focal_point[0], -focal_point[1], orientation * -focal_point[2]);
  if (!session.omegalib) glTranslatef(-focal_point[0], -focal_point[1], orientation * -focal_point[2]);
  GL_Error_Check;

  // Switch coordinate system if applicable and set default polygon front faces
  if (orientation == RIGHT_HANDED)
  {
    glFrontFace(GL_CCW);
  }
  else
  {
    glFrontFace(GL_CW);
    glScalef(1.0, 1.0, -1.0);
  }
  GL_Error_Check;

  //Store the matrix
  std::lock_guard<std::mutex> guard(matrix_lock);
  glGetFloatv(GL_MODELVIEW_MATRIX, modelView);

  //Copy updates to properties
  if (updated)
  {
    exportProps();
    updated = false;
  }
}

void View::importProps()
{
  if (!initialised || updated) return;
  //Copy view properties to cache
  //Skip import cam if not provided
  if (properties.has("rotate"))
  {
    json rot = properties["rotate"];
    /*json erot = properties["xyzrotate"];
    if (erot.size() == 3)
    {
      rotation->identity();
      //setRotation(erot[0], erot[1], erot[2]);
      rotate(erot[0], 0, 0);
      rotate(0, erot[1], 0);
      rotate(0, 0, erot[2]);
    }
    else*/
    if (rot.size() == 4)
      setRotation(rot[0], rot[1], rot[2], rot[3]);
    else if (rot.size() == 3)
      setRotation(rot[0], rot[1], rot[2]);
  }

  if (properties.has("translate"))
  {
    json trans = properties["translate"];
    setTranslation(trans[0], trans[1], trans[2]);
  }

  if (properties.has("focus"))
  {
    json foc = properties["focus"];
    focus(foc[0], foc[1], foc[2]);
  }

  if (properties.has("scale"))
  {
    json sc = properties["scale"];
    scale[0] = sc[0];
    scale[1] = sc[1];
    scale[2] = sc[2];
  }
  //min = aproperties["min"];
  //max = aproperties["max"];
  //init(false, newmin, newmax);
  fov = properties["aperture"];
  near = properties.data["near"];
  far = properties.data["far"];
}

void View::exportProps()
{
  if (!initialised) return;
  float rota[3];
  rotation->toEuler(rota[0], rota[1], rota[2]);
  properties.data["rotate"] = json::array({rotation->x, rotation->y, rotation->z, rotation->w});
  properties.data["xyzrotate"] = json::array({rota[0], rota[1], rota[2]});
  properties.data["translate"] = json::array({model_trans[0], model_trans[1], model_trans[2]});
  properties.data["focus"] = json::array({focal_point[0], focal_point[1], focal_point[2]});
  properties.data["scale"] = json::array({scale[0], scale[1], scale[2]});
  properties.data["aperture"] = fov;
  properties.data["near"] = near;
  properties.data["far"] = far;

  //Can't set min/max properties from auto calc or will override future bounding box calc,
  //useful to get the calculated bounding box, so export as "bounds"
  json bounds;
  bounds["min"] = min;
  bounds["max"] = max;
  properties.data["bounds"] = bounds;
  properties.data["is3d"] = is3d;

  /*/Converts named colours to js readable
  if (properties.data.count("background") > 0)
    properties.data["background"] = Colour(properties.data["background"]).toString();
  if (properties.data.count("bordercolour") > 0)
    properties.data["bordercolour"] = Colour(properties.data["bordercolour"]).toString();*/
}

int View::switchCoordSystem()
{
  if ((int)properties["coordsystem"] == LEFT_HANDED)
    properties.data["coordsystem"] = RIGHT_HANDED;
  else
    properties.data["coordsystem"] = LEFT_HANDED;
  rotated = true;   //Flag rotation
  return properties["coordsystem"];
}

#define ADJUST 0.444444
void View::zoomToFit()
{
  float margin = properties["margin"];
  if (margin < 1.0)
    //Get margin as ratio of viewport width
    margin = floor(width * margin);
  else
    margin *= session.scale2d; //Multiply by 2d scale factor

  // The bounding box of model
  GLfloat rect3d[8][3] = {{min[0], min[1], min[2]},
    {min[0], min[1], max[2]},
    {min[0], max[1], min[2]},
    {min[0], max[1], max[2]},
    {max[0], min[1], min[2]},
    {max[0], min[1], max[2]},
    {max[0], max[1], min[2]},
    {max[0], max[1], max[2]}
  };

  //3d rect vertices, object and window coords
  GLfloat modelview[16];
  GLfloat projection[16];
  int viewport[4];
  int count = 0;
  double error = 1, scalerect, adjust = ADJUST;
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetFloatv(GL_PROJECTION_MATRIX, projection);

  // Continue scaling adjustments until within tolerance
  while (count < 30 && fabs(error) > 0.005)
  {
    float min_x = 10000, min_y = 10000, max_x = -10000, max_y = -10000;
    GLfloat win3d[8][3];
    int i;

    // Set camera and get modelview matrix defined by viewpoint
    apply();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    for (i = 0; i < 8; i++)
    {
      gluProjectf(rect3d[i][0], rect3d[i][1], rect3d[i][2],
                  modelview, projection, viewport, &win3d[i][0]);
      //Save max/min x and y - define bounding 2d rectangle
      if (win3d[i][0] < min_x) min_x = win3d[i][0];
      if (win3d[i][0] > max_x) max_x = win3d[i][0];
      if (win3d[i][1] < min_y) min_y = win3d[i][1];
      if (win3d[i][1] > max_y) max_y = win3d[i][1];
      //printf("  %d) rect %f,%f,%f win %f,%f,%f\n", i, rect3d[i][0], rect3d[i][1], rect3d[i][2], win3d[i][0], win3d[i][1], win3d[i][2]);
    }

    // Calculate min bounding rectangle centered in viewport (with margins)
    {
      // Offset to viewport edges as gluProject returns window coords
      min_x -= (viewport[0] + margin);
      min_y -= (viewport[1] + margin);
      max_x -= (viewport[0] + margin);
      max_y -= (viewport[1] + margin);

      double width = viewport[2] - margin*2;
      double height = viewport[3] - margin*2;
      double centrex = width/2.0;
      double centrey = height/2.0;

      double dminx = fabs(min_x - centrex);
      double dmaxx = fabs(max_x - centrex);
      double new_max_x = centrex + (dminx > dmaxx ? dminx : dmaxx);
      double new_min_x = width - new_max_x;

      double dminy = fabs(min_y - centrey);
      double dmaxy = fabs(max_y - centrey);
      double new_max_y = centrey + (dminy > dmaxy ? dminy : dmaxy);
      double new_min_y = height - new_max_y;

      double xscale = 1.0, yscale = 1.0;
      float rwidth = (new_max_x - new_min_x);
      float rheight = (new_max_y - new_min_y);

      xscale = width / rwidth;
      yscale = height / rheight;
      if (xscale < yscale) scalerect = xscale;
      else scalerect = yscale;
    }

    // debug_print("BB new_min_x %f new_max_x %f === ", new_min_x, new_max_x);
    //   debug_print("Bounding rect: %f,%f - %f,%f === ",  min_x, min_y, max_x, max_y);
    //   debug_print(" Min rect: 0,0 - %f,%f\n", max_x - min_x, max_y - min_y);

    //Self-adjusting: did we overshoot aim? - compare last error to current 2d scale
    if (count > 0)
    {
      if ((error > 0 && scalerect < 1.0) ||
          (error < 0 && scalerect > 1.0))
      {
        adjust *= 0.75;
        if (adjust > ADJUST) adjust = ADJUST;
      }
      else
      {
        adjust *= 1.5;
        if (adjust < ADJUST) adjust = ADJUST;
      }
    }

    // Try to guess the best value adjustment using error margin
    //If error (devergence from 1.0) is > 0, scale down, < 0 scale up
    error = (scalerect - 1.0) / scalerect;

    //Adjust zoom translation by error * adjust (0.5 default)
    //float oldz = model_trans[2];
    model_trans[2] -= (model_trans[2] * error * adjust);
    //if (count > 4) {
    //   debug_print("[%d iterations] ... 2D Scaling factor %f ", count, scalerect);
    //   debug_print(" --> error: %f (adjust %f) zoom from %f to %f\n", error, adjust, oldz, model_trans[2]);
    //}
    count++;
  }

  updated = true;
}

void View::drawOverlay()
{
  //2D overlay objects, apply text scaling
  assert(width && height);
  Viewport2d(width, height);
  glScalef(session.scale2d, session.scale2d, session.scale2d);
  int w = width / session.scale2d;
  int h = height / session.scale2d;
  GL_Error_Check;

  //Colour bars
  int last_B[4] = {0, 0, 0, 0};
  float adjust = cbrt(w*h)/65.0;
  for (unsigned int i=0; i<objects.size(); i++)
  {
    //Only when flagged as colour bar
    if (!objects[i]->properties["colourbar"] || !objects[i]->properties["visible"]) continue;
    objects[i]->setup(); //Required to cache colouring values
    ColourMap* cmap = objects[i]->colourMap;
    //Use the first available colourmap by default
    if (!cmap && session.colourMaps && session.colourMaps->size() > 0)
      cmap = (*session.colourMaps)[0];
    if (!cmap) continue;

    float position = objects[i]->properties["position"];
    std::string align = objects[i]->properties["align"];
    int ww = w, hh = h;
    bool vertical = false;
    bool opposite = (align == "left" || align == "bottom");
    int side = 0;
    if (opposite) side += 1;
    //Vertical?
    if (align == "left" || align == "right")
    {
      side += 2;
      vertical = true;
      //Default position for vertical is offset from top
      if (position == 0 && !objects[i]->properties.has("position"))
        position = -0.06;
      ww = h;
      hh = w;
    }

    //Dimensions, default is to calculate automatically
    json size = objects[i]->properties["size"];
    float breadth = size[1];
    float length = size[0];
    if (length == 0)
      length = vertical ? 0.5 : 0.8;
    if (breadth == 0)
      breadth = vertical ? 20 : 10;

    //Size: if in range [0,1] they are a ratio of window size so multiply to get pixels
    if (length < 1.0) length *= ww;
    if (breadth < 1.0) breadth *= hh;

    //Default to vector font if downsampling and no other font requested
    Properties cbprops(session.globals, session.defaults);
    if (session.scale2d != 1.0 && !objects[i]->properties.has("font"))
    {
      cbprops.data["font"] = "vector";
      cbprops.data["fontscale"] = 0.4*adjust;
    }
    //Update to overwrite defaults if any set by user
    Properties::mergeJSON(cbprops.data, objects[i]->properties.data);

    //Margin offset
    float margin = objects[i]->properties["offset"];
    if (margin == 0)
    {
      //Calculate a sensible default margin
      session.fonts.setFont(cbprops);
      if (vertical)
        margin = 18 + session.fonts.printWidth("1.000001");
      else
        margin = 7 + session.fonts.printWidth("1.1");
    }

    //Position: if in range [0,1] they are a ratio of window size so multiply to get pixels
    if (fabs(position) < 1.0) position *= ww;
    if (margin < 1.0) margin *= hh;

    //Add previous offset used and store for next
    margin = last_B[side] + margin;
    last_B[side] = margin + breadth;
    
    //Calc corner coords
    int start_A = (ww - length) / 2; //Centred, default for horizontal
    if (position > 0) start_A = position;
    if (position < 0) start_A = ww + position - length;
    int start_B = margin;

    if (!opposite) start_B = hh - start_B - breadth;

    cmap->draw(session, cbprops, start_A, start_B, length, breadth, textColour, vertical);
    GL_Error_Check;
  }

  GL_Error_Check;

  //Title
  std::string title = properties["title"];
  if (title.length())
  {
    //Timestep macro ##
    size_t pos =  title.find("##");
    if (pos != std::string::npos && session.now >= 0 && (int)session.timesteps.size() >= session.now)
      title.replace(pos, 2, std::to_string(session.timesteps[session.now]->step));

    glColor3ubv(textColour.rgba);
    session.fonts.setFont(properties, "vector", 1.0);
    if (session.fonts.charset == FONT_VECTOR)
      session.fonts.fontscale *= 0.6*adjust; //Scale down vector font slightly for title
    session.fonts.print(0.5 * (w - session.fonts.printWidth(title.c_str())), h - 3 - session.fonts.printWidth("W"), title.c_str());
  }

  GL_Error_Check;
  //Restore 3d
  Viewport2d(0, 0);
  GL_Error_Check;
}

void View::setBackground()
{
  background = Colour(properties["background"]);
  inverse = background;
  inverse.invert();
  //Calculate text foreground colour black/white depending on background intensity
  int avg = (background.r + background.g + background.b) / 3.0;
  textColour.value = 0xff000000;
  if (avg < 127) 
    textColour.value = 0xffffffff;
  session.defaults["colour"] = textColour.toJson();
}

