//WebGL particle/surface viewer, Owen Kaluza (c) Monash University 2012-14
//TODO: 
//Replace all control creation, update and set with automated routines from list of defined property controls
// License: LGPLv3 for now
var vis = {};
var view = 0; //Active view
var viewer;
var params, properties, objectlist;
var server = false;
var types = {"triangles" : "triangle", "points" : "particle", "lines" : "line", "volume" : "volume", "border" : "line"};
var debug_on = false;
var noui = false;
var MAXIDX = 2047;

function initPage(src, fn) {
  var urlq = decodeURI(window.location.href);
  if (urlq.indexOf("?") > 0) {
    var parts = urlq.split("?"); //whole querystring before and after ?
    var query = parts[1]; 

    //Print debugging output?
    //TODO, this query parser only handles a single arg, fix 
    if (query.indexOf("debug") > 0) debug_on = true;

    if (!src && query.indexOf(".json") > 0) {
      //Passed a json(p) file on URL
      if (query.indexOf(".jsonp") > 0) {
        //Load jsonp file as a script, useful when opening page as file://
        //only way to get around security issues in chrome, 
        //script calls the loadData function providing the content
        var script = document.createElement('script');
        script.id = 'script';
        script.style.display = 'none';
        script.src = query;
        document.body.appendChild(script);
      } else {
        document.getElementById('fileupload').style.display = "none";
        progress("Downloading model data from server...");
        ajaxReadFile(query, initPage, false, updateProgress);
      }
      return;
    }
  } else if (!src && urlq.indexOf("#") > 0) {
    //IPython strips out ? args so have to check for this instead
    var parts = urlq.split("#"); //whole querystring before and after #
    noui = true;
    if (parts[1].indexOf(".json") > 0) {
      //Load filename from url
      ajaxReadFile(parts[1], initPage, false);
      return;
    }

    //Load base64 encoded data from url
    window.location.hash = ""
    src = window.atob(parts[1]);
  }

  progress();

  var canvas = document.getElementById('canvas');
  viewer =  new Viewer(canvas);
  if (canvas) {
    //this.canvas = document.createElement("canvas");
    //this.canvas.style.cssText = "width: 100%; height: 100%; z-index: 0; margin: 0px; padding: 0px; background: black; border: none; display:block;";
    //if (!parentEl) parentEl = document.body;
    //parentEl.appendChild(this.canvas);

    //Canvas event handling
    canvas.mouse = new Mouse(canvas, new MouseEventHandler(canvasMouseClick, canvasMouseWheel, canvasMouseMove, canvasMouseDown, null, null, canvasMousePinch));
    //Following two settings should probably be defaults?
    canvas.mouse.moveUpdate = true; //Continual update of deltaX/Y
    //canvas.mouse.setDefault();

    canvas.mouse.wheelTimer = true; //Accumulate wheel scroll (prevents too many events backing up)
    defaultMouse = document.mouse = canvas.mouse;

    window.onresize = function() {viewer.drawTimed();};
  }

  if (query && query.indexOf("server") >= 0) {
    //Switch to image frame
    setAll('', 'server');
    setAll('none', 'client');

    server = true;

    //if (!viewer.gl) {
      //img = document.getElementById('frame');
      //Image canvas event handling
      //img.mouse = new Mouse(img, new MouseEventHandler(serverMouseClick, serverMouseWheel, serverMouseMove, serverMouseDown));
    //}

    //Initiate the server update
    requestData('/connect', parseRequest);
    //requestData('/objects', parseObjects);

    //Enable to forward key presses to server directly
    //document.onkeypress = keyPress;
    window.onbeforeunload = function() {if (client_id >= 0) requestData("/disconnect=" + client_id, null, true); client_id = -1;};

    //Set viewer window to match ours?
    //resizeToWindow();

  } else {
    setAll('none', 'server');
    setAll('', 'client');
    var frame = document.getElementById('frame');
    if (frame)
      frame.style.display = 'none';
  }

  if (!noui) {
    //Create tool windows
    params =     new Toolbox("params", 20, 20);
    objectlist = new Toolbox("objectlist", 370, 20);
    properties = new Toolbox("properties", 720, 20);

    params.show();
    objectlist.show();
  } else {
    params =     new Toolbox("params", -1, -1);
    objectlist = new Toolbox("objectlist", -1, -1);
    properties = new Toolbox("properties", -1, -1);
  }

  if (src) {
    viewer.loadFile(src);
  } else {
    var source = getSourceFromElement('source');
    if (source) {
      //Preloaded data
      document.getElementById('fileupload').style.display = "none";
      viewer.loadFile(source);
    } else {
      //Demo objects
      //demoData();
      viewer.draw();
    }
  }
}

function loadData(data) {
  initPage(data);
}

function progress(text) {
  var el = document.getElementById('progress');
  if (el.style.display == 'block' || text == undefined)
    //el.style.display = 'none';
    setTimeout("document.getElementById('progress').style.display = 'none';", 150);
  else {
    document.getElementById('progressmessage').innerHTML = text;
    document.getElementById('progressstatus').innerHTML = "";
    document.getElementById('progressbar').style.width = 0;
    el.style.display = 'block';
  }
}

function canvasMouseClick(event, mouse) {
  if (server) {
    if (viewer.rotating)
      sendCommand('' + viewer.getRotationString());
    else
      sendCommand('' + viewer.getTranslationString());
  }

  //if (server) serverMouseClick(event, mouse); //Pass to server handler
  if (viewer.rotating) {
    viewer.rotating = false;
    //viewer.reload = true;
    sortTimer();
  }
  if (document.getElementById("immsort").checked == true) {
    //No timers
    viewer.draw();
    viewer.rotated = true; 
  }

  viewer.draw();
  return false;
}

function sortTimer(ifexists) {
  if (document.getElementById("immsort").checked == true) {
    //No timers
    return;
  }
  //Set a timer to apply the sort function in 2 seconds
  if (viewer.timer) {
    clearTimeout(viewer.timer);
  } else if (ifexists) {
    //No existing timer? Don't start a new one
    return;
  }
  var element = document.getElementById("sort");
  element.style.display = 'block';
  viewer.timer = setTimeout(function() {viewer.rotated = true; element.onclick.apply(element);}, 2000);
}

function canvasMouseDown(event, mouse) {
  //if (server) serverMouseDown(event, mouse); //Pass to server handler
  return false;
}

function canvasMouseMove(event, mouse) {
  //if (server) serverMouseMove(event, mouse); //Pass to server handler
  if (!mouse.isdown || !viewer) return true;
  viewer.rotating = false;

  //Switch buttons for translate/rotate
  var button = mouse.button;
  if (document.getElementById('tmode').checked)
    button = Math.abs(button-2);

  //console.log(mouse.deltaX + "," + mouse.deltaY);
  switch (button)
  {
    case 0:
      viewer.rotateY(mouse.deltaX/5);
      viewer.rotateX(mouse.deltaY/5);
      viewer.rotating = true;
      sortTimer(true);  //Delay sort if queued
      break;
    case 1:
      viewer.rotateZ(Math.sqrt(mouse.deltaX*mouse.deltaX + mouse.deltaY*mouse.deltaY)/5);
      viewer.rotating = true;
      sortTimer(true);  //Delay sort if queued
      break;
    case 2:
      var adjust = viewer.modelsize / 1000;   //1/1000th of size
      viewer.translate[0] += mouse.deltaX * adjust;
      viewer.translate[1] -= mouse.deltaY * adjust;
      break;
  }

  //Always draw border while interacting in server mode?
  if (server)
    viewer.draw(true);
  //Draw border while interacting (automatically on for models > 500K vertices)
  //Hold shift to switch from default behaviour
  else if (document.getElementById("interactive").checked == true)
    viewer.draw();
  else
    viewer.draw(!event.shiftKey);
  return false;
}

var zoomTimer;
var zoomClipTimer;
var zoomSpin = 0;

function canvasMouseWheel(event, mouse) {
  if (event.shiftKey) {
    var factor = event.spin * 0.01;
    if (window.navigator.platform.indexOf("Mac") >= 0)
      factor *= 0.1;
    if (zoomClipTimer) clearTimeout(zoomClipTimer);
    zoomClipTimer = setTimeout(function () {viewer.zoomClip(factor);}, 100 );
  } else {
    if (zoomTimer) 
      clearTimeout(zoomTimer);
    zoomSpin += event.spin;
    zoomTimer = setTimeout(function () {viewer.zoom(zoomSpin); zoomSpin = 0;}, 100 );
    //zoomTimer = setTimeout(function () {viewer.zoom(zoomSpin*0.01); zoomSpin = 0;}, 100 );
  }
  return false; //Prevent default
}

function canvasMousePinch(event, mouse) {
  if (event.distance != 0) {
    var factor = event.distance * 0.0001;
    viewer.zoom(factor);
  }
  return false; //Prevent default
}

//File upload handling
var saved_files;
function fileSelected(files, callback) {
  saved_files = files;

  // Check for the various File API support.
  if (window.File && window.FileReader) { // && window.FileList) {
    //All required File APIs are supported.
    for (var i = 0; i < files.length; i++) {
      var file = files[i];
      //User html5 fileReader api (works offline)
      var reader = new FileReader();

      // Closure to capture the file information.
      reader.onload = (function(file) {
        return function(e) {
          //alert(e.target.result);
          viewer.loadFile(e.target.result);
        };
      })(file);

      // Read in the file (AsText/AsDataURL/AsArrayBuffer/AsBinaryString)
      reader.readAsText(file);
    }
  } else {
    alert('The File APIs are not fully supported in this browser.');
  }
}

function getImageDataURL(img) {
  var canvas = document.createElement("canvas");
  canvas.width = img.width;
  canvas.height = img.height;
  var ctx = canvas.getContext("2d");
  ctx.drawImage(img, 0, 0);
  var dataURL = canvas.toDataURL("image/png");
  return dataURL;
}

function defaultColourMaps() {
  //Add some default colourmaps

  if (!vis.colourmaps) vis.colourmaps = [];
  
  //A bit of a hack, but avoids adding defaults if already present,
  //if there are 5 other maps defined there is probably no need for defaults anyway
  if (vis.colourmaps.length >= 5) return;

  vis.colourmaps.push({
    "name": "Grayscale",
    "logscale": 0,
    "colours": [{"colour": "rgba(0,0,0,255)"},{"colour": "rgba(255,255,255,1)"}]
  });

  vis.colourmaps.push({
    "name": "Topology",
    "logscale": 0,
    "colours": [{"colour": "#66bb33"},
                {"colour": "#00ff00"},
                {"colour": "#3333ff"},
                {"colour": "#00ffff"},
                {"colour": "#ffff77"},
                {"colour": "#ff8800"},
                {"colour": "#ff0000"},
                {"colour": "#000000"}]
  });

  vis.colourmaps.push({
    "name": "Rainbow",
    "logscale": 0,
    "colours": [{"colour": "#a020f0"},
                {"colour": "#0000ff"},
                {"colour": "#00ff00"},
                {"colour": "#ffff00"},
                {"colour": "#ffa500"},
                {"colour": "#ff0000"},
                {"colour": "#000000"}]
  });

  vis.colourmaps.push({
    "name": "Heat",
    "logscale": 0,
    "colours": [{"colour": "#000000"},
                {"colour": "#ff0000"},
                {"colour": "#ffff00"},
                {"colour": "#ffffff"}]
  });

  vis.colourmaps.push({
    "name": "Bluered",
    "logscale": 0,
    "colours": [{"colour": "#0000ff"},
                {"colour": "#1e90ff"},
                {"colour": "#00ced1"},
                {"colour": "#ffe4c4"},
                {"colour": "#ffa500"},
                {"colour": "#b22222"}]
  });
}

