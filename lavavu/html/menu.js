/* dat.gui menu */

function parseColour(input) {
  if (typeof(input) != 'object') {
    //var div = document.createElement('div');
    var div = document.getElementById('hidden_style_div');
    div.style.color = input;
    c = getComputedStyle(div).color;
    o = getComputedStyle(div).opacity;
    C = new Colour(c);
    //c.alpha = o;
    //toFixed() / 1 rounds to fixed position and removes trailing zeros
    c.alpha = parseFloat(o).toFixed(2) / 1;
  } else {
    C = new Colour(input);
  }
  return C.html();
}

//Attempts to get range from data/volume objects for range controls
function menu_getrange(obj, ctrl) {
    //Ranged?
    if (ctrl.length > 1 && ctrl[1].length == 3) {
      var range = [ctrl[1][0], ctrl[1][1]];
      //Mapped range?
      if (ctrl[1][0] === ctrl[1][1] && ctrl[1][0] === 0.0) {
        //Try volume range
        if (obj.volume && obj.volume.minimum < obj.volume.maximum)
          range = [obj.volume.minimum, obj.volume.maximum];
        //Or value data range
        else if (obj.values && obj.values.minimum < obj.values.maximum)
          range = [obj.values.minimum, obj.values.maximum];
      }
      if (range[1] <= range[0]) {
        //console.log(obj.name + " : range (invalid) = " + range);
        return [0.0, 1.0]; //No valid range data, use a default
      }
      //console.log(obj.name + " : range = " + range);
      return range;
    }
    return null;
}

//Add controls to menu, using property metadata
function menu_addctrl(menu, obj, viewer, prop, changefn) {
  //TODO: implement delayed high quality render for faster interaction
  //var changefn = function(value) {viewer.delayedRender(250);};
  var ctrl = viewer.dict[prop].control;

  //Query prop dict for data type, default, min/max/step etc
  var dtype = viewer.dict[prop].type;
  if (prop === 'colourmap') {
    var maps = ['']
    for (var i=0; i<viewer.vis.colourmaps.length; i++)
      maps.push(viewer.vis.colourmaps[i].name);
    //console.log(JSON.stringify(maps));
    menu.add(obj, prop, maps).onFinishChange(changefn);

  } else if (ctrl.length > 2 && ctrl[2] != null) {
    //Select from list of options
    menu.add(obj, prop, ctrl[2]).onFinishChange(changefn);

  } else if ((dtype.indexOf('real') >= 0 || dtype.indexOf('integer') >= 0) && typeof(obj[prop]) == 'number') {
    //Ranged?
    var range = menu_getrange(obj, ctrl);
    //console.log("GOT RANGE " + prop + " " + JSON.stringify(range));
    if (range) {
      //Check value within range
      if (obj[prop] < range[0]) obj[prop] = range[0];
      if (obj[prop] > range[1]) obj[prop] = range[1];
      menu.add(obj, prop, range[0], range[1], ctrl[1][2]).onFinishChange(changefn);
    } else
      menu.add(obj, prop).onFinishChange(changefn);
  } else if (dtype === 'string' || dtype === 'boolean') {
    menu.add(obj, prop).onFinishChange(changefn);
  } else if (dtype === 'boolean') {
    menu.add(obj, prop).onFinishChange(changefn);
  } else if (dtype === 'colour') {
    try {
      //Convert to html colour first
      obj[prop] = parseColour(obj[prop]);
      menu.addColor(obj, prop).onChange(changefn);
    } catch(e) {
      console.log(e);
    }
  }
}

function menu_addctrls(menu, obj, viewer, onchange) {
  var submenu = menu.addFolder(' ... ');
  var submenus = {};
  //var typelist = ['all', 'global', 'view', 'object', 'object(point)', 'object(line)', 'object(volume)', 'object(surface)', 
  //'object(vector)', 'object(tracer)', 'object(shape)', 'colourbar', 'colourmap'];
  var typelist = ['all', 'global', 'view', 'object', 'object(point)', 'object(line)', 'object(volume)', 'object(surface)', 'colourmap'];
  for (var t in typelist)
    submenus[typelist[t]] = submenu.addFolder(typelist[t]);

  //Loop through every available property
  for (var prop in viewer.dict) {

    //Check if it is enabled in GUI
    var ctrl = viewer.dict[prop].control;
    if (!ctrl || ctrl[0] === false) continue; //Control disabled

    //Check if it has been set on the target object
    if (prop in obj) {
      //console.log(prop + " ==> " + JSON.stringify(viewer.dict[prop]));
      menu_addctrl(menu, obj, viewer, prop, onchange);

    //If not, add to a sub-menu that lists all unused properties for all objects
    //When clicked, the property wil lbe moved to the object and activated for editing
    } else {
      //Add to sub-menu
      //  obj[prop] = viewer.dict[prop]["default"]; //Have to set to default for dat.gui
      var mnu = submenus[viewer.dict[prop]["target"]];
      if (mnu) {
        //console.log(prop + " ==> " + JSON.stringify(viewer.dict[prop]));
        var obj2 = {};
        obj2[prop] = function() {
          this.ref[this.property] = viewer.dict[this.property]["default"];
          //Remove this entry
          this.menu.remove(this.controller);
          //Add property controller
          menu_addctrl(this.parentMenu, this.ref, viewer, this.property, onchange);
        };
        obj2.ref = obj;
        obj2.property = prop;
        obj2.menu = mnu;
        obj2.parentMenu = menu;
        obj2.controller = mnu.add(obj2, prop);
      }
    }
  }
}


