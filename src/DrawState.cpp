#include "DrawState.h"

DrawState::DrawState() : prog(), eng0(std::random_device()()), eng1(0), dist(0, 1)
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

DrawState::~DrawState()
{
  fonts.reset();

  if (globalcam) delete globalcam;

  if (x_coords != NULL) delete[] x_coords;
  if (y_coords != NULL) delete[] y_coords;
}

std::string DrawState::counterFilename()
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

void DrawState::reset()
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

void DrawState::init()
{
  //Setup default properties (once only)
  properties = {
    {
      "renderlist",
      {
        "sortedtriangles quads vectors tracers shapes sortedpoints labels lines volume",
        "all",
        "string",
        "List of renderers created and order they are displayed. Valid types are: volume,triangles,sortedtriangles,quads,vectors,tracers,shapes,points,sortedpoints,labels,lines,links"
      }
    },
    {
      "font",
      {
        "vector",
        "all",
        "string",
        "Font typeface vector/small/fixed/sans/serif"
      }
    },
    {
      "fontscale",
      {
        1.0,
        "all",
        "real",
        "Font scaling, note that only the 'vector' font scales well"
      }
    },
    {
      "fontcolour",
      {
        {0,0,0,0},
        "all",
        "colour",
        "Font colour RGB(A)"
      }
    },
    {
      "name",
      {
        "",
        "object",
        "string",
        "Name of object"
      }
    },
    {
      "visible",
      {
        true,
        "object",
        "boolean",
        "Set to false to hide object"
      }
    },
    {
      "geometry",
      {
        "points",
        "object",
        "string",
        "Geometry type to load when adding new data"
      }
    },
    {
      "lit",
      {
        true,
        "object",
        "boolean",
        "Apply lighting to object"
      }
    },
    {
      "cullface",
      {
        false,
        "object",
        "boolean",
        "Cull back facing polygons of object surfaces"
      }
    },
    {
      "wireframe",
      {
        false,
        "object",
        "boolean",
        "Render object surfaces as wireframe"
      }
    },
    {
      "flat",
      {
        false,
        "object",
        "boolean",
        "Renders surfaces as flat shaded, lines/vectors/tracers as 2d, faster but no 3d or lighting"
      }
    },
    {
      "depthtest",
      {
        true,
        "object",
        "boolean",
        "Set to false to disable depth test when drawing object so always drawn regardless of 3d position"
      }
    },
    {
      "dims",
      {
        {0,0,0},
        "object",
        "integer[3]",
        "width/height/depth override for geometry"
      }
    },
    {
      "rotatable",
      {
        false,
        "object",
        "boolean",
        "Set to true to apply the view rotation to this object"
      }
    },
    {
      "shift",
      {
        0.,
        "object",
        "real",
        "Apply a shift to object position by this amount multiplied by model size, to fix depth fighting when visualising objects drawn at same depth"
      }
    },
    {
      "colour",
      {
        {0,0,0,255},
        "object",
        "colour",
        "Object colour RGB(A)"
      }
    },
    {
      "colourmap",
      {
        "",
        "object",
        "string",
        "name of the colourmap to use"
      }
    },
    {
      "opacitymap",
      {
        "",
        "object",
        "string",
        "name of the opacity colourmap to use"
      }
    },
    {
      "opacity",
      {
        1.0,
        "object",
        "real [0,1]",
        "Opacity of object where 0 is transparent and 1 is opaque"
      }
    },
    {
      "brightness",
      {
        0.0,
        "object",
        "real [-1,1]",
        "Brightness of object from -1 (full dark) to 0 (default) to 1 (full light)"
      }
    },
    {
      "contrast",
      {
        1.0,
        "object",
        "real [0,2]",
        "Contrast of object from 0 (none, grey) to 2 (max)"
      }
    },
    {
      "saturation",
      {
        1.0,
        "object",
        "real [0,2]",
        "Saturation of object from 0 (greyscale) to 2 (fully saturated)"
      }
    },
    {
      "ambient",
      {
        0.4,
        "object",
        "real [0,1]",
        "Ambient lighting level (background constant light)"
      }
    },
    {
      "diffuse",
      {
        0.65,
        "object",
        "real [0,1]",
        "Diffuse lighting level (shading light/dark)"
      }
    },
    {
      "specular",
      {
        0.0,
        "object",
        "real [0,1]",
        "Specular highlight lighting level (spot highlights)"
      }
    },
    {
      "lightpos",
      {
        {0.1,-0.1,2.0},
        "object",
        "real[3]",
        "Light position X Y Z relative to camera position (follows camera)"
      }
    },
    {
      "clip",
      {
        true,
        "object",
        "boolean",
        "Allow object to be clipped"
      }
    },
    {
      "clipmap",
      {
        true,
        "object",
        "boolean",
        "Clipping mapped to range normalised [0,1]"
      }
    },
    {
      "xmin",
      {
        -FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, minimum x"
      }
    },
    {
      "ymin",
      {
        -FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum y"
      }
    },
    {
      "zmin",
      {
        -FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, minimum z"
      }
    },
    {
      "xmax",
      {
        FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum x"
      }
    },
    {
      "ymax",
      {
        FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum y"
      }
    },
    {
      "zmax",
      {
        FLT_MAX,
        "object",
        "real [0,1]",
        "Object clipping, maximum z"
      }
    },
    {
      "filters",
      {
        json::array(),
        "object",
        "object",
        "Filter list"
      }
    },
    {
      "glyphs",
      {
        2,
        "object",
        "integer [0,n]",
        "Glyph quality 0=none, 1=low, higher=increasing triangulation detail (arrows/shapes etc)"
      }
    },
    {
      "scaling",
      {
        1.0,
        "object",
        "real",
        "Object scaling factor"
      }
    },
    {
      "texture",
      {
        "",
        "object",
        "string",
        "External texture image file path to load and apply to surface or points"
      }
    },
    {
      "fliptexture",
      {
        true,
        "object",
        "boolean",
        "Flip texture image after loading, usually required"
      }
    },
    {
      "colourby",
      {
        0,
        "object",
        "string or integer",
        "Index or label of data set to colour object by (requires colour map)"
      }
    },
    {
      "opacityby",
      {
        1,
        "object",
        "string or integer",
        "Index or label of data set to apply transparency to object by (requires opacity map)"
      }
    },
    {
      "limit",
      {
        0,
        "object(line)",
        "real",
        "Line length limit, can be used to skip drawing line segments that cross periodic boundary"
      }
    },
    {
      "link",
      {
        false,
        "object(line)",
        "boolean",
        "To chain line vertices into longer lines set to true"
      }
    },
    {
      "scalelines",
      {
        1.0,
        "object(line)",
        "real",
        "Line scaling multiplier, applies to all line objects"
      }
    },
    {
      "linewidth",
      {
        1.0,
        "object(line)",
        "real",
        "Line width scaling"
      }
    },
    {
      "tubes",
      {
        false,
        "object(line)",
        "boolean",
        "Draw lines as 3D tubes"
      }
    },
    {
      "pointsize",
      {
        1.0,
        "object(point)",
        "real",
        "Point size scaling"
      }
    },
    {
      "pointtype",
      {
        1,
        "object(point)",
        "integer/string",
        "Point type 0/blur, 1/smooth, 2/sphere, 3/shiny, 4/flat"
      }
    },
    {
      "scalepoints",
      {
        1.0,
        "object(point)",
        "real",
        "Point scaling multiplier, applies to all points objects"
      }
    },
    {
      "sizeby",
      {
        1,
        "object(point)",
        "string or integer",
        "Index or label of data set to apply to point sizes"
      }
    },
    {
      "opaque",
      {
        false,
        "object(surface)",
        "boolean",
        "If opaque flag is set skips depth sorting step and allows individual surface properties to be applied"
      }
    },
    {
      "optimise",
      {
        true,
        "object(surface)",
        "boolean",
        "Disable this flag to skip the mesh optimisation step"
      }
    },
    {
      "vertexnormals",
      {
        true,
        "object(surface)",
        "boolean",
        "Disable this flag to skip calculating vertex normals"
      }
    },
    {
      "power",
      {
        1.0,
        "object(volume)",
        "real",
        "Power used when applying transfer function, 1.0=linear mapping"
      }
    },
    {
      "samples",
      {
        256,
        "object(volume)",
        "integer",
        "Number of samples to take per ray cast, higher = better quality, slower"
      }
    },
    {
      "density",
      {
        5.0,
        "object(volume)",
        "real",
        "Density multiplier for volume data"
      }
    },
    {
      "isovalue",
      {
        FLT_MAX,
        "object(volume)",
        "real",
        "Isovalue for dynamic isosurface"
      }
    },
    {
      "isoalpha",
      {
        1.0,
        "object(volume)",
        "real [0,1]",
        "Transparency value for isosurface"
      }
    },
    {
      "isosmooth",
      {
        0.1,
        "object(volume)",
        "real",
        "Isosurface smoothing factor for normal calculation"
      }
    },
    {
      "isowalls",
      {
        true,
        "object(volume)",
        "boolean",
        "Connect isosurface enclosed area with walls"
      }
    },
    {
      "tricubicfilter",
      {
        false,
        "object(volume)",
        "boolean",
        "Apply a tricubic filter for increased smoothness"
      }
    },
    {
      "dminclip",
      {
        0.0,
        "object(volume)",
        "real",
        "Minimum density value to map, lower discarded"
      }
    },
    {
      "dmaxclip",
      {
        1.0,
        "object(volume)",
        "real",
        "Maximum density value to map, higher discarded"
      }
    },
    {
      "compresstextures",
      {
        false,
        "object(volume)",
        "boolean",
        "Compress volume textures where possible"
      }
    },
    {
      "texturesize",
      {
        {0,0,0},
        "object(volume)",
        "int[3]",
        "Volume texture size limit (for crop)"
      }
    },
    {
      "textureoffset",
      {
        {0,0,0},
        "object(volume)",
        "int[3]",
        "Volume texture offset (for crop)"
      }
    },
    {
      "arrowhead",
      {
        5.0,
        "object(vector)",
        "real",
        "Arrow head size as a multiple of length or radius, if < 1.0 is multiple of length, if > 1.0 is multiple of radius"
      }
    },
    {
      "scalevectors",
      {
        1.0,
        "object(vector)",
        "real",
        "Vector scaling multiplier, applies to all vector objects"
      }
    },
    {
      "radius",
      {
        0.02,
        "object(vector)",
        "real",
        "Arrow shaft radius as ratio of vector length"
      }
    },
    {
      "length",
      {
        0.0,
        "object(vector)",
        "real",
        "Arrow fixed length, default is to use vector magnitude"
      }
    },
    {
      "normalise",
      {
        1.0,
        "object(vector)",
        "real",
        "Normalisation factor to adjust between vector arrows scaled to their vector length or all arrows having a constant length. If 0.0 vectors are scaled to their vector length, if 1.0 vectors are all scaled to the constant \"length\" property (if property length=0.0, this is ignored)."
      }
    },
    {
      "autoscale",
      {
        true,
        "object(vector)",
        "boolean",
        "Automatically scale vectors based on maximum magnitude"
      }
    },
    {
      "steps",
      {
        0,
        "object(tracer)",
        "integer",
        "Number of time steps to trace particle path"
      }
    },
    {
      "taper",
      {
        true,
        "object(tracer)",
        "boolean",
        "Taper width of tracer arrow up as we get closer to current timestep"
      }
    },
    {
      "fade",
      {
        false,
        "object(tracer)",
        "boolean",
        "Fade opacity of tracer arrow in from transparent as we get closer to current timestep"
      }
    },
    {
      "connect",
      {
        true,
        "object(tracer)",
        "boolean",
        "Set falst to render tracers as points instead of connected lines"
      }
    },
    {
      "scaletracers",
      {
        1.0,
        "object(tracer)",
        "real",
        "Tracer scaling multiplier, applies to all tracer objects"
      }
    },
    {
      "shapewidth",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape width scaling factor"
      }
    },
    {
      "shapeheight",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape height scaling factor"
      }
    },
    {
      "shapelength",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape length scaling factor"
      }
    },
    {
      "shape",
      {
        0,
        "object(shape)",
        "integer",
        "Shape type: 0=ellipsoid, 1=cuboid"
      }
    },
    {
      "scaleshapes",
      {
        1.0,
        "object(shape)",
        "real",
        "Shape scaling multiplier, applies to all shape objects"
      }
    },
    {
      "widthby",
      {
        "widths",
        "object(shape)",
        "string or integer",
        "Index or label of data set to apply to shape widths"
      }
    },
    {
      "heightby",
      {
        "heights",
        "object(shape)",
        "string or integer",
        "Index or label of data set to apply to shape heights"
      }
    },
    {
      "lengthby",
      {
        "lengths",
        "object(shape)",
        "string or integer",
        "Index or label of data set to apply to shape lengths"
      }
    },
    {
      "colourbar",
      {
        false,
        "colourbar",
        "boolean",
        "Indicates object is a colourbar"
      }
    },
    {
      "align",
      {
        "bottom",
        "colourbar",
        "string",
        "Alignment of colour bar to screen edge, top/bottom/left/right"
      }
    },
    {
      "size",
      {
        {0, 0},
        "colourbar",
        "real[2]",
        "Dimensions of colour bar (length/breadth) in pixels or viewport size ratio"
      }
    },
    {
      "ticks",
      {
        0,
        "colourbar",
        "integer",
        "Number of additional tick marks to draw besides start and end"
      }
    },
    {
      "tickvalues",
      {
        json::array(),
        "colourbar",
        "float[]",
        "Values of intermediate tick marks"
      }
    },
    {
      "printticks",
      {
        true,
        "colourbar",
        "boolean",
        "Set to false to disable drawing of intermediate tick values"
      }
    },
    {
      "format",
      {
        "%.5g",
        "colourbar",
        "string",
        "Format specifier for tick values, eg: %.3[f/e/g] standard, scientific, both"
      }
    },
    {
      "scalevalue",
      {
        1.0,
        "colourbar",
        "real",
        "Multiplier to scale tick values"
      }
    },
    {
      "outline",
      {
        1.0,
        "colourbar",
        "integer",
        "Outline width to draw around colour bar"
      }
    },
    {
      "offset",
      {
        0,
        "colourbar",
        "real",
        "Margin to parallel edge in pixels or viewport size ratio"
      }
    },
    {
      "position",
      {
        0,
        "colourbar",
        "real",
        "Margin to perpendicular edge in pixels or viewport size ratio, >0=towards left/bottom, 0=centre (horizontal only), <0=towards right/top"
      }
    },
    {
      "binlabels",
      {
        false,
        "colourbar",
        "boolean",
        "Set to true to label discrete bins rather than tick points"
      }
    },
    {
      "logscale",
      {
        false,
        "colourmap",
        "boolean",
        "Set to true to use log scales"
      }
    },
    {
      "discrete",
      {
        false,
        "colourmap",
        "boolean",
        "Set to true to apply colours as discrete values rather than gradient"
      }
    },
    {
      "colours",
      {
        json::array(),
        "colourmap",
        "colours",
        "Colour list, see [Colour map lists] for more information"
      }
    },
    {
      "range",
      {
        {0.0, 0.0},
        "colourmap",
        "real[2]",
        "Fixed scale range, default is to automatically calculate range based on data min/max"
      }
    },
    {
      "locked",
      {
        false,
        "colourmap",
        "boolean",
        "Set to true to lock colourmap ranges to current values"
      }
    },
    {
      "title",
      {
        "",
        "view",
        "string",
        "Title to display at top centre of view"
      }
    },
    {
      "zoomstep",
      {
        -1,
        "view",
        "integer",
        "When to apply camera auto-zoom to fit model to window, -1=never, 0=first timestep only, 1=every timestep"
      }
    },
    {
      "margin",
      {
        20,
        "view",
        "integer",
        "Margin in pixels to leave around edge of model when to applying camera auto-zoom"
      }
    },
    {
      "rulers",
      {
        false,
        "view",
        "boolean",
        "Draw rulers around object axes"
      }
    },
    {
      "ruleraxes",
      {
        "xyz",
        "view",
        "string",
        "Which figure axes to draw rulers beside (xyzXYZ) lowercase = min, capital = max "
      }
    },
    {
      "rulerticks",
      {
        5,
        "view",
        "integer",
        "Number of tick marks to display on rulers"
      }
    },
    {
      "rulerwidth",
      {
        1.5,
        "view",
        "real",
        "Width of ruler lines"
      }
    },
    {
      "border",
      {
        1.0,
        "view",
        "integer",
        "Border width around model boundary, 0=disabled"
      }
    },
    {
      "fillborder",
      {
        false,
        "view",
        "boolean",
        "Draw filled background box around model boundary"
      }
    },
    {
      "bordercolour",
      {
        "grey",
        "view",
        "colour",
        "Colour of model boundary border"
      }
    },
    {
      "axis",
      {
        true,
        "view",
        "boolean",
        "Draw X/Y/Z legend showing model axes orientation"
      }
    },
    {
      "axislength",
      {
        0.1,
        "view",
        "real",
        "Axis legend scaling factor"
      }
    },
    {
      "quality",
      {
        2,
        "view", //Should be global?
        "integer",
        "Read only: Over-sample antialiasing level, for off-screen rendering"
      }
    },

    {
      "rotate",
      {
        {0.,0.,0.,1.},
        "view",
        "real[4]",
        "Camera rotation quaternion [x,y,z,w]"
      }
    },
    {
      "xyzrotate",
      {
        {0.,0.,0.},
        "view",
        "real[3]",
        "Camera rotation as Euler angles [x,y,z] - output only"
      }
    },
    {
      "translate",
      {
        {0.,0.,0.},
        "view",
        "real[3]",
        "Camera translation [x,y,z]"
      }
    },
    {
      "focus",
      {
        {0.,0.,0.},
        "view",
        "real[3]",
        "Camera focal point [x,y,z]"
      }
    },
    {
      "scale",
      {
        {1.,1.,1.},
        "view",
        "real[3]",
        "Global model scaling factors [x,y,z]"
      }
    },
    {
      "min",
      {
        {0,0,0},
        "view",
        "real[3]",
        "Global model minimum bounds [x,y,z]"
      }
    },
    {
      "max",
      {
        {0,0,0},
        "view",
        "real[3]",
        "Global model maximum bounds [x,y,z]"
      }
    },
    {
      "near",
      {
        0.0,
        "view",
        "real",
        "Near clipping plane position, adjusts where geometry close to the camera is clipped"
      }
    },
    {
      "far",
      {
        0.0,
        "view",
        "real",
        "Far clip plane position, adjusts where far geometry is clipped"
      }
    },
    {
      "coordsystem",
      {
        1,
        "view",
        "integer",
        "Set to determine coordinate system, 1=Right-handed (OpenGL default) -1=Left-handed"
      }
    },
    {
      "follow",
      {
        false,
        "view",
        "boolean",
        "Enable to follow the model bounding box centre with camera as it changes"
      }
    },
    {
      "bounds",
      {
        json::array(),
        "global",
        "real[][]",
        "Read only bounding box (min/max)"
      }
    },
    {
      "caption",
      {
        "default",
        "global",
        "string",
        "Title for caption area and image output filenames"
      }
    },
    {
      "resolution",
      {
        {1024,768},
        "global",
        "integer[2]",
        "Window resolution X,Y"
      }
    },
    {
      "antialias",
      {
        true,
        "global",
        "boolean",
        "Enable multisample anti-aliasing, only works with interactive viewing"
      }
    },
    {
      "fps",
      {
        false,
        "global",
        "boolean",
        "Turn on to display FPS count"
      }
    },
    {
      "filestep",
      {
        false,
        "global",
        "boolean",
        "Turn on to automatically add and switch to a new timestep after loading a data file"
      }
    },
    {
      "hideall",
      {
        false,
        "global",
        "boolean",
        "Turn on to set initial state of all loaded objects to hidden"
      }
    },
    {
      "background",
      {
        {0,0,0,255},
        "global",
        "colour",
        "Background colour RGB(A)"
      }
    },
    {
      "alpha",
      {
        1.0,
        "global",
        "real [0,1]",
        "Global opacity multiplier where 0 is transparent and 1 is opaque, this is combined with \"opacity\" prop"
      }
    },
    {
      "noload",
      {
        false,
        "global",
        "boolean",
        "Disables initial loading of object data from database, only object names loaded, use the \"load\" command to subsequently load selected object data"
      }
    },
    {
      "pointspheres",
      {
        false,
        "global",
        "boolean",
        "Enable rendering points as proper 3d spherical meshes, TODO: deprecate and implement as a renderer type 'particles' that can plot points or spheres"
      }
    },
    {
      "pngalpha",
      {
        false,
        "global",
        "boolean",
        "Enable transparent png output"
      }
    },
    {
      "swapyz",
      {
        false,
        "global",
        "boolean",
        "Enable imported model y/z axis swap"
      }
    },
    {
      "trisplit",
      {
        0,
        "global",
        "integer",
        "Imported model triangle subdivision level. Can also be set to 1 to force vertex normals to be recalculated by ignoring any present in the loaded model data."
      }
    },
    {
      "globalcam",
      {
        false,
        "global",
        "boolean",
        "Enable global camera for all models (default is separate cam for each)"
      }
    },
    {
      "vectorfont",
      {
        false,
        "global",
        "boolean",
        "Always use vector font regardless of individual object properties"
      }
    },
    {
      "volchannels",
      {
        1,
        "global",
        "integer",
        "Volume rendering output channels 1 (luminance) 3/4 (rgba)"
      }
    },
    {
      "volres",
      {
        {256,256,256},
        "global",
        "integer[3]",
        "Volume rendering data voxel resolution X Y Z"
      }
    },
    {
      "volmin",
      {
        {0.,0.,0.},
        "global",
        "real[3]",
        "Volume rendering min bound X Y Z"
      }
    },
    {
      "volmax",
      {
        {1.,1.,1.},
        "global",
        "real[3]",
        "Volume rendering max bound X Y Z"
      }
    },
    {
      "volsubsample",
      {
        {1,1,1},
        "global",
        "int[3]",
        "Volume rendering subsampling factor X Y Z"
      }
    },
    {
      "slicevolumes",
      {
        false,
        "global",
        "boolean",
        "Convert full volume data sets to slices (allows cropping and sub-sampling)"
      }
    },
    {
      "slicedump",
      {
        false,
        "global",
        "boolean",
        "Export full volume data sets to slices"
      }
    },
    {
      "inscale",
      {
        {1.,1.,1.},
        "global",
        "real[3]",
        "Geometry input scaling X Y Z"
      }
    },
    {
      "pointsubsample",
      {
        0,
        "global",
        "integer",
        "Point render sub-sampling factor"
      }
    },
    {
      "pointmaxcount",
      {
        0,
        "global",
        "integer",
        "Point render maximum count before auto sub-sampling"
      },
    },
    {
      "pointdistsample",
      {
        0,
        "global",
        "integer",
        "Point distance sub-sampling factor"
      }
    },
    {
      "pointattribs",
      {
        true,
        "global",
        "boolean",
        "Point size/type attributes can be applied per object (requires more GPU ram)"
      }
    },
    {
      "pointattenuate",
      {
        true,
        "global",
        "boolean",
        "Point distance size attenuation (points shrink when further from viewer ie: perspective)"
      }
    },
    {
      "sort",
      {
        true,
        "global",
        "boolean",
        "Automatic depth sorting enabled"
      }
    },
    {
      "cache",
      {
        false,
        "global",
        "boolean",
        "Cache timestep varying data in ram"
      }
    },
    {
      "gpucache",
      {
        false,
        "global",
        "boolean",
        "Cache timestep varying data on gpu as well as ram (only if model size permits)"
      }
    },
    {
      "timestep",
      {
        -1,
        "global",
        "integer",
        "Holds the current model timestep, read only, -1 indicates no time varying data loaded"
      }
    },
    {
      "data",
      {
        {},
        "global",
        "dict",
        "Holds a dictionary of data sets in the current model by label, read only"
      }
    }

  };

  for (auto p : properties)
  {
    //Copy default values
    std::string key = p.first;
    defaults[key] = p.second[PROPDEFAULT];

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
json& DrawState::global(const std::string& key)
{
  if (has(key)) return globals[key];
  return defaults[key];
}

//Return true if a global parameter has been set
bool DrawState::has(const std::string& key)
{
  return (globals.count(key) > 0 && !globals[key].is_null());
}

// Calculates a set of points on a unit circle for a given number of segments
// Used to optimised rendering circular objects when segment count isn't changed
void DrawState::cacheCircleCoords(int segment_count)
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

