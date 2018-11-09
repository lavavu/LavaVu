//IPython kernel object
var kernel;
var gkernel;
try {
  if (parent && parent.IPython)
    kernel = parent.IPython.notebook.kernel;

    function exec_kernel(cmd) {
      //For debugging:
      //Run a python command in kernel and log output to console
      if (kernel) {
        if (debug_mode) {
          //Debug callback
          var callbacks = {'output' : function(out) {
              if (!out.content.data) {console.log(JSON.stringify(out)); return;}
              data = out.content.data['text/plain']
              console.log("CMD: " + cmd + ", RESULT: " + data);
            }
          };
        }
        kernel.execute(cmd, {iopub: callbacks}, {silent: false});
      }
    }

} catch (e) {
  //Google Colab
  if (google) {
    gkernel = google.colab.kernel;

    function g_execute(wi, fn, cmds, callback) {
      function successCallback(res) {
        result = res["data"]["application/json"];
        console.log(fn + 'success ' + wi.id);
        console.log(("image" in result) + " IMG, " + ("state" in result) + " STATE");
        if (result["image"] && wi.img) {
          console.log("Got image: " + result['image'].length);
          if (wi.img) {
            //Set onload callback if provided
            if (callback)
              wi.img.onload = callback;
            wi.img.src = result["image"];
          }
        } else if (result["state"] && callback) {
          callback(result["state"]);
        }
      }

      function failureCallback(res) {
        console.log(fn + 'failure: ' + res);
      }

      promise = gkernel.invokeFunction(fn + wi.id, cmds, {})
      promise.then(successCallback, failureCallback);
    }
  }
}

//Maintain a list of interactor instances opened by id
//(check in case this script imported twice, don't overwrite previous)
var _wi = window._wi ? window._wi : [];
var delay = 0;
var debug_mode = false;

function WindowInteractor(id, baseurl) {
  //Store self in list and save id
  this.id = id;
  var loc = window.location;
  if (!baseurl)
    this.baseurl = loc.hostname + (loc.port ? ":" + loc.port : "");
  else {
    this.baseurl = loc.protocol + "//" + baseurl;
    //Don't use IPython kernel if passed baseurl
    kernel = null;
    gkernel = null;
  }
  
  console.log("New interactor: " + this.id);
  //Interactor class, handles javascript side of window control
  //Takes viewer id
  // - set as active target for commands
  // - init webgl bounding box
  // - display initial image
  this.img = document.getElementById("imgtarget_" + this.id);

  //No window
  if (!this.img) return;

  //Initial image
  //(Init WebGL bounding box interaction on load)
  var that = this;
  this.get_image(function() {
    //console.log('In image loaded callback ' + that.id);
    //Init on image load with callback function to execute commands
    that.box = initBox(that.img, function(cmd) {that.execute(cmd);});
    //console.log("Box init on " + that.id);
    //Update the box size by getting state
    updateBox(that.box, function(onget) {that.get_state(onget);});
    //Clear onload
    that.img.onload = null;
  });
}

//Load frame image and run command in single action (non-IPython mode only)
var instant = true;

WindowInteractor.prototype.execute = function(cmd, callback) {
console.log("execute: " + cmd);
  if (kernel) {
    exec_kernel('lavavu.control.windows[' + this.id + '].commands("' + cmd + '")');
    //kernel.execute('lavavu.control.windows[' + this.id + '].commands("' + cmd + '")');
    this.get_image(callback);
  } else if (gkernel) {
    //Google Colab
    //Execute and get image result
    g_execute(this, 'img_', [cmd], callback);

  } else {
    //HTTP interface
    //Replace newlines with semi-colon first
    cmd = cmd.replace(/\n/g,';');
    if (instant && this.img) {
      var url = this.baseurl + "/icommand=" + cmd + "?" + new Date().getTime();
      this.img.onload = null;
      if (callback)
        this.img.onload = callback;
      this.img.src = url;
    } else {
      var url = this.baseurl + "/command=" + cmd + "?" + new Date().getTime()
      xhttp = new XMLHttpRequest();
      xhttp.open('GET', url, true);
      xhttp.send();
      this.get_image(callback);
    }

  }

  //Reload state
  if (this.img) {
    var that = this;
    updateBox(this.box, function(onget) {that.get_state(onget);});
  }

  return false;
}

WindowInteractor.prototype.set_prop = function(obj, prop, val) {
  this.execute("select " + obj + "; " + prop + "=" + val);
  this.get_image();
}

