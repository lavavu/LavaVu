"""
LavaVu python interface

Interactive HTML UI controls library
These functions can be called from the ControlFactory provided on

- lavavu.Viewer()
- lavavu.Object()
- lavavu.Properties()

Some work only on the viewer and some only to objects and some to both.
Each will provide a control that allows modifying visualisation properties or executing commands interactively
in an IPython notebook environment

Example
-------

Create a Button control to execute a rotation command:

>>> lv = lavavu.Viewer()
>>> lv.control.Button('rotate x 1')

Create a Range control to adjust the pointsize property:

>>> lv = lavavu.Viewer()
>>> pts = lv.points()
>>> pts.control.Range('pointsize', range=[1, 10])

For convenience, controls to set visualisation properties can often be created by calling control directly with the property name
This will create a control of the default type for for that property if one is defined, e.g.

>>> lv = lavavu.Viewer()
>>> pts = lv.points()
>>> pts.control('pointsize')
"""

__all__ = ['Button', 'Checkbox', 'Colour', 'ColourMapList', 'ColourMaps', 'Command', 'Divider', 'DualRange', 'Entry', 'File', 'Filter', 'Gradient', 'List', 'Number', 'Number2D', 'Number3D', 'ObjectList', 'ObjectSelect', 'Panel', 'Range', 'Range2D', 'Range3D', 'Rotation', 'Tabs', 'TimeStepper', 'Window']

import os
import sys
import time
import datetime
import json
from vutils import is_ipython, is_notebook
import weakref
import string
import re
from random import Random

#Register of windows (viewer instances)
windows = []
#Window unique ids
winids = []
#Generate unique strings for controls and windows
id_random = Random() #Ensure we use our own default seed in case set in notebook
def gen_id(length=10):
    alphabet = string.ascii_letters + string.digits
    try:
        return ''.join(id_random.choices(alphabet, k=length))
    except:
        #Python 3.5
        return ''.join(id_random.choice(string.ascii_uppercase + string.ascii_lowercase + string.digits) for _ in range(length))

basehtml = """
<html>

<head>
<title>LavaVu Interface</title>
<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">

<style>
html, body, table, form {
  padding: 0; margin: 0;
  border: none;
  font-size: 9pt;
  font-family: sans-serif;
}
p {margin: 0px 2px; }

.canvas {
  width: 100%; height: 100%;
}

canvas {
  z-index: 0; margin: 0px; padding:0px;
  border: none;
}

</style>

---SCRIPTS---

</head>

<body onload="---INIT---" oncontextmenu="return false;">
---CONTENT---
</body>

</html>
"""

inlinehtml = """
---SCRIPTS---
<div id="---ID---" style="display: block; width: ---WIDTH---px; height: ---HEIGHT---px;"></div>
---CONTENT---

<script>
---INIT---
</script>
"""

#Some common elements (TODO: dynamically create these when not found)
hiddenhtml = """
<div id="progress" class="popup" style="display: none; width: 310px; height: 32px;">
  <span id="progressmessage"></span><span id="progressstatus"></span>
  <div id="progressbar" style="width: 300px; height: 10px; background: #58f;"></div>
</div>

<div id="hidden" style="display: none">
  <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAkAAAAPCAYAAAA2yOUNAAAAj0lEQVQokWNIjHT8/+zZs//Pnj37/+TJk/9XLp/+f+bEwf9HDm79v2Prqv9aKrz/GUYVEaeoMDMQryJXayWIoi0bFmFV1NWS+z/E1/Q/AwMDA0NVcez/LRsWoSia2luOUAADVcWx/xfO6/1/5fLp/1N7y//HhlmhKoCBgoyA/w3Vyf8jgyyxK4CBUF8zDAUAAJRXY0G1eRgAAAAASUVORK5CYII=" id="slider">
  <canvas id="gradient" width="2048" height="1"></canvas>
  <canvas id="palette" width="512" height="24" class="palette checkerboard"></canvas>
</div>
"""

#Global data (script) - set from lavavu
jsglobals = ''

#Static HTML location
htmlpath = ""

def _isviewer(target):
    """Return true if target is a viewer"""
    #return not hasattr(target, "parent")
    return target.__class__.__name__ == 'Viewer'

def _isobject(target):
    """Return true if target is a lavavu object"""
    return target.__class__.__name__ == 'Object'

def _getviewer(target):
    """Return its viewer if target is vis object
    otherwise just return target as if is a viewer"""
    if not _isviewer(target):
        return target.parent
    return target

def _getproperty(target, propname):
    """Return value of property from target
    if not defined, return the default value for this property
    """
    _lv = _getviewer(target)
    if propname in target:
        return target[propname]
    elif propname in _lv.properties:
        #Get property default
        prop = _lv.properties[propname]
        return prop["default"]
    else:
        return None

_file_cache = dict()

def _webglcode(shaders, css, scripts, menu=True, lighttheme=True, stats=False):
    """
    Returns base WebGL code, by default using full WebGL renderer (draw.js)
    Pass 'drawbox.js' for box interactor only
    """
    code = shaders
    if menu:
        scripts[1] += ['menu.js']
        #Following scripts need require disabled
        scripts[0] += ['dat.gui.min.js']
        #Stats module for WebVR
        if stats:
            scripts[0] += ['stats.min.js']
        if lighttheme:
            css.append("dat-gui-light-theme.css")
        #Some css tweaks
        css.append("gui.css")

    else:
        #DAT.gui disabled
        scripts[0] += ['!GUI_OFF']

    #First scripts list needs require.js disabled
    code += _getjslibs(['!REQUIRE_OFF'] + scripts[0] + ['!REQUIRE_ON'] + scripts[1])

    code += _getcss(css)
    code += '<script>\n' + jsglobals + '\n</script>\n'
    return code

def _webglviewcode(shaderpath, menu=True, lighttheme=True):
    """
    Returns WebGL base code for an interactive visualisation window
    """
    jslibs = [['gl-matrix-min.js'], ['OK-min.js', 'draw.js']]
    return _webglcode(_getshaders(shaderpath), ['styles.css'], jslibs, menu=menu, lighttheme=lighttheme)

def _webglboxcode(menu=True, lighttheme=True):
    """
    Returns WebGL base code for the bounding box interactor window
    """
    jslibs = [['gl-matrix-min.js'], ['OK-min.js', 'control.js', 'drawbox.js']]
    return _webglcode('', ['control.css', 'styles.css'], jslibs, menu=menu, lighttheme=lighttheme)

def _connectcode(target):
    """
    Returns base code to open a connection only, for obtaining correct connection url
    (Only required if no interaction windows have been opened yet)
    """
    #If connection via window already opened, we don't need this
    if len(winids) == 0:
        initjs = '\n_wi[0] = new WindowInteractor(0, {uid}, {port});\n;'.format(uid=id(target), port=target.port)
        #return _readfilehtml('control.js') + initjs
        s = '<script>\n' + _readfilehtml('control.js') + initjs + '</script>\n'
        return s
    else:
        return ""

def _getcss(files=["styles.css"]):
    #Load stylesheets to inline tag
    return _filestohtml(files, tag="style")

def _getjslibs(files):
    #Load a set of combined javascript libraries in script tags
    return _filestohtml(files, tag="script")