function loadColourMaps() {
  //Load colourmaps
  if (!vis.colourmaps) return;
  var list = document.getElementById('colourmap-presets');
  var sel = list.value;
  var canvas = document.getElementById('palette');
  list.options.length = 1; //Remove all except "None"
  for (var i=0; i<vis.colourmaps.length; i++) {
    var palette = new Palette(vis.colourmaps[i].colours);
    vis.colourmaps[i].palette = palette;
    var option = new Option(vis.colourmaps[i].name || ("ColourMap " + i), i);
    list.options[list.options.length] = option;

    paletteLoad(palette);
  }
  //Restore selection
  list.value = sel;
  if (viewer) viewer.setColourMap(sel);
}

function checkPointMinMax(coord) {
  for (var i=0; i<3; i++) {
    vis.views[view].min[i] = Math.min(coord[i], vis.views[view].min[i]);
    vis.views[view].max[i] = Math.max(coord[i], vis.views[view].max[i]);
  }
  //console.log(JSON.stringify(vis.views[view].min) + " -- " + JSON.stringify(vis.views[view].max));
}

function objVertexColour(obj, data, idx) {
  return vertexColour(obj.colour, obj.opacity, obj.colourmap >= 0 ? vis.colourmaps[obj.colourmap] : null, data, idx);
}

function safeLog10(val) {return val < Number.MIN_VALUE ? Math.log10(Number.MIN_VALUE) : Math.log10(val); }

function vertexColour(colour, opacity, colourmap, data, idx) {
  //Default to object colour property
  if (data.values) {
    var colrange = data.vertices.data.length / (3*data.values.data.length);
    idx = Math.floor(idx/colrange);
    if (colourmap) {
      var min = 0;
      var max = 1;
      if (data.values.minimum != undefined) min = data.values.minimum;
      if (data.values.maximum != undefined) max = data.values.maximum;
      //Use a colourmap
      if (colourmap.range != undefined) {
        rmin = parseFloat(colourmap.range[0]);
        rmax = parseFloat(colourmap.range[1]);
        if (rmin < rmax) {
          min = rmin;
          max = rmax;
        }
      }
      //Get nearest pixel on the canvas
      var pos = MAXIDX*0.5;  //If rubbish data, return centre
      //Allows single value for entire object
      if (idx >= data.values.data.length) idx = data.values.data.length-1;
      var val = data.values.data[idx];
      if (val < min)
        pos = 0;
      else if (val > max)
        pos = MAXIDX;
      else if (max > min) {
        var scaled;
        if (colourmap.logscale) {
          val = safeLog10(val);
          min = safeLog10(min);
          max = safeLog10(max);
        }
        //Scale to range [0,1]
        scaled = (val - min) / (max - min);
        //Get colour pos [0-2047)
        pos =  Math.round(MAXIDX * scaled);
      }
      colour = colourmap.palette.cache[pos];
      //if (idx % 100) console.log(" : " + val + " min " + min + " max " + max + " pos = " + pos + " colour: " + colour);
    } else if (data.values.type == 'integer') {
      //Integer data values, treat as colours
      colour = data.values.data[idx];
    }
  }
  //RGBA colour values
  if (data.colours) {
    var colrange = data.vertices.data.length / (3*data.colours.data.length);
    idx = Math.floor(idx/colrange);
    if (data.colours.data.length == 1) idx = 0;  //Single colour only provided
    if (idx >= data.colours.data.length) idx = data.colours.data.length-1;
    colour = data.colours.data[idx];
  }

  //Apply opacity per object setting
  if (opacity) {
    var C = new Colour(colour);
    C.alpha = C.alpha * opacity;
    colour = C.toInt();
  }

  //Return final integer value
  return colour;
}

//Get eye pos vector z by multiplying vertex by modelview matrix
function eyeDistance(M2,P) {
  return -(M2[0] * P[0] + M2[1] * P[1] + M2[2] * P[2] + M2[3]);
}

function newFilledArray(length, val) {
    var array = [];
    for (var i = 0; i < length; i++) {
        array[i] = val;
    }
    return array;
}

function radix(nbyte, source, dest, N)
//void radix(char byte, char size, long N, unsigned char *src, unsigned char *dst)
{
   // Radix counting sort of 1 byte, 8 bits = 256 bins
   var count = newFilledArray(256,0);
   var index = [];
   var i;
   //unsigned char* dst = (unsigned char*)dest;

   //Create histogram, count occurences of each possible byte value 0-255
   var mask = 0xff;// << (byte*8);
   for (i=0; i<N; i++) {
     //if (byte > 0) alert(source[i].key + " : " + (source[i].key >> (byte*8) & mask) + " " + mask); 
      count[source[i].key >> (nbyte*8) & mask]++;
   }

   //Calculate number of elements less than each value (running total through array)
   //This becomes the offset index into the sorted array
   //(eg: there are 5 smaller values so place me in 6th position = 5)
   index[0]=0;
   for (i=1; i<256; i++) index[i] = index[i-1] + count[i-1];

   //Finally, re-arrange data by index positions
   for (i=0; i<N; i++ )
   {
       var val = source[i].key >> (nbyte*8) & mask;  //Get value
       //memcpy(&dest[index[val]], &source[i], size);
       dest[index[val]] = source[i];
       //memcpy(&dst[index[val]*size], &src[i*size], size);
       index[val]++; //Increment index to push next element with same value forward one
   }
}


function radix_sort(source, swap, bytes)
{
   //assert(bytes % 2 == 0);
   //OK.debug("Radix X sort: %d items %d bytes. Byte: ", N, size);
   // Sort bytes from least to most significant 
   var N = source.length;
   for (var x = 0; x < bytes; x += 2) 
   {
      radix(x, source, swap, N);
      radix(x+1, swap, source, N);
   }
}


//http://stackoverflow.com/questions/7936923/assist-with-implementing-radix-sort-in-javascript
//arr: array to be sorted
//begin: 0
//end: length of array
//bit: maximum number of bits required to represent numbers in arr
function msb_radix_sort(arr, begin, end, bit) {
  var i, j, mask, tmp;
  i = begin;
  j = end;
  mask = 1 << bit;
  while(i < j) {
    while(i < j && !(arr[i].key & mask)) ++i;
    while(i < j && (arr[j - 1].key & mask)) --j;
    if(i < j) {
      j--;
      tmp = arr[i]; //Swap
      arr[i] = arr[j];
      arr[j] = tmp;
      i++;
    }
  }
  if(bit && i > begin)
    msb_radix_sort(arr, begin, i, bit - 1);
  if(bit && i < end)
    msb_radix_sort(arr, i, end, bit - 1);
}

/**
 * @constructor
 */
function Toolbox(id, x, y) {
  //Mouse processing:
  this.el = document.getElementById(id);
  this.mouse = new Mouse(this.el, this);
  this.mouse.moveUpdate = true;
  this.el.mouse = this.mouse;
  this.style = this.el.style;
  this.drag = false;
  if (x && y) {
    this.style.left = x + 'px';
    this.style.top = y + 'px';

    if (x < 0 && y < 0) {
      this.style.width = this.style.height = 0;
      this.style.overflow = "hidden";
      this.style.display = "none";
    }
  } else {
    this.style.left = ((window.innerWidth - this.el.offsetWidth) * 0.5) + 'px';
    this.style.top = ((window.innerHeight - this.el.offsetHeight) * 0.5) + 'px';
  }
}

Toolbox.prototype.toggle = function() {
  if (this.style.visibility == 'visible')
    this.hide();
  else
    this.show();
}

Toolbox.prototype.show = function() {
  this.style.visibility = 'visible';
      this.style.overflow = 'visible';
}

Toolbox.prototype.hide = function() {
  this.style.visibility = 'hidden';
      this.style.overflow = 'hidden';
}

//Mouse event handling
Toolbox.prototype.click = function(e, mouse) {
  this.drag = false;
  return true;
}

Toolbox.prototype.down = function(e, mouse) {
  //Process left drag only
  this.drag = false;
  if (mouse.button == 0 && e.target.className.indexOf('scroll') < 0 && ['INPUT', 'SELECT', 'OPTION', 'RADIO'].indexOf(e.target.tagName) < 0)
    this.drag = true;
  return true;
}

Toolbox.prototype.move = function(e, mouse) {
  if (!mouse.isdown) return true;
  if (!this.drag) return true;

  //Drag position
  this.el.style.left = parseInt(this.el.style.left) + mouse.deltaX + 'px';
  this.el.style.top = parseInt(this.el.style.top) + mouse.deltaY + 'px';
  return false;
}

Toolbox.prototype.wheel = function(e, mouse) {
}

//This object encapsulates a vertex buffer and shader set
function Renderer(gl, type, colour, border) {
  this.border = border;
  this.gl = gl;
  this.type = type;
  if (colour) this.colour = new Colour(colour);

  //Only two options for now, points and triangles
  if (type == "particle") {
    //Particle renderer
    this.attributes = ["aVertexPosition", "aVertexColour", "aVertexSize", "aPointType"];
    this.uniforms = ["uPointType", "uPointScale", "uOpacity", "uColour"];
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT,
                        Int32Array.BYTES_PER_ELEMENT,
                        Float32Array.BYTES_PER_ELEMENT,
                        Float32Array.BYTES_PER_ELEMENT];
  } else if (type == "triangle") {
    //Triangle renderer
    this.attributes = ["aVertexPosition", "aVertexNormal", "aVertexColour", "aVertexTexCoord", "aVertexObjectID"];
    this.uniforms = ["uColour", "uCullFace", "uOpacity", "uLighting", "uTextured", "uTexture", "uCalcNormal", "uClipMin", "uClipMax", "uBrightness", "uContrast", "uSaturation", "uAmbient", "uDiffuse", "uSpecular"];
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT,
                        3 * Float32Array.BYTES_PER_ELEMENT,
                        Int32Array.BYTES_PER_ELEMENT,
                        2 * Float32Array.BYTES_PER_ELEMENT,
                        4 * Uint8Array.BYTES_PER_ELEMENT];
  } else if (type == "line") {
    //Line renderer
    this.attributes = ["aVertexPosition", "aVertexColour"],
    this.uniforms = ["uColour", "uOpacity"]
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT,
                        Int32Array.BYTES_PER_ELEMENT];
  } else if (type == "volume") {
    //Volume renderer
    this.attributes = ["aVertexPosition"];
    this.uniforms = ["uVolume", "uTransferFunction", "uEnableColour", "uFilter",
                     "uDensityFactor", "uPower", "uSaturation", "uBrightness", "uContrast", "uSamples",
                     "uViewport", "uBBMin", "uBBMax", "uResolution", "uRange", "uDenMinMax",
                     "uIsoValue", "uIsoColour", "uIsoSmooth", "uIsoWalls", "uInvPMatrix"];
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT];
  }

  this.elementSize = 0;
  for (var i=0; i<this.attribSizes.length; i++)
    this.elementSize += this.attribSizes[i];
}

