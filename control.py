import lavavu
js = """
<script type="text/Javascript">

var kernel;
if (parent.IPython) {
  kernel = parent.IPython.notebook.kernel;
  //This ensures we have imported the lavavu module in notebook scope
  //required for control event pass-back
  //(NOTE: this looks a bit hacky because it is, but also because commands must be one liners)
  kernel.execute('lavavu = None if not "lavavu" in globals() else lavavu');
  kernel.execute('import sys');
  kernel.execute('modules = dict(sys.modules)');
  kernel.execute('for m in modules: lavavu = modules[m].lavavu if "lavavu" in dir(modules[m]) else lavavu');
}

var img;

window.addEventListener('mouseup', mouseUp);

function set_target(id, viewer_id) {
  //Takes image element and viewer id
  //Setups up events and displays output to image
  //Switches to specified viewer id on mouseover
  el = document.getElementById(id);

  img = el;

  if (img && !img.listening) {
    img.addEventListener('mousedown', mouseDown);
    img.ondragstart = function() { return false; };
    img.addEventListener("wheel", mouseWheel);
    img.addEventListener("contextmenu", function(e) { e.preventDefault(); e.stopPropagation(); return false;});
    //Re-capture viewer on hover
    img.addEventListener("mouseover", function(e) {if (img != this) {recapture(viewer_id); set_target(id); }});
    img.listening = true;
  }

  //Initial image
  get_image();
}

function recapture(id) {
  if (!kernel) return;
  kernel.execute("lavavu.viewer = lavavu.control.viewers[" + id + "]");
  get_image();
}

function execute(cmd) {
  if (kernel) {
    kernel.execute('lavavu.viewer.commands("' + cmd + '")');
  } else {
    var url = "http://localhost:8080/command="
    x = new XMLHttpRequest();
    x.open('GET', url + cmd, true);
    x.send();
  }
}

var mouse = {};
mouse.timer = null;
mouse.wheelTimer = null;
mouse.spin = 0;
mouse.modifiers = "";
mouse.down = false;

function keyModifiers(e) {
  var modifiers = "";
  if (e.ctrlKey)
    modifiers += "C";
  if (e.altKey)
    modifiers += "A";
  if (e.shiftKey)
    modifiers += "S";
  return modifiers;
}

function mouseUp(e) {
  if (!img) return;
  window.removeEventListener('mousemove', mouseMove, true);
  if (mouse.down) { //mouse.moving) {
    mouse.down = false;
    var button = e.button+1;
    mouse.modifiers = keyModifiers(e);
    if (mouse.timer) {
      clearTimeout(mouse.timer)
      mouse.timer = null;
      imgDrag(e.clientX, e.clientY, button);
    } else {
      execute('mouse mouse=up,modifiers=' + mouse.modifiers + ',button=' + button + ',x=' + e.clientX + ',y=' + e.clientY);
      get_frame();
    }
    e.preventDefault();
    return false;
  }
}

function mouseDown(e) {
  window.addEventListener('mousemove', mouseMove, true);
  mouse.down = true;
  var button = e.button+1;
  mouse.modifiers = keyModifiers(e);
  execute('mouse mouse=down,modifiers=' + mouse.modifiers + ',button=' + button + ',x=' + e.clientX + ',y=' + e.clientY);
  e.preventDefault();
  return false;
}

function mouseMove(e) {
  if (!img) return;
  //if (!mouse.down) return true;
  var x = e.clientX;
  var y = e.clientY;
  var button = e.button+1;
  mouse.modifiers = keyModifiers(e);
  if (mouse.timer) clearTimeout(mouse.timer)
  mouse.timer = setTimeout(function () {imgDrag(x, y, button);}, 10);
  e.preventDefault();
  return false;
}

function mouseWheel(e) {
  mouse.modifiers = keyModifiers(e);
  if (mouse.wheelTimer) clearTimeout(mouse.wheelTimer)
  mouse.wheelTimer = setTimeout(function () {imgZoom(mouse.spin); mouse.spin = 0;}, 10);
  mouse.spin -= e.deltaY
  e.preventDefault();
  return false;
}

function imgDrag(x, y, button){
  if (!img) return;
  execute('mouse mouse=move,modifiers=' + mouse.modifiers + 'button=' + button + ',x=' + x + ',y=' + y);
  if (!mouse.timer)
    execute('mouse mouse=up,modifiers=' + mouse.modifiers + ',button=' + button + ',x=' + x + ',y=' + y);
  get_frame();
}

function imgZoom(factor){
  execute('mouse mouse=scroll,modifiers=' + mouse.modifiers + ',spin=' + factor);
  get_frame();
  mouse.wheelTimer = null;
}

var imgTimer = null;
function get_frame() {
  if (imgTimer) clearTimeout(imgTimer);
  imgTimer = setTimeout(function () {get_image();}, 20);
}

function get_image(cmd) {
  //console.log('get')
  var callbacks = {'output' : function(out) {
      data = out.content.data['text/plain']
      data = data.substring(1, data.length-1)
      if (img) img.src = data;
    }
  };
  if (kernel) {
    kernel.execute('lavavu.viewer.frame((640, 480))', {iopub: callbacks}, {silent: false});
  }
  //else
  //  alert('TODO: html get image');
}

function set_prop(obj, prop, val) {
  execute("select " + obj);
  execute(prop + "=" + val);
  get_image();
}

function do_action(id, val) {
  //alert("lavavu.control.action(" + id + "," + val + ")")
  if (kernel) {
    kernel.execute("cmds = lavavu.control.action(" + id + "," + val + ")");
    kernel.execute("if len(cmds): lavavu.viewer.parseCommands(cmds)");
  } else {
    //HTML control actions via http
    actions[id](val);
  }
  get_image();
}

</script>
"""

