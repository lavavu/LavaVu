import os
import sys
import time
import json
output = ""

#Register of controls and their actions
actions = []
#Register of windows (viewer instances)
windows = []

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

#Static HTML location
htmlpath = ""
initialised = False
initcell = ""

def export(html):
    if not htmlpath: return
    #Dump all output to control.html
    filename = os.path.join(htmlpath, "control.html")

    #Process actions
    actionjs = '<script type="text/Javascript">\nvar actions = [];\n'
    for act in actions:
        #Default placeholder action
        actfunction = ''
        actcmd = None
        if len(act["args"]) == 0:
            #No action
            pass
        elif act["type"] == "PROPERTY":
            #Set a property
            target = act["args"][0]
            # - Globally
            if hasattr(target, "binpath"):
                prop = act["args"][1]
                actfunction = 'select; ' + prop + '=" + value + "'
                if len(act["args"]) > 2:
                    actcmd = act["args"][2]
            # - On an object
            else:
                name = act["args"][0]["name"]
                prop = act["args"][1]
                actfunction = 'select ' + name + '; ' + prop + '=" + value + "'
                if len(act["args"]) > 2:
                    actcmd = act["args"][2]

        #Run a command with a value argument
        elif act["type"] == "COMMAND":
            cmd = act["args"][0]
            if len(act["args"]) > 1:
                actcmd = act["args"][1]
            actfunction = cmd + ' " + value + "'
        #Set a filter range
        elif act["type"] == "FILTER":
            name = act["args"][0]["name"]
            index = act["args"][1]
            prop = act["args"][2]
            cmd = "filtermin" if prop == "minimum" else "filtermax"
            actfunction = 'select ' + name + '; ' + cmd + ' ' + str(index) + ' " + value + "; redraw'
        #Set a colourmap
        elif act["type"] == "COLOURMAP":
            id = act["args"][0].id
            actfunction = 'colourmap ' + str(id) + ' \\"" + value + "\\"' #Reload?

        #Append additional command (eg: reload)
        if actcmd:
          actfunction += ";" + actcmd
        actfunction = 'wi.execute("' + actfunction + '", true);'
        #actfunction = 'wi.execute("' + actfunction + '");'
        #Add to actions list
        actionjs += 'actions.push(function(value) {' + actfunction + '});\n'

    #Add init and finish
    actionjs += 'function init() {wi = new WindowInteractor(0);}\n</script>'
    hfile = open(filename, "w")
    hfile.write("""
    <html>
    <head>
    <meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
    <script type="text/Javascript" src="control.js"></script>
    <script type="text/Javascript" src="drawbox.js"></script>
    <script type="text/Javascript" src="OK-min.js"></script>
    <script type="text/Javascript" src="gl-matrix-min.js"></script>
    <link rel="stylesheet" type="text/css" href="control.css">""")
    hfile.write(fragmentShader)
    hfile.write(vertexShader)
    hfile.write(actionjs)
    hfile.write('</head>\n<body onload="init();">\n')
    hfile.write(html)
    hfile.write("\n</body>\n</html>")
    hfile.close()
    filename = os.path.join(htmlpath, "control.html")

def initialise():
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

            #Load stylesheet
            css = '<style>\n'
            fo = open(os.path.join(htmlpath, "control.css"), 'r')
            css += fo.read()
            fo.close()
            css += '</style>\n'

            #Load combined javascript libraries
            jslibs = '<script>\n'
            for f in ['gl-matrix-min.js', 'OK-min.js', 'drawbox.js', 'control.js']:
                fpath = os.path.join(htmlpath, f)
                fo = open(fpath, 'r')
                jslibs += fo.read()
                fo.close()
            jslibs += '</script>\n'

            display(HTML(css + fragmentShader + vertexShader + jslibs))
    except NameError, ImportError:
        pass