Renderer.prototype.init = function() {
  if (this.type == "triangle" && !viewer.hasTriangles) return false;
  if (this.type == "particle" && !viewer.hasPoints) return false;
  var fs = this.type + '-fs';
  var vs = this.type + '-vs';

  //User defined shaders if provided...
  if (vis.shaders) {
    if (vis.shaders[this.type]) {
      fs = vis.shaders[this.type].fragment || fs;
      vs = vis.shaders[this.type].vertex || vs;
    }
  }

  if (this.type == "volume" && this.id && this.image) {
    //Setup two-triangle rendering
    viewer.webgl.init2dBuffers(this.gl.TEXTURE1); //Use 2nd texture unit

    //Override texture params set in previous call
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);

    //Load the volume texture image
    viewer.webgl.loadTexture(this.image, this.gl.LINEAR); //this.gl.LUMINANCE, true

    //Calculated scaling
    this.properties = vis.objects[this.id];
    this.res = vis.objects[this.id].volume["res"];
    this.dims = vis.objects[this.id].volume["scale"];
    this.scaling = this.dims;
    //Auto compensate for differences in resolution..
    if (vis.objects[this.id].volume.autoscale) {
      //Divide all by the highest res
      var maxn = Math.max.apply(null, this.res);
      this.scaling = [this.res[0] / maxn * this.dims[0], 
                      this.res[1] / maxn * this.dims[1],
                      this.res[2] / maxn * this.dims[2]];
    }
    this.tiles = [this.image.width / this.res[0],
                  this.image.height / this.res[1]];
    this.iscale = [1.0 / this.scaling[0], 1.0 / this.scaling[1], 1.0 / this.scaling[2]]
      
    var defines = "precision highp float;\nconst highp vec2 slices = vec2(" + this.tiles[0] + "," + this.tiles[1] + ");\n";
    defines += (!!window.MSInputMethodContext ? "#define IE11\n" : "#define NOT_IE11\n");
    var maxSamples = 1024; //interactive ? 1024 : 256;
    defines += "const int maxSamples = " + maxSamples + ";\n\n\n\n\n"; //Extra newlines so errors in main shader have correct line #
    fs = defines + getSourceFromElement('volume-fs');
  }

  //Compile the shaders
  this.program = new WebGLProgram(this.gl, vs, fs);
  if (this.program.errors) OK.debug(this.program.errors);
  //Setup attribs/uniforms (flag set to skip enabling attribs)
  this.program.setup(this.attributes, this.uniforms, true);

  return true;
}

function SortIdx(idx, key) {
  this.idx = idx;
  this.key = key;
}

Renderer.prototype.loadElements = function() {
  if (this.border) return;
  OK.debug("Loading " + this.type + " elements...");
  var start = new Date();
  var distances = [];
  var indices = [];
  //Only update the positions array when sorting due to update
  if (!this.positions || !viewer.rotated || this.type == 'line') {
    this.positions = [];
    //Add visible element positions
    for (var id in vis.objects) {
      var name = vis.objects[id].name;
      var skip = !document.getElementById('object_' + name).checked;
      if (this.type == "particle") {
        if (vis.objects[id].points) {
          for (var e in vis.objects[id].points) {
            var dat = vis.objects[id].points[e];
            var count = dat.vertices.data.length;
            //OK.debug(name + " " + skip + " : " + count);
            for (var i=0; i<count; i += 3)
              this.positions.push(skip ? null : [dat.vertices.data[i], dat.vertices.data[i+1], dat.vertices.data[i+2]]);
          }
        }
      } else if (this.type == "triangle") {
        if (vis.objects[id].triangles) {
          for (var e in vis.objects[id].triangles) {
            var dat =  vis.objects[id].triangles[e];
            var count = dat.indices.data.length/3;

            //console.log(name + " " + skip + " : " + count + " - " + dat.centroids.length);
            for (var i=0; i<count; i++) {
              //this.positions.push(skip ? null : dat.centroids[i]);
              if (skip || !dat.centroids)
                this.positions.push(null);
              else if (dat.centroids.length == 1)
                this.positions.push(dat.centroids[0]);
              else
                this.positions.push(dat.centroids[i]);
            }
          }
        }
      } else if (this.type == "line") {
        //Write lines directly to indices buffer, no depth sort necessary
        if (skip) continue;
        if (vis.objects[id].lines) {
          for (var e in vis.objects[id].lines) {
            var dat =  vis.objects[id].lines[e];
            var count = dat.indices.data.length;
            //OK.debug(name + " " + skip + " : " + count);
            for (var i=0; i<count; i++)
              indices.push(dat.indices.data[i]);
          }
        }
      }
    }

    var time = (new Date() - start) / 1000.0;
    OK.debug(time + " seconds to update positions ");
    start = new Date();
  }

  //Depth sorting and create index buffer for objects that require it...
  if (indices.length == 0) {
    var distance;
    //Calculate min/max distances from view plane
    var minmax = minMaxDist();
    var mindist = minmax[0];
    var maxdist = minmax[1];

    //Update eye distances, clamping int distance to integer between 0 and 65535
    var multiplier = 65534.0 / (maxdist - mindist);
    var M2 = [viewer.webgl.modelView.matrix[2],
              viewer.webgl.modelView.matrix[6],
              viewer.webgl.modelView.matrix[10],
              viewer.webgl.modelView.matrix[14]];

    //Add visible element distances to sorting array
    for (var i=0; i<this.positions.length; i++) {
      if (this.positions[i]) {
        if (this.positions[i].length == 0) {
          //No position data, draw last
          distances.push(new SortIdx(i, 65535));
        } else {
          //Distance from viewing plane is -eyeZ
          distance = multiplier * (-(M2[0] * this.positions[i][0] + M2[1] * this.positions[i][1] + M2[2] * this.positions[i][2] + M2[3]) - mindist);
          //distance = (-(M2[0] * this.positions[i][0] + M2[1] * this.positions[i][1] + M2[2] * this.positions[i][2] + M2[3]) - mindist);
          //      if (i%100==0 && this.positions[i].length == 4) console.log(distance + " - " + this.positions[i][3]);
          //if (this.positions[i].length == 4) distance -= this.positions[i][3];
          //distance *= multiplier;
          //if (distance < 0) distance = 0;
          if (distance > 65535) distance = 65535;
          distances.push(new SortIdx(i, 65535 - Math.round(distance)));
        }
      }
    }

    var time = (new Date() - start) / 1000.0;
    OK.debug(time + " seconds to update distances ");

    if (distances.length > 0) {
      //Sort
      start = new Date();
      //distances.sort(function(a,b){return a.key - b.key});
      //This is about 10 times faster than above:
      msb_radix_sort(distances, 0, distances.length, 16);
      //Pretty sure msb is still fastest...
      //if (!this.swap) this.swap = [];
      //radix_sort(distances, this.swap, 2);
      time = (new Date() - start) / 1000.0;
      OK.debug(time + " seconds to sort");

      start = new Date();
      //Reload index buffer
      if (this.type == "particle") {
        //Process points
        for (var i = 0; i < distances.length; ++i)
          indices.push(distances[i].idx);
          //if (distances[i].idx > this.elements) alert("ERROR: " + i + " - " + distances[i].idx + " > " + this.elements);
      } else if (this.type == "triangle") {
        //Process triangles
        for (var i = 0; i < distances.length; ++i) {
          var i3 = distances[i].idx*3;
          indices.push(i3);
          indices.push(i3+1);
          indices.push(i3+2);
          //if (i3+2 > this.elements) alert("ERROR: " + i + " - " + (i3+2) + " > " + this.elements);
        }
      }
      time = (new Date() - start) / 1000.0;
      OK.debug(time + " seconds to load index buffers");
    }
  }

  start = new Date();
  if (indices.length > 0) {
    this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(indices), this.gl.STATIC_DRAW);
    //this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(indices), this.gl.DYNAMIC_DRAW);
    //this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), this.gl.DYNAMIC_DRAW);

    time = (new Date() - start) / 1000.0;
    OK.debug(time + " seconds to update index buffer object");
  }
  //Update count to visible elements...
  this.elements = indices.length;
}

var texcoords = [[[0,0], [0,0], [0,0]], [[0,0], [0,255], [255,0]], [[255,255], [0,0], [0,255]]];

function VertexBuffer(elements, size) {
  this.size = size;
  this.vertexSizeInFloats = size / Float32Array.BYTES_PER_ELEMENT;
  this.array = new ArrayBuffer(elements * size);
  // Map this buffer to a Float32Array to access the positions/normals/sizes
  this.floats = new Float32Array(this.array);
  // Map the same buffer to an Int32Array to access the color
  this.ints = new Int32Array(this.array);
  this.bytes = new Uint8Array(this.array);
  this.offset = 0;
  OK.debug(elements + " - " + size);
  OK.debug("Created vertex buffer");
}

VertexBuffer.prototype.loadParticles = function(object) {
  for (var p in object.points) {
    var dat =  object.points[p];
    for (var i=0; i<dat.vertices.data.length/3; i++) {
      var i3 = i*3;
      var vert = [dat.vertices.data[i3], dat.vertices.data[i3+1], dat.vertices.data[i3+2]];
      this.floats[this.offset] = vert[0];
      this.floats[this.offset+1] = vert[1];
      this.floats[this.offset+2] = vert[2];
      this.ints[this.offset+3] = objVertexColour(object, dat, i);
      this.floats[this.offset+4] = dat.sizes ? dat.sizes.data[i] * object.pointsize : object.pointsize;
      this.floats[this.offset+5] = object.pointtype > 0 ? object.pointtype-1 : -1;
      this.offset += this.vertexSizeInFloats;
    }
  }
}