function menu_addcmaps(menu, obj, viewer, onchange) {
  //Colourmap editing menu

  //Loop through every available property
  for (var prop in viewer.dict) {
    //Check if it is enabled in GUI
    var ctrl = viewer.dict[prop].control;
    if (!ctrl || ctrl[0] === false) continue; //Control disabled

    //Check if it applies to colourmap object
    if (viewer.dict[prop]["target"] == 'colourmap') {
      //console.log(prop + " ==> " + JSON.stringify(viewer.dict[prop]));
      //Need to add property default if not set
      if (!obj[prop])
        obj[prop] = viewer.dict[prop].default;
      menu_addctrl(menu, obj, viewer, prop, onchange);
    }
  }

  //Loop through colours
  var sm1 = menu.addFolder("Colours");
  sm1.add({"Add Colour" : function() {obj.colours.unshift({"colour" : "rgba(0,0,0,1)", "position" : 0.0}); viewer.gui.close(); onchange(); }}, "Add Colour");
  var sm2 = menu.addFolder("Positions");
  //Need to add delete buttons in closure to get correct pos/index
  function del_btn(pos) {sm2.add({"Delete" : function() {obj.colours.splice(pos, 1); viewer.gui.close(); onchange(); }}, 'Delete').name('Delete ' + pos);}
  for (var c in obj.colours) {
    var o = obj.colours[c];
    //Convert to html colour first
    o.colour = parseColour(o.colour);
    sm1.addColor(o, 'colour').onChange(onchange).name('Colour ' + c);
    sm2.add(o, 'position', 0.0, 1.0, 0.01).onFinishChange(onchange).name('Position ' + c);
    del_btn(c);
  }
}

