#include "Session.h"

Session::Session() : shaders(), eng0(std::random_device()()), eng1(0), dist(0, 1)
{
  havecontext = false;
  segments = 0;    // Saves segment count for circle based objects
  //reset();
  defaults = json::object();
  // Saves arrays of x,y points on circle for set segment count
  x_coords = NULL;
  y_coords = NULL;
}

Session::~Session()
{
  destroy();
}

void Session::destroy()
{
  if (fonts.context)
    fonts.clear();

  if (globalcam) delete globalcam;
  globalcam = NULL;

  if (x_coords != NULL) delete[] x_coords;
  if (y_coords != NULL) delete[] y_coords;
  x_coords = NULL;
  y_coords = NULL;
}

std::string Session::counterFilename()
{
  //Apply image counter to default filename when multiple images/videos output
  std::string title = global("caption");
  std::stringstream outpath;
  std::string filename;
  outpath << title;
  if (now >= 0 && (int)timesteps.size() > now)
    outpath << "-" << std::setfill('0') << std::setw(5) << timesteps[now]->step;
  filename = outpath.str();
  int counter = 1;
  while (FileExists(filename + ".png"))
  {
    std::stringstream outpath2;
    outpath2 << outpath.str() << "-" << std::setfill('0') << std::setw(5) << counter;
    filename = outpath2.str();
    counter++;
  }
  return filename;
}

void Session::reset()
{
  timesteps.clear();

  automate = false;

  now = -1;
  gap = 1;
  context.scale2d = 1.0;

  for (int i=0; i<3; i++)
  {
    min[i] = HUGE_VALF;
    max[i] = -HUGE_VALF;
    dims[i] = 0;
  }

  colourMaps.clear();
}

int Session::parse(Properties* target, const std::string& property)
{
  //Parse a key=value property where value is a json object
  if (property.length() < 2) return 0; //Require at least "x="

  //If Properties object provided, parse into its data, otherwise into globals
  json& dest = target ? target->data : globals;

  std::string key, value;
  size_t pos = property.find("=");
  key = property.substr(0,pos);
  //Ignore case
  std::transform(key.begin(), key.end(), key.begin(), ::tolower);

  //Parse simple increments and decrements
  int end = key.length()-1;
  char prev = key.at(end);
  if (prev == '+' || prev == '-' || prev == '*')
  {
    key = key.substr(0,end);
  }

  //Get value
  value = property.substr(pos+1);

  //Support array property set, eg: rotate[1]=30
  int IDX = -1;
  if (key.back() == ']')
  {
    size_t pos0 = property.find("[");
    std::string idx = key.substr(pos0+1,key.length()-2-pos0);
    std::stringstream ss(idx);
    ss >> IDX;
    key = key.substr(0,pos0);
    //std::cout << "KEY : " << key << " VALUE : " << value << " IDX : " << IDX << std::endl;
  }

  //Check a valid key provided
  bool strict = false;
  bool validate = global("validate");
  int redraw = 0;
  if (properties.count(key) == 0)
  {
    //Validation of names, ensures typos etc cause errors
    if (validate)
    {
      std::cerr << key << " : Invalid property name" << std::endl;
      return 0;
    }
  }
  else
  {
    //Get metadata
    json prop = properties[key];
    strict = prop["strict"];
    redraw = prop["redraw"];
  }

  if (value.length() == 0)
  {
    //Clear property
    dest.erase(key);
    return redraw;
  }
  //std::cerr << "Key " << key << " == " << value << std::endl;
  std::string valuel = value;
  std::transform(valuel.begin(), valuel.end(), valuel.begin(), ::tolower);

  // Parse without exceptions - required for emscripten
  if (IDX >= 0)
  {
    //Set to default first
    if (!dest.count(key) || dest[key].is_null())
      dest[key] = defaults[key];
    if (dest[key].is_array() && (int)dest[key].size() > IDX)
    {
      json parsedval = json::parse(value, nullptr, false);
      if (!parsedval.is_discarded())
        dest[key][IDX] = parsedval;
    }
  }
  //Parse simple increments and decrements
  else if (prev == '+' || prev == '-' || prev == '*')
  {
    json parsedval = json::parse(value, nullptr, false);
    if (!parsedval.is_discarded())
    {
      float val = dest[key];
      if (prev == '+')
        val = val + (float)parsedval;
      else if (prev == '-')
        val = val - (float)parsedval;
      else if (prev == '*')
        val = val * (float)parsedval;

      //Keep as int unless either side is float
      if (dest[key].is_number_float() || parsedval.is_number_float())
        dest[key] = val;
      else
        dest[key] = (int)val;
    }
  }
  else if (valuel == "true")
  {
    dest[key] = true;
  }
  else if (valuel == "false")
  {
    dest[key] = false;
  }
  else if (valuel == "null")
  {
    dest.erase(key);
  }
  else
  {
    json parsedval = json::parse(value, nullptr, false);
    if (parsedval.is_discarded())
    {
      //std::cerr << e.what() << " : '" << key << "' => " << value << std::endl;
      //Treat as a string value
      dest[key] = value;
    }
    else
    {
      dest[key] = parsedval;
      //std::cerr << value << " : '" << key << "' => " << dest[key] << std::endl;
    }
  }

  //if (!validate)
  //  return 0;

  //Run a type check
  //checkall(strict);
  Properties::check(dest, defaults, strict);

  return redraw;
}