VertexBuffer.prototype.loadTriangles = function(object, id) {
  //Process triangles
  if (!this.byteOffset) this.byteOffset = 9 * Float32Array.BYTES_PER_ELEMENT;
  var T = 0;
  if (object.wireframe) T = 1;
  for (var t in object.triangles) {
    var dat =  object.triangles[t];
    var calcCentroids = false;
    if (!dat.centroids) {
      calcCentroids = true;
      dat.centroids = [];
    }
    //if (dat.values)
    //  console.log(object.name + " : " + dat.values.minimum + " -> " + dat.values.maximum);
    //if (object.colourmap >= 0)
    //  console.log(object.name + " :: " + vis.colourmaps[object.colourmap].range[0] + " -> " + vis.colourmaps[object.colourmap].range[1]);

    for (var i=0; i<dat.indices.data.length/3; i++) {
      //Generate tex-coords
      var texc = texcoords[(i%2+1)*T];

      //Indices holds references to vertices and other data
      var i3 = i * 3;
      var ids = [dat.indices.data[i3], dat.indices.data[i3+1], dat.indices.data[i3+2]];
      var ids3 = [ids[0]*3, ids[1]*3, ids[2]*3];

      for (var j=0; j<3; j++) {
        this.floats[this.offset] = dat.vertices.data[ids3[j]];
        this.floats[this.offset+1] = dat.vertices.data[ids3[j]+1];
        this.floats[this.offset+2] = dat.vertices.data[ids3[j]+2];
        if (dat.normals && dat.normals.data.length == 3) {
          //Single surface normal
          this.floats[this.offset+3] = dat.normals.data[0];
          this.floats[this.offset+4] = dat.normals.data[1];
          this.floats[this.offset+5] = dat.normals.data[2];
        } else if (dat.normals) {
          this.floats[this.offset+3] = dat.normals.data[ids3[j]];
          this.floats[this.offset+4] = dat.normals.data[ids3[j]+1];
          this.floats[this.offset+5] = dat.normals.data[ids3[j]+2];
        } else {
          this.floats[this.offset+3] = 0.0;
          this.floats[this.offset+4] = 0.0;
          this.floats[this.offset+5] = 0.0;
        }
        this.ints[this.offset+6] = objVertexColour(object, dat, ids[j]);
        if (dat.texcoords) {
          this.floats[this.offset+7] = dat.texcoords.data[ids[j]*2];
          this.floats[this.offset+8] = dat.texcoords.data[ids[j]*2+1];
        } else {
          this.floats[this.offset+7] = texc[j][0];
          this.floats[this.offset+8] = texc[j][1];
        }
        this.bytes[this.byteOffset] = id;
        this.offset += this.vertexSizeInFloats;
        this.byteOffset += this.size;
      }

      //Calc centroids (only required if vertices changed)
      if (calcCentroids) {
        if (dat.width) //indices.data.length == 6)
          //Cross-sections, null centroid - always drawn last
          dat.centroids.push([]);
        else {
          //(x1+x2+x3, y1+y2+y3, z1+z2+z3)
          var verts = dat.vertices.data;
          /*/Side lengths: A-B, A-C, B-C
          var AB = vec3.createFrom(verts[ids3[0]] - verts[ids3[1]], verts[ids3[0] + 1] - verts[ids3[1] + 1], verts[ids3[0] + 2] - verts[ids3[1] + 2]);
          var AC = vec3.createFrom(verts[ids3[0]] - verts[ids3[2]], verts[ids3[0] + 1] - verts[ids3[2] + 1], verts[ids3[0] + 2] - verts[ids3[2] + 2]);
          var BC = vec3.createFrom(verts[ids3[1]] - verts[ids3[2]], verts[ids3[1] + 1] - verts[ids3[2] + 1], verts[ids3[1] + 2] - verts[ids3[2] + 2]);
          var lengths = [vec3.length(AB), vec3.length(AC), vec3.length(BC)];
                //Size weighting shift
                var adj = (lengths[0] + lengths[1] + lengths[2]) / 9.0;
                //if (i%100==0) console.log(verts[ids3[0]] + "," + verts[ids3[0] + 1] + "," + verts[ids3[0] + 2] + " " + adj);*/
          dat.centroids.push([(verts[ids3[0]]     + verts[ids3[1]]     + verts[ids3[2]])     / 3,
                              (verts[ids3[0] + 1] + verts[ids3[1] + 1] + verts[ids3[2] + 1]) / 3,
                              (verts[ids3[0] + 2] + verts[ids3[1] + 2] + verts[ids3[2] + 2]) / 3]);
        }
      }
    }
  }
}

VertexBuffer.prototype.loadLines = function(object) {
  for (var l in object.lines) {
    var dat =  object.lines[l];
    for (var i=0; i<dat.vertices.data.length/3; i++) {
      var i3 = i*3;
      var vert = [dat.vertices.data[i3], dat.vertices.data[i3+1], dat.vertices.data[i3+2]];
      this.floats[this.offset] = vert[0];
      this.floats[this.offset+1] = vert[1];
      this.floats[this.offset+2] = vert[2];
      this.ints[this.offset+3] = objVertexColour(object, dat, i);
      this.offset += this.vertexSizeInFloats;
    }
  }
}

VertexBuffer.prototype.update = function(gl) {
  start = new Date();
  gl.bufferData(gl.ARRAY_BUFFER, this.array, gl.STATIC_DRAW);
  //gl.bufferData(gl.ARRAY_BUFFER, buffer, gl.DYNAMIC_DRAW);
  //gl.bufferData(gl.ARRAY_BUFFER, this.vertices * this.elementSize, gl.DYNAMIC_DRAW);
  //gl.bufferSubData(gl.ARRAY_BUFFER, 0, buffer);

  time = (new Date() - start) / 1000.0;
  OK.debug(time + " seconds to update vertex buffer object");
}

Renderer.prototype.updateBuffers = function() {
  if (this.border) {
    this.box(vis.views[view].min, vis.views[view].max);
    this.elements = 24;
    return;
  }

  //Count vertices
  this.elements = 0;
  for (var id in vis.objects) {
    if (this.type == "triangle" && vis.objects[id].triangles) {
      for (var t in vis.objects[id].triangles)
        this.elements += vis.objects[id].triangles[t].indices.data.length;
    } else if (this.type == "particle" && vis.objects[id].points) {
      for (var t in vis.objects[id].points)
        this.elements += vis.objects[id].points[t].vertices.data.length/3;
    } else if (this.type == "line" && vis.objects[id].lines) {
      for (var t in vis.objects[id].lines)
        this.elements += vis.objects[id].lines[t].indices.data.length;
    }
  }

  if (this.elements == 0) return;
  OK.debug("Updating " + this.type + " data... (" + this.elements + " elements)");

  //Load vertices and attributes into buffers
  var start = new Date();

  //Copy data to VBOs
  var buffer = new VertexBuffer(this.elements, this.elementSize);

  //Reload vertex buffers
  if (this.type == "particle") {
    //Process points

    /*/Auto 50% subsample when > 1M particles
    var subsample = 1;
    if (document.getElementById("subsample").checked == true && newParticles > 1000000) {
      subsample = Math.round(newParticles/1000000 + 0.5);
      OK.debug("Subsampling at " + (100/subsample) + "% (" + subsample + ") to " + Math.floor(newParticles / subsample));
    }*/
    //Random subsampling
    //if (subsample > 1 && Math.random() > 1.0/subsample) continue;

    for (var id in vis.objects)
      if (vis.objects[id].points)
        buffer.loadParticles(vis.objects[id]);

  } else if (this.type == "line") {
    //Process lines
    for (var id in vis.objects)
      if (vis.objects[id].lines)
        buffer.loadLines(vis.objects[id]);

  } else if (this.type == "triangle") {
    //Process triangles
    for (var id in vis.objects)
      if (vis.objects[id].triangles)
        buffer.loadTriangles(vis.objects[id], id);
  }

  var time = (new Date() - start) / 1000.0;
  OK.debug(time + " seconds to load buffers... (elements: " + this.elements + " bytes: " + buffer.bytes.length + ")");

  buffer.update(this.gl);
}

Renderer.prototype.box = function(min, max) {
  var vertices = new Float32Array(
        [
          min[0], min[1], max[2],
          min[0], max[1], max[2],
          max[0], max[1], max[2],
          max[0], min[1], max[2],
          min[0], min[1], min[2],
          min[0], max[1], min[2],
          max[0], max[1], min[2],
          max[0], min[1], min[2]
        ]);

  var indices = new Uint16Array(
        [
          0, 1, 1, 2, 2, 3, 3, 0,
          4, 5, 5, 6, 6, 7, 7, 4,
          0, 4, 3, 7, 1, 5, 2, 6
        ]
     );
  this.gl.bufferData(this.gl.ARRAY_BUFFER, vertices, this.gl.STATIC_DRAW);
  this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, indices, this.gl.STATIC_DRAW);
}