function createMenu(viewer, onchange, webglmode) {
  if (!dat) return null;

  //Exists? Destroy
  if (viewer.gui)
    viewer.gui.destroy();

  //Insert within element rather than whole document
  var el = null;
  if (viewer.canvas.parentElement != document.body && viewer.canvas.parentElement.parentElement != document.body) {
    pel = viewer.canvas.parentElement.parentElement;
    pel.style.position = 'relative'; //Parent element relative prevents menu escaping wrapper div
    var id = 'dat_gui_menu_' + viewer.canvas.id;
    el = document.getElementById(id)
    if (el)
      el.parentNode.removeChild(el);
    el = document.createElement("div");
    el.id = id;
    el.style.cssText = "position: absolute; top: 0em; right: 0em; z-index: 255;";
    pel.insertBefore(el, pel.childNodes[0]);
  }

  var gui;
  //Insert within element rather than whole document
  if (el) {
    gui = new dat.GUI({ autoPlace: false });
    el.appendChild(gui.domElement);
  } else {
    gui = new dat.GUI();
  }

  //Re-create menu on element mouse down if we need to reload
  //(Instead of calling whenever state changes, re-creation is slow!)
  gui.domElement.onmousedown = function() {
    //console.log("mouse down");
    if (viewer.reloadgui)
      viewer.menu();
    return true;
  }

  //Hidden element for style compute, required to be on page for chrome, can't just use createElement
  var elem = document.getElementById('hidden_style_div');
  if (!elem) {
    elem = document.createElement('div');
    elem.style.display = 'none';
    elem.id = 'hidden_style_div';
    document.body.appendChild(elem);
  }

  //Move above other controls
  gui.domElement.parentElement.style.zIndex = '255';
  //Save close button for hide menu check
  gui.closebtn = gui.domElement.getElementsByClassName('close-button')[0];
  //Hide/show on mouseover (only if overlapping)
  if (!viewer.reloadgui) {
    //Create closed and hidden for the first time, leave as is otherwise
    gui.close();
    hideMenu(viewer.canvas, gui);
  }

  gui.add({"Reload" : function() {viewer.reload();}}, "Reload");
  //VR supported? (WebGL only)
  if (webglmode && navigator.getVRDisplays) {
    viewer.vrDisplay = null;
    viewer.inVR = false;
    gui.add({"VR Mode" : function() {start_VR(viewer);}}, 'VR Mode');
  }

  if (!!window.chrome) //Stupid chrome disabled data URL open
    gui.add({"Export" : function() {var w = window.open(); w.document.write('<pre>' + viewer.toString()  + '</pre>');}}, 'Export');
  else
    gui.add({"Export" : function() {window.open('data:application/json;base64,' + window.btoa(viewer.toString()));}}, 'Export');
  //gui.add({"loadFile" : function() {document.getElementById('fileupload').click();}}, 'loadFile'). name('Load Image file');
  //gui.add({"ColourMaps" : function() {window.colourmaps.toggle();}}, 'ColourMaps');

  //Non-persistent settings
  gui.add(viewer, "mode", ['Rotate', 'Translate', 'Zoom']);
  //var s = gui.addFolder('Settings');
  if (webglmode) {
    gui.add(viewer, "interactive");
    gui.add(viewer, "immediatesort");
    gui.add(viewer, "showBorder").onFinishChange(function() {viewer.draw();});
  }



  var g = gui.addFolder('Globals/Defaults');
  menu_addctrls(g, viewer.vis.properties, viewer, onchange);

  var v = gui.addFolder('Views');
  var ir2 = 1.0 / Math.sqrt(2.0);
  v.add({"Reset" : function() {viewer.reset(); }}, 'Reset');
  v.add({"XY" : function() {viewer.rotate = quat4.create([0, 0, 0, 1]); viewer.syncRotation(); }}, 'XY');
  v.add({"YX" : function() {viewer.rotate = quat4.create([0, 1, 0, 0]); viewer.syncRotation();}}, 'YX');
  v.add({"XZ" : function() {viewer.rotate = quat4.create([ir2, 0, 0, -ir2]); viewer.syncRotation();}}, 'XZ');
  v.add({"ZX" : function() {viewer.rotate = quat4.create([ir2, 0, 0, ir2]); viewer.syncRotation();}}, 'ZX');
  v.add({"YZ" : function() {viewer.rotate = quat4.create([0, -ir2, 0, -ir2]); viewer.syncRotation();}}, 'YZ');
  v.add({"ZY" : function() {viewer.rotate = quat4.create([0, -ir2, 0, ir2]); viewer.syncRotation();}}, 'ZY');
  //console.log(JSON.stringify(viewer.view));
  menu_addctrls(v, viewer.view, viewer, onchange);

  var o = gui.addFolder('Objects');
  for (var id in viewer.vis.objects) {
    var of = o.addFolder(viewer.vis.objects[id].name)
    menu_addctrls(of, viewer.vis.objects[id], viewer, onchange);
  }

  viewer.gui = gui;

  viewer.cgui = viewer.gui.addFolder('ColourMaps');
  viewer.cgui.folders = [];

  createColourMapMenu(viewer, onchange);

}


function createColourMapMenu(viewer, onchange) {
  //Re-create colourmap menu entries if not found
  for (var id in viewer.vis.colourmaps) {
    if (!viewer.cgui.folders[viewer.vis.colourmaps[id].name]) {
      var of = viewer.cgui.addFolder(viewer.vis.colourmaps[id].name)
      menu_addcmaps(of, viewer.vis.colourmaps[id], viewer, onchange);
      viewer.cgui.folders[viewer.vis.colourmaps[id].name] = of;
    }
  }
}

function updateMenu(viewer, onchange) {
  //Attempt to update DAT.GUI controls
  function updateDisplay(gui) {
    for (var i in gui.__controllers)
      gui.__controllers[i].updateDisplay();
    for (var f in gui.__folders)
      updateDisplay(gui.__folders[f]);
  }
  updateDisplay(viewer.gui);

  createColourMapMenu(viewer, onchange);
}

function hideMenu(canvas, gui) {
  //No menu, but hide the mode controls
  if (!gui) {
    canvas.imgtarget.nextElementSibling.style.display = "none";
    return;
  }

  //Requires menu to be closed and hiding enabled
  if (!gui.closed) return;

  //Only hide if overlapping the canvas
  var rect0 = gui.closebtn.getBoundingClientRect();
  var rect1 = canvas.getBoundingClientRect();
  if (rect0.right < rect1.left || rect0.left > rect1.right ||
      rect0.bottom < rect1.top || rect0.top > rect1.bottom) {
    //No overlap, don't hide
    return;
  }

  //Reached this point? Menu needs hiding
  gui.domElement.style.display = "none";
}

