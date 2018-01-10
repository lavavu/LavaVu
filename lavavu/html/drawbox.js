//Bounding box only in WebGL
//Urgent TODO: move shared code with draw.js into common file
//maintaining two copies currently!
function initBox(el, cmd_callback) {
  //console.log("INITBOX: " + el.id);
  var canvas = document.createElement("canvas");
  if (!el) el = document.body.firstChild;
  canvas.id = "canvas_" + el.id;
  canvas.imgtarget = el
  el.parentElement.appendChild(canvas);
  canvas.style.cssText = "position: absolute; width: 100%; height: 100%; margin: 0px; padding: 0px; top: 0; left: 0; bottom: 0; right: 0; z-index: 11; border: none;"
  viewer = new Viewer(canvas);

  //Canvas event handling
  canvas.mouse = new Mouse(canvas, new MouseEventHandler(canvasMouseClick, canvasMouseWheel, canvasMouseMove, canvasMouseDown, null, null, canvasMousePinch));
  //Following two settings should probably be defaults?
  canvas.mouse.moveUpdate = true; //Continual update of deltaX/Y
  //canvas.mouse.setDefault();
  canvas.mouse.wheelTimer = true; //Accumulate wheel scroll (prevents too many events backing up)
  defaultMouse = document.mouse = canvas.mouse;

  //Attach viewer object to canvas
  canvas.viewer = viewer;

  //Command callback function
  viewer.command = cmd_callback;

  return viewer;
}

function updateBox(viewer, loaderfn) {
  if (!viewer) return;
  //Loader callback
  loaderfn(function(data) {viewer.loadFile(data);});
}

