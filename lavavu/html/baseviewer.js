//Base functions to communicate between emscripten WebGL2 viewer and IPython/Javascript page

//Init standalong gui menu handler
var hideBoxTimer;
function initBase(dict, defaultcmaps) {
  var c = document.getElementById("canvas");
  c.style.display = 'block';

  //console.log("INITBOX: " + el.id);
  var viewer = new BaseViewer();
  window.viewer = viewer;

  //Command callback function
  viewer.command = function(cmds) {window.commands.push(cmds);}

  //Data dict and colourmap names from passed strings
  window.dictionary = viewer.dict = JSON.parse(dict);
  window.defaultcolourmaps = viewer.defaultcolourmaps = JSON.parse(defaultcmaps);

  document.addEventListener('mousemove', mouseover);

  //Touch events
  c.addEventListener("touchstart", touchHandler, true);
  c.addEventListener("touchmove", touchHandler, true);
  c.addEventListener("touchend", touchHandler, true);

  return viewer;
}

function mouseover() {
  //GUI elements to show on mouseover
  if (window.viewer.gui)
    window.viewer.gui.domElement.style.display = "block";
  if (hideBoxTimer) clearTimeout(hideBoxTimer);
  hideBoxTimer = setTimeout(function () { hideMenu(null, window.viewer.gui);}, 1000 );
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

//Basic touch event handling
function touchHandler(event)
{
  var mouse = event.target.mouse;
  if (!mouse)
    event.target.mouse = mouse = {"identifier" : -1};

  if (window.viewer.gui)
    mouseover();

  switch(event.type)
  {
    case "touchstart":
      if (!mouse.touchmax || mouse.touchmax < event.touches.length) {
        mouse.touchmax = event.touches.length;
        //console.log("touchmax: " + mouse.touchmax);
        if (event.touches.length == 2)
          mouse.scaling = 0;
      }
      break;
    case "touchmove":
      //console.log("touchmax: " + mouse.touchmax + " == " + event.touches.length);
      if (event.touches.length == mouse.touchmax) {
        var down = false;
        var touch;
        //Get the touch to track, always use the same id
        if (mouse.identifier == -1) {
          mouse.identifier = event.touches[0].identifier;
          touch = event.touches[0];
          down = true;
        } else {
          //Find logged touch
          for (var t=0; t<event.touches.length; t++) {
            //console.log(t + " / " + event.touches.length + " : " + event.touches[t].identifier + ' === ' + mouse.identifier)
            if (event.touches[t].identifier === mouse.identifier) {
              touch = event.touches[t];
              break;
            }
          }
        }

        var x = touch.pageX;
        var y = touch.pageY;

        //Delayed mouse-down event post (need to wait until all of multiple touches registered)
        if (down)
          window.viewer.command('mouse mouse=down,button=' + (event.touches.length > 1 ? 2 : 0) + ',x=' + x + ',y=' + y);

        if (event.touches.length == 3) {
          //Translate on drag with 3 fingers(equivalent to drag with right mouse)
          window.viewer.command('mouse mouse=move,button=2,x=' + x + ',y=' + y);

        } else if (event.touches.length == 2) {
          var pinch = 0;
          if (mouse.scaling != null && event.touches.length == 2) {
            var dist = Math.sqrt(
              (event.touches[0].pageX-event.touches[1].pageX) * (event.touches[0].pageX-event.touches[1].pageX) +
              (event.touches[0].pageY-event.touches[1].pageY) * (event.touches[0].pageY-event.touches[1].pageY));

            if (mouse.scaling > 0)
              pinch = (dist - mouse.scaling);
            else
              mouse.scaling = dist;
            //console.log("DIST " + dist + " SCALING " + mouse.scaling + " PINCH " + pinch);
          }

          //Pinch to scale? If not sufficient scaling, interpret as translate drag
          if (Math.abs(pinch) > 50)
            window.viewer.command('zoom ' + (pinch * 0.0005));
          else
            //Also translate on drag with 2 fingers(equivalent to drag with right mouse)
            window.viewer.command('mouse mouse=move,button=2,x=' + x + ',y=' + y);

        } else if (event.touches.length == 1) {
          //Rotate
          window.viewer.command('mouse mouse=move,button=0,x=' + x + ',y=' + y);
        }
      }
      break;
    case "touchend":
      if (event.touches.length == mouse.touchmax) {
        mouse.scaling = null;
        window.viewer.command('mouse mouse=up,button=' + (event.touches.length > 1 ? 2 : 0));
      }
      mouse.identifier = -1;
      mouse.touchmax = 0;
      break;
    default:
      return;
  }

  event.preventDefault();
  return false;
}