def _filestohtml(files, tag="script"):
    #Load a set of files into a string enclosed in specified html tag
    code = '<' + tag + '>\n'
    for f in files:
        code += _readfilehtml(f)
    code += '\n</' + tag + '>\n'
    return code

def _readfilehtml(filename):
    #HACK: Need to disable require.js to load libraries from inline script tags
    if filename == "!REQUIRE_OFF":
        return '_backup_define = window.define; window.define = undefined;'
    elif filename == "!REQUIRE_ON":
        return 'window.define = _backup_define; delete _backup_define;'
    elif filename == "!GUI_OFF":
        return 'window.dat = undefined;'

    #Read a file from the htmlpath (or cached copy)
    global _file_cache
    if not filename in _file_cache:
        _file_cache[filename] = _readfile(os.path.join(htmlpath, filename))
    return _file_cache[filename]

def _readfile(filename):
    #Read a text file and return contents
    data = ""
    with open(filename, 'r') as f:
        data = f.read()
        f.close()
    return data

def _getshaders(path, shaders=['points', 'lines', 'triangles', 'volume']):
    #Load combined shaders
    src = '<script>\nvar shaders = {};\n'
    sdict = {'points' : 'point', 'lines' : 'line', 'triangles' : 'tri', 'volume' : 'volume'};
    for key in shaders:
        vs = _readfile(os.path.join(path, sdict[key] + 'Shader.vert'))
        fs = _readfile(os.path.join(path, sdict[key] + 'Shader.frag'))
        vs = vs.replace('"', '\\"').replace('\n', '\\n')
        fs = fs.replace('"', '\\"').replace('\n', '\\n')
        #vs = re.sub("^\s*in ", 'attribute ', vs, flags=re.MULTILINE);
        #vs = re.sub("^\s*out ", 'varying ', vs, flags=re.MULTILINE)
        #fs = re.sub("^\s*in ", 'varying ', fs, flags=re.MULTILINE);
        src += 'shaders["' + key + '-vs"] = "' + vs + '";'
        src += 'shaders["' + key + '-fs"] = "' + fs + '";'
    src += '\n</script>\n\n'
    return src

def _getshaders_as_scripts(path, shaders=['points', 'lines', 'triangles', 'volume']):
    #Load shaders into script tags, not compatible with colab for some reason
    src = ''
    sdict = {'points' : 'point', 'lines' : 'line', 'triangles' : 'tri', 'volume' : 'volume'};
    for key in shaders:
        src += '<script id="' + key + '-vs" type="x-shader/x-vertex">\n'
        src += _readfile(os.path.join(path, sdict[key] + 'Shader.vert'))
        src += '</script>\n\n'
        src += '<script id="' + key + '-fs" type="x-shader/x-fragment">\n'
        src += _readfile(os.path.join(path, sdict[key] + 'Shader.frag'))
        src += '</script>\n\n'
    return src

class _Action(object):
    """Base class for an action triggered by a control

    Default action is to run a command
    """

    def __init__(self, target, command=None, readproperty=None):
        self.target = target
        self.command = command
        if not hasattr(self, "property"):
            self.property = readproperty
        self.lastvalue = 0
        self.index = None

    def script(self):
        #Return script action for HTML export
        if self.command is None or not len(self.command): return ""
        #Run a command with a value argument
        return self.command + ' ---VAL---'

class _PropertyAction(_Action):
    """Property change action triggered by a control
    """

    def __init__(self, target, prop, command=None, index=None):
        self.property = prop
        #Default action after property set is redraw, can be set to provided
        #if command is None:
        #    command = "redraw"
        self.command = command
        super(_PropertyAction, self).__init__(target, command)
        self.index = index

    def script(self):
        #Return script action for HTML export
        #Set a property
        #Check for index (3D prop)
        propset = self.property + '=---VAL---'
        if self.index is not None:
            propset = self.property + '[' + str(self.index) + ']=---VAL---'
        # - Globally
        script = ''
        if _isviewer(self.target):
            script = 'select; ' + propset
        # - on an object selector, select the object
        elif type(self.target).__name__ == 'ObjectSelect':
            script = propset
        # - on a properties collection
        elif type(self.target).__name__ == 'Properties':
            script = '$' + str(id(self.target)) + ' ' + self.property + ' ---VAL--- '
        # - On an object
        else:
            script = '<' + self.target["name"] + '>' + propset
        #Add any additional commands
        return script + '; ' + super(_PropertyAction, self).script()


class _CommandAction(_Action):
    """Command action triggered by a control, with object select before command
    """

    def __init__(self, target, command, readproperty):
        self.command = command
        super(_CommandAction, self).__init__(target, command, readproperty)

    def script(self):
        #Return script action for HTML export
        #Set a property
        # - Globally
        script = ''
        if _isviewer(self.target):
            script = 'select; '
        # - on an object selector, select the object
        elif type(self.target).__name__ == 'ObjectSelect':
            script = ''
        # - On an object
        else:
            script = '<' + self.target["name"] + '>'
        #Add the commands
        return script + super(_CommandAction, self).script()

class _FilterAction(_PropertyAction):
    """Filter property change action triggered by a control
    """
    def __init__(self, target, findex, prop, command=None):
        self.findex = findex
        if command is None: command = "redraw"
        self.command = command
        super(_FilterAction, self).__init__(target, prop)

    def script(self):
        #Return script action for HTML export
        #Set a filter range
        cmd = "filtermin" if self.property == "minimum" else "filtermax"
        return 'select ' + self.target["name"] + '; ' + cmd + ' ' + str(self.findex) + ' ---VAL---'
        #propset = "filters=" + json.dumps()
        #script = 'select ' + self.target["name"] + '; ' + propset

class _HTML(object):
    """A class to output HTML controls
    """

    #Parent class for container types
    def __init__(self, label):
        self.label = label
        self.uniqueid()

    def html(self):
        """Return the HTML code"""
        return ''

    def uniqueid(self):
        #Get a unique control identifier
        self.id = gen_id()
        self.elid = "lvctrl_" + self.id

    def labelhtml(self):
        #Default label
        html = ""
        if self.label:
            html += '<p>' + self.label + ':</p>\n'
        return html


class _Container(_HTML):
    """A container for a set of controls
    """
    #Parent class for container types
    def __init__(self, viewer, label=""):
        self.viewer = viewer
        self._content = []
        self._current = 0
        super(_Container, self).__init__(label)

    def add(self, ctrl):
        self._content.append(ctrl)
        return ctrl

    def controls(self):
        return self.html()

    def __iter__(self):
        return self

    def __next__(self):
        if self._current > len(self._content)-1:
            self._current = 0
            raise StopIteration
        else:
            self._current += 1
            return self._content[self._current-1]

    def html(self):
        html = self.labelhtml()
        html += '<div style="padding:0px; margin: 0px; position: relative;" class="lvctrl">\n'
        for i in range(len(self._content)):
            html += self._content[i].controls()
        html += '</div>\n'
        return html

    def scripts(self):
        #Returns script dictionary
        d = {}
        for i in range(len(self._content)):
            d.update(self._content[i].scripts())
        return d

