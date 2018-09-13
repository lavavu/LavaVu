"""
LavaVu python interface: interactive HTML UI controls library
"""
import os
import sys
import time
import json
output = ""

#Register of controls and their actions
actions = []
#Register of windows (viewer instances)
windows = []
#Register of all control objects
allcontrols = []

vertexShader = """
<script id="line-vs" type="x-shader/x-vertex">
precision highp float;
//Line vertex shader
attribute vec3 aVertexPosition;
uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform vec4 uColour;
varying vec4 vColour;
void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  gl_Position = uPMatrix * mvPosition;
  vColour = uColour;
}
</script>
"""

fragmentShader = """
<script id="line-fs" type="x-shader/x-fragment">
precision highp float;
varying vec4 vColour;
void main(void)
{
  gl_FragColor = vColour;
}
</script>
"""

basehtml = """
<html>

<head>
<title>LavaVu Interface</title>
<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">

---SCRIPTS---

</head>

<body onload="initPage();" oncontextmenu="return false;">
---HIDDEN---
</body>

</html>
"""

inlinehtml = """
---SCRIPTS---
<div id = "lavavu_GUI_---ID---" style="position: absolute; top: 0em; right: 0em;"></div>
<div id="---ID---" style="display: block; width: ---WIDTH---px; height: ---HEIGHT---px;">
</div>
---HIDDEN---

<script>
initPage("---ID---");
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

#Static HTML location
htmlpath = ""
initialised = False
initcell = ""

def isviewer(target):
    """Return true if target is a viewer"""
    return not hasattr(target, "instance")

def getviewer(target):
    """Return its viewer if target is vis object
    otherwise just return target as if is a viewer"""
    if not isviewer(target):
        return target.instance
    return target

def getproperty(target, propname):
    """Return value of property from target
    if not defined, return the default value for this property
    """
    _lv = getviewer(target)
    if propname in target:
        return target[propname]
    elif propname in _lv.properties:
        #Get property default
        prop = _lv.properties[propname]
        return prop["default"]
    else:
        return None

def getcontrolvalues(names=None):
    """Get the property control values from their targets
    """
    #Can pass a newline seperated list of props
    #If not provided, updates all properties
    if names is not None:
        names = names.split('\n')
    #Build a list of controls to update and their values
    updates = []
    for c in allcontrols:
        if hasattr(c, 'id') and c.elid:
            #print c.elid,c.id
            action = Action.actions[c.id]
            #print action
            if hasattr(action, "property") and (names is None or action.property in names):
                #print "  ",c.elid,action
                value = getproperty(action.target, action.property)
                #print "  = ",value
                if value is not None:
                    updates.append([c.elid, value, action.property])
    return updates


_file_cache = dict()

def _webglboxcode():
    """
    Returns WebGL base code for the bounding box interactor window
    """
    code = getcss(["control.css"])
    code += fragmentShader
    code += vertexShader
    code += getjslibs(['gl-matrix-min.js', 'OK-min.js', 'drawbox.js', 'control.js'])
    return code

def _webglviewcode(shaderpath, menu=True, lighttheme=False):
    """
    Returns WebGL base code for an interactive visualisation window
    """
    css = ["styles.css"]
    code = getshaders(shaderpath)
    code += getjslibs(['gl-matrix-min.js', 'OK-min.js', 'draw.js'])
    #code += getjslibs(['gl-matrix-min.js', 'OK.js', 'draw.js'])
    if menu:
        #HACK: Need to disable require.js to load dat.gui from inline script tags
        code += """
        <script>
        _backup_define = window.define;
        window.define = undefined;
        </script>
        """
        code += getjslibs(['dat.gui.min.js'])
        code += """
        <script>
        window.define = _backup_define;
        delete _backup_define;
        </script>
        """
        if lighttheme:
            css.append("dat-gui-light-theme.css")
    code += getcss(css)
    return code

def getcss(files=["styles.css"]):
    #Load stylesheets to inline tag
    return _filestohtml(files, tag="style")

def getjslibs(files):
    #Load a set of combined javascript libraries in script tags
    return _filestohtml(files, tag="script")

def _filestohtml(files, tag="script"):
    #Load a set of files into a string enclosed in specified html tag
    if not htmlpath: return ""
    code = '<' + tag + '>\n'
    for f in files:
        code += _readfilehtml(f)
    code += '\n</' + tag + '>\n'
    return code

def _readfilehtml(filename):
    #Read a file from the htmlpath (or cached copy)
    if not htmlpath: return ""
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

#def getshaders(path, shaders=['points', 'lines', 'triangles']):
def getshaders(path, shaders=['points', 'lines', 'triangles', 'volume']):
    #Load combined shaders
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

def initialise(initcode):
    global initialised, initcell
    if not htmlpath: return
    try:
        if __IPYTHON__:
            from IPython.display import display,HTML,Javascript
            """ Re-import check
            First check if re-executing the cell init code was inserted from, if so must re-init
            Then sneakily scans the IPython history for "import lavavu" in cell AFTER the one where
            the control interface was last initialised, if it is found, assume we need to initialise again!
            A false positive will add the init code again, which is harmless but bloats the notebook.
            A false negative will cause the interactive viewer widget to not display the webgl bounding box,
            as was the previous behaviour after a re-run without restart.
            """
            ip = get_ipython()
            #Returns a list of executed cells and their content
            history = list(ip.history_manager.get_range())
            #If init was done in cell with exact same code this check is false positive
            #(eg: lv.window())
            if initialised and history[-1][2] != initcell:
                count = 0
                found = False
                #Loop through all the cells in history list
                for cell in history:
                    #Skip cells, until we pass the previous init cell
                    count += 1
                    if count <= initialised: continue;
                    #Each cell is tuple, 3rd element contains line
                    if "import lavavu" in cell[2] or "import glucifer" in cell[2]:
                        #LavaVu has been re-imported, re-init
                        found = True
                        break

                if not found:
                    #Viewer was initialised in an earlier cell
                    return

            #Save cell # and content from history we insert initialisation code at
            initialised = len(history)
            initcell = history[-1][2]

            #Insert stylesheet, shaders and combined javascript libraries
            display(HTML(initcode))
    except (NameError, ImportError):
        pass

class Action(object):
    """Base class for an action triggered by a control

    Default action is to run a command

    also holds the global action list
    """
    actions = []

    #actions.append({"type" : "COMMAND", "args" : [command]})
    def __init__(self, target, command=None, readproperty=None):
        self.target = target
        self.command = command
        if not hasattr(self, "property"):
            self.property = readproperty
        Action.actions.append(self)

    def run(self, value):
        #Run a command with a value argument
        if self.command is None: return ""
        return self.command + " " + str(value) + "\nredraw"

    def script(self):
        #Return script action for HTML export
        if self.command is None: return ""
        #Run a command with a value argument
        return self.command + ' " + value + "'

    @staticmethod
    def do(id, value):
        #Execute actions from client in IPython
        if len(Action.actions) <= id:
            return "#NoAction " + str(id)
        return Action.actions[id].run(value)

    @staticmethod
    def export(html):
        #Dump all output to control.html in current directory & htmlpath
        if not htmlpath: return
        #Process actions
        actionjs = '<script type="text/Javascript">\nvar actions = [\n'
        for act in Action.actions:
            #Default placeholder action
            actscript = act.script()
            if len(actscript) == 0:
                #No action
                pass
            #Add to actions list
            #(Only a single interactor is supported in exported html, so always use id=0)
            actionjs += '  function(value) {_wi[0].execute("' + actscript + '");},\n'
        #Add init and finish
        actionjs += '  null];\nfunction init() {_wi[0] = new WindowInteractor(0);}\n</script>'

        def writefile(fn):
            hfile = open(fn, "w")
            hfile.write('<html>\n<head>\n<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">')
            hfile.write(_webglboxcode())
            hfile.write(actionjs)
            hfile.write('</head>\n<body onload="init();">\n')
            hfile.write(html)
            hfile.write("\n</body>\n</html>\n")
            hfile.close()

        #Write the file, locally and in htmlpath
        writefile("control.html")
        #This will fail if installed to non writable location
        #writefile(os.path.join(htmlpath, "control.html"))

class PropertyAction(Action):
    """Property change action triggered by a control
    """

    #actions.append({"type" : "PROPERTY", "args" : [target, property, command]})
    def __init__(self, target, prop, command=None):
        self.property = prop
        #Default action after property set is redraw, can be set to provided
        if command is None:
            command = "redraw"
        self.command = command
        super(PropertyAction, self).__init__(target, command)

    def run(self, value):
        #Set a property
        self.target[self.property] = value
        #Run any command action after setting
        return self.command

    def script(self):
        #Return script action for HTML export
        #Set a property
        # - Globally
        script = ''
        if isviewer(self.target):
            script = 'select; ' + self.property + '=" + value + "'
        #TODO: on an object selector
        # - On an object
        else:
            script = 'select ' + self.target["name"] + '; ' + self.property + '=" + value + "'
        #Add any additional commands
        return script + '; ' + super(PropertyAction, self).script()

class FilterAction(PropertyAction):
    """Filter property change action triggered by a control
    """
    def __init__(self, target, index, prop, command=None):
        self.index = index
        if command is None: command = "redraw"
        self.command = command
        super(FilterAction, self).__init__(target, prop)

    def run(self, value):
        #Set a filter range
        f = self.target["filters"]
        f[self.index][self.property] = value
        self.target["filters"] = f
        return self.command

    def script(self):
        #Return script action for HTML export
        #Set a filter range
        cmd = "filtermin" if self.property == "minimum" else "filtermax"
        return 'select ' + self.target["name"] + '; ' + cmd + ' ' + str(self.index) + ' " + value + "'

class ColourMapAction(Action):
    """Colourmap change action triggered by a control
    """
    def __init__(self, target):
        super(ColourMapAction, self).__init__(target)

    def run(self, value):
        #Set a colourmap
        if self.target.id > 0:
            self.target.colourmap(value)
        return self.command

    def script(self):
        #Return script action for HTML export
        #Set a colourmap
        if self.target.id > 0:
            return 'colourmap ' + str(self.target.id) + ' \\"" + value + "\\"' #Reload?
        return ""

class HTML(object):
    """A class to output HTML controls
    """
    lastid = 0

    #Parent class for container types
    def __init__(self):
        self.uniqueid()

    def html(self):
        """Return the HTML code"""
        return ''

    def uniqueid(self):
        #Get a unique control identifier
        HTML.lastid += 1
        self.elid = "lvctrl_" + str(HTML.lastid)
        return self.elid

class Container(HTML):
    """A container for a set of controls
    """
    #Parent class for container types
    def __init__(self, viewer):
        self.viewer = viewer
        self._content = []
        super(Container, self).__init__()

    def add(self, ctrl):
        self._content.append(ctrl)

    def controls(self):
        return self.html()

    def html(self):
        html = ''
        for i in range(len(self._content)):
            html += self._content[i].controls()
        return html

class Window(Container):
    """
    Creates an interaction window with an image of the viewer frame 
    and webgl controller for rotation/translation

    Parameters
    ----------
    align: str
        Set to "left/right" to align viewer window, default is left
    """
    def __init__(self, viewer, align="left"):
        super(Window, self).__init__(viewer)
        self.align = align

    def html(self, wrapper=True, wrapperstyle=""):
        style = 'min-height: 200px; min-width: 200px; background: #ccc; position: relative; display: inline-block; '
        #style += 'float: ' + self.align + ';'
        if wrapper:
            style += ' margin-right: 10px;'
        html = ""
        html += '<div style="' + style + '">\n'
        html += '<img id="imgtarget_---VIEWERID---" draggable=false style="margin: 0px; border: 1px solid #aaa; display: inline-block;">\n'
        html += """
           <div style="display: none; z-index: 200; position: absolute; top: 5px; right: 5px;">
             <select onchange="_wi[---VIEWERID---].box.mode = this.value;">
               <option>Rotate</option>
               <option>Translate</option>
               <option>Zoom</option>
             </select>
             <input type="button" value="Reset" onclick="_wi[---VIEWERID---].execute('reset');">
           </div>"""
        html += '</div>\n'
        #Display any contained controls
        if wrapper:
            html += '<div style="' + wrapperstyle + '" class="lvctrl">\n'
        html += super(Window, self).html()
        if wrapper:
            html += '</div>\n'
        html += '<div style="clear: both;">\n'
        return html

class Panel(Container):
    """Creates a control panel with an interactive viewer window and a set of controls
    By default the controls will be placed to the left with the viewer aligned to the right

    Parameters
    ----------
    showwin: boolean
        Set to False to exclude the interactive window
    """
    def __init__(self, viewer, showwin=True):
        super(Panel, self).__init__(viewer)
        self.win = None
        if showwin:
            self.win = Window(viewer, align="right")

    def html(self):
        if not htmlpath: return
        html = ""
        if self.win: html = self.win.html(wrapper=False)
        #Add control wrapper
        html += '<div style="padding:0px; margin: 0px; position: relative;" class="lvctrl">\n'
        html += super(Panel, self).html()
        html += '</div>\n'
        #if self.win: html += self.win.html(wrapperstyle="float: left; padding:0px; margin: 0px; position: relative;")
        return html

class Tabs(Container):
    """Creates a group of controls with tabs that can be shown or hidden

    Parameters
    ----------
    buttons: boolean
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
        label: str
            Label for the tab, if omitted will be blank
        """
        self.tabs.append(label)

    def add(self, ctrl):
        if not len(self.tabs): self.tab()
        self._content.append(ctrl)
        ctrl.tab = len(self.tabs)-1

    def html(self):
        if not htmlpath: return
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
                html += '<button class="' + classes + '" onclick="openTab_---ELID---(this, this.innerHTML)">---LABEL---</button>'
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

class Control(HTML):
    """
    Control object

    Parameters
    ----------
    target: Obj or Viewer
        Add a control to an object to control object properties
        Add a control to a viewer to control global proerties and run commands
    property: str
        Property to modify
    command: str
        Command to run
    value: any type
        Initial value of the controls
    label: str
        Descriptive label for the control
    readproperty: str
        Property to read control value from on update (but not modified)
    """

    def __init__(self, target, property=None, command=None, value=None, label=None, readproperty=None):
        super(Control, self).__init__()
        self.label = label

        #Get id and add to register
        self.id = len(Action.actions)

        #Can either set a property directly or run a command
        if property:
            action = PropertyAction(target, property, command)
            if label is None:
                self.label = property.capitalize()
        elif command:
            action = Action(target, command, readproperty)
            if label is None:
                self.label = command.capitalize()
        else:
            #Assume derived class will fill out the action, this is just a placeholder
            action = Action(target)

        if not self.label:
            self.label = ""

        #Get value from target or default if not provided
        if value is None and property is not None and target:
            value = getproperty(target, property)
        self.value = value

    def onchange(self):
        return "_wi[---VIEWERID---].do_action(" + str(self.id) + ", this.value, this);"

    def show(self):
        #Show only this control
        html = '<div style="" class="lvctrl">\n'
        html += self.html()
        html += '</div>\n'

    def html(self):
        return self.controls()

    def labelhtml(self):
        #Default label
        html = ""
        if len(self.label):
            html += '<p>' + self.label + ':</p>\n'
        return html

    def controls(self, type='number', attribs={}, onchange=""):
        #Input control
        html =  '<input id="---ELID---" class="---ELID---" type="' + type + '" '
        for key in attribs:
            html += key + '="' + str(attribs[key]) + '" '
        html += 'value="' + str(self.value) + '" '
        #Onchange event
        onchange += self.onchange()
        html += 'onchange="' + onchange + '" '
        html += '>\n'
        html = html.replace('---ELID---', self.elid)
        return html

class Divider(Control):
    """A divider element
    """

    def controls(self):
        return '<hr style="clear: both;">\n'

class Number(Control):
    """A basic numerical input control
    """

    def controls(self):
        html = self.labelhtml()
        html += super(Number, self).controls()
        return html + '<br>\n'

class Checkbox(Control):
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
        return "; _wi[---VIEWERID---].do_action(" + str(self.id) + ", this.checked ? 1 : 0, this);"

class Range(Control):
    """A slider control for a range of values

    Parameters
    ----------
    range: list/tuple
        Min/max values for the range
    """
    def __init__(self, target=None, property=None, command=None, value=None, label=None, range=(0.,1.), step=None, readproperty=None):
        super(Range, self).__init__(target, property, command, value, label, readproperty)

        self.range = range
        self.step = step
        if not step:
            #Assume a step size of 1 if range max > 1
            if self.range[1] > 1.0:
                self.step = 1
            else:
                self.step = 0.01

    def controls(self):
        attribs = {"min" : self.range[0], "max" : self.range[1], "step" : self.step}
        html = self.labelhtml()
        html += super(Range, self).controls('number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += super(Range, self).controls('range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        return html + '<br>\n'

class Button(Control):
    """A push button control to execute a defined command
    """
    def __init__(self, target, command, label=None):
        super(Button, self).__init__(target, "", command, "", label)

    def onchange(self):
        return "_wi[---VIEWERID---].do_action(" + str(self.id) + ", '', this);"

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

class Entry(Control):
    """A generic input control for string values
    """
    def controls(self):
        html = self.labelhtml()
        html += '<input class="---ELID---" type="text" value="" onkeypress="if (event.keyCode == 13) { _wi[---VIEWERID---].do_action(---ID---, this.value.trim(), this); };"><br>\n'
        html = html.replace('---ELID---', self.elid)
        return html.replace('---ID---', str(self.id))

class Command(Control):
    """A generic input control for executing command strings
    """
    def __init__(self, *args, **kwargs):
        super(Command, self).__init__(command=" ", label="Command", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <input class="---ELID---" type="text" value="" 
        onkeypress="if (event.keyCode == 13) { var cmd=this.value.trim(); 
        _wi[---VIEWERID---].do_action(---ID---, cmd ? cmd : 'repeat', this); this.value=''; };"><br>\n
        """
        html = html.replace('---ELID---', self.elid)
        return html.replace('---ID---', str(self.id))

