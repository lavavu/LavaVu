/* dat.gui menu */
function createMenu(viewer, onchange, webglmode) {
  if (!dat) return null;

  //Insert within element rather than whole document
  var el = null;
  if (viewer.canvas.parentElement != document.body) {
    pel = viewer.canvas.parentElement.parentElement;
    pel.style.position = 'relative'; //Parent element relative prevents menu escaping wrapper div
    el = document.createElement("div");
    el.style.cssText = "position: absolute; top: 0em; right: 0em; z-index: 255;";
    pel.insertBefore(el, pel.childNodes[0]);
  }

  //Exists? Destroy
  if (viewer.gui)
    viewer.gui.destroy();

  var gui;
  //Insert within element rather than whole document
  if (el) {
    gui = new dat.GUI({ autoPlace: false });
    el.appendChild(gui.domElement);
  } else {
    gui = new dat.GUI();
  }

  gui.close();

  //Hide/show on mouseover (only if overlapping)
  //hideMenu(viewer.canvas, gui);

  gui.add({"Reload" : function() {viewer.reload();}}, "Reload");
  if (navigator.getVRDisplays)
    gui.add({"VR Mode" : function() {start_VR();}}, 'VR Mode');
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

  getrange = function(obj, ctrl) {
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
        console.log(obj.name + " : range (invalid) = " + range);
        return [0.0, 1.0]; //No valid range data, use a default
      }
      console.log(obj.name + " : range = " + range);
      return range;
    }
    return null;
  }

  addctrl = function(menu, obj, viewer, prop) {
    //TODO: implement delayed high quality render for faster interaction
    //var changefn = function(value) {viewer.delayedRender(250);};
    var changefn = onchange; //function(value) {viewer.reload(); viewer.draw();};
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
      var range = getrange(obj, ctrl);
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
        menu.addColor(obj, prop).onFinishChange(changefn);
      } catch(e) {
        console.log(e);
      }
    }
  }

  addctrls = function(menu, obj, viewer) {
    var submenu = menu.addFolder(' ... ');
    var submenus = {};
    //var typelist = ['all', 'global', 'view', 'object', 'object(point)', 'object(line)', 'object(volume)', 'object(surface)', 'object(vector)', 'object(tracer)', 'object(shape)', 'colourbar', 'colourmap'];
    var typelist = ['all', 'global', 'view', 'object', 'object(point)', 'object(line)', 'object(volume)', 'object(surface)', 'colourmap'];
    for (var t in typelist)
      submenus[typelist[t]] = submenu.addFolder(typelist[t]);

    for (var prop in viewer.dict) {

      var ctrl = viewer.dict[prop].control;
      if (!ctrl || ctrl[0] === false) continue; //Control disabled

      if (prop in obj) {
        //console.log(prop + " ==> " + JSON.stringify(viewer.dict[prop]));
        addctrl(menu, obj, viewer, prop);

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
            addctrl(this.parentMenu, this.ref, viewer, this.property);
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

  var g = gui.addFolder('Globals/Defaults');
  addctrls(g, viewer.vis.properties, viewer);

  var v = gui.addFolder('Views');
  var ir2 = 1.0 / Math.sqrt(2.0);
  v.add({"XY" : function() {viewer.rotate = quat4.create([0, 0, 0, 1]); viewer.syncRotation(); }}, 'XY');
  v.add({"YX" : function() {viewer.rotate = quat4.create([0, 1, 0, 0]); viewer.syncRotation();}}, 'YX');
  v.add({"XZ" : function() {viewer.rotate = quat4.create([ir2, 0, 0, -ir2]); viewer.syncRotation();}}, 'XZ');
  v.add({"ZX" : function() {viewer.rotate = quat4.create([ir2, 0, 0, ir2]); viewer.syncRotation();}}, 'ZX');
  v.add({"YZ" : function() {viewer.rotate = quat4.create([0, -ir2, 0, -ir2]); viewer.syncRotation();}}, 'YZ');
  v.add({"ZY" : function() {viewer.rotate = quat4.create([0, -ir2, 0, ir2]); viewer.syncRotation();}}, 'ZY');
  //console.log(JSON.stringify(viewer.view));
  addctrls(v, viewer.view, viewer);

  var o = gui.addFolder('Objects');
  for (var id in viewer.vis.objects) {
    var of = o.addFolder(viewer.vis.objects[id].name)
    console.log("------------------- " + viewer.vis.objects[id].name);
    addctrls(of, viewer.vis.objects[id], viewer);
  }

  var c = gui.addFolder('ColourMaps');
  for (var id in viewer.vis.colourmaps) {
    var of = c.addFolder(viewer.vis.colourmaps[id].name)
    addctrls(of, viewer.vis.colourmaps[id], viewer);
  }

    /*for (var type in viewer.vis.objects[id]) {
      if (type in types)
        console.log(type);
    }*/

  viewer.gui = gui;
}

function hideMenu(canvas, gui) {
  if (!gui.closed) return;

  if (canvas.parentElement != document.body) {
    var rect0 = canvas.getBoundingClientRect();
    var rect1 = gui.domElement.getBoundingClientRect();
    if (rect1.left > rect0.left + rect0.width)
      //No overlap, don't hide
      return;
  }

  //Reached this point? Menu needs hiding
  gui.domElement.style.display = "none";
  gui.hidetimer = true;
}


