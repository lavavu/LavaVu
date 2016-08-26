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

Camera* View::globalcam = NULL;

View::View(float xf, float yf, float nearc, float farc)
{
  // default view params
  near_clip = nearc;       //Near clip plane
  far_clip = farc;       //Far clip plane
  eye_sep_ratio = 0.03f;  //Eye separation ratio to focal length
  fov = 45.0f; //60.0     //Field of view - important to adjust for stereo viewing
  focal_length = focal_length_adj = 0.0; //Stereo zero parallex distance adjustment
  scene_shift = 0.0;      //Stereo projection shift
  rotated = rotating = sort = false;

  model_size = 0.0;       //Scalar magnitude of model dimensions
  width = 0;              //Viewport width
  height = 0;             //Viewport height
  textscale = false;
  scale2d = 1.0;

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

  is3d = true;

  //View properties can only be set if exist, so copy defaults
  std::string viewprops[] = {"title", "zoomstep", "margin", 
                             "rulers", "rulerticks", "rulerwidth", 
                             "fontscale", "border", "fillborder", "bordercolour", 
                             "axis", "axislength", "timestep", "antialias", "shift"};
  for (auto key : viewprops)
    properties.data[key] = Properties::defaults[key];
}

View::~View()
{
  delete localcam;
}

void View::addObject(DrawingObject* obj)
{
  objects.push_back(obj);
  debug_print("Object '%s' added to viewport\n", obj->name().c_str());
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
    if (!ISFINITE(newmin[i]) || !ISFINITE(newmax[i])) return false;

    //If bounds changed, reset focal point to default
    if (min[i] != newmin[i] || max[i] != newmax[i]) focal_point[i] = default_focus[i];

    min[i] = newmin[i];
    max[i] = newmax[i];
    dims[i] = fabs(max[i] - min[i]);

    //Fallback focal point and rotation centre = model bounding box centre point
    if (focal_point[i] == FLT_MIN) focal_point[i] = min[i] + dims[i] / 2.0f;
    rotate_centre[i] = focal_point[i];
  }

  //Save magnitude of dimensions
  model_size = sqrt(dotProduct(dims,dims));
  if (model_size == 0 || !ISFINITE(model_size)) return false;

  //Check and calculate near/far clip planes
  checkClip();

  if (max[2] > min[2]+FLT_EPSILON) is3d = true;
  else is3d = false;
  debug_print("Model size %f dims: %f,%f,%f - %f,%f,%f (scale %f,%f,%f) 3d? %s\n",
              model_size, min[0], min[1], min[2], max[0], max[1], max[2], scale[0], scale[1], scale[2], (is3d ? "yes" : "no"));

  //Auto-cam etc should only be procesed once...and only when viewport size has been set
  if ((force || !initialised) && width > 0 && height > 0)
  {
    //Only flag correctly initialised after focal point set (have model bounds)
    initialised = true;

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
    debug_print("   Clip planes: near %f far %f. Focal length %f Eye separation ratio: %f\n", near_clip, far_clip, focal_length, eye_sep_ratio);
    debug_print("   Translate: %f,%f,%f\n", model_trans[0], model_trans[1], model_trans[2]);

    //Apply changes to view
    apply();
  }

  return true;
}

void View::checkClip()
{
  //Adjust clipping planes
  if (near_clip == 0 || far_clip == 0)
  {
    //NOTE: Too much clip plane range can lead to depth buffer precision problems
    //Near clip should be as far away as possible as greater precision reserved for near
    float min_dist = model_size / 10.0;  //Estimate of min dist between viewer and geometry
    float aspectRatio = 1.33;
    if (width && height)
      aspectRatio = width / (float)height;
    near_clip = min_dist / sqrt(1 + pow(tan(0.5*M_PI*fov/180), 2) * (pow(aspectRatio, 2) + 1));
    //near_clip = model_size / 5.0;
    far_clip = model_size * 20.0;
    debug_print("Auto-corrected clip planes: near %f far %f.\n", near_clip, far_clip);
    assert(near_clip > 0.0 && far_clip > 0.0);
  }
}

