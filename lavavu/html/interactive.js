/////////////////////////////////////////////////////////////////////////
//Server Event handling
var defaultMouse;

var IMAGE_TIMER = 20
var MOUSE_TIMER = 5
var HIDE_TIMER = 1000

function initServerPage() {
  //Use image frame
  img = document.getElementById('frame');
  //Image canvas event handling
  img.mouse = new Mouse(img, new MouseEventHandler(serverMouseClick, serverMouseWheel, serverMouseMove, serverMouseDown));
  defaultMouse = document.mouse = img.mouse;

  //Initiate the server update
  requestData('/connect', parseConnectionRequest);

  /*window.onbeforeunload = function() {
    //In a popup?
    if (window.opener && window.opener !== window) {
      var http = new XMLHttpRequest();
      http.open("GET", "/command=quit", false);
      http.send(null);
    } else if (client_id >= 0) {
      var http = new XMLHttpRequest();
      http.open("GET", "/disconnect=" + client_id, false);
      http.send(null);
    }
    window.close();
  };*/

  //Enable to forward key presses to server directly
  document.onkeypress = keyPress;
  window.onresize = function() {resizeToWindow();};
}

function keyPress(event) {
  var code = 0;
  var key = 0;
  if (event.which && event.charCode != 0)
     code = event.which;
  else
     code = event.keyCode;

  // space and arrow keys, prevent scrolling
  if ([32, 37, 38, 39, 40].indexOf(code) > -1)
    event.preventDefault();

  //Special key codes
  if (code == 38) key = 17;
  else if (code == 40) key = 18;
  else if (code == 37) key = 20;
  else if (code == 39) key = 19;
  else if (code == 33) key = 24;
  else if (code == 34) key = 25;
  else if (code == 36) key = 22;
  else if (code == 35) key = 23;
  else key = code;

  var modifiers = getModifiers(event);

  //Ignore CTRL+R, CTRL+SHIFT+R
  if (key == 114 && modifiers == 'C' || key == 82 && modifiers == 'CS') return;

  //console.log("PRESS Code: " + code + " Key: " + key);
  requestData('/key=' + key + ',modifiers=' + modifiers + ",x=" + defaultMouse.x + ",y=" + defaultMouse.y);
}

function getModifiers(event) {
  var modifiers = '';
  if (event.ctrlKey) modifiers += 'C';
  if (event.shiftKey) modifiers += 'S';
  if (event.altKey) modifiers += 'A';
  if (event.metaKey) modifiers += 'M';
  return modifiers;
}

//Mouse event handling
function serverMouseClick(event, mouse) {
  if (event.button > 0) return true;
  if (mvtimeout) clearTimeout(mvtimeout);  //Clear move timer
  requestData('/mouse=up,button=' + (mouse.button+1) + ',modifiers=' + getModifiers(event) + ",x=" + mouse.x + ",y=" + mouse.y);
}

function serverMouseDown(event, mouse) {
  var button = mouse.button+1;
  requestData('/mouse=down,button=' + button + ",x=" + mouse.x + ",y=" + mouse.y);
  return false; //Prevent drag
}

var mvtimeout;
var spintimeout;
var spincount = 0;
var hideTimer;

