#include "Session.h"

Session::Session() : shaders(), eng0(std::random_device()()), eng1(0), dist(0, 1)
{
  borderobj = axisobj = rulerobj = NULL;
  omegalib = false;
  segments = 0;    // Saves segment count for circle based objects
  //reset();
  defaults = json::object();
  // Saves arrays of x,y points on circle for set segment count
  x_coords = NULL;
  y_coords = NULL;
}

Session::~Session()
{
  fonts.reset();

  if (globalcam) delete globalcam;

  if (x_coords != NULL) delete[] x_coords;
  if (y_coords != NULL) delete[] y_coords;
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
  globals = json::object();

  automate = false;

  if (borderobj) delete borderobj;
  if (axisobj) delete axisobj;
  if (rulerobj) delete rulerobj;
  borderobj = axisobj = rulerobj = NULL;

  now = -1;
  gap = 1;
  scale2d = 1.0;

  for (int i=0; i<3; i++)
  {
    min[i] = HUGE_VALF;
    max[i] = -HUGE_VALF;
    dims[i] = 0;
  }

  colourMaps = NULL;
}

int Session::parse(Properties* target, const std::string& property, bool validate)
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

  //Check a valid key provided
  bool strict = false;
  int redraw = 0;
  if (properties.count(key) == 0)
  {
    //Strict validation of names, ensures typos etc cause errors
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
    strict = prop[PROPSTRICT];
    redraw = prop[PROPREDRAW];
  }

  value = property.substr(pos+1);
  if (value.length() > 0)
  {
    //std::cerr << "Key " << key << " == " << value << std::endl;
    std::string valuel = value;
    std::transform(valuel.begin(), valuel.end(), valuel.begin(), ::tolower);

    try
    {
      //Parse simple increments and decrements
      if (prev == '+' || prev == '-' || prev == '*')
      {
        json parsedval = json::parse(value);
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
      else if (valuel == "true")
      {
        dest[key] = true;
      }
      else if (valuel == "false")
      {
        dest[key] = false;
      }
      else
      {
        dest[key] = json::parse(value);
      }
    }
    catch (std::exception& e)
    {
      //std::cerr << e.what() << " : '" << key << "' => " << value << std::endl;
      //Treat as a string value
      dest[key] = value;
    }
  }

  if (!validate)
    return 0;

  //Run a type check
  //checkall(strict);
  Properties::check(dest, defaults, strict);

  return redraw;
}

//Parse multi-line string
void Session::parseSet(Properties& target, const std::string& properties, bool validate)
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
      parse(&target, line, validate);
  }
}