void View::getMinMaxDistance(float* mindist, float* maxdist)
{
  //Save min/max distance
  float vert[3], dist;
  glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
  *maxdist = -HUGE_VAL;
  *mindist = HUGE_VAL;
  for (int i=0; i<2; i++)
  {
    vert[0] = i==0 ? min[0] : max[0];
    for (int j=0; j<2; j++)
    {
      vert[1] = j==0 ? min[1] : max[1];
      for (int k=0; k<2; k++)
      {
        vert[2] = k==0 ? min[2] : max[2];
        dist = eyeDistance(modelView, vert);
        if (dist < *mindist) *mindist = dist;
        if (dist > *maxdist) *maxdist = dist;
      }
    }
  }
  if (*maxdist == *mindist) *maxdist += 0.0000001;
  //printf("DISTANCE MIN %f MAX %f\n", *mindist, *maxdist);
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
  fov += aperture;
  if (fov < 30) fov = 30;
  if (fov > 70) fov = 70;
  focal_length_adj += focal_len;
  eye_sep_ratio += eye_sep;
  //if (eye_sep_ratio < 0) eye_sep_ratio = 0;
  debug_print("STEREO: Aperture %f Focal Length Adj %f Eye Separation %f\n", fov, focal_length_adj, eye_sep_ratio);
  std::ostringstream ss;
  if (aperture) ss << "aperture " << aperture;
  if (focal_len) ss << "focallength " << focal_len;
  if (eye_sep) ss << "eyeseparation " << eye_sep;
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
}

void View::setTranslation(float x, float y, float z)
{
  model_trans[0] = x;
  model_trans[1] = y;
  model_trans[2] = z;
}

void View::translate(float x, float y, float z)
{
  model_trans[0] += x;
  model_trans[1] += y;
  model_trans[2] += z;
}

void View::setRotation(float x, float y, float z, float w)
{
  rotation->x = x;
  rotation->y = y;
  rotation->z = z;
  rotation->w = w;
}

void View::rotate(float degrees, Vec3d axis)
{
  Quaternion nrot;
  nrot.fromAxisAngle(axis, degrees);
  nrot.normalise();
  *rotation = nrot * *rotation;
}

void View::rotate(float degreesX, float degreesY, float degreesZ)
{
  //std::cerr << "Rotate : " << degreesX << "," << degreesY << "," << degreesZ << std::endl;
  rotate(degreesZ, Vec3d(0,0,1));
  rotate(degreesY, Vec3d(0,1,0));
  rotate(degreesX, Vec3d(1,0,0));
}

void View::setScale(float x, float y, float z, bool replace)
{
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
}

std::string View::zoom(float factor)
{
  float adj = factor * model_size;
  model_trans[2] += adj;
  std::ostringstream ss;
  ss << "translate z " << adj;
  return ss.str();
}

std::string View::zoomClip(float factor)
{
  near_clip += factor * model_size;
  if (near_clip < model_size * 0.001) near_clip = model_size * 0.001; //Bounds check
  //debug_print("Near clip = %f\n", near_clip);
  std::ostringstream ss;
  ss << "nearclip " << near_clip;
  return ss.str();
}

void View::reset()
{
  for (int i=0; i<3; i++)
  {
    model_trans[i] = 0;
    //Default focal point and rotation centre = model bounding box centre point
    //if (focal_point[i] == FLT_MIN)
    //   focal_point[i] = min[i] + dims[i] / 2.0f;
    rotate_centre[i] = focal_point[i];
  }
  rotation->identity();
  rotated = true;   //Flag rotation
}

void View::print()
{
  float xrot, yrot, zrot;
  rotation->toEuler(xrot, yrot, zrot);
  printf("------------------------------\n");
  printf("Viewport %d,%d %d x %d\n", xpos, ypos, width, height);
  printf("Clip planes: near %f far %f\n", near_clip, far_clip);
  printf("Model size %f dims: %f,%f,%f - %f,%f,%f (scale %f,%f,%f)\n",
         model_size, min[0], min[1], min[2], max[0], max[1], max[2], scale[0], scale[1], scale[2]);
  printf("Focal Point %f,%f,%f\n", focal_point[0], focal_point[1], focal_point[2]);
  printf("------------------------------\n");
  printf("%s\n", translateString().c_str());
  printf("%s\n", rotateString().c_str());
  printf("------------------------------\n");
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
  //assert(near_clip != far_clip);

  // Perspective viewing frustum parameters
  float left, right, top, bottom;
  float eye_separation, frustum_shift;

  //Ensure clip planes valid
  checkClip();

  //This is zero parallax distance, objects closer than this will appear in front of the screen,
  //default is to set to distance to model front edge...
  float focal_length = fabs(model_trans[2]) - model_size * 0.5; //3/4 of model size - distance from eye to model centre
  focal_length += focal_length_adj;   //Apply user adjustment (default 0)
  if (focal_length < 0) focal_length = 0.1;

  //Calculate eye separation based on focal length
  eye_separation = focal_length * eye_sep_ratio;

  // Build the viewing frustum
  // Top of frustum calculated from field of view (aperture) and near clipping plane, camera->aperture = fov in degrees
  top = tan(0.5f * DEG2RAD * fov) * near_clip;
  bottom = -top;
  // Account for aspect ratio (width/height) to get right edge of frustum
  right = aspectRatio * top;
  left = -right;

  //Shift frustum to the left/right to account for right/left eye viewpoint
  frustum_shift = eye * 0.5 * eye_separation * fabs(near_clip / focal_length);  //Mutiply by eye (-1 left, 0 none, 1 right)
  //Viewport eye shift in pixels => for raycasting shader
  eye_shift = eye * eye_sep_ratio * height * 0.6 / tan(DEG2RAD * fov);

  // In Stereo, View vector for each camera is parallel
  // Need to adjust eye position and focal point to left/right to account for this...
  //Shift model by half of eye-separation in opposite direction of eye (equivalent to camera shift in same direction as eye)
  scene_shift = eye * eye_separation * -0.5f;

  if (eye) debug_print("STEREO %s: focalLen: %f eyeSep: %f frustum_shift: %f, scene_shift: %f eye_shift %f\n", (eye < 0 ? "LEFT (RED)  " : "RIGHT (BLUE)"),
                         focal_length, eye_separation, frustum_shift, scene_shift, eye_shift);
  //debug_print(" Ratio %f Left %f Right %f Top %f Bottom %f Near %f Far %f\n",
  //         aspectRatio, left, right, bottom, top, near_clip, far_clip);

  // Set up our projection transform
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(left - frustum_shift, right - frustum_shift, bottom, top, near_clip, far_clip);

  // Return to model view
  glMatrixMode(GL_MODELVIEW);
  GL_Error_Check;
}

