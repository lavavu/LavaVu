import os
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

def export():
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
    <link rel="stylesheet" type="text/css" href="control.css">
    """ + fragmentShader + vertexShader + actionjs + """
    </head>
    <body onload="init();">""" + output + """
    </body>
    </html>""")
    hfile.close()
    filename = os.path.join(htmlpath, "control.html")

def redisplay(id):
    #Simply update the active viewer image, if any
    try:
        if __IPYTHON__:
            from IPython.display import display,Javascript
            display(Javascript('redisplay(' + str(id) + ');'))
    except NameError, ImportError:
        pass

def render(html):
    if not htmlpath: return
    try:
        if __IPYTHON__:
            from IPython.display import display,HTML
            display(HTML(html))
    except NameError, ImportError:
        global output
        output += html
        export()

def initialise():
    if not htmlpath: return
    try:
        if __IPYTHON__:
            from IPython.display import display,HTML,Javascript
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

def window(viewer, html="", align="left"):
    if not htmlpath: return
    viewerid = len(windows)

    html += '<div style="position: relative; float: ' + align + '; display: inline;" data-id="' + str(viewerid) + '">\n'
    html += '<img id="imgtarget_' + str(viewerid) + '" draggable=false style="border: 1px solid #aaa;">\n'
    html += '</div>\n'

    #Append the viewer ref
    windows.append(viewer)
    #print "Viewer appended " + str(id(viewer)) + " app= " + str(id(viewer.app)) + " # " + str(len(windows))
    try:
        if __IPYTHON__:
            #Create windowinteractor
            from IPython.display import display,HTML,Javascript
            display(HTML(html))
            display(Javascript('var wi = new WindowInteractor(' + str(viewerid) + ');'))
    except NameError, ImportError:
        render(html)

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

lastid = 0
def uniqueid():
    global lastid
    lastid += 1
    return str(lastid)

class Panel(object):
    def __init__(self, viewer, showwin=True):
        self.viewer = viewer
        self.controls = []
        self.showwin = showwin

    def add(self, ctrl):
        self.controls.append(ctrl)

    def html(self):
        html = ''
        for i in range(len(self.controls)):
            if self.controls[i].label and not isinstance(self.controls[i], Checkbox):
                html += '<p>' + self.controls[i].label + ':</p>\n'
            else:
                html += '<br>\n'
            html += self.controls[i].controls()
        return html

    def show(self):
        if not htmlpath: return
        viewerid = len(windows)
        #Add control wrapper with the viewer id as a custom attribute
        html = '<div data-id="' + str(viewerid) + '" style="float: left; padding:0px; margin: 0px; position: relative;" class="lvctrl">\n'
        html += self.html() + '</div>\n'
        if self.showwin:
            window(self.viewer, html, "right")
        else:
            render(html)

class Control(object):
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
            value = target[property]
        self.value = value

    def onchange(self):
        return "do_action(" + str(self.id) + ", this.value, this);"

    def show(self):
        if not htmlpath: return
        #Show only this control
        viewerid = len(windows)-1 #Just use the most recently added interactor instance
        if viewerid < 0: viewerid = 0 #Not yet added, assume it will be
        html = '<div data-id="' + str(viewerid) + '" style="" class="lvctrl">\n'
        #html = '<div data-id="' + str(viewerid) + '" style="float: left;" class="lvctrl">\n'
        if len(self.label): html += '<p>' + self.label + ':</p>\n'
        html += self.controls()
        html += '</div>\n'
        render(html)

    def controls(self, type='number', attribs={}, onchange=""):
        html =  '<input type="' + type + '" '
        for key in attribs:
            html += key + '="' + str(attribs[key]) + '" '
        html += 'value="' + str(self.value) + '" '
        onchange += self.onchange()
        html += 'onchange="' + onchange + '" '
        html += '>\n'
        return html

class Checkbox(Control):
    def __init__(self, *args, **kwargs):
        super(Checkbox, self).__init__(*args, **kwargs)

    def controls(self):
        attribs = {}
        if self.value: attribs = {"checked" : "checked"}
        html = "<label>\n"
        html += super(Checkbox, self).controls('checkbox', attribs)
        html += " " + self.label + "</label>\n"
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
        html = super(Range, self).controls('number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += super(Range, self).controls('range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        return html

class Command(Control):
    def __init__(self, *args, **kwargs):
        super(Command, self).__init__(command=" ", label="Command", *args, **kwargs)

    def controls(self):
        html = """
        <input type="text" value="" 
        onkeypress="if (event.keyCode == 13) { var cmd=this.value.trim(); 
        do_action(---ID---, cmd ? cmd : 'repeat', this); this.value=''; };">\n
        """
        return html.replace('---ID---', str(self.id))

class Colour(Control):
    def __init__(self, *args, **kwargs):
        super(Colour, self).__init__(command="", *args, **kwargs)

    def controls(self):
        html = """
        <div><div class="colourbg checkerboard">
          <div id="colour_---ELID---" class="colour" onclick="
            var col = new Colour();
            var offset = findElementPos(this);
            var el = this;
            var savefn = function(val) {
              var col = new Colour(0);
              col.setHSV(val);
              el.style.backgroundColor = col.html();
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
        html = html.replace('---ELID---', uniqueid())
        return html.replace('---ID---', str(self.id))

class ColourMap(Control):
    def __init__(self, target, *args, **kwargs):
        super(ColourMap, self).__init__(target, property="colourmap", command="", *args, **kwargs)
        #Get and save the map id of target object
        maps = target.instance.state["colourmaps"]
        if self.value >= len(maps)-1:
            self.map = maps[self.value]
        #Replace action on the control
        actions[self.id] = {"type" : "COLOURMAP", "args" : [target]}

    def controls(self):
        html = """
        <canvas id="palette_---ELID---" width="512" height="24" class="palette checkerboard">
        </canvas>
        <script>
        var el = document.getElementById("palette_---ELID---");
        if (!el.gradient) {
          //Create the gradient editor
          el.gradient = new GradientEditor(el, function(obj, id) {
              //Gradient updated
              console.log("PALETTE: " + obj.palette);
              //do_action(---ID---, obj.palette, el);
              do_action(---ID---, obj.palette.toString(), el);
            }
          , true); //Enable premultiply
          //Load the initial colourmap
          el.gradient.read(---COLOURMAP---);
        }
        </script>
        """
        html = html.replace('---COLOURMAP---', json.dumps(self.map["colours"]))
        html = html.replace('---ELID---', uniqueid())
        return html.replace('---ID---', str(self.id))

class TimeStepper(Range):
    def __init__(self, viewer, *args, **kwargs):
        #Acts as a command setter with some additional controls
        super(TimeStepper, self).__init__(target=viewer, label="Timestep", command="timestep", *args, **kwargs)

        self.timesteps = viewer.timesteps()
        self.range = (self.timesteps[0], self.timesteps[-1])
        self.step = 1
        self.value = 0

    def controls(self):
        attribs = {"min" : self.range[0], "max" : self.range[1], "step" : self.step}
        html = Control.controls(self, 'number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += Control.controls(self, 'range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        html += """<br>
        <input type="button" onclick="var el = this.previousElementSibling.previousElementSibling; el.stepDown(); el.onchange()" value="&larr;" />
        <input type="button" onclick="var el = this.previousElementSibling.previousElementSibling.previousElementSibling; el.stepUp(); el.onchange()" value="&rarr;" />
        """
        return html

class Filter(object):
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
        return self.ctrlmin.controls() + "<br>\n" + self.ctrlmax.controls()

class ObjectList(Control):
    def __init__(self, viewer, *args, **kwargs):
        super(ObjectList, self).__init__(target=viewer, label="Objects", *args, **kwargs)
        self.objctrls = []
        for obj in viewer.objects.list:
            self.objctrls.append(Checkbox(obj, "visible", label=obj["name"])) 

    def controls(self):
        html = ""
        for ctrl in self.objctrls:
            html += ctrl.controls() + "<br>\n"
        return html