def action(id, value):
    #return str(id) + " : " + str(value)
    #Execute actions from IPython
    global actions
    if len(actions) <= id:
        return "#NoAction " + str(id)

    args = actions[id]["args"]
    act = actions[id]["type"]

    if act == "PROPERTY":
        #Set a property
        if len(args) < 3: return "#args<3"
        target = args[0]
        property = args[1]
        command = args[2]

        target[property] = value
        return command

    elif act == "COMMAND":
        if len(args) < 1: return "#args<1"
        #Run a command with a value argument
        command = args[0]
        return command + " " + str(value) + "\nredraw"

    elif act == "FILTER":
        if len(args) < 3: return "#args<3"
        #Set a filter range
        target = args[0]
        index = args[1]
        property = args[2]

        f = target["filters"]
        f[index][property] = value
        target["filters"] = f
        return "redraw"

    elif act == "COLOURMAP":
        if len(args) < 1: return "#args<1"
        #Set a colourmap
        target = args[0]
        #index = args[1]

        return 'colourmap ' + str(target.id) + ' "' + value + '"'
        """
        map = json.loads(value)
        f = target["colourmaps"]
        maps = target.instance["colourmaps"]
        maps[index] = value
        #print value
        target.instance["colourmaps"] = maps
        return "reload"
        """

    return ""

class Container(object):
    #Parent class for container types
    def __init__(self, viewer):
        self.viewer = viewer
        self.controls = []

    def add(self, ctrl):
        self.controls.append(ctrl)

    def html(self):
        html = ''
        for i in range(len(self.controls)):
            html += self.controls[i].controls()
        return html

class Window(Container):
    #Creates an interactor with a view window and webgl box control for rotation/translation
    def __init__(self, viewer, align="left"):
        super(Window, self).__init__(viewer)
        self.align = align

    def html(self, wrapper=True, wrapperstyle=""):
        style = 'min-height: 200px; min-width: 200px; background: #ccc; position: relative;'
        style += 'float: ' + self.align + '; display: inline;'
        if wrapper:
            style += ' margin-right: 10px;'
        html = '<div style="' + style + '" data-id="---VIEWERID---">\n'
        html += '<img id="imgtarget_---VIEWERID---" draggable=false style="border: 1px solid #aaa;">\n'
        html += '</div>\n'
        #Display any contained controls
        if wrapper:
            html += '<div data-id="---VIEWERID---" style="' + wrapperstyle + '" class="lvctrl">\n'
        html += super(Window, self).html()
        if wrapper:
            html += '</div>\n'
        return html

class Panel(Container):
    def __init__(self, viewer, showwin=True):
        super(Panel, self).__init__(viewer)
        self.win = None
        if showwin:
            self.win = Window(viewer, align="right")

    def html(self):
        if not htmlpath: return
        #Add control wrapper with the viewer id as a custom attribute
        html = '<div data-id="---VIEWERID---'
        html += '" style="float: left; padding:0px; margin: 0px; position: relative;" class="lvctrl">\n'
        html += super(Panel, self).html()
        html += '</div>\n'
        if self.win: html += self.win.html(wrapper=False)
        #if self.win: html += self.win.html(wrapperstyle="float: left; padding:0px; margin: 0px; position: relative;")
        return html