class List(Control):
    """A list of predefined input values to set properties or run commands

    Parameters
    ----------
    options: list
        List of the available value strings
    """
    def __init__(self, target, options=[], *args, **kwargs):
        self.options = options
        super(List, self).__init__(target, *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += '<select class="---ELID---" id="---ELID---" value="" '
        html += 'onchange="' + self.onchange() + '">\n'
        for opt in self.options:
            #Can be dict {"label" : label, "value" : value, "selected" : True/False}
            #or list [value, label, selected]
            #or just: value
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

class Colour(Control):
    """A colour picker for setting colour properties
    """
    def __init__(self, *args, **kwargs):
        super(Colour, self).__init__(command="", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <div><div class="colourbg checkerboard">
          <div id="---ELID---" class="colour ---ELID---" onclick="
            var col = new Colour();
            var offset = findElementPos(this);
            var el = this;
            var savefn = function(val) {
              var col = new Colour(0);
              col.setHSV(val);
              el.style.backgroundColor = col.html();
              console.log(col.html());
              _wi[---VIEWERID---].do_action(---ID---, col.html(), el);
            }
            el.picker = new ColourPicker(savefn);
            el.picker.pick(col, offset[0], offset[1]);">
            </div>
        </div></div>
        <script>
        var el = document.getElementById("---ELID---");
        //Set the initial colour
        var col = new Colour('---VALUE---');
        el.style.backgroundColor = col.html();
        </script>
        """
        html = html.replace('---VALUE---', str(self.value))
        html = html.replace('---ELID---', self.elid)
        return html.replace('---ID---', str(self.id))

class Gradient(Control):
    """A colourmap editor
    """
    def __init__(self, target, *args, **kwargs):
        super(Gradient, self).__init__(target, property="colourmap", command="", *args, **kwargs)
        #Get and save the map id of target object
        self.maps = target.instance.state["colourmaps"]
        self.map = None
        for m in self.maps:
            if m["name"] == self.value:
                self.map = m
        self.selected = -1;
        #Replace action on the control
        Action.actions[self.id] = ColourMapAction(target)

    def controls(self):
        html = self.labelhtml()
        html += """
        <canvas id="---ELID---" width="512" height="24" class="palette checkerboard">
        </canvas>
        <script>
        var el = document.getElementById("---ELID---");
        el.colourmaps = ---COLOURMAPS---;
        el.selectedIndex = ---SELID---;
        if (!el.gradient) {
          //Create the gradient editor
          el.gradient = new GradientEditor(el, function(obj, id) {
              //Gradient updated
              _wi[---VIEWERID---].do_action(---ID---, obj.palette.toString(), el);

              //Update stored maps list
              if (el.selectedIndex >= 0)
                el.colourmaps[el.selectedIndex] = el.gradient.palette.get().colours;
            }
          , true); //Enable premultiply
          //Load the initial colourmap
          el.gradient.read(---COLOURMAP---);
        }
        </script>
        """
        mapstr = '['
        for m in range(len(self.maps)):
            mapstr += json.dumps(self.maps[m]["colours"])
            if m < len(self.maps)-1: mapstr += ','
        mapstr += ']'
        html = html.replace('---COLOURMAPS---', mapstr)
        if self.map:
            html = html.replace('---COLOURMAP---', json.dumps(self.map["colours"]))
        else:
            html = html.replace('---COLOURMAP---', '"black white"')
        html = html.replace('---SELID---', str(self.selected))
        html = html.replace('---ELID---', self.elid)
        return html.replace('---ID---', str(self.id))

class ColourMapList(List):
    """A colourmap list selector, populated by the default colour maps
    """
    def __init__(self, target, selection=None, *args, **kwargs):
        #Load maps list
        if selection is None:
            selection = target.instance.defaultcolourmaps()
        options = [''] + selection

        super(ColourMapList, self).__init__(target, options=options, command="reload", property="colourmap", *args, **kwargs)

        #Replace action on the control
        Action.actions[self.id] = ColourMapAction(target)

class ColourMaps(List):
    """A colourmap list selector, populated by the available colour maps,
    combined with a colourmap editor for the selected colour map
    """
    def __init__(self, target, *args, **kwargs):
        #Load maps list
        self.maps = target.instance.state["colourmaps"]
        options = [["", "None"]]
        sel = target["colourmap"]
        if sel is None: sel = ""
        for m in range(len(self.maps)):
            options.append([self.maps[m]["name"], self.maps[m]["name"]])
            #Mark selected
            if sel == self.maps[m]["name"]:
                options[-1].append(True)
                sel = m

        self.gradient = Gradient(target)
        self.gradient.selected = sel #gradient editor needs to know selection index
        self.gradient.label = "" #Clear label

        super(ColourMaps, self).__init__(target, options=options, command="", property="colourmap", *args, **kwargs)

    def onchange(self):
        #Find the saved palette entry and load it
        script = """
        var el = document.getElementById('---PALLID---'); 
        var sel = document.getElementById('---ELID---');
        if (sel.selectedIndex > 0) {
            el.gradient.read(el.colourmaps[sel.selectedIndex-1]);
            el.selectedIndex = sel.selectedIndex-1;
        }
        """
        return script + super(ColourMaps, self).onchange()

    def controls(self):
        html = super(ColourMaps, self).controls() + self.gradient.controls()
        html = html.replace('---PALLID---', str(self.gradient.elid))
        return html

class TimeStepper(Range):
    """A time step selection range control with up/down buttons
    """
    def __init__(self, viewer, *args, **kwargs):
        #Acts as a command setter with some additional controls
        super(TimeStepper, self).__init__(target=viewer, label="Timestep", command="timestep", readproperty="timestep", *args, **kwargs)

        self.timesteps = viewer.timesteps()
        self.range = (self.timesteps[0], self.timesteps[-1])
        self.step = 1
        #Calculate step gap
        if len(self.timesteps) > 1:
            self.step = self.timesteps[1] - self.timesteps[0]
        self.value = 0

    def controls(self):
        html = Range.controls(self)
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
          el = document.getElementById('---ELID---');
          if (el) {
            //Call again on image load - pass callback
            _wi[---VIEWERID---].execute("next", startTimer_---ELID---);
            getAndUpdateControlValues('timestep');
          }
        }
        function playPause_---ELID---(btn) {
          if (timer_---ELID--- >= 0) {
            btn.value="\u25BA";
            btn.style.fontSize = "12px"
            window.cancelAnimationFrame(timer_---ELID---);
            timer_---ELID--- = -1;
          } else {
            timer_---ELID--- = 0;
            startTimer_---ELID---();
            btn.value="\u25ae\u25ae";
            btn.style.fontSize = "10px"
          }
        }
        </script>
        """
        html += '<input type="button" style="width: 50px;" onclick="var el = document.getElementById(\'---ELID---\'); el.stepDown(); el.onchange()" value="&larr;" />'
        html += '<input type="button" style="width: 50px;" onclick="var el = document.getElementById(\'---ELID---\'); el.stepUp(); el.onchange()" value="&rarr;" />'
        html += '<input type="button" style="width: 60px;" onclick="playPause_---ELID---(this);" value="&#9658;" />'
        html = html.replace('---ELID---', self.elid)
        return html

class DualRange(Control):
    """A set of two range slider controls for adjusting a minimum and maximum range

    Parameters
    ----------
    range: list/tuple
        Min/max values for the range
    """
    def __init__(self, target, properties, values, label, range=(0.,1.), step=None):
        self.label = label

        self.ctrlmin = Range(target=target, property=properties[0], step=step, value=values[0], range=range, label="")
        self.ctrlmax = Range(target=target, property=properties[1], step=step, value=values[1], range=range, label="")

    def controls(self):
        return self.labelhtml() + self.ctrlmin.controls() + self.ctrlmax.controls()

class Filter(Control):
    """A set of two range slider controls for adjusting a minimum and maximum filter range

    Parameters
    ----------
    range: list/tuple
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
            self.label = self.filter['by'].capitalize()

        #Get the default range limits from the matching data source
        self.data = target["data"][self.filter['by']]
        if not range:
            #self.range = (self.filter["minimum"], self.filter["maximum"])
            if self.filter["map"]:
                range = (0.,1.)
            else:
                range = (self.data["minimum"], self.data["maximum"])

        self.ctrlmin = Range(step=step, range=range, value=self.filter["minimum"])
        self.ctrlmax = Range(step=step, range=range, value=self.filter["maximum"])

        #Replace actions on the controls
        Action.actions[self.ctrlmin.id] = FilterAction(target, filteridx, "minimum")
        Action.actions[self.ctrlmax.id] = FilterAction(target, filteridx, "maximum")

    def controls(self):
        return self.labelhtml() + self.ctrlmin.controls() + self.ctrlmax.controls()

class ObjectList(Control):
    """A set of checkbox controls for controlling visibility of all visualisation objects
    """
    def __init__(self, viewer, *args, **kwargs):
        super(ObjectList, self).__init__(target=viewer, label="Objects", *args, **kwargs)
        self.objctrls = []
        for obj in viewer.objects.list:
            self.objctrls.append(Checkbox(obj, "visible", label=obj["name"])) 

    def controls(self):
        html = self.labelhtml()
        for ctrl in self.objctrls:
            html += ctrl.controls()
        return html

class ObjectSelect(List):
    """A list selector of all visualisation objects that can be used to
    choose the target of a set of controls

    Parameters
    ----------
    objects: list
        A list of objects to display, by default all available objects are added
    """
    def __init__(self, viewer, objects=None, *args, **kwargs):
        if not isviewer(viewer):
            print("Can't add ObjectSelect control to an Object, must add to Viewer")
            return
        self.instance = viewer
        if objects is None:
            objects = viewer.objects.list
        
        #Load maps list
        self.object = 0 #Default to no selection
        options = [(0, "None")]
        for o in range(len(objects)):
            obj = objects[o]
            options += [(o+1, obj["name"])]

        super(ObjectSelect, self).__init__(target=self, label="Objects", options=options, property="object", *args, **kwargs)
        #Holds a control factory so controls can be added with this as a target
        self.control = ControlFactory(self)

    def onchange(self):
        #Update the control values on change
        return super(ObjectSelect, self).onchange() + "; getAndUpdateControlValues();"

    def __contains__(self, key):
        #print "CONTAINS",key
        #print "OBJECT == ",self.object,(key in self.instance.objects.list[self.object-1])
        return key == "object" or self.object > 0 and key in self.instance.objects.list[self.object-1]

    def __getitem__(self, key):
        #print "GETITEM",key
        if key == "object":
            return self.object
        elif self.object > 0:
            #Passtrough: Get from selected object
            return self.instance.objects.list[self.object-1][key]
        return None

    def __setitem__(self, key, value):
        #print "SETITEM",key,value
        if key == "object":
            self.object = value
            #Copy object id
            if self.object > 0:
                self.id = self.instance.objects.list[self.object-1].id
            else:
                self.id = 0
            #Update controls
            #self.instance.control.update()
        elif self.object > 0:
            #Passtrough: Set on selected object
            self.instance.objects.list[self.object-1][key] = value
        else:
            self.id = 0

    #Undefined methods supported directly as LavaVu commands
    def __getattr__(self, key):
        #__getattr__ called if no attrib/method found
        def any_method(*args, **kwargs):
            #If member function exists on target, call it
            if self.object > 0:
                method = getattr(self.instance.objects.list[self.object-1], key, None)
                if method and callable(method):
                    return method(*args, **kwargs)

        return any_method

class ObjectTabs(ObjectSelect):
    """Object selection with control tabs for each object"""
    def __init__(self, *args, **kwargs):
        super(ObjectTabs, self).__init__(target=self, label="Objects", options=options, property="object", *args, **kwargs)
    #Add predefined controls?
    #

class ControlFactory(object):
    """
    Create and manage sets of controls for interaction with a Viewer or Object
    Controls can run commands or change properties
    """
    #Creates a control factory used to generate controls for a specified target
    def __init__(self, target):
        self._target = target
        self.clear()
        self.interactor = False
        self.output = ""

        #Save types of all control/containers
        def all_subclasses(cls):
            return cls.__subclasses__() + [g for s in cls.__subclasses__() for g in all_subclasses(s)]

        #Control contructor shortcut methods
        #(allows constructing controls directly from the factory object)
        #Use a closure to define a new method to call constructor and add to controls
        def addmethod(constr):
            def method(*args, **kwargs):
                #Return the new control and add it to the list
                newctrl = constr(self._target, *args, **kwargs)
                self.add(newctrl)
                return newctrl
            return method

        self._control_types = all_subclasses(Control)
        self._container_types = all_subclasses(Container)
        for constr in self._control_types + self._container_types:
            key = constr.__name__
            method = addmethod(constr)
            #Set docstring (+ Control docs)
            if constr in self._control_types:
                method.__doc__ = constr.__doc__ + Control.__doc__
            else:
                method.__doc__ = constr.__doc__
            self.__setattr__(key, method)

    def add(self, ctrl):
        """
        Add a control
        """
        if type(ctrl) in self._container_types:
            #Save new container, further controls will be added to it
            self._containers.append(ctrl)
        elif len(self._containers):
            #Add to existing container
            self._containers[-1].add(ctrl)
        else:
            #Add to global list
            self._content.append(ctrl)

        #Add to viewer instance list too if not already being added
        if not isviewer(self._target):
            self._target.instance.control.add(ctrl)
        else:
            #Add to master list - not cleared after display
            allcontrols.append(ctrl)

    def getid(self):
        viewerid = len(windows)
        if isviewer(self._target):
            try:
                #Find viewer id
                viewerid = windows.index(self._target)
            except (ValueError):
                #Append the current viewer ref
                windows.append(self._target)
                #Use viewer instance just added
                viewerid = len(windows)-1
        return viewerid

    def show(self, fallback=None):
        """
        Displays all added controls including viewer if any

        fallback: function
            A function which is called in place of the viewer display when run outside IPython
        """
        #Show all controls in container
        if not htmlpath: return

        #Creates an interactor to connect javascript/html controls to IPython and viewer
        #if no viewer Window() created, it will be a windowless interactor
        viewerid = self.getid()

        #Generate the HTML
        html = ""
        chtml = ""
        for c in self._content:
            chtml += c.html()
        if len(chtml):
            html = '<div style="" class="lvctrl">\n' + chtml + '</div>\n'
        for c in self._containers:
            html += c.html()

        #Set viewer id
        html = html.replace('---VIEWERID---', str(viewerid))

        #Display HTML inline or export
        self.output += html
        try:
            if __IPYTHON__:
                from IPython.display import display,HTML,Javascript
                #Interaction requires some additional js/css/webgl
                initialise(_webglboxcode())

                #Output the controls
                display(HTML(html))

                #Create WindowInteractor instance
                js = '_wi[' + str(viewerid) + '] = new WindowInteractor(' + str(viewerid) + ');'
                display(Javascript(js))

            else:
                raise NameError()
        except (NameError, ImportError):
            Action.export(self.output)
            if callable(fallback): fallback(self._target)

        #Auto-clear after show?
        #Prevents doubling up if cell executed again
        self.clear()

    def redisplay(self):
        """Update the active viewer image if any
        Applies changes made in python to the viewer and forces a redisplay
        """
        try:
            #Find viewer id
            viewerid = windows.index(self._target)
            if __IPYTHON__:
                from IPython.display import display,Javascript
                js = '_wi[' + str(viewerid) + '].redisplay(' + str(viewerid) + ');'
                display(Javascript(js))

        except (NameError, ImportError, ValueError):
            pass

    def update(self):
        """Update the control values from current viewer data
        Applies changes made in python to the UI controls
        """
        try:
            #Build a list of controls to update and their values
            #pass it to javascript function to update the page
            updates = getcontrolvalues()
            if __IPYTHON__:
                from IPython.display import display,Javascript
                jso = json.dumps(updates)
                js = "updateControlValues(" + jso + ");"
                display(Javascript(js))
        except (NameError, ImportError, ValueError) as e:
            print(str(e))
            pass
        
    def clear(self):
        self._content = []
        self._containers = []

    def interactive(self, port=8080, resolution=(640,480)):
        """Display popup interactive viewer window
        """
        if not htmlpath: return
        try:
            if __IPYTHON__:
                from IPython.display import display,Javascript
                js = 'var win = window.open("http://localhost:---PORT---/interactive.html", "LavaVu", "toolbar=no,location=no,directories=no,status=no,menubar=no,scrollbars=no,resizable=no,width=' + str(resolution[0]) + ',height=' + str(resolution[1]) + '");'
                js = js.replace('---PORT---', str(port))
                display(Javascript(js))
        except (NameError, ImportError):
            pass

