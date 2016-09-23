import lavavu
#TODO: js added for every control, should be an external script which is included once only
js = """
<script type="text/Javascript">

var IPython = parent.IPython;
var kernel = IPython.notebook.kernel;
var img = document.getElementById('imgtarget');

var mouse = {};
mouse.timer = null;
mouse.wheelTimer = null;
mouse.spin = 0;
mouse.modifiers = "";
mouse.down = false;

if (img) {
  //Ensure cleaned up
  window.removeEventListener("mouseup", mouseUp);
  img.removeEventListener("wheel", mouseWheel);
  img.removeEventListener('mousedown', mouseDown);

  img.addEventListener('mousedown', mouseDown);
  img.ondragstart = function() { return false; };
  window.addEventListener('mouseup', mouseUp);
  //window.addEventListener('mousemove', mouseMove);
  img.addEventListener("wheel", mouseWheel);
  img.addEventListener("contextmenu", function(e) { e.preventDefault(); e.stopPropagation(); return false;})

  //Initial image
  get_image();
}

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
  window.removeEventListener('mousemove', mouseMove, true);
  //if (mouse.target === img || mouse.moving) {
  //alert(mouse.down);
  if (mouse.down) { //mouse.moving) {
    mouse.down = false;
    var button = e.button+1;
    mouse.modifiers = keyModifiers(e);
    if (mouse.timer) {
      clearTimeout(mouse.timer)
      mouse.timer = null;
      imgDrag(e.clientX, e.clientY, button);
    } else {
      kernel.execute('lavavu.viewer.mouse("mouse=up,modifiers=' + mouse.modifiers + ',button=' + button + ',x=' + e.clientX + ',y=' + e.clientY + '")');
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
  kernel.execute('lavavu.viewer.mouse("mouse=down,modifiers=' + mouse.modifiers + ',button=' + button + ',x=' + e.clientX + ',y=' + e.clientY + '")');
  e.preventDefault();
  return false;
}

function mouseMove(e) {
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
  kernel.execute('lavavu.viewer.mouse("mouse=move,modifiers=' + mouse.modifiers + 'button=' + button + ',x=' + x + ',y=' + y + '")');
  if (!mouse.timer)
    kernel.execute('lavavu.viewer.mouse("mouse=up,modifiers=' + mouse.modifiers + ',button=' + button + ',x=' + x + ',y=' + y + '")');
  get_frame();
}

function imgZoom(factor){
  kernel.execute('lavavu.viewer.mouse("mouse=scroll,modifiers=' + mouse.modifiers + ',spin=' + factor + '")');
  get_frame();
  mouse.wheelTimer = null;
}

imgTimer = null;
function get_frame() {
  if (imgTimer) clearTimeout(imgTimer);
  imgTimer = setTimeout(function () {get_image();}, 20);
}

function get_image(cmd) {
  //console.log('get')
  var callbacks = {'output' : function(out) {
      data = out.content.data['text/plain']
      data = data.substring(1, data.length-1)
      //var img = document.getElementById('imgtarget');
      if (img) img.src = data;
    }
  };
  kernel.execute('lavavu.viewer.frame((640, 480))', {iopub: callbacks}, {silent: false});
}

function set_prop(obj, val) {
  console.log('set: ' + obj)
  kernel.execute(obj + " = " + str(val));
  get_image();
}

function do_action(id, val) {
  //alert("lavavu.control.action(" + id + "," + val + ")")
  kernel.execute("cmds = lavavu.control.action(" + id + "," + val + ")");
  kernel.execute("if len(cmds): lavavu.viewer.parseCommands(cmds)");
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

def viewer():
    try:
      if __IPYTHON__:
        from IPython.display import display,Image,HTML
        display(HTML("<img id=imgtarget></img>" + js))
    except NameError, ImportError:
        pass

#Register of controls and their actions
actions = []

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
    return "reload"

def commandsetter(command, value):
    return command + " " + str(value) + "\nredraw"

class Panel(object):
    def __init__(self):
        self.controls = []

    def add(self, ctrl):
        self.controls.append(ctrl)

    def html(self):
        html = '<div style="padding:0px; margin: 0px;">'
        html += style
        for i in range(len(self.controls)):
            html += '<p>' + self.controls[i].label + ':</p>'
            html += self.controls[i].controls()
            html += '<img id="imgtarget" draggable=false style="float: right; user-select: none; user-drag: none; -moz-user-select: none; -moz-user-drag: none; -webkit-user-select: none; -webkit-user-drag: none;"></img>'
        html += '</div>'
        return html + js

    def display(self):
        html = self.html()
        try:
          if __IPYTHON__:
            from IPython.display import display,Image,HTML
            display(HTML(html + js))
            #self.export()
            #display(HTML('<iframe src="control.html" style="border: none; width:100%; height:500px"></iframe>'))
        except NameError, ImportError:
          return html + js

    def export(self, filename="control.html"):
        #Save panel UI in external html file
        actionjs = '<script type="text/Javascript">'
        #for act in actions:
        #    as
        actionjs += '</script>'
        hfile = open(filename, "w")
        hfile.write('<html><head</head><body>' + js + self.html() + '</body></html>')
        hfile.close()

class Control(object):

    def __init__(self, label="", target=None, property=None, command=None, value=None):
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
        elif command:
            actions.append({"call" : commandsetter, "args" : [command]})
        else:
            #Assume derived class will fill out the action
            actions.append({"call" : setter, "args" : []})

        #Get value from target if not provided
        if not value:
          if target and property and property in target:
            value = target[property]
        self.value = value

    def onchange(self):
        return "; do_action(" + str(self.id) + ", this.value);"

    #cmd = ""
    #if self.url: #eg: http://localhost:8080/command=
    #    cmd += "x = new XMLHttpRequest();"
    #    cmd += "x.open('GET', '" + self.url + this.value + "', true);"
    #    cmd += "x.send();"
    #return cmd

    def show(self):
        #Show only this control with a border
        html = '<div>'
        html += style
        html += self.controls()
        html += '</div>'
        try:
          if __IPYTHON__:
            from IPython.display import display,Image,HTML
            display(HTML(html + js))
        except NameError, ImportError:
          return html + js

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

    def __init__(self, label="", target=None, property=None, command=None, value=None, range=(0.,1.), step=None):

        super(Range, self).__init__(label, target, property, command, value)

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
    def __init__(self, label, target, filteridx, range=None, step=None):
        self.label = label
        self.filter = target["filters"][filteridx]

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
            self.objctrls.append(Checkbox(obj["name"], obj, "visible")) 

    def controls(self):
        html = ""
        for ctrl in self.objctrls:
            html += ctrl.controls() + "<br>"
        return html

