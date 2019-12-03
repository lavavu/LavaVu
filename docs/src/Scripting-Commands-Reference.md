
# Scripting command reference

 - **[General](#general-commands)**  

[quit](#quit), [repeat](#repeat), [animate](#animate), [history](#history), [clearhistory](#clearhistory), [pause](#pause), [list](#list), [step](#step), [timestep](#timestep), [jump](#jump), [model](#model), [reload](#reload), [redraw](#redraw), [clear](#clear)
 - **[Input](#input-commands)**  

[file](#file), [script](#script), [figure](#figure), [savefigure](#savefigure), [view](#view), [scan](#scan)
 - **[Output](#output-commands)**  

[image](#image), [images](#images), [outwidth](#outwidth), [outheight](#outheight), [movie](#movie), [record](#record), [bake](#bake), [bakecolour](#bakecolour), [glaze](#glaze), [export](#export), [csv](#csv), [json](#json), [save](#save), [tiles](#tiles)
 - **[View](#view-commands)**  

[rotate](#rotate), [rotatex](#rotatex), [rotatey](#rotatey), [rotatez](#rotatez), [rotation](#rotation), [zoom](#zoom), [translate](#translate), [translatex](#translatex), [translatey](#translatey), [translatez](#translatez), [translation](#translation), [autorotate](#autorotate), [spin](#spin), [focus](#focus), [fov](#fov), [focallength](#focallength), [eyeseparation](#eyeseparation), [nearclip](#nearclip), [farclip](#farclip), [zoomclip](#zoomclip), [zerocam](#zerocam), [reset](#reset), [bounds](#bounds), [fixbb](#fixbb), [camera](#camera), [resize](#resize), [fullscreen](#fullscreen), [fit](#fit), [autozoom](#autozoom), [stereo](#stereo), [coordsystem](#coordsystem), [sort](#sort)
 - **[Object](#object-commands)**  

[hide](#hide), [show](#show), [delete](#delete), [load](#load), [select](#select), [add](#add), [append](#append), [name](#name), [isosurface](#isosurface), [texture](#texture)
 - **[Display](#display-commands)**  

[background](#background), [alpha](#alpha), [axis](#axis), [rulers](#rulers), [antialias](#antialias), [valuerange](#valuerange), [colourmap](#colourmap), [colourbar](#colourbar), [pointtype](#pointtype), [pointsample](#pointsample), [border](#border), [title](#title), [scale](#scale), [modelscale](#modelscale)
 - **[Scripting](#scripting-commands)**  

[next](#next), [play](#play), [stop](#stop), [open](#open), [interactive](#interactive), [display](#display)
 - **[Miscellanious](#miscellanious-commands)**  

[shaders](#shaders), [blend](#blend), [props](#props), [defaults](#defaults), [test](#test), [voltest](#voltest), [newstep](#newstep), [filter](#filter), [filterout](#filterout), [filtermin](#filtermin), [filtermax](#filtermax), [clearfilters](#clearfilters), [verbose](#verbose), [toggle](#toggle), [createvolume](#createvolume), [clearvolume](#clearvolume), [palette](#palette)

---
## General commands:


### quit

 > Quit the program  


### repeat

 > Repeat commands from history  
 >   
 > **Usage:** repeat [count=1] [N=1]  
 >   
 > count (integer) : repeat commands count times  
 > N (integer) : repeat the last N commands  


### animate

 > Update display between each command  
 >   
 > **Usage:** animate rate  
 >   
 > rate (integer) : animation timer to fire every (rate) msec (default: 50)  
 > When on, if multiple commands are issued the frame is re-rendered at set framerate  
 > When off, all commands will be processed before the display is updated  


### history

 > Write command history to output (eg: terminal)  
 >   
 > **Usage:** history [filename]  
 >   
 > filename (string) : optional file to write data to  


### clearhistory

 > Clear command history  


### pause

 > Pause execution (for scripting)  
 >   
 > **Usage:** pause msecs  
 >   
 > msecs (integer) : how long to pause for in milliseconds  


### list

 > List available data  
 >   
 > **Usage:** list objects/colourmaps/elements  
 >   
 > objects : enable object list (stays on screen until disabled)  
 >           (dimmed objects are hidden or not in selected viewport)  
 > colourmaps : show colourmap list  
 > elements : show geometry elements by id in terminal  
 > data : show available data sets in selected object or all  


### step

 > Set timestep to view  
 >   
 > **Usage:** timestep -/+/value  
 >   
 > value (integer) : the timestep to view  
 > - : switch to previous timestep if available  
 > + : switch to next timestep if available  


### timestep

 > Set timestep to view  
 >   
 > **Usage:** timestep -/+/value  
 >   
 > value (integer) : the timestep to view  
 > - : switch to previous timestep if available  
 > + : switch to next timestep if available  


### jump

 > Jump from current timestep forward/backwards  
 >   
 > **Usage:** jump value  
 >   
 > value (integer) : how many timesteps to jump (negative for backwards)  


### model

 > Set model to view (when multiple models loaded)  
 >   
 > **Usage:** model up/down/value  
 >   
 > value (integer) : the model index to view [1,n]  
 > up : switch to previous model if available  
 > down : switch to next model if available  
 > add : add a new model  


### reload

 > Reload all data of current model/timestep from database  
 >   
 > **Usage:** reload [object_id]  
 >   
 > If an object is selected, reload will apply to that object only  
 > (If no database, will still reload data to gpu)  


### redraw

 > Redraw all object data, required after changing some parameters but may be slow  


### clear

 > Clear all data of current model/timestep  
 >   
 > **Usage:** clear [objects]  
 >   
 > objects : optionally clear all object entries  
 >           (if omitted, only the objects geometry data is cleared)  


---
## Input commands:


### file

 > Load database or model file  
 > **Usage:** file "filename"  
 >   
 > filename (string) : the path to the file, quotes necessary if path contains spaces  


### script

 > Run script file  
 > Load a saved script of viewer commands from a file  
 >   
 > **Usage:** script filename  
 >   
 > filename (string) : path and name of the script file to load  


### figure

 > Set figure to view  
 >   
 > **Usage:** figure up/down/value  
 >   
 > value (integer/string) : the figure index or name to view  
 > up : switch to previous figure if available  
 > down : switch to next figure if available  


### savefigure

 > Store current vis settings in a figure  
 >   
 > **Usage:** figure name  
 >   
 > name (string) : the figure name to save  
 >                 if ommitted, currently active or default figure will be saved  
 >                 if it doesn't exist it will be created  


### view

 > Set view (when available)  
 >   
 > **Usage:** view up/down/value  
 >   
 > value (integer) : the view index to switch to  
 > up : switch to previous view if available  
 > down : switch to next view if available  


### scan

 > Rescan the current directory for timestep database files  
 > based on currently loaded filename  
 >   


---
## Output commands:


### image

 > Save an image of the current view  
 > **Usage:** image [filename]  
 >   
 > filename (string) : optional base filename without extension  


### images

 > Write images in sequence from current timestep to specified timestep  
 >   
 > **Usage:** images endstep  
 >   
 > endstep (integer) : last frame timestep  


### outwidth

 > Set output image width (height calculated to match window aspect)  
 >   
 > **Usage:** outwidth value  
 >   
 > value (integer) : width in pixels for output images  


### outheight

 > Set output image height  
 >   
 > **Usage:** outheight value  
 >   
 > value (integer) : height in pixels for output images  


### movie

 > Encode video of model running from current timestep to specified timestep  
 > (Requires libavcodec)  
 >   
 > **Usage:** movie [endstep]  
 > **Usage:** movie [startstep] [endstep]  
 > **Usage:** movie [startstep] [endstep] [fps]  
 >   
 > startstep (integer) : first frame timestep (defaults to first)  
 > endstep (integer) : last frame timestep (defaults to final)  
 > fps (integer) : fps, default=30  


### record

 > Encode video of all frames displayed at specified framerate  
 > (Requires libavcodec)  
 >   
 > **Usage:** record (framerate) (quality)  
 >   
 > framerate (integer): frames per second (default 30)  
 > quality (integer): 1=low/2=medium/3=high (default 1)  


### bake

 > 'Bake' model geometry, turns dynamic meshes into static by permanently replacing higher level  
 > glyph types (vector/tracer/shape) with generated low-level primitives (point/line/triangle)  
 > This removes the ability to change the glyph properties used to create primitives,  
 > but makes the meshes render much faster in animations as they don't need to be regenerated  
 >   
 > **Usage:** bake [object]  
 >   
 > object (integer/string) : the index or name of object to bake, defaults to all (see: "list objects")  
 >   
 > **Usage:** bakecolour [object]  
 >   
 > As above, but also convert colourmap + value data into calculated RGBA values  
 >   
 > **Usage:** glaze [object]  
 >   
 > As above, but also convert colourmap + value data into a texture image and associated texcoords  
 >   


### bakecolour

 > 'Bake' model geometry, turns dynamic meshes into static by permanently replacing higher level  
 > glyph types (vector/tracer/shape) with generated low-level primitives (point/line/triangle)  
 > This removes the ability to change the glyph properties used to create primitives,  
 > but makes the meshes render much faster in animations as they don't need to be regenerated  
 >   
 > **Usage:** bake [object]  
 >   
 > object (integer/string) : the index or name of object to bake, defaults to all (see: "list objects")  
 >   
 > **Usage:** bakecolour [object]  
 >   
 > As above, but also convert colourmap + value data into calculated RGBA values  
 >   
 > **Usage:** glaze [object]  
 >   
 > As above, but also convert colourmap + value data into a texture image and associated texcoords  
 >   


### glaze

 > 'Bake' model geometry, turns dynamic meshes into static by permanently replacing higher level  
 > glyph types (vector/tracer/shape) with generated low-level primitives (point/line/triangle)  
 > This removes the ability to change the glyph properties used to create primitives,  
 > but makes the meshes render much faster in animations as they don't need to be regenerated  
 >   
 > **Usage:** bake [object]  
 >   
 > object (integer/string) : the index or name of object to bake, defaults to all (see: "list objects")  
 >   
 > **Usage:** bakecolour [object]  
 >   
 > As above, but also convert colourmap + value data into calculated RGBA values  
 >   
 > **Usage:** glaze [object]  
 >   
 > As above, but also convert colourmap + value data into a texture image and associated texcoords  
 >   


### export

 > Export model database  
 >   
 > **Usage:** export [filename] [objects] [uncompressed]  
 >   
 > filename (string) : the name of the file to export to, extension must be .gldb  
 > objects (integer/string) : the indices or names of the objects to export (see: "list objects")  
 > uncompressed : add this to skip the zlib compression step in the export process  
 > If object ommitted all will be exported  


### csv

 > Export CSV data  
 >   
 > **Usage:** csv [objects]  
 >   
 > objects (integer/string) : the indices or names of the objects to export (see: "list objects")  
 > If object ommitted all will be exported  


### json

 > Export json data  
 >   
 > **Usage:** json [object]  
 >   
 > objects (integer/string) : the indices or names of the objects to export (see: "list objects")  
 > If object ommitted all will be exported  


### save

 > Export all settings as json state file that can be reloaded later  
 >   
 > **Usage:** save ["filename"]  
 >   
 > file (string) : name of file to import  
 > If filename omitted and database loaded, will save the state  
 > to the active figure in the database instead  


### tiles

 > Save a tiled image of selected volume dataset  
 > **Usage:** tiles (object)  


---
## View commands:


### rotate

 > Rotate model  
 >   
 > **Usage:** rotate axis degrees  
 >   
 > axis (x/y/z) : axis of rotation  
 > degrees (number) : degrees of rotation  
 >   
 > **Usage:** rotate x y z  
 >   
 > x (number) : x axis degrees of rotation  
 > y (number) : y axis degrees of rotation  
 > z (number) : z axis degrees of rotation  


### rotatex

 > Rotate model about x-axis  
 >   
 > **Usage:** rotatex degrees  
 >   
 > degrees (number) : degrees of rotation  


### rotatey

 > Rotate model about y-axis  
 >   
 > **Usage:** rotatey degrees  
 >   
 > degrees (number) : degrees of rotation  


### rotatez

 > Rotate model about z-axis  
 >   
 > **Usage:** rotatez degrees  
 >   
 > degrees (number) : degrees of rotation  


### rotation

 > Set model rotation (replaces any previous rotation)  
 >   
 > **Usage:** rotation x y z [w]  
 >   
 > x (number) : rotation x component  
 > y (number) : rotation y component  
 > z (number) : rotation z component  
 > w (number) : rotation w component  
 > If only x,y,z are provided they determine the rotation  
 > about these axis (roll/pitch/yaw)  
 > If x,y,z,w are provided, the rotation is loaded  
 > as a quaternion with these components  
 >   


### zoom

 > Translate model in Z direction (zoom)  
 >   
 > **Usage:** zoom units  
 >   
 > units (number) : distance to zoom, units are based on model size  
 >                  1 unit is approx the model bounding box size  
 >                  negative to zoom out, positive in  


### translate

 > Translate model in 3 dimensions  
 >   
 > **Usage:** translate dir shift  
 >   
 > dir (x/y/z) : direction to translate  
 > shift (number) : amount of translation  
 >   
 > **Usage:** translation x y z  
 >   
 > x (number) : x axis shift  
 > y (number) : y axis shift  
 > z (number) : z axis shift  


### translatex

 > Translate model in X direction  
 >   
 > **Usage:** translationx shift  
 >   
 > shift (number) : x axis shift  


### translatey

 > Translate model in Y direction  
 >   
 > **Usage:** translationy shift  
 >   
 > shift (number) : y axis shift  


### translatez

 > Translate model in Z direction  
 >   
 > **Usage:** translationz shift  
 >   
 > shift (number) : z axis shift  


### translation

 > Set model translation in 3d coords (replaces any previous translation)  
 >   
 > **Usage:** translation x y z  
 >   
 > x (number) : x axis shift  
 > y (number) : y axis shift  
 > z (number) : z axis shift  


### autorotate

 > Auto-Rotate model to face camera if flat in X or Y dimensions  
 >   


### spin

 > Auto-spin model, animates  
 >   
 > **Usage:** spin axis degrees  
 >   
 > axis (x/y/z) : axis of rotation  
 > degrees (number) : degrees of rotation per frame  
 >   


### focus

 > Set model focal point in 3d coords  
 >   
 > **Usage:** focus x y z  
 >   
 > x (number) : x coord  
 > y (number) : y coord  
 > z (number) : z coord  


### fov

 > Set camera fov (field-of-view)  
 >   
 > **Usage:** fov degrees  
 >   
 > degrees (number) : degrees of viewing range  


### focallength

 > Set camera focal length (for stereo viewing)  
 >   
 > **Usage:** focallength len  
 >   
 > len (number) : distance from camera to focal point  


### eyeseparation

 > Set camera stereo eye separation  
 >   
 > **Usage:** eyeseparation len  
 >   
 > len (number) : distance between left and right eye camera viewpoints  


### nearclip

 > Set near clip plane, before which geometry is not displayed  
 >   
 > **Usage:** nearclip dist  
 >   
 > dist (number) : distance from camera to near clip plane  


### farclip

 > Set far clip plane, beyond which geometry is not displayed  
 >   
 > **Usage:** farrclip dist  
 >   
 > dist (number) : distance from camera to far clip plane  


### zoomclip

 > Adjust near clip plane relative to current value  
 >   
 > **Usage:** zoomclip multiplier  
 >   
 > multiplier (number) : multiply current near clip plane by this value  


### zerocam

 > Set the camera postiion to the origin (for scripting, not generally advisable)  


### reset

 > Reset the camera to the default model view  


### bounds

 > Recalculate the model bounding box from geometry  


### fixbb

 > Fix the model bounding box to it's current size, will no longer be elastic  


### camera

 > Output camera state for use in model scripts  


### resize

 > Resize view window  
 >   
 > **Usage:** resize width height  
 >   
 > width (integer) : width in pixels  
 > height (integer) : height in pixels  


### fullscreen

 > Switch viewer to full-screen mode and back to windowed mode  


### fit

 > Adjust camera view to fit the model in window  


### autozoom

 > Automatically adjust camera view to always fit the model in window (enable/disable)  


### stereo

 > Enable/disable stereo projection  
 > If no stereo hardware found will use side-by-side mode  


### coordsystem

 > Switch between Right-handed and Left-handed coordinate system  


### sort

 > Sort geometry on demand (with idle timer)  
 >   
 > **Usage:** sort on/off  
 >   
 > on : (default) sort after model rotation  
 > off : no sorting  
 > If no options passed, does immediate re-sort  


---
## Object commands:


### hide

 > hide objects/geometry types  
 >   
 > **Usage:** hide object  
 >   
 > object (integer/string) : the index or name of the object to hide (see: "list objects")  
 >   
 > **Usage:** hide geometry_type id  
 >   
 > geometry_type : points/labels/vectors/tracers/triangles/quads/shapes/lines/volumes  
 > id (integer, optional) : id of geometry to hide  
 > eg: 'hide points' will hide all point data  
 > note: 'hide all' will hide all objects  


### show

 > show objects/geometry types  
 >   
 > **Usage:** show object  
 >   
 > object (integer/string) : the index or name of the object to show (see: "list objects")  
 >   
 > **Usage:** show geometry_type id  
 >   
 > geometry_type : points/labels/vectors/tracers/triangles/quads/shapes/lines/volumes  
 > id (integer, optional) : id of geometry to show  
 > eg: 'hide points' will show all point data  
 > note: 'hide all' will show all objects  


### delete

 > Delete objects  
 > **Usage:** delete object  
 >   
 > object (integer/string) : the index or name of the object to delete (see: "list objects")  


### load

 > Load objects from database  
 > Used when running with deferred loading (-N command line switch)  
 >   
 > **Usage:** load object  
 >   
 > object (integer/string) : the index or name of the object to load (see: "list objects")  


### select

 > Select object as the active object  
 > Used for setting properties of objects  
 > following commands that take an object id will no longer require one)  
 >   
 > **Usage:** select object  
 >   
 > object (integer/string) : the index or name of the object to select (see: "list objects")  
 > Leave object parameter empty to clear selection.  


### add

 > Add a new object and select as the active object  
 >   
 > **Usage:** add [object_name] [data_type]  
 >   
 > object_name (string) : optional name of the object to add  
 > data_type (string) : optional name of the default data type  
 > (points/labels/vectors/tracers/triangles/quads/shapes/lines)  


### append

 > Append a new geometry container to an object  
 > eg: to divide line objects into segments  
 >   
 > **Usage:** append [object]  
 >   
 > object (integer/string) : the index or name of the object to select (see: "list objects")  


### name

 > Set object name  
 >   
 > **Usage:** name object newname  
 >   
 > object (integer/string) : the index or name of the object to rename (see: "list objects")  
 > newname (string) : new name to apply  


### isosurface

 > Generate an isosurface from a volume object  
 >   
 > **Usage:** isosurface [isovalue]  
 >   
 > isovalue (number) : isovalue to draw the surface at,  
 >                     if not specified, will use 'isovalues' property  


### texture

 > Set or clear object texture data  


---
## Display commands:


### background

 > Set window background colour  
 >   
 > **Usage:** background colour/value/white/black/grey/invert  
 >   
 > colour (string) : any valid colour string  
 > value (number [0,1]) : sets to greyscale value with luminosity in range [0,1] where 1.0 is white  
 > white/black/grey : sets background to specified value  
 > invert (or omitted value) : inverts current background colour  


### alpha

 > Set global transparency value  
 >   
 > **Usage:** opacity/alpha value  
 >   
 > 'opacity' is global default, applies to all objects without own opacity setting  
 > 'alpha' is global override, is combined with opacity setting of all objects  
 > value (integer > 1) : sets opacity as integer in range [1,255] where 255 is fully opaque  
 > value (number [0,1]) : sets opacity as real number in range [0,1] where 1.0 is fully opaque  


### axis

 > Display/hide the axis legend  
 >   
 > **Usage:** axis (on/off)  
 >   
 > on/off (optional) : show/hide the axis, if omitted switches state  


### rulers

 > Enable/disable axis rulers (unfinished)  


### antialias

 > Enable/disable multi-sample anti-aliasing (if supported by OpenGL drivers)  


### valuerange

 > Adjust colourmaps on object to fit actual value range  


### colourmap

 > Set colourmap on object or create a new colourmap  
 >   
 > **Usage:** colourmap [colourmap / data]  
 > If an object is selected, the colourmap will be set on that object, otherwise a new colourmap will be created  
 >   
 > colourmap (integer/string) : the index or name of the colourmap to apply (see: "list colourmaps")  
 > data (string) : data to load into selected object's colourmap, if no colourmap one will be added  


### colourbar

 > Add a colour bar display to selected object  
 >   
 > **Usage:** colourbar [alignment] [size]  
 >   
 > alignment (string) : viewport alignment (left/right/top/bottom)  


### pointtype

 > Set point-rendering type on object  
 >   
 > **Usage:** pointtype all/object type/up/down  
 >   
 > all : use 'all' to set the global default point type  
 > object (integer/string) : the index or name of the object to set (see: "list objects")  
 > type (integer) : Point type [0,3] to apply (gaussian/flat/sphere/highlight sphere)  
 > up/down : use 'up' or 'down' to switch to the previous/next type in list  


### pointsample

 > Set point sub-sampling value  
 >   
 > **Usage:** pointsample value/up/down  
 >   
 > value (integer) : 1=no sub-sampling=50%% sampled etc..  
 > up/down : use 'up' or 'down' to sample more (up) or less (down) points  


### border

 > Set model border frame  
 >   
 > **Usage:** border on/off/filled  
 >   
 > on : line border around model bounding-box  
 > off : no border  
 > filled : filled background bounding box  
 > (no value) : switch between possible values  


### title

 > Set title heading to following text  


### scale

 > Scale model or applicable objects  
 >   
 > **Usage:** scale axis value  
 >   
 > axis : x/y/z  
 > value (number) : scaling value applied to all geometry on specified axis (multiplies existing)  
 >   
 > **Usage:** scale geometry_type value/up/down  
 >   
 > geometry_type : points/vectors/tracers/shapes  
 > value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling  
 >   
 > **Usage:** scale object value/up/down  
 >   
 > object (integer/string) : the index or name of the object to scale (see: "list objects")  
 > value (number) or 'up/down' : scaling value or use 'up' or 'down' to reduce/increase scaling  


### modelscale

 > Set model scaling, multiples by existing values  
 >   
 > **Usage:** scale xval yval zval  
 >   
 > xval (number) : scaling value applied to x axis  
 >   
 > yval (number) : scaling value applied to y axis  
 > zval (number) : scaling value applied to z axis  


---
## Scripting commands:


### next

 > Go to next timestep in sequence  


### play

 > Display model timesteps in sequence from current timestep to specified timestep  
 >   
 > **Usage:** play endstep  
 >   
 > endstep (integer) : last frame timestep  
 > If endstep omitted, will loop continually until 'stop' command entered  


### stop

 > Stop the looping 'play' command  


### open

 > Open the viewer window if not already done, for use in scripts  


### interactive

 > Switch to interactive mode, for use in scripts  
 >   
 > **Usage:** interactive [noshow]  
 >   
 > options (string) : "noshow" default is to show visible interactive window, set this to activate event loop  
 >                    without showing window, can use browser interactive mode  
 >                    "noloop" : set this option to show the interactive window only,  
 >                    and handle events outside on returning  


---
## Miscellanious commands:


### shaders

 > Reload shader program files from disk  


### blend

 > Select blending type  
 >   
 > **Usage:** blend mode  
 >   
 > mode (string) : normal: multiplicative, add: additive, png: premultiplied for png output  


### props

 > Print properties on active object or global and view if none selected  


### defaults

 > Print all available properties and their default values  


### test

 > Create and display some test data, points, lines, quads  


### voltest

 > Create and display a test volume dataset  


### newstep

 > Add a new timestep after the current one  


### filter

 > Set a data filter on selected object  
 >   
 > **Usage:** filter index min max [map]  
 >   
 > index (integer) : the index of the data set to filter on (see: "list data")  
 > min (number) : the minimum value of the range to filter in or out  
 > max (number) : the maximum value of the range to filter in or out  
 > map (literal) : add map keyword for min/max [0,1] on available data range  
 >                   eg: 0.1-0.9 will filter the lowest and highest 10% of values  


### filterout

 > Set a data filter on selected object  
 >   
 > **Usage:** filter index min max [map]  
 >   
 > index (integer) : the index of the data set to filter on (see: "list data")  
 > min (number) : the minimum value of the range to filter in or out  
 > max (number) : the maximum value of the range to filter in or out  
 > map (literal) : add map keyword for min/max [0,1] on available data range  
 >                   eg: 0.1-0.9 will filter the lowest and highest 10% of values  


### filtermin

 > Modify a data filter on selected object  
 >   
 > **Usage:** filtermin index value  
 >   
 > index (integer) : the index filter to set [0 - N-1] where N = # of filters added  
 > value (number) : the minimum value of the range to filter in or out  


### filtermax

 > Modify a data filter on selected object  
 >   
 > **Usage:** filtermax index value  
 >   
 > index (integer) : the index filter to set [0 - N-1] where N = # of filters added  
 > value (number) : the maximum value of the range to filter in or out  


### clearfilters

 > Clear filters on selected object  


### verbose

 > Switch verbose output mode on/off  
 >   
 > **Usage:** verbose [off]  


### toggle

 > Toggle a boolean property  
 >   
 > **Usage:** toogle (property-name)  
 >   
 > property-name : name of property to switch  
 > If an object is selected, will try there, then view, then global  


### createvolume

 > Create a static volume object which will receive all following volumes as timesteps  


### clearvolume

 > Clear global static volume object, any future volume loads will create a new object automatically  


### palette

 > Export colour data of selected object  
 >   
 > **Usage:** palette [type] [filename]  
 >   
 > type (string) : type of export (image/json/text) default = text  
 > filename (string) : defaults to palette.[png/json/txt]  

