var statusElement = document.getElementById('status');
var progressElement = document.getElementById('progress');
var spinnerElement = document.getElementById('spinner');
var canvasElement = document.getElementById('canvas');

canvasElement.addEventListener("webglcontextlost", function(e) { document.location.reload(); }, false);


//LavaVu args - can be parsed in url params
window.args = window.location.search.substr(1).split('&');
//window.args.push("-v")

var Module = {
  arguments: window.args,
  preRun: [],
  postRun: [],
  print: (function() {
    var element = null;
    if ("-v" in args) {
      element = document.getElementById('output');
      element.style.display = 'block';
      if (element) element.value = ''; // clear browser cache
    }
    return function(text) {
      if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
      console.log(text);
      if (element) {
        element.value += text + "\n";
        element.scrollTop = element.scrollHeight; // focus on bottom
      }
    };
  })(),

  printErr: function(text) {
    if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
    //console.error(text);
    console.log(text);
    //Module.print(text);
  },

  canvas: (function() {
    var canvas = document.getElementById('canvas');
    return canvas;
  })(),

  setStatus: function(text) {
    if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
    if (text === Module.setStatus.last.text) return;
    var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
    var now = Date.now();
    if (m && now - Module.setStatus.last.time < 30) return; // if this is a progress update, skip it if too soon
    Module.setStatus.last.time = now;
    Module.setStatus.last.text = text;
    if (m) {
      text = m[1];
      progressElement.value = parseInt(m[2])*100;
      progressElement.max = parseInt(m[4])*100;
      progressElement.hidden = false;
      spinnerElement.hidden = false;
    } else {
      progressElement.value = null;
      progressElement.max = null;
      progressElement.hidden = true;
      if (!text) spinnerElement.style.display = 'none';
    }
    statusElement.innerHTML = text;
  },
  locateFile: function(path, prefix) {
    // Source from github by default
    if (path.endsWith("LavaVu.wasm") || path.endsWith("LavaVu.data")) return "https://cdn.jsdelivr.net/gh/lavavu/lavavu.github.io@LAVAVU_VERSION/" + path;
    // otherwise, use the default, the prefix (JS file's dir) + the path
    return prefix + path;
  },
  totalDependencies: 0,
  monitorRunDependencies: function(left) {
    this.totalDependencies = Math.max(this.totalDependencies, left);
    Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
  }
};
Module.setStatus('Downloading...');
window.onerror = function(event) {
  // TODO: do not warn on ok events like simulating an infinite loop or exitStatus
  Module.setStatus('Exception thrown, see JavaScript console');
  spinnerElement.style.display = 'none';
  Module.setStatus = function(text) {
    if (text) Module.printErr('[post-exception status] ' + text);
  };
};

//https://stackoverflow.com/questions/54466870/emscripten-offer-to-download-save-a-generated-memfs-file
//https://motley-coder.com/2019/04/01/download-files-emscripten/
function download(filenamePtr, mimePtr) {
  const filename = UTF8ToString(filenamePtr)
  let mime = UTF8ToString(mimePtr)
  if (mime.length == 0)
    mime = "application/octet-stream";

  let content = Module.FS.readFile(filename);
  console.log(`Offering download of "${filename}", with ${content.length} bytes...`);

  var a = document.createElement('a');
  a.download = filename;
  a.href = URL.createObjectURL(new Blob([content], {type: mime}));
  a.style.display = 'none';

  document.body.appendChild(a);
  a.click();
  setTimeout(() => {
    document.body.removeChild(a);
    URL.revokeObjectURL(a.href);
  }, 2000);
}
window.download = download
window.reload_flag = false;
window.resized = true;
window.commands = [];

//Flag window resize
window.onresize = function() {
  window.resized = true;
  clear_mods();
};

//Hack for firefox to fix mod key state bugs in emscripten glfw
window.set_mods = function(e) {
  window.m_alt = e.getModifierState('Alt');
  window.m_shift = e.getModifierState('Shift');
  window.m_ctrl = e.getModifierState('Control');
  //console.log(e.key + " ALT: " + window.m_alt + " SHIFT: " + window.m_shift + " CTRL: " + window.m_ctrl);
}
window.clear_mods = function() {
  window.m_alt = false;
  window.m_shift = false;
  window.m_ctrl = false;
  //console.log(" ALT: " + window.m_alt + " SHIFT: " + window.m_shift + " CTRL: " + window.m_ctrl);
}
window.onkeydown = function(e){
  set_mods(e);
}
window.onkeyup = function(e){
  set_mods(e);
}

//Switch the theme, called when background changed
var light_theme = "dat-gui-light-theme.css";

window.set_light_theme = function(enabled) {
  for (var i = 0; i < document.styleSheets.length; i++) {
    var href = document.styleSheets[i].href;
    if (href && href.includes(light_theme)) {
      document.styleSheets[i].disabled = !enabled;
      return;
    }
  }

  //If we reach here, light theme is not loaded, add it
  if (enabled) {
    var head = document.getElementsByTagName("head")[0];
    var fileref = document.createElement("link")
    fileref.setAttribute("rel", "stylesheet")
    fileref.setAttribute("type", "text/css")
    fileref.setAttribute("href", light_theme)
    head.appendChild(fileref);
  }
}

// https://stackoverflow.com/questions/47313403/passing-client-files-to-webassembly-from-the-front-end
function useFileInput(fileInput) {
  if (fileInput.files.length == 0)
    return;
  //TODO: support multiple?
  var file = fileInput.files[0];

  var fr = new FileReader();
  fr.onload = function () {
    var data = new Uint8Array(fr.result);

    Module['FS_createDataFile']('/', file.name, data, true, true, true);
    window.commands.push("clear all; file " + file.name);

    fileInput.value = '';
  };
  fr.readAsArrayBuffer(file);
}