//Parse multi-line string
void Session::parseSet(Properties& target, const std::string& properties)
{
  //Properties can be provided as valid json object {...}
  //in which case, parse directly
  if (properties.length() > 1 && properties.at(0) == '{')
  {
    json props = json::parse(properties);
    target.merge(props);
  }
  //Otherwise, provided as single prop=value per line
  //where value is a parsable as json
  else
  {
    std::stringstream ss(properties);
    std::string line;
    while (std::getline(ss, line))
      parse(&target, line);
  }
}

void Session::init(std::string& binpath)
{
  //Load the properties dictionary from external file
  //If dict.json found locally in working directory, use it instead
  std::string fname = "dict.json";
  std::string filepath;
  if (!FileExists(fname))
    filepath = binpath;

  filepath += fname;
  debug_print("Property dictionary loading: %s\n", filepath.c_str());

  std::ifstream ifs(filepath);
  std::stringstream buffer;
  if (ifs.is_open())
    buffer << ifs.rdbuf();
  else
    std::cerr << "Error opening property dictionary: " << filepath << std::endl;
  properties = json_fifo::parse(buffer.str());

  //Add built-ins
  viewProps.push_back("is3d");
  viewProps.push_back("bounds");
  for (auto it = properties.begin(); it != properties.end(); ++it)
  {
    //std::cout << it.key() << " | " << it.value() << "\n";
    json_fifo& entry = it.value();
    std::string key = it.key();
    //Save view properties
    std::string target = entry["target"];
    //if (target.find("view") != std::string::npos)
    if (target == "view")
      viewProps.push_back(key);

    //Save colourmap properties
    if (target.find("colourmap") != std::string::npos)
      colourMapProps.push_back(key);

    //Save defaults
    defaults[key] = entry["default"];
  }

  //Load the geometry class map
  //classMap: map of renderer aliases to actual renderer names
  //typeMap: map of renderer aliases to primitive types
  json r = properties["renderers"]["default"];
  lucGeometryType t = lucMinType;
  for (auto g : r)
  {
    for (auto it = g.begin(); it != g.end(); ++it)
    {
      classMap[it.key()] = it.value();
      typeMap[it.key()] = t;
    }
    t = (lucGeometryType)((int)t + 1); //Next type
  }


#ifdef DEBUG
  //std::cerr << std::setw(2) << properties << std::endl;
  //std::cerr << std::setw(2) << defaults << std::endl;
#endif
}

//Return a global parameter (or default if not set)
json& Session::global(const std::string& key)
{
  if (has(key)) return globals[key];
  return defaults[key];
}

//Return true if a global parameter has been set
bool Session::has(const std::string& key)
{
  return (globals.count(key) > 0 && !globals[key].is_null());
}

// Calculates a set of points on a unit circle for a given number of segments
// Used to optimised rendering circular objects when segment count isn't changed
void Session::cacheCircleCoords(int segment_count)
{
  // Recalc required? Only done first time called and when segment count changes
  if (segment_count == 0 || segments == segment_count) return;
  GLfloat angle;
  float angle_inc = 2*M_PI / (float)segment_count;
  int idx;

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