function canvasMouseClick(event, mouse) {
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

function canvasMouseDown(event, mouse) {
  return false;
}

var hideTimer;

function canvasMouseMove(event, mouse) {
  if (mouse.element && mouse.element.imgtarget) {
    var rect = mouse.element.getBoundingClientRect();
    x = event.clientX-rect.left;
    y = event.clientY-rect.top;
    if (x >= 0 && y >= 0 && x < rect.width && y < rect.height) {
      mouse.element.imgtarget.nextElementSibling.style.display = "block";
      if (hideTimer) 
        clearTimeout(hideTimer);
      hideTimer = setTimeout(function () {mouse.element.imgtarget.nextElementSibling.style.display = "none";}, 1000 );
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

var zoomTimer;
var zoomClipTimer;
var zoomSpin = 0;

function canvasMouseWheel(event, mouse) {
  if (event.shiftKey) {
    var factor = event.spin * 0.01;
    if (window.navigator.platform.indexOf("Mac") >= 0)
      factor *= 0.1;
    if (zoomClipTimer) clearTimeout(zoomClipTimer);
    zoomClipTimer = setTimeout(function () {mouse.element.viewer.zoomClip(factor);}, 100 );
  } else {
    if (zoomTimer) 
      clearTimeout(zoomTimer);
    zoomSpin += event.spin;
    zoomTimer = setTimeout(function () {mouse.element.viewer.zoom(zoomSpin*0.01); zoomSpin = 0;}, 100 );
    //Clear the box after a second
    setTimeout(function() {mouse.element.viewer.clear();}, 1000);
  }
  return false; //Prevent default
}

function canvasMousePinch(event, mouse) {
  if (event.distance != 0) {
    var factor = event.distance * 0.0001;
    mouse.element.viewer.zoom(factor);
    //Clear the box after a second
    setTimeout(function() {mouse.element.viewer.clear();}, 1000);
  }
  return false; //Prevent default
}

//This object encapsulates a vertex buffer and shader set
function Renderer(gl, colour) {
  this.gl = gl;
  if (colour) this.colour = colour;

    //Line renderer
    this.attributes = ["aVertexPosition"],
    this.uniforms = ["uColour"]
    this.attribSizes = [3 * Float32Array.BYTES_PER_ELEMENT];

  this.elements = 0;
  this.elementSize = 0;
  for (var i=0; i<this.attribSizes.length; i++)
    this.elementSize += this.attribSizes[i];
}

Renderer.prototype.init = function() {
  //Compile the shaders
  this.program = new WebGLProgram(this.gl, "line-vs", "line-fs");
  if (this.program.errors) console.log(this.program.errors);
  //Setup attribs/uniforms (flag set to skip enabling attribs)
  this.program.setup(this.attributes, this.uniforms, true);

  return true;
}

Renderer.prototype.updateBuffers = function(view) {
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
  this.elements = 24;
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

Renderer.prototype.draw = function(webgl) {
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

  if (this.colour)
    this.gl.uniform4f(this.program.uniforms["uColour"], this.colour[0], this.colour[1], this.colour[2], this.colour[3]);
 
  //Line box render
  this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], 3, this.gl.FLOAT, false, 0, 0);
  this.gl.drawElements(this.gl.LINES, this.elements, this.gl.UNSIGNED_SHORT, 0);

  //Disable attribs
  for (var key in this.program.attributes)
    this.gl.disableVertexAttribArray(this.program.attributes[key]);

  this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
  this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
  this.gl.useProgram(null);
}

//This object holds the viewer details and calls the renderers
function Viewer(canvas) {
  this.canvas = canvas;
  if (!canvas) {alert("Invalid Canvas"); return;}
  try {
    this.webgl = new WebGL(this.canvas, {antialias: true}); //, premultipliedAlpha: false});
    this.gl = this.webgl.gl;
    canvas.addEventListener("webglcontextlost", function(event) {
      event.preventDefault();
      console.log("CONTEXT LOSS DETECTED");
    }, false);

  } catch(e) {
    //No WebGL
    console.log("No WebGL: " + e);
  }

  this.vis = {};
  this.view = 0; //Active view

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

  if (!this.gl) return;

  //Create the renderers
  this.border = new Renderer(this.gl, [0.5,0.5,0.5,1]);

  this.gl.enable(this.gl.DEPTH_TEST);
  this.gl.depthFunc(this.gl.LEQUAL);
  //this.gl.depthMask(this.gl.FALSE);
  this.gl.enable(this.gl.BLEND);
  //this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);
  //this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ZERO, this.gl.ONE);
  this.gl.blendFuncSeparate(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA, this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
}

Viewer.prototype.checkPointMinMax = function(coord) {
  for (var i=0; i<3; i++) {
    this.vis.views[this.view].min[i] = Math.min(coord[i], this.vis.views[this.view].min[i]);
    this.vis.views[this.view].max[i] = Math.max(coord[i], this.vis.views[this.view].max[i]);
  }
  //console.log(JSON.stringify(this.vis.views[this.view].min) + " -- " + JSON.stringify(this.vis.views[this.view].max));
}

Viewer.prototype.loadFile = function(source) {
  //Skip update to rotate/translate etc if in process of updating
  //if (document.mouse.isdown) return;

  //Replace data
  this.vis = JSON.parse(source);

  //Always set a bounding box, get from objects if not in view
  var objbb = false;
  if (!this.vis.views[this.view].min || !this.vis.views[this.view].max) {
    this.vis.views[this.view].min = [Number.MAX_VALUE, Number.MAX_VALUE, Number.MAX_VALUE];
    this.vis.views[this.view].max = [-Number.MAX_VALUE, -Number.MAX_VALUE, -Number.MAX_VALUE];
    objbb = true;
  }
  //console.log(JSON.stringify(this.vis.views[this.view],null, 2));

  if (this.vis.views[this.view]) {
    this.near_clip = this.vis.views[this.view].near || 0;
    this.far_clip = this.vis.views[this.view].far || 0;
    this.orientation = this.vis.views[this.view].orientation || 1;
    this.axes = this.vis.views[this.view].axis == undefined ? true : this.vis.views[this.view].axis;
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

  this.updateDims(this.vis.views[this.view]);

  //Update display
  if (!this.gl) return;
  this.draw();
  this.clear();
}


Viewer.prototype.clear = function() {
  if (!this.gl) return;
  this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
}

Viewer.prototype.draw = function() {
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
  if (!this.gl) return;

  this.gl.viewport(0, 0, this.gl.viewportWidth, this.gl.viewportHeight);
  //this.gl.clearColor(1, 1, 1, 0);
  this.gl.clearColor(0, 0, 0, 0);
  this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

  this.webgl.view(this);

  //Render objects
  this.border.draw(this.webgl);

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
    this.updateDims(this.vis.views[this.view]);
    this.draw();
  }

  this.command('reset');
}

Viewer.prototype.zoom = function(factor) {
  if (this.gl) {
    this.translate[2] += factor * this.modelsize;
    this.draw();
  }

  this.command('' + this.getTranslationString());
  //this.command('zoom ' + factor);
}

Viewer.prototype.zoomClip = function(factor) {
  if (this.gl) {
     var near_clip = this.near_clip + factor * this.modelsize;
     if (near_clip >= this.modelsize * 0.001)
       this.near_clip = near_clip;
    this.draw();
  }

  this.command('zoomclip ' + factor);
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

  //console.log("DIMS: " + min[0] + " to " + max[0] + "," + min[1] + " to " + max[1] + "," + min[2] + " to " + max[2]);
  //console.log("New model size: " + this.modelsize + ", Focal point: " + this.focus[0] + "," + this.focus[1] + "," + this.focus[2]);
  //console.log("Translate: " + this.translate[0] + "," + this.translate[1] + "," + this.translate[2]);

  if (!this.gl) return;
 
  //Create the bounding box vertex buffer
  this.border.updateBuffers(this.vis.views[this.view]);
}