style = """
<style scoped>
  input { display: inline; }
  input[type=range] { width: 200px; }
  input[type=button] { width: 120px; }
  input[type=number] { width: 120px; height: 28px; border-radius: 0; border: 1px dotted #999;}
  pre { display: inline; }
  p  { color: gray; margin: 0px}
</style>
"""

output = ""

def export():
    #Dump all output to control.html
    import os
    filename = os.path.join(lavavu.viewer.app.binpath, "html/control.html")

    #Process actions
    actionjs = '<script type="text/Javascript">'
    actionjs += 'var actions = [];'
    for act in actions:
        print act["args"]
        if act["call"] == setter:
            print "SETTER: "
            if len(act["args"]) == 0:
                print "Empty action"
                #Add a dummy function
                actionjs += 'actions.push(function(value) {});'
            elif isinstance(act["args"][0], lavavu.Obj):
                name = act["args"][0]["name"]
                prop = act["args"][1]
                print "  OBJ : " + name + " : " + prop
                actionjs += 'actions.push(function(value) {set_prop("' + name + '", "' + prop + '", value);});'
            elif isinstance(act["args"][0], lavavu.Viewer):
                print "  GLOB "
        elif act["call"] == commandsetter:
            print "COMMAND"
        elif act["call"] == filtersetter:
            print "FILTER"

    actionjs += '</script>'

    hfile = open(filename, "w")
    hfile.write('<html><head>' + js + actionjs + '</head><body>' + output + '</body></html>')
    hfile.close()

def display():
    #Simply update the active viewer image, if any
    try:
        if __IPYTHON__:
            from IPython.display import display,Javascript
            display(Javascript('get_image();'))
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

def viewer(html=None, style=""):
    viewerid = len(viewers)
    style += "border: 1px solid #aaa; "
    style += "user-select: none; user-drag: none; "
    style += "-moz-user-select: none; -moz-user-drag: none; "
    style += "-webkit-user-select: none; -webkit-user-drag: none; "
    imgsrc = '<img id="imgtarget_' + str(viewerid) + '" draggable=false style="' + style + '" ></img>'
    #Optional template
    if html:
        html = html.replace("~~~TARGET~~~", imgsrc)
    else:
        html = imgsrc

    try:
        if __IPYTHON__:
            from IPython.display import display,HTML,Javascript
            if viewerid == 0:
                #Add the control script first time through
                display(HTML(js))
            viewers.append(lavavu.viewer)
            #Display the inline html
            display(HTML(html))
            #Execute javascript to set the output target to added image (also pass the viewer id)
            display(Javascript('set_target("imgtarget_' + str(viewerid) + '", ' + str(viewerid) + ');'))
    except NameError, ImportError:
        render(html)

