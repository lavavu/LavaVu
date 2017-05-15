var kernel;
//Maintain a list of interactor instances opened by id
var instances = [];
var delay = 0;

function initLoad() {
  if (parent.IPython) {
    kernel = parent.IPython.notebook.kernel;
    //This ensures we have imported the lavavu module in notebook scope
    //required for control event pass-back
    //(NOTE: this looks a bit hacky because it is, but it seems kernel.execute loops only work as a one liners)
    kernel.execute('lavavu = None if not "lavavu" in globals() else lavavu');
    kernel.execute('import sys');
    kernel.execute('modules = dict(sys.modules)');
    kernel.execute('for m in modules: lavavu = modules[m].lavavu if "lavavu" in dir(modules[m]) else lavavu');
  }
}

function getUrl() {
  var loc = window.location;
  var baseUrl = loc.protocol + "//" + loc.hostname + (loc.port ? ":" + loc.port : "");
  return baseUrl;
}

function do_action(id, val, element) {
  //Get instance id from parent element, next level up or just use 0
  //console.log("DO_ACTION " + id + " ( " + val + " ) " + element.id);
  var inst_id = element.parentElement.dataset.id;
  if (inst_id == undefined)
    inst_id = element.parentElement.parentElement.dataset.id;
  if (inst_id == undefined)
    inst_id = 0;
  else
    inst_id = parseInt(inst_id);

  //if (inst_id-1 > instances.length) return;
  if (inst_id+1 > instances.length) {console.log("Invalid ID for action instance: " + (inst_id+1) + ' > ' + instances.length); return;}
  if (!instances[inst_id]) {console.log("NO ACTION INSTANCE: [" + inst_id + "] " + instances[inst_id]); return;}

  //Call action on interactor which owns control
  instances[inst_id].do_action(id, val);
}

function redisplay(id) {
  //If instance doesn't exist yet, try again in 0.5 seconds... only seems to be needed in chrome
  if (id+1 > instances.length || !instances[id] || instances[id].box === undefined) {
    if (delay > 5) return; //Give up after 5 tries
    console.log("Delaying redisplay " + id);
    setTimeout(function() {redisplay(id);}, 500); 
    delay++;
    if (id+1 > instances.length) console.log(" -- Invalid ID " + (id+1) + ' > ' + instances.length);
    else if (!instances[id]) console.log(" -- No instance: [" + id + "] " + instances[id]);
    else if (!instances[id].box) console.log(" -- No box viewer: [" + id + "] " + instances[id]);
    return;
  }

  var that = instances[id];
  if (that.img) {
    //Call get_image
    that.get_image();
    //Update the box size by getting state
    updateBox(that.box, function(onget) {that.get_state(onget);});
  }
}

function WindowInteractor(id) {
  //Always check for lavavu module loaded, in case IPython notebook has been reloaded
  initLoad();
  //Check instances list valid, wipe if adding initial interactor
  if (!instances || id==0) instances = [];
  //Store self in list and save id
  this.id = id;
  instances[id] = this;
  
  console.log("New interactor: " + this.id + " : " + instances.length + " :: " + instances[this.id]);
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
    //console.log('In image loaded callback 0');
    //Init on image load with callback function to execute commands
    that.box = initBox(that.img, function(cmd) {that.execute(cmd, true);});
    //console.log("Box init on " + that.id);
    //Update the box size by getting state
    updateBox(that.box, function(onget) {that.get_state(onget);});
    //Clear onload
    that.img.onload = null;
  });
}

WindowInteractor.prototype.execute = function(cmd, instant) { //, viewer_id) {
  //console.log("execute: " + cmd);
  if (kernel) {
    kernel.execute('lavavu.control.windows[' + this.id + '].commands("' + cmd + '")');
    this.get_image();
  } else {
    //HTTP interface
    //Replace newlines with semi-colon first
    cmd = cmd.replace(/\n/g,';');
    if (instant && this.img) {
      var url = getUrl() + "/icommand=" + cmd + "?" + new Date().getTime();
      this.img.onload = null;
      this.img.src = url;
    } else {
      var url = getUrl() + "/command=" + cmd + "?" + new Date().getTime()
      x = new XMLHttpRequest();
      x.open('GET', url, true);
      x.send();
      this.get_image();
    }

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
    kernel.execute('cmds = lavavu.control.action(' + id + ',' + val + ')');
    kernel.execute('if len(cmds): lavavu.control.windows[' + this.id + '].commands(cmds)');
    /*/Debug callback
    var callbacks = {'output' : function(out) {
        if (!out.content.data) {alert("NO DATA"); return;}
        data = out.content.data['text/plain']
          alert("RESULT: " + data);
      }
    };*/
    //kernel.execute('len(lavavu.control.actions)', {iopub: callbacks}, {silent: false});
    //kernel.execute('str(lavavu.control.actions[' + id + '])', {iopub: callbacks}, {silent: false});
    //kernel.execute('lavavu.control.action(' + id + ',' + val + ')', {iopub: callbacks}, {silent: false});
    //kernel.execute('str(lavavu.control.actions[' + id + ']["args"][0])', {iopub: callbacks}, {silent: false});
    //kernel.execute('cmds', {iopub: callbacks}, {silent: false});
    this.get_image();
  } else {
    //HTML control actions via http
    actions[id](val);
  }

  //Reload state
  var that = this;
  if (that.img)
    updateBox(this.box, function(onget) {that.get_state(onget);});
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
          that.img.onload = onload;
          that.img.src = data;
        }
      }
    };

    kernel.execute('lavavu.control.windows[' + this.id + '].frame()', {iopub: callbacks}, {silent: false});
  } else {
    //if (!this.img) this.img = document.getElementById('imgtarget_0');
    var url = getUrl() + "/image?" + new Date().getTime();
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
        if (!out.content.data) return;
        data = out.content.data['text/plain']
        data = data.replace(/(?:\\n)+/g, "");
        data = data.substring(1, data.length-1)
        onget(data);
      }
    };
    kernel.execute('lavavu.control.windows[' + this.id + '].getState()', {iopub: callbacks}, {silent: false});
  }
  else {
    var url = getUrl() + "/getstate"
    x = new XMLHttpRequest();
    x.onload = function() { 
      if(x.status == 200)
        onget(x.response);
      else  
        console.log("Ajax Request Error: " + url + ", returned status code " + x.status + " " + x.statusText);
    } 
    x.open('GET', url, true);
    x.send();
  }
}


