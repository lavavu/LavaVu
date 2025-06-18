
# Property reference

 * [all](#all)
 * [timestep](#timestep)
 * [object](#object)
 * [object,line](#object-line)
 * [object,point](#object-point)
 * [object,surface](#object-surface)
 * [object,volume](#object-volume)
 * [object,vector](#object-vector)
 * [object,tracer](#object-tracer)
 * [object,shape](#object-shape)
 * [colourbar](#colourbar)
 * [colourmap](#colourmap)
 * [view](#view)
 * [global](#global)
 * [view](#view)
 * [global](#global)

## all

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*renderers*       | object     | [...]          | Holds list of available renderer types that can be created and their aliases, grouped in order of base types (label/point/grid/tri/vector/tracer/line/shape/volume/screen) read only.|
|*renderlist*      | string     | ""             | Sets a list of renderers to create and order they are displayed. This allows overriding the default renderer order, defined by the object order. For valid types see "renderers" property.|
|*subrenderers*    | string     | "sortedtriangles points simplelines" | List of renderers created for rendering primitives (points, lines, triangles) from more complex renderers (eg: vectors). Allows selection of different primitive rendering modes, globally or per object.|
|*font*            | string     | "vector"       | Font typeface vector (thicker, better for larger text) or line (better for small labels)|
|*fontscale*       | real       | 1.0            | Font scaling, applied by multiplying with any existing calculated scaling.|
|*fontsize*        | real       | 1.0            | Font size, absolute scaling that overrides all automatic calculations of the font size|
|*fontcolour*      | colour     | [0,0,0,0]      | Font colour RGB(A)|

## timestep

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*time*            | real       | 0.0            | Time value for timestep|

## object

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*name*            | string     | ""             | Name of object|
|*visible*         | boolean    | true           | Set to false to hide object|
|*inview*          | boolean    | true           | Set to false to exclude object from bounding box calculation and the default camera view|
|*fixed*           | boolean    | false          | Set to true to make all data fixed, not time varying, for this object|
|*renderer*        | string     | ""             | Create a custom renderer using label provided instead of using the default renderers, type of renderer created based on the "geometry" property|
|*shaders*         | object     | []             | Custom shaders for rendering object, either filenames or source strings, provide either [fragment], [vertex, fragment] or [geometry, vertex, fragment]|
|*uniforms*        | object     | {}             | Custom shader uniforms for rendering objects, dict of uniform names/values, will be copied from property data|
|*steprange*       | boolean    | true           | Calculate dynamic range values per step rather than over full time range|
|*lit*             | boolean    | true           | Apply lighting to object|
|*cullface*        | boolean    | false          | Cull back facing polygons of object surfaces|
|*wireframe*       | boolean    | false          | Render object surfaces as wireframe|
|*flat*            | boolean    | false          | Renders surfaces as flat shaded, lines/vectors/tracers as 2d, faster but no 3d or lighting|
|*depthtest*       | boolean    | true           | Set to false to disable depth test when drawing object so always drawn regardless of 3d position|
|*depthwrite*      | boolean    | true           | Set to false to disable depth buffer write when drawing object, so other objects behind it will still be drawn and will appear in front if drawn after this object|
|*dims*            | integer[3] | [0,0,0]        | Width,Height,Depth override for geometry. Usually this data will be determined from the numpy array shape when loaded from python but setting this property overrides the shape, unless it doesn't match the data size. Note this property is the reverse of the numpy array shape (Depth,Height,Width) or (Height,Width).|
|*shift*           | real       | 0.0            | Apply a shift to object position by this amount multiplied by model size*10^-7, to fix depth fighting when visualising objects drawn at same depth|
|*colour*          | colour     | [0,0,0,255]    | Object colour RGB(A)|
|*colourmap*       | string     | ""             | name of the colourmap to use|
|*opacitymap*      | string     | ""             | name of the opacity colourmap to use|
|*opacity*         | real       | 1.0            | Opacity of object where 0 is transparent and 1 is opaque|
|*brightness*      | real       | 0.0            | Brightness of object from -1 (full dark) to 0 (default) to 1 (full light)|
|*contrast*        | real       | 1.0            | Contrast of object from 0 (none, grey) to 2 (max)|
|*saturation*      | real       | 1.0            | Saturation of object from 0 (greyscale) to 2 (fully saturated)|
|*ambient*         | real       | 0.4            | Ambient lighting level (background constant light)|
|*diffuse*         | real       | 0.65           | Diffuse lighting level (shading light/dark)|
|*specular*        | real       | 0.0            | Specular highlight lighting level (spot highlights)|
|*shininess*       | real       | 0.5            | Specular shininess factor, controls size of highlight|
|*light*           | real[4]    | [1.0,1.0,1.0,0.0] | Light properties, colour Red,Green,Blue values and fourth component if provided is two-sided lighting flag, set to 1.0 to switch to single-sided lighting|
|*lightpos*        | real[4]    | [0.1,-0.1,2.0,0.0] | Light position X Y Z. Default is relative to camera (light moves follows camera, rotating the scene changes the illuminated area). Fourth component, if provided, is scene light flag, set to 1.0 to switch to a position in the scene, regardless of camera/view position the same parts of the scene will be illuminated.|
|*clip*            | boolean    | true           | Allow object to be clipped|
|*clipmap*         | boolean    | true           | Clipping mapped to range normalised [0,1]|
|*clipmin*         | real[3]    | [0.0,0.0,0.0]  | Object clipping minimum [x,y,z]|
|*clipmax*         | real[3]    | [1.0,1.0,1.0]  | Object clipping maximum [x,y,z]|
|*xmin*            | real       | 0.0            | (legacy) Object clipping, minimum x|
|*ymin*            | real       | 0.0            | (legacy) Object clipping, maximum y|
|*zmin*            | real       | 0.0            | (legacy) Object clipping, minimum z|
|*xmax*            | real       | 1.0            | (legacy) Object clipping, maximum x|
|*ymax*            | real       | 1.0            | (legacy) Object clipping, maximum y|
|*zmax*            | real       | 1.0            | (legacy) Object clipping, maximum z|
|*filters*         | object     | []             | Filter list|
|*glyphs*          | integer    | 2              | Glyph quality 0=none, 1=low, higher=increasing triangulation detail (arrows/shapes etc)|
|*scaling*         | real       | 1.0            | Object scaling factor|
|*texture*         | string     | ""             | Apply a texture, either external texture image file path to load or colourmap|
|*fliptexture*     | boolean    | true           | Flip texture image after loading, usually required|
|*repeat*          | boolean    | false          | Repeat texture image enabled, default is clamp to edges|
|*texturefilter*   | integer    | 2              | Type of texture filtering, 0=nearest, 1=linear, 2=mipmap (default)|
|*colourby*        | string or integer | 0              | Index or label of data set to colour object by (requires colour map)|
|*opacityby*       | string or integer | ""             | Index or label of data set to apply transparency to object by (requires opacity map)|

## object-line

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*limit*           | real       | 0              | Line length limit, can be used to skip drawing line segments that cross periodic boundary|
|*link*            | boolean    | false          | To chain line vertices into longer lines set to true|
|*loop*            | boolean    | false          | To join the start and end of linked lines set to true|
|*scalelines*      | real       | 1.0            | Line scaling multiplier, applies to all line objects|
|*linewidth*       | real       | 1.0            | Line width scaling|
|*tubes*           | boolean    | false          | Draw lines as 3D tubes|

## object-point

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*pointsize*       | real       | 1.0            | Point size scaling|
|*pointtype*       | integer/string | 1              | Point type 0/blur, 1/smooth, 2/sphere, 3/shiny, 4/flat|
|*scalepoints*     | real       | 1.0            | Point scaling multiplier, applies to all points objects|
|*sizeby*          | string or integer | ""             | Index or label of data set to apply to point sizes|

## object-surface

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*opaque*          | boolean    | false          | If opaque flag is set skips depth sorting step and allows individual surface properties to be applied|
|*optimise*        | boolean    | true           | Disable this flag to skip the mesh optimisation step|
|*vertexnormals*   | boolean    | true           | Disable this flag to skip calculating vertex normals|
|*smoothangle*     | int        | 90             | Angle between surface normals (in degrees) below which normals will be smoothed. Reduce this to preserve hard edges.|
|*flip*            | boolean    | false          | Set this flag to reverse the surface faces (currently implemented for grids only)|

## object-volume

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*power*           | real       | 1.0            | Power used when applying transfer function, 1.0=linear mapping|
|*bloom*           | real       | 0.0            | Bloom effect, brightens colours, particularly around edges of transparent areas, by using a more additive blend equation. Setting this to ~0.5 will replicate the old (technically incorrect) volume shader blending for lavavu pre v1.5.|
|*samples*         | integer    | 256            | Number of samples to take per ray cast, higher = better quality, but slower|
|*density*         | real       | 5.0            | Density multiplier for volume data|
|*isovalue*        | real       | 0.0            | Isovalue for dynamic isosurface (normalised to [0,1] over actual data range)|
|*isovalues*       | real[]     | []             | Isovalues to extract from volume into mesh isosurface|
|*isoalpha*        | real       | 1.0            | Transparency value for isosurface|
|*isosmooth*       | real       | 0.1            | Isosurface smoothing factor for normal calculation|
|*isowalls*        | boolean    | true           | Connect isosurface enclosed area with walls|
|*tricubicfilter*  | boolean    | false          | Apply a tricubic filter for increased smoothness|
|*densityclip*     | real[2]    | [0.0,1.0]      | Density range [min,max] to map, values outside the range are discarded|
|*minclip*         | real       | 0.0            | (legacy) Minimum density value to map, lower discarded|
|*maxclip*         | real       | 1.0            | (legacy) Maximum density value to map, higher discarded|
|*compresstextures*| boolean    | false          | Compress volume textures where possible|
|*texturesize*     | int[3]     | [0,0,0]        | Volume texture size limit (for crop)|
|*textureoffset*   | int[3]     | [0,0,0]        | Volume texture offset (for crop)|

## object-vector

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*scalemax*        | real       | 0.0            | Length scaling maximum, sets the range over which vectors will be scaled [0,scalemax]. Default automatically calculated based on data max|
|*arrowhead*       | real       | 5.0            | Arrow head size as a multiple of length or radius, if < 1.0 is multiple of length, if > 1.0 is multiple of radius|
|*scalevectors*    | real       | 1.0            | Vector scaling multiplier, applies to all vector objects|
|*thickness*       | real       | 0.0            | Arrow shaft thickness as fixed value (overrides "radius")|
|*length*          | real       | 0.0            | Arrow fixed length, default is to use vector magnitude|
|*normalise*       | real       | 1.0            | Normalisation factor to adjust between vector arrows scaled to their vector length or all arrows having a constant length. If 0.0 vectors are scaled to their vector length, if 1.0 vectors are all scaled to the constant "length" property (if property length=0.0, this is ignored).|
|*autoscale*       | boolean    | true           | Automatically scale vectors based on maximum magnitude|
|*radius*          | real       | 0.02           | When applied to Vector Arrows: Arrow shaft radius as ratio of vector length. When applied to shapes: radius of spheres.|

## object-tracer

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*steps*           | integer    | 0              | Number of time steps to trace particle path|
|*taper*           | boolean    | true           | Taper width of tracer arrow up as we get closer to current timestep|
|*fade*            | boolean    | false          | Fade opacity of tracer arrow in from transparent as we get closer to current timestep|
|*connect*         | boolean    | true           | Set false to render tracers as points instead of connected lines|
|*scaletracers*    | real       | 1.0            | Tracer scaling multiplier, applies to all tracer objects|

## object-shape

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*shapewidth*      | real       | 1.0            | Shape width scaling factor|
|*shapeheight*     | real       | 1.0            | Shape height scaling factor|
|*shapelength*     | real       | 1.0            | Shape length scaling factor|
|*shape*           | integer    | 0              | Shape type: 0=ellipsoid, 1=cuboid|
|*scaleshapes*     | real       | 1.0            | Shape scaling multiplier, applies to all shape objects|
|*widthby*         | string or integer | "widths"       | Index or label of data set to apply to shape widths|
|*heightby*        | string or integer | "heights"      | Index or label of data set to apply to shape heights|
|*lengthby*        | string or integer | "lengths"      | Index or label of data set to apply to shape lengths|
|*segments*        | integer    | 32             | Triangulation quality for spheres, number of segments to draw (minimum 4)|
|*rotation*        | quaternion | [0.0,0.0,0.0,1.0] | Shape rotation about Y axis, applies to all shape objects, [x,y,z,y] or [X,Y,Z]|

## colourbar

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*colourbar*       | boolean    | false          | Indicates object is a colourbar|
|*align*           | string     | "bottom"       | Alignment of colour bar to screen edge, top/bottom/left/right|
|*size*            | real[2]    | [0,0]          | Dimensions of colour bar (length/breadth) in pixels or viewport size ratio|
|*ticks*           | integer    | 0              | Number of additional tick marks to draw besides start and end|
|*tickvalues*      | real[]     | []             | Values of intermediate tick marks|
|*printticks*      | boolean    | true           | Set to false to disable drawing of intermediate tick values|
|*format*          | string     | "%.5g"         | Format specifier for label values, eg: %.3[f/e/g] standard, scientific, both|
|*scalevalue*      | real       | 1.0            | Multiplier to scale tick values|
|*outline*         | integer    | 1.0            | Outline width to draw around colour bar|
|*offset*          | real       | 0              | Margin to parallel edge in pixels or viewport size ratio|
|*position*        | real       | 0              | Margin to perpendicular edge in pixels or viewport size ratio, >0=towards left/bottom, 0=centre (horizontal only), <0=towards right/top|
|*binlabels*       | boolean    | false          | Set to true to label discrete bins rather than tick points|

## colourmap

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*logscale*        | boolean    | false          | Set to true to use log scales|
|*discrete*        | boolean    | false          | Set to true to apply colours as discrete values rather than gradient|
|*interpolate*     | boolean    | true           | Set to false to disable interpolation between colours, nearest colour will always be applied|
|*colours*         | colours    | []             | Colour list (or string), X11 colour names, rgb(a) colours or html hex colours|
|*range*           | real[2]    | [0.0,0.0]      | Fixed scale range, default is to automatically calculate range based on data min/max|
|*locked*          | boolean    | false          | Set to true to lock colourmap ranges to current values|

## view

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*zoomstep*        | integer    | -1             | When to apply camera auto-zoom to fit model to window, -1=never, 0=first timestep only, 1=every timestep|
|*margin*          | real       | 0.025          | Margin in pixels (when >= 1) or ratio of viewport width (when < 1.0) around edges of model when to applying camera auto-zoom|
|*rotate*          | quaternion | [0.0,0.0,0.0,1.0] | Camera rotation quaternion [x,y,z,w], if applied to an object rotates the object, can be quaternion or Euler angles [x,y,z] in this case|
|*xyzrotate*       | real[3]    | [0.0,0.0,0.0]  | Camera rotation as Euler angles [x,y,z] - output only|
|*translate*       | real[3]    | [0.0,0.0,0.0]  | Camera translation [x,y,z]|
|*focus*           | real[3]    | [0.0,0.0,0.0]  | Camera focal point [x,y,z]|
|*origin*          | real[3]    | [0.0,0.0,0.0]  | Origin / centre of rotation [x,y,z] if not set, defaults to same as focal point.|
|*scale*           | real[3]    | [1.0,1.0,1.0]  | Global model scaling factors [x,y,z]|
|*near*            | real       | 0.0            | Near clipping plane position, adjusts where geometry close to the camera is clipped|
|*far*             | real       | 0.0            | Far clip plane position, adjusts where far geometry is clipped|
|*fov*             | real       | 45.0           | Field-Of-View (horizontal) in degrees|
|*orthographic*    | boolean    | false          | Enable to switch to an orthographic projection instead of the default perspective projection|
|*coordsystem*     | integer    | 1              | Set to determine coordinate system, 1=Right-handed (OpenGL default) -1=Left-handed|
|*follow*          | boolean    | false          | Enable to follow the model bounding box centre with camera as it changes|
|*port*            | real[4]    | [0,0,1.0,1.0]  | Viewport position and dimensions array [x,y,width,height] in normalised coords [0,1]|

## global

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*min*             | real[3]    | [0,0,0]        | Global model minimum bounds [x,y,z]|
|*max*             | real[3]    | [0,0,0]        | Global model maximum bounds [x,y,z]|

## view

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*title*           | string     | ""             | Title to display at top centre of view|

## global

| Property         | Type       | Default            | Description                               |
| ---------------- | ---------- | ------------------ | ----------------------------------------- |
|*filename*        | string     | ""             | Active database filename|
|*compression*     | integer    | 1              | Set zlib compression level for GLDB, -1=default, 0=None, 1=fast, 9=best. LavaVu default is 1 (fast).|
|*rulers*          | boolean    | false          | Draw rulers around object axes|
|*ruleraxes*       | string     | "xyz"          | Which figure axes to draw rulers beside (xyzXYZ) lowercase = min, capital = max |
|*rulerticks*      | integer    | 5              | Number of tick marks to display on rulers|
|*rulerformat*     | string     | "%-10.3f"      | Format specifier for ruler tick values, eg: %.3[f/e/g] standard, scientific, both|
|*rulerlabels*     | string[][] | []             | Tick labels to display on rulers, 2d array, one dimension per model axis, replaces rulerticks. If values are numeric, will define the position value, otherwise define the label only.|
|*rulerwidth*      | real       | 1.5            | Width of ruler lines|
|*rulerscale*      | real       | 1.0            | Scaling of ruler label text|
|*border*          | real       | 1.0            | Border width around model boundary, 0=disabled|
|*fillborder*      | boolean    | false          | Draw filled background box around model boundary|
|*bordercolour*    | colour     | "grey"         | Colour of model boundary border|
|*axis*            | boolean    | true           | Draw X/Y/Z legend showing model axes orientation|
|*axislength*      | real       | 0.1            | Axis legend scaling factor|
|*axisbox*         | real[4]    | [0,0,0,0]      | Custom dimensions of viewport for drawing the axis (x, y, width, height) proportional to main viewport size|
|*quality*         | integer    | 2              | Read only: Over-sample antialiasing level, for off-screen rendering|
|*bounds*          | real[][]   | []             | Read only bounding box (min/max)|
|*caption*         | string     | "default"      | Title for caption area and image output filenames|
|*resolution*      | integer[2] | [1024,768]     | Window resolution X,Y|
|*antialias*       | boolean    | true           | Enable multisample anti-aliasing, only works with interactive viewing|
|*upscalelines*    | real       | false          | Enable to scale lines with the viewport size.|
|*fps*             | boolean    | false          | Turn on to display FPS count|
|*filestep*        | boolean    | false          | Turn on to automatically add and switch to a new timestep after loading a data file|
|*hideall*         | boolean    | false          | Turn on to set initial state of all loaded objects to hidden|
|*background*      | colour     | [0,0,0,255]    | Background colour RGB(A)|
|*alpha*           | real       | 1.0            | Global opacity multiplier where 0 is transparent and 1 is opaque, this is combined with "opacity" prop|
|*noload*          | boolean    | false          | Disables initial loading of object data from database, only object names loaded, use the "load" command to subsequently load selected object data|
|*merge*           | boolean    | false          | Enable to load subsequent databases into the current model, if disabled then each database is loaded into a new model|
|*pngalpha*        | boolean    | false          | Enable transparent png output|
|*trisplit*        | integer    | 0              | Imported model triangle subdivision level. Can also be set to 1 to force vertex normals to be recalculated by ignoring any present in the loaded model data.|
|*trilimit*        | real       | 0              | Imported model triangle edge size limit. Triangles will be split until edges below this threashold.|
|*globalcam*       | boolean    | false          | Enable global camera for all models (default is separate cam for each)|
|*volchannels*     | integer    | 1              | Volume rendering output channels 1 (luminance) 3/4 (rgba)|
|*volres*          | integer[3] | [256,256,256]  | Volume rendering data voxel resolution X Y Z|
|*volmin*          | real[3]    | [0.0,0.0,0.0]  | Volume rendering min bound X Y Z|
|*volmax*          | real[3]    | [1.0,1.0,1.0]  | Volume rendering max bound X Y Z|
|*volsubsample*    | int[3]     | [1,1,1]        | Volume rendering subsampling factor X Y Z|
|*slicevolumes*    | boolean    | false          | Convert full volume data sets to slices (allows cropping and sub-sampling)|
|*slicedump*       | boolean    | false          | Export full volume data sets to slices|
|*pointsubsample*  | integer    | 0              | Point render sub-sampling factor|
|*pointmaxcount*   | integer    | 0              | Point render maximum count before auto sub-sampling|
|*pointdistsample* | integer    | 0              | Point distance sub-sampling factor|
|*pointattribs*    | boolean    | true           | Point size/type attributes can be applied per object (requires more GPU ram)|
|*pointattenuate*  | boolean    | true           | Point distance size attenuation (points shrink when further from viewer ie: perspective)|
|*pointpixelscale* | int        | 1              | Set to zero for constant point size in pixels, set to 1 to scale points when the viewport is resized after storing the initial render size. If set to > 1, this is the viewport height where pointsize = pixels. As the viewport height is adjusted points are scaled relative to this height - so points will appear the same regardless of render size|
|*sort*            | boolean    | true           | Automatic depth sorting enabled|
|*cache*           | boolean    | false          | Cache all time varying data in ram on initial load|
|*gpucache*        | boolean    | false          | Cache timestep varying data on gpu as well as ram (only if model size permits)|
|*clearstep*       | boolean    | false          | Clear all time varying data from previous step on loading another|
|*timestep*        | integer    | -1             | Holds the current model timestep, read only, -1 indicates no time varying data loaded|
|*validate*        | boolean    | true           | Disable to turn off validation of property names from the dictionary. Allows setting/reading custom properties.|
|*data*            | dict       | null           | Holds a dictionary of data sets in the current model by label, read only|