void Session::init()
{
  //1) Default value
  //2) Target of property
  //3) Doc string
  //4) Strict parsing flag
  //5) Redraw level 0-4 (none, redraw only, full data reload, full reload and view reset, full reload and view reset with autozoom)

  //Setup default properties (once only)
  std::vector<std::pair<std::string,json>> property_data =
  {
    {
      "renderlist",
      {
        "sortedtriangles quads vectors tracers shapes sortedpoints labels lines volume screen",
        "all",
        "string",
        "List of renderers created and order they are displayed. Valid types are: volume,triangles,sortedtriangles,quads,vectors,tracers,shapes,points,sortedpoints,labels,lines,links",
        true, 0
      }
    },
    {
      "glyphrenderlist",
      {
        "sortedtriangles points lines",
        "all",
        "string",
        "List of renderers created for glyph rendering and order they are displayed.",
        true, 0
      }
    },
    {
      "font",
      {
        "vector",
        "all",
        "string",
        "Font typeface vector/small/fixed/sans/serif",
        true, 0
      }
    },
    {
      "fontscale",
      {
        1.0,
        "all",
        "real",
        "Font scaling, note that only the 'vector' font scales well",
        true, 0
      }
    },
    {
      "fontcolour",
      {
        {0,0,0,0},
        "all",
        "colour",
        "Font colour RGB(A)",
        true, 0
      }
    },
    {
      "name",
      {
        "",
        "object",
        "string",
        "Name of object",
        true, 0
      }
    },
    {
      "visible",
      {
        true,
        "object",
        "boolean",
        "Set to false to hide object",
        true, 0
      }
    },
    {
      "geometry",
      {
        "points",
        "object",
        "string",
        "Geometry type to load when adding new data",
        true, 0
      }
    },
    {
      "shaders",
      {
        json::array(),
        "object",
        "object",
        "Custom shaders for rendering object, either filenames or source strings, provide either [fragment], [vertex, fragment] or [geometry, vertex, fragment]",
        true, 0
      }
    },
    {
      "uniforms",
      {
        json::array(),
        "object",
        "object",
        "Custom shader uniforms for rendering objects, list of uniform names, will be copied from property data",
        true, 0
      }
    },
    {
      "steprange",
      {
        true,
        "object",
        "boolean",
        "Calculate dynamic range values per step rather than over full time range",
        true, 3
      }
    },
    {
      "lit",
      {
        true,
        "object",
        "boolean",
        "Apply lighting to object",
        true, 0
      }
    },
    {
      "cullface",
      {
        false,
        "object",
        "boolean",
        "Cull back facing polygons of object surfaces",
        true, 0
      }
    },
    {
      "wireframe",
      {
        false,
        "object",
        "boolean",
        "Render object surfaces as wireframe",
        true, 0
      }
    },
    {
      "flat",
      {
        false,
        "object",
        "boolean",
        "Renders surfaces as flat shaded, lines/vectors/tracers as 2d, faster but no 3d or lighting",
        true, 0
      }
    },
    {
      "depthtest",
      {
        true,
        "object",
        "boolean",
        "Set to false to disable depth test when drawing object so always drawn regardless of 3d position",
        true, 0
      }
    },
    {
      "dims",
      {
        {0,0,0},
        "object",
        "integer[3]",
        "width/height/depth override for geometry",
        true, 0
      }
    },
    {
      "rotatable",
      {
        false,
        "object",
        "boolean",
        "Set to true to apply the view rotation to this object",
        true, 0
      }
    },
    {
      "shift",
      {
        0.,
        "object",
        "real",
        "Apply a shift to object position by this amount multiplied by model size, to fix depth fighting when visualising objects drawn at same depth",
        true, 0
      }
    },
    {
      "colour",
      {
        {0,0,0,255},
        "object",
        "colour",
        "Object colour RGB(A)",
        true, 2
      }
    },
    {
      "colourmap",
      {
        "",
        "object",
        "string",
        "name of the colourmap to use",
        false, 2
      }
    },
    {
      "opacitymap",
      {
        "",
        "object",
        "string",
        "name of the opacity colourmap to use",
        false, 2
      }
    },
    {
      "opacity",
      {
        1.0,
        "object",
        "real [0,1]",
        "Opacity of object where 0 is transparent and 1 is opaque",
        true, 2
      }
    },
    {
      "brightness",
      {
        0.0,
        "object",
        "real [-1,1]",
        "Brightness of object from -1 (full dark) to 0 (default) to 1 (full light)",
        true, 0
      }
    },
    {
      "contrast",
      {
        1.0,
        "object",
        "real [0,2]",
        "Contrast of object from 0 (none, grey) to 2 (max)",
        true, 0
      }
    },
    {
      "saturation",
      {
        1.0,
        "object",
        "real [0,2]",
        "Saturation of object from 0 (greyscale) to 2 (fully saturated)",
        true, 0
      }
    },
    {
      "ambient",
      {
        0.4,
        "object",
        "real [0,1]",
        "Ambient lighting level (background constant light)",
        true, 0
      }
    },
    {
      "diffuse",
      {
        0.65,
        "object",
        "real [0,1]",
        "Diffuse lighting level (shading light/dark)",
        true, 0
      }
    },
    {
      "specular",
      {
        0.0,
        "object",
        "real [0,1]",
        "Specular highlight lighting level (spot highlights)",
        true, 0
      }
    },
    {
      "lightpos",
      {
        {0.1,-0.1,2.0},
        "object",
        "real[3]",
        "Light position X Y Z relative to camera position (follows camera)",
        true, 0
      }
    },
    {
      "clip",
      {
        true,
        "object",
        "boolean",
        "Allow object to be clipped",
        true, 0
      }
    },
    {
      "clipmap",
      {
        true,
        "object",
        "boolean",
        "Clipping mapped to range normalised [0,1]",
        true, 0
      }
    },
    {
      "xmin",
      {
        -FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, minimum x",
        true, 0
      }
    },
    {
      "ymin",
      {
        -FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum y",
        true, 0
      }
    },
    {
      "zmin",
      {
        -FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, minimum z",
        true, 0
      }
    },
    {
      "xmax",
      {
        FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum x",
        true, 0
      }
    },
    {
      "ymax",
      {
        FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum y",
        true, 0
      }
    },
    {
      "zmax",
      {
        FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum z",
        true, 0
      }
    },
    {
      "filters",
      {
        json::array(),
        "object",
        "object",
        "Filter list",
        true, 1
      }
    },
    {
      "glyphs",
      {
        2,
        "object",
        "integer [0,n]",
        "Glyph quality 0=none, 1=low, higher=increasing triangulation detail (arrows/shapes etc)",
        true, 0
      }
    },
    {
      "scaling",
      {
        1.0,
        "object",
        "real",
        "Object scaling factor",
        true, 0
      }
    },
    {
      "texture",
      {
        "",
        "object",
        "string",
        "Apply a texture, either external texture image file path to load or colourmap",
        true, 0
      }
    },
    {
      "fliptexture",
      {
        true,
        "object",
        "boolean",
        "Flip texture image after loading, usually required",
        true, 0
      }
    },
    {
      "repeat",
      {
        false,
        "object",
        "boolean",
        "Repeat texture image enabled, default is clamp to edges",
        true, 0
      }
    },
    {
      "colourby",
      {
        0,
        "object",
        "string or integer",
        "Index or label of data set to colour object by (requires colour map)",
        false, 2
      }
    },
    {
      "opacityby",
      {
        "",
        "object",
        "string or integer",
        "Index or label of data set to apply transparency to object by (requires opacity map)",
        false, 2
      }
    },
    {
      "limit",
      {
        0,
        "object(line)",
        "real",
        "Line length limit, can be used to skip drawing line segments that cross periodic boundary",
        true, 0
      }
    },
    {
      "link",
      {
        false,
        "object(line)",
        "boolean",
        "To chain line vertices into longer lines set to true",
        true, 0
      }
    },
    {
      "scalelines",
      {
        1.0,
        "object(line)",
        "real",
        "Line scaling multiplier, applies to all line objects",
        true, 0
      }
    },
    {
      "linewidth",
      {
        1.0,
        "object(line)",
        "real",
        "Line width scaling",
        true, 0
      }
    },
    {
      "tubes",
      {
        false,
        "object(line)",
        "boolean",
        "Draw lines as 3D tubes",
        true, 0
      }
    },
    {
      "pointsize",
      {
        1.0,
        "object(point)",
        "real",
        "Point size scaling",
        true, 2
      }
    },
    {
      "pointtype",
      {
        1,
        "object(point)",
        "integer/string",
        "Point type 0/blur, 1/smooth, 2/sphere, 3/shiny, 4/flat",
        true, 0
      }
    },
    {
      "scalepoints",
      {
        1.0,
        "object(point)",
        "real",
        "Point scaling multiplier, applies to all points objects",
        true, 0
      }
    },
    {
      "sizeby",
      {
        "",
        "object(point)",
        "string or integer",
        "Index or label of data set to apply to point sizes",
        false, 2
      }
    },
    {
      "opaque",
      {
        false,
        "object(surface)",
        "boolean",
        "If opaque flag is set skips depth sorting step and allows individual surface properties to be applied",
        true, 0
      }
    },
    {
      "optimise",
      {
        true,
        "object(surface)",
        "boolean",
        "Disable this flag to skip the mesh optimisation step",
        true, 0
      }
    },
    {
      "vertexnormals",
      {
        true,
        "object(surface)",
        "boolean",
        "Disable this flag to skip calculating vertex normals",
        true, 0
      }
    },
    {
      "power",
      {
        1.0,
        "object(volume)",
        "real",
        "Power used when applying transfer function, 1.0=linear mapping",
        true, 0
      }
    },
    {
      "samples",
      {
        256,
        "object(volume)",
        "integer",
        "Number of samples to take per ray cast, higher = better quality, slower",
        true, 0
      }
    },
    {
      "density",
      {
        5.0,
        "object(volume)",
        "real",
        "Density multiplier for volume data",
        true, 0
      }
    },
    {
      "isovalue",
      {
        FLT_MAX,
        "object(volume)",
        "real",
        "Isovalue for dynamic isosurface",
        true, 0
      }
    },
    {
      "isovalues",
      {
        json::array(),
        "object(volume)",
        "real[]",
        "Isovalues to extract from volume into mesh isosurface",
        true, 0
      }
    },
    {
      "isoalpha",
      {
        1.0,
        "object(volume)",
        "real [0,1]",
        "Transparency value for isosurface",
        true, 0
      }
    },
    {
      "isosmooth",
      {
        0.1,
        "object(volume)",
        "real",
        "Isosurface smoothing factor for normal calculation",
        true, 0
      }
    },
    {
      "isowalls",
      {
        true,
        "object(volume)",
        "boolean",
        "Connect isosurface enclosed area with walls",
        true, 0
      }
    },
    {
      "tricubicfilter",
      {
        false,
        "object(volume)",
        "boolean",
        "Apply a tricubic filter for increased smoothness",
        true, 0
      }
    },
    {
      "minclip",
      {
        0.0,
        "object(volume)",
        "real",
        "Minimum density value to map, lower discarded",
        true, 0
      }
    },
    {
      "maxclip",
      {
        1.0,
        "object(volume)",
        "real",
        "Maximum density value to map, higher discarded",
        true, 0
      }
    },
    {
      "compresstextures",
      {
        false,
        "object(volume)",
        "boolean",
        "Compress volume textures where possible",
        true, 0
      }
    },
    {
      "texturesize",
      {
        {0,0,0},
        "object(volume)",
        "int[3]",
        "Volume texture size limit (for crop)",
        true, 0
      }
    },
    {
      "textureoffset",
      {
        {0,0,0},
        "object(volume)",
        "int[3]",
        "Volume texture offset (for crop)",
        true, 0
      }
    },
    {
      "scalemax",
      {
        0.0,
        "object(vector)",
        "real",
        "Length scaling maximum, sets the range over which vectors will be scaled [0,scalemax]. Default automatically calculated based on data max",
        true, 1
      }
    },
    {
      "arrowhead",
      {
        5.0,
        "object(vector)",
        "real",
        "Arrow head size as a multiple of length or radius, if < 1.0 is multiple of length, if > 1.0 is multiple of radius",
        true, 1
      }
    },
    {
      "scalevectors",
      {
        1.0,
        "object(vector)",
        "real",
        "Vector scaling multiplier, applies to all vector objects",
        true, 0
      }
    },
    {
      "radius",
      {
        0.02,
        "object(vector)",
        "real",
        "Arrow shaft radius as ratio of vector length",
        true, 1
      }
    },
    {
      "thickness",
      {
        0.0,
        "object(vector)",
        "real",
        "Arrow shaft thickness as fixed value (overrides \"radius\")",
        true, 1
      }
    },
    {
      "length",
      {
        0.0,
        "object(vector)",
        "real",
        "Arrow fixed length, default is to use vector magnitude",
        true, 1
      }
    },
    {
      "normalise",
      {
        1.0,
        "object(vector)",
        "real",
        "Normalisation factor to adjust between vector arrows scaled to their vector length or all arrows having a constant length. If 0.0 vectors are scaled to their vector length, if 1.0 vectors are all scaled to the constant \"length\" property (if property length=0.0, this is ignored).",
        true, 1
      }
    },
    {
      "autoscale",
      {
        true,
        "object(vector)",
        "boolean",
        "Automatically scale vectors based on maximum magnitude",
        true, 1
      }
    },
    {
      "steps",
      {
        0,
        "object(tracer)",
        "integer",
        "Number of time steps to trace particle path",
        true, 1
      }
    },
    {
      "taper",
      {
        true,
        "object(tracer)",
        "boolean",
        "Taper width of tracer arrow up as we get closer to current timestep",
        true, 1
      }
    },
    {
      "fade",
      {
        false,
        "object(tracer)",
        "boolean",
        "Fade opacity of tracer arrow in from transparent as we get closer to current timestep",
        true, 1
      }
    },
    {
      "connect",
      {
        true,
        "object(tracer)",
        "boolean",
        "Set false to render tracers as points instead of connected lines",
        true, 1
      }
    },
    {
      "scaletracers",
      {
        1.0,
        "object(tracer)",
        "real",
        "Tracer scaling multiplier, applies to all tracer objects",
        true, 0
      }
    },
    {
      "shapewidth",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape width scaling factor",
        true, 0
      }
    },
    {
      "shapeheight",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape height scaling factor",
        true, 0
      }
    },
    {
      "shapelength",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape length scaling factor",
        true, 0
      }
    },
    {
      "shape",
      {
        0,
        "object(shape)",
        "integer",
        "Shape type: 0=ellipsoid, 1=cuboid",
        true, 0
      }
    },
    {
      "scaleshapes",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape scaling multiplier, applies to all shape objects",
        true, 0
      }
    },
    {
      "widthby",
      {
        "widths",
        "object(shape)",
        "string or integer",
        "Index or label of data set to apply to shape widths",
        false, 2
      }
    },
    {
      "heightby",
      {
        "heights",
        "object(shape)",
        "string or integer",
        "Index or label of data set to apply to shape heights",
        false, 2
      }
    },
    {
      "lengthby",
      {
        "lengths",
        "object(shape)",
        "string or integer",
        "Index or label of data set to apply to shape lengths",
        false, 2
      }
    },
    {
      "colourbar",
      {
        false,
        "colourbar",
        "boolean",
        "Indicates object is a colourbar",
        true, 0
      }
    },
    {
      "align",
      {
        "bottom",
        "colourbar",
        "string",
        "Alignment of colour bar to screen edge, top/bottom/left/right",
        true, 0
      }
    },
    {
      "size",
      {
        {0, 0},
        "colourbar",
        "real[2]",
        "Dimensions of colour bar (length/breadth) in pixels or viewport size ratio",
        true, 0
      }
    },
    {
      "ticks",
      {
        0,
        "colourbar",
        "integer",
        "Number of additional tick marks to draw besides start and end",
        true, 0
      }
    },
    {
      "tickvalues",
      {
        json::array(),
        "colourbar",
        "float[]",
        "Values of intermediate tick marks",
        true, 0
      }
    },
    {
      "printticks",
      {
        true,
        "colourbar",
        "boolean",
        "Set to false to disable drawing of intermediate tick values",
        true, 0
      }
    },
    {
      "format",
      {
        "%.5g",
        "colourbar",
        "string",
        "Format specifier for tick values, eg: %.3[f/e/g] standard, scientific, both",
        true, 0
      }
    },
    {
      "scalevalue",
      {
        1.0,
        "colourbar",
        "real",
        "Multiplier to scale tick values",
        true, 0
      }
    },
    {
      "outline",
      {
        1.0,
        "colourbar",
        "integer",
        "Outline width to draw around colour bar",
        true, 0
      }
    },
    {
      "offset",
      {
        0,
        "colourbar",
        "real",
        "Margin to parallel edge in pixels or viewport size ratio",
        true, 0
      }
    },
    {
      "position",
      {
        0,
        "colourbar",
        "real",
        "Margin to perpendicular edge in pixels or viewport size ratio, >0=towards left/bottom, 0=centre (horizontal only), <0=towards right/top",
        true, 0
      }
    },
    {
      "binlabels",
      {
        false,
        "colourbar",
        "boolean",
        "Set to true to label discrete bins rather than tick points",
        true, 0
      }
    },
    {
      "logscale",
      {
        false,
        "colourmap",
        "boolean",
        "Set to true to use log scales",
        true, 0
      }
    },
    {
      "discrete",
      {
        false,
        "colourmap",
        "boolean",
        "Set to true to apply colours as discrete values rather than gradient",
        true, 0
      }
    },
    {
      "colours",
      {
        json::array(),
        "colourmap",
        "colours",
        "Colour list (or string), see [Colour map lists] for more information",
        false, 0
      }
    },
    {
      "range",
      {
        {0.0, 0.0},
        "colourmap",
        "real[2]",
        "Fixed scale range, default is to automatically calculate range based on data min/max",
        true, 0
      }
    },
    {
      "locked",
      {
        false,
        "colourmap",
        "boolean",
        "Set to true to lock colourmap ranges to current values",
        true, 0
      }
    },
    {
      "title",
      {
        "",
        "view",
        "string",
        "Title to display at top centre of view",
        true, 0
      }
    },
    {
      "zoomstep",
      {
        -1,
        "view",
        "integer",
        "When to apply camera auto-zoom to fit model to window, -1=never, 0=first timestep only, 1=every timestep",
        true, 0
      }
    },
    {
      "margin",
      {
        20,
        "view",
        "integer",
        "Margin in pixels to leave around edge of model when to applying camera auto-zoom",
        true, 0
      }
    },
    {
      "rulers",
      {
        false,
        "view",
        "boolean",
        "Draw rulers around object axes",
        true, 0
      }
    },
    {
      "ruleraxes",
      {
        "xyz",
        "view",
        "string",
        "Which figure axes to draw rulers beside (xyzXYZ) lowercase = min, capital = max ",
        true, 0
      }
    },
    {
      "rulerticks",
      {
        5,
        "view",
        "integer",
        "Number of tick marks to display on rulers",
        true, 0
      }
    },
    {
      "rulerwidth",
      {
        1.5,
        "view",
        "real",
        "Width of ruler lines",
        true, 0
      }
    },
    {
      "border",
      {
        1.0,
        "view",
        "integer",
        "Border width around model boundary, 0=disabled",
        true, 0
      }
    },
    {
      "fillborder",
      {
        false,
        "view",
        "boolean",
        "Draw filled background box around model boundary",
        true, 0
      }
    },
    {
      "bordercolour",
      {
        "grey",
        "view",
        "colour",
        "Colour of model boundary border",
        true, 0
      }
    },
    {
      "axis",
      {
        true,
        "view",
        "boolean",
        "Draw X/Y/Z legend showing model axes orientation",
        true, 0
      }
    },
    {
      "axislength",
      {
        0.1,
        "view",
        "real",
        "Axis legend scaling factor",
        true, 0
      }
    },
    {
      "quality",
      {
        2,
        "view", //Should be global?
        "integer",
        "Read only: Over-sample antialiasing level, for off-screen rendering",
        true, 0
      }
    },

    {
      "rotate",
      {
        {0.,0.,0.,1.},
        "view",
        "real[4]",
        "Camera rotation quaternion [x,y,z,w]",
        true, 0
      }
    },
    {
      "xyzrotate",
      {
        {0.,0.,0.},
        "view",
        "real[3]",
        "Camera rotation as Euler angles [x,y,z] - output only",
        true, 0
      }
    },
    {
      "translate",
      {
        {0.,0.,0.},
        "view",
        "real[3]",
        "Camera translation [x,y,z]",
        true, 0
      }
    },
    {
      "focus",
      {
        {0.,0.,0.},
        "view",
        "real[3]",
        "Camera focal point [x,y,z]",
        true, 0
      }
    },
    {
      "scale",
      {
        {1.,1.,1.},
        "view",
        "real[3]",
        "Global model scaling factors [x,y,z]",
        true, 0
      }
    },
    {
      "min",
      {
        {0,0,0},
        "view",
        "real[3]",
        "Global model minimum bounds [x,y,z]",
        true, 0
      }
    },
    {
      "max",
      {
        {0,0,0},
        "view",
        "real[3]",
        "Global model maximum bounds [x,y,z]",
        true, 0
      }
    },
    {
      "near",
      {
        0.0,
        "view",
        "real",
        "Near clipping plane position, adjusts where geometry close to the camera is clipped",
        true, 0
      }
    },
    {
      "far",
      {
        0.0,
        "view",
        "real",
        "Far clip plane position, adjusts where far geometry is clipped",
        true, 0
      }
    },
    {
      "aperture",
      {
        45.0,
        "view",
        "real",
        "Aperture or Field-Of-View in degrees",
        true, 0
      }
    },
    {
      "coordsystem",
      {
        1,
        "view",
        "integer",
        "Set to determine coordinate system, 1=Right-handed (OpenGL default) -1=Left-handed",
        true, 0
      }
    },
    {
      "follow",
      {
        false,
        "view",
        "boolean",
        "Enable to follow the model bounding box centre with camera as it changes",
        true, 0
      }
    },
    {
      "bounds",
      {
        json::array(),
        "global",
        "real[][]",
        "Read only bounding box (min/max)",
        true, 0
      }
    },
    {
      "caption",
      {
        "default",
        "global",
        "string",
        "Title for caption area and image output filenames",
        true, 0
      }
    },
    {
      "resolution",
      {
        {1024,768},
        "global",
        "integer[2]",
        "Window resolution X,Y",
        true, 0
      }
    },
    {
      "antialias",
      {
        true,
        "global",
        "boolean",
        "Enable multisample anti-aliasing, only works with interactive viewing",
        true, 0
      }
    },
    {
      "fps",
      {
        false,
        "global",
        "boolean",
        "Turn on to display FPS count",
        true, 0
      }
    },
    {
      "filestep",
      {
        false,
        "global",
        "boolean",
        "Turn on to automatically add and switch to a new timestep after loading a data file",
        true, 0
      }
    },
    {
      "hideall",
      {
        false,
        "global",
        "boolean",
        "Turn on to set initial state of all loaded objects to hidden",
        true, 0
      }
    },
    {
      "background",
      {
        {0,0,0,255},
        "global",
        "colour",
        "Background colour RGB(A)",
        true, 0
      }
    },
    {
      "alpha",
      {
        1.0,
        "global",
        "real [0,1]",
        "Global opacity multiplier where 0 is transparent and 1 is opaque, this is combined with \"opacity\" prop",
        true, 0
      }
    },
    {
      "noload",
      {
        false,
        "global",
        "boolean",
        "Disables initial loading of object data from database, only object names loaded, use the \"load\" command to subsequently load selected object data",
        true, 0
      }
    },
    {
      "merge",
      {
        false,
        "global",
        "boolean",
        "Enable to load subsequent databases into the current model, if disabled then each database is loaded into a new model",
        true, 0
      }
    },
    {
      "pointspheres",
      {
        false,
        "global",
        "boolean",
        "Enable rendering points as proper 3d spherical meshes, TODO: deprecate and implement as a renderer type 'particles' that can plot points or spheres",
        true, 0
      }
    },
    {
      "pngalpha",
      {
        false,
        "global",
        "boolean",
        "Enable transparent png output",
        true, 0
      }
    },
    {
      "swapyz",
      {
        false,
        "global",
        "boolean",
        "Enable imported model y/z axis swap",
        true, 0
      }
    },
    {
      "trisplit",
      {
        0,
        "global",
        "integer",
        "Imported model triangle subdivision level. Can also be set to 1 to force vertex normals to be recalculated by ignoring any present in the loaded model data.",
        true, 0
      }
    },
    {
      "globalcam",
      {
        false,
        "global",
        "boolean",
        "Enable global camera for all models (default is separate cam for each)",
        true, 0
      }
    },
    {
      "vectorfont",
      {
        false,
        "global",
        "boolean",
        "Always use vector font regardless of individual object properties",
        true, 0
      }
    },
    {
      "volchannels",
      {
        1,
        "global",
        "integer",
        "Volume rendering output channels 1 (luminance) 3/4 (rgba)",
        true, 0
      }
    },
    {
      "volres",
      {
        {256,256,256},
        "global",
        "integer[3]",
        "Volume rendering data voxel resolution X Y Z",
        true, 0
      }
    },
    {
      "volmin",
      {
        {0.,0.,0.},
        "global",
        "real[3]",
        "Volume rendering min bound X Y Z",
        true, 0
      }
    },
    {
      "volmax",
      {
        {1.,1.,1.},
        "global",
        "real[3]",
        "Volume rendering max bound X Y Z",
        true, 0
      }
    },
    {
      "volsubsample",
      {
        {1,1,1},
        "global",
        "int[3]",
        "Volume rendering subsampling factor X Y Z",
        true, 0
      }
    },
    {
      "slicevolumes",
      {
        false,
        "global",
        "boolean",
        "Convert full volume data sets to slices (allows cropping and sub-sampling)",
        true, 0
      }
    },
    {
      "slicedump",
      {
        false,
        "global",
        "boolean",
        "Export full volume data sets to slices",
        true, 0
      }
    },
    {
      "inscale",
      {
        {1.,1.,1.},
        "global",
        "real[3]",
        "Geometry input scaling X Y Z",
        true, 0
      }
    },
    {
      "pointsubsample",
      {
        0,
        "global",
        "integer",
        "Point render sub-sampling factor",
        true, 0
      }
    },
    {
      "pointmaxcount",
      {
        0,
        "global",
        "integer",
        "Point render maximum count before auto sub-sampling",
        true, 0
      },
    },
    {
      "pointdistsample",
      {
        0,
        "global",
        "integer",
        "Point distance sub-sampling factor",
        true, 0
      }
    },
    {
      "pointattribs",
      {
        true,
        "global",
        "boolean",
        "Point size/type attributes can be applied per object (requires more GPU ram)",
        true, 0
      }
    },
    {
      "pointattenuate",
      {
        true,
        "global",
        "boolean",
        "Point distance size attenuation (points shrink when further from viewer ie: perspective)",
        true, 0
      }
    },
    {
      "sort",
      {
        true,
        "global",
        "boolean",
        "Automatic depth sorting enabled",
        true, 0
      }
    },
    {
      "cache",
      {
        false,
        "global",
        "boolean",
        "Cache all time varying data in ram on initial load",
        true, 0
      }
    },
    {
      "gpucache",
      {
        false,
        "global",
        "boolean",
        "Cache timestep varying data on gpu as well as ram (only if model size permits)",
        true, 0
      }
    },
    {
      "clearstep",
      {
        false,
        "global",
        "boolean",
        "Clear all time varying data from previous step on loading another",
        true, 0
      }
    },
    {
      "timestep",
      {
        -1,
        "global",
        "integer",
        "Holds the current model timestep, read only, -1 indicates no time varying data loaded",
        true, 0
      }
    },
    {
      "data",
      {
        {},
        "global",
        "dict",
        "Holds a dictionary of data sets in the current model by label, read only",
        true, 0
      }
    }

  };

  //Call init with default data
  init(property_data);
}

void Session::init(std::vector<std::pair<std::string,json>>& property_data)
{
  for (auto p : property_data)
  {
    //Copy default values
    std::string key = p.first;
    properties[key] = p.second;

    //Save view properties
    std::string target = p.second[PROPTARGET];
    //if (target.find("view") != std::string::npos)
    if (target == "view")
      viewProps.push_back(key);

    //Save colourmap properties
    if (target.find("colourmap") != std::string::npos)
      colourMapProps.push_back(key);

    //Save defaults
    defaults[key] = p.second[PROPDEFAULT];
  }

#ifdef DEBUG
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

