/* dat.gui menu */

function parseColour(input) {
  if (typeof(input) != 'object') {
    //var div = document.createElement('div');
    var div = document.getElementById('hidden_style_div');
    div.style.color = input;
    //This triggers a full layout calc - can slow things down
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

  } else if ((dtype.indexOf('real') >= 0 || dtype.indexOf('integer') >= 0)) { // && typeof(obj[prop]) == 'number') {

    //Ranged?
    var range = menu_getrange(obj, ctrl);
    var addnumeric = function(menu, obj, prop, range, changefn) {
      if (range) {
        if (obj[prop] < range[0]) obj[prop] = range[0];
        if (obj[prop] > range[1]) obj[prop] = range[1];
        return menu.add(obj, prop, range[0], range[1], ctrl[1][2]).onFinishChange(changefn);
      } else {
        return menu.add(obj, prop).onFinishChange(changefn);
      }
    }

    //Array quantities
    var dims = 1;
    var p0 = dtype.indexOf('[');
    var p1 = dtype.indexOf(']');
    if (p0 >= 0 && p1 >= 0)
      dims = parseInt(dtype.slice(p0+1, p1));
    if (dims > 1) {
      //2d, 3d ?
      for (var d=0; d<dims; d++) {
        //console.log("Adding DIM " + d);
        var added = addnumeric(menu, obj[prop], "" + d, range, changefn)
        added.name(prop + '[' + d + ']');
      }

    //Only add if a number, anything else will cause dat.gui to error
    } else if (typeof(obj[prop]) == 'number') {
      //1d
      console.log("Adding ", prop, typeof(obj[prop]));
      addnumeric(menu, obj, prop, range, changefn)
    }

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
  //Loop through every available property
  var extras = [];
  for (var prop in viewer.dict) {

    //Check if it is enabled in GUI
    var ctrl = viewer.dict[prop].control;
    if (!ctrl || ctrl[0] === false) continue; //Control disabled

    //Check if it has been set on the target object
    if (prop in obj) {
      //console.log(prop + " ==> " + JSON.stringify(viewer.dict[prop]));
      menu_addctrl(menu, obj, viewer, prop, onchange);

    } else {
      //Save list of properties without controls
      extras.push(prop);
      continue;
    }
  }

  //Sort into alphabetical order
  extras.sort();

  //Add the extra properties to a dropdown list
  //Selecting a property will add a controller for it
  var propadd = {"properties" : ""};
  var propselfn = function(prop) {
    //Set the property to the default value
    propadd.ref[prop] = viewer.dict[prop]["default"];
    //Add new property controller
    menu_addctrl(propadd.menu, propadd.ref, viewer, prop, onchange);
    //Remove, then add the props list back at the bottom, minus new prop
    propadd.menu.remove(propadd.controller);
    //Save the new filtered list
    propadd.list = propadd.list.filter(word => word != prop);
    propadd.controller = propadd.menu.add(propadd, "properties", propadd.list).name("More properties").onFinishChange(propadd.fn);
  }
  propadd.controller = menu.add(propadd, "properties", extras).name("More properties").onFinishChange(propselfn);
  propadd.ref = obj;
  propadd.menu = menu;
  propadd.fn = propselfn;
  propadd.list = extras;
}


function menu_addcmaps(menu, obj, viewer, onchange) {
  //Colourmap editing menu
  if (viewer.cgui.prmenu) viewer.cgui.removeFolder(viewer.cgui.prmenu);
  if (viewer.cgui.cmenu) viewer.cgui.removeFolder(viewer.cgui.cmenu);
  if (viewer.cgui.pomenu) viewer.cgui.removeFolder(viewer.cgui.pomenu);
  viewer.cgui.prmenu = viewer.cgui.addFolder("Properties");
  viewer.cgui.cmenu = viewer.cgui.addFolder("Colours");
  viewer.cgui.pomenu = viewer.cgui.addFolder("Positions");
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
      menu_addctrl(menu.prmenu, obj, viewer, prop, onchange);
    }
  }

  var reload_onchange = function() {onchange("", true);};

  //Loop through colours
  menu.cmenu.add({"Add Colour" : function() {obj.colours.unshift({"colour" : "rgba(0,0,0,1)", "position" : 0.0}); viewer.gui.close(); reload_onchange(); }}, "Add Colour");
  //Need to add delete buttons in closure to get correct pos/index
  function del_btn(pos) {menu.pomenu.add({"Delete" : function() {obj.colours.splice(pos, 1); viewer.gui.close(); reload_onchange(); }}, 'Delete').name('Delete ' + pos);}
  for (var c in obj.colours) {
    var o = obj.colours[c];
    //Convert to html colour first
    o.colour = parseColour(o.colour);
    o.opacity = 1.0;
    menu.cmenu.addColor(o, 'colour').onChange(reload_onchange).name('Colour ' + c);
    menu.pomenu.add(o, 'position', 0.0, 1.0, 0.01).onFinishChange(reload_onchange).name('Position ' + c);
    del_btn(c);
  }
}

