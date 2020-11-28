//WebGL viewer, Owen Kaluza (c) Monash University 2012-18
// License: LGPLv3 for now
var server = false;
var types = ["triangles", "points", "lines", "volume"]
var MAXIDX = 2047;
/** @const */
var DEBUG = false;

function initPage(elid, menu, src) {
  //Load from the data tag if available, otherwise URL
  var dtag = document.getElementById('data');
  if (!src && dtag && dtag.innerHTML.length > 100)
    src = dtag.innerHTML;

  var urlq = decodeURI(window.location.href);
  if (!src && urlq.indexOf("?") > 0) {
    var parts = urlq.split("?"); //whole querystring before and after ?
    var query = parts[1]; 

    if (query.indexOf(".json") > 0) {
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
        var el = document.getElementById('fileupload');
        if (el) el.style.display = "none";
        progress("Downloading model data from server...");
        ajaxReadFile(query, initPage, false, updateProgress);
      }
      return;
    }
  } else if (!src && urlq.indexOf("#") > 0) {
    //IPython strips out ? args so have to check for this instead
    var parts = urlq.split("#"); //whole querystring before and after #
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

  var container;
  if (elid)
    container = document.getElementById(elid);
  else
    container = document.body;
  var canvas = document.createElement("canvas");
  canvas.id = "canvas_" + container.id;
  container.appendChild(canvas);
  var viewer =  new Viewer(canvas);
  viewer.menu = menu; //GUI menu flag
  if (canvas) {
    //this.canvas = document.createElement("canvas");
    //this.canvas.style.cssText = "width: 100%; height: 100%; z-index: 0; margin: 0px; padding: 0px; background: black; border: none; display:block;";
    //if (!parentEl) parentEl = document.body;
    //parentEl.appendChild(this.canvas);

    //Canvas event handling
    var handler = new MouseEventHandler(canvasMouseClick, canvasMouseWheel, canvasMouseMove, canvasMouseDown, null, null, canvasMousePinch);
    canvas.mouse = new Mouse(canvas, handler);
    //Following two settings should probably be defaults?
    canvas.mouse.moveUpdate = true; //Continual update of deltaX/Y
    //canvas.mouse.setDefault();

    canvas.mouse.wheelTimer = true; //Accumulate wheel scroll (prevents too many events backing up)
    defaultMouse = document.mouse = canvas.mouse;

    //Reference to viewer object on canvas for event handling
    canvas.viewer = viewer;
    window.viewer = viewer; //Only most recent stored on window

    //Enable auto resize for full screen view
    if (canvas.parentElement == document.body) {
      window.onresize = function() {viewer.drawTimed();};
      //No border
      canvas.style.cssText = "width: 100%; height: 100%; z-index: 0; margin: 0px; padding: 0px; background: black; border: none; display:block;";
    } else {
      //Light border
      canvas.style.cssText = "width: 100%; height: 100%; z-index: 0; margin: 0px; padding: 0px; background: black; border: 1px solid #aaa; display:block;";
    }
  }

  //Data dict and colourmap names stored in globals
  viewer.dict = window.dictionary;
  viewer.defaultcolourmaps = window.defaultcolourmaps;

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

  if (src) {
    viewer.loadFile(src);
  } else {
    var source = getSourceFromElement('source');
    if (source) {
      //Preloaded data
      var el = document.getElementById('fileupload');
      if (el) el.style.display = "none";
      viewer.loadFile(source);
    } else {
      //Demo objects
      //demoData();
      viewer.draw();
    }
  }

  //VR setup
  if (navigator.getVRDisplays)
    setup_VR(viewer);
}

function loadData(data) {
  initPage(data);
}

function progress(text) {
  var el = document.getElementById('progress');
  if (!el) return;
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
    if (mouse.element.viewer.rotating)
      sendCommand('' + mouse.element.viewer.getRotationString());
    else
      sendCommand('' + mouse.element.viewer.getTranslationString());
  }

  //if (server) serverMouseClick(event, mouse); //Pass to server handler
  if (mouse.element.viewer.rotating) {
    mouse.element.viewer.rotating = false;
    //mouse.element.viewer.reload = true;
    sortTimer(false, mouse.element.viewer);
    return false;
  }

  mouse.element.viewer.draw();
  return false;
}

function sortTimer(ifexists, viewer) {
  if (viewer.vis.sortenabled == false) {
    //Sorting disabled
    viewer.rotated = false;
    viewer.draw();
    return;
  }
  if (viewer.vis.immediatesort == true) {
    //No timers
    viewer.rotated = true; 
    viewer.draw();
    return;
  }
  //Set a timer to apply the sort function in 2 seconds
  if (viewer.timer) {
    clearTimeout(viewer.timer);
  } else if (ifexists) {
    //No existing timer? Don't start a new one
    return;
  }

  viewer.timer = setTimeout(function() {viewer.rotated = true; viewer.draw();}, 50);
}

function canvasMouseDown(event, mouse) {
  //if (server) serverMouseDown(event, mouse); //Pass to server handler
  return false;
}

var hideTimer;

function canvasMouseMove(event, mouse) {
  //if (server) serverMouseMove(event, mouse); //Pass to server handler

  //GUI elements to show on mouseover
  if (mouse.element && mouse.element.viewer) {
    var gui = mouse.element.viewer.gui;
    var rect = mouse.element.getBoundingClientRect();
    x = event.clientX-rect.left;
    y = event.clientY-rect.top;
    if (x >= 0 && y >= 0 && x < rect.width && y < rect.height) {
      if (!gui && mouse.element.imgtarget)
        mouse.element.imgtarget.nextElementSibling.style.display = "block";

      if (gui) {
        if (mouse.element.imgtarget) mouse.element.imgtarget.nextElementSibling.style.display = "none";
        gui.domElement.style.display = "block";
      }

      if (hideTimer)
        clearTimeout(hideTimer);

      hideTimer = setTimeout(function () { hideMenu(mouse.element, gui);}, 1000 );
    }
  }

  if (!mouse.isdown || !mouse.element.viewer) return true;
  mouse.element.viewer.rotating = false;

  //Switch buttons for translate/rotate
  var button = mouse.button;
  if (mouse.element.viewer.mode == "Translate") {
    //Swap rotate/translate buttons
    if (button == 0)
      button = 2
    else if (button == 2)
      button = 0;
  } else if (button==0 && mouse.element.viewer.mode == "Zoom") {
      button = 100;
  }

  //console.log(mouse.deltaX + "," + mouse.deltaY);
  switch (button)
  {
    case 0:
      mouse.element.viewer.rotateY(mouse.deltaX/5);
      mouse.element.viewer.rotateX(mouse.deltaY/5);
      mouse.element.viewer.rotating = true;
      sortTimer(true, mouse.element.viewer);  //Delay sort if queued
      break;
    case 1:
      mouse.element.viewer.rotateZ(Math.sqrt(mouse.deltaX*mouse.deltaX + mouse.deltaY*mouse.deltaY)/5);
      mouse.element.viewer.rotating = true;
      sortTimer(true, mouse.element.viewer);  //Delay sort if queued
      break;
    case 2:
      var adjust = mouse.element.viewer.modelsize / 1000;   //1/1000th of size
      mouse.element.viewer.translate[0] += mouse.deltaX * adjust;
      mouse.element.viewer.translate[1] -= mouse.deltaY * adjust;
      break;
    case 100:
      var adjust = mouse.element.viewer.modelsize / 1000;   //1/1000th of size
      mouse.element.viewer.translate[2] += mouse.deltaX * adjust;
      break;
  }

  //Always draw border while interacting in server mode?
  if (server)
    mouse.element.viewer.draw(true);
  //Draw border while interacting (automatically on for models > 500K vertices)
  //Hold shift to switch from default behaviour
  else if (mouse.element.viewer.vis.interactive)
    mouse.element.viewer.draw();
  else
    mouse.element.viewer.draw(!event.shiftKey);
  return false;
}

var zoomTimer;
var zoomClipTimer;
var zoomSpin = 0;

