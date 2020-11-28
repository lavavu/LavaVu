//Bounding box only in WebGL
//Urgent TODO: move shared code with draw.js into common file
//maintaining two copies currently!
//Re-init using previous callback data

function initBox(el, cmd_callback, fixedsize) {
  //console.log("INITBOX: " + el.id);
  var canvas = document.createElement("canvas");
  canvas.classList.add("resized");
  if (!el) el = document.body.firstChild;
  canvas.id = "canvas_" + el.id;
  canvas.imgtarget = el
  el.parentElement.appendChild(canvas);
  canvas.style.cssText = "position: absolute; width: 100%; height: 100%; margin: 0px; padding: 0px; top: 0; left: 0; bottom: 0; right: 0; z-index: 11; border: none;"
  var viewer = new BoxViewer(canvas);

  //Canvas event handling
  canvas.mouse = new Mouse(canvas, new MouseEventHandler(canvasBoxMouseClick, canvasBoxMouseWheel, canvasBoxMouseMove, canvasBoxMouseDown, null, null, canvasBoxMousePinch));
  //Following two settings should probably be defaults?
  canvas.mouse.moveUpdate = true; //Continual update of deltaX/Y
  //canvas.mouse.setDefault();
  canvas.mouse.wheelTimer = true; //Accumulate wheel scroll (prevents too many events backing up)
  defaultMouse = document.mouse = canvas.mouse;

  //Attach viewer object to canvas
  canvas.viewer = viewer;

  //Disable context menu and prevent bubbling
  canvas.addEventListener('contextmenu', e => {e.preventDefault(); e.stopPropagation();});

  //Command callback function
  viewer.command = cmd_callback;

  //Data dict and colourmap names stored in globals
  viewer.dict = window.dictionary;
  viewer.defaultcolourmaps = window.defaultcolourmaps;

  //Enable to forward key presses to server directly
  //(only used for full size viewers)
  if (fixedsize == 1)
    document.addEventListener('keyup', function(event) {keyPress(event, viewer);});

  return viewer;
}

function keyModifiers(event) {
  modifiers = '';
  if (event.ctrlKey) modifiers += 'C';
  if (event.shiftKey) modifiers += 'S';
  if (event.altKey) modifiers += 'A';
  if (event.metaKey) modifiers += 'M';
  return modifiers;
}

function keyPress(event, viewer) {
  // space and arrow keys, prevent scrolling
  if ([' ', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.key))
    event.preventDefault();

  if (event.target.tagName == 'INPUT') return;

  //Special key codes
  if (event.key == 'ArrowUp') key = 17;
  else if (event.key == 'ArrowDown') key = 18;
  else if (event.key == 'ArrowLeft') key = 20;
  else if (event.key == 'ArrowRight') key = 19;
  else if (event.key == 'PageUp') key = 24;
  else if (event.key == 'PageDown') key = 25;
  else if (event.key == 'PageUp') key = 24;
  else if (event.key == 'Home') key = 22;
  else if (event.key == 'End') key = 23;
  else if (event.key == 'Backspace') key = 8;
  else if (event.key == 'Tab') key = 9;
  else if (event.key == 'Escape') key = 27;
  else if (event.key == 'Enter') key = 13;
  else if (event.key == 'Insert') key = 21;
  else if (event.key == 'Delete') key = 127;
  else if (event.key[0] == 'F' && event.key.length > 1)
    key = 189 + parseInt(event.key.substr(1));
  else if (event.key.length == 1) {
    key = event.key.charCodeAt(0);
    if (key > 127 && event.code.length == 4 && event.code.substr(0,3) == 'Key')
      key = event.code.charCodeAt(3);
  } else
    return;

  //console.log(event.key);
  //console.log(event.code);
  //console.log(key);

  var modifiers = keyModifiers(event);

  //Ignore CTRL+R, CTRL+SHIFT+R
  if (key == 114 && modifiers == 'C' || key == 82 && modifiers == 'CS') return;

  cmd = 'key key=' + key + ',modifiers=' + modifiers + ",x=" + defaultMouse.x + ",y=" + defaultMouse.y;
  //console.log(cmd);
  viewer.command(cmd);
}

