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

  //Fonts
  FontManager fonts;

  //Global colourmaps list for active model
  std::vector<ColourMap*> * colourMaps;

  DrawState() : prog()
  {
    borderobj = axisobj = rulerobj = NULL;
    counter = 0;
    omegalib = false;
    //reset();
  }

  int counter;

  std::string counterFilename()
  {
    //Apply image counter to default filename when multiple images/videos output
    std::stringstream outpath;
    std::string title = global("caption");
    outpath << title;
    if (counter > 0)
      outpath <<  "-" << counter;
    counter++;
    return outpath.str();
  }

  void reset()
  {
    defaults = json::object();
    globals = json::object();

    automate = false;

    if (borderobj) delete borderobj;
    if (axisobj) delete axisobj;
    if (rulerobj) delete rulerobj;
    borderobj = axisobj = rulerobj = NULL;

    fonts.reset();

    now = -1;
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

    fonts.reset();

    colourMaps = NULL;

    //Setup default properties
    //(Comments formatted to be parsed into documentation)
    
    //Labels/text
    // | all | string | Font typeface vector/small/fixed/sans/serif
    defaults["font"] = "vector";
    // | all | real | Font scaling, note that only the vector font scales well
    defaults["fontscale"] = 1.0;
    // | all | colour | Font colour RGB(A)
    defaults["fontcolour"] = {0, 0, 0, 0};

    //Object defaults
    // | object | string | Name of object
    defaults["name"] = "";
    // | object | boolean | Set to false to hide object
    defaults["visible"] = true;
    // | object | boolean | Set to true if object remains static regardless of timestep
    defaults["static"] = false;
    // | object | string | Geometry type to load when adding new data
    defaults["geometry"] = "points";
    // | object | boolean | Apply lighting to object
    defaults["lit"] = true;
    // | object | boolean | Cull back facing polygons of object surfaces
    defaults["cullface"] = false;
    // | object | boolean | Render object surfaces as wireframe
    defaults["wireframe"] = false;
    // | object | boolean | Renders surfaces as flat shaded, lines/vectors as 2d, faster but no 3d or lighting
    defaults["flat"] = false;
    // | object | boolean | Set to false to disable depth test when drawing object so always drawn regardless of 3d position
    defaults["depthtest"] = true;
    // | object | integer[3] | width/height/depth override for geometry
    defaults["dims"] = {0, 0, 0};
    // | object | boolean | Set to true to apply the view rotation to this object
    defaults["rotatable"] = true;

    // | object | colour | Object colour RGB(A)
    defaults["colour"] = {0, 0, 0, 255};
    // | object | integer | id of the colourmap to use
    defaults["colourmap"] = -1;
    // | object | integer | id of opacity colourmap to use
    defaults["opacitymap"] = -1;
    // | object | real [0,1] | Opacity of object where 0 is transparent and 1 is opaque
    defaults["opacity"] = 1.0;
    // | object | real [-1,1] | Brightness of object from -1 (full dark) to 0 (default) to 1 (full light)
    defaults["brightness"] = 0.0;
    // | object | real [0,2] | Contrast of object from 0 (none, grey) to 2 (max)
    defaults["contrast"] = 1.0;
    // | object | real [0,2] | Saturation of object from 0 (greyscale) to 2 (fully saturated)
    defaults["saturation"] = 1.0;

    // | object | real [0,1] | Ambient lighting level (background constant light)
    defaults["ambient"] = 0.4;
    // | object | real [0,1] | Diffuse lighting level (shading light/dark)
    defaults["diffuse"] = 0.8;
    // | object | real [0,1] | Sepcular highlight lighting level (spot highlights)
    defaults["specular"] = 0.0;

    // | object | boolean | Allow object to be clipped
    defaults["clip"] = true;
    // | object | boolean | Clipping mapped to range normalised [0,1]
    defaults["clipmap"] = true;
    // | object | real [0,1] | Object clipping, minimum x
    defaults["xmin"] = -FLT_MAX;
    // | object | real [0,1] | Object clipping, maximum y
    defaults["ymin"] = -FLT_MAX;
    // | object | real [0,1] | Object clipping, minimum z
    defaults["zmin"] = -FLT_MAX;
    // | object | real [0,1] | Object clipping, maximum x
    defaults["xmax"] = FLT_MAX;
    // | object | real [0,1] | Object clipping, maximum y
    defaults["ymax"] = FLT_MAX;
    // | object | real [0,1] | Object clipping, maximum z
    defaults["zmax"] = FLT_MAX;
    // | object | object | Filter list
    defaults["filters"] = json::array();

    // | object | integer [0,n] | Glyph quality 0=none, 1=low, higher=increasing triangulation detail (arrows/shapes etc)
    defaults["glyphs"] = 2;
    // | object | real | Object scaling factor
    defaults["scaling"] = 1.0;
    // | object | string | External texture image file path to load and apply to surface or points
    defaults["texturefile"] = "";
    // | object | boolean | Flip texture image after loading, usually required
    defaults["fliptexture"] = true;
    // | object | integer [0,n] | Index of data set to colour object by (requires colour map)
    defaults["colourby"] = 0;
    // | object | integer [0,n] | Index of data set to apply transparency to object by (requires opacity map)
    defaults["opacityby"] = 1;

    // | object(line) | real | Line length limit, can be used to skip drawing line segments that cross periodic boundary
    defaults["limit"] = 0;
    // | object(line) | boolean | To chain line vertices into longer lines set to true
    defaults["link"] = false;
    // | object(line) | real | Line scaling multiplier, applies to all line objects
    defaults["scalelines"] = 1.0;
    // | object(line) | real | Line width scaling
    defaults["linewidth"] = 1.0;
    // | object(line) | boolean | Draw lines as 3D tubes
    defaults["tubes"] = false;

    // | object(point) | real | Point size scaling
    defaults["pointsize"] = 1.0;
    // | object(point) | integer/string | Point type 0/blur, 1/smooth, 2/sphere, 3/shiny, 4/flat
    defaults["pointtype"] = 1;
    // | object(point) | real | Point scaling multiplier, applies to all points objects
    defaults["scalepoints"] = 1.0;
    // | object(point) | integer [0,n] | Index of data set to apply to point sizes
    defaults["sizeby"] = 1;

    // | object(surface) | boolean | If opaque flag is set skips depth sorting step and allows individual surface properties to be applied
    defaults["opaque"] = false;
    // | object(surface) | boolean | Disable this flag to skip the mesh optimisation step
    defaults["optimise"] = true;
    // | object(surface) | boolean | Disable this flag to skip calculating vertex normals
    defaults["vertexnormals"] = true;

    // | object(volume) | real | Power used when applying transfer function, 1.0=linear mapping
    defaults["power"] = 1.0;
    // | object(volume) | integer | Number of samples to take per ray cast, higher = better quality, slower
    defaults["samples"] = 256;
    // | object(volume) | real | Density multiplier for volume data
    defaults["density"] = 5.0;
    // | object(volume) | real | Isovalue for dynamic isosurface
    defaults["isovalue"] = FLT_MAX;
    // | object(volume) | real [0,1] | Transparency value for isosurface
    defaults["isoalpha"] = 1.0;
    // | object(volume) | real | Isosurface smoothing factor for normal calculation
    defaults["isosmooth"] = 0.1;
    // | object(volume) | boolean | Connect isosurface enclosed area with walls
    defaults["isowalls"] = true;
    // | object(volume) | boolean | Apply a tricubic filter for increased smoothness
    defaults["tricubicfilter"] = false;
    // | object(volume) | real | Minimum density value to map, lower discarded
    defaults["dminclip"] = 0.0;
    // | object(volume) | real | Maximum density value to map, higher discarded
    defaults["dmaxclip"] = 1.0;
    // | object(volume) | boolean | Compress volume textures where possible
    defaults["compresstextures"] = false;
    // | object(volume) | int[3] | Volume texture size limit (for crop)
    defaults["texturesize"] = {0, 0, 0};
    // | object(volume) | int[3] | Volume texture offset (for crop)
    defaults["textureoffset"] = {0, 0, 0};

    // | object(vector) | real | Arrow head size as a multiple of width
    defaults["arrowhead"] = 2.0;
    // | object(vector) | real | Vector scaling multiplier, applies to all vector objects
    defaults["scalevectors"] = 1.0;
    // | object(vector) | real | Arrow fixed shaft radius, default is to calculate proportional to length
    defaults["radius"] = 0.0;
    // | object(vector) | boolean | Automatically scale vectors based on maximum magnitude
    defaults["autoscale"] = true;

    // | object(tracer) | integer | Number of time steps to trace particle path
    defaults["steps"] = 0;
    // | object(tracer) | boolean | Taper width of tracer arrow up as we get closer to current timestep
    defaults["taper"] = true;
    // | object(tracer) | boolean | Fade opacity of tracer arrow in from transparent as we get closer to current timestep
    defaults["fade"] = false;
    // | object(tracer) | real | Tracer scaling multiplier, applies to all tracer objects
    defaults["scaletracers"] = 1.0;

    // | object(shape) | real | Shape width scaling factor
    defaults["shapewidth"] = 1.0;
    // | object(shape) | real | Shape height scaling factor
    defaults["shapeheight"] = 1.0;
    // | object(shape) | real | Shape length scaling factor
    defaults["shapelength"] = 1.0;
    // | object(shape) | integer | Shape type: 0=ellipsoid, 1=cuboid
    defaults["shape"] = 0;
    // | object(shape) | real | Shape scaling multiplier, applies to all shape objects
    defaults["scaleshapes"] = 1.0;
    // | object(shape) | integer [0,n] | Index of data set to apply to shape widths
    defaults["widthby"] = 1;
    // | object(shape) | integer [0,n] | Index of data set to apply to shape heights
    defaults["heightby"] = 2;
    // | object(shape) | integer [0,n] | Index of data set to apply to shape lengths
    defaults["lengthby"] = 3;

    // | colourbar | boolean | Indicates object is a colourbar
    defaults["colourbar"] = false;
    // | colourbar | string | Alignment of colour bar to screen edge, top/bottom/left/right
    defaults["align"] = "bottom";
    // | colourbar | real[2] | Dimensions of colour bar (length/breadth) in pixels or viewport size ratio
    defaults["size"] = {0, 0};
    // | colourbar | integer | Number of additional tick marks to draw besides start and end
    defaults["ticks"] = 0;
    // | colourbar | float[] | Values of intermediate tick marks
    defaults["tickvalues"] = json::array();
    // | colourbar | boolean | Set to false to disable drawing of tick values
    defaults["printticks"] = true;
    // | colourbar | string | Format specifier for tick values, eg: %.3[f/e/g] standard, scientific, both
    defaults["format"] = "%.5g";
    // | colourbar | real | Multiplier to scale tick values
    defaults["scalevalue"] = 1.0;
    // | colourbar | integer | Outline width to draw around colour bar
    defaults["outline"] = 1.0;
    // | colourbar | real | Margin to parallel edge in pixels or viewport size ratio
    defaults["offset"] = 0;
    // | colourbar | real | Margin to perpendicular edge in pixels or viewport size ratio, >0=towards left/bottom, 0=centre (horizontal only), <0=towards right/top
    defaults["position"] = 0;

    // | colourmap | boolean | Set to true to use log scales
    defaults["logscale"] = false;
    // | colourmap | boolean | Set to true to apply colours as discrete values rather than gradient
    defaults["discrete"] = false;
    // | colourmap | colours | Colour list, see [Colour map lists] for more information
    defaults["colours"] = "";
    // | colourmap | real[2] | Fixed scale range, default is to automatically calculate range based on data min/max
    defaults["range"] = {0.0, 0.0};
    // | colourmap | boolean | Set to true to lock colourmap ranges to current values
    defaults["locked"] = false;

    // | view | string | Title to display at top centre of view
    defaults["title"] = "";
    // | view | integer | When to apply camera auto-zoom to fit model to window, -1=never, 0=first timestep only, 1=every timestep
    defaults["zoomstep"] = -1;
    // | view | integer | Margin in pixels to leave around edge of model when to applying camera auto-zoom
    defaults["margin"] = 20;
    // | view | boolean | Draw rulers around object axes
    defaults["rulers"] = false;
    // | view | string | Which figure axes to draw rulers beside (xyzXYZ) lowercase = min, capital = max 
    defaults["ruleraxes"] = "xyz";
    // | view | integer | Number of tick marks to display on rulers
    defaults["rulerticks"] = 5;
    // | view | real | Width of ruler lines
    defaults["rulerwidth"] = 1.5;
    // | view | integer | Border width around model boundary, 0=disabled
    defaults["border"] = 1.0;
    // | view | boolean | Draw filled background box around model boundary
    defaults["fillborder"] = false;
    // | view | colour | Colour of model boundary border
    defaults["bordercolour"] = "grey";
    // | view | boolean | Draw X/Y/Z legend showing model axes orientation
    defaults["axis"] = true;
    // | view | real | Axis legend scaling factor
    defaults["axislength"] = 0.1;
    // | view | boolean | Draw a timestep label at top right of view - CURRENTLY NOT IMPLEMENTED
    defaults["timestep"] = false;
    // | view | boolean | Enable multisample anti-aliasing, only works with interactive viewing
    defaults["antialias"] = true; //Should be global
    // | view | real | Apply a shift to object depth sort index by this amount multiplied by id, improves visualising objects drawn at same depth
    defaults["shift"] = 0.;
    //View: Camera
    // | view | real[4] | Camera rotation quaternion [x,y,z,w]
    defaults["rotate"] = {0., 0., 0., 1.};
    // | view | real[3] | Camera translation [x,y,z]
    defaults["translate"] = {0., 0., 0.};
    // | view | real[3] | Camera focal point [x,y,z]
    defaults["focus"] = {0., 0., 0.};
    // | view | real[3] | Global model scaling factors [x,y,z]
    defaults["scale"] = {1., 1., 1.};
    // | view | real[3] | Global model minimum bounds [x,y,z]
    defaults["min"] = {0, 0, 0};
    // | view | real[3] | Global model maximum bounds [x,y,z]
    defaults["max"] = {0, 0, 0};
    // | view | real | Near clipping plane position, adjusts where geometry close to the camera is clipped
    defaults["near"] = 0.0;
    // | view | real | Far clip plane position, adjusts where far geometry is clipped
    defaults["far"] = 0.0;
    // | view | integer | Set to determine coordinate system, 1=Right-handed (OpenGL default) -1=Left-handed
    defaults["coordsystem"] = 1;

    //Global Properties
    // | global | string | Title of window for caption area
    defaults["caption"] = "LavaVu";
    // | global | integer[2] | Window resolution X,Y
    defaults["resolution"] = {1024, 768};
    // | global | boolean | Turn on to display FPS count
    defaults["fps"] = false;
    // | global | boolean | Turn on to automatically add and switch to a new timestep after loading a data file
    defaults["filestep"] = false;
    // | global | boolean | Turn on to set initial state of all loaded objects to hidden
    defaults["hideall"] = false;
    // | global | colour | Background colour RGB(A)
    defaults["background"] = {0, 0, 0, 255};
    // | global | real [0,1] | Global opacity multiplier where 0 is transparent and 1 is opaque, this is combined with "opacity" prop
    defaults["alpha"] = 1.0;
    // | global | boolean | Disables initial loading of object data from database, only object names loaded, use the "load" command to subsequently load selected object data
    defaults["noload"] = false;
    // | global | boolean | Enable rendering points as proper 3d spherical meshes
    defaults["pointspheres"] = false;
    // | global | boolean | Enable transparent png output
    defaults["pngalpha"] = false;
    // | global | boolean | Enable imported model y/z axis swap
    defaults["swapyz"] = false;
    // | global | integer | Imported model triangle subdivision level
    defaults["trisplit"] = 0;
    // | global | boolean | Enable global camera for all models (default is separate cam for each)
    defaults["globalcam"] = false;
    // | global | boolean | Always use vector font regardless of individual object properties
    defaults["vectorfont"] = false;
    // | global | integer | Volume rendering output channels 1 (luminance) 3/4 (rgba)
    defaults["volchannels"] = 1;
    // | global | integer[3] | Volume rendering data voxel resolution X Y Z
    defaults["volres"] = {256, 256, 256};
    // | global | real[3] | Volume rendering min bound X Y Z
    defaults["volmin"] = {0., 0., 0.};
    // | global | real[3] | Volume rendering max bound X Y Z
    defaults["volmax"] = {1., 1., 1.};
    // | global | int[3] | Volume rendering subsampling factor X Y Z
    defaults["volsubsample"] = {1, 1, 1};
    // | global | boolean | Convert full volume data sets to slices (allows cropping and sub-sampling)
    defaults["slicevolumes"] = false;
    // | global | boolean | Export full volume data sets to slices
    defaults["slicedump"] = false;
    // | global | real[3] | Geometry input scaling X Y Z
    defaults["inscale"] = {1., 1., 1.};
    // | global | integer | Point render sub-sampling factor
    defaults["pointsubsample"] = 0;
    // | global | integer | Point distance sub-sampling factor
    defaults["pointdistsample"] = 0;
    // | global | boolean | Point size/type attributes can be applied per object (requires more GPU ram)
    defaults["pointattribs"] = true;
    // | global | boolean | Point distance size attenuation (points shrink when further from viewer ie: perspective)
    defaults["pointattenuate"] = true;
    // | global | integer | Automatic depth sorting, -1=on, 0=disabled, >0=timer
    defaults["sort"] = -1;
    // | global | boolean | Cache timestep varying data in ram
    defaults["cache"] = false;
    // | global | boolean | Cache timestep varying data on gpu as well as ram (only if model size permits)
    defaults["gpucache"] = false;

#ifdef DEBUG
    //std::cerr << std::setw(2) << defaults << std::endl;
#endif
  }

  //Return a global parameter (or default if not set)
  json& global(const std::string& key)
  {
    if (has(key)) return globals[key];
    return defaults[key];
  }

  //Return true if a global parameter has been set
  bool has(const std::string& key)
  {
    return (globals.count(key) > 0 && !globals[key].is_null());
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