void View::apply(bool use_fp)
{
  // Right-handed (GL default) or Left-handed
  int orientation = properties["coordsystem"];
  if (properties["globalcam"])
  {
    if (!globalcam) 
      globalcam = new Camera(localcam);

    //Global camera override
    rotate_centre = globalcam->rotate_centre;
    focal_point = globalcam->focal_point;
    model_trans = globalcam->model_trans;
    rotation = &globalcam->rotation;
  }
  else
  {
    //Use the local cam by default
    rotate_centre = localcam->rotate_centre;
    focal_point = localcam->focal_point;
    model_trans = localcam->model_trans;
    rotation = &localcam->rotation;
  }

  GL_Error_Check;
  // Setup view transforms
  glMatrixMode(GL_MODELVIEW);
#ifndef USE_OMEGALIB
  glLoadIdentity();
  GL_Error_Check;

  // Translate to cancel stereo parallax
  glTranslatef(scene_shift, 0.0, 0.0);
  GL_Error_Check;

  // Translate model away from eye by camera zoom/pan translation
  //debug_print("APPLYING VIEW '%s': trans %f,%f,%f\n", title.c_str(), model_trans[0], model_trans[1], model_trans[2]);
  glTranslatef(model_trans[0]*scale[0], model_trans[1]*scale[0], model_trans[2]*scale[0]);
  GL_Error_Check;
#endif

  // Adjust centre of rotation, default is same as focal point so this does nothing...
  float adjust[3] = {(focal_point[0]-rotate_centre[0])*scale[0], (focal_point[1]-rotate_centre[1])*scale[1], (focal_point[2]-rotate_centre[2])*scale[2]};
  if (use_fp) glTranslatef(-adjust[0], -adjust[1], -adjust[2]);
  GL_Error_Check;

  // rotate model
  rotation->apply();
  GL_Error_Check;

  // Adjust back for rotation centre
  if (use_fp) glTranslatef(adjust[0], adjust[1], adjust[2]);
  GL_Error_Check;

  // Translate to align eye with model centre - view focal point
  //glTranslatef(-rotate_centre[0], -rotate_centre[1], -rotate_centre[2]);
  if (use_fp) glTranslatef(-focal_point[0]*scale[0], -focal_point[1]*scale[1], orientation * -focal_point[2]*scale[2]);
  GL_Error_Check;

  // Switch coordinate system if applicable
  glScalef(1.0, 1.0, 1.0 * orientation);
  GL_Error_Check;

  // Apply scaling factors
  if (scale[0] != 1.0 || scale[1] != 1.0 || scale[2] != 1.0)
  {
    glScalef(scale[0], scale[1], scale[2]);
    // Enable automatic rescaling of normal vectors when scaling is turned on
    //glEnable(GL_RESCALE_NORMAL);
    glEnable(GL_NORMALIZE);
  }
  GL_Error_Check;

  // Set default polygon front faces
  if (orientation == RIGHT_HANDED)
    glFrontFace(GL_CCW);
  else
    glFrontFace(GL_CW);
  GL_Error_Check;
}

bool View::scaleSwitch()
{
  static float save_scale[3];
  if (scaled)
  {
    save_scale[0] = scale[0];
    save_scale[1] = scale[1];
    save_scale[2] = scale[2];
    scale[0] = 1.0;
    scale[1] = 1.0;
    scale[2] = 1.0;
    scaled = false;
  }
  else
  {
    scale[0] = save_scale[0];
    scale[1] = save_scale[1];
    scale[2] = save_scale[2];
    scaled = true;
  }
  return scaled;
}