function canvasBoxMouseClick(event, mouse) {
  mouse.element.viewer.check_context(mouse);

  if (mouse.element.viewer.rotating)
    mouse.element.viewer.command('' + mouse.element.viewer.getRotationString());
  else
    mouse.element.viewer.command('' + mouse.element.viewer.getTranslationString());

  if (mouse.element.viewer.rotating) {
    mouse.element.viewer.rotating = false;
    //mouse.element.viewer.reload = true;
  }

  //Clear the webgl box
  mouse.element.viewer.clear();

  return false;
}

function canvasBoxMouseDown(event, mouse) {
  mouse.element.viewer.check_context(mouse);
  //Just draw so the box appears
  mouse.element.viewer.draw();
  return false;
}

var hideBoxTimer;

function canvasBoxMouseMove(event, mouse) {
  //GUI elements to show on mouseover
  if (mouse.element && mouse.element.viewer && Object.keys(mouse.element.viewer.vis).length) {
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

      if (hideBoxTimer) 
        clearTimeout(hideBoxTimer);

      hideBoxTimer = setTimeout(function () { hideMenu(mouse.element, gui);}, 1000 );
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
      break;
    case 1:
      mouse.element.viewer.rotateZ(Math.sqrt(mouse.deltaX*mouse.deltaX + mouse.deltaY*mouse.deltaY)/5);
      mouse.element.viewer.rotating = true;
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

  mouse.element.viewer.draw();

  return false;
}

function canvasBoxMouseWheel(event, mouse) {
  mouse.element.viewer.check_context(mouse);
  var scroll = event.deltaY < 0 ? 0.01 : -0.01;
  //console.log(event.shiftKey,event.deltaY * -0.01);
  if (event.shiftKey) {
    mouse.element.viewer.zoomClip(scroll);
    //mouse.element.viewer.zoomClip(event.spin*0.01);
  } else {
    mouse.element.viewer.zoom(scroll);
    //mouse.element.viewer.zoom(event.spin*0.01);
  }
  return false; //Prevent default
}

function canvasBoxMousePinch(event, mouse) {
  mouse.element.viewer.check_context(mouse);
  if (event.distance != 0) {
    var factor = event.distance * 0.0001;
    mouse.element.viewer.zoom(factor);
    //Clear the box after a second
    setTimeout(function() {mouse.element.viewer.clear();}, 1000);
  }
  return false; //Prevent default
}

//This object encapsulates a vertex buffer and shader set
function BoxRenderer(gl, colour) {
  this.gl = gl;
  if (colour)
    this.colour = colour;
  else
    this.colour = [0.5, 0.5, 0.5, 1.0];

  //Line renderer
  this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT];

  this.elements = 0;
  this.elementSize = 0;
  for (var i=0; i<this.attribSizes.length; i++)
    this.elementSize += this.attribSizes[i];
}

var vs = "precision highp float; \n \
attribute vec3 aVertexPosition; \n \
attribute vec4 aVertexColour; \n \
uniform mat4 uMVMatrix; \n \
uniform mat4 uPMatrix; \n \
uniform vec4 uColour; \n \
varying vec4 vColour; \n \
void main(void) \n \
{ \n \
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0); \n \
  gl_Position = uPMatrix * mvPosition; \n \
  vColour = uColour; \n \
}";

var fs = "precision highp float; \n \
varying vec4 vColour; \n \
void main(void) \n \
{ \n \
  gl_FragColor = vColour; \n \
}";

BoxRenderer.prototype.init = function() {
  //Compile the shaders
  this.program = new WebGLProgram(this.gl, vs, fs);
  if (this.program.errors) console.log(this.program.errors);
  //Setup attribs/uniforms (flag set to skip enabling attribs)
  this.program.setup(undefined, undefined, true);

  return true;
}

BoxRenderer.prototype.updateBuffers = function(view) {
  //Create buffer if not yet allocated
  if (this.vertexBuffer == undefined) {
    //Init shaders etc...
    if (!this.init()) return;
    this.vertexBuffer = this.gl.createBuffer();
    this.indexBuffer = this.gl.createBuffer();
  }

  //Bind buffers
  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

  this.box(view.min, view.max);
}