Renderer.prototype.draw = function() {
  if (!vis.objects || !vis.objects.length) return;
  var start = new Date();
  var desc = "";
  //console.log(" ----- " + this.type + " --------------------------------------------------------------------");

  //Create buffer if not yet allocated
  if (this.vertexBuffer == undefined) {
    //Init shaders etc...
    //(This is done here now so we can skip triangle shaders if not required, 
    //due to really slow rendering when doing triangles and points... still to be looked into)
    if (!this.init()) return;
    OK.debug("Creating " + this.type + " buffers...");
    this.vertexBuffer = this.gl.createBuffer();
    this.indexBuffer = this.gl.createBuffer();
    //viewer.reload = true;
  }

  if (this.program.attributes["aVertexPosition"] == undefined) return; //Require vertex buffer

  viewer.webgl.use(this.program);
  viewer.webgl.setMatrices();

  //Bind buffers
  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

  //Reload geometry if required
  viewer.canvas.mouse.disabled = true;
  if (this.reload) this.updateBuffers();
  if (this.sort || viewer.rotated) this.loadElements();
  this.reload = this.sort = false;
  viewer.canvas.mouse.disabled = false;

  if (this.elements == 0 && this.type != "volume") return;

  //Enable attributes
  for (var key in this.program.attributes)
    this.gl.enableVertexAttribArray(this.program.attributes[key]);

  //General uniform vars
  this.gl.uniform1i(this.program.uniforms["uLighting"], 1);
  this.gl.uniform1f(this.program.uniforms["uOpacity"], vis.properties.opacity || 1.0);
  this.gl.uniform1f(this.program.uniforms["uBrightness"], vis.properties.brightness || 0.0);
  this.gl.uniform1f(this.program.uniforms["uContrast"], vis.properties.contrast || 1.0);
  this.gl.uniform1f(this.program.uniforms["uSaturation"], vis.properties.saturation || 1.0);
  this.gl.uniform1f(this.program.uniforms["uAmbient"], vis.properties.ambient || 0.4);
  this.gl.uniform1f(this.program.uniforms["uDiffuse"], vis.properties.diffuse || 0.8);
  this.gl.uniform1f(this.program.uniforms["uSpecular"], vis.properties.specular || 0.0);
  if (this.colour)
    this.gl.uniform4f(this.program.uniforms["uColour"], this.colour.red/255.0, this.colour.green/255.0, this.colour.blue/255.0, this.colour.alpha);
  var cmin = [vis.views[view].min[0] + viewer.dims[0] * (vis.properties.xmin || 0.0),
              vis.views[view].min[1] + viewer.dims[1] * (vis.properties.ymin || 0.0),
              vis.views[view].min[2] + viewer.dims[2] * (vis.properties.zmin || 0.0)];
  var cmax = [vis.views[view].min[0] + viewer.dims[0] * (vis.properties.xmax || 1.0),
              vis.views[view].min[1] + viewer.dims[1] * (vis.properties.ymax || 1.0),
              vis.views[view].min[2] + viewer.dims[2] * (vis.properties.zmax || 1.0)];
  this.gl.uniform3fv(this.program.uniforms["uClipMin"], new Float32Array(cmin));
  this.gl.uniform3fv(this.program.uniforms["uClipMax"], new Float32Array(cmax));

  if (this.type == "particle") {

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, this.elementSize, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, this.elementSize, this.attribSizes[0]);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexSize"], 1, this.gl.FLOAT, false, this.elementSize, this.attribSizes[0]+this.attribSizes[1]);
    this.gl.vertexAttribPointer(this.program.attributes["aPointType"], 1, this.gl.FLOAT, false, this.elementSize, this.attribSizes[0]+this.attribSizes[1]+this.attribSizes[2]);

    //Set uniforms...
    this.gl.uniform1i(this.program.uniforms["uPointType"], viewer.pointType);
    this.gl.uniform1f(this.program.uniforms["uPointScale"], viewer.pointScale*viewer.modelsize);

    //Draw points
    this.gl.drawElements(this.gl.POINTS, this.elements, this.gl.UNSIGNED_INT, 0);
    //this.gl.drawElements(this.gl.POINTS, this.positions.length, this.gl.UNSIGNED_SHORT, 0);
    //this.gl.drawArrays(this.gl.POINTS, 0, this.positions.length);
    desc = this.elements + " points";

  } else if (this.type == "triangle") {

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, this.elementSize, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexNormal"], 3, this.gl.FLOAT, false, this.elementSize, this.attribSizes[0]);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, this.elementSize, this.attribSizes[0]+this.attribSizes[1]);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexTexCoord"], 2, this.gl.FLOAT, true, this.elementSize, this.attribSizes[0]+this.attribSizes[1]+this.attribSizes[2]);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexObjectID"], 1, this.gl.UNSIGNED_BYTE, false, this.elementSize, this.attribSizes[0]+this.attribSizes[1]+this.attribSizes[2]+this.attribSizes[3]);

    //Set uniforms...
    //this.gl.enable(this.gl.CULL_FACE);
    //this.gl.cullFace(this.gl.BACK_FACE);
    
    //Per-object uniform arrays
    var cullfaces = [];
    for (var id in vis.objects)
      cullfaces.push(vis.objects[id].cullface ? 1 : 0);

    this.gl.uniform1iv(this.program.uniforms["uCullFace"], cullfaces);

    //Texture -- TODO: Switch per object!
    this.gl.activeTexture(this.gl.TEXTURE0);
    //this.gl.bindTexture(this.gl.TEXTURE_2D, viewer.webgl.textures[0]);
    this.gl.bindTexture(this.gl.TEXTURE_2D, vis.objects[0].tex);
    this.gl.uniform1i(this.program.uniforms["uTexture"], 0);
    this.gl.uniform1i(this.program.uniforms["uTextured"], 0); //Disabled unless texture attached!

    this.gl.uniform1i(this.program.uniforms["uCalcNormal"], 0);

    //Draw triangles
    this.gl.drawElements(this.gl.TRIANGLES, this.elements, this.gl.UNSIGNED_INT, 0);
    //this.gl.drawElements(this.gl.TRIANGLES, this.positions.length*3, this.gl.UNSIGNED_SHORT, 0);
    //this.gl.drawArrays(this.gl.TRIANGLES, 0, this.positions.length*3);
    desc = (this.elements / 3) + " triangles";

  } else if (this.border) {
    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, 0, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, 0, 0);
    this.gl.drawElements(this.gl.LINES, this.elements, this.gl.UNSIGNED_SHORT, 0);
    desc = "border";

  } else if (this.type == "line") {

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, this.elementSize, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, this.elementSize, this.attribSizes[0]);

    //Draw lines
    this.gl.drawElements(this.gl.LINES, this.elements, this.gl.UNSIGNED_INT, 0);
    desc = (this.elements / 2) + " lines";
    //this.gl.drawArrays(this.gl.LINES, 0, this.positions.length);

  } else if (this.type == "volume") {
    
    //Volume render
    //
    //Problems:
    //Clipping at back of box
    //Lighting position?
    //Uniform defaults

    if (viewer.gradient.mapid != vis.objects[this.id].colourmap) {
      //Map selected has changed, so update texture (used on initial render only)
      paletteUpdate({}, vis.objects[this.id].colourmap);
    }

    //Setup volume camera
    viewer.webgl.modelView.push();

    //Copy props
    this.properties = vis.objects[this.id];
    this.resolution = vis.objects[this.id].volume["res"];
    this.scaling = vis.objects[this.id].volume["scale"];
  
    //this.rayCamera();
    {
      //Apply translation to origin, any rotation and scaling
      viewer.webgl.modelView.identity()
      viewer.webgl.modelView.translate(viewer.translate)

      // rotate model 
      var rotmat = quat4.toMat4(viewer.rotate);
      viewer.webgl.modelView.mult(rotmat);

      //For a volume cube other than [0,0,0] - [1,1,1], need to translate/scale here...
      viewer.webgl.modelView.translate([-this.scaling[0]*0.5, -this.scaling[1]*0.5, -this.scaling[2]*0.5]);  //Translate to origin
      //viewer.webgl.modelView.translate([-0.5, -0.5, -0.5]);  //Translate to origin
      //Inverse of scaling
      viewer.webgl.modelView.scale([this.iscale[0], this.iscale[1], this.iscale[2]]);

      //Perspective matrix
      viewer.webgl.setPerspective(viewer.fov, this.gl.viewportWidth / this.gl.viewportHeight, 0.1, 1000.0);

      //Get inverted matrix for volume shader
      this.invPMatrix = mat4.create(viewer.webgl.perspective.matrix);
      mat4.inverse(this.invPMatrix);
      //console.log(JSON.stringify(viewer.webgl.modelView));
    }

    this.gl.activeTexture(this.gl.TEXTURE0);
    this.gl.bindTexture(this.gl.TEXTURE_2D, viewer.webgl.textures[0]);

    this.gl.activeTexture(this.gl.TEXTURE1);
    this.gl.bindTexture(this.gl.TEXTURE_2D, viewer.webgl.gradientTexture);

    //Only render full quality when not interacting
    //this.gl.uniform1i(this.program.uniforms["uSamples"], this.samples);
    //TODO: better default handling here!
    this.gl.uniform1i(this.program.uniforms["uSamples"], this.properties.samples || 256);
    this.gl.uniform1i(this.program.uniforms["uVolume"], 0);
    this.gl.uniform1i(this.program.uniforms["uTransferFunction"], 1);
    this.gl.uniform1i(this.program.uniforms["uEnableColour"], this.properties.usecolourmap || 1);
    this.gl.uniform1i(this.program.uniforms["uFilter"], this.properties.tricubicFilter || 0);
    this.gl.uniform4fv(this.program.uniforms["uViewport"], new Float32Array([0, 0, this.gl.viewportWidth, this.gl.viewportHeight]));

    var bbmin = [this.properties.xmin || 0.0, this.properties.ymin || 0.0, this.properties.zmin || 0.0];
    var bbmax = [this.properties.xmax || 1.0, this.properties.ymax || 1.0, this.properties.zmax || 1.0];
    this.gl.uniform3fv(this.program.uniforms["uBBMin"], new Float32Array(bbmin));
    this.gl.uniform3fv(this.program.uniforms["uBBMax"], new Float32Array(bbmax));
    this.gl.uniform3fv(this.program.uniforms["uResolution"], new Float32Array(this.resolution));

    this.gl.uniform1f(this.program.uniforms["uDensityFactor"], this.properties.density || 5);
    // brightness and contrast
    this.gl.uniform1f(this.program.uniforms["uSaturation"], this.properties.saturation || 1.0);
    this.gl.uniform1f(this.program.uniforms["uBrightness"], this.properties.brightness || 0.0);
    this.gl.uniform1f(this.program.uniforms["uContrast"], this.properties.contrast || 1.0);
    this.gl.uniform1f(this.program.uniforms["uPower"], this.properties.power || 1.0);

    this.gl.uniform1f(this.program.uniforms["uIsoValue"], this.properties.isovalue || 0.0);
    var colour = new Colour(this.properties.colour);
    colour.alpha = this.properties.isoalpha || 1.0;
    this.gl.uniform4fv(this.program.uniforms["uIsoColour"], colour.rgbaGL());
    this.gl.uniform1f(this.program.uniforms["uIsoSmooth"], this.properties.isosmooth || 1.0);
    this.gl.uniform1i(this.program.uniforms["uIsoWalls"], this.properties.isowalls);

    //Data value range (default only for now)
    this.gl.uniform2fv(this.program.uniforms["uRange"], new Float32Array([0.0, 1.0]));
    //Density clip range
    this.gl.uniform2fv(this.program.uniforms["uDenMinMax"], new Float32Array([this.properties.mindensity || 0.0, this.properties.maxdensity || 1.0]));

    //Draw two triangles
    viewer.webgl.initDraw2d();
    this.gl.uniformMatrix4fv(this.program.uniforms["uInvPMatrix"], false, this.invPMatrix);
    viewer.webgl.setMatrices();
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, viewer.webgl.vertexPositionBuffer.numItems);

    viewer.webgl.modelView.pop();
  }

  //Disable attribs
  for (var key in this.program.attributes)
    this.gl.disableVertexAttribArray(this.program.attributes[key]);

  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
  this.gl.useProgram(null);

  var time = (new Date() - start) / 1000.0;
  if (time > 0.01) OK.debug(time + " seconds to draw " + desc);
}

function minMaxDist()
{
  //Calculate min/max distances from view plane
  var M2 = [viewer.webgl.modelView.matrix[0*4+2],
            viewer.webgl.modelView.matrix[1*4+2],
            viewer.webgl.modelView.matrix[2*4+2],
            viewer.webgl.modelView.matrix[3*4+2]];
  var maxdist = -Number.MAX_VALUE, mindist = Number.MAX_VALUE;
  for (i=0; i<2; i++)
  {
     var x = i==0 ? vis.views[view].min[0] : vis.views[view].max[0];
     for (var j=0; j<2; j++)
     {
        var y = j==0 ? vis.views[view].min[1] : vis.views[view].max[1];
        for (var k=0; k<2; k++)
        {
           var z = k==0 ? vis.views[view].min[2] : vis.views[view].max[2];
           var dist = eyeDistance(M2, [x, y, z]);
           if (dist < mindist) mindist = dist;
           if (dist > maxdist) maxdist = dist;
        }
     }
  }
  //alert(mindist + " --> " + maxdist);
  if (maxdist == mindist) maxdist += 0.0000001;
  return [mindist, maxdist];
}

//This object holds the viewer details and calls the renderers
function Viewer(canvas) {
  this.canvas = canvas;
  if (canvas)
  {
    try {
      this.webgl = new WebGL(this.canvas, {antialias: true, premultipliedAlpha: false});
      this.gl = this.webgl.gl;
      this.ext = (
        this.gl.getExtension('OES_element_index_uint') ||
        this.gl.getExtension('MOZ_OES_element_index_uint') ||
        this.gl.getExtension('WEBKIT_OES_element_index_uint')
      );
      this.gl.getExtension('OES_standard_derivatives');
    } catch(e) {
      //No WebGL
      OK.debug(e);
      if (!this.webgl) document.getElementById('canvas').style.display = 'none';
    }
  }

  //Default colour editor
  this.gradient = new GradientEditor(document.getElementById('palette'), paletteUpdate);

  this.width = 0; //Auto resize
  this.height = 0;

  this.rotating = false;
  this.translate = [0,0,0];
  this.rotate = quat4.create();
  quat4.identity(this.rotate);
  this.fov = 45;
  this.focus = [0,0,0];
  this.centre = [0,0,0];
  this.near_clip = this.far_clip = 0.0;
  this.modelsize = 1;
  this.scale = [1, 1, 1];
  this.orientation = 1.0; //1.0 for RH, -1.0 for LH
  this.background = new Colour(0xff404040);
  document.body.style.background = this.background.html();
  document.getElementById("bgColour").value = '';
  this.showBorder = document.getElementById("border").checked;
  this.pointScale = 1.0;
  this.pointType = 0;

  //Create the renderers
  this.renderers = [];
  if (this.gl) {
    this.points = new Renderer(this.gl, 'particle');
    this.triangles = new Renderer(this.gl, 'triangle');
    this.lines = new Renderer(this.gl, 'line');
    this.border = new Renderer(this.gl, 'line', 0xffffffff, true);

    this.renderers.push(this.points);
    this.renderers.push(this.triangles);
    this.renderers.push(this.lines);
    this.renderers.push(this.border);

    this.gl.enable(this.gl.DEPTH_TEST);
    this.gl.depthFunc(this.gl.LEQUAL);
    //this.gl.depthMask(this.gl.FALSE);
    this.gl.enable(this.gl.BLEND);
    //this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);
    //this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ZERO, this.gl.ONE);
    this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
  }
}