function canvasMouseWheel(event, mouse) {
  if (event.shiftKey) {
    var factor = event.spin * 0.01;
    if (zoomClipTimer) clearTimeout(zoomClipTimer);
    zoomClipTimer = setTimeout(function () {mouse.element.viewer.zoomClip(factor);}, 100 );
  } else {
    if (zoomTimer) 
      clearTimeout(zoomTimer);
    zoomSpin += event.spin;
    zoomTimer = setTimeout(function () {mouse.element.viewer.zoom(zoomSpin*0.01); zoomSpin = 0;}, 100 );
    //Clear the box after a second
    //setTimeout(function() {mouse.element.viewer.clear();}, 1000);
  }
  return false; //Prevent default
}

function canvasMousePinch(event, mouse) {
  if (event.distance != 0) {
    var factor = event.distance * 0.0001;
    mouse.element.viewer.zoom(factor);
    //Clear the box after a second
    //setTimeout(function() {mouse.element.viewer.clear();}, 1000);
  }
  return false; //Prevent default
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

function loadColourMaps(vis) {
  //Load colourmaps
  if (!vis.colourmaps) return;

  var canvas = document.getElementById('palette');
  for (var i=0; i<vis.colourmaps.length; i++) {
    var palette = new Palette(vis.colourmaps[i].colours);
    vis.colourmaps[i].palette = palette;
    paletteLoad(palette);
  }

  //if (viewer) viewer.setColourMap(sel);
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
  var C = new Colour(colour);
  if (opacity < 1.0)
    C.alpha *= opacity;
  //Return final integer value
  return C.toInt();
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
   //DEBUG && console.log("Radix X sort: %d items %d bytes. Byte: ", N, size);
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

//This object encapsulates a vertex buffer and shader set
function Renderer(viewer, type, colour, border) {
  this.viewer = viewer;
  this.gl = viewer.gl;
  this.type = type;
  this.border = border;
  if (colour) this.colour = new Colour(colour);

  //Only two options for now, points and triangles
  if (type == "points") {
    //Particle renderer
    //Vertex3, Colour, Size, Type
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT,
                        Int32Array.BYTES_PER_ELEMENT,
                        Float32Array.BYTES_PER_ELEMENT,
                        Float32Array.BYTES_PER_ELEMENT];
  } else if (type == "triangles") {
    //Triangle renderer
    //Vertex3, Normal3, Colour, TexCoord2
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT,
                        3 * Float32Array.BYTES_PER_ELEMENT,
                        Int32Array.BYTES_PER_ELEMENT,
                        2 * Float32Array.BYTES_PER_ELEMENT];
  } else if (type == "lines") {
    //Line renderer
    //Vertex3, Colour
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT,
                        Int32Array.BYTES_PER_ELEMENT];
  } else if (type == "volume") {
    //Volume renderer
    //Vertex3
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT];
  }

  this.elementSize = 0;
  for (var i=0; i<this.attribSizes.length; i++)
    this.elementSize += this.attribSizes[i];
}

Renderer.prototype.init = function() {
  if (this.type == "triangles" && !this.viewer.hasTriangles) return false;
  if (this.type == "points" && !this.viewer.hasPoints) return false;
  var fs = this.type + '-fs';
  var vs = this.type + '-vs';
  var vis = this.viewer.vis;

  //User defined shaders if provided...
  if (vis.shaders) {
    if (vis.shaders[this.type]) {
      fs = vis.shaders[this.type].fragment || fs;
      vs = vis.shaders[this.type].vertex || vs;
    }
  }

  var fdefines = "#extension GL_OES_standard_derivatives : enable\nprecision highp float;\n#define WEBGL\n";
  var vdefines = "precision highp float;\n#define WEBGL\n";

  if (this.type == "volume" && this.id && this.image) {
    //Setup two-triangle rendering
    this.viewer.webgl.init2dBuffers(this.gl.TEXTURE1); //Use 2nd texture unit

    //Override texture params set in previous call
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);

    //Load the volume texture image
    this.viewer.webgl.loadTexture(this.image, this.gl.LINEAR); //this.gl.LUMINANCE, true

    //Calculated scaling
    this.res = vis.objects[this.id].volume["res"];
    this.scaling = vis.objects[this.id].volume["scale"];
    /*/Auto compensate for differences in resolution..
    if (vis.objects[this.id].volume.autoscale) {
      //Divide all by the highest res
      var maxn = Math.max.apply(null, this.res);
      this.scaling = [this.res[0] / maxn * vis.objects[this.id].volume["scale"][0], 
                      this.res[1] / maxn * vis.objects[this.id].volume["scale"][1],
                      this.res[2] / maxn * vis.objects[this.id].volume["scale"][2]];
    }*/
    this.tiles = [this.image.width / this.res[0],
                  this.image.height / this.res[1]];
    this.iscale = [1.0 / this.scaling[0], 1.0 / this.scaling[1], 1.0 / this.scaling[2]]
      
    var maxSamples = 1024; //interactive ? 1024 : 256;
    fdefines += "const highp vec2 slices = vec2(" + this.tiles[0] + "," + this.tiles[1] + ");\n";
    fdefines += "const int maxSamples = " + maxSamples + ";\n";

    if (vis.objects[this.id].tricubicfilter)
      fdefines += "#define ENABLE_TRICUBIC\n";
  }

  //vs = vdefines + getSourceFromElement(vs).replace(/^out /gm, 'varying ').replace(/^in /gm, 'attribute ');
  //fs = fdefines + getSourceFromElement(fs).replace(/^in /gm, 'varying ');
  vs = vdefines + shaders[vs].replace(/^in /gm, 'attribute ').replace(/^out /gm, 'varying ');
  fs = fdefines + shaders[fs].replace(/^in /gm, 'varying ');

  try {
    //Compile the shaders
    this.program = new WebGLProgram(this.gl, vs, fs);
    if (this.program.errors) DEBUG && console.log(this.program.errors);
    //Setup attribs/uniforms (flag set to skip enabling attribs)
    this.program.setup(undefined, undefined, true);
  } catch(e) {
    console.log("FRAGMENT SHADER:\n" + fs);
    console.log("VERTEX SHADER:\n" + vs);
    console.error(e);
    return false;
  }
  return true;
}

function SortIdx(idx, key) {
  this.idx = idx;
  this.key = key;
}