int View::direction()
{
  //Returns 1 or -1 multiplier representing direction of viewer
  return model_trans[2] > 0 ? -1 : 1;
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
void View::zoomToFit(int margin)
{
  if (margin < 0) margin = properties["margin"];

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
  GLfloat modelView[16];
  GLfloat projection[16];
  int viewport[4];
  int count = 0;
  double error = 1, scale2d, adjust = ADJUST;
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
    glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
    for (i = 0; i < 8; i++)
    {
      gluProjectf(rect3d[i][0], rect3d[i][1], rect3d[i][2],
                  modelView, projection, viewport, &win3d[i][0]);
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
      if (xscale < yscale) scale2d = xscale;
      else scale2d = yscale;
    }

    // debug_print("BB new_min_x %f new_max_x %f === ", new_min_x, new_max_x);
    //   debug_print("Bounding rect: %f,%f - %f,%f === ",  min_x, min_y, max_x, max_y);
    //   debug_print(" Min rect: 0,0 - %f,%f\n", max_x - min_x, max_y - min_y);

    //Self-adjusting: did we overshoot aim? - compare last error to current 2d scale
    if (count > 0)
    {
      if ((error > 0 && scale2d < 1.0) ||
          (error < 0 && scale2d > 1.0))
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
    error = (scale2d - 1.0) / scale2d;

    //Adjust zoom translation by error * adjust (0.5 default)
    //float oldz = model_trans[2];
    model_trans[2] -= (model_trans[2] * error * adjust);
    //if (count > 4) {
    //   debug_print("[%d iterations] ... 2D Scaling factor %f ", count, scale2d);
    //   debug_print(" --> error: %f (adjust %f) zoom from %f to %f\n", error, adjust, oldz, model_trans[2]);
    //}
    count++;
  }
}

void View::drawOverlay(Colour& colour)
{
#ifdef PDF_CAPTURE
  return;   //Skip overlay
#endif
  //2D overlay objects, apply text scaling
  Viewport2d(width, height);
  glScalef(scale2d, scale2d, scale2d);
  int w = width / scale2d;
  int h = height / scale2d;

  //Colour bars
  GL_Error_Check;
  float last_y = 0;
  float last_margin = 0;
  for (unsigned int i=0; i<objects.size(); i++)
  {
    //Only when flagged as colour bar
    ColourMap* cmap = objects[i]->getColourMap();
    //Use the first available colourmap by default
    if (!cmap && objects[i]->colourMaps && objects[i]->colourMaps->size() > 0)
      cmap = (*objects[i]->colourMaps)[0];
    if (!objects[i] || !objects[i]->properties["colourbar"] ||
        !objects[i]->properties["visible"] || !cmap) continue;
    int pos = objects[i]->properties["position"];
    std::string align = objects[i]->properties["align"];
    int ww = w, hh = h;
    bool vertical = false;
    bool opposite = (align == "left" || align == "bottom");
    if (align == "left" || align == "right")
    {
      vertical = true;
      ww = h;
      hh = w;
    }

    int margin = objects[i]->properties["margin"];
    int length = ww * (float)objects[i]->properties["lengthfactor"];
    int bar_height = objects[i]->properties.getInt("width", 10); //Conflict with shape width
    int startx = (ww - length) / 2;
    if (pos == 1) startx = margin;
    if (pos == -1) startx = ww - margin - length;
    int starty = margin;
    if (last_margin == margin) starty += last_y + margin;   //Auto-increase y margin if same value
    if (!opposite) starty = hh - starty - bar_height;
    //float border = properties["border", 1.0);
    //if (border > 0) glLineWidth(border*textscale*0.75); else glLineWidth(textscale*0.75);

    std::string font = objects[i]->properties["font"];
    if (textscale && font != "vector")
      objects[i]->properties.data["font"] = "vector"; //Force vector font if downsampling

    last_y = starty;
    last_margin = margin;

    cmap->draw(objects[i]->properties, startx, starty, length, bar_height, colour, vertical);
    GL_Error_Check;
  }

  GL_Error_Check;

  //Title
  std::string title = properties["title"];
  if (title.length())
  {
    //Timestep macro ##
    size_t pos =  title.find("##");
    if (pos != std::string::npos && TimeStep::timesteps.size() >= Model::now)
      title.replace(pos, 2, std::to_string(TimeStep::timesteps[Model::now]->step));
    float fontscale = PrintSetFont(properties, "vector", 1.0);
    if (fontscale < 0)
      lucSetFontScale(fabs(fontscale)*0.6); //Scale down vector font slightly for title
    Print(0.5 * (w - PrintWidth(title.c_str())), h - 3 - PrintWidth("W"), title.c_str());

  }

  GL_Error_Check;
  //Restore 3d
  Viewport2d(0, 0);
  GL_Error_Check;
}

