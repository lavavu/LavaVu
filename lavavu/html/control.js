//Maintain a list of interactor instances opened by id
//(check in case this script imported twice, don't overwrite previous)
var _wi = window._wi ? window._wi : [];
var debug_mode = false;

function WindowInteractor(id, port) {
  //Interactor class, handles javascript side of window control
  //Takes viewer id, command server port optional
  //(otherwise assumes command server same as page server)
  // - set as active target for commands
  // - init webgl bounding box
  // - display initial image

  //Store self in list and save id
  this.id = id;
  var loc = window.location;
  this.baseurl = loc.protocol + "//" + loc.hostname + (loc.port ? ":" + loc.port : "");
  if (port) {
    this.proxyurl = this.baseurl + "/proxy/" + port;
    this.baseurl = loc.protocol + "//" + loc.hostname + ":" + port;
    //Check for jupyter-server-proxy support
    var that = this;
    var xhttp = new XMLHttpRequest();
    xhttp.onload = function() {
      //Success? Use the proxy url
      if (xhttp.status != 404)
        that.baseurl = that.proxyurl;
      console.log("--- Proxy request attempted, status: " + xhttp.status);
      console.log("--- Connected to LavaVu via " + that.baseurl);

      //Ready to initialise
      that.init();
    } 
    xhttp.open('GET', this.proxyurl + "/connect?" + new Date().getTime(), true);
    xhttp.send();
  }
}

WindowInteractor.prototype.init = function() {
  console.log("WindowInteractor: " + this.id + " baseurl: " + this.baseurl);

  this.img = document.getElementById("imgtarget_" + this.id);

  //No window
  if (!this.img) return;

  //Load frame image and run command in single action
  this.instant = true;

  //Initial image
  //(Init WebGL bounding box interaction on load)
  var that = this;
  this.get_image(function() {
    //console.log('In image loaded callback ' + that.id);
    //Init on image load with callback function to execute commands
    that.box = initBox(that.img, function(cmd) {that.execute(cmd);});
    console.log("Box init on " + that.id);
    //Clear onload
    that.img.onload = null;
    //Update the box size by getting state
    that.get_state();
  });
}

WindowInteractor.prototype.execute = function(cmd, callback) {
  //console.log("execute: " + cmd);
  if (!this.box) {
    console.log("ABORT, not yet initialised. Skip executing: " + cmd);
    return;
  }

  //HTTP interface
  //Replace newlines with semi-colon first
  cmd = cmd.replace(/\n/g,';');
  var url = "";
  if (this.instant && this.img) {
    //Base64 encode to avoid issues with jupyterlab and command urls
    cmd = '_' + window.btoa(cmd);
    url = this.baseurl + "/icommand=" + cmd + "?" + new Date().getTime();
    //this.img.onload = null; //This breaks interact while timestepper animating
    if (callback)
      this.img.onload = callback;
    this.img.src = url;
  } else {
    url = this.baseurl + "/command=" + cmd + "?" + new Date().getTime()
    var xhttp = new XMLHttpRequest();
    xhttp.open('GET', url, true);
    xhttp.send();
    this.get_image(callback);
  }
  //console.log("URL: " + url);

  //Reload state (this seems to get called twice sometimes, with empty response on 2nd)
  //(skip if interacting)
  if (!this.box.canvas.mouse.isdown && !this.box.zoomTimer)
    this.get_state();

  return false;
}

WindowInteractor.prototype.set_prop = function(obj, prop, val) {
  this.execute("select " + obj + "; " + prop + "=" + val);
  this.get_image();
}

WindowInteractor.prototype.do_action = function(id, val) {
  //HTML control actions via http
  this.actions[id](val);

  //Reload state - this happens in action execute anyway, so not needed?
  //this.get_state();
}

WindowInteractor.prototype.redisplay = function() {
  //console.log("redisplay: " + this.id);
  if (this.img && this.box) {
    //Call get_image
    this.get_image();
    //Update the box size by getting state
    this.get_state();
  }
}

WindowInteractor.prototype.get_image = function(onload) {
  if (!this.img) return;
  //console.log("get_img: " + this.id);

  //if (!this.img) this.img = document.getElementById('imgtarget_0');
  var url = this.baseurl + "/image?" + new Date().getTime();
  if (this.img) {
    this.img.onload = onload;
    this.img.src = url;
  }
}

WindowInteractor.prototype.get_state = function() {
  //Reload state
  //console.log("get_state called by " + this.get_state.caller);
  //if (!this.img) return; //Needed?
  var that = this;
  var box = that.box;
  var onget = function(data) { box.loadFile(data); };
  var url = that.baseurl + "/getstate?" + new Date().getTime();
  var xhttp = new XMLHttpRequest();
  xhttp.onload = function() {
    if (xhttp.status == 200)
      onget(xhttp.response);
    else  
      console.log("Ajax Request Error: " + url + ", returned status code " + xhttp.status + " " + xhttp.statusText);
  } 
  xhttp.open('GET', url, true);
  xhttp.send();
}