function createMenu(viewer, onchange, webglmode, global) {
  if (!dat) return null;
  //var t0 = performance.now();

  //Exists? Destroy
  if (viewer.gui)
    viewer.gui.destroy();

  var el = null;
  //Insert within element rather than whole document
  if (!global && viewer.canvas && viewer.canvas.parentElement != document.body && viewer.canvas.parentElement.parentElement != document.body) {
    var pel = viewer.canvas.parentElement;
    //A bit of a hack to detect converted HTML output and skip going up two parent elements
    if (pel.parentElement.className != 'section')
      pel = pel.parentElement;
    pel.style.position = 'relative'; //Parent element relative prevents menu escaping wrapper div
    //Jupyter notebook overflow hacks (allows gui to be larger than cell)
    var e = pel;
    //These parent elements need 'overflow: visible' to prevent scrolling or cut-off of menu
    //Add the "pgui" class to all the parent containers 
    while (e != document.body) {
      e.classList.add("pgui");
      e = e.parentElement;
    }
    //pel.style.overflow = 'visible'; //Parent element relative prevents menu escaping wrapper div
    //pel.parentElement.style.overflow = 'visible'; //Parent element relative prevents menu escaping wrapper div
    var id = 'dat_gui_menu_' + (viewer.canvas ? viewer.canvas.id : 'default');
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
    gui = new dat.GUI({ autoPlace: false, width: 275, hideable: false });
    el.appendChild(gui.domElement);
  } else {
    gui = new dat.GUI({ hideable: false, width: 275 });
  }

  //Re-create menu on element mouse down if we need to reload
  //(Instead of calling whenever state changes, re-creation is slow!)
  gui.domElement.onmousedown = function(e) {
    if (viewer.reloadgui)
      viewer.menu();
    return true;
  }

  //Horrible hack to stop codemirror stealing events when menu is over an editor element
  gui.domElement.onmouseenter = function(e) {
    //console.log('mouseenter');
    var stylesheet = document.styleSheets[0];
    stylesheet.insertRule('.CodeMirror-code { pointer-events: none;}', 0);
  };

  gui.domElement.onmouseleave = function(e) {
    //console.log('mouseleave');
    var stylesheet = document.styleSheets[0];
    if (stylesheet.cssRules[0].cssText.indexOf('CodeMirror') >= 0)
      stylesheet.deleteRule(0);
  };

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
  if (webglmode === 1 && navigator.getVRDisplays) {
    viewer.vrDisplay = null;
    viewer.inVR = false;
    gui.add({"VR Mode" : function() {start_VR(viewer);}}, 'VR Mode');
  }

  if (!!window.chrome) //Stupid chrome disabled data URL open
    gui.add({"Export" : function() {var w = window.open(); w.document.write('<pre>' + viewer.toString()  + '</pre>');}}, 'Export');
  else
    gui.add({"Export" : function() {window.open('data:application/json;base64,' + window.btoa(viewer.toString()));}}, 'Export');
  //gui.add({"loadFile" : function() {document.getElementById('fileupload').click();}}, 'loadFile'). name('Load Image file');

  //Non-persistent settings
  gui.add(viewer, "mode", ['Rotate', 'Translate', 'Zoom']);
  if (webglmode === 0) {
    viewer.cmd = '';
    gui.add(viewer, "cmd").onFinishChange(function(cmd) {if (cmd.length) {viewer.command(cmd); viewer.cmd = '';}}).name('Command');
  }

  //var s = gui.addFolder('Settings');
  if (webglmode === 1) {
    //Old WebGL 1.0 mode
    gui.add(viewer.vis, "interactive").name("Interactive Render");
    gui.add(viewer.vis, "immediatesort").name("Immediate Sort");
    gui.add(viewer.vis, "sortenabled").name('Sort Enabled');
  } else if (webglmode === 2) {
    //Emscripten WebGL2 mode
    gui.add({"Full Screen" : function() {Module.requestFullscreen(false,true);}}, 'Full Screen');
    var params = {loadBrowserFile : function() { document.getElementById('fileinput').click(); } };
    gui.add(params, 'loadBrowserFile').name('Load file');
    gui.add({"Export GLDB" : function() {window.commands.push('export');}}, 'Export GLDB');
  } else if (viewer.canvas) {
    //Server render
    var url = viewer.canvas.imgtarget.baseurl;
    if (url)
      gui.add({"Popup Viewer" : function() {window.open(url, "LavaVu", "toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=1024,height=768");}}, 'Popup Viewer');
  }

  var g = gui.addFolder('Globals/Defaults');
  menu_addctrls(g, viewer.vis.properties, viewer, onchange);

  var v = gui.addFolder('Views');
  var ir2 = 1.0 / Math.sqrt(2.0);
  v.add({"Reset" : function() {viewer.reset(); }}, 'Reset');
  v.add({"XY" : function() {viewer.syncRotation([0, 0, 0, 1]); }}, 'XY');
  v.add({"YX" : function() {viewer.syncRotation([0, 1, 0, 0]);}}, 'YX');
  v.add({"XZ" : function() {viewer.syncRotation([ir2, 0, 0, -ir2]);}}, 'XZ');
  v.add({"ZX" : function() {viewer.syncRotation([ir2, 0, 0, ir2]);}}, 'ZX');
  v.add({"YZ" : function() {viewer.syncRotation([0, -ir2, 0, -ir2]);}}, 'YZ');
  v.add({"ZY" : function() {viewer.syncRotation([0, -ir2, 0, ir2]);}}, 'ZY');
  //console.log(JSON.stringify(viewer.view));
  if (viewer.view)
    menu_addctrls(v, viewer.view, viewer, onchange);
  else
    menu_addctrls(v, viewer.vis.views[0], viewer, onchange);

  var o = gui.addFolder('Objects');
  for (var id in viewer.vis.objects) {
    var of = o.addFolder(viewer.vis.objects[id].name);
    menu_addctrls(of, viewer.vis.objects[id], viewer, onchange);
  }

  viewer.gui = gui;
  if (!viewer.selectedcolourmap) {
    viewer.selectedcolourmap = "";
    viewer.newcolourmap = "";
  }
  viewer.cgui = viewer.gui.addFolder('ColourMaps');

  createColourMapMenu(viewer, onchange, webglmode === 1);

  //var t1 = performance.now();
  //console.log("Call to menu() took " + (t1 - t0) + " milliseconds.")
}