//https://hacks.mozilla.org/2018/09/converting-a-webgl-application-to-webvr/
//https://github.com/Manishearth/webgl-to-webvr/
// This function is triggered when the user clicks the "enter VR" button
function start_VR(viewer) {
  if (viewer.vrDisplay != null) {
    if (!viewer.inVR) {
      viewer.inVR = true;
      // hand the canvas to the WebVR API
      viewer.vrDisplay.requestPresent([{ source: viewer.canvas }]);
      // requestPresent() will request permission to enter VR mode,
      // and once the user has done this our `vrdisplaypresentchange`
      // callback will be triggered

      //Show stats
      if (!document.getElementById("stats_info")) {
        var stats = new Stats();
        document.body.appendChild(stats.dom);
        requestAnimationFrame(function loop() {
          stats.update();
          requestAnimationFrame(loop)
        });
      }

    } else {
      stop_VR(viewer);
    }
  }
}

function stop_VR(viewer) {
  viewer.inVR = false;
  // resize canvas to regular non-VR size if necessary
  viewer.width = 0; //Auto resize
  viewer.height = 0;
  viewer.canvas.style.width = "100%";
  viewer.canvas.style.height = "100%";

  viewer.drawFrame();
  viewer.draw();
}

function setup_VR(viewer) {
  if (!navigator.getVRDisplays) {
    alert("Your browser does not support WebVR");
    return;
  }

  function display_setup(displays) {
    if (displays.length === 0)
      return;
    //Use last in list
    viewer.vrDisplay = displays[displays.length-1];
  }

  navigator.getVRDisplays().then(display_setup);

  function VR_change() {
    // no VR display, exit
    if (viewer.vrDisplay == null)
        return;

    // are we entering or exiting VR?
    if (viewer.vrDisplay.isPresenting) {
      // optional, but recommended
      viewer.vrDisplay.depthNear = viewer.near_clip;
      viewer.vrDisplay.depthFar = viewer.far_clip;

      // We should make our canvas the size expected
      // by WebVR
      const eye = viewer.vrDisplay.getEyeParameters("left");
      // multiply by two since we're rendering both eyes side
      // by side
      viewer.width = eye.renderWidth * 2;
      viewer.height = eye.renderHeight;
      viewer.canvas.style.width = viewer.width + "px";
      viewer.canvas.style.height = viewer.height + "px";
      viewer.canvas.width = viewer.canvas.clientWidth;
      viewer.canvas.height = viewer.canvas.clientHeight;

      const vrCallback = () => {
        if (viewer.vrDisplay == null || !viewer.inVR)
          return;

        // reregister callback if we're still in VR
        viewer.vrDisplay.requestAnimationFrame(vrCallback);

        // render scene
        renderVR(viewer);
      };

      // register callback
      viewer.vrDisplay.requestAnimationFrame(vrCallback);

    } else {
      stop_VR(viewer);
    }
  }

  window.addEventListener('vrdisplaypresentchange', VR_change);
}

function renderVR(viewer) {
  //Clear full canvas
  viewer.gl.viewport(0, 0, viewer.canvas.width, viewer.canvas.height);
  viewer.gl.clear(viewer.gl.COLOR_BUFFER_BIT | viewer.gl.DEPTH_BUFFER_BIT);

  //Left eye
  renderEye(viewer, true);

  //Right eye
  renderEye(viewer, false);

  viewer.vrDisplay.submitFrame();
}

function renderEye(viewer, isLeft) {
  let projection, mview;
  let frameData = new VRFrameData();
  var gl = viewer.webgl.gl;
  var width = viewer.canvas.width / 2;
  var height = viewer.canvas.height;

  viewer.vrDisplay.getFrameData(frameData);

  // choose which half of the canvas to draw on
  if (isLeft) {
    viewer.webgl.viewport = new Viewport(0, 0, width, height);
    gl.viewport(0, 0, width, height);
    //Apply the default camera
    viewer.webgl.apply(viewer);

    projection = frameData.leftProjectionMatrix;
    mview = frameData.leftViewMatrix;
  } else {
    viewer.webgl.viewport = new Viewport(width, 0, width, height);
    gl.viewport(width, 0, width, height);
//viewer.webgl.viewport = new Viewport(0, 0, width, height);
//gl.viewport(0, 0, width, height);
    projection = frameData.rightProjectionMatrix;
    mview = frameData.rightViewMatrix;
  }

  //Apply the default camera
  viewer.webgl.apply(viewer);

  //Update matrices with VR modified versions
  //mat4.multiply(mview, viewer.webgl.modelView.matrix);
  //viewer.webgl.modelView.matrix = mview;
  mat4.multiply(viewer.webgl.modelView.matrix, mview);
//console.log(isLeft ? "LEFT " : "RIGHT "); printMatrix(mview);
  //gl.uniformMatrix4fv(viewer.webgl.program.mvMatrixUniform, false, mview);

  viewer.webgl.projection.matrix = projection;

  //Render objects
  for (var r in viewer.renderers) {
    if (viewer.renderers[r].border && !viewer.showBorder) continue;
    viewer.renderers[r].draw();
  }

  viewer.rotated = false; //Clear rotation flag
}

