class DrawState;

#ifndef DrawState__
#define DrawState__

#include "TimeStep.h"

//Class to hold global draw state
class DrawState
{
public:
//Properties
  json globals;
  json defaults;

//TimeStep
  std::vector<TimeStep*> timesteps; //Active model timesteps
  int gap;

//Geometry
  float min[3], max[3], dims[3];
  float *x_coords, *y_coords;  // Saves arrays of x,y points on circle for set segment count
  int segments = 0;    // Saves segment count for circle based objects


//TriSurfaces, Lines, Points, Volumes
  Shader* prog[lucMaxType];
//Points
  GLuint pindexvbo, pvbo;

//View
  Camera* globalcam = NULL;

//Font?!!
//

  DrawState() : prog()
  {
    reset();
  }

  void reset()
  {
    defaults["default"] = false; //Fallback value
    if (globals.is_null()) globals = json::object();

    gap = 0;

    for (int i=0; i<3; i++)
    {
      min[i] = HUGE_VALF;
      max[i] = -HUGE_VALF;
      dims[i] = 0;
    }

    // Saves arrays of x,y points on circle for set segment count
    x_coords = NULL;
    y_coords = NULL;
    segments = 0;    // Saves segment count for circle based objects

    pindexvbo = 0;
    pvbo = 0;
  }

  json& global(const std::string& key)
  {
    if (globals.count(key) > 0 && !globals[key].is_null()) return globals[key];
    return defaults[key];
  }

  // Calculates a set of points on a unit circle for a given number of segments
  // Used to optimised rendering circular objects when segment count isn't changed
  void cacheCircleCoords(int segment_count)
  {
    // Recalc required? Only done first time called and when segment count changes
    GLfloat angle;
    float angle_inc = 2*M_PI / (float)segment_count;
    int idx;
    if (segments == segment_count) return;

    // Calculate unit circle points when divided into specified segments
    // and store in static variable to re-use every time a vector with the
    // same segment count is drawn
    segments = segment_count;
    if (x_coords != NULL) delete[] x_coords;
    if (y_coords != NULL) delete[] y_coords;

    x_coords = new float[segment_count + 1];
    y_coords = new float[segment_count + 1];

    // Loop around in a circle and specify even points along the circle
    // as the vertices for the triangle fan cone, cone base and arrow shaft
    for (idx = 0; idx <= segments; idx++)
    {
      angle = angle_inc * (float)idx;
      // Calculate x and y position of the next vertex and cylinder normals (unit circle coords)
      x_coords[idx] = sin(angle);
      y_coords[idx] = cos(angle);
    }
  }
};

#endif // DrawState__