Viewer.prototype.loadFile = function(source) {
  //Skip update to rotate/translate etc if in process of updating
  //console.log(source);
  if (document.mouse && document.mouse.isdown) return;
  var start = new Date();
  var updated = true;
  try {
    if (typeof source != 'object') {
      if (source.indexOf("{") < 0) {
        if (server) {
          //Not a json string, assume a command script
          //TODO: this doesn't seem to work in LavaVR?
          var lines = source.split("\n");
          for (var line in lines) {
            sendCommand('' + lines[line]);
          }
          return;
        } else {
          console.log(source);
          alert("Invalid data");
          return;
        }
      } else if (source.substr(8) == "loadData") {
        //jsonp, strip function call when loaded from FileReader
        source = source.substring(10, source.lastIndexOf("\n"));
      }
      source = JSON.parse(source);
    }
  } catch(e) {
    console.log(source);
    alert(e);
  }
  var time = (new Date() - start) / 1000.0;
  OK.debug(time + " seconds to parse data");

  if (source.exported) {
    if (!vis.views && !vis.views[view]) {OK.debug("Exported settings require loaded model"); return;}
    var old = this.toString();
    //Copy, overwriting if exists in source
    if (source.views[view].rotate) vis.views[view].rotate = source.views[view].rotate;
    Merge(vis, source);
    if (!source.reload) updated = false;  //No reload necessary
  } else {
    //Clear geometry
    for (var type in types)
      if (this[type])
        this[type].elements = 0;

    //Replace
    vis = source;

    //Add default colour maps
    defaultColourMaps();
  }

  //Always set a bounding box, get from objects if not in view
  var objbb = false;
  if (!vis.views[view].min || !vis.views[view].max) {
    vis.views[view].min = [Number.MAX_VALUE, Number.MAX_VALUE, Number.MAX_VALUE];
    vis.views[view].max = [-Number.MAX_VALUE, -Number.MAX_VALUE, -Number.MAX_VALUE];
    objbb = true;
  }
  //console.log(JSON.stringify(vis.views[view]));

  //Load some user options...
  loadColourMaps();

  if (vis.views[view]) {
    this.near_clip = vis.views[view].near || 0;
    this.far_clip = vis.views[view].far || 0;
    this.orientation = vis.views[view].orientation || 1;
    this.showBorder = vis.views[view].border == undefined ? true : vis.views[view].border > 0;
    this.axes = vis.views[view].axis == undefined ? true : vis.views[view].axis;
  }

  this.pointScale = vis.properties.scalepoints || 1.0;
  this.pointType = vis.properties.pointtype; // > 0 ? vis.properties.pointtype : 0;

  this.applyBackground(vis.properties.background);

  if (this.canvas && vis.properties.resolution && vis.properties.resolution[0] && vis.properties.resolution[1]) {
    this.width = vis.properties.resolution[0];
    this.height = vis.properties.resolution[1];
    this.canvas.style.width = "";
    this.canvas.style.height = "";
  }

  //Copy global options to controls where applicable..
  document.getElementById("bgColour").value = this.background.r;
  document.getElementById("pointScale-out").value = (this.pointScale || 1.0);
  document.getElementById("pointScale").value = document.getElementById("pointScale-out").value * 10.0;
  document.getElementById("border").checked = this.showBorder;
  document.getElementById("axes").checked = this.axes;
  document.getElementById("globalPointType").value = this.pointType;

  document.getElementById("global-opacity").value = document.getElementById("global-opacity-out").value = (vis.properties.opacity || 1.0).toFixed(2);
  document.getElementById('global-brightness').value = document.getElementById("global-brightness-out").value = (vis.properties.brightness || 0.0).toFixed(2);
  document.getElementById('global-contrast').value = document.getElementById("global-contrast-out").value = (vis.properties.contrast || 1.0).toFixed(2);
  document.getElementById('global-saturation').value = document.getElementById("global-saturation-out").value = (vis.properties.saturation || 1.0).toFixed(2);

  document.getElementById('global-xmin').value = document.getElementById("global-xmin-out").value = (vis.properties.xmin || 0.0).toFixed(2);
  document.getElementById('global-xmax').value = document.getElementById("global-xmax-out").value = (vis.properties.xmax || 1.0).toFixed(2);
  document.getElementById('global-ymin').value = document.getElementById("global-ymin-out").value = (vis.properties.ymin || 0.0).toFixed(2);
  document.getElementById('global-ymax').value = document.getElementById("global-ymax-out").value = (vis.properties.ymax || 1.0).toFixed(2);
  document.getElementById('global-zmin').value = document.getElementById("global-zmin-out").value = (vis.properties.zmin || 0.0).toFixed(2);
  document.getElementById('global-zmax').value = document.getElementById("global-zmax-out").value = (vis.properties.zmax || 1.0).toFixed(2);

  //Load objects and add to form
  var objdiv = document.getElementById("objects");
  removeChildren(objdiv);

  //Decode into Float buffers and replace original base64 data
  //-Colour lookups: do in shader with a texture?
  //-Sizes don't change at per-particle level here, per-object? send id and calc size in shader? (short:id + short:size = 4 bytes)

  //min = [Number.MAX_VALUE, Number.MAX_VALUE, Number.MAX_VALUE];
  //max = [-Number.MAX_VALUE, -Number.MAX_VALUE, -Number.MAX_VALUE];

  //Process object data and convert base64 to Float32Array
  if (!source.exported) this.vertexCount = 0;
  for (var id in vis.objects) {
    //Opacity bug fix hack
    if (!vis.objects[id]['opacity']) vis.objects[id]['opacity'] = 1.0;
    var name = vis.objects[id].name;
    //Process points/triangles
    if (!source.exported) {
      //Texure loading
      if (vis.objects[id].texturefile) {
        this.hasTexture = true;
        vis.objects[id].image = new Image();
        vis.objects[id].image.src = vis.objects[id].texturefile;
        vis.objects[id].image.onload = function() {
          // Flip the image's Y axis to match the WebGL texture coordinate space.
          viewer.webgl.gl.activeTexture(viewer.webgl.gl.TEXTURE0);
          vis.objects[id].tex = viewer.webgl.loadTexture(vis.objects[id].image, viewer.gl.LINEAR, viewer.gl.RGBA, true); //Flipped
          viewer.drawFrame();
          viewer.draw();
        };
      }

      for (var type in vis.objects[id]) {
        if (["triangles", "points", "lines", "volume"].indexOf(type) < 0) continue;
        if (type == "triangles") this.hasTriangles = true;
        if (type == "points") this.hasPoints = true;
        if (type == "lines") this.hasLines = true;

        if (type == "volume" && vis.objects[id][type].url) {
          //Single volume per object only, each volume needs own renderer
          this.hasVolumes = true;
          var vren = new Renderer(this.gl, 'volume');
          vren.id = id;
          this.renderers.push(vren);
          vren.image = new Image();
          vren.image.src = vis.objects[id][type].url;
          vren.image.onload = function(){ viewer.drawFrame(); viewer.draw(); };
        }

        //Apply object bounding box
        if (objbb && vis.objects[id].min)
          checkPointMinMax(vis.objects[id].min);
        if (objbb && vis.objects[id].max)
          checkPointMinMax(vis.objects[id].max);

        //Read vertices, values, normals, sizes, etc...
        for (var idx in vis.objects[id][type]) {
          //Only support following data types for now
          decodeBase64(id, type, idx, 'vertices');
          decodeBase64(id, type, idx, 'values');
          decodeBase64(id, type, idx, 'normals');
          decodeBase64(id, type, idx, 'texcoords');
          decodeBase64(id, type, idx, 'colours', 'integer');
          decodeBase64(id, type, idx, 'sizes');
          decodeBase64(id, type, idx, 'indices', 'integer');

          if (vis.objects[id][type][idx].vertices) {
            OK.debug("Loaded " + vis.objects[id][type][idx].vertices.data.length/3 + " vertices from " + name);
            this.vertexCount += vis.objects[id][type][idx].vertices.data.length/3;
          }

          //Create indices for cross-sections & lines
          if (type == 'lines' && !vis.objects[id][type][idx].indices) {
            //Collect indices
            var verts = vis.objects[id][type][idx].vertices.data;
            var buf = new Uint32Array(verts.length);
            for (var j=0; j < verts.length; j++)  //Iterate over h-1 strips
              buf[j] = j;
            vis.objects[id][type][idx].indices = {"data" : buf, "type" : "integer"};
          }
          if (type == 'triangles' && !vis.objects[id][type][idx].indices) {
            //Collect indices
            var h = vis.objects[id][type][idx].height;
            var w = vis.objects[id][type][idx].width;
            var buf = new Uint32Array((w-1)*(h-1)*6);
            var i = 0;
            for (var j=0; j < h-1; j++)  //Iterate over h-1 strips
            {
              var offset0 = j * w;
              var offset1 = (j+1) * w;
              for (var k=0; k < w-1; k++)  //Iterate width of strips, w vertices
              {
                 //Tri 1
                 buf[i++] = offset0 + k;
                 buf[i++] = offset1 + k;
                 buf[i++] = offset0 + k + 1;
                 //Tri 2
                 buf[i++] = offset1 + k;
                 buf[i++] = offset0 + k + 1;
                 buf[i++] = offset1 + k + 1;
              }
            }

            vis.objects[id][type][idx].indices = {"data" : buf, "type" : "integer"};
            /*/Get center point for depth sort...
            var verts = vis.objects[id][type][idx].vertices.data;
            vis.objects[id][type][idx].centroids = [null];
            vis.objects[id][type][idx].centroids = 
              [[
                (verts[0]+verts[verts.length-3])/2,
                (verts[1]+verts[verts.length-2])/2,
                (verts[2]+verts[verts.length-1])/2
              ]];*/
          }
        }
      }
    }

    this.updateDims(vis.views[view]);

    var div= document.createElement('div');
    div.className = "object-control";
    objdiv.appendChild(div);

    var check= document.createElement('input');
    //check.checked = !vis.objects[id].hidden;
    check.checked = !(vis.objects[id].visible === false);
    check.setAttribute('type', 'checkbox');
    check.setAttribute('name', 'object_' + name);
    check.setAttribute('id', 'object_' + name);
    check.setAttribute('onchange', 'viewer.action(' + id + ', false, true, this);');
    div.appendChild(check);

    var label= document.createElement('label');
    label.appendChild(label.ownerDocument.createTextNode(name));
    label.setAttribute("for", 'object_' + name);
    div.appendChild(label);

    var props = document.createElement('input');
    props.type = "button";
    props.value = "...";
    props.id = "props_" + name;
    props.setAttribute('onclick', 'viewer.properties(' + id + ');');
    div.appendChild(props);
  }
  var time = (new Date() - start) / 1000.0;
  OK.debug(time + " seconds to import data");

  //Default to interactive render if vertex count < 0.5 M
  document.getElementById("interactive").checked = (this.vertexCount <= 500000);

  //TODO: loaded custom shader is not replaced by default when new model loaded...
  for (var type in types) {
    if (this[type]) {
      //Custom shader load
      if (vis.shaders && vis.shaders[types[type]])
        this[type].init();
      //Set update flags
      this[type].sort = this[type].reload = updated;
    }
  }

  //Defer drawing until textures loaded if necessary
  if (!this.hasVolumes && !this.hasTexture) {
    this.drawFrame();
    this.draw();
  }
}

