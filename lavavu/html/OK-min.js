/** @preserve Javascript graphics utility library
 * Helper functions, WebGL classes, Mouse input, Colours and Gradients UI
 * Copyright (c) 2014, Owen Kaluza
 * Released into public domain:
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it as long as this header remains intact
 */
//Miscellaneous javascript helper functions
//Module definition, TODO: finish module
var OK = (function () {
  var ok = {};

  ok.debug_on = false;
  ok.debug = function(str) {
      if (!ok.debug_on) return;
      var uconsole = document.getElementById('console');
      if (uconsole)
        uconsole.innerHTML = "<div style=\"font-family: 'monospace'; font-size: 8pt;\">" + str + "</div>" + uconsole.innerHTML;
      else
        console.log(str);
  };

  ok.clear = function consoleClear() {
    var uconsole = document.getElementById('console');
    if (uconsole) uconsole.innerHTML = '';
  };

  return ok;
}());

function getSearchVariable(variable, defaultVal) {
  var query = window.location.search.substring(1);
  var vars = query.split("&");
  for (var i=0;i<vars.length;i++) {
    var pair = vars[i].split("=");
    if (unescape(pair[0]) == variable) {
      return unescape(pair[1]);
    }
  }
  return defaultVal;
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

//DOM

//Shortcuts for element and style lookup
if (!window.ELEMENT) {
  window.ELEMENT = function(v,o) { return((typeof(o)=='object'?o:document).getElementById(v)); }
}
if (!window.STYLE) {
  window.STYLE = function(o)  { o = document.getElementById(o); if(o) return(o.style); }
}
if (!window.toggle) {
  window.toggle = function(v) { var d = document.getElementById(v).style.display; if (d == 'none' || !d) document.getElementById(v).style.display='block'; else document.getElementById(v).style.display='none'; }
}

//Set display style of all elements of classname
function setAll(display, classname) {
  var elements = document.getElementsByClassName(classname)
  for (var i=0; i<elements.length; i++)
    elements[i].style.display = display;
}

//Get some data stored in a script element
function getSourceFromElement(id) {
  var script = document.getElementById(id);
  if (!script) return null;
  var str = "";
  var k = script.firstChild;
  while (k) {
    if (k.nodeType == 3)
      str += k.textContent;
    k = k.nextSibling;
  }
  return str;
}

function removeChildren(element) {
  if (element.hasChildNodes()) {
    while (element.childNodes.length > 0)
      element.removeChild(element.firstChild);
  }
}

//Browser specific animation frame request
if ( !window.requestAnimationFrame ) {
  window.requestAnimationFrame = ( function() {
    return window.webkitRequestAnimationFrame ||
           window.mozRequestAnimationFrame ||
           window.oRequestAnimationFrame ||
           window.msRequestAnimationFrame;
  } )();
}

//Browser specific full screen request
function requestFullScreen(id) {
  var element = document.getElementById(id);
  if (element.requestFullscreen)
      element.requestFullscreen();
  else if (element.mozRequestFullScreen)
      element.mozRequestFullScreen();
  else if (element.webkitRequestFullScreen)
      element.webkitRequestFullScreen();
}

function typeOf(value) {
  var s = typeof value;
  if (s === 'object') {
    if (value) {
      if (typeof value.length === 'number' &&
          !(value.propertyIsEnumerable('length')) &&
          typeof value.splice === 'function') {
        s = 'array';
      }
    } else {
      s = 'null';
    }
  }
  return s;
}

function isEmpty(o) {
  var i, v;
  if (typeOf(o) === 'object') {
    for (i in o) {
      v = o[i];
      if (v !== undefined && typeOf(v) !== 'function') {
        return false;
      }
    }
  }
  return true;
}

//AJAX
//Reads a file from server, responds when done with file data + passed name to callback function
function ajaxReadFile(filename, callback, nocache, progress, headers)
{ 
  var http = new XMLHttpRequest();
  var total = 0;
  if (progress != undefined) {
    if (typeof(progress) == 'number')
      total = progress;
    else
      http.onprogress = progress;
  }

  http.onreadystatechange = function()
  {
    if (total > 0 && http.readyState > 2) {
      //Passed size progress
      var recvd = parseInt(http.responseText.length);
      //total = parseInt(http.getResponseHeader('Content-length'))
      if (progress) setProgress(recvd / total * 100);
    }

    if (http.readyState == 4) {
      if (http.status == 200) {
        if (progress) setProgress(100);
        OK.debug("RECEIVED: " + filename);
        if (callback)
          callback(http.responseText, filename);
      } else {
        if (callback)
          callback("Error: " + http.status + " : " + filename);    //Error callback
        else
          OK.debug("Ajax Read File Error: returned status code " + http.status + " " + http.statusText);
      }
    }
  } 

  //Add date to url to prevent caching
  if (nocache)
  {
    var d = new Date();
    http.open("GET", filename + "?d=" + d.getTime(), true); 
  }
  else
    http.open("GET", filename, true); 

  //Custom headers
  for (var key in headers)
    http.setRequestHeader(key, headers[key]);

  http.send(null); 
}

function readURL(url, nocache, progress) {
  //Read url (synchronous)
  var http = new XMLHttpRequest();
  var total = 0;
  if (progress != undefined) {
    if (typeof(progress) == 'number')
      total = progress;
    else
      http.onprogress = progress;
  }

  http.onreadystatechange = function()
  {
    if (total > 0 && http.readyState > 2) {
      //Passed size progress
      var recvd = parseInt(http.responseText.length);
      //total = parseInt(http.getResponseHeader('Content-length'))
      if (progress) setProgress(recvd / total * 100);
    }
  } 

  //Add date to url to prevent caching
  if (nocache)
  {
    var d = new Date();
    http.open("GET", url + "?d=" + d.getTime(), false); 
  } else
    http.open('GET', url, false);
  http.overrideMimeType('text/plain; charset=x-user-defined');
  http.send(null);
  if (http.status != 200) return '';
  if (progress) setProgress(100);
  return http.responseText;
}

function updateProgress(evt) 
{
  //evt.loaded: bytes browser received/sent
  //evt.total: total bytes set in header by server (for download) or from client (upload)
  if (evt.lengthComputable) {
    setProgress(evt.loaded / evt.total * 100);
    OK.debug(evt.loaded + " / " + evt.total);
  }
} 

function setProgress(percentage)
{
  var val = Math.round(percentage);
  document.getElementById('progressbar').style.width = (3 * val) + "px";
  document.getElementById('progressstatus').innerHTML = val + "%";
} 

//Posts request to server, responds when done with response data to callback function
function ajaxPost(url, params, callback, progress, headers)
{ 
  var http = new XMLHttpRequest();
  if (progress != undefined) http.upload.onprogress = progress;

  http.onreadystatechange = function()
  { 
    if (http.readyState == 4) {
      if (http.status == 200) {
        if (progress) setProgress(100);
        OK.debug("POST: " + url);
        if (callback)
          callback(http.responseText);
      } else {
        if (callback)
          callback("Error, status:" + http.status);    //Error callback
        else
          OK.debug("Ajax Post Error: returned status code " + http.status + " " + http.statusText);
      }
    }
  }

  http.open("POST", url, true); 

  //Send the proper header information along with the request
  if (typeof(params) == 'string')
    http.setRequestHeader("Content-type", "application/x-www-form-urlencoded");

  //Custom headers
  if (headers) {
    for (key in headers)
      //alert(key + " : " + headers[key]);
      http.setRequestHeader(key, headers[key]);
  }

  http.send(params); 
}


  var defaultMouse;
  var dragMouse; //Global drag tracking

  //Handler class from passed functions
  /**
   * @constructor
   */
  function MouseEventHandler(click, wheel, move, down, up, leave, pinch) {
    //All these functions should take (event, mouse)
    this.click = click;
    this.wheel = wheel;
    this.move = move;
    this.down = down;
    this.up = up;
    this.leave = leave;
    this.pinch = pinch;
  }

  /**
   * @constructor
   */
  function Mouse(element, handler, enableContext) {
    this.element = element;
    //Custom handler for mouse actions...
    //requires members: click(event, mouse), move(event, mouse) and wheel(event, mouse)
    this.handler = handler;

    this.disabled = false;
    this.isdown = false;
    this.button = null;
    this.dragged = false;
    this.x = 0;
    this.x = 0;
    this.absoluteX = 0;
    this.absoluteY = 0;
    this.lastX = 0;
    this.lastY = 0;
    this.slider = null;
    this.spin = 0;
    //Option settings...
    this.moveUpdate = false;  //Save mouse move origin once on mousedown or every move
    this.enableContext = enableContext ? true : false;

    element.addEventListener("onwheel" in document ? "wheel" : "mousewheel", handleMouseWheel, false);
    element.onmousedown = handleMouseDown;
console.log("ELEMENT: " + element.id);
console.log("OMD: " + element.onmousedown);
    element.onmouseout = handleMouseLeave;
    document.onmouseup = handleMouseUp;
    document.onmousemove = handleMouseMove;
    //Touch events! testing...
    element.addEventListener("touchstart", touchHandler, true);
    element.addEventListener("touchmove", touchHandler, true);
    element.addEventListener("touchend", touchHandler, true);
    //To disable context menu
    element.oncontextmenu = function() {return this.mouse.enableContext;}
  }

  Mouse.prototype.setDefault = function() {
    //Sets up this mouse as the default for the document
    //Multiple mouse handlers can be created for elements but only
    //one should be set to handle document events
    defaultMouse = document.mouse = this;
  }

  Mouse.prototype.update = function(e) {
    // Get the mouse position relative to the document.
    if (!e) var e = window.event;
    this.x = e.pageX;
    this.y = e.pageY;

    //Save doc relative coords
    this.absoluteX = this.x;
    this.absoluteY = this.y;
    //Get element offset in document
    var rect = this.element.getBoundingClientRect();
    var offset = [rect.left, rect.top];
    //Convert coords to position relative to element
    this.x -= offset[0];
    this.y -= offset[1];
    //Save position without scrolling, only checked in ff5 & chrome12
    this.clientx = e.clientX - offset[0];
    this.clienty = e.clientY - offset[1];
  }

  function getMouse(event) {
    if (!event) event = window.event; //access the global (window) event object
    var mouse = event.target.mouse;
    if (mouse) return mouse;
    //Attempt to find in parent nodes
    var target = event.target;
    var i = 0;
    while (target != document) {
      target = target.parentNode;
      if (target.mouse) return target.mouse;
    }

    return null;
  }

  function handleMouseDown(event) {
console.log("MOUSE DOWN");
    //Event delegation details
    var mouse = getMouse(event);
    if (!mouse || mouse.disabled) return true;
    var e = event || window.event;
    mouse.target = e.target;
    //Clear dragged flag on mouse down
    mouse.dragged = false;

    mouse.update(event);
    if (!mouse.isdown) {
      mouse.lastX = mouse.absoluteX;
      mouse.lastY = mouse.absoluteY;
    }
    mouse.isdown = true;
    dragMouse = mouse;
    mouse.button = event.button;
    //Set document move & up event handlers to this.mouse object's
    document.mouse = mouse;

    //Handler for mouse down
    var action = true;
    if (mouse.handler.down) action = mouse.handler.down(event, mouse);
    //If handler returns false, prevent default action
    if (!action && event.preventDefault) event.preventDefault();
    return action;
  }

  //Default handlers for up & down, call specific handlers on element
  function handleMouseUp(event) {
    var mouse = document.mouse;
    if (!mouse || mouse.disabled) return true;
    var action = true;
    if (mouse.isdown) 
    {
      mouse.update(event);
      if (mouse.handler.click) action = mouse.handler.click(event, mouse);
      mouse.isdown = false;
      dragMouse = null;
      mouse.button = null;
      mouse.dragged = false;
    }
    if (mouse.handler.up) action = action && mouse.handler.up(event, mouse);
    //Restore default mouse on document
    document.mouse = defaultMouse;

    //If handler returns false, prevent default action
    if (!action && event.preventDefault) event.preventDefault();
    return action;
  }

  function handleMouseMove(event) {
    //Use previous mouse if dragging
    var mouse = dragMouse ? dragMouse : getMouse(event);
    if (!mouse || mouse.disabled) return true;
    mouse.update(event);
    mouse.deltaX = mouse.absoluteX - mouse.lastX;
    mouse.deltaY = mouse.absoluteY - mouse.lastY;
    var action = true;

    //Set dragged flag if moved more than limit
    if (!mouse.dragged && mouse.isdown && Math.abs(mouse.deltaX) + Math.abs(mouse.deltaY) > 3)
      mouse.dragged = true;

    if (mouse.handler.move)
      action = mouse.handler.move(event, mouse);

    if (mouse.moveUpdate) {
      //Constant update of last position
      mouse.lastX = mouse.absoluteX;
      mouse.lastY = mouse.absoluteY;
    }

    //If handler returns false, prevent default action
    if (!action && event.preventDefault) event.preventDefault();
    return action;
  }
 
  function handleMouseWheel(event) {
    var mouse = getMouse(event);
    if (!mouse || mouse.disabled) return true;
    mouse.update(event);
    var action = false; //Default action disabled

    var delta = event.deltaY ? -event.deltaY : event.wheelDelta;
    event.spin = delta > 0 ? 1 : -1;

    if (mouse.handler.wheel) action = mouse.handler.wheel(event, mouse);

    //If handler returns false, prevent default action
    if (!action && event.preventDefault) event.preventDefault();
    return action;
  } 

  function handleMouseLeave(event) {
    var mouse = getMouse(event);
    if (!mouse || mouse.disabled) return true;

    var action = true;
    if (mouse.handler.leave) action = mouse.handler.leave(event, mouse);

    //If handler returns false, prevent default action
    if (!action && event.preventDefault) event.preventDefault();
    event.returnValue = action; //IE
    return action;
  } 

  //Basic touch event handling
  //Based on: http://ross.posterous.com/2008/08/19/iphone-touch-events-in-javascript/
  //Pinch handling all by OK
  function touchHandler(event)
  {
    var touches = event.changedTouches,
        first = touches[0],
        simulate = null,  //Mouse event to simulate
        prevent = false,
        mouse = getMouse(event);

    switch(event.type)
    {
      case "touchstart":
        if (event.touches.length == 2) {
          mouse.isdown = false; //Ignore first pinch touchdown being processed as mousedown
          mouse.scaling = 0;
        } else
          simulate = "mousedown";
        break;
      case "touchmove":
        if (mouse.scaling != null && event.touches.length == 2) {
          var dist = Math.sqrt(
            (event.touches[0].pageX-event.touches[1].pageX) * (event.touches[0].pageX-event.touches[1].pageX) +
            (event.touches[0].pageY-event.touches[1].pageY) * (event.touches[0].pageY-event.touches[1].pageY));

          if (mouse.scaling > 0) {
            event.distance = (dist - mouse.scaling);
            if (mouse.handler.pinch) action = mouse.handler.pinch(event, mouse);
            //If handler returns false, prevent default action
            var action = true;
            if (!action && event.preventDefault) event.preventDefault();  // Firefox
            event.returnValue = action; //IE
          } else
            mouse.scaling = dist;
        } else
          simulate = "mousemove";
        break;
      case "touchend":
        if (mouse.scaling != null) {
          //Pinch sends two touch start/end,
          //only turn off scaling after 2nd touchend
          if (mouse.scaling == 0)
            mouse.scaling = null;
          else
            mouse.scaling = 0;
        } else
          simulate = "mouseup";
        break;
      default:
        return;
    }
    if (event.touches.length > 1) //Avoid processing multiple touch except pinch zoom
      simulate = null;

    //Passes other events on as simulated mouse events
    if (simulate) {
      //OK.debug(event.type + " - " + event.touches.length + " touches");

      //initMouseEvent(type, canBubble, cancelable, view, clickCount, 
      //           screenX, screenY, clientX, clientY, ctrlKey, 
      //           altKey, shiftKey, metaKey, button, relatedTarget);
      var simulatedEvent = document.createEvent("MouseEvent");
      simulatedEvent.initMouseEvent(simulate, true, true, window, 1, 
                                first.screenX, first.screenY, 
                                first.clientX, first.clientY, event.ctrlKey, 
                                event.altKey, event.shiftKey, event.metaKey, 0 /*left*/, null);

      //Prevent default where requested
      prevent = !first.target.dispatchEvent(simulatedEvent);
      event.preventDefault();
    }

    //if (prevent || scaling)
    //  event.preventDefault();

  }


  /**
   * WebGL interface object
   * standard utilities for WebGL 
   * Shader & matrix utilities for 3d & 2d
   * functions for 2d rendering / image processing
   * (c) Owen Kaluza 2012
   */

  /**
   * @constructor
   */
  function Viewport(x, y, width, height) {
    this.x = x; 
    this.y = y; 
    this.width = width; 
    this.height = height; 
  }

  /**
   * @constructor
   */
  function WebGL(canvas, options) {
    this.program = null;
    this.modelView = new ViewMatrix();
    this.perspective = new ViewMatrix();
    this.textures = [];
    this.timer = null;

    if (!window.WebGLRenderingContext) throw "No browser WebGL support";

    // Try to grab the standard context. If it fails, fallback to experimental.
    try {
      this.gl = canvas.getContext("webgl", options) || canvas.getContext("experimental-webgl", options);
    } catch (e) {
      OK.debug("detectGL exception: " + e);
      throw "No context"
    }
    this.viewport = new Viewport(0, 0, canvas.width, canvas.height);
    if (!this.gl) throw "Failed to get context";

  }

  WebGL.prototype.setMatrices = function() {
    //Model view matrix
    this.gl.uniformMatrix4fv(this.program.mvMatrixUniform, false, this.modelView.matrix);
    //Perspective matrix
    this.gl.uniformMatrix4fv(this.program.pMatrixUniform, false, this.perspective.matrix);
    //Normal matrix
    if (this.program.nMatrixUniform) {
      var nMatrix = mat4.create(this.modelView.matrix);
      mat4.inverse(nMatrix);
      mat4.transpose(nMatrix);
      this.gl.uniformMatrix4fv(this.program.nMatrixUniform, false, nMatrix);
    }
  }

  WebGL.prototype.initDraw2d = function() {
    this.gl.viewport(this.viewport.x, this.viewport.y, this.viewport.width, this.viewport.height);

    this.gl.enableVertexAttribArray(this.program.attributes["aVertexPosition"]);
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexPositionBuffer);
    this.gl.vertexAttribPointer(this.program.attributes["aVertexPosition"], this.vertexPositionBuffer.itemSize, this.gl.FLOAT, false, 0, 0);

    if (this.program.attributes["aTextureCoord"]) {
      this.gl.enableVertexAttribArray(this.program.attributes["aTextureCoord"]);
      this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.textureCoordBuffer);
      this.gl.vertexAttribPointer(this.program.attributes["aTextureCoord"], this.textureCoordBuffer.itemSize, this.gl.FLOAT, false, 0, 0);
    }

    this.setMatrices();
  }

  WebGL.prototype.updateTexture = function(texture, image, unit) {
    //Set default texture unit if not provided
    if (unit == undefined) unit = this.gl.TEXTURE0;
    this.gl.activeTexture(unit);
    this.gl.bindTexture(this.gl.TEXTURE_2D, texture);
    this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, this.gl.RGBA, this.gl.UNSIGNED_BYTE, image);
    this.gl.bindTexture(this.gl.TEXTURE_2D, null);
  }

  WebGL.prototype.init2dBuffers = function(unit) {
    //Set default texture unit if not provided
    if (unit == undefined) unit = this.gl.TEXTURE0;
    //All output drawn onto a single 2x2 quad
    this.vertexPositionBuffer = this.gl.createBuffer();
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexPositionBuffer);
    var vertexPositions = [1.0,1.0,  -1.0,1.0,  1.0,-1.0,  -1.0,-1.0];
    this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(vertexPositions), this.gl.STATIC_DRAW);
    this.vertexPositionBuffer.itemSize = 2;
    this.vertexPositionBuffer.numItems = 4;

    //Gradient texture
    this.gl.activeTexture(unit);
    this.gradientTexture = this.gl.createTexture();
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.gradientTexture);

    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);

    //Texture coords
    this.textureCoordBuffer = this.gl.createBuffer();
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.textureCoordBuffer);
    var textureCoords = [1.0, 1.0,  0.0, 1.0,  1.0, 0.0,  0.0, 0.0];
    this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(textureCoords), this.gl.STATIC_DRAW);
    this.textureCoordBuffer.itemSize = 2;
    this.textureCoordBuffer.numItems = 4;
  }

  WebGL.prototype.loadTexture = function(image, filter, type, flip) {
    if (filter == undefined) filter = this.gl.NEAREST;
    if (type == undefined) type = this.gl.RGBA;
    this.texid = this.textures.length;
    this.textures.push(this.gl.createTexture());
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.textures[this.texid]);
    if (flip) this.gl.pixelStorei(this.gl.UNPACK_FLIP_Y_WEBGL, true);
    this.gl.texImage2D(this.gl.TEXTURE_2D, 0, type, type, this.gl.UNSIGNED_BYTE, image);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, filter);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, filter);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
    this.gl.bindTexture(this.gl.TEXTURE_2D, null);
    return this.textures[this.texid];
  }

  WebGL.prototype.setPerspective = function(fovy, aspect, znear, zfar) {
    this.perspective.matrix = mat4.perspective(fovy, aspect, znear, zfar);
  }

  WebGL.prototype.use = function(program) {
    this.program = program;
    if (this.program.program)
      this.gl.useProgram(this.program.program);
  }

  WebGL.prototype.view = function(view) {
    //Setup camera for viewing a model
    //
    //Passed view object requires following properties:
    //
    //fov, near_clip, far_clip
    //translate, focus, centre, rotate, scale, orientation
    
    if (!this.gl) return;

    this.gl.viewport(0, 0, this.gl.viewportWidth, this.gl.viewportHeight);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

    this.setPerspective(view.fov, this.gl.viewportWidth / this.gl.viewportHeight, view.near_clip, view.far_clip);

    //Apply translation to origin, any rotation and scaling (inverse of zoom factor)
    this.modelView.identity()
    this.modelView.translate([view.translate[0], view.translate[1], view.translate[2]])
    
    // Adjust centre of rotation, default is same as focal point so view does nothing...
    var adjust = [-(view.focus[0] - view.centre[0]), -(view.focus[1] - view.centre[1]), -(view.focus[2] - view.centre[2])];
    this.modelView.translate(adjust);

    // rotate model 
    this.modelView.mult(quat4.toMat4(view.rotate));

    // Apply scaling factors
    this.modelView.scale(view.scale);

    // Adjust back for rotation centre
    this.modelView.translate([-adjust[0], -adjust[1], -adjust[2]]);

    // Translate back by centre of model to align eye with model centre
    this.modelView.translate([-view.focus[0], -view.focus[1], -view.focus[2] * view.orientation]);

    //console.log(JSON.stringify(this.modelView));

    // Set orientation scaling and default polygon front faces
    if (view.orientation == 1.0) {
      this.gl.frontFace(view.gl.CCW);
    } else {
      this.gl.frontFace(view.gl.CW);
      this.modelView.scale([1, 1, -1]);
    }
  }

  /**
   * @constructor
   */
  //Program object
  function WebGLProgram(gl, vs, fs) {
    //Can be passed source directly or script tag
    this.program = null;
    if (vs.indexOf("main") < 0) vs = getSourceFromElement(vs);
    if (fs.indexOf("main") < 0) fs = getSourceFromElement(fs);
    //Pass in vertex shader, fragment shaders...
    this.gl = gl;
    if (this.program && this.gl.isProgram(this.program))
    {
      //Clean up previous shader set
      if (this.gl.isShader(this.vshader))
      {
        this.gl.detachShader(this.program, this.vshader);
        this.gl.deleteShader(this.vshader);
      }
      if (this.gl.isShader(this.fshader))
      {
        this.gl.detachShader(this.program, this.fshader);
        this.gl.deleteShader(this.fshader);
      }
      this.gl.deleteProgram(this.program);  //Required for chrome, doesn't like re-using this.program object
    }

    this.program = this.gl.createProgram();

    this.vshader = this.compileShader(vs, this.gl.VERTEX_SHADER);
    this.fshader = this.compileShader(fs, this.gl.FRAGMENT_SHADER);

    this.gl.attachShader(this.program, this.vshader);
    this.gl.attachShader(this.program, this.fshader);

    this.gl.linkProgram(this.program);
 
    if (!this.gl.getProgramParameter(this.program, this.gl.LINK_STATUS)) {
      throw "Could not initialise shaders: " + this.gl.getProgramInfoLog(this.program);
    }
  }

  WebGLProgram.prototype.compileShader = function(source, type) {
    //alert("Compiling " + type + " Source == " + source);
    var shader = this.gl.createShader(type);
    this.gl.shaderSource(shader, source);
    this.gl.compileShader(shader);
    if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS))
      throw this.gl.getShaderInfoLog(shader);
    return shader;
  }

  //Setup and load uniforms
  WebGLProgram.prototype.setup = function(attributes, uniforms, noenable) {
    if (!this.program) return;
    if (attributes == undefined) attributes = ["aVertexPosition", "aTextureCoord"];
    this.attributes = {};
    var i;
    for (i in attributes) {
      this.attributes[attributes[i]] = this.gl.getAttribLocation(this.program, attributes[i]);
      if (!noenable) this.gl.enableVertexAttribArray(this.attributes[attributes[i]]);
    }

    this.uniforms = {};
    for (i in uniforms)
      this.uniforms[uniforms[i]] = this.gl.getUniformLocation(this.program, uniforms[i]);
    this.mvMatrixUniform = this.gl.getUniformLocation(this.program, "uMVMatrix");
    this.pMatrixUniform = this.gl.getUniformLocation(this.program, "uPMatrix");
    this.nMatrixUniform = this.gl.getUniformLocation(this.program, "uNMatrix");
  }

  /**
   * @constructor
   */
  function ViewMatrix() {
    this.matrix = mat4.create();
    mat4.identity(this.matrix);
    this.stack = [];
  }

  ViewMatrix.prototype.toString = function() {
    return JSON.stringify(this.toArray());
  }

  ViewMatrix.prototype.toArray = function() {
    return JSON.parse(mat4.str(this.matrix));
  }

  ViewMatrix.prototype.push = function(m) {
    if (m) {
      this.stack.push(mat4.create(m));
      this.matrix = mat4.create(m);
    } else {
      this.stack.push(mat4.create(this.matrix));
    }
  }

  ViewMatrix.prototype.pop = function() {
    if (this.stack.length == 0) {
      throw "Matrix stack underflow";
    }
    this.matrix = this.stack.pop();
    return this.matrix;
  }

  ViewMatrix.prototype.mult = function(m) {
    mat4.multiply(this.matrix, m);
  }

  ViewMatrix.prototype.identity = function() {
    mat4.identity(this.matrix);
  }

  ViewMatrix.prototype.scale = function(v) {
    mat4.scale(this.matrix, v);
  }

  ViewMatrix.prototype.translate = function(v) {
    mat4.translate(this.matrix, v);
  }

  ViewMatrix.prototype.rotate = function(angle,v) {
    var arad = angle * Math.PI / 180.0;
    mat4.rotate(this.matrix, arad, v);
  }

  /**
   * @constructor
   */
  function Palette(source, premultiply) {
    this.premultiply = premultiply;
    //Default transparent black background
    this.background = new Colour("rgba(0,0,0,0)");
    //Colour palette array
    this.colours = [];
    this.slider = new Image();
    this.slider.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAkAAAAPCAYAAAA2yOUNAAAAj0lEQVQokWNIjHT8/+zZs//Pnj37/+TJk/9XLp/+f+bEwf9HDm79v2Prqv9aKrz/GUYVEaeoMDMQryJXayWIoi0bFmFV1NWS+z/E1/Q/AwMDA0NVcez/LRsWoSia2luOUAADVcWx/xfO6/1/5fLp/1N7y//HhlmhKoCBgoyA/w3Vyf8jgyyxK4CBUF8zDAUAAJRXY0G1eRgAAAAASUVORK5CYII=";

    if (!source) {
      //Default greyscale
      this.colours.push(new ColourPos("rgba(255,255,255,1)", 0));
      this.colours.push(new ColourPos("rgba(0,0,0,1)", 1.0));
      return;
    }

    var calcPositions = false;

    if (typeof(source) == 'string') {
      //Palette string data parser
      var lines = source.split(/[\n;]/); // split on newlines and/or semi-colons
      var position;
      for (var i = 0; i < lines.length; i++) {
        var line = lines[i].trim();
        if (!line) continue;

        //Palette: parse into attrib=value pairs
        var pair = line.split("=");
        if (pair[0] == "Background")
          this.background = new Colour(pair[1]);
        else if (pair[0][0] == "P") //Very old format: PositionX=
          position = parseFloat(pair[1]);
        else if (pair[0][0] == "C") { //Very old format: ColourX=
          //Colour constructor handles html format colours, if no # or rgb/rgba assumes integer format
          this.colours.push(new ColourPos(pair[1], position));
          //Some old palettes had extra colours at end which screws things up so check end position
          if (position == 1.0) break;
        } else if (pair.length == 2) {
          //New style: position=value
          this.colours.push(new ColourPos(pair[1], pair[0]));
        } else {
          //Interpret as colour only, calculate positions
          calcPositions = true;
          this.colours.push(new ColourPos(line));
        }
      }
    } else {
      //JSON colour/position list data
      for (var j=0; j<source.length; j++) {
        //Calculate default positions if none provided
        if (source[j].position == undefined)
          calcPositions = true;
        //Create the entry
        this.colours.push(new ColourPos(source[j].colour, source[j].position));
      }
      //Use background if included
      if (source.background)
        this.background = new Colour(source.background);
    }

    //Calculate default positions
    if (calcPositions) {
      for (var j=0; j<this.colours.length; j++)
        this.colours[j].position = j * (1.0 / (this.colours.length-1));
    }

    //Sort by position (fix out of order entries in old palettes)
    this.sort();

    //Check for all-transparent palette and fix
    var opaque = false;
    for (var c = 0; c < this.colours.length; c++) {
      if (this.colours[c].colour.alpha > 0) opaque = true;
      //Fix alpha=255
      if (this.colours[c].colour.alpha > 1.0)
        this.colours[c].colour.alpha = 1.0;
    }
    if (!opaque) {
      for (var c = 0; c < this.colours.length; c++)
        this.colours[c].colour.alpha = 1.0;
    }
  }

  Palette.prototype.sort = function() {
    this.colours.sort(function(a,b){return a.position - b.position});
  }

  Palette.prototype.newColour = function(position, colour) {
    var col = new ColourPos(colour, position);
    this.colours.push(col);
    this.sort();
    for (var i = 1; i < this.colours.length-1; i++)
      if (this.colours[i].position == position) return i;
    return -1;
  }

  Palette.prototype.inRange = function(pos, range, length) {
    for (var i = 0; i < this.colours.length; i++)
    {
      var x = this.colours[i].position * length;
      if (pos == x || (range > 1 && pos >= x - range / 2 && pos <= x + range / 2))
        return i;
    }
    return -1;
  }

  Palette.prototype.inDragRange = function(pos, range, length) {
    for (var i = 1; i < this.colours.length-1; i++)
    {
      var x = this.colours[i].position * length;
      if (pos == x || (range > 1 && pos >= x - range / 2 && pos <= x + range / 2))
        return i;
    }
    return 0;
  }

  Palette.prototype.remove = function(i) {
    this.colours.splice(i,1);
  }

  Palette.prototype.toString = function() {
    var paletteData = 'Background=' + this.background.html();
    for (var i = 0; i < this.colours.length; i++)
      paletteData += '\n' + this.colours[i].position.toFixed(6) + '=' + this.colours[i].colour.html();
    return paletteData;
  }

  Palette.prototype.get = function() {
    var obj = {};
    obj.background = this.background.html();
    obj.colours = [];
    for (var i = 0; i < this.colours.length; i++)
      obj.colours.push({'position' : this.colours[i].position, 'colour' : this.colours[i].colour.html()});
    return obj;
  }

  Palette.prototype.toJSON = function() {
    return JSON.stringify(this.get());
  }

  //Palette draw to canvas
  Palette.prototype.draw = function(canvas, ui) {
    //Slider image not yet loaded?
    if (!this.slider.width && ui) {
      var _this = this;
      setTimeout(function() { _this.draw(canvas, ui); }, 150);
      return;
    }
    
    // Figure out if a webkit browser is being used
    if (!canvas) {alert("Invalid canvas!"); return;}
    var webkit = /webkit/.test(navigator.userAgent.toLowerCase());

    if (this.colours.length == 0) {
      this.background = new Colour("#ffffff");
      this.colours.push(new ColourPos("#000000", 0));
      this.colours.push(new ColourPos("#ffffff", 1));
    }

    //Colours might be out of order (especially during editing)
    //so save a (shallow) copy and sort it
    list = this.colours.slice(0);
    list.sort(function(a,b){return a.position - b.position});

    if (canvas.getContext) {
      //Draw the gradient(s)
      var width = canvas.width;
      var height = canvas.height;
      var context = canvas.getContext('2d');  
      context.clearRect(0, 0, width, height);

      if (webkit) {
        //Split up into sections or webkit draws a fucking awful gradient with banding
        var x0 = 0;
        for (var i = 1; i < list.length; i++) {
          var x1 = Math.round(width * list[i].position);
          context.fillStyle = context.createLinearGradient(x0, 0, x1, 0);
          var colour1 = list[i-1].colour;
          var colour2 = list[i].colour;
          //Pre-blend with background unless in UI mode
          if (this.premultiply && !ui) {
            colour1 = this.background.blend(colour1);
            colour2 = this.background.blend(colour2);
          }
          context.fillStyle.addColorStop(0.0, colour1.html());
          context.fillStyle.addColorStop(1.0, colour2.html());
          context.fillRect(x0, 0, x1-x0, height);
          x0 = x1;
        }
      } else {
        //Single gradient
        context.fillStyle = context.createLinearGradient(0, 0, width, 0);
        for (var i = 0; i < list.length; i++) {
          var colour = list[i].colour;
          //Pre-blend with background unless in UI mode
          if (this.premultiply && !ui)
            colour = this.background.blend(colour);
          context.fillStyle.addColorStop(list[i].position, colour.html());
        }
        context.fillRect(0, 0, width, height);
      }

      /* Posterise mode (no gradients)
      var x0 = 0;
      for (var i = 1; i < list.length; i++) {
        var x1 = Math.round(width * list[i].position);
        //Pre-blend with background unless in UI mode
        var colour2 = ui ? list[i].colour : this.background.blend(list[i].colour);
        context.fillStyle = colour2.html();
        context.fillRect(x0, 0, x1-x0, height);
        x0 = x1;
      }
      */

      //Background colour
      var bg = document.getElementById('backgroundCUR');
      if (bg) bg.style.background = this.background.html();

      //User interface controls
      if (!ui) return;  //Skip drawing slider interface
      for (var i = 1; i < list.length-1; i++)
      {
        var x = Math.floor(width * list[i].position) + 0.5;
        var HSV = list[i].colour.HSV();
        if (HSV.V > 50)
          context.strokeStyle = "black";
        else
          context.strokeStyle = "white";
        context.beginPath();
        context.moveTo(x, 0);
        context.lineTo(x, canvas.height);
        context.closePath();
        context.stroke();
        x -= (this.slider.width / 2);
        context.drawImage(this.slider, x, 0);  
      } 
    } else alert("getContext failed!");
  }


  /**
   * @constructor
   */
  function ColourPos(colour, pos) {
    //Stores colour as rgba and position as real [0,1]
    if (pos == undefined)
      this.position = 0.0;
    else
      this.position = parseFloat(pos);
    //Detect out of range...
    if (this.position >= 0 && this.position <= 1) {
      if (colour) {
        if (typeof(colour) == 'object')
          this.colour = colour;
        else
          this.colour = new Colour(colour);
      } else {
        this.colour = new Colour("#000000");
      }
    } else {
      throw( "Invalid Colour Position: " + pos);
    }
  }
  
  /**
   * @constructor
   */
  function Colour(colour) {
    //Construct... stores colour as r,g,b,a values
    //Can pass in html colour string, HSV object, Colour object or integer rgba
    if (typeof colour == "undefined")
      this.set("#ffffff")
    else if (typeof(colour) == 'string')
      this.set(colour);
    else if (typeof(colour) == 'object') {
      //Determine passed type, Colour, RGBA or HSV
      if (typeof colour.H != "undefined")
        //HSV
        this.setHSV(colour);
      else if (typeof colour.red != "undefined") {
        //Another Colour object
        this.red = colour.red;
        this.green = colour.green;
        this.blue = colour.blue;
        this.alpha = colour.alpha;
      } else if (colour.R) {
        //RGBA
        this.red = colour.R;
        this.green = colour.G;
        this.blue = colour.B;
        this.alpha = typeof colour.A == "undefined" ? 1.0 : colour.A;
      } else {
        //Assume array
        this.red = colour[0];
        this.green = colour[1];
        this.blue = colour[2];
        //Convert float components to [0-255]
        //NOTE: This was commented, not sure where the problem was
        //Needed for parsing JSON array [0,1] colours
        if (this.red <= 1.0 && this.green <= 1.0 && this.blue <= 1.0) {
          this.red = Math.round(this.red * 255);
          this.green = Math.round(this.green * 255);
          this.blue = Math.round(this.blue * 255);
        }
        this.alpha = typeof colour[3] == "undefined" ? 1.0 : colour[3];
      }
    } else {
      //Convert from integer AABBGGRR
      this.fromInt(colour);
    }
  }

  Colour.prototype.set = function(val) {
    if (!val) val = "#ffffff"; //alert("No Value provided!");
    var re = /^rgba?\((\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,?\s*(\d\.?\d*)?\)$/;
    var bits = re.exec(val);
    if (bits)
    {
      this.red = parseInt(bits[1]);
      this.green = parseInt(bits[2]);
      this.blue = parseInt(bits[3]);
      this.alpha = typeof bits[4] == "undefined" ? 1.0 : parseFloat(bits[4]);

    } else if (val.charAt(0) == "#") {
      var hex = val.substring(1,7);
      this.alpha = 1.0;
      this.red = parseInt(hex.substring(0,2),16);
      this.green = parseInt(hex.substring(2,4),16);
      this.blue = parseInt(hex.substring(4,6),16);
    } else {
      //Attempt to parse as integer
      this.fromInt(parseInt(val));
    }
  }

  Colour.prototype.fromInt = function(intcolour) {
    //Convert from integer AABBGGRR
    this.red = (intcolour&0x000000ff);
    this.green = (intcolour&0x0000ff00) >>> 8;
    this.blue = (intcolour&0x00ff0000) >>> 16;
    this.alpha = ((intcolour&0xff000000) >>> 24) / 255.0;
  }

  Colour.prototype.toInt = function() {
    //Convert to integer AABBGGRR
    var result = this.red;
    result += (this.green << 8);
    result += (this.blue << 16);
    result += (Math.round(this.alpha * 255) << 24);
    return result;
  }

  Colour.prototype.toString = function() {return this.html();}

  Colour.prototype.html = function() {
    return "rgba(" + this.red + "," + this.green + "," + this.blue + "," + this.alpha.toFixed(2) + ")";
  }

  Colour.prototype.rgbaGL = function() {
    var arr = [this.red/255.0, this.green/255.0, this.blue/255.0, this.alpha];
    return new Float32Array(arr);
  }

  Colour.prototype.rgbaGLSL = function() {
    var c = this.rgbaGL();
    return "rgba(" + c[0].toFixed(4) + "," + c[1].toFixed(4) + "," + c[2].toFixed(4) + "," + c[3].toFixed(4) + ")";
  }

  Colour.prototype.rgba = function() {
    var rgba = [this.red/255.0, this.green/255.0, this.blue/255.0, this.alpha];
    return rgba;
  }

  Colour.prototype.rgbaObj = function() {
  //OK.debug('R:' + this.red + ' G:' + this.green + ' B:' + this.blue + ' A:' + this.alpha);
    return({'R':this.red, 'G':this.green, 'B':this.blue, 'A':this.alpha});
  }

  Colour.prototype.print = function() {
    OK.debug(this.printString(true));
  }

  Colour.prototype.printString = function(alpha) {
    return 'R:' + this.red + ' G:' + this.green + ' B:' + this.blue + (alpha ? ' A:' + this.alpha : '');
  }

  Colour.prototype.HEX = function(o) {
     o = Math.round(Math.min(Math.max(0,o),255));
     return("0123456789ABCDEF".charAt((o-o%16)/16)+"0123456789ABCDEF".charAt(o%16));
   }

  Colour.prototype.htmlHex = function(o) { 
    return("#" + this.HEX(this.red) + this.HEX(this.green) + this.HEX(this.blue)); 
  };

  Colour.prototype.hex = function(o) { 
    //hex RGBA in expected order
    return(this.HEX(this.red) + this.HEX(this.green) + this.HEX(this.blue) + this.HEX(this.alpha*255)); 
  };

  Colour.prototype.hexGL = function(o) { 
    //RGBA for openGL (stored ABGR internally on little endian)
    return(this.HEX(this.alpha*255) + this.HEX(this.blue) + this.HEX(this.green) + this.HEX(this.red)); 
  };

  Colour.prototype.setHSV = function(o)
  {
    var R, G, A, B, C, S=o.S/100, V=o.V/100, H=o.H/360;

    if(S>0) { 
      if(H>=1) H=0;

      H=6*H; F=H-Math.floor(H);
      A=Math.round(255*V*(1-S));
      B=Math.round(255*V*(1-(S*F)));
      C=Math.round(255*V*(1-(S*(1-F))));
      V=Math.round(255*V); 

      switch(Math.floor(H)) {
          case 0: R=V; G=C; B=A; break;
          case 1: R=B; G=V; B=A; break;
          case 2: R=A; G=V; B=C; break;
          case 3: R=A; G=B; B=V; break;
          case 4: R=C; G=A; B=V; break;
          case 5: R=V; G=A; B=B; break;
      }

      this.red = R ? R : 0;
      this.green = G ? G : 0;
      this.blue = B ? B : 0;
    } else {
      this.red = (V=Math.round(V*255));
      this.green = V;
      this.blue = V;
    }
    this.alpha = typeof o.A == "undefined" ? 1.0 : o.A;
  }

  Colour.prototype.HSV = function() {
    var r = ( this.red / 255.0 );                   //RGB values = 0 ÷ 255
    var g = ( this.green / 255.0 );
    var b = ( this.blue / 255.0 );

    var min = Math.min( r, g, b );    //Min. value of RGB
    var max = Math.max( r, g, b );    //Max. value of RGB
    deltaMax = max - min;             //Delta RGB value

    var v = max;
    var s, h;
    var deltaRed, deltaGreen, deltaBlue;

    if ( deltaMax == 0 )                     //This is a gray, no chroma...
    {
       h = 0;                               //HSV results = 0 ÷ 1
       s = 0;
    }
    else                                    //Chromatic data...
    {
       s = deltaMax / max;

       deltaRed = ( ( ( max - r ) / 6 ) + ( deltaMax / 2 ) ) / deltaMax;
       deltaGreen = ( ( ( max - g ) / 6 ) + ( deltaMax / 2 ) ) / deltaMax;
       deltaBlue = ( ( ( max - b ) / 6 ) + ( deltaMax / 2 ) ) / deltaMax;

       if      ( r == max ) h = deltaBlue - deltaGreen;
       else if ( g == max ) h = ( 1 / 3 ) + deltaRed - deltaBlue;
       else if ( b == max ) h = ( 2 / 3 ) + deltaGreen - deltaRed;

       if ( h < 0 ) h += 1;
       if ( h > 1 ) h -= 1;
    }

    return({'H':360*h, 'S':100*s, 'V':v*100});
  }

  Colour.prototype.HSVA = function() {
    var hsva = this.HSV();
    hsva.A = this.alpha;
    return hsva;
  }

  Colour.prototype.interpolate = function(other, lambda) {
    //Interpolate between this colour and another by lambda
    this.red = Math.round(this.red + lambda * (other.red - this.red));
    this.green = Math.round(this.green + lambda * (other.green - this.green));
    this.blue = Math.round(this.blue + lambda * (other.blue - this.blue));
    this.alpha = Math.round(this.alpha + lambda * (other.alpha - this.alpha));
  }

  Colour.prototype.blend = function(src) {
    //Blend this colour with another and return result (uses src alpha from other colour)
    return new Colour([
      Math.round((1.0 - src.alpha) * this.red + src.alpha * src.red),
      Math.round((1.0 - src.alpha) * this.green + src.alpha * src.green),
      Math.round((1.0 - src.alpha) * this.blue + src.alpha * src.blue),
      (1.0 - src.alpha) * this.alpha + src.alpha * src.alpha
    ]);
  }

/* JavaScript colour picker with opacity, (c) Owen Kaluza, Public Domain
 * Depends on: utils.js, colours.js
 * */

/**
 * Draggable window class *
 * @constructor
 */
function MoveWindow(id) {
  //Mouse processing:
  if (!id) return;
  this.element = document.getElementById(id);
  if (!this.element) {alert("No such element: " + id); return null;}
  this.mouse = new Mouse(this.element, this);
  this.mouse.moveUpdate = true;
  this.element.mouse = this.mouse;
}

MoveWindow.prototype.open = function(x, y) {
  //Show the window
  var style = this.element.style;

  if (x<0) x=0;
  if (y<0) y=0;
  if (x != undefined) style.left = x + "px";
  if (y != undefined) style.top = y + "px";
  style.display = 'block';

  //Correct if outside window width/height
  var w = this.element.offsetWidth,
      h = this.element.offsetHeight;
  if (x + w > window.innerWidth - 20)
    style.left=(window.innerWidth - w - 20) + 'px';
  if (y + h > window.innerHeight - 20)
    style.top=(window.innerHeight - h - 20) + 'px';
  //console.log("Open " + this.element.id + " " + style.left + "," + style.top + " : " + style.display);
}

MoveWindow.prototype.close = function() {
  this.element.style.display = 'none';
}

MoveWindow.prototype.move = function(e, mouse) {
  //console.log("Move: " + mouse.isdown);
  if (!mouse.isdown) return;
  if (mouse.button > 0) return; //Process left drag only
  //Drag position
  var style = mouse.element.style;
  style.left = parseInt(style.left) + mouse.deltaX + 'px';
  style.top = parseInt(style.top) + mouse.deltaY + 'px';
}

MoveWindow.prototype.down = function(e, mouse) {
  //Prevents drag/selection
  return false;
}

function scale(val, range, min, max) {return clamp(max * val / range, min, max);}
function clamp(val, min, max) {return Math.max(min, Math.min(max, val));}

/**
 * @constructor
 */
function ColourPicker(savefn, abortfn) {
  // Originally based on :
  // DHTML Color Picker, Programming by Ulyses, ColorJack.com (Creative Commons License)
  // http://www.dynamicdrive.com/dynamicindex11/colorjack/index.htm
  // (Stripped down, clean class based interface no IE6 support for HTML5 browsers only)

  //Check for existing instance
  var exists = document.getElementById('picker');
  if (exists && exists.picker) {
    exists.picker.savefn = savefn;
    exists.picker.abortfn = abortfn;
    return exists.picker;
  }

  function createDiv(id, inner, styles) {
    var div = document.createElement("div");
    div.id = id;
    if (inner) div.innerHTML = inner;
    if (styles) div.style.cssText = styles;

    return div;
  }

  var parentElement = document.body;
  //Images
  var checkimg = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAIElEQVQ4jWP4TwAcOHAAL2YYNWBYGEBIASEwasCwMAAALvidroqDalkAAAAASUVORK5CYII="
  var slideimg = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB0AAAAFCAYAAAC5Fuf5AAAAKklEQVQokWP4////fwY6gv////9n+A8F9LIQxVJaW4xiz4D5lB4WIlsMAPjER7mTpG/OAAAAAElFTkSuQmCC"
  var pickimg = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAkAAAAJCAYAAADgkQYQAAAALUlEQVQYlWNgQAX/kTBW8B8ZYFMIk0ARQFaIoQCbQuopIspNRPsOrpABSzgBAFHzU61KjdKlAAAAAElFTkSuQmCC";
  var svimg = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAEG0lEQVQ4jQEQBO/7APz8/Pz7+/vx+/v75Pr6+tb6+vrF+Pj4tPf396H4+PiO9/f3e/X19Wfz8/NU8PDwQuvr6zLi4uIjzs7OFZmZmQoA8PDw/O/v7/Ht7e3l7Ozs2Ozs7Mjq6uq35ubmpeXl5ZLf39+A3NzcbtXV1VvMzMxLvr6+O6ioqCyEhIQfQEBAFADk5OT84eHh8uDg4Obe3t7Z3Nzcy9nZ2brV1dWq0NDQmcrKyofCwsJ2uLi4ZKqqqlSYmJhFfX19N1lZWSsnJychANPT0/zT09Pz0NDQ6c3NzdzKysrNx8fHv8DAwK+6urqfsrKyj6mpqX+cnJxvjIyMX3l5eVBeXl5EPz8/ORsbGy8Aw8PD/MHBwfS+vr7qurq63ra2ttKxsbHErKystaOjo6eampqXj4+PiYODg3lycnJrXl5eX0hISFIuLi5IEBAQPwCwsLD9r6+v9aysrOynp6fioqKi1p2dncmVlZW8jo6OroODg6F5eXmUa2trhl1dXXlLS0ttNzc3YiIiIlkNDQ1RAJ6env2bm5v2l5eX7pSUlOWPj4/aiIiIz4GBgcN5eXm3cHBwq2RkZJ5XV1eSSkpKhzk5OX0qKipzGBgYawgICGMAioqK/YeHh/eDg4PvgICA6Hp6et90dHTVbW1ty2VlZcBcXFy1UVFRqkZGRqA6OjqWLS0tjSEhIYQSEhJ9BgYGdwB2dnb+c3Nz+HFxcfJra2vrZmZm42JiYttaWlrRUlJSyUtLS79CQkK2Nzc3rS0tLaQiIiKdGBgYlQ4ODo8EBASKAGNjY/5gYGD5XV1d9FpaWu5VVVXnTk5O4UlJSdlCQkLRPDw8yTQ0NMEqKiq7IiIisxkZGa0RERGmCgoKoQMDA5wAUFBQ/k9PT/pKSkr3R0dH8kNDQ+w+Pj7mOTk54DMzM9otLS3TJycnzSAgIMgZGRnBExMTvA0NDbcHBweyAwMDrwA9PT3+PDw8+zo6Ovg2Njb0MzMz8DAwMOwqKirnJSUl4iEhId4cHBzYFxcX1BISEtAODg7KCQkJxwQEBMQBAQHBAC0tLf4rKyv9Kioq+iYmJvclJSX0ISEh8R4eHu4aGhrqFhYW5xMTE+MQEBDgDQ0N3AgICNkGBgbWBAQE0wAAANEAHh4e/h0dHf0bGxv7Ghoa+hgYGPcWFhb2FBQU8xEREfEPDw/uDAwM7AoKCuoICAjoBgYG5gMDA+MBAQHiAAAA4QARERH+EBAQ/g8PD/0NDQ38DQ0N+wsLC/kKCgr4CAgI9wcHB/YFBQX0BAQE8wICAvIBAQHwAQEB7wAAAO8AAADuAAUFBf4FBQX+BAQE/gQEBP4DAwP+AwMD/QMDA/0CAgL8AQEB/AEBAfsAAAD7AAAA+wAAAPoAAAD6AAAA+QAAAPmq2NbsCl2m4wAAAABJRU5ErkJggg=="

  var checked = 'background-image: url("' + checkimg + '");';
  var slider = 'cursor: crosshair; float: left; height: 170px; position: relative; width: 19px; padding: 0;' + checked;
  var sliderControl = 'top: 0px; left: -5px; background: url("' + slideimg + '"); height: 5px; width: 29px; position: absolute; ';
  var sliderBG = 'position: relative;';

  this.element = createDiv("picker", null, "display:none; top: 58px; z-index: 20; background: #0d0d0d; color: #aaa; cursor: move; font-family: arial; font-size: 11px; padding: 7px 10px 11px 10px; position: fixed; width: 248px; border-radius: 5px; border: 1px solid #444;");
  var bg = createDiv("pickCURBG", null, checked + " float: left; width: 12px; height: 12px; margin-right: 3px;");
    bg.appendChild(createDiv("pickCUR", null, "float: left; width: 12px; height: 12px; background: #fff; margin-right: 3px;"));
  this.element.appendChild(bg);
  var rgb = createDiv("pickRGB", "R: 255 G: 255 B: 255", "float: left; position: relative; top: -1px;");
  rgb.onclick = "colours.picker.updateString()";
  this.element.appendChild(rgb);
  this.element.appendChild(createDiv("pickCLOSE", "X", "float: right; cursor: pointer; margin: 0 8px 3px;"));
  this.element.appendChild(createDiv("pickOK", "OK", "float: right; cursor: pointer; margin: 0 8px 3px;"));
  var sv = createDiv("SV", null, "position: relative; cursor: crosshair; float: left; height: 170px; width: 170px; margin-right: 10px; background: url('" + svimg +"') no-repeat; background-size: 100%;");
    sv.appendChild(createDiv("SVslide", null, "background: url('" + pickimg +"'); height: 9px; width: 9px; position: absolute; cursor: crosshair"));
  this.element.appendChild(sv);
  var h = createDiv("H", null, slider);
    h.appendChild(createDiv("Hmodel", null, sliderBG));
    h.appendChild(createDiv("Hslide", null, sliderControl));
  this.element.appendChild(h);
  var o = createDiv("O", null, slider + "border: 1px solid #888; left: 9px;");
    o.appendChild(createDiv("Omodel", null, sliderBG));
    o.appendChild(createDiv("Oslide", null, sliderControl));
  this.element.appendChild(o);
  parentElement.appendChild(this.element);

  /* Hover rules require appending to stylesheet */
  var css = '#pickRGB:hover {color: #FFD000;} #pickCLOSE:hover {color: #FFD000;} #pickOK:hover {color: #FFD000;}';
  var style = document.createElement('style');
  if (style.styleSheet)
      style.styleSheet.cssText = css;
  else 
      style.appendChild(document.createTextNode(css));
  document.getElementsByTagName('head')[0].appendChild(style);

  // call base class constructor
  MoveWindow.call(this, "picker"); 

  this.savefn = savefn;
  this.abortfn = abortfn;
  this.size = 170.0; //H,S & V range in pixels
  this.sv = 5;   //Half size of SV selector
  this.oh = 2;   //Half size of H & O selectors
  this.picked = {H:360, S:100, V:100, A:1.0};
  this.max = {'H':360,'S':100,'V':100, 'A':1.0};
  this.colour = new Colour();

  //Load hue strip
  var i, html='', bgcol, opac;
  for(i=0; i<=this.size; i++) { 
    bgcol = new Colour({H:Math.round((360/this.size)*i), S:100, V:100, A:1.0});
    html += "<div class='hue' style='height: 1px; width: 19px; margin: 0; padding: 0; background: " + bgcol.htmlHex()+";'> <\/div>"; 
  }
  document.getElementById('Hmodel').innerHTML = html;

  //Load alpha strip
  html='';
  for(i=0; i<=this.size; i++) {
    opac=1.0-i/this.size;
    html += "<div class='opacity' style='height: 1px; width: 19px; margin: 0; padding: 0; background: #000;opacity: " + opac.toFixed(2) + ";'> <\/div>"; 
  }
  document.getElementById('Omodel').innerHTML = html;

  //Save the class to element for re-use
  this.element.picker = this;
}

//Inherits from MoveWindow
ColourPicker.prototype = new MoveWindow;
ColourPicker.prototype.constructor = MoveWindow;

ColourPicker.prototype.pick = function(colour, x, y) {
  //Show the picker, with selected colour
  this.update(colour.HSVA());
  if (this.element.style.display == 'block') return;
  MoveWindow.prototype.open.call(this, x, y);
}

ColourPicker.prototype.select = function(element, x, y) {
  if (!x || !y) {
    var rect = element.getBoundingClientRect();
    x = x ? x : rect.left+32;
    y = y ? y : rect.top+32;
  }
  var colour = new Colour(element.style.backgroundColor);
  //Show the picker, with selected colour
  this.update(colour.HSVA());
  if (this.element.style.display == 'block') return;
  MoveWindow.prototype.open.call(this, x, y);
  this.target = element;
}

//Mouse event handling
ColourPicker.prototype.click = function(e, mouse) {
  if (mouse.target.id == "pickCLOSE") {
    if (this.abortfn) this.abortfn();
    toggle('picker'); 
  } else if (mouse.target.id == "pickOK") {
    if (this.savefn)
      this.savefn(this.picked);

    //Set element background
    if (this.target) {
      var colour = new Colour(this.picked);
      this.target.style.backgroundColor = colour.html();
    }

    toggle('picker'); 
  } else if (mouse.target.id == 'SV') 
    this.setSV(mouse);
  else if (mouse.target.id == 'Hslide' || mouse.target.className == 'hue')
    this.setHue(mouse);
  else if (mouse.target.id == 'Oslide' || mouse.target.className == 'opacity')
    this.setOpacity(mouse);
}

ColourPicker.prototype.move = function(e, mouse) {
  //Process left drag
  if (mouse.isdown && mouse.button == 0) {
    if (mouse.target.id == 'picker' || mouse.target.id == 'pickCUR' || mouse.target.id == 'pickRGB') {
    //Call base class function
    MoveWindow.prototype.move.call(this, e, mouse);
    } else if (mouse.target) {
      //Drag on H/O slider acts as click
      this.click(e, mouse);
    }
  }
}

ColourPicker.prototype.wheel = function(e, mouse) {
  this.incHue(-e.spin);
}

ColourPicker.prototype.setSV = function(mouse) {
  var X = mouse.clientx - parseInt(document.getElementById('SV').offsetLeft),
      Y = mouse.clienty - parseInt(document.getElementById('SV').offsetTop);
  //Saturation & brightness adjust
  this.picked.S = scale(X, this.size, 0, this.max['S']);
  this.picked.V = this.max['V'] - scale(Y, this.size, 0, this.max['V']);
  this.update(this.picked);
}

ColourPicker.prototype.setHue = function(mouse) {
  var X = mouse.clientx - parseInt(document.getElementById('H').offsetLeft),
      Y = mouse.clienty - parseInt(document.getElementById('H').offsetTop);
  //Hue adjust
  this.picked.H = scale(Y, this.size, 0, this.max['H']);
  this.update(this.picked);
}

ColourPicker.prototype.incHue = function(inc) {
  //Hue adjust incrementally
  this.picked.H += inc;
  this.picked.H = clamp(this.picked.H, 0, this.max['H']);
  this.update(this.picked);
}

ColourPicker.prototype.setOpacity = function(mouse) {
  var X = mouse.clientx - parseInt(document.getElementById('O').offsetLeft),
      Y = mouse.clienty - parseInt(document.getElementById('O').offsetTop);
  //Alpha adjust
  this.picked.A = 1.0 - clamp(Y / this.size, 0, 1);
  this.update(this.picked);
}

ColourPicker.prototype.updateString = function(str) {
  if (!str) str = prompt('Edit colour:', this.colour.html());
  if (!str) return;
  this.colour = new Colour(str);
  this.update(this.colour.HSV());
}

ColourPicker.prototype.update = function(HSV) {
  this.picked = HSV;
  this.colour = new Colour(HSV),
      rgba = this.colour.rgbaObj(),
      rgbaStr = this.colour.html(),
      bgcol = new Colour({H:HSV.H, S:100, V:100, A:255});

  document.getElementById('pickRGB').innerHTML=this.colour.printString();
  document.getElementById('pickCUR').style.background=rgbaStr;
  document.getElementById('pickCUR').style.backgroundColour=rgbaStr;
  document.getElementById('SV').style.backgroundColor=bgcol.htmlHex();

  //Hue adjust
  document.getElementById('Hslide').style.top = this.size * (HSV.H/360.0) - this.oh + 'px';
  //SV adjust
  document.getElementById('SVslide').style.top = Math.round(this.size - this.size*(HSV.V/100.0) - this.sv) + 'px';
  document.getElementById('SVslide').style.left = Math.round(this.size*(HSV.S/100.0) - this.sv) + 'px';
  //Alpha adjust
  document.getElementById('Oslide').style.top = this.size * (1.0-HSV.A) - this.oh - 1 + 'px';
};



/**
 * @constructor
 */
function GradientEditor(canvas, callback, premultiply, nopicker, scrollable) {
  this.canvas = canvas;
  this.callback = callback;
  this.premultiply = premultiply;
  this.changed = true;
  this.inserting = false;
  this.editing = null;
  this.element = null;
  this.spin = 0;
  this.scrollable = scrollable;
  var self = this;
  function saveColour(val) {self.save(val);}
  function abortColour() {self.cancel();}
  if (!nopicker)
    this.picker = new ColourPicker(this.save.bind(this), this.cancel.bind(this));

  //Create default palette object (enable premultiply if required)
  this.palette = new Palette(null, premultiply);
  //Event handling for palette
  this.canvas.mouse = new Mouse(this.canvas, this);
  this.canvas.oncontextmenu="return false;";
  this.canvas.oncontextmenu = function() { return false; }      

  //this.update();
}

//Palette management
GradientEditor.prototype.read = function(source) {
  //Read a new palette from source data
  this.palette = new Palette(source, this.premultiply);
  this.reset();
  this.update(true);
}

GradientEditor.prototype.update = function(nocallback) {
  //Redraw and flag change
  this.changed = true;
  this.palette.draw(this.canvas, true);
  //Trigger callback if any
  if (!nocallback && this.callback) this.callback(this);
}

//Draw gradient to passed canvas if data has changed
//If no changes, return false
GradientEditor.prototype.get = function(canvas, cache) {
  if (cache && !this.changed) return false;
  this.changed = false;
  //Update passed canvas
  this.palette.draw(canvas, false);
  return true;
}

GradientEditor.prototype.insert = function(position, x, y) {
  //Flag unsaved new colour
  this.inserting = true;
  var col = new Colour();
  this.editing = this.palette.newColour(position, col)
  //this.update();
  //Edit new colour
  this.picker = new ColourPicker(this.save.bind(this), this.cancel.bind(this));
  this.picker.pick(col, x, y);
}

GradientEditor.prototype.editBackground = function(element) {
  this.editing = -1;
  var rect = element.getBoundingClientRect();
  var offset = [rect.left, rect.top];
  this.element = element;
  this.picker = new ColourPicker(this.save.bind(this), this.cancel.bind(this));
  this.picker.pick(this.palette.background, offset[0]+32, offset[1]+32);
}

GradientEditor.prototype.edit = function(val, x, y) {
  this.picker = new ColourPicker(this.save.bind(this), this.cancel.bind(this));
  if (typeof(val) == 'number') {
    this.editing = val;
    this.picker.pick(this.palette.colours[val].colour, x, y);
  } else if (typeof(val) == 'object') {
    //Edit element
    this.cancel();  //Abort any current edit first
    this.element = val;
    var col = new Colour(val.style.backgroundColor)
    var rect = val.getBoundingClientRect();
    var offset = [rect.left, rect.top];
    this.picker.pick(col, offset[0]+32, offset[1]+32);
  }
  this.update();
}

GradientEditor.prototype.save = function(val) {
  if (this.editing != null) {
    if (this.editing >= 0)
      //Update colour with selected
      this.palette.colours[this.editing].colour.setHSV(val);
    else
      //Update background colour with selected
      this.palette.background.setHSV(val);
  }
  if (this.element) {
    var col = new Colour(0);
    col.setHSV(val);
    this.element.style.backgroundColor = col.html();
    if (this.element.onchange) this.element.onchange();  //Call change function
  }
  this.reset();
  this.update();
}

GradientEditor.prototype.cancel = function() {
  //If aborting a new colour add, delete it
  if (this.editing >= 0 && this.inserting)
    this.palette.remove(this.editing);
  this.reset();
  this.update();
}

GradientEditor.prototype.reset = function() {
  //Reset editing data
  this.inserting = false;
  this.editing = null;
  this.element = null;
}

//Mouse event handling
GradientEditor.prototype.click = function(event, mouse) {
  //this.changed = true;
  if (event.ctrlKey) {
    //Flip
    for (var i = 0; i < this.palette.colours.length; i++)
      this.palette.colours[i].position = 1.0 - this.palette.colours[i].position;
    this.update();
    return false;
  }

  //Use non-scrolling position
  if (!this.scrollable) mouse.x = mouse.clientx;

  if (mouse.slider != null)
  {
    //Slider moved, update texture
    mouse.slider = null;
    this.palette.sort(); //Fix any out of order colours
    this.update();
    return false;
  }
  var pal = this.canvas;
  if (pal.getContext){
    this.cancel();  //Abort any current edit first
    var context = pal.getContext('2d'); 
    var rect = pal.getBoundingClientRect();
    var ypos = rect[1]+30;

    //Get selected colour
    //In range of a colour pos +/- 0.5*slider width?
    var i = this.palette.inRange(mouse.x, this.palette.slider.width, pal.width);
    if (i >= 0) {
      if (event.button == 0) {
        //Edit colour on left click
        this.edit(i, event.clientX-128, ypos);
      } else if (event.button == 2) {
        //Delete on right click
        this.palette.remove(i);
        this.update();
      }
    } else {
      //Clicked elsewhere, add new colour
      this.insert(mouse.x / pal.width, event.clientX-128, ypos);
    }
  }
  return false;
}

GradientEditor.prototype.down = function(event, mouse) {
   return false;
}

GradientEditor.prototype.move = function(event, mouse) {
  if (!mouse.isdown) return true;

  //Use non-scrolling position
  if (!this.scrollable) mouse.x = mouse.clientx;

  if (mouse.slider == null) {
    //Colour slider dragged on?
    var i = this.palette.inDragRange(mouse.x, this.palette.slider.width, this.canvas.width);
    if (i>0) mouse.slider = i;
  }

  if (mouse.slider == null)
    mouse.isdown = false; //Abort action if not on slider
  else {
    if (mouse.x < 1) mouse.x = 1;
    if (mouse.x > this.canvas.width-1) mouse.x = this.canvas.width-1;
    //Move to adjusted position and redraw
    this.palette.colours[mouse.slider].position = mouse.x / this.canvas.width;
    this.update(true);
  }
}

GradientEditor.prototype.wheel = function(event, mouse) {
  if (this.timer)
    clearTimeout(this.timer);
  else
    this.canvas.style.cursor = "wait";
  this.spin += 0.01 * event.spin;
  //this.cycle(0.01 * event.spin);
  var this_ = this;
  this.timer = setTimeout(function() {this_.cycle(this_.spin); this_.spin = 0;}, 150);
}

GradientEditor.prototype.leave = function(event, mouse) {
}

GradientEditor.prototype.cycle = function(inc) {
  this.canvas.style.cursor = "default";
  this.timer = null;
  //Shift all colours cyclically
  for (var i = 1; i < this.palette.colours.length-1; i++)
  {
    var x = this.palette.colours[i].position;
    x += inc;
    if (x <= 0) x += 1.0;
    if (x >= 1.0) x -= 1.0;
    this.palette.colours[i].position = x;
  }
  this.palette.sort(); //Fix any out of order colours
  this.update();
}


