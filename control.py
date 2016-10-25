import os
import time
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

#LavaVu location
binpath = ""

def export():
    #Dump all output to control.html
    import os
    filename = os.path.join(binpath, "html/control.html")

    #Process actions
    actionjs = 'var actions = [];\n'
    for act in actions:
        #Default placeholder action
        actfunction = ''
        actcmd = None
        if len(act["args"]) == 0:
            #No action
            pass
        elif act["call"] == setter:
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
        elif act["call"] == commandsetter:
            cmd = act["args"][0]
            if len(act["args"]) > 1:
                actcmd = act["args"][1]
            actfunction = cmd + ' " + value + "'
        #Set a filter range
        elif act["call"] == filtersetter:
            name = act["args"][0]["name"]
            index = act["args"][1]
            prop = act["args"][2]
            cmd = "filtermin" if prop == "minimum" else "filtermax"
            actfunction = 'select ' + name + '; ' + cmd + ' ' + str(index) + ' " + value + "; redraw'

        #Append additional command (eg: reload)
        if actcmd:
          actfunction += ";" + actcmd
        actfunction = 'wi.execute("' + actfunction + '", true);'
        #Add to actions list
        actionjs += 'actions.push(function(value) {' + actfunction + '});\n'

    hfile = open(filename, "w")
    hfile.write('<html>\n<head>\n')
    hfile.write('<meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">\n');
    hfile.write('<script type="text/Javascript" src="control.js"></script>\n')
    hfile.write('<script type="text/Javascript" src="drawbox.js"></script>\n')
    hfile.write('<script type="text/Javascript" src="OK-min.js"></script>\n')
    hfile.write('<script type="text/Javascript" src="gl-matrix-min.js"></script>\n')
    hfile.write('<link rel="stylesheet" type="text/css" href="control.css">\n')
    hfile.write(fragmentShader);
    hfile.write(vertexShader);
    hfile.write('<script type="text/Javascript">\n')
    hfile.write(actionjs)
    hfile.write('function init() {wi = new WindowInteractor(0);}\n')
    hfile.write('</script>\n')
    hfile.write('</head>\n<body onload="init();">\n')
    hfile.write(output)
    hfile.write('</body>\n</html>\n')
    hfile.close()
    filename = os.path.join(binpath, "html/control.html")

def redisplay(id):
    #Simply update the active viewer image, if any
    try:
        if __IPYTHON__:
            from IPython.display import display,Javascript
            display(Javascript('redisplay(' + str(id) + ');'))
    except NameError, ImportError:
        pass

def render(html):
    try:
        if __IPYTHON__:
            from IPython.display import display,HTML
            display(HTML(html))
    except NameError, ImportError:
        global output
        output += html
        export()

def loadscripts(onload="", viewer=None, html=""):
    try:
        if __IPYTHON__:
            from IPython.display import display,HTML,Javascript
            #Create link to web content directory
            if not os.path.isdir("html"):
                os.symlink(os.path.join(binpath, 'html'), 'html')
            #Stylesheet, shaders and inline html
            display(HTML('<link rel="stylesheet" type="text/css" href="html/control.css">\n' + fragmentShader + vertexShader + html))
            #Load external scripts via require.js
            #Append onload code to the callback to init after scripts loaded
            js = """
            require.config({baseUrl: 'html',
                            paths: {'control': ["control"],
                                    'ok': ["OK-min"],
                                    'gl-matrix': ["gl-matrix-min"],
                                    'drawbox': ["drawbox"]
                                   }
                           }
                          );

            require(["control", "ok", "gl-matrix", "drawbox"], function() {
                console.log("Loaded scripts");
                ---ONLOAD---
                return {};
            });
            """
            js = js.replace('---ONLOAD---', onload);
            display(Javascript(js))
    except NameError, ImportError:
        pass