Renderer.prototype.loadElements = function() {
  if (this.border) return;
  DEBUG && console.log("Loading " + this.type + " elements...");
  var start = new Date();
  var distances = [];
  var indices = [];
  var vis = this.viewer.vis;
  //Only update the positions array when sorting due to update
  if (!this.positions || !this.viewer.rotated || this.type == 'lines') {
    this.positions = [];
    //Add visible element positions
    for (var id in vis.objects) {
      var name = vis.objects[id].name;
      var skip = !vis.objects[id].visible;

      if (this.type == "points") {
        if (vis.objects[id].points) {
          for (var e in vis.objects[id].points) {
            var pdat = vis.objects[id].points[e];
            var count = pdat.vertices.data.length;
            //DEBUG && console.log(name + " " + skip + " : " + count);
            for (var i=0; i<count; i += 3)
              this.positions.push(skip ? null : [pdat.vertices.data[i], pdat.vertices.data[i+1], pdat.vertices.data[i+2]]);
          }
        }
      } else if (this.type == "triangles") {
        if (vis.objects[id].triangles) {
          for (var e in vis.objects[id].triangles) {
            var tdat =  vis.objects[id].triangles[e];
            var count = tdat.indices.data.length/3;

            //console.log(name + " " + skip + " : " + count + " - " + tdat.centroids.length);
            for (var i=0; i<count; i++) {
              //this.positions.push(skip ? null : tdat.centroids[i]);
              if (skip || !tdat.centroids)
                this.positions.push(null);
              else if (tdat.centroids.length == 1)
                this.positions.push(tdat.centroids[0]);
              else
                this.positions.push(tdat.centroids[i]);
            }
          }
        }
      } else if (this.type == "lines") {
        //Write lines directly to indices buffer, no depth sort necessary
        if (skip) continue;
        if (vis.objects[id].lines) {
          for (var e in vis.objects[id].lines) {
            var ldat =  vis.objects[id].lines[e];
            var count = ldat.indices.data.length;
            //DEBUG && console.log(name + " " + skip + " : " + count);
            for (var i=0; i<count; i++)
              indices.push(ldat.indices.data[i]);
          }
        }
      }
    }

    var time = (new Date() - start) / 1000.0;
    DEBUG && console.log(this.type + " : " + time + " seconds to update positions " + this.positions.length);
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
    var M2 = [this.viewer.webgl.modelView.matrix[2],
              this.viewer.webgl.modelView.matrix[6],
              this.viewer.webgl.modelView.matrix[10],
              this.viewer.webgl.modelView.matrix[14]];

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
    DEBUG && console.log(this.type + " : " + time + " seconds to update distances " + distances.length);

    if (distances.length > 0) {
      //Sort
      start = new Date();
      //distances.sort(function(a,b){return a.key - b.key});
      //This is about 10 times faster than above:
      if (this.viewer.view.is3d)
        msb_radix_sort(distances, 0, distances.length, 16);
      //Pretty sure msb is still fastest...
      //if (!this.swap) this.swap = [];
      //radix_sort(distances, this.swap, 2);
      time = (new Date() - start) / 1000.0;
      DEBUG && console.log(time + " seconds to sort");

      start = new Date();
      //Reload index buffer
      if (this.type == "points") {
        //Process points
        for (var i = 0; i < distances.length; ++i)
          indices.push(distances[i].idx);
          //if (distances[i].idx > this.elements) alert("ERROR: " + i + " - " + distances[i].idx + " > " + this.elements);
      } else if (this.type == "triangles") {
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
      DEBUG && console.log(time + " seconds to load index buffers");
    }
  }

  start = new Date();
  if (indices.length > 0) {
    //this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(indices), this.gl.STATIC_DRAW);
    this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(indices), this.gl.DYNAMIC_DRAW);
    //this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), this.gl.DYNAMIC_DRAW);

    time = (new Date() - start) / 1000.0;
    DEBUG && console.log(this.type + " : " + time + " seconds to update index buffer object " + indices.length);
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
  DEBUG && console.log(elements + " - " + size);
  DEBUG && console.log("Created vertex buffer");
}

VertexBuffer.prototype.loadPoints = function(object, viewer) {
  for (var p in object.points) {
    var pdat =  object.points[p];

    /*console.log("loadPoints " + p);
    if (pdat.values)
      console.log(object.name + " : " + pdat.values.minimum + " -> " + pdat.values.maximum);
    if (object.colourmap >= 0)
      console.log(object.name + " :: " + vis.colourmaps[object.colourmap].range[0] + " -> " + vis.colourmaps[object.colourmap].range[1]);
    else
      console.log(object.colourmap);*/

    var map = viewer.lookupMap(object.colourmap);

    var psize = object.pointsize ? object.pointsize : viewer.vis.properties.pointsize;
    if (!psize) psize = 1.0;
    psize = psize * (object.scaling || 1.0);
    //console.log("POINTSIZE: " + psize);

    for (var i=0; i<pdat.vertices.data.length/3; i++) {
      var i3 = i*3;
      var vert = [pdat.vertices.data[i3], pdat.vertices.data[i3+1], pdat.vertices.data[i3+2]];
      this.floats[this.offset] = vert[0];
      this.floats[this.offset+1] = vert[1];
      this.floats[this.offset+2] = vert[2];
      this.ints[this.offset+3] = vertexColour(object.colour, object.opacity, map, pdat, i)
      this.floats[this.offset+4] = pdat.sizes ? pdat.sizes.data[i] * psize : psize;
      this.floats[this.offset+5] = object.pointtype >= 0 ? object.pointtype : -1;
      this.offset += this.vertexSizeInFloats;
    }
  }
}

VertexBuffer.prototype.loadTriangles = function(object, id, viewer) {
  //Process triangles
  if (!this.byteOffset) this.byteOffset = 9 * Float32Array.BYTES_PER_ELEMENT;
  var T = 0;
  if (object.wireframe) T = 1;
  for (var t in object.triangles) {
    var tdat =  object.triangles[t];
    var calcCentroids = false;
    if (!tdat.centroids) {
      calcCentroids = true;
      tdat.centroids = [];
    }

    //if (tdat.values)
    //  console.log(object.name + " : " + tdat.values.minimum + " -> " + tdat.values.maximum);
    //  console.log("OPACITY: " + object.name + " : " + object.opacity);
    //if (object.colourmap >= 0)
    //  console.log(object.name + " :: " + viewer.vis.colourmaps[object.colourmap].range[0] + " -> " + viewer.vis.colourmaps[object.colourmap].range[1]);

    var map = viewer.lookupMap(object.colourmap);

    for (var i=0; i<tdat.indices.data.length/3; i++) {
      if (i%10000 == 0) console.log(i);
      //Indices holds references to vertices and other data
      var i3 = i * 3;
      var ids = [tdat.indices.data[i3], tdat.indices.data[i3+1], tdat.indices.data[i3+2]];
      var ids3 = [ids[0]*3, ids[1]*3, ids[2]*3];

      for (var j=0; j<3; j++) {
        this.floats[this.offset] = tdat.vertices.data[ids3[j]];
        this.floats[this.offset+1] = tdat.vertices.data[ids3[j]+1];
        this.floats[this.offset+2] = tdat.vertices.data[ids3[j]+2];
        if (tdat.normals && tdat.normals.data.length == 3) {
          //Single surface normal
          this.floats[this.offset+3] = tdat.normals.data[0];
          this.floats[this.offset+4] = tdat.normals.data[1];
          this.floats[this.offset+5] = tdat.normals.data[2];
        } else if (tdat.normals) {
          this.floats[this.offset+3] = tdat.normals.data[ids3[j]];
          this.floats[this.offset+4] = tdat.normals.data[ids3[j]+1];
          this.floats[this.offset+5] = tdat.normals.data[ids3[j]+2];
        } else {
          this.floats[this.offset+3] = 0.0;
          this.floats[this.offset+4] = 0.0;
          this.floats[this.offset+5] = 0.0;
        }
        if (!object.tex)
          this.ints[this.offset+6] = vertexColour(object.colour, object.opacity, map, tdat, ids[j])
        else {
          if (tdat.texcoords) {
            this.floats[this.offset+7] = tdat.texcoords.data[ids[j]*2];
            this.floats[this.offset+8] = tdat.texcoords.data[ids[j]*2+1];
          } else {
            this.floats[this.offset+7] = texcoords[(i%2+1)*T][j][0];
            this.floats[this.offset+8] = texcoords[(i%2+1)*T][j][1];
          }
        }
        this.offset += this.vertexSizeInFloats;
        this.byteOffset += this.size;
      }

      //Calc centroids (only required if vertices changed)
      if (calcCentroids) {
        if (tdat.width) //indices.data.length == 6)
          //Cross-sections, null centroid - always drawn last
          tdat.centroids.push([]);
        else {
          //(x1+x2+x3, y1+y2+y3, z1+z2+z3)
          var verts = tdat.vertices.data;
          /*/Side lengths: A-B, A-C, B-C
          var AB = vec3.createFrom(verts[ids3[0]] - verts[ids3[1]], verts[ids3[0] + 1] - verts[ids3[1] + 1], verts[ids3[0] + 2] - verts[ids3[1] + 2]);
          var AC = vec3.createFrom(verts[ids3[0]] - verts[ids3[2]], verts[ids3[0] + 1] - verts[ids3[2] + 1], verts[ids3[0] + 2] - verts[ids3[2] + 2]);
          var BC = vec3.createFrom(verts[ids3[1]] - verts[ids3[2]], verts[ids3[1] + 1] - verts[ids3[2] + 1], verts[ids3[1] + 2] - verts[ids3[2] + 2]);
          var lengths = [vec3.length(AB), vec3.length(AC), vec3.length(BC)];
                //Size weighting shift
                var adj = (lengths[0] + lengths[1] + lengths[2]) / 9.0;
                //if (i%100==0) console.log(verts[ids3[0]] + "," + verts[ids3[0] + 1] + "," + verts[ids3[0] + 2] + " " + adj);*/
          tdat.centroids.push([(verts[ids3[0]]    + verts[ids3[1]]     + verts[ids3[2]])     / 3,
                              (verts[ids3[0] + 1] + verts[ids3[1] + 1] + verts[ids3[2] + 1]) / 3,
                              (verts[ids3[0] + 2] + verts[ids3[1] + 2] + verts[ids3[2] + 2]) / 3]);
        }
      }
    }
  }
}

VertexBuffer.prototype.loadLines = function(object) {
  for (var l in object.lines) {
    var ldat =  object.lines[l];
    var map = viewer.lookupMap(object.colourmap);
    for (var i=0; i<ldat.vertices.data.length/3; i++) {
      var i3 = i*3;
      var vert = [ldat.vertices.data[i3], ldat.vertices.data[i3+1], ldat.vertices.data[i3+2]];
      this.floats[this.offset] = vert[0];
      this.floats[this.offset+1] = vert[1];
      this.floats[this.offset+2] = vert[2];
      this.ints[this.offset+3] = vertexColour(object.colour, object.opacity, map, ldat, i)
      this.offset += this.vertexSizeInFloats;
    }
  }
}

VertexBuffer.prototype.update = function(gl) {
  start = new Date();
  //gl.bufferData(gl.ARRAY_BUFFER, this.array, gl.STATIC_DRAW);
  gl.bufferData(gl.ARRAY_BUFFER, this.array, gl.DYNAMIC_DRAW);
  //gl.bufferData(gl.ARRAY_BUFFER, this.vertices * this.elementSize, gl.DYNAMIC_DRAW);
  //gl.bufferSubData(gl.ARRAY_BUFFER, 0, buffer);

  time = (new Date() - start) / 1000.0;
  DEBUG && console.log(time + " seconds to update vertex buffer object");
}

Renderer.prototype.updateBuffers = function() {
  var vis = viewer.vis;
  if (this.border) {
    this.box(viewer.view.min, viewer.view.max);
    this.elements = 24;
    return;
  }

  //Count vertices
  this.elements = 0;
  for (var id in vis.objects) {
    if (this.type == "triangles" && vis.objects[id].triangles) {
      for (var t in vis.objects[id].triangles)
        this.elements += vis.objects[id].triangles[t].indices.data.length;
    } else if (this.type == "points" && vis.objects[id].points) {
      for (var t in vis.objects[id].points)
        this.elements += vis.objects[id].points[t].vertices.data.length/3;
    } else if (this.type == "lines" && vis.objects[id].lines) {
      for (var t in vis.objects[id].lines)
        this.elements += vis.objects[id].lines[t].indices.data.length;
    }
  }

  if (this.elements == 0) return;
  DEBUG && console.log("Updating " + this.type + " data... (" + this.elements + " elements)");
  console.log("Updating " + this.type + " data... (" + this.elements + " elements)");

  //Load vertices and attributes into buffers
  var start = new Date();

  //Copy data to VBOs
  var buffer = new VertexBuffer(this.elements, this.elementSize);

  //Reload vertex buffers
  if (this.type == "points") {
    //Process points

    /*/Auto 50% subsample when > 1M particles
    var subsample = 1;
    if (document.getElementById("subsample").checked == true && newParticles > 1000000) {
      subsample = Math.round(newParticles/1000000 + 0.5);
      DEBUG && console.log("Subsampling at " + (100/subsample) + "% (" + subsample + ") to " + Math.floor(newParticles / subsample));
    }*/
    //Random subsampling
    //if (subsample > 1 && Math.random() > 1.0/subsample) continue;

    for (var id in vis.objects)
      if (vis.objects[id].points)
        buffer.loadPoints(vis.objects[id], this.viewer);

  } else if (this.type == "lines") {
    //Process lines
    for (var id in vis.objects)
      if (vis.objects[id].lines)
        buffer.loadLines(vis.objects[id]);

  } else if (this.type == "triangles") {
    //Process triangles
    for (var id in vis.objects)
      if (vis.objects[id].triangles)
        buffer.loadTriangles(vis.objects[id], id, this.viewer);
  }

  var time = (new Date() - start) / 1000.0;
  DEBUG && console.log(time + " seconds to load buffers... (elements: " + this.elements + " bytes: " + buffer.bytes.length + ")");
  console.log(time + " seconds to load buffers... (elements: " + this.elements + " bytes: " + buffer.bytes.length + ")");

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

Renderer.prototype.getglobal = function(prop) {
//Lookup global prop, then use default if not found
  if (this.viewer.vis.properties[prop] != undefined)
    return this.viewer.vis.properties[prop];
  return this.viewer.dict[prop].default;
}

Renderer.prototype.getprop = function(prop, id) {
//Lookup prop on object (or renderer if not set) first, then global, then use default
  //console.log(prop + " getprop " + this.id);
  var object = this;
  if (id == undefined) id = this.id;
  if (id != undefined) object = this.viewer.vis.objects[id];
  if (object[prop] != undefined)
    return object[prop];
  return this.getglobal(prop);
}

Renderer.prototype.draw = function() {
  var vis = this.viewer.vis;
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
    DEBUG && console.log("Creating " + this.type + " buffers...");
    this.vertexBuffer = this.gl.createBuffer();
    this.indexBuffer = this.gl.createBuffer();
    //this.viewer.reload = true;
  }

  if (this.program.attributes["aVertexPosition"] == undefined) return; //Require vertex buffer

  this.viewer.webgl.use(this.program);
  this.viewer.webgl.setMatrices();

  //Bind buffers
  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

  //Reload geometry if required
  this.viewer.canvas.mouse.disabled = true;
  if (this.reload) this.updateBuffers();
  if (this.sort || this.viewer.rotated) this.loadElements();
  this.reload = this.sort = false;
  this.viewer.canvas.mouse.disabled = false;

  if (this.elements == 0 && this.type != "volume") return;

  //Enable attributes
  for (var key in this.program.attributes)
    this.gl.enableVertexAttribArray(this.program.attributes[key]);

  //General uniform vars
  //TODO: fix this? can't us getprop() as draw is not per object - support per object renderers as in library?
  this.gl.uniform1i(this.program.uniforms["uLighting"], this.getglobal('lit'));
  this.gl.uniform1f(this.program.uniforms["uOpacity"], this.getglobal('opacity')); //Global opacity
  this.gl.uniform1f(this.program.uniforms["uBrightness"], this.getglobal('brightness'));
  this.gl.uniform1f(this.program.uniforms["uContrast"], this.getglobal('contrast'));
  this.gl.uniform1f(this.program.uniforms["uAmbient"], this.getglobal('ambient'));
  this.gl.uniform1f(this.program.uniforms["uSaturation"], this.getglobal('saturation'));
  this.gl.uniform1f(this.program.uniforms["uAmbient"], this.getglobal('ambient'));
  this.gl.uniform1f(this.program.uniforms["uDiffuse"], this.getglobal('diffuse'));
  this.gl.uniform1f(this.program.uniforms["uSpecular"], this.getglobal('specular'));
  this.gl.uniform1f(this.program.uniforms["uShininess"], this.getglobal('shininess'));
  this.gl.uniform4fv(this.program.uniforms["uLightPos"], new Float32Array(this.getglobal('lightpos')));
  this.gl.uniform4fv(this.program.uniforms["uLight"], new Float32Array(this.getglobal('light')));
  var colour = this.getglobal('colour');
  //if (this.colour)
  if (colour)
    this.gl.uniform4f(this.program.uniforms["uColour"], colour.red/255.0, colour.green/255.0, colour.blue/255.0, colour.alpha);

  var cmin = [vis.properties.xmin || -Infinity,
              vis.properties.ymin || -Infinity,
              this.viewer.view.is3d ? (vis.properties.zmin || -Infinity) : -Infinity];
  var cmax = [vis.properties.xmax || Infinity,
              vis.properties.ymax || Infinity,
              this.viewer.view.is3d ? (vis.properties.zmax || Infinity) : Infinity];

  if (vis.properties.clipmap == undefined || vis.properties.clipmap) {
    cmin = [this.viewer.view.min[0] + this.viewer.dims[0] * cmin[0],
            this.viewer.view.min[1] + this.viewer.dims[1] * cmin[1],
            this.viewer.view.min[2] + this.viewer.dims[2] * cmin[2]];
    cmax = [this.viewer.view.min[0] + this.viewer.dims[0] * cmax[0],
            this.viewer.view.min[1] + this.viewer.dims[1] * cmax[1],
            this.viewer.view.min[2] + this.viewer.dims[2] * cmax[2]];
  }
  this.gl.uniform3fv(this.program.uniforms["uClipMin"], new Float32Array(cmin));
  this.gl.uniform3fv(this.program.uniforms["uClipMax"], new Float32Array(cmax));

  if (this.type == "points") {

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, this.elementSize, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, this.elementSize, this.attribSizes[0]);
    this.gl.vertexAttribPointer(this.program.attributes["aSize"], 1, this.gl.FLOAT, false, this.elementSize, this.attribSizes[0]+this.attribSizes[1]);
    this.gl.vertexAttribPointer(this.program.attributes["aPointType"], 1, this.gl.FLOAT, false, this.elementSize, this.attribSizes[0]+this.attribSizes[1]+this.attribSizes[2]);

    //Set uniforms...
    this.gl.uniform1i(this.program.uniforms["uPointType"], this.viewer.pointType >= 0 ? this.viewer.pointType : 1);
    this.gl.uniform1f(this.program.uniforms["uPointScale"], this.viewer.pointScale*this.viewer.modelsize);

    //Draw points
    this.gl.drawElements(this.gl.POINTS, this.elements, this.gl.UNSIGNED_INT, 0);
    //this.gl.drawElements(this.gl.POINTS, this.positions.length, this.gl.UNSIGNED_SHORT, 0);
    //this.gl.drawArrays(this.gl.POINTS, 0, this.positions.length);
    desc = this.elements + " points";

  } else if (this.type == "triangles") {

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, this.elementSize, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexNormal"], 3, this.gl.FLOAT, false, this.elementSize, this.attribSizes[0]);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, this.elementSize, this.attribSizes[0]+this.attribSizes[1]);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexTexCoord"], 2, this.gl.FLOAT, true, this.elementSize, this.attribSizes[0]+this.attribSizes[1]+this.attribSizes[2]);

    //Set uniforms...
    //this.gl.enable(this.gl.CULL_FACE);
    //this.gl.cullFace(this.gl.BACK_FACE);
    
    //Texture -- TODO: Switch per object!
    //this.gl.bindTexture(this.gl.TEXTURE_2D, this.viewer.webgl.textures[0]);
    if (vis.objects[0].tex) {
      this.gl.activeTexture(this.gl.TEXTURE0);
      this.gl.bindTexture(this.gl.TEXTURE_2D, vis.objects[0].tex);
      this.gl.uniform1i(this.program.uniforms["uTexture"], 0);
      this.gl.uniform1i(this.program.uniforms["uTextured"], 1); //Disabled unless texture attached!
    } else {
      this.gl.uniform1i(this.program.uniforms["uTextured"], 0); //Disabled unless texture attached!
    }

    this.gl.uniform1i(this.program.uniforms["uCalcNormal"], 0);

    //Draw triangles
    this.gl.drawElements(this.gl.TRIANGLES, this.elements, this.gl.UNSIGNED_INT, 0);
    //this.gl.drawElements(this.gl.TRIANGLES, this.positions.length*3, this.gl.UNSIGNED_SHORT, 0);
    //this.gl.drawArrays(this.gl.TRIANGLES, 0, this.positions.length*3);
    desc = (this.elements / 3) + " triangles";

  } else if (this.border) {
    this.gl.lineWidth(vis.properties.border >= 0 ? vis.properties.border : 1.0);

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, 0, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, 0, 0);
    this.gl.drawElements(this.gl.LINES, this.elements, this.gl.UNSIGNED_SHORT, 0);
    desc = "border";

  } else if (this.type == "lines") {

    //Default line width (TODO: per object settings, using first object only here)
    this.gl.lineWidth(vis.objects[0].linewidth || 1.0);

    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, this.elementSize, 0);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexColour"], 4, this.gl.UNSIGNED_BYTE, true, this.elementSize, this.attribSizes[0]);

    //Draw lines
    this.gl.drawElements(this.gl.LINES, this.elements, this.gl.UNSIGNED_INT, 0);
    desc = (this.elements / 2) + " lines";
    //this.gl.drawArrays(this.gl.LINES, 0, this.positions.length);

  } else if (this.type == "volume") {
    
    //Volume render
    this.viewer.webgl.initDraw2d(); //Prep for two-triangle render

    //Premultiplied alpha blending
    this.gl.blendFunc(this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);

    if (this.viewer.gradient.mapid != vis.objects[this.id].colourmap) {
      //Map selected has changed, so update texture (used on initial render only)
      var obj = {};
      obj.viewer = this.viewer;
      paletteUpdate(obj, vis.objects[this.id].colourmap);
    }

    //Setup volume camera
    this.viewer.webgl.modelView.push();
    this.viewer.webgl.projection.push();
    {
      var props = vis.objects[this.id];
      var scaling = vis.objects[this.id].volume["scale"];
      /*/Apply translation to origin, any rotation and scaling
      this.viewer.webgl.modelView.identity()
      this.viewer.webgl.modelView.translate(this.viewer.translate)

      // rotate model 
      var rotmat = quat4.toMat4(this.viewer.rotate);
      this.viewer.webgl.modelView.mult(rotmat);*/

      //TODO: get object rotation & translation and apply them (pass to apply?)
      //Apply the default camera
      //this.viewer.webgl.apply(viewer, rot, trans);

      //console.log("DIMS: " + (scaling) + " TRANS: " + [-scaling[0]*0.5, -scaling[1]*0.5, -scaling[2]*0.5] + " SCALE: " + [1.0/scaling[0], 1.0/scaling[1], 1.0/scaling[2]]);
      //For a volume cube other than [0,0,0] - [1,1,1], need to translate/scale here...
      //this.viewer.webgl.modelView.translate([-scaling[0]*0.5, -scaling[1]*0.5, -scaling[2]*0.5]);  //Translate to origin
      //this.viewer.webgl.modelView.translate([-0.5, -0.5, -0.5]);  //Translate to origin
      
      //Apply any corner offset translation here
      var minv = vis.objects[this.id].min;
      this.viewer.webgl.modelView.translate(minv);  //Translate to origin
      //this.viewer.webgl.modelView.translate([-minv[0], -minv[1], -minv[2]]);  //Translate to origin

      //Get the mvMatrix scaled by volume size
      //(used for depth calculations)
      var matrix = mat4.create(this.viewer.webgl.modelView.matrix);
      mat4.scale(matrix, [scaling[0], scaling[1], scaling[2]]);

      //Inverse of scaling
      this.viewer.webgl.modelView.scale([this.iscale[0], this.iscale[1], this.iscale[2]]);

      //Send the normal matrix now
      this.viewer.webgl.setNormalMatrix();

      //Perspective matrix
      //this.viewer.webgl.setPerspective(this.viewer.fov, this.viewer.webgl.viewport.width / this.viewer.webgl.viewport.height, this.viewer.near_clip, this.viewer.far_clip);
      //Apply model scaling, inverse squared
      this.viewer.webgl.modelView.scale([1.0/(this.viewer.scale[0]*this.viewer.scale[0]), 1.0/(this.viewer.scale[1]*this.viewer.scale[1]), 1.0/(this.viewer.scale[2]*this.viewer.scale[2])]);

      //Send default matrices now
      //this.viewer.webgl.setMatrices();  //(can't use this as will overwrite our normal matrix)
      //Model view matrix
      this.gl.uniformMatrix4fv(this.viewer.webgl.program.mvMatrixUniform, false, this.viewer.webgl.modelView.matrix);
      //Perspective projection matrix
      this.gl.uniformMatrix4fv(this.viewer.webgl.program.pMatrixUniform, false, this.viewer.webgl.projection.matrix);

      //Combined ModelView * Projection
      var MVPMatrix = mat4.create(this.viewer.webgl.projection.matrix);
      mat4.multiply(MVPMatrix, matrix);

      //Transpose of model view
      var tMVMatrix = mat4.create(this.viewer.webgl.modelView.matrix);
      mat4.transpose(tMVMatrix);

      //Combined InverseProjection * InverseModelView
      var invPMatrix = mat4.create(this.viewer.webgl.projection.matrix);
      mat4.inverse(invPMatrix);
      var invMVPMatrix = mat4.create(this.viewer.webgl.modelView.matrix);
      mat4.transpose(invMVPMatrix);
      mat4.multiply(invMVPMatrix, invPMatrix);
    }
    this.viewer.webgl.modelView.pop();

    this.gl.activeTexture(this.gl.TEXTURE0);
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.viewer.webgl.textures[0]);

    this.gl.activeTexture(this.gl.TEXTURE1);
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.viewer.webgl.gradientTexture);

    //Only render full quality when not interacting
    //this.gl.uniform1i(this.program.uniforms["uSamples"], this.samples);
    //TODO: better default handling here - get default values from the properties dict, needs to be exported to a json file
    this.gl.uniform1i(this.program.uniforms["uSamples"], props.samples || 256);
    this.gl.uniform1i(this.program.uniforms["uVolume"], 0);
    this.gl.uniform1i(this.program.uniforms["uTransferFunction"], 1);
    this.gl.uniform1i(this.program.uniforms["uEnableColour"], props.usecolourmap || 1);
    this.gl.uniform1i(this.program.uniforms["uFilter"], props.tricubicfilter || 0);
    this.gl.uniform4fv(this.program.uniforms["uViewport"], this.viewer.webgl.viewport.array);

    var bbmin = [props.xmin || 0.0, props.ymin || 0.0, props.zmin || 0.0];
    var bbmax = [props.xmax || 1.0, props.ymax || 1.0, props.zmax || 1.0];
    this.gl.uniform3fv(this.program.uniforms["uBBMin"], new Float32Array(bbmin));
    this.gl.uniform3fv(this.program.uniforms["uBBMax"], new Float32Array(bbmax));
    this.gl.uniform3fv(this.program.uniforms["uResolution"], new Float32Array(vis.objects[this.id].volume["res"]));

    this.gl.uniform1f(this.program.uniforms["uDensityFactor"], (props.density != undefined ? props.density : 5));
    //Per object settings
    this.gl.uniform1f(this.program.uniforms["uOpacity"], this.getprop('opacity')); //TODO: fix per object settings
    // brightness and contrast
    this.gl.uniform1f(this.program.uniforms["uSaturation"], props.saturation || 1.0);
    this.gl.uniform1f(this.program.uniforms["uBrightness"], props.brightness || 0.0);
    this.gl.uniform1f(this.program.uniforms["uContrast"], props.contrast || 1.0);
    this.gl.uniform1f(this.program.uniforms["uPower"], props.power || 1.0);
    this.gl.uniform1f(this.program.uniforms["uBloom"], props.bloom || 0.0);
    //Density clip range
    var dclip = [vis.objects[this.id].minclip || 0.0, vis.objects[this.id].maxclip || 1.0];
    this.gl.uniform2fv(this.program.uniforms["uDenMinMax"], new Float32Array(dclip));

    //Data value range for isoValue etc
    var range = new Float32Array([0.0, 1.0]);
    var isovalue = props.isovalue;
    var isoalpha = props.isoalpha;
    if (isoalpha == undefined) isoalpha = 1.0;
    if (props.isovalue != undefined) {
      if (vis.objects[this.id]["volume"].minimum != undefined && vis.objects[this.id]["volume"].maximum != undefined) {
        var r = [vis.objects[this.id]["volume"].minimum, vis.objects[this.id]["volume"].maximum];
        //Normalise isovalue to range [0,1] to match data (non-float textures always used in WebGL)
        isovalue = (isovalue - r[0]) / (r[1] - r[0]);
        //This is used in shader to normalize data to [0,1] when using float textures
        //range[0] = r[0];
        //range[1] = r[1];
      }
    } else {
      isoalpha = 0.0;
    }

    this.gl.uniform2fv(this.program.uniforms["uRange"], range);
    this.gl.uniform1f(this.program.uniforms["uIsoValue"], isovalue);

    var colour = new Colour(props.colour);
    colour.alpha = isoalpha;
    this.gl.uniform4fv(this.program.uniforms["uIsoColour"], colour.rgbaGL());

    this.gl.uniform1f(this.program.uniforms["uIsoSmooth"], props.isosmooth || 0.1);
    this.gl.uniform1i(this.program.uniforms["uIsoWalls"], props.isowalls != undefined ? props.isowalls : 1.0);

    //Draw two triangles
    this.gl.uniformMatrix4fv(this.program.uniforms["uTMVMatrix"], false, tMVMatrix);
    this.gl.uniformMatrix4fv(this.program.uniforms["uInvMVPMatrix"], false, invMVPMatrix);
    this.gl.uniformMatrix4fv(this.program.uniforms["uMVPMatrix"], false, MVPMatrix);
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, this.viewer.webgl.vertexPositionBuffer.numItems);

    //Restore default blending
    this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
  }

  //Disable attribs
  for (var key in this.program.attributes)
    this.gl.disableVertexAttribArray(this.program.attributes[key]);

  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
  this.gl.useProgram(null);

  var time = (new Date() - start) / 1000.0;
  if (time > 0.01) DEBUG && console.log(time + " seconds to draw " + desc);
}

