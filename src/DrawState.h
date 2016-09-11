#ifndef DrawState__
#define DrawState__

#include "TimeStep.h"

//Class to hold global draw state
class DrawState
{
public:
//TimeStep
  std::vector<TimeStep*> timesteps; //Active model timesteps
  int gap;

//Geometry
  float min[3], max[3], dims[3];
  float *x_coords_ = NULL, *y_coords_ = NULL;  // Saves arrays of x,y points on circle for set segment count
  int segments__ = 0;    // Saves segment count for circle based objects

//GeomData
  std::array<std::string, lucMaxType> names;

//TriSurfaces, Lines, Points, Volumes
  Shader* prog;
//Lines
  bool tubes;
//Points
  GLuint indexvbo, vbo;

//Properties
  json globals;
  json defaults;

//Font?!!
//

  DrawState()
  {
    gap = 0;

    for (int i=0; i<3; i++)
    {
      min[i] = HUGE_VALF;
      max[i] = -HUGE_VALF;
      dims[i] = 0;
    }

    names = {"labels", "points", "quads", "triangles", "vectors", "tracers", "lines", "shapes", "volume"};

    // Saves arrays of x,y points on circle for set segment count
    x_coords_ = NULL;
    y_coords_ = NULL;
    segments__ = 0;    // Saves segment count for circle based objects

    prog = NULL;

    defaults["default"] = false; //Fallback value
    if (globals.is_null()) globals = json::object();

  }
};

#endif // DrawState__