class Window(_Container):
    """
    Creates an interaction window with an image of the viewer frame 
    and webgl controller for rotation/translation

    Parameters
    ----------
    align : str
        Set to "left/right" to align viewer window, default is left. Set to None to skip.
    wrapper : str
        Set the style of the wrapper div, default is empty string so wrapper is enabled with no custom style
        Set to None to disable wrapper
    """
    def __init__(self, viewer, resolution=None, align="left", wrapper=""):
        super(Window, self).__init__(viewer)
        self.align = align
        self.wrapper = wrapper
        if resolution is not None:
            viewer.output_resolution = resolution

    def html(self):
        style = 'min-height: 50px; min-width: 50px; position: relative; display: inline-block; '
        if self.align is not None:
            style += 'float: ' + self.align + ';'
        if self.wrapper is not None:
            style += ' margin-right: 10px;'
        html = ""
        html += '<div style="' + style + '">\n'
        html += '<img id="imgtarget_---VIEWERID---" draggable=false style="margin: 0px; border: 1px solid #aaa; display: inline-block;" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAQAAAAAYLlVAAAAPUlEQVR42u3OMQEAAAgDINe/iSU1xh5IQPamKgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgLtwAMBsGqBDct9xQAAAABJRU5ErkJggg==">\n'
        html += """
           <div style="display: none; z-index: 200; position: absolute; top: 5px; right: 5px;">
             <select onchange="_wi['---VIEWERID---'].box.mode = this.value;">
               <option>Rotate</option>
               <option>Translate</option>
               <option>Zoom</option>
             </select>
             <input type="button" value="Reset" onclick="_wi['---VIEWERID---'].execute('reset');">
           </div>"""
        html += '</div>\n'

        #Display any contained controls
        if self.wrapper is not None:
            html += '<div style="' + self.wrapper + '" class="lvctrl">\n'
        html += super(Window, self).html()
        if self.wrapper is not None:
            html += '</div>\n'
        #html += '<div style="clear: both;">\n'
        return html

class FillWindow(_Container):
    """
    Creates an interaction window with an image of the viewer frame 
    and webgl controller for rotation/translation

    Unlike Window, which uses the size of the returned image to set the interaction canvas size,
    FillWindow creates an interaction canvas that fills the parent element width completely.
    Images from the renderer are requested to match that size without rescaling.

    Parameters
    ----------
    aspect_ratio : tuple (int,int)
        Aspect ratio to calculate the height, eg: (16,9) (default) or (4,3)
    minwidth : int
        Minimum width of the image
    minheight : int
        Minimum height of the image
    """
    def __init__(self, viewer, aspect_ratio=(16,9), minwidth=100, minheight=50):
        super(FillWindow, self).__init__(viewer)
        if aspect_ratio is not None and len(aspect_ratio) == 2:
            viewer.output_resolution = (viewer.output_resolution[0], int(viewer.output_resolution[0] * aspect_ratio[1] / aspect_ratio[0]))
        self.minwidth = minwidth
        self.minheight = minheight

    def html(self):
        style = 'height: 100%; width: 100%; position: relative; display: inline-block; '
        style += 'min-width: ' + str(self.minwidth) + 'px; '
        style += 'min-height: ' + str(self.minheight) + 'px; '
        html = ""
        html += '<div>\n'
        html += '<img id="imgtarget_---VIEWERID---" draggable=false style="' + style + 'margin: 0px; border: 0px; display: inline-block;" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAQAAAAAYLlVAAAAPUlEQVR42u3OMQEAAAgDINe/iSU1xh5IQPamKgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgLtwAMBsGqBDct9xQAAAABJRU5ErkJggg==">\n'
        html += """
           <div style="display: none; z-index: 200; position: absolute; top: 5px; right: 5px;">
             <select onchange="_wi['---VIEWERID---'].box.mode = this.value;">
               <option>Rotate</option>
               <option>Translate</option>
               <option>Zoom</option>
             </select>
             <input type="button" value="Reset" onclick="_wi['---VIEWERID---'].execute('reset');">
           </div>"""
        html += '</div>\n'

        #Display any contained controls
        html += super(FillWindow, self).html()
        return html

class Panel(Window):
    """Creates a control panel with an interactive viewer window and a set of controls
    placed to the left with the viewer aligned to the right
    """
    def __init__(self, *args, **kwargs):
        super(Panel, self).__init__(*args, align="right", wrapper=None, **kwargs)

class Tabs(_Container):
    """Creates a group of controls with tabs that can be shown or hidden

    Parameters
    ----------
    buttons : boolean
        Display the tab buttons for switching tabs
    """
    def __init__(self, target, buttons=True):
        self.tabs = []
        self.buttons = buttons
        super(Tabs, self).__init__(target)

    def tab(self, label=""):
        """Add a new tab, any controls appending will appear in the new tab
        Parameters
        ----------
        label : str
            Label for the tab, if omitted will be blank
        """
        self.tabs.append(label)

    def add(self, ctrl):
        if not len(self.tabs): self.tab()
        self._content.append(ctrl)
        ctrl.tab = len(self.tabs)-1

    def html(self):
        html = """
        <script>
        function openTab_---ELID---(el, tabName) {
          var i;
          var x = document.getElementsByClassName("---ELID---");
          for (i = 0; i < x.length; i++)
             x[i].style.display = "none";  
          document.getElementById("---ELID---_" + tabName).style.display = "block";  

          tabs = document.getElementsByClassName("tab_---ELID---");
          for (i = 0; i < x.length; i++)
            tabs[i].className = tabs[i].className.replace(" lvseltab", "");
          el.className += " lvseltab";
        }
        </script>
        """
        if self.buttons:
            html += "<div class='lvtabbar'>\n"
            for t in range(len(self.tabs)):
                #Add header items
                classes = 'lvbutton lvctrl tab_---ELID---'
                if t == 0: classes += ' lvseltab'
                html += '<a class="' + classes + '" onclick="openTab_---ELID---(this, this.innerHTML)">---LABEL---</a>'
                html = html.replace('---LABEL---', self.tabs[t])
            html += "</div>\n"
        for t in range(len(self.tabs)):
            #Add control wrappers
            style = ''
            if t != 0: style = 'display: none;'
            html += '<div id="---ELID---_---LABEL---" style="' + style + '" class="lvtab lvctrl ---ELID---">\n'
            for ctrl in self._content:
                if ctrl.tab == t:
                    html += ctrl.controls()
            html += '</div>\n'
            html = html.replace('---LABEL---', self.tabs[t])
        html = html.replace('---ELID---', self.elid)
        return html