#Register of controls and their actions
actions = []
#Register of viewers
viewers = []

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
    def __init__(self, showviewer=True):
        self.controls = []
        self.showviewer = showviewer

    def add(self, ctrl):
        self.controls.append(ctrl)

    def html(self):
        html = '<div style="padding:0px; margin: 0px;">'
        html += style
        if self.showviewer: html += "~~~TARGET~~~"
        for i in range(len(self.controls)):
            if self.controls[i].label and not isinstance(self.controls[i], Checkbox):
                html += '<p>' + self.controls[i].label + ':</p>'
            else:
                html += '<br>'
            html += self.controls[i].controls()
        html += '</div>'
        return html

    def show(self):
        html = self.html()
        if self.showviewer:
            viewer(html, "float: right; ")
        else:
            render(html)

class Control(object):

    def __init__(self, target=None, property=None, command=None, value=None, label=None):
        if not target:
            target = lavavu.viewer
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
        if not value and property:
          if target and property in target:
            value = target[property]
        self.value = value

    def onchange(self):
        return "; do_action(" + str(self.id) + ", this.value);"

    def show(self):
        #Show only this control with a border
        html = '<div>'
        html += style
        if self.label: html += '<p>' + self.label + ':</p>'
        html += self.controls()
        html += '</div>'
        render(html)

    def controls(self, type='number', attribs={}, onchange=""):
        html =  '<input type="' + type + '" '
        for key in attribs:
            html += key + '="' + str(attribs[key]) + '" '
        html += 'value="' + str(self.value) + '" '
        onchange += self.onchange();
        html += 'onchange="' + onchange + '" '
        html += '>'
        return html

class Checkbox(Control):

    def __init__(self, *args, **kwargs):

        super(Checkbox, self).__init__(*args, **kwargs)

    def controls(self):
        attribs = {}
        if self.value: attribs = {"checked" : "checked"};
        html = "<label>"
        html += super(Checkbox, self).controls('checkbox', attribs)
        html += " " + self.label + "</label>"
        return html

    def onchange(self):
        return "; do_action(" + str(self.id) + ", this.checked ? 1 : 0);"

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
    def __init__(self, timesteps, *args, **kwargs):

        #Acts as a command setter with some additional controls
        super(TimeStepper, self).__init__(label="Timestep", command="timestep", *args, **kwargs)

        self.timesteps = timesteps
        self.range = (self.timesteps[0], self.timesteps[-1])
        self.step = 1;
        self.value = 0;

    def controls(self):
        attribs = {"min" : self.range[0], "max" : self.range[1], "step" : self.step};
        html = Control.controls(self, 'number', attribs, onchange='this.nextElementSibling.value=this.value; ')
        html += Control.controls(self, 'range', attribs, onchange='this.previousElementSibling.value=this.value; ')
        html += """<button onclick="var el = this.previousElementSibling.previousElementSibling; el.stepDown(); el.onchange()">&larr;</button>"""
        html += """<button onclick="var el = this.previousElementSibling.previousElementSibling.previousElementSibling; el.stepUp(); el.onchange()">&rarr;</button>"""
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
        return self.ctrlmin.controls() + "<br>" + self.ctrlmax.controls()

class ObjectList(Control):
    def __init__(self, *args, **kwargs):
        super(ObjectList, self).__init__(label="Objects", *args, **kwargs)
        self.objctrls = []
        for obj in lavavu.viewer.objects.list:
            self.objctrls.append(Checkbox(obj, "visible", label=obj["name"])) 

    def controls(self):
        html = ""
        for ctrl in self.objctrls:
            html += ctrl.controls() + "<br>"
        return html

