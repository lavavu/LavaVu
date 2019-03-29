//Maintain a list of interactor instances opened by id
//(check in case this script imported twice, don't overwrite previous)
var _wi = window._wi ? window._wi : [];
var debug_mode = false;

function WindowInteractor(id, uid, port) {
  //Interactor class, handles javascript side of window control
  //Takes viewer id, viewer python id, command server port optional
  //(otherwise assumes command server same as page server)
  // - set as active target for commands
  // - init webgl bounding box
  // - display initial image

  //Store self in list and save id
  this.id = id;
  this.uid = uid;
  var loc = window.location;

  //Request type settings
  this.instant = true; //false; //Use image.src to issue commands, combines into single request;
  this.post = false; //Set this to use POST instead of GET

  //Connection attempts via this function, pass url
  var that = this;
  var connect = function(url) {
    var xhttp = new XMLHttpRequest();
    xhttp.onload = function() {
      //Success? Use current url
      if (xhttp.status == 200) {
        if (!that.baseurl) {
          if (that.uid && that.uid != parseInt(xhttp.response)) {
            console.log("--- Connection OK, but UID does not match! " + url + " : " + that.uid + " != " + xhttp.response);
          } else {
            console.log("--- Connected to LavaVu via " + url + " UID: " + that.uid);
            that.baseurl = url;
            //Ready to initialise
            that.init();
          }
        } else {
          console.log("--- Connection OK, but already connected: " + url)
        }
      } else {
        console.log("--- Connection failed on : " + url);
      }
    }
    //Catch errors, we expect some as not all urls will work
    try {
      xhttp.open('GET', url + "/connect?" + new Date().getTime(), true);
      xhttp.send();
    } catch (err) {
      console.log(err.message);
    }
  }

  //Possible connection modes:
  // 1) No port provided, assume running on same port/address as current page
  // 2) Port provided, running on this port, accssible via hostname:port
  // 3) Port provided, running on this port, accssible via hostname/proxy/port (jupyter-server-proxy)
  // 4) Port provided, running on this port, accessible via localhost:port (google colab auto-translated proxy)

  //Call connect function for each url
  //First to succeed will be used
  console.log("Attempting to connect to LavaVu server");
  if (!port) {
    //No port provided? We are the server,
    //Just use the same address for requests
    connect(loc);
  } else {

    //Several possible modes to try
    if (loc.hostname == 'hub.mybinder.org') {
      var regex = /\/user\/[a-z0-9-]+\//i;
      var base = regex.exec(loc.pathname)[0];
      connect(loc.protocol + "//hub.mybinder.org" + base + "proxy/" + port);
      return;
    }
    if (loc.protocol != 'file:') {
      //(Don't bother for file:// urls)
      connect(loc.protocol + "//" + loc.hostname + ":" + port);
      connect(loc.protocol + "//" + loc.hostname + (loc.port ? ":" + loc.port : "") + "/proxy/" + port);
    }
    if (loc.hostname != "localhost") {
      connect("https://localhost:" + port);
      //connect("http://localhost:" + port);
      connect("http://127.0.0.1:" + port);
      //connect("https://127.0.0.1:" + port);
    }
  }
}

WindowInteractor.prototype.init = function() {
  console.log("WindowInteractor: " + this.id + " baseurl: " + this.baseurl);

  this.img = document.getElementById("imgtarget_" + this.id);

  //No window
  if (!this.img) return;

  //Load frame image and run command in single action

  //Initial image
  //(Init WebGL bounding box interaction on load)
  var that = this;
  this.get_image(function() {
    //console.log('In image loaded callback ' + that.id);
    //Init on image load with callback function to execute commands
    that.box = initBox(that.img, function(cmd) {that.execute(cmd);});
    console.log("Window initialised, id: " + that.id);
    //Clear onload
    that.img.onload = null;
    //Update the box size by getting state
    that.get_state();
  });
}

WindowInteractor.prototype.execute = function(cmd, callback) {
  //console.log(this.id + " execute: " + cmd);
  var that = this;
  var final_callback = function(response) {
    if (callback)
      callback(response);

    //Update state
    that.get_state();
  }

  //Replace newlines with semi-colon first
  if (cmd.charAt(0) != '{')
    cmd = cmd.replace(/\n/g,';');

  //Use IMG.SRC to issue the command and retrieve new image in single action
  if (this.instant && this.img) {
    cmd = '_' + window.btoa(cmd); //Base64 encode to avoid issues with jupyterlab and command urls
    var url = this.baseurl + "/icommand=" + cmd + "?" + new Date().getTime();
    //this.img.onload = null; //This breaks interact while timestepper animating
    this.img.onload = final_callback;
    this.img.src = url;
  //Use HTTP POST or GET to issue command and IMG.SRC to get image
  } else {
    var xhttp = new XMLHttpRequest();
    var url = this.baseurl;
    var params = undefined;
    if (this.post) {
      xhttp.open('POST', url, true);
      params = cmd;
      //console.log("POST: " + params);
    } else {
      cmd = '_' + window.btoa(cmd); //Base64 encode to avoid issues with jupyterlab and command urls
      url = this.baseurl + "/icommand=" + cmd + "?" + new Date().getTime()
      xhttp.open('GET', url, true);
    }
    xhttp.onload = function() {
      if (xhttp.status == 200) {
        that.get_image();
        final_callback(xhttp.response);
      } else
        console.log("Ajax Error: " + url + ", returned status code " + xhttp.status + " " + xhttp.statusText);
    }
    xhttp.send(params);
  }
}

WindowInteractor.prototype.do_action = function(id, val, el) {
  //HTML control actions via http, replace value first
  var script = this.actions[id];
  //console.log(id + " : " + val);
  script = script.replace(/---VAL---/g, val);
  this.execute(script);
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
  if (!this.box) {
    //console.log("Not yet initialised. Skip get_state");
    //No box interactor
    //if this interactor has value controls only,
    //trigger redisplay on matching interactive window if any
    for (var w in _wi) {
      if (_wi[w] != this && _wi[w].uid == this.uid) {
        console.log(this.id + ': Redisplaying on matching controller found: ' + w)
        _wi[w].redisplay();
      }
    }
    return;
  }
  //console.log("get_state called by " + this.get_state.caller);
  if (!this.img) return; //Skip for control only interator
  var that = this;
  var box = this.box;
  var onget = function(data) { box.loadFile(data); };
  var url = this.baseurl + "/getstate?" + new Date().getTime();
  var xhttp = new XMLHttpRequest();
  xhttp.onload = function() {
    if (xhttp.status == 200) {
      //Success, callback
      onget(xhttp.response);
      that.redisplay_reset();
    } else
      console.log("Ajax Request Error: " + url + ", returned status code " + xhttp.status + " " + xhttp.statusText);
  } 
  xhttp.open('GET', url, true);
  xhttp.send();
}

WindowInteractor.prototype.redisplay_reset = function() {
  //Auto redisplay on idle - should be called on successful image load
  if (!this.img) return;

  //Redisplay interval / keep-alive (10 seconds)
  //Reset the timer whenever get_state called, should only trigger after idle period
  //console.log("REDISPLAY RESET " + this.id);
  var that = this;
  if (this.redisplay_timer)
    clearTimeout(this.redisplay_timer);

  //Image element no longer exists? Skip
  if (!document.getElementById("imgtarget_" + this.id)) return;

  this.redisplay_timer = setTimeout(function() { console.log("Redisplay " + that.id); that.redisplay(); }, 10000);
}