class _Control(_HTML):
    """
    _Control object

    Parameters
    ----------
    target : Obj or Viewer
        Add a control to an object to control object properties
        Add a control to a viewer to control global proerties and run commands
    property : str
        Property to modify
    command : str
        Command to run
    value : any type
        Initial value of the controls
    label : str
        Descriptive label for the control
    readproperty : str
        Property to read control value from on update (but not modified)
    """

    def __init__(self, target, property=None, command=None, value=None, label=None, index=None, readproperty=None):
        super(_Control, self).__init__(label)
        self.label = label

        #Can either set a property directly or run a command
        self.property = readproperty
        self.target = target
        self.action = None
        if property:
            #Property set
            self.action = _PropertyAction(target, property, command, index)
            if label is None:
                self.label = property.capitalize()
            self.property = property
        elif command:
            #Command only
            self.action = _CommandAction(target, command, readproperty)
            if label is None:
                self.label = command.capitalize()
        else:
            #Assume derived class will fill out the action, this is just a placeholder
            self.action = _Action(target)

        if not self.label:
            self.label = ""

        #Get value from target or default if not provided
        if property is not None:
            if value is None:
                value = _getproperty(target, property)
            else:
                #2d/3D value?
                if index is not None:
                    V = target[property]
                    V[index] = value
                    target[property][index] = V #Set the provided value
                else:
                    target[property] = value #Set the provided value

            #Append reload command from prop dict if no command provided
            _lv = _getviewer(target)
            if property in _lv.properties:
                prop = _lv.properties[property]
                #TODO: support higher reload values
                cmd = ""
                if prop["redraw"] > 1 and not "reload" in str(command):
                    cmd = "reload"
                elif prop["redraw"] > 0 and not "redraw" in str(command):
                    cmd = "reload"
                #Append if command exists
                if command is None:
                    command = cmd
                else:
                   #Pass the value to command if provided
                   command += " ---VAL--- ; " + cmd

        self.value = value
        self._value = value #Store passed initial value

    def onchange(self):
        return "_wi['---VIEWERID---'].do_action('" + str(self.id) + "', this.value, this);"

    def show(self):
        #Show only this control
        html = '<div style="" class="lvctrl">\n'
        html += self.html()
        html += '</div>\n'

    def html(self):
        return self.controls()

    def controls(self, type='number', attribs={}, onchange=""):
        #Input control
        html =  '<input id="---ELID---_{0}" class="---ELID---" type="{0}"'.format(type)
        #Set custom attribute for property controls
        if not "step" in attribs or attribs["step"] is None:
            attribs["step"] = "any"
            #if self.value:
            #    attribs["step"] = abs(self.value) / 100.
            #else:
            #    attribs["step"] = "any"
        for key in attribs:
            html += key + '="' + str(attribs[key]) + '" '
        html += self.attribs()
        html += 'value="' + str(self.value) + '" '
        #Onchange event
        onchange += self.onchange()
        html += 'onchange="' + onchange + '" '
        html += '>\n'
        html = html.replace('---ELID---', self.elid)
        return html

    def attribs(self):
        html = ""
        if self.property:
            #Set custom attribute for property controls
            if _isobject(self.target):
                html += 'data-target="' + str(self.target["name"]) + '" '
            html += 'data-property="' + self.property + '" '
            if self.action.index is not None:
                html += 'data-index="' + str(self.action.index) + '" '
        return html

    def scripts(self):
        #Returns script dictionary
        return {self.id : self.action.script()}

class _MultiControl(_Control):
    """
    _Control object that holds multiple integrated Controls
    """

    def __init__(self, *args, **kwargs):
        super(_MultiControl, self).__init__(*args, **kwargs)
        self._content = []
        self._current = 0

    def add(self, ctrl):
        self._content.append(ctrl)
        return ctrl

    def controls(self):
        return self.html()

    def __iter__(self):
        return self

    def __next__(self):
        if self._current > len(self._content)-1:
            self._current = 0
            raise StopIteration
        else:
            self._current += 1
            return self._content[self._current-1]

    def html(self):
        html = self.labelhtml()
        for i in range(len(self._content)):
            html += self._content[i].controls()
        return html

    def scripts(self):
        #Returns script dictionary
        d = {}
        for i in range(len(self._content)):
            d[self._content[i].id] = self._content[i].action.script()
        return d

class Divider(_Control):
    """A divider element
    """

    def controls(self):
        return '<hr style="clear: both;">\n'

class Number(_Control):
    """A basic numerical input control
    """
    def __init__(self, target=None, property=None, command=None, value=None, label=None, index=None, step=None, readproperty=None):
        super(Number, self).__init__(target, property, command, value, label, index, readproperty)

        #Get step defaults from prop dict
        _lv = _getviewer(target)
        if step is None and property is not None and property in _lv.properties:
            prop = _lv.properties[property]
            #Check for integer type, set default step to 1
            T = prop["type"]
            ctrl = prop["control"]
            #Whole number step size?
            if "integer" in T:
                step = 1
            elif len(ctrl) > 1 and len(ctrl[1]) == 3:
                step = ctrl[1][2]
            elif "real" in T:
                r = 1.0
                #Use given range if any provided
                if len(ctrl) > 1 and len(ctrl[1]) > 1:
                    r = ctrl[1][1] - ctrl[1][0]
                step = r / 100.0

        self.step = step

    def controls(self):
        html = self.labelhtml()
        attribs = {"step" : self.step}
        html += super(Number, self).controls('number', attribs)
        return html + '<br>\n'

class Number2D(_MultiControl):
    """A set of two numeric controls for adjusting a 2D value
    """
    def __init__(self, target, property, label=None, step=None, *args, **kwargs):
        super(Number2D, self).__init__(target, property, label=label, *args, **kwargs)
        if self._value is None:
            self._value = _getproperty(target, property)
        self.ctrlX = self.add(Number(target=target, property=property, value=self._value[0], step=step, label="", index=0))
        self.ctrlY = self.add(Number(target=target, property=property, value=self._value[1], step=step, label="", index=1))

    def controls(self):
        html = self.labelhtml() + self.ctrlX.controls() + self.ctrlY.controls()
        return html.replace('<br>', '') + '<br>'

class Number3D(_MultiControl):
    """A set of three numeric controls for adjusting a 3D value
    """
    def __init__(self, target, property, label=None, step=None, *args, **kwargs):
        super(Number3D, self).__init__(target, property, label=label, *args, **kwargs)
        if self._value is None:
            self._value = _getproperty(target, property)
        self.ctrlX = self.add(Number(target=target, property=property, value=self._value[0], step=step, label="", index=0))
        self.ctrlY = self.add(Number(target=target, property=property, value=self._value[1], step=step, label="", index=1))
        self.ctrlZ = self.add(Number(target=target, property=property, value=self._value[2], step=step, label="", index=2))

    def controls(self):
        html = self.labelhtml() + self.ctrlX.controls() + self.ctrlY.controls() + self.ctrlZ.controls()
        return html.replace('<br>', '') + '<br>'

class Rotation(_MultiControl):
    """A set of six buttons for adjusting a 3D rotation
    """
    def __init__(self, target, *args, **kwargs):
        self.label="Rotation"
        super(Rotation, self).__init__(target, *args, **kwargs)

        self.ctrlX0 = self.add(Button(target=target, command="rotate x -1", label="-X"))
        self.ctrlX1 = self.add(Button(target=target, command="rotate x 1", label="+X"))
        self.ctrlY0 = self.add(Button(target=target, command="rotate y -1", label="-Y"))
        self.ctrlY1 = self.add(Button(target=target, command="rotate y 1", label="+Y"))
        self.ctrlZ0 = self.add(Button(target=target, command="rotate z -1", label="-Z"))
        self.ctrlZ1 = self.add(Button(target=target, command="rotate z 1", label="+Z"))

    def controls(self):
        html = self.labelhtml() + self.ctrlX0.controls() + self.ctrlX1.controls() + self.ctrlY0.controls() + self.ctrlY1.controls() + self.ctrlZ0.controls() + self.ctrlZ1.controls()
        return html.replace('<br>', '') + '<br>'


class Checkbox(_Control):
    """A checkbox control for a boolean value
    """
    def labelhtml(self):
        return '' #'<br>\n'

    def controls(self):
        attribs = {}
        if self.value: attribs = {"checked" : "checked"}
        html = "<label>\n"
        html += super(Checkbox, self).controls('checkbox', attribs)
        html += " " + self.label + "</label><br>\n"
        return html

    def onchange(self):
        return "; _wi['---VIEWERID---'].do_action('" + str(self.id) + "', this.checked ? 1 : 0, this);"

