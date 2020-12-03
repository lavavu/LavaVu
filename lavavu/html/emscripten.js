var statusElement = document.getElementById('status');
var progressElement = document.getElementById('progress');
var spinnerElement = document.getElementById('spinner');
var canvasElement = document.getElementById('canvas');

canvasElement.addEventListener("webglcontextlost", function(e) { canvasElement.style.display = 'none'; Module.setStatus('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

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

