class DrawState;

#ifndef DrawState__
#define DrawState__

#include "TimeStep.h"

#define PROPDEFAULT 0
#define PROPTARGET  1
#define PROPTYPE    2
#define PROPDOC     3

//Class to hold global draw state
class DrawState
{
public:
  //Properties
  json globals;
  json defaults;

  bool automate;
  //Set when called from omegalib, camera handled externally
  bool omegalib;

  //Model
  int now;

  DrawingObject* borderobj;
  DrawingObject* axisobj;
  DrawingObject* rulerobj;

  //Mutex for thread safe updates
  std::mutex mutex;

  //TimeStep
  std::vector<TimeStep*> timesteps; //Active model timesteps
  int gap;

  //Geometry
  float min[3], max[3], dims[3];
  float *x_coords, *y_coords;  // Saves arrays of x,y points on circle for set segment count
  int segments = 0;    // Saves segment count for circle based objects

  //TriSurfaces, Lines, Points, Volumes
  Shader* prog[lucMaxType];

  //View
  Camera* globalcam = NULL;
  float scale2d = 1.0;

  //Fonts
  FontManager fonts;

  //Global colourmaps list for active model
  std::vector<ColourMap*> * colourMaps;

  //List of properties that apply to colourMaps, views
  std::vector<std::string> colourMapProps;
  std::vector<std::string> viewProps;

  //Property metadata / documentation
  std::vector<std::pair<std::string,json>> properties;

  // Engines - mersenne twister
  std::mt19937 eng0, eng1;
  std::uniform_real_distribution<float> dist;

  DrawState();
  ~DrawState();
  std::string counterFilename();
  void reset();
  void init();
  json& global(const std::string& key);
  bool has(const std::string& key);
  void cacheCircleCoords(int segment_count);

  float random() {return dist(eng0);}
  float random_d() {return dist(eng1);}

};

#endif // DrawState__