class Range(_Control):
    """A slider control for a range of values

    Parameters
    ----------
    range : list or tuple
        Min/max values for the range
    """
    def __init__(self, target=None, property=None, command=None, value=None, label=None, index=None, range=None, step=None, readproperty=None):
        super(Range, self).__init__(target, property, command, value, label, index, readproperty)

        #Get range & step defaults from prop dict
        _lv = _getviewer(target)
        defrange = [0., 1., 0.]
        T = None
        if property is not None and property in _lv.properties:
            prop = _lv.properties[property]
            #Check for integer type, set default step to 1
            T = prop["type"]
            if "integer" in T:
                defrange[2] = 1
            ctrl = prop["control"]
            if len(ctrl) > 1 and len(ctrl[1]) == 3:
                defrange = ctrl[1]

        if range is None:
            range = defrange[0:2]
        if step is None:
            step = defrange[2]

        self.range = range
        self.step = step
        if not step:
            #Whole number step size?
            r = range[1] - range[0]
            #Has no type from property, or type is not real: Assume a step size of 1 if range max-min > 5 and both are integers
            if (T is None or not "real" in T) and (r >= 5 and range[0] - int(range[0]) == 0 and range[1] - int(range[1]) == 0):
                self.step = 1
            #Range is less than 5 or range values indicate real numbers
            else:
                self.step = r / 100.0

    def controls(self):
        attribs = {"min" : self.range[0], "max" : self.range[1], "step" : self.step}
        html = self.labelhtml()
        html += super(Range, self).controls('number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += super(Range, self).controls('range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        return html + '<br>\n'

class Button(_Control):
    """A push button control to execute a defined command
    """
    def __init__(self, target, command, label=None):
        super(Button, self).__init__(target, None, command, None, label)

    def onchange(self):
        return "_wi['---VIEWERID---'].do_action('" + str(self.id) + "', '', this);"

    def labelhtml(self):
        return ''

    def controls(self):
        html = self.labelhtml()
        html =  '<input class="---ELID---" type="button" value="' + str(self.label) + '" '
        #Onclick event
        html += 'onclick="' + self.onchange() + '" '
        html += '><br>\n'
        html = html.replace('---ELID---', self.elid)
        return html

class Entry(_Control):
    """A generic input control for string values
    """
    def controls(self):
        html = self.labelhtml()
        html += '<input class="---ELID---" type="text" value="" '
        html += self.attribs()
        html += ' onkeypress="if (event.keyCode == 13) { _wi[\'---VIEWERID---\'].do_action(\'---ID---\', this.value.trim(), this); };"><br>\n'
        html = html.replace('---ELID---', self.elid)
        return html.replace('---ID---', self.id)

class Command(_Control):
    """A generic input control for executing command strings
    """
    def __init__(self, *args, **kwargs):
        super(Command, self).__init__(command=" ", label="Command", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <input class="---ELID---" type="text" value="" 
        onkeypress="if (event.keyCode == 13) { var cmd=this.value.trim(); 
        _wi['---VIEWERID---'].do_action('---ID---', cmd ? cmd : 'repeat', this); this.value=''; };"><br>\n
        """
        html = html.replace('---ELID---', self.elid)
        return html.replace('---ID---', self.id)

class File(_Control):
    """A file picker control

    Unfortunately there is no way to get the file path
    """
    def __init__(self, command="file", directory=False, multiple=False, accept="", *args, **kwargs):
        self.options = ""
        if directory:
            self.options += 'webkitdirectory directory '
        elif multiple:
            self.options += 'multiple '
        if accept:
            #Comma separated file types list, e.g. image/*,audio/*,video/*,.pdf
            self.options += 'accept="' + accept + '"'

        super(File, self).__init__(command=command, label="Load File", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <div style="border:solid #aaa 1px; padding:10px; margin-bottom: 5px;">
        <input class="---ELID---" type="file" id="file_selector_---ELID---" ---OPTIONS--- name="files[]"/>
        <output id="list"></output>
        </div>
        <script>
          function fileSelected_---ELID---(event) {
            var output = [];
            for (var i = 0; i < event.target.files.length; i++) {
              var f = event.target.files.item(i);
              output.push('<li><strong>', escape(f.name), '</strong> (', f.type || 'n/a', ') - ',
                           f.size, ' bytes, last modified: ',
                           '</li>');
                _wi['---VIEWERID---'].do_action("---ID---", f.name);
            }
            document.getElementById('list').innerHTML = '<ul>' + output.join('') + '</ul>';
          }
          document.getElementById('file_selector_---ELID---').addEventListener('change', fileSelected_---ELID---, false);            
        </script>
        """
        html = html.replace('---ELID---', self.elid)
        html = html.replace('---OPTIONS---', self.options)
        return html.replace('---ID---', self.id)

class List(_Control):
    """A list of predefined input values to set properties or run commands

    Parameters
    ----------
    options: list
        List of the available value strings
    """
    def __init__(self, target, property=None, options=None, *args, **kwargs):
        #Get default options from prop dict
        if options is None:
            defoptions = []
            _lv = _getviewer(target)
            if property is not None and property in _lv.properties:
                prop = _lv.properties[property]
                ctrl = prop["control"]
                if len(ctrl) > 2 and len(ctrl[2]):
                    defoptions = ctrl[2]
            options = defoptions
        self.options = options
        super(List, self).__init__(target, property, *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += '<select class="---ELID---" id="---ELID---" value="" '
        html += self.attribs()
        html += 'onchange="' + self.onchange() + '">\n'
        for opt in self.options:
            #Each element of options list can be:
            # - dict {"label" : label, "value" : value, "selected" : True/False}
            # - list [value, label, selected]
            # - value only
            if isinstance(opt, dict):
                selected = "selected" if opt.selected else ""
                html += '<option value="' + str(opt["value"]) + '" ' + selected + '>' + opt["label"] + '</option>\n'
            elif isinstance(opt, list) or isinstance(opt, tuple):
                selected = "selected" if len(opt) > 2 and opt[2] else ""
                html += '<option value="' + str(opt[0]) + '" ' + selected + '>' + str(opt[1]) + '</option>\n'
            else:
                html += '<option>' + str(opt) + '</option>\n'
        html += '</select><br>\n'
        html = html.replace('---ELID---', self.elid)
        return html

class Colour(_Control):
    """A colour picker for setting colour properties
    """
    def __init__(self, *args, **kwargs):
        self.style = ""
        super(Colour, self).__init__(command="", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <div class="colourbg checkerboard" style="---STYLE---">
          <div id="---ELID---" ---ATTRIBS--- class="colour ---ELID---" onclick="
            var col = new Colour(this.style.backgroundColor);
            var offset = [this.getBoundingClientRect().left, this.getBoundingClientRect().top];
            var el = this;
            var savefn = function(val) {
              var c = new Colour(0);
              c.setHSV(val);
              el.style.backgroundColor = c.html();
              _wi['---VIEWERID---'].do_action('---ID---', c.html(), el);
            }
            el.picker = new ColourPicker(savefn);
            el.picker.pick(col, offset[0], offset[1]);">
          </div>
        </div>
        <script>
        var el = document.getElementById("---ELID---");
        //Set the initial colour
        var col = new Colour('---VALUE---');
        el.style.backgroundColor = col.html();
        </script>
        """
        html = html.replace('---VALUE---', str(self.value))
        html = html.replace('---ELID---', self.elid)
        html = html.replace('---ATTRIBS---', self.attribs())
        html = html.replace('---STYLE---', self.style)
        return html.replace('---ID---', self.id)

class ColourIndicator(Colour):
    """A small indicator for showing/setting the colour of the object
    """
    def __init__(self, *args, **kwargs):
        super(ColourIndicator, self).__init__(*args, **kwargs)
        #Custom div style:
        self.style="border: #888 1px solid; display: inline-block; width: 20px; height: 20px;"
        self.label = ""
        #Set the colour from cached data so at least the indicated colour is correct
        if not self.target["colour"]:
            self.target["colour"] = self.target.colour.toString()

class Gradient(_Control):
    """A colourmap editor
    """
    def __init__(self, target, *args, **kwargs):
        super(Gradient, self).__init__(target, property="colourmap", command="", *args, **kwargs)
        #Get and save the map id of target object
        if _isviewer(target):
            raise(Exception("Gradient control requires an Object target, not Viewer"))
        self.maps = target.parent.state["colourmaps"]
        self.map = None
        for m in self.maps:
            if m["name"] == self.value:
                self.map = m
        self.selected = -1;

    def controls(self):
        html = self.labelhtml()
        html += """
        <canvas id="---ELID---_canvas" ---ATTRIBS--- width="512" height="24" class="palette checkerboard">
        </canvas>
        <script>
        var el = document.getElementById("---ELID---_canvas"); //Get the canvas
        //Store the maps
        el.colourmaps = ---COLOURMAPS---;
        el.currentmap = ---COLOURMAP---;
        el.selectedIndex = ---SELID---;
        if (!el.gradient) {
          //Create the gradient editor
          el.gradient = new GradientEditor(el, function(obj, id) {
            //Gradient updated
            //var colours = obj.palette.toJSON()
            el.currentmap = obj.palette.get(el.currentmap);
            _wi['---VIEWERID---'].do_action('---ID---', JSON.stringify(el.currentmap.colours));
            //_wi['---VIEWERID---'].do_action('---ID---', obj.palette.toJSON(), el);

            //Update stored maps list by name
            if (el.selectedIndex >= 0)
              el.colourmaps[el.selectedIndex].colours = el.currentmap.colours; //el.gradient.palette.get();
          }
          , true); //Enable premultiply
        }
        //Load the initial colourmap
        el.gradient.read(el.currentmap.colours);
        </script>
        """
        mapstr = json.dumps(self.maps)
        html = html.replace('---COLOURMAPS---', mapstr)
        if self.map:
            html = html.replace('---COLOURMAP---', json.dumps(self.map))
        else:
            html = html.replace('---COLOURMAP---', '"black white"')
        html = html.replace('---SELID---', str(self.selected))
        html = html.replace('---ELID---', self.elid)
        html = html.replace('---ATTRIBS---', self.attribs())
        return html.replace('---ID---', self.id)

class ColourMapList(List):
    """A colourmap list selector, populated by the default colour maps
    """
    def __init__(self, target, selection=None, *args, **kwargs):
        if _isviewer(target):
            raise(Exception("ColourMapList control requires an Object target, not Viewer"))
        #Load maps list
        if selection is None:
            selection = target.parent.defaultcolourmaps()
        options = [''] + selection
        #Also add the matplotlib colourmaps if available
        try:
            #Load maps list
            import matplotlib
            import matplotlib.pyplot as plt
            sel = matplotlib.pyplot.colormaps()
            options += matplotlib.pyplot.colormaps()
        except:
            pass

        #Preceding command with '.' calls via python API, allowing use of matplotlib maps
        super(ColourMapList, self).__init__(target, options=options, command=".colourmap", label="Load Colourmap", *args, **kwargs)

class ColourMaps(_MultiControl):
    """A colourmap list selector, populated by the available colour maps,
    combined with a colourmap editor for the selected colour map
    """
    def __init__(self, target, *args, **kwargs):
        #Load maps list
        if _isviewer(target):
            raise(Exception("ColourMaps control requires an Object target, not Viewer"))
        self.maps = target.parent.state["colourmaps"]
        options = [["", "None"]]
        sel = target["colourmap"]
        if sel is None: sel = ""
        for m in range(len(self.maps)):
            options.append([self.maps[m]["name"], self.maps[m]["name"]])
            #Mark selected
            if sel == self.maps[m]["name"]:
                options[-1].append(True)
                sel = m

        super(ColourMaps, self).__init__(target, *args, **kwargs)
        self.list = self.add(List(target, options=options, command="", property="colourmap", label="", *args, **kwargs))

        self.gradient = Gradient(target)
        self.gradient.selected = sel #gradient editor needs to know selection index
        self.gradient.label = self.label
        self.add(self.gradient) #Add control list

    def controls(self):
        html = self.list.controls() + self.gradient.controls()
        html = html.replace('---PALLID---', str(self.gradient.elid))
        return html + '<br>'

class TimeStepper(Range):
    """A time step selection range control with up/down buttons
    """
    def __init__(self, viewer, *args, **kwargs):
        #Acts as a command setter with some additional controls
        super(TimeStepper, self).__init__(target=viewer, label="Timestep", command="timestep", readproperty="timestep", *args, **kwargs)

        self.timesteps = viewer.timesteps()
        self.step = 1
        if len(self.timesteps):
            self.range = (self.timesteps[0], self.timesteps[-1])
            #Calculate step gap
            self.step = self.timesteps[1] - self.timesteps[0]
        else:
            self.range = (0,0)
        self.value = 0

    def controls(self):
        html = Range.controls(self)
        #Note: unicode symbol escape must use double slash to be
        # passed through to javascript or python will process them
        html += """
        <script>
        var timer_---ELID--- = -1;
        function startTimer_---ELID---() {
          if (timer_---ELID--- >= 0) {
            if (timer_---ELID--- > 0)
              window.cancelAnimationFrame(timer_---ELID---);
            timer_---ELID--- = window.requestAnimationFrame(nextStep_---ELID---);
          }
        }
        function nextStep_---ELID---() {
          el = document.getElementById('---ELID---_number');
          if (el) {
            //Call again on image load - pass callback
            var V = _wi['---VIEWERID---'];
            if (!V.box.canvas.mouse.isdown && !V.box.zoomTimer && (!V.box.gui || V.box.gui.closed))
              V.execute("next", startTimer_---ELID---);
            else
              setTimeout(nextStep_---ELID---, 100);
          }
        }
        function playPause_---ELID---(btn) {
          if (timer_---ELID--- >= 0) {
            btn.value="\\u25BA";
            window.cancelAnimationFrame(timer_---ELID---);
            timer_---ELID--- = -1;
          } else {
            timer_---ELID--- = 0;
            startTimer_---ELID---();
            btn.value="\\u25FE";
          }
        }
        </script>
        """
        html += '<input type="button" style="width: 50px;" onclick="var el = document.getElementById(\'---ELID---_range\'); el.stepDown(); el.onchange()" value="&larr;" />'
        html += '<input type="button" style="width: 50px;" onclick="var el = document.getElementById(\'---ELID---_range\'); el.stepUp(); el.onchange()" value="&rarr;" />'
        html += '<input type="button" style="width: 60px;" onclick="playPause_---ELID---(this);" value="&#9658;" />'
        html = html.replace('---ELID---', self.elid)
        return html

class DualRange(_MultiControl):
    """A set of two range slider controls for adjusting a minimum and maximum range

    Parameters
    ----------
    range : list or tuple
        Min/max values for the range
    """
    def __init__(self, target, properties, values=[None,None], label=None, range=(0.,1.), step=None):
        super(DualRange, self).__init__(target)
        self.label = label

        self.ctrlmin = self.add(Range(target=target, property=properties[0], step=step, value=values[0], range=range, label=""))
        self.ctrlmax = self.add(Range(target=target, property=properties[1], step=step, value=values[1], range=range, label=""))

class Range2D(_MultiControl):
    """A set of two range slider controls for adjusting a 2D value

    Parameters
    ----------
    range : list or tuple
        Min/max values for the ranges
    """
    def __init__(self, target, property, label=None, value=None, range=(0.,1.), step=None, *args, **kwargs):
        super(Range2D, self).__init__(target, property=property, command=None, value=value, label=label, *args, **kwargs)

        if self._value is None:
            self._value = _getproperty(target, property)

        self.ctrlX = self.add(Range(target=target, property=property, step=step, value=self._value[0], range=range, label="", index=0))
        self.ctrlY = self.add(Range(target=target, property=property, step=step, value=self._value[1], range=range, label="", index=1))

class Range3D(_MultiControl):
    """A set of three range slider controls for adjusting a 3D value

    Parameters
    ----------
    range : list or tuple
        Min/max values for the ranges
    """
    def __init__(self, target, property, label=None, value=None, range=(0.,1.), step=None, *args, **kwargs):
        super(Range3D, self).__init__(target, property=property, command=None, value=value, label=label, *args, **kwargs)

        if self._value is None:
            self._value = _getproperty(target, property)

        self.ctrlX = self.add(Range(target=target, property=property, step=step, value=self._value[0], range=range, label="", index=0))
        self.ctrlY = self.add(Range(target=target, property=property, step=step, value=self._value[1], range=range, label="", index=1))
        self.ctrlZ = self.add(Range(target=target, property=property, step=step, value=self._value[2], range=range, label="", index=2))

class Filter(DualRange):
    """A set of two range slider controls for adjusting a minimum and maximum filter range

    Parameters
    ----------
    range : list or tuple
        Min/max values for the filter range
    """
    def __init__(self, target, filteridx, label=None, range=None, step=None):
        self.label = label
        if len(target["filters"]) <= filteridx:
            print("Filter index out of range: ", filteridx, len(target["filters"]))
            return None
        self.filter = target["filters"][filteridx]

        #Default label - data set name
        if label is None:
            label = self.filter['by'].capitalize()

        #Get the default range limits from the matching data source
        fname = self.filter['by']
        if not target["data"]:
            #Attempt to update
            target.reload()
            target.parent.render()

        if not target["data"]:
            raise(ValueError("target data list empty, can't use filter"))
        if not fname in target['data']:
            raise(ValueError(fname + " not found in target data list, can't use filter"))
        self.data = target["data"][fname]
        if not range:
            #self.range = (self.filter["minimum"], self.filter["maximum"])
            if self.filter["map"]:
                range = (0.,1.)
            else:
                range = (self.data["minimum"]*0.99, self.data["maximum"]*1.01)

        #Setup DualRange using filter min/max
        super(Filter, self).__init__(_getviewer(target), properties=[None, None], values=[self.filter["minimum"], self.filter["maximum"]], label=label, range=range)

        #Replace actions on the controls
        self.ctrlmin.action = _FilterAction(target, filteridx, "minimum")
        self.ctrlmax.action = _FilterAction(target, filteridx, "maximum")

class ObjectList(_MultiControl):
    """A set of checkbox controls for controlling visibility of all visualisation objects
    """
    def __init__(self, viewer, *args, **kwargs):
        super(ObjectList, self).__init__(viewer, label="Objects", *args, **kwargs)
        for obj in viewer.objects.list:
            self.add(ColourIndicator(obj, "colour"))
            self.add(Checkbox(obj, "visible", label=obj["name"])) 

#TODO: broken
class ObjectSelect(_Container):
    """A list selector of all visualisation objects that can be used to
    choose the target of a set of controls

    Parameters
    ----------
    objects : list
        A list of objects to display, by default all available objects are added
    """
    def __init__(self, viewer, objects=None, *args, **kwargs):
        if not _isviewer(viewer):
            print("Can't add ObjectSelect control to an Object, must add to Viewer")
            return
        self.parent = viewer
        if objects is None:
            objects = viewer.objects.list
        
        #Load maps list
        options = [(0, "None")]
        for o in range(len(objects)):
            obj = objects[o]
            options += [(o+1, obj["name"])]

        #The list control
        self._list = List(target=viewer, label="", options=options, command="select", *args, **kwargs)

        #Init container
        super(ObjectSelect, self).__init__(viewer) #, label="Objects", options=options, command="select", *args, **kwargs)

        #Holds a control factory so controls can be added with this as a target
        self.control = _ControlFactory(self)

    #def onchange(self):
    #    #Update the control values on change
    #    #return super(ObjectSelect, self).onchange()
    #    return self._list.onchange()

    def __contains__(self, key):
        #print "CONTAINS",key
        obj = self._list.action.lastvalue
        #print "OBJECT == ",obj,(key in self.parent.objects.list[obj-1])
        return obj > 0 and key in self.parent.objects.list[obj-1]

    def __getitem__(self, key):
        #print "GETITEM",key
        obj = self._list.action.lastvalue
        if obj > 0:
            #Passthrough: Get from selected object
            return self.parent.objects.list[obj-1][key]
        return None

    def __setitem__(self, key, value):
        obj = self._list.action.lastvalue
        #print "SETITEM",key,value
        if obj > 0:
            #Passtrough: Set on selected object
            self.parent.objects.list[obj-1][key] = value

    #Undefined method call - pass call to target
    def __getattr__(self, key):
        #__getattr__ called if no attrib/method found
        def any_method(*args, **kwargs):
            #If member function exists on target, call it
            obj = self._list.action.lastvalue
            if obj > 0:
                method = getattr(self.parent.objects.list[obj-1], key, None)
                if method and callable(method):
                    return method(*args, **kwargs)
        return any_method

    def html(self):
        html = '<div style="border: #888 1px solid; display: inline-block; padding: 6px;" class="lvctrl">\n'
        html += self._list.controls()
        html += '<hr>\n'
        html += super(ObjectSelect, self).html()
        html += '</div>\n'
        return html

    def controls(self):
        return self.html()

class _ControlFactory(object):
    """
    Create and manage sets of controls for interaction with a Viewer or Object
    Controls can run commands or change properties
    """
    #Creates a control factory used to generate controls for a specified target
    def __init__(self, target):
        self._target = weakref.ref(target)
        self.clear()
        self.interactor = False
        #self.output = ""

        #Save types of all control/containers
        def all_subclasses(cls):
            return cls.__subclasses__() + [g for s in cls.__subclasses__() for g in all_subclasses(s)]

        #_Control contructor shortcut methods
        #(allows constructing controls directly from the factory object)
        #Use a closure to define a new method to call constructor and add to controls
        def addmethod(constr):
            def method(*args, **kwargs):
                #Return the new control and add it to the list
                newctrl = constr(self._target(), *args, **kwargs)
                self.add(newctrl)
                return newctrl
            return method

        self._control_types = all_subclasses(_Control)
        self._container_types = all_subclasses(_Container)
        for constr in self._control_types + self._container_types:
            key = constr.__name__
            method = addmethod(constr)
            #Set docstring (+ _Control docs)
            if constr in self._control_types:
                method.__doc__ = constr.__doc__ + _Control.__doc__
            else:
                method.__doc__ = constr.__doc__
            self.__setattr__(key, method)

    def __call__(self, property, *args, **kwargs):
        """
        Calling with a property name creates the default control for that property
        """
        _lv = _getviewer(self._target())
        if property is not None and property in _lv.properties:
            #Get control info from prop dict
            prop = _lv.properties[property]
            T = prop["type"]
            ctrl = prop["control"]
            if len(ctrl) > 2 and len(ctrl[2]) > 1:
                #Has selections
                return self.List(property, *args, **kwargs)
            if "integer" in T or "real" in T:
                hasrange = len(ctrl) > 1 and len(ctrl[1]) == 3
                #Multi-dimensional?
                if "[2]" in T:
                    return self.Range2D(property, *args, **kwargs) if hasrange else self.Number2D(property, *args, **kwargs)
                elif "[3]" in T:
                    return self.Range3D(property, *args, **kwargs) if hasrange else self.Number3D(property, *args, **kwargs)
                else:
                    return self.Range(property, *args, **kwargs) if hasrange else self.Number(property, *args, **kwargs)
            elif T == "string":
                return self.Entry(property, *args, **kwargs)
            elif T == "boolean":
                return self.Checkbox(property, *args, **kwargs)
            elif T == "colour":
                return self.Colour(property, *args, **kwargs)
            else:
                print("Unable to determine control type for property: " + property)
                print(prop)

    def add(self, ctrl):
        """
        Add a control
        """
        #Just add to the parent lists if not a viewer
        if not _isviewer(self._target()):
            self._target().parent.control.add(ctrl)
        else:
            if type(ctrl) in self._container_types:
                #New container, further controls will be added to it
                self._containers.append(ctrl)
            else:
                #Normal control: add to active container
                self._containers[-1].add(ctrl)

        return ctrl

    def show(self, menu=True, filename=None, fallback=None):
        """
        Displays all added controls including viewer if any

        Parameters
        ----------
        filename : str
            Filename to write output HTML
            If in a notebook this is not necessary as content is displayed inline
            If not provided and run outside IPython, defaults to "control.html"
        fallback : function
            A function which is called in place of the viewer display method when run outside IPython
        """
        #Show all controls in container
        target = self._target()

        #Creates an interactor to connect javascript/html controls to IPython and viewer
        #if no viewer Window() created, it will be a windowless interactor
        if _isviewer(target):
            #Append the current viewer ref
            windows.append(target)
            #Get unique ID
            winids.append(gen_id())
        else:
            target.parent.control.show()
            return

        viewerid = winids[-1]

        #Generate the HTML and associated action JS
        html = "<form novalidate>"
        chtml = ""
        actions = {}
        for con in self._containers:
            html += con.html()
            actions.update(con.scripts())
        html += "</form>"
        #print('\n'.join([str(a)+":"+actions[a] for a in actions]))

        #Auto-clear now content generated
        #Prevents doubling up if cell executed again
        self.clear()

        #Set viewer id
        html = html.replace('---VIEWERID---', str(viewerid))
        #self.output += html

        code = _webglboxcode(menu)

        #Display HTML inline or export
        if is_notebook() and filename is None:
            if not target.server:
                raise(Exception("LavaVu HTTP Server must be active for interactive controls, set port= parameter to > 0"))
            """
            HTTP server mode interaction, rendering in separate render thread:
             - This should work in all notebook contexts, colab, jupyterlab etc
             - Only problem remaining is port access, from docker or cloud instances etc, need to forward port
             - jupyter-server-proxy packaged (available on pip) supports forwarding port via the jupyter server
            """
            from IPython.display import display,HTML
            #Interaction requires some additional js/css/webgl
            #Insert stylesheet, shaders and combined javascript libraries
            display(HTML(code))

            #Try and prevent this getting cached
            timestamp = datetime.datetime.now().strftime('%Y%m%d%H%M%S')
            html += "<!-- CREATION TIMESTAMP {0} -->".format(timestamp)

            #Pass port and object id from server
            html += "<script>init('{0}');</script>".format(viewerid)
            actionjs = self.export_actions(actions, id(target), target.port)
            #Output the controls and start interactor
            display(HTML(actionjs + html))

        else:
            #Export standalone html
            #return as string or write output to .html file in current directory
            template = basehtml
            template = template.replace('---SCRIPTS---', code + self.export_actions(actions)) #Process actions
            template = template.replace('---INIT---', 'init(\'{0}\');'.format(viewerid))
            template = template.replace('---CONTENT---', html)
            full_html = template

            if filename == '':
                #Just return code
                return full_html
            else:
                #Write the file
                if filename is None:
                    filename = "control.html"
                hfile = open(filename, "w")
                hfile.write(full_html)
                hfile.close()

            if callable(fallback):
                fallback(target)

    def export_actions(self, actions, uid=0, port=0, proxy=False):
        #Process actions
        topjs = '<script type="text/javascript">\n'
        if port > 0:
            topjs += 'function init(viewerid) {{_wi[viewerid] = new WindowInteractor(viewerid, {uid}, {port});\n'.format(uid=uid, port=port)
        else:
            topjs += 'function init(viewerid) {{_wi[viewerid] = new WindowInteractor(viewerid, {uid});\n'.format(uid=uid)

        topjs += '_wi[viewerid].actions = {\n'

        actionjs = ''
        for act in actions:
            #Add to action functions to list, each takes the value of the control when called
            if len(actionjs):
                actionjs += ',\n'
            actionjs += '  "' + str(act) + '" : "' + actions[act] + '"'; 

        #Add init and finish
        return topjs + actionjs + '};\n}\n</script>\n'

    def redisplay(self):
        """Update the active viewer image if any
        Applies changes made in python to the viewer and forces a redisplay
        """
        if not is_notebook():
            return
        #Find matching viewer id, redisplay all that match
        for idx,obj in enumerate(windows):
            if obj == self._target():
                viewerid = winids[idx]
                from IPython.display import display,HTML,Javascript
                #display(Javascript('_wi["{0}"].redisplay();'.format(viewerid)))
                display(HTML('<script>_wi["{0}"].redisplay();</script>'.format(viewerid)))

    def update(self):
        """Update the control values from current viewer data
        Applies changes made in python to the UI controls
        """
        #NOTE: to do this now, all we need is to trigger a get_state call from interactor by sending any command
        #TODO: currently only works for controllers with a window, other controls will not be updated
        if not is_notebook():
            return
        #Find matching viewer id, update all that match
        for idx,obj in enumerate(windows):
            #if obj == self._target():
            viewerid = winids[idx]
            from IPython.display import display,HTML
            display(HTML('<script>_wi["{0}"].execute(" ");</script>'.format(viewerid)))
        
    def clear(self):
        #Initialise with a default control wrapper
        self._containers = [_Container(self)]