function createColourMapMenu(viewer, onchange, webglmode) {
  //Remove existing entries
  for (var i in viewer.cgui.__controllers)
    viewer.cgui.__controllers[i].remove();

  var cms = [];

  //Dropdown to select a colourmap, when selected the editing menu will be populated
  for (var id in viewer.vis.colourmaps)
    cms.push(viewer.vis.colourmaps[id].name);

  if (webglmode) {
    //Add a default colourmap
    viewer.cgui.add({"Add" : function() {viewer.addColourMap(); viewer.gui.close();}}, 'Add');
  } else {
    //Add a colourmap from list of defaults
    cmapaddfn = function(value) {
      if (!value || !value.length) return;
      viewer.command("select; colourmap " + value + " " + value);
      viewer.gui.close();
    };

    viewer.cgui.add(viewer, "newcolourmap", viewer.defaultcolourmaps).onFinishChange(cmapaddfn).name("Add");
  }

  //Do the rest only if colourmaps exist...
  if (cms.length == 0) return;

  //When selected, populate the Colours & Positions menus
  cmapselfn = function(value) {
    if (!value) return;
    for (var id in viewer.vis.colourmaps) {
      if (value == viewer.vis.colourmaps[id].name)
        menu_addcmaps(viewer.cgui, viewer.vis.colourmaps[id], viewer, onchange);
    }
  };

  viewer.cgui.cmap = viewer.cgui.add(viewer, "selectedcolourmap", cms).onFinishChange(cmapselfn).name("Colourmap");

  //Re-select if previous value if any
  if (viewer.selectedcolourmap)
  cmapselfn(viewer.selectedcolourmap);
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
  updateDisplay(viewer.cgui);
}

function hideMenu(canvas, gui) {
  //No menu, but hide the mode controls
  if (!gui) {
    if (canvas && canvas.imgtarget)
      canvas.imgtarget.nextElementSibling.style.display = "none";
    return;
  }

  //Requires menu to be closed and hiding enabled
  if (!gui.closed) return;

  //Only hide if overlapping the canvas (unless no canvas passed)
  if (canvas) {
    var rect0 = gui.closebtn.getBoundingClientRect();
    var rect1 = canvas.getBoundingClientRect();
    if (rect0.right < rect1.left || rect0.left > rect1.right ||
        rect0.bottom < rect1.top || rect0.top > rect1.bottom) {
      //No overlap, don't hide
      return;
    }
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