BoxRenderer.prototype.box = function(min, max) {
  var zero = [min[0]+0.5*(max[0] - min[0]), min[1]+0.5*(max[1] - min[1]), min[2]+0.5*(max[2] - min[2])];
  var min10 = [min[0] + 0.45*(max[0] - min[0]), min[1]+0.45*(max[1] - min[1]), min[2]+0.45*(max[2] - min[2])];
  var max10 = [min[0] + 0.55*(max[0] - min[0]), min[1]+0.55*(max[1] - min[1]), min[2]+0.55*(max[2] - min[2])];
  var vertices = new Float32Array(
        [
          /* Bounding box */
          min[0], min[1], max[2],
          min[0], max[1], max[2],
          max[0], max[1], max[2],
          max[0], min[1], max[2],
          min[0], min[1], min[2],
          min[0], max[1], min[2],
          max[0], max[1], min[2],
          max[0], min[1], min[2],
          /* 10% box */
          min10[0], min10[1], max10[2],
          min10[0], max10[1], max10[2],
          max10[0], max10[1], max10[2],
          max10[0], min10[1], max10[2],
          min10[0], min10[1], min10[2],
          min10[0], max10[1], min10[2],
          max10[0], max10[1], min10[2],
          max10[0], min10[1], min10[2],
          /* Axis lines */
          min[0], zero[1], zero[2],
          max[0], zero[1], zero[2],
          zero[0], min[1], zero[2],
          zero[0], max[1], zero[2],
          zero[0], zero[1], min[2],
          zero[0], zero[1], max[2]
        ]);

  var indices = new Uint16Array(
        [
          /* Bounding box */
          0, 1, 1, 2, 2, 3, 3, 0,
          4, 5, 5, 6, 6, 7, 7, 4,
          0, 4, 3, 7, 1, 5, 2, 6,
          /* 10% box */
          8, 9, 9, 10, 10, 11, 11, 8,
          12, 13, 13, 14, 14, 15, 15, 12,
          8, 12, 11, 15, 9, 13, 10, 14,
          /* Axis lines */
          16, 17,
          18, 19,
          20, 21
        ]
     );
  this.gl.bufferData(this.gl.ARRAY_BUFFER, vertices, this.gl.STATIC_DRAW);
  this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, indices, this.gl.STATIC_DRAW);
  this.elements = 24+24+6;
}

BoxRenderer.prototype.draw = function(webgl, nobox, noaxes) {
  if (!this.elements) return;

  if (this.program.attributes["aVertexPosition"] == undefined) return; //Require vertex buffer

  webgl.use(this.program);
  webgl.setMatrices();

  //Bind buffers
  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

  //Enable attributes
  for (var key in this.program.attributes)
    this.gl.enableVertexAttribArray(this.program.attributes[key]);

 
  //Line box render
  this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, 0, 0);
  //Bounding box
  if (!nobox) {
    this.gl.uniform4f(this.program.uniforms["uColour"], this.colour[0], this.colour[1], this.colour[2], this.colour[3]);
    this.gl.drawElements(this.gl.LINES, 24, this.gl.UNSIGNED_SHORT, 0);
  }

  //10% box (always draw)
  this.gl.drawElements(this.gl.LINES, 24, this.gl.UNSIGNED_SHORT, 24 * 2);

  //Axes (2 bytes per unsigned short)
  if (!noaxes) {
    this.gl.uniform4f(this.program.uniforms["uColour"], 1.0, 0.0, 0.0, 1.0);
    this.gl.drawElements(this.gl.LINES, 2, this.gl.UNSIGNED_SHORT, (24+24) * 2);
    this.gl.uniform4f(this.program.uniforms["uColour"], 0.0, 1.0, 0.0, 1.0);
    this.gl.drawElements(this.gl.LINES, 2, this.gl.UNSIGNED_SHORT, (24+24+2) * 2);
    this.gl.uniform4f(this.program.uniforms["uColour"], 0.0, 0.0, 1.0, 1.0);
    this.gl.drawElements(this.gl.LINES, 2, this.gl.UNSIGNED_SHORT, (24+24+4) * 2);
  }

  //Disable attribs
  for (var key in this.program.attributes)
    this.gl.disableVertexAttribArray(this.program.attributes[key]);

  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
  this.gl.useProgram(null);
}