class Control(object):
    lastid = 0

    def __init__(self, target, property=None, command=None, value=None, label=None):
        self.label = label

        #Get id and add to register
        self.id = len(actions)

        #Can either set a property directly or run a command
        if property:
            #Default action after property set is redraw, can be set to provided
            if command == None:
                command = "redraw"
            actions.append({"type" : "PROPERTY", "args" : [target, property, command]})
            if label == None:
                self.label = property.capitalize()
        elif command:
            actions.append({"type" : "COMMAND", "args" : [command]})
            if label == None:
                self.label = command.capitalize()
        else:
            #Assume derived class will fill out the action
            actions.append({"type" : "PROPERTY", "args" : []})

        if not self.label:
            self.label = ""

        #Get value from target if not provided
        if value == None and property != None:
            if target and property in target:
                #TODO: query function that gets the value used even if prop not set
                value = target[property]
        self.value = value

    def uniqueid(self):
        #Get a unique control identifier
        Control.lastid += 1
        self.elid = Control.lastid
        return str(self.elid)

    def onchange(self):
        return "do_action(" + str(self.id) + ", this.value, this);"

    def show(self):
        #Show only this control
        html = '<div data-id="---VIEWERID---" style="" class="lvctrl">\n'
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
        html =  '<input type="' + type + '" '
        for key in attribs:
            html += key + '="' + str(attribs[key]) + '" '
        html += 'value="' + str(self.value) + '" '
        #Onchange event
        onchange += self.onchange()
        html += 'onchange="' + onchange + '" '
        html += '>\n'
        return html

class Number(Control):
    def controls(self):
        html = self.labelhtml()
        html += super(Number, self).controls()
        return html + '<br>\n'

class Checkbox(Control):
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
        return "; do_action(" + str(self.id) + ", this.checked ? 1 : 0, this);"

class Range(Control):
    def __init__(self, target=None, property=None, command=None, value=None, label=None, range=(0.,1.), step=None):
        super(Range, self).__init__(target, property, command, value, label)

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
    def __init__(self, command, label=""):
        super(Button, self).__init__(None, None, command, "", label)

    def onchange(self):
        return "do_action(" + str(self.id) + ", '', this);"

    def labelhtml(self):
        return ''

    def controls(self):
        html = self.labelhtml()
        html =  '<input type="button" value="' + str(self.label) + '" '
        #Onclick event
        html += 'onclick="' + self.onchange() + '" '
        html += '><br>\n'
        return html

class Entry(Control):
    def controls(self):
        html = self.labelhtml()
        html += '<input type="text" value="" onkeypress="if (event.keyCode == 13) { do_action(---ID---, this.value.trim(), this); };"><br>\n'
        return html.replace('---ID---', str(self.id))