WindowInteractor.prototype.do_action = function(id, val) {
  if (kernel) {
    //Non-numeric, add quotes and strip newlines
    if (typeof(val) == 'string' && ("" + parseFloat(val) !== val)) {
      val = val.replace(/\n/g,';');
      val = '"' + val + '"';
    }

    //kernel.execute('cmds = lavavu.control.action(' + id + ',' + val + ')');
    var cmd = 'cmds = lavavu.control.Action.do(' + id + ',' + val + ')';
    exec_kernel(cmd);
    cmd = 'if cmds: lavavu.control.windows[' + this.id + '].commands(cmds)';
    exec_kernel(cmd);
    this.get_image();
  } else if (gkernel) {
    //Google Colab
    g_execute(this, 'act_', [id, value]);

  } else {
    //HTML control actions via http
    this.actions[id](val);
  }

  /*/Reload state - this happens in action execute anyway, so not needed?
  if (this.img) {
    var that = this;
    updateBox(this.box, function(onget) {that.get_state(onget);});
  }*/
}

WindowInteractor.prototype.redisplay = function() {
  //console.log("redisplay: " + this.id);
  if (this.img) {
    //Call get_image
    this.get_image();
    //Update the box size by getting state
    var that = this;
    updateBox(that.box, function(onget) {that.get_state(onget);});
  }
}

WindowInteractor.prototype.get_image = function(onload) {
  if (!this.img) return;
  //console.log("get_img: " + this.id);
  if (kernel) {
    var that = this;
    var callbacks = {'output' : function(out) {
        //Skip first message we get without data
        //if (!out.content.data) {console.log(JSON.stringify(out)); return;}
        if (!out.content.data) return;
        data = out.content.data['text/plain']
        data = data.substring(1, data.length-1)
        //console.log("Got image: " + data.length);
        if (that.img) {
          //Only set onload if provided
          if (onload) // && !that.img.onload)
            that.img.onload = onload;
          that.img.src = data;
        }
      }
    };

    kernel.execute('lavavu.control.windows[' + this.id + '].frame()', {iopub: callbacks}, {silent: false});

  } else if (gkernel) {
    //Google Colab
    g_execute(this, 'img_', [''], onload);

  } else {
    //if (!this.img) this.img = document.getElementById('imgtarget_0');
    var url = this.baseurl + "/image?" + new Date().getTime();
    if (this.img) {
      this.img.onload = onload;
      this.img.src = url;
    }
  }
}

WindowInteractor.prototype.get_state = function(onget) {
  //console.log("get_state: " + this.id);
  if (kernel) {
    var callbacks = {'output' : function(out) {
        //if (!out.content.data) {console.log(JSON.stringify(out)); return;}
        if (!out.content.data) return;
        data = out.content.data['text/plain']
        data = data.replace(/(?:\\n)+/g, "");
        data = data.substring(1, data.length-1)
        onget(data);
      }
    };
    kernel.execute('lavavu.control.windows[' + this.id + '].app.getState()', {iopub: callbacks}, {silent: false});
  } else if (gkernel) {
    //Google Colab
    g_execute(this, 'cmd_', [''], onget);
  } else {
    var url = this.baseurl + "/getstate"
    xhttp = new XMLHttpRequest();
    xhttp.onload = function() {
      if(xhttp.status == 200)
        onget(xhttp.response);
      else  
        console.log("Ajax Request Error: " + url + ", returned status code " + xhttp.status + " " + xhttp.statusText);
    } 
    xhttp.open('GET', url, true);
    xhttp.send();
  }
}

//Update controls from JSON data
function updateControlValues(controls) {
  for (var c in controls) {
    var control = controls[c];
    var els = document.getElementsByClassName(control[0]);
    for(var i = 0; i < els.length; i++) {
      //console.log(els[i].id + " : " + els[i].value + " ==> " + control[1]);
      if (els[i].type == 'checkbox')
        els[i].checked = control[1];
      else
        els[i].value = control[1];
    };
  }
}

function getAndUpdateControlValues(names) {
  //Get the updated values
  if (kernel) {
    var callbacks = {'output' : function(out) {
        if (!out.content.data) {console.log(JSON.stringify(out)); return;}
        data = out.content.data['text/plain']
        data = data.substring(1, data.length-1)
        data = data.replace(/(?:\\n)+/g, "");
        data = data.replace(/'/g, '"');
        //console.log(data);
        updateControlValues(JSON.parse(data));
      }
    };
    kernel.execute('import json; json.dumps(lavavu.control.getcontrolvalues("' + names + '"))', {iopub: callbacks}, {silent: false});
  } else {
    //TODO: http version, need to request updated values from web server
  }
}


