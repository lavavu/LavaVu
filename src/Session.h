class Session;

#ifndef Session__
#define Session__

#include "TimeStep.h"
#include "GraphicsUtil.h"
#include "Util.h"
#include "Shaders.h"
#include "View.h"
#include "RenderContext.h"

//Class to hold global draw state
class Session
{
public:
  //Properties
  json globals;
  json defaults;

  bool automate;
  //Context provided by user
  bool havecontext;

  //Model
  int now;
  int zlib_compression = Z_BEST_SPEED;

  //Mutex for thread safe updates
  std::mutex mutex;

  //TimeStep
  std::vector<TimeStep*> timesteps; //Active model timesteps
  int gap;

  //Geometry
  float min[3], max[3], dims[3];
  float *x_coords, *y_coords;  // Saves arrays of x,y points on circle for set segment count
  int segments = 0;    // Saves segment count for circle based objects

  //Shaders by geometry type
  Shader_Ptr shaders[lucMaxType];

  //View
  Camera* globalcam = NULL;

  //Render state
  RenderContext context;

  //Fonts
  FontManager fonts;

  //Global colourmaps list for active model
  std::vector<ColourMap*> colourMaps;

  //List of properties that apply to colourMaps, views
  std::vector<std::string> colourMapProps;
  std::vector<std::string> viewProps;

  std::map<std::string, std::string> classMap;
  std::map<std::string, lucGeometryType> typeMap;

  //Property metadata / documentation
  json_fifo properties;

  // Engines - mersenne twister
  std::mt19937 eng0, eng1;
  std::uniform_real_distribution<float> dist;

  Session();
  ~Session();
  void destroy();
  std::string counterFilename();
  void reset();
  int parse(Properties* target, const std::string& property);
  void parseSet(Properties& target, const std::string& properties);
  void init(std::string& binpath);
  json& global(const std::string& key);
  bool has(const std::string& key);
  void cacheCircleCoords(int segment_count);

  float random() {return dist(eng0);}
  float random_d() {return dist(eng1);}
};

#endif // Session__