function minMaxDist()
{
  //Calculate min/max distances from view plane
  var M2 = [this.viewer.webgl.modelView.matrix[0*4+2],
            this.viewer.webgl.modelView.matrix[1*4+2],
            this.viewer.webgl.modelView.matrix[2*4+2],
            this.viewer.webgl.modelView.matrix[3*4+2]];
  var maxdist = -Number.MAX_VALUE, mindist = Number.MAX_VALUE;
  for (i=0; i<2; i++)
  {
     var x = i==0 ? this.viewer.view.min[0] : this.viewer.view.max[0];
     for (var j=0; j<2; j++)
     {
        var y = j==0 ? this.viewer.view.min[1] : this.viewer.view.max[1];
        for (var k=0; k<2; k++)
        {
           var z = k==0 ? this.viewer.view.min[2] : this.viewer.view.max[2];
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
  this.vis = {};
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
      DEBUG && console.log(e);
      if (!this.webgl) document.getElementById('canvas').style.display = 'none';
    }
  }

  //Default colour editor
  this.gradient = new GradientEditor(document.getElementById('palette'), paletteUpdate);
  this.gradient.viewer = this;

  this.width = 0; //Auto resize
  this.height = 0;

  this.rotating = false;
  this.translate = [0,0,0];
  this.rotate = quat4.create();
  quat4.identity(this.rotate);
  this.focus = [0,0,0];
  this.centre = [0,0,0];
  this.fov = 45.0;
  this.near_clip = this.far_clip = 0.0;
  this.modelsize = 1;
  this.scale = [1, 1, 1];
  this.orientation = 1.0; //1.0 for RH, -1.0 for LH
  this.background = new Colour(0xff404040);
  this.canvas.parentElement.style.background = this.background.html();
  this.pointScale = 1.0;
  this.pointType = 0;

  //Non-persistant settings
  this.border = 0;
  this.mode = 'Rotate';

  //Create the renderers
  this.renderers = [];
  if (this.gl) {
    this.points = new Renderer(this, 'points');
    this.triangles = new Renderer(this, 'triangles');
    this.lines = new Renderer(this, 'lines');
    this.border = new Renderer(this, 'lines', 0xffffffff, true);

    //Defines draw order
    this.renderers.push(this.triangles);
    this.renderers.push(this.points);
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
    return;
  }
  var time = (new Date() - start) / 1000.0;
  DEBUG && console.log(time + " seconds to parse data");

  if (source.exported) {
    if (!this.vis.views && !this.view) {DEBUG && console.log("Exported settings require loaded model"); return;}
    var old = this.toString();
    //Copy, overwriting if exists in source
    if (source.views[0].rotate) this.vis.views[0].rotate = source.views[0].rotate;
    //Merge keys - preserves original objects for gui access
    Merge(this.vis, source);
    if (!source.reload) updated = false;  //No reload necessary
  } else {
    //Clear geometry
    for (var r in this.renderers)
      this.renderers[r].elements = 0;

    //Replace
    //Merge keys - preserves original objects for gui access
    Merge(this.vis, source);
    //this.vis = source;

    if (!this.vis.colourmaps) this.vis.colourmaps = [];
  }

  var vis = this.vis;
  if (!vis.properties) vis.properties = {}; //Ensure valid object even if no properties

  //Set active view (always first for now)
  this.view = vis.views[0];

  //Always set a bounding box, get from objects if not in view
  var objbb = false;
  if (!this.view.min || !this.view.max) {
    this.view.min = [Number.MAX_VALUE, Number.MAX_VALUE, Number.MAX_VALUE];
    this.view.max = [-Number.MAX_VALUE, -Number.MAX_VALUE, -Number.MAX_VALUE];
    objbb = true;
  }
  //console.log(JSON.stringify(this.view));

  //Load some user options...
  loadColourMaps(this.vis);

  if (this.view) {
    this.fov = this.view.fov || 45;
    this.near_clip = this.view.near || 0;
    this.far_clip = this.view.far || 0;
    this.orientation = this.view.orientation || 1;
    //this.axes = this.view.axis == undefined ? true : this.view.axis;
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
      //Set default colour if not yet set
      if (!vis.objects[id].colour) {
        if ("volume" in vis.objects[id]) //Use a different default for volume (isosurface) colour
          vis.objects[id].colour = [220, 220, 200, 255];
        else
          vis.objects[id].colour = this.dict["colour"].default;
      }

      for (var type in vis.objects[id]) {
        if (["triangles", "points", "lines", "volume"].indexOf(type) < 0) continue;
        if (type == "triangles") this.hasTriangles = true;
        if (type == "points") this.hasPoints = true;
        if (type == "lines") this.hasLines = true;

        if (type == "volume" && vis.objects[id][type].url) {
          //Single volume per object only, each volume needs own renderer
          this.hasVolumes = true;
          var vren = new Renderer(this, 'volume');
          vren.id = id;
          this.renderers.push(vren);
          vren.image = new Image();
          vren.image.src = vis.objects[id][type].url;
          var viewer = this;
          vren.image.onload = function(){ viewer.drawFrame(); viewer.draw(); };
        }

        //Apply object bounding box
        if (objbb && vis.objects[id].min)
          this.checkPointMinMax(vis.objects[id].min);
        if (objbb && vis.objects[id].max)
          this.checkPointMinMax(vis.objects[id].max);

        //Read vertices, values, normals, sizes, etc...
        for (var idx in vis.objects[id][type]) {
          //Only support following data types for now
          decodeBase64(vis.objects[id][type][idx], 'vertices');
          decodeBase64(vis.objects[id][type][idx], 'values');
          decodeBase64(vis.objects[id][type][idx], 'normals');
          decodeBase64(vis.objects[id][type][idx], 'texcoords');
          decodeBase64(vis.objects[id][type][idx], 'colours', 'integer');
          decodeBase64(vis.objects[id][type][idx], 'sizes');
          decodeBase64(vis.objects[id][type][idx], 'indices', 'integer');

          if (vis.objects[id][type][idx].vertices) {
            DEBUG && console.log("Loaded " + vis.objects[id][type][idx].vertices.data.length/3 + " vertices from " + name);
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
            DEBUG && console.log("GRID w : " + w + " , h : " + h);
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

    this.updateDims(this.view);
  }
  var time = (new Date() - start) / 1000.0;
  DEBUG && console.log(time + " seconds to import data");

  //Load texture images
  this.loadTextures();

  //Defaults for interaction flags
  //(Default to interactive render if vertex count < 0.5 M)
  if (vis.interactive == undefined) vis.interactive = (this.vertexCount <= 500000);
  if (vis.immediatesort == undefined) vis.immediatesort = false;
  if (vis.sortenabled == undefined) vis.sortenabled = true;

  //TODO: loaded custom shader is not replaced by default when new model loaded...
  for (var r in this.renderers) {
    var ren = this.renderers[r];
    //Custom shader load
    if (vis.shaders && vis.shaders[ren.type])
      ren.init();
    //Set update flags
    ren.sort = ren.reload = updated;
  }

  //Defer drawing until textures loaded if necessary
  if (!this.hasVolumes && !this.hasTexture) {
    this.drawFrame();
    this.draw();
  }

  //Create UI - disable by omitting dat.gui.min.js
  var viewer = this;
  var changefn = function(value) {
    viewer.reload();
    viewer.draw();
  };

  //Create GUI menu if enabled
  if (this.menu)
    createMenu(this, changefn, 1);
}

Viewer.prototype.loadTextures = function() {
  //Load/reload textures for all objects
  var vis = this.vis;
  for (var id in vis.objects) {
    //Texure loading
    if (vis.objects[id].texture) {
      this.hasTexture = true;
      vis.objects[id].image = new Image();
      vis.objects[id].image.crossOrigin = "anonymous";
      vis.objects[id].image.src = vis.objects[id].texture + '?_' + new Date().getTime(); //Prevent caching
      var viewer = this;
      var i = id; //Variable for closure
      vis.objects[id].image.onload = function() {
        // Flip the image's Y axis to match the WebGL texture coordinate space.
        viewer.webgl.gl.activeTexture(viewer.webgl.gl.TEXTURE0);
        //Flip jpegs
        var flip = vis.objects[i].image.src.slice(0,1024).toLowerCase().indexOf('png') < 0;
        vis.objects[i].tex = viewer.webgl.loadTexture(vis.objects[i].image, viewer.gl.LINEAR, viewer.gl.RGBA, flip);
        viewer.drawFrame();
        viewer.draw();
      };
    }
  }
}

Viewer.prototype.reload = function() {
  for (var r in this.renderers) {
    //Set update flags
    var ren = this.renderers[r];
    ren.sort = ren.reload = true;
  }

  this.applyBackground(this.vis.properties.background);
}

Viewer.prototype.checkPointMinMax = function(coord) {
  for (var i=0; i<3; i++) {
    this.view.min[i] = Math.min(coord[i], this.view.min[i]);
    this.view.max[i] = Math.max(coord[i], this.view.max[i]);
  }
  //console.log(JSON.stringify(this.view.min) + " -- " + JSON.stringify(this.view.max));
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
             "views"      : this.exportView(nocam),
             "properties" : this.vis.properties};

  exp.exported = true;
  exp.reload = reload ? true : false;

  if (nocam) return JSON.stringify(exp);
  //Export with 2 space indentation
  return JSON.stringify(exp, undefined, 2);
}

Viewer.prototype.exportView = function(nocam) {
  //Update camera settings of current view
  if (nocam)
    this.view = {};
  else {
    this.view.rotate = this.getRotation();
    this.view.focus = this.focus;
    this.view.translate = this.translate;
    this.view.scale = this.scale;
  }
  this.view.fov = this.fov;
  this.view.near = this.near_clip;
  this.view.far = this.far_clip;
  //this.properties.border = this.border ? 1 : 0;
  //this.properties.axis = this.axes;
  //this.view.background = this.background.toString();

  return [this.view];
}

Viewer.prototype.exportObjects = function() {
  objs = [];
  for (var id in this.vis.objects) {
    objs[id] = {};
    //Skip geometry data
    for (var type in this.vis.objects[id]) {
      if (type != "triangles" && type != "points" && type != 'lines' && type != "volume") {
        objs[id][type] = this.vis.objects[id][type];
      }
    }
  }
  return objs;
}

Viewer.prototype.exportColourMaps = function() {
  cmaps = [];
  if (this.vis.colourmaps) {
    for (var i=0; i<this.vis.colourmaps.length; i++) {
      cmaps[i] = this.vis.colourmaps[i].palette.get();
      //Copy additional properties
      for (var type in this.vis.colourmaps[i]) {
        if (type != "palette" && type != "colours")
          cmaps[i][type] = this.vis.colourmaps[i][type];
      }
    }
  }
  return cmaps;
}

Viewer.prototype.exportFile = function() {
  window.open('data:text/plain;base64,' + window.btoa(this.toString(false, true)));
}

Viewer.prototype.applyBackground = function(bg) {
  if (!bg) return;
  this.background = new Colour(bg);
  var hsv = this.background.HSV();
  if (this.border) this.border.colour = hsv.V > 50 ? new Colour(0xff444444) : new Colour(0xffbbbbbb);
  this.canvas.parentElement.style.background = this.background.html();
}

Viewer.prototype.addColourMap = function() {
  var properties = this.vis.properties;
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
  this.vis.colourmaps.push(newmap);

  loadColourMaps(this.vis);
}

Viewer.prototype.setColourMap = function(id) {
  var properties = this.vis.properties;
  if (properties.id == undefined) return;
  this.vis.objects[properties.id].colourmap = parseInt(id);
  if (id === undefined) id = -1;
  //Set new colourmap on active object
  if (id < 0) {
    document.getElementById('palette').style.display = 'none';
    document.getElementById('log').style.display = 'none';
  } else {
    //Draw palette UI
    document.getElementById('logscale').checked = this.vis.colourmaps[id].logscale;
    document.getElementById('log').style.display = 'block';
    document.getElementById('palette').style.display = 'block';
    this.gradient.palette = this.vis.colourmaps[id].palette;
    this.gradient.mapid = id; //Save the id
    this.gradient.update();
  }
}

function decodeBase64(obj, datatype, format) {
  if (!format) format = 'float';
  if (!obj[datatype]) return null;
  var buf;
  if (typeof obj[datatype].data == 'string') {
    //Base64 encoded string
    var decoded = atob(obj[datatype].data);
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
      buf = new Float32Array(obj[datatype].data);
    else
      buf = new Uint32Array(obj[datatype].data);
  }

  obj[datatype].data = buf;
  obj[datatype].type = format;

  if (datatype == 'values') {
    if (!obj.values.minimum ||
        !obj.values.maximum) {
      //Value field max min
      var minval = Number.MAX_VALUE, maxval = -Number.MAX_VALUE;
      for (var i=0; i<buf.length; i++) {
        if (buf[i] > maxval)
          maxval = buf[i];
        if (buf[i] < minval)
          minval = buf[i];
      }

      //Set from data where none provided...
      if (!obj.values.minimum)
        obj.values.minimum = minval;
      if (!obj.values.maximum)
        obj.values.maximum = maxval;
    }
  }
}

function removeChildren(element) {
  if (element.hasChildNodes()) {
    while (element.childNodes.length > 0 )
    element.removeChild(element.firstChild);
  }
}

Viewer.prototype.lookupMapId = function(id) {
  if (typeof(id) == 'string') {
    for (var i = 0; i < this.vis.colourmaps.length; i++) {
      if (this.vis.colourmaps[i].name == id) {
        return i;
        break;
      }
    }
  }
  return id;
}

Viewer.prototype.lookupMap = function(id) {
  var mapid = this.lookupMapId(id);
  if (mapid >= 0)
    return this.vis.colourmaps[mapid];
  return null;
}

paletteUpdate = function(obj, id) {
  //Convert name to index for now
  //console.log("paletteUpdate " + id + " : " + obj.name);
  viewer = obj.viewer;
  id = viewer.lookupMapId(id);
  if (id != undefined) viewer.gradient.mapid = id;

  //Load colourmap change
  if (viewer.gradient.mapid < 0) return;
  var cmap = viewer.vis.colourmaps[viewer.gradient.mapid];
  if (!cmap) return;

  paletteLoad(cmap.palette);

  //Update colour data
  cmap.colours = cmap.palette.colours;

  if (viewer.webgl) {
    var gradient = document.getElementById('gradient');
    viewer.webgl.updateTexture(viewer.webgl.gradientTexture, gradient, viewer.gl.TEXTURE1);  //Use 2nd texture unit
    //window.open(gradient.toDataURL());
  }
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
  var viewer = this;
  this.drawTimer = setTimeout(function () {viewer.drawFrame();}, 100 );
}

Viewer.prototype.draw = function(borderOnly) {
  //If requested draw border only (used while interacting)
  //Draw the full model on a timer
  this.drawFrame(borderOnly);
  if (borderOnly && !server) this.drawTimed();
}

Viewer.prototype.drawFrame = function(borderOnly) {
  if (!this.canvas || this.inVR) return;

  if (server && !document.mouse.isdown && this.gl) {
    //Don't draw in server mode unless interacting (border view)
    this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
    return;
  }
  
  /*/Show screenshot while interacting or if using server
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
  }*/
  
  if (!this.gl) return;

  if (this.width == 0 || this.height == 0) {
    //Get size from window/container
    if (this.canvas.parentElement != document.body) {
      //Get from container
      this.width = this.canvas.parentElement.offsetWidth;
      this.height = this.canvas.parentElement.offsetHeight;
      //alert(this.canvas.parentElement.id + " ==> " + this.canvas.parentElement.offsetWidth + " : " + this.canvas.parentElement.clientWidth + " : " + this.canvas.parentElement.style.width);
    } else {
      this.width = window.innerWidth;
      this.height = window.innerHeight;
    }
  }

  if (this.width != this.canvas.width || this.height != this.canvas.height) {
    this.canvas.width = this.canvas.clientWidth = this.width;
    this.canvas.height = this.canvas.clientHeight = this.height;
    this.canvas.setAttribute("width", this.width);
    this.canvas.setAttribute("height", this.height);
    this.webgl.viewport = new Viewport(0, 0, this.width, this.height);
  }
  this.width = this.height = 0;

  var start = new Date();

  this.gl.viewport(this.webgl.viewport.x, this.webgl.viewport.y, this.webgl.viewport.width, this.webgl.viewport.height);
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
    if (this.renderers[r].border) {
      if (!borderOnly && !this.vis.border) continue;
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

Viewer.prototype.syncRotation = function() {
  this.rotated = true;
  this.draw();
  //if (this.command)
  //  this.command('' + this.getRotationString());
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
  return [this.rotate[0], this.rotate[1], this.rotate[2], this.rotate[3]];
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
    this.updateDims(this.view);
    this.draw();
  }

  if (server) {
    //sendCommand('' + this.getRotationString());
    //sendCommand('' + this.getTranslationString());
    sendCommand('reset');
  }
}

Viewer.prototype.zoom = function(factor) {
  if (this.gl) {
    var adj = factor * this.modelsize;
    this.translate[2] += adj;
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

  /*
  console.log("DIMS: " + this.dims[0] + "," + this.dims[1] + "," + this.dims[2]);
  console.log("New model size: " + this.modelsize + ", Focal point: " + this.focus[0] + "," + this.focus[1] + "," + this.focus[2]);
  console.log("Translate: " + this.translate[0] + "," + this.translate[1] + "," + this.translate[2]);
  console.log("Rotate: " + this.rotate[0] + "," + this.rotate[1] + "," + this.rotate[2] + "," + this.rotate[3]);
  console.log(JSON.stringify(view));
  */
}

/*
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
*/