//This object holds the viewer details and calls the renderers
function BoxViewer(canvas) {
  this.vis = {};
  this.vis.objects = [];
  this.vis.colourmaps = [];
  this.canvas = canvas;
  if (!canvas) {alert("Invalid Canvas"); return;}
  try {
    this.webgl = new WebGL(this.canvas, {antialias: true}); //, premultipliedAlpha: false});
    this.gl = this.webgl.gl;
    canvas.addEventListener("webglcontextlost", function(event) {
      //event.preventDefault();
      console.log("Context loss detected, clearing data/state and flagging defunct");
      this.mouse.lostContext = true;
    }, false);

    canvas.addEventListener("webglcontextrestored", function(event) {
      console.log("Context restored");
    }, false);

  } catch(e) {
    //No WebGL
    console.log("No WebGL: " + e);
  }

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

  //Non-persistant settings
  this.mode = 'Rotate';
  if (!this.gl) return;

  //Create the renderers
  this.border = new BoxRenderer(this.gl, [0.5,0.5,0.5,1]);

  this.gl.enable(this.gl.DEPTH_TEST);
  this.gl.depthFunc(this.gl.LEQUAL);
  //this.gl.depthMask(this.gl.FALSE);
  this.gl.enable(this.gl.BLEND);
  //this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);
  //this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ZERO, this.gl.ONE);
  this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
}

BoxViewer.prototype.check_context = function(mouse) {
  if (mouse.lostContext) {
    console.log("Recreating viewer after context loss");
    //Reinit on timer? on click?
    var box = initBox(this.canvas.imgtarget, this.command);
    box.loadFile(JSON.stringify(this.vis));
    //Delete the old canvas now
    this.canvas.parentNode.removeChild(this.canvas);
    this.vis = null;
    mouse.lostContext = false;
    this.deleted = true;
  }
}

BoxViewer.prototype.reload = function() {
  this.command('reload');
}

BoxViewer.prototype.checkPointMinMax = function(coord) {
  for (var i=0; i<3; i++) {
    this.view.min[i] = Math.min(coord[i], this.view.min[i]);
    this.view.max[i] = Math.max(coord[i], this.view.max[i]);
  }
  //console.log(JSON.stringify(this.view.min) + " -- " + JSON.stringify(this.view.max));
}

function Merge(obj1, obj2) {
  for (var p in obj2) {
    try {
      //console.log(p + " ==> " + obj2[p].constructor);
      // Property in destination object set; update its value.
      if (!obj2.hasOwnProperty(p)) continue;
      if (obj2[p].constructor == Object || obj2[p].constructor == Array) {
        obj1[p] = Merge(obj1[p], obj2[p]);
      } else {
        //Just copy
        obj1[p] = obj2[p];
      }
    } catch(e) {
      // Property in destination object not set; create it and set its value.
      obj1[p] = obj2[p];
    }
  }

  //Clear any keys in obj1 that are not in obj2
  for (var p in obj1) {
    if (!obj1.hasOwnProperty(p)) continue; //Ignore built in keys
    if (p in obj2) continue;
    if (typeof(obj1[p]) != 'object') {
      //Just delete
      delete obj1[p]
    }
  }

  return obj1;
}

BoxViewer.prototype.exportJson = function(nocam) {
  return {"objects"    : this.exportObjects(),
          "colourmaps" : this.vis.colourmaps,
          "views"      : this.exportView(nocam),
          "properties" : this.vis.properties};
}

BoxViewer.prototype.sendState = function(reload) {
  var exp = this.exportJson();
  exp.reload = reload;
  this.command(JSON.stringify(exp));
}

BoxViewer.prototype.toString = function(nocam, reload, noformat) {
  var exp = this.exportJson(nocam);
  exp.exported = true;
  exp.reload = reload ? true : false;

  if (noformat)
    return JSON.stringify(exp);
  //Export with 2 space indentation
  return JSON.stringify(exp, undefined, 2);
}