function serverMouseMove(event, mouse) {
  //Mouseover processing
  if (mouse.x >= 0 && mouse.y >= 0 && mouse.x <= mouse.element.width && mouse.y <= mouse.element.height)
  {
    //Convert mouse coords
    //...
    //document.getElementById("coords").innerHTML = "&nbsp;x: " + mouse.x + " y: " + mouse.y;
  }

  var modeDiv = document.getElementById('mode');
  var rect = mouse.element.getBoundingClientRect();
  modeDiv.style.display = "block";
  modeDiv.disabled = false;
  if (hideTimer)
    clearTimeout(hideTimer);
  hideTimer = setTimeout(function () {modeDiv.style.display = "none"; modeDiv.disabled=true; }, HIDE_TIMER );

  //Following is for drag action only
  if (!mouse.isdown) return true;

  //Right & middle buttons: drag to scroll
  if (mouse.button > 0) {
    // Set the scroll position
    //window.scrollBy(-mouse.deltaX, -mouse.deltaY);
    //return true;
  }

  if (mvtimeout) clearTimeout(mvtimeout);

  //Switch buttons for translate/rotate
  var button = mouse.button;

  var mode = document.getElementById('mode_select');
  var modeStr = mode.options[mode.selectedIndex].value;

  if (modeStr == "Translate") {
    //Swap rotate/translate buttons
    if (button == 0)
      button = 2
    else if (button == 2)
      button = 0;
  } else if (button==0 && modeStr == "Zoom") {
    if (mouse.previousX) {
      //event.spin = (mouse.x - mouse.previousX) / 5.0;
      //console.log(event.spin);
      //serverMouseWheel(event, mouse);
      spincount += Math.sign(mouse.x - mouse.previousX);
      var request = "/mouse=scroll,spin=" + spincount + ",modifiers=C,x=" + document.mouse.x + ",y=" + document.mouse.y;
      spintimeout = setTimeout("requestData('" + request + "'); spincount = 0;", MOUSE_TIMER);
    }
    mouse.previousX = mouse.x;
    return false;
  }

  //Drag processing
  //requestData('/mouse=move,button=' + (mouse.button+1) + ",x=" + mouse.x + ",y=" + mouse.y);
  //document.body.style.cursor = "wait";
  var request = "/mouse=move,button=" + button + ",x=" + document.mouse.x + ",y=" + document.mouse.y;
  mvtimeout = setTimeout("requestData('" + request + "');", MOUSE_TIMER);
  return false;
}

function serverMouseWheel(event, mouse) {
  if (spintimeout) clearTimeout(spintimeout);
  //document.body.style.cursor = "wait";
  spincount += event.spin;
  var request = "/mouse=scroll,spin=" + Math.floor(spincount) + ',modifiers=' + getModifiers(event) + ",x=" + document.mouse.x + ",y=" + document.mouse.y;
  spintimeout = setTimeout("requestData('" + request + "'); spincount = 0;", MOUSE_TIMER);
  //requestData('/mouse=scroll,spin=' + event.spin + ',modifiers=' + getModifiers(event) + ",x=" + mouse.x + ",y=" + mouse.y);
}

///////////////////////////////////////////////////////
var count = 0;
function requestData(data, callback) {
  var http = new XMLHttpRequest();
  // the url of the script where we send the asynchronous call
  var url = data.replace(/\n/g, ';'); //Replace newlines with semi-colon
  //console.log(url);

  //Add count to url to prevent caching
  if (data) {
    url += "&" + count;
    count++;
  }

  http.onreadystatechange = function() {
    if(http.readyState == 4)
      if(http.status == 200) {
        if (callback)
          callback(http.responseText);
        //else
        //  console.log(http.responseText);
      } else
        console.log("Request Error: " + url + ", returned status code " + http.status + " " + http.statusText);
  }

  //Add date to url to prevent caching
  //var d = new Date();
  //http.open("GET", url + "?d=" + d.getTime(), true);
  http.open("GET", url, true);
  http.send(null);
}

var imgtimer;
var client_id = 0;
function requestImage(target) {
  if (client_id < 0) return; //No longer connected
  var http = new XMLHttpRequest();
  //Add count to url to prevent caching
  var url = '/image=' + client_id + '&' + count; count++;
  //console.log(url);

  http.onload = function() {
    if(http.status == 200) {
      //Clean up when loaded
      target.onload = function(e) {
        window.URL.revokeObjectURL(target.src);
      };
      target.src = window.URL.createObjectURL(http.response);

      //Request next image frame (after brief timeout so we don't flood the server)
      if (imgtimer) clearTimeout(imgtimer);
      imgtimer = setTimeout(requestImage(target), IMAGE_TIMER);
    } else
      console.log("Request Error: " + url + ", returned status code " + http.status + " " + http.statusText);
  }

  http.open("GET", url, true);
  http.responseType = 'blob';
  http.send(null);
}

//Get client_id after connect call
function parseConnectionRequest(response) {
  console.log("Connecting...");
  client_id = parseInt(response);

  resizeToWindow(0);

  //Ensure model open and viewports initialised
  setTimeout("requestData('/command=open');", IMAGE_TIMER);

  //Get first frame
  var target = document.getElementById('frame');
  if (target)
    requestImage(target);
}

var resizetimer;
function resizeToWindow(timer=100) {
  if (resizetimer) clearTimeout(resizetimer);
  var cmd = '/command=resize ' + window.innerWidth + ' ' + window.innerHeight;
  console.log(cmd);
  resizetimer = setTimeout('requestData("' + cmd + '")', timer);
}