function Merge(obj1, obj2) {
  for (var p in obj2) {
    try {
      //alert(p + " ==> " + obj2[p].constructor);
      // Property in destination object set; update its value.
      if (obj2[p].constructor == Object || obj2[p].constructor == Array) {
        obj1[p] = Merge(obj1[p], obj2[p]);
      } else {
        obj1[p] = obj2[p];
      }
    } catch(e) {
      // Property in destination object not set; create it and set its value.
      obj1[p] = obj2[p];
    }
  }
  return obj1;
}

Viewer.prototype.toString = function(nocam, reload) {
  var exp = {"objects"    : this.exportObjects(), 
             "colourmaps" : this.exportColourMaps(),
             "views"      : this.exportViews(nocam),
             "properties" : vis.properties};

  exp.exported = true;
  exp.reload = reload ? true : false;

  if (nocam) return JSON.stringify(exp);
  //Export with 2 space indentation
  return JSON.stringify(exp, undefined, 2);
}

Viewer.prototype.exportViews = function(nocam) {
  //Update camera settings of current view
  if (nocam)
    vis.views[view] = {};
  else {
    vis.views[view].rotate = this.getRotation();
    vis.views[view].focus = this.focus;
    vis.views[view].translate = this.translate;
    vis.views[view].scale = this.scale;
  }
  vis.views[view].near = this.near_clip;
  vis.views[view].far = this.far_clip;
  vis.views[view].border = this.showBorder ? 1 : 0;
  vis.views[view].axis = this.axes;
  //views[view].background = this.background.toString();
  return vis.views;
}

Viewer.prototype.exportObjects = function() {
  objs = [];
  for (var id in vis.objects) {
    objs[id] = {};
    //Skip geometry data
    for (var type in vis.objects[id]) {
      if (type != "triangles" && type != "points" && type != 'lines' && type != "volume") {
        objs[id][type] = vis.objects[id][type];
      }
    }
  }
  return objs;
}

Viewer.prototype.exportColourMaps = function() {
  cmaps = [];
  if (vis.colourmaps) {
    for (var i=0; i<vis.colourmaps.length; i++) {
      cmaps[i] = vis.colourmaps[i].palette.get();
      //Copy additional properties
      for (var type in vis.colourmaps[i]) {
        if (type != "palette" && type != "colours")
          cmaps[i][type] = vis.colourmaps[i][type];
      }
    }
  }
  return cmaps;
}

Viewer.prototype.exportFile = function() {
  window.open('data:text/plain;base64,' + window.btoa(this.toString(false, true)));
}

Viewer.prototype.properties = function(id) {
  //Properties button clicked... Copy to controls
  properties.id = id;
  document.getElementById('obj_name').innerHTML = vis.objects[id].name;
  this.setColourMap(vis.objects[id].colourmap);

  function loadProp(name, def) {document.getElementById(name + '-out').value = document.getElementById(name).value = vis.objects[id][name] ? parseFloat(vis.objects[id][name].toFixed(2)) : def;}

  loadProp('opacity', 1.0);
  document.getElementById('pointSize').value = vis.objects[id].pointsize ? vis.objects[id].pointsize : 10.0;
  document.getElementById('pointType').value = vis.objects[id].pointtype ? vis.objects[id].pointtype : -1;

  document.getElementById('wireframe').checked = vis.objects[id].wireframe;
  document.getElementById('cullface').checked = vis.objects[id].cullface;

  loadProp('density', 5);
  loadProp('power', 1.0);
  loadProp('samples', 255);
  loadProp('isovalue', 0.0);
  loadProp('isosmooth', 1.0);
  loadProp('isoalpha', 1.0);
  loadProp('dminclip', 0.0);
  loadProp('dmaxclip', 1.0);
  document.getElementById('isowalls').checked = vis.objects[id].isowalls;
  document.getElementById('isofilter').checked = vis.objects[id].isofilter;

  var c = new Colour(vis.objects[id].colour);
  document.getElementById('colour_set').style.backgroundColor = c.html();

  //Type specific options
  setAll(vis.objects[id].points ? 'block' : 'none', 'point-obj');
  setAll(vis.objects[id].triangles ? 'block' : 'none', 'surface-obj');
  setAll(vis.objects[id].volume ? 'block' : 'none', 'volume-obj');
  setAll(vis.objects[id].lines ? 'block' : 'none', 'line-obj');

  properties.show();
}

//Global property set
Viewer.prototype.setProperties = function() {
  function setProp(name, fieldname) {
    if (fieldname == undefined) fieldname = name;
    vis.properties[name] = document.getElementById('global-' + fieldname + '-out').value = parseFloat(document.getElementById('global-' + fieldname).value);
  }

  viewer.pointScale = vis.properties.scalepoints = document.getElementById('pointScale-out').value = document.getElementById('pointScale').value / 10.0;
  if (document.getElementById('globalPointType').value)
    viewer.pointType = vis.properties.pointtype = parseInt(document.getElementById('globalPointType').value);
  viewer.showBorder = document.getElementById("border").checked;
  viewer.axes = document.getElementById("axes").checked;
  var c = document.getElementById("bgColour").value;
  if (c != "") {
    var cc = Math.round(255*c);
    //vis.views[view].background = "rgba(" + cc + "," + cc + "," + cc + ",1.0)"
    vis.properties.background = "rgba(" + cc + "," + cc + "," + cc + ",1.0)"
  }
  setProp('opacity');
  setProp('brightness');
  setProp('contrast');
  setProp('saturation');
  setProp('xmin');
  setProp('xmax');
  setProp('ymin');
  setProp('ymax');
  setProp('zmin');
  setProp('zmax');

  //Set the local/server props
  if (server) {
    //Export all data except views
    ajaxPost('/post', this.toString(true, false)); //, callback, progress, headers)
    setTimeout(requestObjects, 100);
  } else {
    viewer.applyBackground(vis.properties.background);
    viewer.draw();
  }
}

Viewer.prototype.setTimeStep = function() {
  //TODO: To implement this accurately, need to get timestep range from server
  //For now just do a jump and reset slider to middle
  var timejump = document.getElementById("timestep").value - 50.0;
  document.getElementById("timestep").value = 50;
  if (server) {
    //Issue server commands
    sendCommand('jump ' + timejump);
  } else {
    //No timesteps supported in WebGL viewer yet
  }
}

Viewer.prototype.applyBackground = function(bg) {
  if (!bg) return;
  this.background = new Colour(bg);
  var hsv = this.background.HSV();
  if (this.border) this.border.colour = hsv.V > 50 ? new Colour(0xff444444) : new Colour(0xffbbbbbb);
  document.body.style.background = this.background.html();
}

Viewer.prototype.addColourMap = function() {
  if (properties.id == undefined) return;
  var name = prompt("Enter colourmap name:");
  //New colourmap on active object
  if (server)
    sendCommand("colourmap " + properties.id + " add");

  var newmap = {
    "name": name,
    "range": [0, 1], "logscale": 0,
    "colours": [{"position": 0, "colour": "rgba(0,0,0,0)"},{"position": 1,"colour": "rgba(255,255,255,1)"}
    ]
  }
  vis.colourmaps.push(newmap);

  loadColourMaps();

  //Select newly added
  var list = document.getElementById('colourmap-presets');
  viewer.setColourMap(list.options.length-2)
}

Viewer.prototype.setColourMap = function(id) {
  if (properties.id == undefined) return;
  vis.objects[properties.id].colourmap = parseInt(id);
  if (id === undefined) id = -1;
  //Set new colourmap on active object
  document.getElementById('colourmap-presets').value = id;
  if (id < 0) {
    document.getElementById('palette').style.display = 'none';
    document.getElementById('log').style.display = 'none';
  } else {
    //Draw palette UI
    document.getElementById('logscale').checked = vis.colourmaps[id].logscale;
    document.getElementById('log').style.display = 'block';
    document.getElementById('palette').style.display = 'block';
    viewer.gradient.palette = vis.colourmaps[id].palette;
    viewer.gradient.mapid = id; //Save the id
    viewer.gradient.update();
  }
}

Viewer.prototype.setObjectProperties = function() {
  //Copy from controls
  var id = properties.id;
  function setProp(name) {vis.objects[id][name] = document.getElementById(name + '-out').value = parseFloat(document.getElementById(name).value);}
  vis.objects[id].pointsize = document.getElementById('pointSize-out').value = document.getElementById('pointSize').value / 10.0;
  vis.objects[id].pointtype = parseInt(document.getElementById('pointType').value);
  vis.objects[id].wireframe = document.getElementById('wireframe').checked;
  vis.objects[id].cullface = document.getElementById('cullface').checked;
  setProp('opacity');
  setProp('density');
  setProp('power');
  setProp('samples'); //parseInt??
  setProp('isovalue');
  setProp('isosmooth');
  setProp('isoalpha');
  setProp('dminclip');
  setProp('dmaxclip');
  vis.objects[id].isowalls = document.getElementById('isowalls').checked;
  vis.objects[id].isofilter = document.getElementById('isofilter').checked;
  var colour = new Colour(document.getElementById('colour_set').style.backgroundColor);
  vis.objects[id].colour = colour.html();
  if (vis.objects[id].colourmap >= 0)
    vis.colourmaps[vis.objects[id].colourmap].logscale = document.getElementById('logscale').checked;

  //Flag reload on WebGL objects
  for (var type in types) {
    if (vis.objects[id][type] && this[type])
      this[type].sort = this[type].reload = true;
  }
  viewer.draw();

  //Server side...
  if (server) {
    //Export all data except views
    ajaxPost('/post', this.toString(true, true)); //, callback, progress, headers)
    setTimeout(requestObjects, 100);
  }
}

Viewer.prototype.action = function(id, reload, sort, el) {
  //Object checkbox clicked
  if (server) {
    var show = el.checked;
    if (show) {
      vis.objects[id].visible = true;
      sendCommand('show ' + (id+1));
    } else {
      vis.objects[id].visible = false;
      sendCommand('hide ' + (id+1));
    }
    return;
  }

  document.getElementById('apply').disabled = false;

  for (var type in types) {
    if (vis.objects[id][type] && this[type]) {
      this[type].sort = sort;
      this[type].reload = reload;
    }
  }
}

Viewer.prototype.apply = function() {
  document.getElementById('apply').disabled = true;
  this.draw();
}