BoxViewer.prototype.exportView = function(nocam) {
  //Update camera settings of current view
  if (nocam)
    this.view = {};
  else {
    this.view.rotate = this.getRotation();
    this.view.focus = this.focus;
    this.view.translate = this.translate;
    //Allows scale editing in UI by commenting this
    //this.view.scale = this.scale;
  }
  /*
   * If we overwrite these, changes from menu will not apply
   *
  this.view.fov = this.fov;
  this.view.near = this.near_clip;
  this.view.far = this.far_clip;
  this.view.border = this.showBorder ? 1 : 0;
  this.view.axis = this.axes;
  */
  //this.view.background = this.background.toString();

  //Never export min/max
  var V = Object.assign(this.view);
  V.min = undefined;
  V.max = undefined;
  return [V];
}

BoxViewer.prototype.exportObjects = function() {
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
  //console.log("OBJECTS: " + JSON.stringify(objs));

  return objs;
}

BoxViewer.prototype.exportColourMaps = function() {
  return this.vis.colourmaps;
//DEPRECATED
  //Below extracts map from gui editor, not needed unless editor hasn't updated vis object
  cmaps = [];
  if (this.vis.colourmaps) {
    for (var i=0; i<this.vis.colourmaps.length; i++) {
      console.log(i + " - " + this.vis.colourmaps[i].palette);
      if (!this.vis.colourmaps[i].palette) continue;
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

BoxViewer.prototype.exportFile = function() {
  window.open('data:text/plain;base64,' + window.btoa(this.toString(false, true)));
}

BoxViewer.prototype.loadFile = function(source) {
  //Skip update to rotate/translate etc if in process of updating
  //if (document.mouse.isdown) return;
  if (source.length < 3) {
    console.log('Invalid source data, ignoring');
    console.log(source);
    console.log(BoxViewer.prototype.loadFile.caller);
    return; //Invalid
  }

  if (!this.vis)
    this.vis = {};

  //Parse data
  var src = {};
  try {
    src = JSON.parse(source);
  } catch(e) {
    console.log(source);
    console.log("Parse Error: " + e);
    return;
  }

  //Before merge, delete all colourmap data if changed
  for (var c in this.vis.colourmaps) {
    //Name or colour count mismatch? delete so can be recreated
    if (this.vis.colourmaps[c].name != src.colourmaps[c].name || 
        this.vis.colourmaps[c].colours.length != src.colourmaps[c].colours.length) {
      //Delete the colourmap folder for this map, will be re-created
      if (this.cgui && this.cgui.__folders[this.vis.colourmaps[c].name]) {
        this.cgui.removeFolder(this.cgui.__folders[this.vis.colourmaps[c].name]);
        this.cgui.__folders[this.vis.colourmaps[c].name] = undefined;
      }
      //Clear all the colours so new data will replace in merge
      this.vis.colourmaps[c].colours = undefined;
    }
  }

  //Merge keys - preserves original objects for gui access
  Merge(this.vis, src);

  //No model loaded? Prevents errors so default are loaded
  if (!this.vis.properties)
    this.vis.properties = {};

  //Set active view (always first for now)
  this.view = this.vis.views[0];

  //Always set a bounding box, get from objects if not in view
  var objbb = false;
  if (!this.view.min || !this.view.max) {
    this.view.min = [Number.MAX_VALUE, Number.MAX_VALUE, Number.MAX_VALUE];
    this.view.max = [-Number.MAX_VALUE, -Number.MAX_VALUE, -Number.MAX_VALUE];
    objbb = true;
  }
  //console.log(JSON.stringify(this.view,null, 2));

  if (this.view) {
    this.fov = this.view.fov || 45;
    this.near_clip = this.view.near || 0;
    this.far_clip = this.view.far || 0;
    this.orientation = this.view.orientation || 1;
    this.axes = this.view.axis == undefined ? true : this.view.axis;
  }

  if (this.vis.properties.resolution && this.vis.properties.resolution[0] && this.vis.properties.resolution[1]) {
    this.width = this.vis.properties.resolution[0];
    this.height = this.vis.properties.resolution[1];
    //this.canvas.style.width = "";
    //this.canvas.style.height = "";
  } else {
    //Autosize when img loaded
    this.width = 0;
    this.height = 0;
  }

  //Process object data
  for (var id in this.vis.objects) {
    //Apply object bounding box
    if (objbb && this.vis.objects[id].min)
      this.checkPointMinMax(this.vis.objects[id].min);
    if (objbb && this.vis.objects[id].max)
      this.checkPointMinMax(this.vis.objects[id].max);
  }

  this.updateDims(this.view);

  //Update display
  if (!this.gl) return;
  this.draw();
  this.clear();

  //Create UI - disable by omitting dat.gui.min.js
  //(If menu doesn't exist or is open, update immediately
  // otherwise, wait until clicked/opened as update is slow)
  if (!this.gui || !this.gui.closed)
    this.menu();
  else
    this.reloadgui = true;

  //Copy updated values to all generated controls
  //This allows data changed by other means (ie: through python) to 
  // be reflected in the HTML control values
  var that = this;
  var getValWithIndex = function(prop, val, idx) {
    if (val === undefined) return;
    if (idx != null && idx >= 0 && val.length > idx)
       val = val[idx];
    //Round off floating point numbers
    if (that.dict[property].type.indexOf('real') >= 0)
       return val.toFixed(6) / 1; //toFixed() / 1 rounds to fixed position and removes trailing zeros
    return val;
  }
  var pel = this.canvas.parentElement.parentElement;
  var children = pel.getElementsByTagName('*');
  for (var i=0; i<children.length; i++) {
    //Process property controls
    var id = children[i].id;
    if (id.indexOf('lvctrl') >= 0) {
      var target = children[i].getAttribute("data-target");
      var property = children[i].getAttribute("data-property");
      var idx = children[i].getAttribute("data-index");
      //console.log(id + " : " + target + " : " + property + " [" + idx + "]");
      if (property) {
        if (target) {
          //Loop through objects, find those whose name matches element target
          for (var o in this.vis.objects) {
            var val = this.vis.objects[o][property];
            if (val === undefined) continue;
            if (this.vis.objects[o].name == target) {
              if (idx != null && idx >= 0)
                val = val[idx];
              //console.log(target + " ==> SET " + id + " : ['" + property + "'] VALUE TO " + val);
              //console.log("TAG: " + children[i].tagName);
              if (children[i].type == 'checkbox')
                children[i].checked = val;
              else if (children[i].tagName == 'SELECT') {
                //If integer, set by index, otherwise set by value
                var parsed = parseInt(val);
                if (isNaN(parsed) || parsed.toString() != val) {
                  //If the value is in the options, set it, otherwise leave as is
                  for (var c in children[i].options) {
                    if (children[i].options[c].value == val) {
                      children[i].selectedIndex = c;
                      break;
                    }
                  }
                } else {
                  children[i].selectedIndex = parsed;
                }
              } else if (children[i].tagName == 'DIV') {
                children[i].style.background = new Colour(val).html();
              } else if (children[i].tagName == 'CANVAS' && children[i].gradient) {
                //Store full list of colourmaps
                var el = children[i]; //canvas element
                //Ensure we aren't merging the same object
                el.colourmaps = Merge(JSON.parse(JSON.stringify(el.colourmaps)), this.vis.colourmaps);
                //Load the initial colourmap
                if (!el.selectedIndex >= 0) {
                  //Get the selected index from the property value
                  for (var c in this.vis.colourmaps) {
                    if (this.vis.colourmaps[c].name == val) {
                      el.selectedIndex = c;
                      //Copy, don't link
                      el.currentmap = JSON.parse(JSON.stringify(this.vis.colourmaps[c]));
                      //el.currentmap = Object.assign(this.vis.colourmaps[c]);
                      break;
                    }
                  }
                }
                //Can't use gradient.read() here as it triggers another state load,
                //looping infinitely - so load the palette change directly
                el.gradient.palette = new Palette(this.vis.colourmaps[el.selectedIndex].colours, true);
                //el.gradient.reset(); //For some reason this screws up colour editing
                el.gradient.update(true); //Update without triggering callback that triggers a state reload
              } else {
                //console.log(id + " : " + target + " : " + property + " = " + val);
                children[i].value = getValWithIndex(property, val, idx);
              }
            }
          }
        } else if (this.vis.views[0][property] != null) {
          //View property
          var val = this.vis.views[0][property];
          //console.log("SET " + id + " : ['" + property + "'] VIEW PROP VALUE TO " + val);
          children[i].value = getValWithIndex(property, val, idx);
        } else if (this.vis.properties[property] != null) {
          //Global property
          var val = this.vis.properties[property];
          //console.log("SET " + id + " : ['" + property + "'] GLOBAL VALUE TO " + val);
          children[i].value = getValWithIndex(property, val, idx);
        }
      }
    }
  }
}

BoxViewer.prototype.menu = function() {
  //Create UI - disable by omitting dat.gui.min.js
  //This is slow! Don't call while animating
  var viewer = this;
  var changefn = function(value, reload) {
    //console.log(JSON.stringify(Object.keys(this)));
    //console.log(value);
    if (reload == undefined) {
      reload = true;
      if (this.property && viewer.dict[this.property])
      {
        //Get reload level from prop dict
        //console.log(this.property + " REDRAW: " + viewer.dict[this.property].redraw);
        reload = viewer.dict[this.property].redraw;
      }
    }

    //Sync state and reload
    viewer.sendState(reload);
  };

  if (!this.gui || this.gui.closed) {
    //Re-create from scratch if closed or not present
    createMenu(this, changefn);
  } else {
    updateMenu(this, changefn);
  }
  this.reloadgui = false;
}

BoxViewer.prototype.clear = function() {
  if (!this.gl) return;
  this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
}

BoxViewer.prototype.draw = function() {
  if (!this.canvas) return;

  //Get the dimensions from the current canvas
  if (this.width != this.canvas.offsetWidth || this.height != this.canvas.offsetHeight) {
    this.width = this.canvas.offsetWidth;
    this.height = this.canvas.offsetHeight;
    //Need to set this too for some reason
    this.canvas.width = this.width;
    this.canvas.height = this.height;
    if (this.gl) {
      this.gl.viewportWidth = this.width;
      this.gl.viewportHeight = this.height;
      this.webgl.viewport = new Viewport(0, 0, this.width, this.height);
    }
  }

  /*/Attempt to prevent element resizing to 0,0 when a frame is dropped
  if (this.width)
    this.canvas.parentElement.style.minWidth = this.width + 'px';
  if (this.height)
    this.canvas.parentElement.style.minHeight = this.height + 'px';
  */

  if (!this.gl) return;

  //Check isContextLost
  if (this.gl.isContextLost()) {
    console.log("boxviewer_draw_: context lost");
    this.mouse.lostContext = true;
    return;
  }

  this.gl.viewport(0, 0, this.gl.viewportWidth, this.gl.viewportHeight);
  //this.gl.clearColor(1, 1, 1, 0);
  this.gl.clearColor(0, 0, 0, 0);
  this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

  this.webgl.view(this);

  //Render objects
  this.border.draw(this.webgl, false, true);

    //Remove translation and re-render axes
    var tr = this.translate.slice();
    this.translate = [0,0,-this.modelsize*1.25];
    this.webgl.apply(this);
    this.border.draw(this.webgl, true, false);
    this.translate = tr;
}

BoxViewer.prototype.syncRotation = function(rot) {
  if (rot)
    this.rotate = quat4.create(rot);
  this.rotated = true;
  this.draw();
  rstr = '' + this.getRotationString();
  that = this;
  window.requestAnimationFrame(function() {that.command(rstr);});
}

BoxViewer.prototype.rotateX = function(deg) {
  this.rotation(deg, [1,0,0]);
}

BoxViewer.prototype.rotateY = function(deg) {
  this.rotation(deg, [0,1,0]);
}

BoxViewer.prototype.rotateZ = function(deg) {
  this.rotation(deg, [0,0,1]);
}

BoxViewer.prototype.rotation = function(deg, axis) {
  //Quaterion rotate
  var arad = deg * Math.PI / 180.0;
  var rotation = quat4.fromAngleAxis(arad, axis);
  rotation = quat4.normalize(rotation);
  this.rotate = quat4.multiply(rotation, this.rotate);
}

BoxViewer.prototype.getRotation = function() {
  return [this.rotate[0], this.rotate[1], this.rotate[2], this.rotate[3]];
}

BoxViewer.prototype.getRotationString = function() {
  //Return current rotation quaternion as string
  var q = this.getRotation();
  return 'rotation ' + q[0] + ' ' + q[1] + ' ' + q[2] + ' ' + q[3];
}

BoxViewer.prototype.getTranslationString = function() {
  return 'translation ' + this.translate[0] + ' ' + this.translate[1] + ' ' + this.translate[2];
}

BoxViewer.prototype.reset = function() {
  if (this.gl) {
    this.updateDims(this.view);
    this.draw();
  }

  this.command('reset');
}

BoxViewer.prototype.zoom = function(factor) {
  if (this.gl) {
    this.translate[2] += factor * Math.max(0.01*this.modelsize, Math.abs(this.translate[2]));
    this.draw();
  }

  var that = this;
  if (this.zoomTimer) 
    clearTimeout(this.zoomTimer);
  this.zoomTimer = setTimeout(function () {that.command('' + that.getTranslationString());  that.clear(); that.zoomTimer = null;}, 500 );

  //this.command('' + this.getTranslationString());
  //this.command('zoom ' + factor);
}

BoxViewer.prototype.zoomClip = function(factor) {
  var near_clip = this.near_clip + factor * this.modelsize;
  if (near_clip >= this.modelsize * 0.001)
     this.near_clip = near_clip;

  if (this.gl) this.draw();

  var that = this;
  if (this.zoomTimer) clearTimeout(this.zoomTimer);
  //this.zoomTimer = setTimeout(function () {that.command('zoomclip ' + factor);  that.clear();}, 500 );
  this.zoomTimer = setTimeout(function () {that.command('nearclip ' + that.near_clip);  that.clear();}, 500 );

  //this.command('zoomclip ' + factor);
}

BoxViewer.prototype.updateDims = function(view) {
  if (!view) return;
  var oldsize = this.modelsize;

  //Check for valid dims
  for (var i=0; i<3; i++) {
    if (view.bounds && view.max[i] < view.bounds.max[i])
      view.max[i] = view.bounds.max[i];
    if (view.bounds && view.min[i] > view.bounds.min[i])
      view.min[i] = view.bounds.min[i];
    /*if (view.max[i] <  view.min[i] <= 0.0) {
        view.max[i] = 1.0;
        view.min[i] = -1.0;
      }
    }*/
  }

  this.dims = [view.max[0] - view.min[0], view.max[1] - view.min[1], view.max[2] - view.min[2]];
  this.modelsize = Math.sqrt(this.dims[0]*this.dims[0] + this.dims[1]*this.dims[1] + this.dims[2]*this.dims[2]);

  this.focus = [view.min[0] + 0.5*this.dims[0], view.min[1] + 0.5*this.dims[1], view.min[2] + 0.5*this.dims[2]];
  this.centre = [this.focus[0],this.focus[1],this.focus[2]];

  this.translate = [0,0,0];
  if (this.modelsize != oldsize) this.translate[2] = -this.modelsize*1.25;

  if (this.near_clip == 0.0) this.near_clip = this.modelsize / 10.0;   
  if (this.far_clip == 0.0) this.far_clip = this.modelsize * 10.0;

  quat4.identity(this.rotate);

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

  //console.log("DIMS: " + view.min[0] + " to " + view.max[0] + "," + view.min[1] + " to " + view.max[1] + "," + view.min[2] + " to " + view.max[2]);
  //console.log("New model size: " + this.modelsize + ", Focal point: " + this.focus[0] + "," + this.focus[1] + "," + this.focus[2]);
  //console.log("Translate: " + this.translate[0] + "," + this.translate[1] + "," + this.translate[2]);

  if (!this.gl) return;
 
  //Create the bounding box vertex buffer
  this.border.updateBuffers(this.view);
}