class Command(Control):
    def __init__(self, *args, **kwargs):
        super(Command, self).__init__(command=" ", label="Command", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <input type="text" value="" 
        onkeypress="if (event.keyCode == 13) { var cmd=this.value.trim(); 
        do_action(---ID---, cmd ? cmd : 'repeat', this); this.value=''; };"><br>\n
        """
        return html.replace('---ID---', str(self.id))

class List(Control):
    def __init__(self, target, options=[], *args, **kwargs):
        self.options = options
        super(List, self).__init__(target, *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += '<select id="select_---ELID---" value="" '
        html += 'onchange="' + self.onchange() + '">\n'
        for opt in self.options:
            if isinstance(opt, dict):
                selected = "selected" if opt.selected else ""
                html += '<option value="' + str(opt["value"]) + '" ' + selected + '>' + opt["label"] + '</option>\n'
            elif isinstance(opt, list):
                selected = "selected" if len(opt) > 2 and opt[2] else ""
                html += '<option value="' + str(opt[0]) + '" ' + selected + '>' + str(opt[1]) + '</option>\n'
            else:
                html += '<option>' + str(opt) + '</option>\n'
        html += '</select><br>\n'
        html = html.replace('---ELID---', self.uniqueid())
        return html.replace('---ID---', str(self.id))

class Colour(Control):
    def __init__(self, *args, **kwargs):
        super(Colour, self).__init__(command="", *args, **kwargs)

    def controls(self):
        html = self.labelhtml()
        html += """
        <div><div class="colourbg checkerboard">
          <div id="colour_---ELID---" class="colour" onclick="
            var col = new Colour();
            var offset = findElementPos(this);
            var el = this;
            var savefn = function(val) {
              var col = new Colour(0);
              col.setHSV(val);
              el.style.backgroundColor = col.html();
              console.log(col.html());
              do_action(---ID---, col.html(), el);
            }
            el.picker = new ColourPicker(savefn);
            el.picker.pick(col, offset[0], offset[1]);">
            </div>
        </div></div>
        <script>
        var el = document.getElementById("colour_---ELID---");
        //Set the initial colour
        var col = new Colour('---VALUE---');
        el.style.backgroundColor = col.html();
        </script>
        """
        html = html.replace('---VALUE---', str(self.value))
        html = html.replace('---ELID---', self.uniqueid())
        return html.replace('---ID---', str(self.id))

class ColourMap(List):
    def __init__(self, target, *args, **kwargs):
        super(ColourMap, self).__init__(target, property="colourmap", command="", *args, **kwargs)
        #Get and save the map id of target object
        self.maps = target.instance.state["colourmaps"]
        if self.value != None and self.value < len(self.maps):
            self.map = self.maps[self.value]
        #Replace action on the control
        actions[self.id] = {"type" : "COLOURMAP", "args" : [target]}
        self.selected = -1;

    def controls(self):
        html = self.labelhtml()
        html += """
        <canvas id="palette_---ELID---" width="512" height="24" class="palette checkerboard">
        </canvas>
        <script>
        var el = document.getElementById("palette_---ELID---");
        el.colourmaps = ---COLOURMAPS---;
        el.selectedIndex = ---SELID---;
        if (!el.gradient) {
          //Create the gradient editor
          el.gradient = new GradientEditor(el, function(obj, id) {
              //Gradient updated
              do_action(---ID---, obj.palette.toString(), el);

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
        html = html.replace('---COLOURMAP---', json.dumps(self.map["colours"]))
        html = html.replace('---SELID---', str(self.selected))
        html = html.replace('---ELID---', self.uniqueid())
        return html.replace('---ID---', str(self.id))

class ColourMaps(List):
    def __init__(self, target, *args, **kwargs):
        #Load maps list
        self.maps = target.instance.state["colourmaps"]
        options = [[-1, "None"]]
        for m in range(len(self.maps)):
            options.append([m, self.maps[m]["name"]])
        #Mark selected
        sel = target["colourmap"]
        if sel is None: sel = -1
        options[sel+1].append(True)

        self.gradient = ColourMap(target)
        self.gradient.selected = sel #gradient editor needs to know selection index
        self.gradient.label = "" #Clear label

        super(ColourMaps, self).__init__(target, options=options, command="reload", property="colourmap", *args, **kwargs)

    def onchange(self):
        #Find the saved palette entry and load it
        script = """
        var el = document.getElementById('palette_---PALLID---'); 
        var sel = document.getElementById('select_---ELID---');
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
    def __init__(self, viewer, *args, **kwargs):
        #Acts as a command setter with some additional controls
        super(TimeStepper, self).__init__(target=viewer, label="Timestep", command="timestep", *args, **kwargs)

        self.timesteps = viewer.timesteps()
        self.range = (self.timesteps[0], self.timesteps[-1])
        self.step = 1
        #Calculate step gap
        if len(self.timesteps) > 1:
            self.step = self.timesteps[1] - self.timesteps[0]
        self.value = 0

    def controls(self):
        html = Range.controls(self)
        html += '<input type="button" style="width: 50px;" onclick="var el = this.previousElementSibling.previousElementSibling; el.stepDown(); el.onchange()" value="&larr;" />'
        html += '<input type="button" style="width: 50px;" onclick="var el = this.previousElementSibling.previousElementSibling.previousElementSibling; el.stepUp(); el.onchange()" value="&rarr;" />'
        return html

class DualRange(Control):
    def __init__(self, target, properties, values, label, step=None):
        self.label = label

        self.ctrlmin = Range(target=target, property=properties[0], step=step, value=values[0], label="")
        self.ctrlmax = Range(target=target, property=properties[1], step=step, value=values[1], label="")

    def controls(self):
        return self.labelhtml() + self.ctrlmin.controls() + self.ctrlmax.controls()

class Filter(Control):
    def __init__(self, target, filteridx, label=None, range=None, step=None):
        self.label = label
        self.filter = target["filters"][filteridx]

        #Default label - data set name
        if label == None:
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
        actions[self.ctrlmin.id] = {"type" : "FILTER", "args" : [target, filteridx, "minimum"]}
        actions[self.ctrlmax.id] = {"type" : "FILTER", "args" : [target, filteridx, "maximum"]}

    def controls(self):
        return self.labelhtml() + self.ctrlmin.controls() + self.ctrlmax.controls()

class ObjectList(Control):
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

class ControlFactory(object):
    #Creates a control factory used to generate controls for a specified target
    def __init__(self, target):
        self._target = target
        self.clear()
        self.interactor = False
        self.output = ""

        #Save types of all control/containers
        def all_subclasses(cls):
            return cls.__subclasses__() + [g for s in cls.__subclasses__() for g in all_subclasses(s)]
        self._control_types = all_subclasses(Control)
        self._container_types = all_subclasses(Container)
        self._all_types = self._control_types + self._container_types

    #Undefined methods call Control constructors if defined, passing saved target
    def __getattr__(self, key):
        #__getattr__ called if no attrib/method found
        def any_method(*args, **kwargs):
            #Find if class exists that extends Control or Container
            current_module = sys.modules[__name__]
            method = getattr(current_module, key)
            if isinstance(method, type) and method in self._all_types:
                #Return the new control and add it to the list
                newctrl = method(self._target, *args, **kwargs)
                self.add(newctrl)
                return newctrl

        return any_method

    def add(self, ctrl):
        if type(ctrl) in self._container_types:
            #Save new container, further controls will be added to it
            self._container = ctrl
        elif self._container:
            #Add to existing container
            self._container.add(ctrl)
        else:
            #Add to global list
            self._controls.append(ctrl)

        #Add to viewer instance list too if target is Obj
        if not hasattr(self._target, 'objects'):
            self._target.instance.control.add(ctrl)

    def show(self, win=None):
        #Show all controls in container
        if not htmlpath: return

        #Generate the HTML
        html = ""
        chtml = ""
        for c in self._controls:
            chtml += c.html()
        if len(chtml):
            html = '<div data-id="---VIEWERID---" style="" class="lvctrl">\n' + chtml + '</div>\n'
        if self._container:
            html += self._container.html()

        #Creates an interactor to connect javascript/html controls to IPython and viewer
        #if no viewer Window() created, it will be a windowless interactor
        viewerid = len(windows)
        if hasattr(self._target, 'objects'):
            try:
                #Find viewer id
                viewerid = windows.index(self._target)
            except ValueError:
                #Append the current viewer ref
                windows.append(self._target)
                #Use viewer instance just added
                viewerid = len(windows)-1

        #Set viewer id
        html = html.replace('---VIEWERID---', str(viewerid))

        #Display HTML inline or export
        self.output += html
        try:
            if __IPYTHON__:
                from IPython.display import display,HTML,Javascript
                #Interaction requires some additional js/css/webgl
                initialise()
                #Output the controls
                display(HTML(html))
                #Create WindowInteractor instance (if none created, or output contains a viewer target)
                if not self.interactor or 'imgtarget' in html:
                    display(Javascript('var wi = new WindowInteractor(' + str(viewerid) + ');'))
                    self.interactor = True
            else:
                export(self.output)
        except NameError, ImportError:
            export(self.output)

        #Auto-Clear after show?
        self.clear()

    def redisplay(self):
        #Update the active viewer image, if any
        try:
            #Find viewer id
            viewerid = windows.index(self._target)
            if __IPYTHON__:
                from IPython.display import display,Javascript
                display(Javascript('redisplay(' + str(viewerid) + ');'))

        except (NameError, ImportError, ValueError):
            pass

    def clear(self):
        self._controls = []
        self._container = None