function decodeBase64(id, type, idx, datatype, format) {
  if (!format) format = 'float';
  if (!vis.objects[id][type][idx][datatype]) return null;
  var buf;
  if (typeof vis.objects[id][type][idx][datatype].data == 'string') {
    //Base64 encoded string
    var decoded = atob(vis.objects[id][type][idx][datatype].data);
    var buffer = new ArrayBuffer(decoded.length);
    var bufView = new Uint8Array(buffer);
    for (var i=0, strLen=decoded.length; i<strLen; i++) {
      bufView[i] = decoded.charCodeAt(i);
    }
    if (format == 'float')
      buf = new Float32Array(buffer);
    else
      buf = new Uint32Array(buffer);
  } else {
    //Literal array
    if (format == 'float')
      buf = new Float32Array(vis.objects[id][type][idx][datatype].data);
    else
      buf = new Uint32Array(vis.objects[id][type][idx][datatype].data);
  }

  vis.objects[id][type][idx][datatype].data = buf;
  vis.objects[id][type][idx][datatype].type = format;

  if (datatype == 'values') {
    if (!vis.objects[id][type][idx].values.minimum ||
        !vis.objects[id][type][idx].values.maximum) {
      //Value field max min
      var minval = Number.MAX_VALUE, maxval = -Number.MAX_VALUE;
      for (var i=0; i<buf.length; i++) {
        if (buf[i] > maxval)
          maxval = buf[i];
        if (buf[i] < minval)
          minval = buf[i];
      }

      //Set from data where none provided...
      if (!vis.objects[id][type][idx].values.minimum)
        vis.objects[id][type][idx].values.minimum = minval;
      if (!vis.objects[id][type][idx].values.maximum)
        vis.objects[id][type][idx].values.maximum = maxval;
    }
  }
}

function removeChildren(element) {
  if (element.hasChildNodes()) {
    while (element.childNodes.length > 0 )
    element.removeChild(element.firstChild);
  }
}

paletteUpdate = function(obj, id) {
  if (id != undefined) viewer.gradient.mapid = id;
  //Load colourmap change
  if (viewer.gradient.mapid < 0) return;
  var cmap = vis.colourmaps[viewer.gradient.mapid];
  if (!cmap) return;

  paletteLoad(cmap.palette);

  //Update colour data
  cmap.colours = cmap.palette.colours;

  if (viewer.webgl)
    viewer.webgl.updateTexture(viewer.webgl.gradientTexture, gradient, viewer.gl.TEXTURE1);  //Use 2nd texture unit
}

paletteLoad = function(palette) {
  //Update colours and cache
  var canvas = document.getElementById('palette');
  var gradient = document.getElementById('gradient');
  var context = gradient.getContext('2d');  
  if (!context) alert("getContext failed");

  //Get colour data and store in array
  //Redraw without UI elements
  //palette.draw(canvas, false);
  palette.draw(gradient, false);
  //Cache colour values
  var pixels = context.getImageData(0, 0, MAXIDX+1, 1).data;
  palette.cache = [];
  for (var c=0; c<=MAXIDX; c++)
    palette.cache[c] = pixels[c*4] + (pixels[c*4+1] << 8) + (pixels[c*4+2] << 16) + (pixels[c*4+3] << 24);

  //Redraw UI
  palette.draw(canvas, true);
}

Viewer.prototype.drawTimed = function() {
  if (this.drawTimer)
    clearTimeout(this.drawTimer);
  this.drawTimer = setTimeout(function () {viewer.drawFrame();}, 100 );
}

Viewer.prototype.draw = function(borderOnly) {
  //If requested draw border only (used while interacting)
  //Draw the full model on a timer
  viewer.drawFrame(borderOnly);
  if (borderOnly && !server) viewer.drawTimed();
}

Viewer.prototype.drawFrame = function(borderOnly) {
  if (!this.canvas) return;

  if (server && !document.mouse.isdown && this.gl) {
    //Don't draw in server mode unless interacting (border view)
    this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
    return;
  }
  
  //Show screenshot while interacting or if using server
  //if (server || borderOnly)
  var frame = document.getElementById('frame');
  if (frame) {
    if (server) {
      frame.style.display = 'block';
      this.width = frame.offsetWidth;
      this.height = frame.offsetHeight;
      this.canvas.style.width = this.width + "px";
      this.canvas.style.height = this.height + "px";
    } else { 
      frame.style.display = 'none';
    }
  }
  
  if (!this.gl) return;

  if (this.width == 0 || this.height == 0) {
    //Get size from window
    this.width = window.innerWidth;
    this.height = window.innerHeight;
  }

  if (this.width != this.canvas.width || this.height != this.canvas.height) {
    this.canvas.width = this.width;
    this.canvas.height = this.height;
    this.canvas.setAttribute("width", this.width);
    this.canvas.setAttribute("height", this.height);
    if (this.gl) {
      this.gl.viewportWidth = this.width;
      this.gl.viewportHeight = this.height;
      this.webgl.viewport = new Viewport(0, 0, this.width, this.height);      
    }
  }
  this.width = this.height = 0;

  var start = new Date();

  this.gl.viewport(0, 0, this.gl.viewportWidth, this.gl.viewportHeight);
  //  console.log(JSON.stringify(this.webgl.viewport));
  //this.gl.clearColor(this.background.red/255, this.background.green/255, this.background.blue/255, server || document.mouse.isdown ? 0 : 1);
  this.gl.clearColor(this.background.red/255, this.background.green/255, this.background.blue/255, server ? 0 : 1);
  //this.gl.clearColor(this.background.red/255, this.background.green/255, this.background.blue/255, 0);
  //this.gl.clearColor(1, 1, 1, 0);
  this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

  //Apply the camera
  this.webgl.view(this);
  
  //Render objects
  for (var r in this.renderers) {
    //if (!document.mouse.isdown && !this.showBorder && type == 'border') continue;
    if (this.renderers[r].border) {
      if (!borderOnly && !this.showBorder) continue;
    } else {
      if (borderOnly) continue;
    }

    this.renderers[r].draw();
    //console.log("Draw: " + r + " : " + this.renderers[r].type);
  }

  /*/Save canvas image to background for display while interacting
  if (!borderOnly && !server) {
    if (document.mouse.isdown || !("frame").src) {
      //document.getElementById("frame").src = document.getElementById('canvas').toDataURL("image/png");
      //document.getElementById('canvas').toBlob(function (blob) {document.getElementById("frame").src =  URL.createObjectURL(blob);});
      this.border.draw();
    }
  }*/

  this.rotated = false; //Clear rotation flag
}

Viewer.prototype.rotateX = function(deg) {
  this.rotation(deg, [1,0,0]);
}

Viewer.prototype.rotateY = function(deg) {
  this.rotation(deg, [0,1,0]);
}

Viewer.prototype.rotateZ = function(deg) {
  this.rotation(deg, [0,0,1]);
}

Viewer.prototype.rotation = function(deg, axis) {
  //Quaterion rotate
  var arad = deg * Math.PI / 180.0;
  var rotation = quat4.fromAngleAxis(arad, axis);
  rotation = quat4.normalize(rotation);
  this.rotate = quat4.multiply(rotation, this.rotate);
}

Viewer.prototype.getRotation = function() {
  return [viewer.rotate[0], viewer.rotate[1], viewer.rotate[2], viewer.rotate[3]];
}

Viewer.prototype.getRotationString = function() {
  //Return current rotation quaternion as string
  var q = this.getRotation();
  return 'rotation ' + q[0] + ' ' + q[1] + ' ' + q[2] + ' ' + q[3];
}

Viewer.prototype.getTranslationString = function() {
  return 'translation ' + this.translate[0] + ' ' + this.translate[1] + ' ' + this.translate[2];
}

Viewer.prototype.reset = function() {
  if (this.gl) {
    this.updateDims(vis.views[view]);
    this.draw();
  }

  if (server) {
    //sendCommand('' + this.getRotationString());
    //sendCommand('' + this.getTranslationString());
    sendCommand('reset');
  }
}

Viewer.prototype.zoom = function(factor) {
  factor = 0.01*factor*factor*factor;
  if (window.navigator.platform.indexOf("Mac") >= 0)
    factor *= 0.1;

  if (this.gl) {
    var adj = factor * this.modelsize;
    if (Math.abs(this.translate[2]) < this.modelsize) adj *= 0.1;
    this.translate[2] += adj;
    if (this.translate[2] > this.modelsize*0.3) this.translate[2] = this.modelsize*0.3;
    this.draw();
  }

  if (server)
    sendCommand('' + this.getTranslationString());
    //sendCommand('zoom ' + factor);
}

Viewer.prototype.zoomClip = function(factor) {
  if (this.gl) {
     var near_clip = this.near_clip + factor * this.modelsize;
     if (near_clip >= this.modelsize * 0.001)
       this.near_clip = near_clip;
    this.draw();
  }
  if (server)
    sendCommand('zoomclip ' + factor);
}

Viewer.prototype.updateDims = function(view) {
  if (!view) return;
  var oldsize = this.modelsize;
  this.dims = [view.max[0] - view.min[0], view.max[1] - view.min[1], view.max[2] - view.min[2]];
  this.modelsize = Math.sqrt(this.dims[0]*this.dims[0] + this.dims[1]*this.dims[1] + this.dims[2]*this.dims[2]);

  this.focus = [view.min[0] + 0.5*this.dims[0], view.min[1] + 0.5*this.dims[1], view.min[2] + 0.5*this.dims[2]];
  this.centre = [this.focus[0],this.focus[1],this.focus[2]];

  this.translate = [0,0,0];
  if (this.modelsize != oldsize) this.translate[2] = -this.modelsize*1.25;

  if (this.near_clip == 0.0) this.near_clip = this.modelsize / 10.0;   
  if (this.far_clip == 0.0) this.far_clip = this.modelsize * 10.0;

  quat4.identity(this.rotate);
  this.rotated = true; 

  if (view) {
    //Initial rotation
    if (view.rotate) {
      if (view.rotate.length == 3) {
        this.rotateZ(-view.rotate[2]);
        this.rotateY(-view.rotate[1]);
        this.rotateX(-view.rotate[0]);
      } else if (view.rotate.length == 4) {
        this.rotate = quat4.create(view.rotate);
      }
    }

    //Translate
    if (view.translate) {
      this.translate[0] = view.translate[0];
      this.translate[1] = view.translate[1];
      this.translate[2] = view.translate[2];
    }

    //Scale
    if (view.scale) {
      this.scale[0] = view.scale[0];
      this.scale[1] = view.scale[1];
      this.scale[2] = view.scale[2];
    }

    //Focal point
    if (view.focus) {
      this.focus[0] = this.centre[0] = view.focus[0];
      this.focus[1] = this.centre[1] = view.focus[1];
      this.focus[2] = this.centre[2] = view.focus[2];
    }

  }

  /*console.log("DIMS: " + this.dims[0] + "," + this.dims[1] + "," + this.dims[2]);
  console.log("New model size: " + this.modelsize + ", Focal point: " + this.focus[0] + "," + this.focus[1] + "," + this.focus[2]);
  console.log("Translate: " + this.translate[0] + "," + this.translate[1] + "," + this.translate[2]);
  console.log("Rotate: " + this.rotate[0] + "," + this.rotate[1] + "," + this.rotate[2] + "," + this.rotate[3]);
  console.log(JSON.stringify(view));*/
}

function resizeToWindow() {
  //var canvas = document.getElementById('canvas');
  //if (canvas.width < window.innerWidth || canvas.height < window.innerHeight)
    sendCommand('resize ' + window.innerWidth + " " + window.innerHeight);
  var frame = document.getElementById('frame');
  if (frame) {
    canvas.style.width = frame.style.width = "100%";
    canvas.style.height = frame.style.height = "100%";
  }
}

function connectWindow() {
  requestData('/render');
  window.location.reload();
}