def window(viewer, html="", align="left"):
    viewerid = len(windows)

    html += '<div style="position: relative; float: ' + align + '; display: inline;" data-id="' + str(viewerid) + '">'
    html += '<img id="imgtarget_' + str(viewerid) + '" draggable=false style="border: 1px solid #aaa;"></img>'
    html += '</div>'

    #Append the viewer ref
    windows.append(viewer)
    #print "Viewer appended " + str(id(viewer)) + " app= " + str(id(viewer.app)) + " # " + str(len(windows))
    try:
        if __IPYTHON__:
            #Script init and create windowinteractor
            loadscripts('var wi = new WindowInteractor(' + str(viewerid) + ');', viewer, html)
    except NameError, ImportError:
        render(html)
    #Return the ID
    return viewerid

def action(id, value):
    if len(actions) > id:
        #Pass the arguments list as function args
        return actions[id]["call"](*actions[id]["args"], value=value)
    return ""

def setter(target, property, command, value):
    target[property] = value
    return command

def filtersetter(target, index, property, value):
    f = target["filters"]
    f[index][property] = value
    target["filters"] = f
    return "redraw"

def commandsetter(command, value):
    return command + " " + str(value) + "\nredraw"

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
            actions.append({"call" : setter, "args" : [target, property, command]})
            if label == None:
                self.label = property.capitalize()
        elif command:
            actions.append({"call" : commandsetter, "args" : [command]})
            if label == None:
                self.label = command.capitalize()
        else:
            #Assume derived class will fill out the action
            actions.append({"call" : setter, "args" : []})

        if not self.label:
            self.label = ""

        #Get value from target if not provided
        if value == None and property != None:
          if target and property in target:
            value = target[property]
        self.value = value

    def onchange(self):
        return "; do_action(" + str(self.id) + ", this.value, this);"

    def show(self):
        #Show only this control
        viewerid = len(windows)-1 #Just use the most recently added interactor instance
        if viewerid < 0: viewerid = 0 #Not yet added, assume it will be
        html = '<div data-id="' + str(viewerid) + '" style="float: left;" class="lvctrl">\n'
        #html += style
        if self.label: html += '<p>' + self.label + ':</p>\n'
        html += self.controls()
        html += '</div>\n'
        render(html)

    def controls(self, type='number', attribs={}, onchange=""):
        html =  '<input type="' + type + '" '
        for key in attribs:
            html += key + '="' + str(attribs[key]) + '" '
        html += 'value="' + str(self.value) + '" '
        onchange += self.onchange();
        html += 'onchange="' + onchange + '" '
        html += '>\n'
        return html

class Checkbox(Control):

    def __init__(self, *args, **kwargs):

        super(Checkbox, self).__init__(*args, **kwargs)

    def controls(self):
        attribs = {}
        if self.value: attribs = {"checked" : "checked"};
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
        attribs = {"min" : self.range[0], "max" : self.range[1], "step" : self.step};
        html = super(Range, self).controls('number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += super(Range, self).controls('range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        return html

class TimeStepper(Range):
    def __init__(self, viewer, *args, **kwargs):

        #Acts as a command setter with some additional controls
        super(TimeStepper, self).__init__(target=viewer, label="Timestep", command="timestep", *args, **kwargs)

        self.timesteps = viewer.timesteps()
        self.range = (self.timesteps[0], self.timesteps[-1])
        self.step = 1;
        self.value = 0;

    def controls(self):
        attribs = {"min" : self.range[0], "max" : self.range[1], "step" : self.step};
        html = Control.controls(self, 'number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += Control.controls(self, 'range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        html += '<br>\n'
        html += '<input type="button" onclick="var el = this.previousElementSibling.previousElementSibling; el.stepDown(); el.onchange()" value="&larr;" />\n'
        html += '<input type="button" onclick="var el = this.previousElementSibling.previousElementSibling.previousElementSibling; el.stepUp(); el.onchange()" value="&rarr;" />\n'
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
        actions[self.ctrlmin.id] = {"call" : filtersetter, "args" : [target, filteridx, "minimum"]}
        actions[self.ctrlmax.id] = {"call" : filtersetter, "args" : [target, filteridx, "maximum"]}

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

