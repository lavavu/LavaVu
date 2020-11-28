//Base functions to communicate between emscripten WebGL2 viewer and IPython/Javascript page

//Init standalong gui menu handler
var hideBoxTimer;
function initBase(dict, defaultcmaps) {
  //console.log(dict);
  //console.log(defaultcmaps);

  //
  var c = document.getElementById("canvas");
  c.style.display = 'block';

  //console.log("INITBOX: " + el.id);
  var viewer = new BaseViewer();
  window.viewer = viewer;

  //Command callback function
  viewer.command = function(cmds) {window.commands = cmds;}

  //Data dict and colourmap names from passed strings
  window.dictionary = viewer.dict = JSON.parse(dict);
  window.defaultcolourmaps = viewer.defaultcolourmaps = JSON.parse(defaultcmaps);

  document.addEventListener('mousemove', e => {
    //GUI elements to show on mouseover
    if (window.viewer.gui)
      window.viewer.gui.domElement.style.display = "block";
    if (hideBoxTimer) clearTimeout(hideBoxTimer);
    hideBoxTimer = setTimeout(function () { hideMenu(null, window.viewer.gui);}, 1000 );
  });

  return viewer;
}

//This object holds the viewer details and calls the renderers
function BaseViewer() {
  this.vis = {};
  this.vis.objects = [];
  this.vis.colourmaps = [];
  this.canvas = undefined;

  //Non-persistant settings
  this.mode = 'Rotate';
}

BaseViewer.prototype.reload = function() {
  this.command('reload');
}

BaseViewer.prototype.sendState = function(reload) {
  //Remove the cam data
  this.vis.exported = true;
  this.vis.reload = reload;
  this.command(JSON.stringify(this.vis));
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

BaseViewer.prototype.toString = function(nocam, reload) {

  this.vis.exported = true;
  this.vis.reload = reload ? true : false;

  if (nocam) return JSON.stringify(this.vis);
  //Export with 2 space indentation
  return JSON.stringify(this.vis, undefined, 2);
}

BaseViewer.prototype.exportFile = function() {
  window.open('data:text/plain;base64,' + window.btoa(this.toString(false, true)));
}

BaseViewer.prototype.loadFile = function(source) {
  //Skip update to rotate/translate etc if in process of updating
  //if (document.mouse.isdown) return;
  //console.log("LOADING: \n" + source);
  if (source.length < 3) {
    console.log('Invalid source data, ignoring');
    console.log(source);
    console.log(BaseViewer.prototype.loadFile.caller);
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

  //Create UI - disable by omitting dat.gui.min.js
  //(If menu doesn't exist or is open, update immediately
  // otherwise, wait until clicked/opened as update is slow)
  if (!this.gui || !this.gui.closed)
    this.menu();
  else
    this.reloadgui = true;
}

BaseViewer.prototype.menu = function() {
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
    createMenu(this, changefn, 2);
  } else {
    updateMenu(this, changefn);
  }
  this.reloadgui = false;
}

BaseViewer.prototype.syncRotation = function(rot) {
  if (rot)
    this.vis.views[0].rotate = rot;
  rstr = '' + this.getRotationString();
  that = this;
  window.requestAnimationFrame(function() {that.command(rstr);});
}

BaseViewer.prototype.getRotationString = function() {
  //Return current rotation quaternion as string
  var q = this.vis.views[0].rotate;
  return 'rotation ' + q[0] + ' ' + q[1] + ' ' + q[2] + ' ' + q[3];
}

BaseViewer.prototype.getTranslationString = function() {
  return 'translation ' + this.translate[0] + ' ' + this.translate[1] + ' ' + this.translate[2];
}

BaseViewer.prototype.reset = function() {
  this.command('reset');
}


