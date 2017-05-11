"""
LavaVu python interface: viewer utils & wrapper

NOTE: regarding sync of state between python and library
- sync from python to lavavu is immediate,
    property setting must always trigger a sync to python
- sync from lavavu to python is lazy, always need to call _get()
    before using state data
"""
import json
import math
import sys
import os
import glob
import control
import numpy

#Attempt to import swig module
libpath = "bin"
try:
    #This file should be found one dir above bin dir containing built modules
    binpath = os.path.join(os.path.dirname(__file__), 'bin')
    sys.path.append(binpath)
    import LavaVuPython
    modpath = os.path.abspath(os.path.dirname(__file__))
    libpath = os.path.join(modpath, "bin")
except Exception,e:
    print "LavaVu visualisation module load failed: " + str(e)
    raise

#Function to enable the interactive control interface
#Called on initial import, on subsequent imports must be called by user if required
#(ie: in IPython, if re-running notebook without re-starting)
enablecontrol = False
def resetcontrols():
    global enablecontrol
    enablecontrol = True
    #Reset initialised state
    control.initialised = False

resetcontrols()

#Some preset colourmaps
# aim to reduce banding artifacts by being either 
# - isoluminant
# - smoothly increasing in luminance
# - diverging in luminance about centre value
colourMaps = {}
#Isoluminant blue-orange
colourMaps["isolum"] = "#288FD0 #50B6B8 #989878 #C68838 #FF7520"
#Diverging blue-yellow-orange
colourMaps["diverge"] = "#288FD0 #fbfb9f #FF7520"
#Isoluminant rainbow blue-green-orange
colourMaps["rainbow"] = "#5ed3ff #6fd6de #7ed7be #94d69f #b3d287 #d3ca7b #efc079 #ffb180"
#CubeLaw indigo-blue-green-yellow
colourMaps["cubelaw"] = "#440088 #831bb9 #578ee9 #3db6b6 #6ce64d #afeb56 #ffff88"
#CubeLaw indigo-blue-green-orange-yellow
colourMaps["cubelaw2"] = "#440088 #1b83b9 #6cc35b #ebbf56 #ffff88"
#CubeLaw heat blue-magenta-yellow)
colourMaps["smoothheat"] = "#440088 #831bb9 #c66f5d #ebbf56 #ffff88"
#Paraview cool-warm (diverging)
colourMaps["coolwarm"] = "#3b4cc0 #7396f5 #b0cbfc #dcdcdc #f6bfa5 #ea7b60 #b50b27"
#colourMaps["coolwarm"] = "#4860d1 #87a9fc #a7c5fd #dcdcdc #f2c8b4 #ee8669 #d95847"

TOL_DEFAULT = 0.0001 #Default error tolerance for image tests

geomtypes = {"labels":    LavaVuPython.lucLabelType,
             "points":    LavaVuPython.lucPointType,
             "grid":      LavaVuPython.lucGridType,
             "triangles": LavaVuPython.lucTriangleType,
             "vectors":   LavaVuPython.lucVectorType,
             "tracers":   LavaVuPython.lucTracerType,
             "lines":     LavaVuPython.lucLineType,
             "shapes":    LavaVuPython.lucShapeType,
             "volume":    LavaVuPython.lucVolumeType}

datatypes = {"vertices":  LavaVuPython.lucVertexData,
             "normals":   LavaVuPython.lucNormalData,
             "vectors":   LavaVuPython.lucVectorData,
             "indices":   LavaVuPython.lucIndexData,
             "colours":   LavaVuPython.lucRGBAData,
             "texcoords": LavaVuPython.lucTexCoordData,
             "luminance": LavaVuPython.lucLuminanceData,
             "rgb":       LavaVuPython.lucRGBData,
             "values":    LavaVuPython.lucMaxDataType}

def convert_keys(dictionary):
    """Recursively converts dictionary keys
       and unicode values to utf-8 strings."""
    if isinstance(dictionary, list):
        for i in range(len(dictionary)):
            dictionary[i] = convert_keys(dictionary[i])
        return dictionary
    if not isinstance(dictionary, dict):
        if isinstance(dictionary, unicode):
            return dictionary.encode('utf-8')
        return dictionary
    return dict((k.encode('utf-8'), convert_keys(v)) 
        for k, v in dictionary.items())

#Wrapper class for drawing object
#handles property updating via internal dict
class Obj(object):
    """  
    The Object class provides an interface to a LavaVu drawing object
    Obj instances are created internally in the Viewer class and can
    be retrieved from the object list

    New objects are also created using viewer methods
    
    Example
    -------
        
    Create a viewer, load some test data, get objects
    
    >>> import lavavu
    >>> lv = lavavu.Viewer()
    >>> lv.test()
    >>> objects = lv.getobjects()
    >>> print objects

    """
    def __init__(self, idict, instance, *args, **kwargs):
        self.dict = idict
        self.instance = instance

    def name(self):
        return str(self.dict["name"])

    def _setprops(self, props):
        #Replace props with new data from app
        self.dict.clear()
        self.dict.update(props)

    def _set(self):
        #Send updated props (via ref in case name changed)
        self.instance._setupobject(self.ref, **self.dict)

    def __getitem__(self, key):
        self.instance._get() #Ensure in sync
        return self.dict[key]

    def __setitem__(self, key, value):
        #self.instance._get() #Ensure in sync
        #Set new value and send
        self.dict[key] = value
        self._set()

    def __contains__(self, key):
        return key in self.dict

    def __str__(self):
        self.instance._get() #Ensure in sync
        return str('\n'.join(['%s=%s' % (k,json.dumps(v)) for k,v in self.dict.iteritems()]))

    #Interface for setting filters
    def include(self, *args, **kwargs):
        return self.filter(*args, out=False, **kwargs)

    def includemap(self, *args, **kwargs):
        return self.filter(*args, out=False, map=True, **kwargs)

    def exclude(self, *args, **kwargs):
        return self.filter(*args, out=True, **kwargs)
            
    def excludemap(self, *args, **kwargs):
        return self.filter(*args, out=True, map=True, **kwargs)

    def filter(self, label, values, out=False, map=False):
        #Pass a tuple for exclusive range (min < val < max), list for inclusive range (min <= val <= max)
        filter = {"by" : label, "minimum" : values[0], "maximum" : values[1], "map" : map, "out" : out, "inclusive" : False}
        if isinstance(values, list):
            filter["inclusive"] = True
        self["filters"].append(filter)
        self._set()
        return len(self["filters"])-1

    def datasets(self):
        #Return json data set list
        #TODO: use Geometry wrapper?
        return json.dumps(self.dict["data"])

    def append(self):
        self.instance.append(self.id) #self.name())

    def triangles(self, data, split=0):
        if split > 1:
            self.instance.app.loadTriangles(self.ref, data, self.name(), split)
        else:
            self.vertices(data)

    def _loadScalar(self, data, dtype):
        #Passes a scalar dataset (float/uint8/uint32)
        if not isinstance(data, numpy.ndarray):
            data = numpy.asarray(data, dtype=numpy.float32)
        if data.dtype == numpy.float32:
            self.instance.app.arrayFloat(self.ref, data.ravel(), dtype)
        elif data.dtype == numpy.uint32:
            self.instance.app.arrayUInt(self.ref, data.ravel(), dtype)
        elif data.dtype == numpy.uint8:
            self.instance.app.arrayUChar(self.ref, data.ravel(), dtype)

    def _loadVector(self, data, dtype):
        #Passes a vector dataset (float)
        if not isinstance(data, numpy.ndarray) or data.dtype != numpy.float32:
            data = numpy.asarray(data, dtype=numpy.float32)
        self.instance.app.arrayFloat(self.ref, data.ravel(), dtype)

    def data(self, filter=None):
        return Geometry(self, filter)

    def vertices(self, data=None):
        self._loadVector(data, LavaVuPython.lucVertexData)

    def normals(self, data):
        self._loadVector(data, LavaVuPython.lucNormalData)

    def vectors(self, data):
        self._loadVector(data, LavaVuPython.lucVectorData)

    def values(self, data, label="default"):
        if not isinstance(data, numpy.ndarray) or data.dtype != numpy.float32:
            data = numpy.asarray(data, dtype=numpy.float32)
        self.instance.app.arrayFloat(self.ref, data.ravel(), label)

    def colours(self, data):
        if isinstance(data, numpy.ndarray):
            self._loadScalar(data, LavaVuPython.lucRGBAData)
            return
        #Convert to list of strings
        if isinstance(data, str):
            data = data.split()
        if len(data) < 1: return
        #Load as string array or unsigned int array
        if isinstance(data[0], list):
            #Convert to strings for json parsing first
            data = [str(i) for i in data]
        if isinstance(data[0], str):
            #Each element will be parsed as a colour string
            self.instance.app.loadColours(self.ref, data)
        else:
            #Plain list, assume unsigned colour data
            data = numpy.asarray(data, dtype=numpy.uint32)
            self.colours(data)

    def indices(self, data):
        #Accepts only uint32 indices
        if not isinstance(data, numpy.ndarray) or data.dtype != numpy.uint32:
            data = numpy.asarray(data, dtype=numpy.uint32)
        self._loadScalar(data, LavaVuPython.lucIndexData)

    def texture(self, data, width, height, depth=4, flip=True):
        if not isinstance(data, numpy.ndarray):
            data = numpy.asarray(data, dtype=numpy.uint32)
        if data.dtype == numpy.uint32:
            self.instance.app.textureUInt(self.ref, data.ravel(), width, height, depth, flip)
        elif data.dtype == numpy.uint8:
            self.instance.app.textureUChar(self.ref, data.ravel(), width, height, depth, flip)

    def label(self, data):
        if isinstance(data, str):
            data = [data]
        self.instance.app.label(self.ref, data)

    def colourmap(self, data, **kwargs):
        #Load colourmap and set property on this object
        cmap = self.instance.colourmap(self.name() + '-default', data, **kwargs)
        self["colourmap"] = cmap
        self.instance.app.reloadObject(self.ref)
        return cmap

    def select(self):
        self.instance.app.aobject = self.ref
    
    def file(self, *args, **kwargs):
        #Load file with this object selected (import)
        self.select()
        self.instance.file(*args, name=self.name(), **kwargs)
        self.instance.app.aobject = None

    def colourbar(self, name=None, **kwargs):
        #Create a new colourbar for this object
        ref = self.instance.app.colourBar(self.ref)
        if not ref: return
        #Update list
        self.instance._get() #Ensure in sync
        #Setups up new object, all other args passed to properties dict
        return self.instance._setupobject(ref, **kwargs)

    def save(self, filename="state.json"):
        with open(filename, "w") as state_file:
            state_file.write(self.app.getState())

    def load(self, filename="state.json"):
        with open(filename, "r") as state_file:
            self.app.setState(state_file.read())

    def clear(self):
        self.instance.app.clearObject(self.ref)

    def cleardata(self, typename=""):
        if typename in datatypes:
            #Found data type name
            dtype = datatypes[typename]
            self.instance.app.clearData(self.ref, dtype)
        else:
            #Assume values by label (or all values if blank)
            self.instance.app.clearValues(self.ref, typename)

    def update(self, geomname=None, compress=True):
        #Update object data at current timestep
        if geomname is None:
            #Re-writes all data to db for this object
            self.instance.app.update(self.ref, compress)
        else:
            #Re-writes data to db for this object and geom type
            self.instance.app.update(self.ref, geomtypes[geomname], compress)

    def getcolourmap(self, array=False):
        #Return colourmap as a string/array that can be passed to re-create the map
        cmid = self["colourmap"]
        arr = []
        if cmid < 0: return [] if array else ''

        cmaps = self.instance.state["colourmaps"]
        cm = cmaps[cmid]
        if array:
            arrstr = '['
            import re
            for c in cm["colours"]:
                comp = re.findall(r"[\d\.]+", c["colour"])
                comp = [int(comp[0]), int(comp[1]), int(comp[2]), int(255*float(comp[3]))]
                arrstr += "(%6.4f, %s),\n" % (c["position"], str(comp))
            return arrstr[0:-2] + ']\n'
        else:
            cmstr = '"""\n'
            for c in cm["colours"]:
                cmstr += "%6.4f=%s\n" % (c["position"],c["colour"])
            cmstr += '"""\n'
            return cmstr

    def isosurface(self, name=None, convert=False, updatedb=False, compress=True, **kwargs):
        #Generate and return an isosurface object, 
        #pass properties as kwargs (eg: isovalues=[])
        isobj = self
        if not convert:
            #Create a new object for the surface
            if name is None: name = self.name() + "_surface"
            isobj = self.instance.add(name, **kwargs)
            isobj["geometry"] = "triangles"
        else:
            #Convert existing object (self) set properties 
            self.instance._setupobject(self.ref, **kwargs)
        #Create surface, If requested, write the new data to the database
        self.instance.isosurface(isobj.ref, self.ref, convert)
        #Re-write modified types to the database
        if updatedb:
            self.instance.update(isobj.ref, LavaVuPython.lucVolumeType, compress)
            self.instance.update(isobj.ref, LavaVuPython.lucTriangleType, compress)
        return isobj

#Wrapper dict+list of objects
class Objects(dict):
    """  
    The Objects class is used internally to manage and synchronise the drawing object list
    """
    def __init__(self, instance):
        self.instance = instance

    def _sync(self):
        self.list = []
        for obj in self.instance.state["objects"]:
            if obj["name"] in self:
                #Update object with new properties
                self[obj["name"]]._setprops(obj)
                self.list.append(self[obj["name"]])
            else:
                #Create a new object wrapper
                o = Obj(obj, self.instance)
                self[obj["name"]] = o
                self.list.append(o)
            #Flag sync
            self[obj["name"]].found = True
            #Save the object id and reference (use id # to get)
            _id = len(self.list)
            self.list[-1].id = _id
            self.list[-1].ref = self.instance.getObject(_id)
            
        #Delete any objects from dict that are no longer present
        for name in self.keys():
            if not self[name].found:
                del self[name]
            else:
                self[name].found = False

    def __str__(self):
        return '[' + ', '.join(self.keys()) + ']'

class Viewer(object):
    """  
    The Viewer class provides an interface to a LavaVu session
    
    Parameters
    ----------
    **kwargs: 
        Global properties passed to the created viewer
            
    Example
    -------
        
    Create a viewer, set the background colour to white
    
    >>> import lavavu
    >>> lv = lavavu.Viewer(background="white")

    """
    def __init__(self, binpath=libpath, omegalib=False, *args, **kwargs):
        self.resolution = (640,480)
        self._ctr = 0
        self.app = None
        self.objects = Objects(self)
        try:
            self.app = LavaVuPython.LavaVu(binpath, omegalib)
            self.setup(*args, **kwargs)

            #Control setup, expect html files in same path as viewer binary
            global enablecontrol
            if enablecontrol:
                control.htmlpath = os.path.join(binpath, "html")
                if not os.path.isdir(control.htmlpath):
                    control.htmlpath = None
                    print("Can't locate html dir, interactive view disabled")
                else:
                    control.initialise()
                #Copy control module ref so can be accessed from instance
                #TODO: Just move control.py functions to Viewer class
                self.control = control
        except RuntimeError,e:
            print "LavaVu Init error: " + str(e)
            pass

    def setup(self, arglist=None, database=None, figure=None, timestep=None, 
         port=0, verbose=False, interactive=False, hidden=True, cache=False,
         quality=2, writeimage=False, resolution=None, script=None, initscript=False, usequeue=False, **kwargs):
        #Convert options to args
        args = []
        if not initscript:
          args += ["-S"]
        if verbose:
          args += ["-v"]
        #Automation: scripted mode, no interaction
        if not interactive:
          args += ["-a"]
        #Hidden window
        if hidden:
          args += ["-h"]
        #Timestep cache
        if cache:
          args += ["-c1"]
        #Subsample anti-aliasing for image output
        args += ["-z" + str(quality)]
        #Timestep range
        if timestep != None:
            if isinstance(timestep, int):
                args += ["-" + str(timestep)]
            if isinstance(timestep, (tuple, list)) and len(timestep) > 1:
                args += ["-" + str(timestep[0]), "-" + str(timestep[1])]
        #Web server
        args += ["-p" + str(port)]
        #Database file
        if database:
          args += [database]
        #Initial figure
        if figure != None:
          args += ["-f" + str(figure)]
        #Resolution
        if resolution != None and isinstance(resolution,tuple) or isinstance(resolution,list):
          #Output res
          args += ["-x" + str(resolution[0]) + "," + str(resolution[1])]
          #Interactive res
          args += ["-r" + str(resolution[0]) + "," + str(resolution[1])]
          self.resolution = resolution
        #Save image and quit
        if writeimage:
          args += ["-I"]
        if script and isinstance(script,list):
          args += script
        if arglist:
            args += arglist
        self.queue = usequeue

        try:
            self.app.run(args)
            if database:
                #Load objects/state
                self._get()
            #Additional keyword args = properties
            for key in kwargs:
                self[key] = kwargs[key]
        except RuntimeError,e:
            print "LavaVu Run error: " + str(e)
            pass

    #dict methods
    def __getitem__(self, key):
        #Get view/global property
        self._get()
        view = self.state["views"][0]
        if key in view:
            return view[key]
        elif key in self.state:
            return self.state[key]
        else:
            return self.state["properties"][key]
        return None

    def __setitem__(self, key, item):
        #Set view/global property
        #self.app.parseCommands("select") #Ensure no object selected
        #self.app.parseCommands(key + '=' + str(item))
        self._get()
        #self.state = json.loads(self.app.getState())
        view = self.state["views"][0]
        if key in view:
            view[key] = item
        elif key in self.state:
            self.state[key] = item
        else:
            self.state["properties"][key] = item
        self._set()

    def __contains__(self, key):
        return key in self.state or key in self.state["properties"] or key in self.state["views"][0]

    def __str__(self):
        #View/global props to string
        self._get()
        properties = self.state["properties"]
        properties.update(self.state["views"][0])
        return str('\n'.join(['    %s=%s' % (k,json.dumps(v)) for k,v in properties.iteritems()]))

    def _get(self):
        #Import state from lavavu
        self.state = convert_keys(json.loads(self.app.getState()))
        self.objects._sync()

    def _set(self):
        #Export state to lavavu
        #(include current object list state)
        #self.state["objects"] = [obj.dict for obj in self.objects.list]
        self.app.setState(json.dumps(self.state))

    def getobjects(self):
        self._get()
        return self.objects

    def commands(self, cmds):
        if isinstance(cmds, list):
            cmds = '\n'.join(cmds)
        if self.queue: #Thread safe queue requested
            self.app.queueCommands(cmds)
        else:
            self.app.parseCommands(cmds)

    #Callable with commands...
    def __call__(self, cmds):
        self.commands(cmds)

    #Undefined methods supported directly as LavaVu commands
    def __getattr__(self, key):
        #__getattr__ called if no attrib/method found
        def any_method(*args, **kwargs):
            #If member function exists on LavaVu, call it
            method = getattr(self.app, key, None)
            if method and callable(method):
                return method(*args, **kwargs)
            #Check for add object by geom type shortcut
            if key in ["labels", "points", "quads", "triangles", "vectors", "tracers", "lines", "shapes", "volume"]:
                #Allows calling add by geometry type, eg: obj = lavavu.lines()
                return self._addtype(key, *args, **kwargs)
            #Otherwise, pass args as command string
            argstr = key
            for arg in args:
                argstr += " " + str(arg)
            self.commands(argstr)
        return any_method

    def _setupobject(self, ref=None, **kwargs):
        #Strip data keys from kwargs and put aside for loading
        datasets = {}
        cmapstr = None
        for key in kwargs.keys():
            if key in ["vertices", "normals", "vectors", "colours", "indices", "values", "labels"]:
                datasets[key] = kwargs.pop(key, None)
            if key == "colourmap" and isinstance(kwargs[key], str):
                cmapstr = kwargs.pop(key, None)

        #Call function to add/setup the object, all other args passed to properties dict
        if ref is None:
            ref = self.app.createObject(str(json.dumps(kwargs)))
        else:
            self.app.setObject(ref, str(json.dumps(kwargs)))

        #Get the created/update object
        obj = self.getobject(ref)

        #Read any property data sets (allows object creation and load with single prop dict)
        for key in datasets:
            #Get the load function matching the data set (eg: vertices() ) and call on data
            func = getattr(obj, key)
            func(datasets[key])

        if not cmapstr is None:
            #Convert string colourmap
            obj.colourmap(cmapstr)

        #Return wrapper obj
        return obj

    def add(self, name, **kwargs):
        if isinstance(self.objects, Objects) and name in self.objects:
            print "Object exists: " + name
            return self.objects[name]

        #Put provided name in properties
        if len(name):
            kwargs["name"] = name

        #Adds a new object, all other args passed to properties dict
        return self._setupobject(ref=None, **kwargs)

    #Shortcut for adding specific geometry types
    def _addtype(self, typename, name=None, **kwargs):
        #Set name to typename if none provided
        if not name: 
            self._ctr += 1
            name = typename + str(self._ctr)
        kwargs["geometry"] = typename
        return self.add(name, **kwargs)

    def grid(self, name=None, width=100, height=100, dims=[2,2], verts=[], *args, **kwargs):
        obj = self._addtype("quads", name, dims=dims, *args, **kwargs)
        if len(verts) != dims[0]*dims[1]:
            verts = []
            yc = 0.0
            for y in range(dims[1]):
                xc = 0.0
                for x in range(dims[0]):
                    verts.append([xc, yc, 0])
                    xc += width / float(dims[0])
                yc += height / float(dims[1])
        obj.vertices(verts)
        return obj

    def getobject(self, identifier=None):
        #Return object by name/ref or last in list if none provided
        #Get state and update object list
        self._get()
        if len(self.objects.list) == 0:
            print "WARNING: No objects exist!"
            return None
        #If name passed, find this object in updated list, if not just use the last
        if isinstance(identifier, str):
            for obj in self.objects.list:
                if obj["name"] == identifier:
                    return obj
        if isinstance(identifier, int):
            if len(self.objects.list) >= identifier:
                return self.objects.list[identifier-1]
        elif isinstance(identifier, LavaVuPython.DrawingObject):
            for obj in self.objects.list:
                if obj.ref == identifier:
                    return obj
        return self.objects.list[-1]

    def file(self, filename, name=None, **kwargs):
        #Load a new object from file
        self.app.loadFile(filename)

        #Get object by name (or last if none provided)
        obj = self.getobject(name)

        #Setups up new object, all other args passed to properties dict
        return self._setupobject(obj.ref, **kwargs)
    
    def files(self, filespec, name=None, **kwargs):
        #Load list of files with glob
        filelist = glob.glob(filespec)
        obj = None
        for infile in sorted(filelist):
            obj = self.file(infile, kwargs)
        return obj

    def colourmap(self, name, data, **kwargs):
        datastr = data
        if isinstance(data, list):
            #Convert list map to string format
            datastr = ""
            for item in data:
                if isinstance(item, list) or isinstance(item, tuple):
                    datastr += str(item[0]) + '=' + str(item[1]) + '\n'
                else:
                    datastr += item + '\n'
        elif data in colourMaps.keys():
            #Use a predefined map by name
            datastr = colourMaps[data]
        data = datastr
        #Load colourmap
        return self.app.colourMap(name, data, str(json.dumps(kwargs)))

    def clear(self):
        self.close()

    def timesteps(self):
        return json.loads(self.app.getTimeSteps())

    def addstep(self):
        return self.app.parseCommands("newstep")

    def frame(self, resolution=None):
        #Jpeg encoded frame data
        if not resolution: resolution = self.resolution
        return self.app.image("", resolution[0], resolution[1], 85);

    def display(self, resolution=(640,480)):
        """        
        Shows the generated image inline within an ipython notebook.
        
        If IPython is installed, displays the result image content inline

        If IPython is not installed, this method will call the default image 
        output routines to save the result with a default filename in the current directory

        """
        try:
            if __IPYTHON__:
                from IPython.display import display,Image,HTML
                #Return inline image result
                img = self.app.image("", resolution[0], resolution[1])
                display(HTML("<img src='%s'>" % img))
        except NameError, ImportError:
            #Not in IPython, call default image save routines (autogenerated filenames)
            self.app.image("displayed", resolution[0], resolution[1])

    def webgl(self, resolution=(640,480)):
        """        
        Shows the generated model inline within an ipython notebook.
        
        If IPython is installed, displays the result WebGL content inline

        If IPython is not installed, this method will call the default web 
        output routines to save the result with a default filename in the current directory

        """

        try:
            if __IPYTHON__:
                #TODO: build single html file instead with inline scripts/data/custom controls
                #Create link to web content directory
                if not os.path.isdir("html"):
                    htmldir = os.path.join(binpath, 'html')
                    os.symlink(htmldir, 'html')
                from IPython.display import display,Image,HTML
                #Write files to disk first, can be passed directly on url but is slow for large datasets
                filename = "input.json"
                text_file = open("html/" + filename, "w")
                text_file.write(self.web());
                text_file.close()
                from IPython.display import IFrame
                display(IFrame("html/viewer.html#" + filename, width=resolution[0], height=resolution[1]))
        except NameError, ImportError:
            self.app.web(True)
        except Exception,e:
            print "WebGL output error: " + str(e)
            pass

    def video(self, filename="", fps=10, resolution=(640,480)):
        """        
        Shows the generated video inline within an ipython notebook.
        
        If IPython is installed, displays the result video content inline

        If IPython is not installed, this method will call the default video 
        output routines to save the result in the current directory

        """

        try:
            if __IPYTHON__:
                from IPython.display import display,HTML
                fn = self.app.video(filename, fps, resolution[0], resolution[1])
                html = """
                <video src="---FN---" controls loop>
                Sorry, your browser doesn't support embedded videos, 
                </video><br>
                <a href="---FN---">Download Video</a> 
                """
                #Get a UUID based on host ID and current time
                import uuid
                uid = uuid.uuid1()
                html = html.replace('---FN---', fn + "?" + str(uid))
                display(HTML(html))
        except NameError, ImportError:
            self.app.web(True)
        except Exception,e:
            print "Video output error: " + str(e)
            pass

    def imageBytes(self, width=640, height=480, channels=3):
        img = numpy.zeros(shape=(width,height,channels), dtype=numpy.uint8)
        self.imageBuffer(img)
        return img

    def testimages(self, imagelist=None, tolerance=TOL_DEFAULT, expectedPath='expected/', outputPath='./', clear=True):
        results = []
        if not imagelist:
            #Default to all png images in expected dir
            cwd = os.getcwd()
            os.chdir(expectedPath)
            imagelist = glob.glob("*.png")
            os.chdir(cwd)
        for f in imagelist:
            outfile = outputPath+f
            expfile = expectedPath+f
            results.append(self.testimage(outfile, expfile, tolerance))
        #Combined result
        overallResult = all(results)
        if not overallResult:
            raise RuntimeError("Image tests failed due to one or more image comparisons above tolerance level!")
        if clear:
            try:
                for f in imagelist:
                    os.remove(f)
            except:
                pass
        print "-------------\nTests Passed!\n-------------"

    def testimage(self, outfile, expfile, tolerance=TOL_DEFAULT):
        if len(expfile) and not os.path.exists(expfile):
            print "Test skipped, Reference image '%s' not found!" % expfile
            return 0
        if len(outfile) and not os.path.exists(outfile):
            raise RuntimeError("Generated image '%s' not found!" % outfile)

        diff = self.imageDiff(outfile, expfile)
        result = diff <= tolerance
        reset = '\033[0m'
        red = '\033[91m'
        green = '\033[92m'
        failed = red + 'FAIL' + reset
        passed = green + 'PASS' + reset
        if not result:
            print "%s: %s Image comp errors %f, not"\
                  " within tolerance %g of reference image."\
                % (failed, outfile, diff, tolerance)
        else:
            print "%s: %s Image comp errors %f, within tolerance %f"\
                  " of ref image."\
                % (passed, outfile, diff, tolerance)
        return result

    def serve(self):
        if not self.control: return
        try:
            import server
            os.chdir(control.htmlpath)
            server.serve(self)
        except Exception,e:
            print "LavaVu error: " + str(e)
            print "Web Server failed to run"
            import traceback
            traceback.print_exc()
            pass

    def window(self):
        if not self.control: return
        #Interactive viewer instance, store id
        self.winid = control.window(self)

    def redisplay(self):
        #Issue redisplay to active viewer, or last saved 
        if not self.control: return
        if isinstance(self.control.winid, int):
            control.redisplay(self.control.winid)
        else:
            control.redisplay(self.winid)

    def camera(self):
        self._get()
        me = getVariableName(self)
        print me + ".translation(" + str(self.state["views"][0]["translate"])[1:-1] + ")"
        print me + ".rotation(" + str(self.state["views"][0]["rotate"])[1:-1] + ")"
        #Also print in terminal for debugging
        self.commands("camera")

    def view(self):
        self._get()
        return self.state["views"][0]

#Wrapper for list of geomdata objects
class Geometry(list):
    """  
    The Geometry class provides an interface to a drawing object's internal data
    Geometry instances are created internally in the Obj class with the data() method

    Example
    -------
        
    Get all object data

    >>> data = obj.data()
    
    Get only triangle data

    >>> data = obj.data("triangles")

    Loop through data

    >>> data = obj.data()
    >>> for el in obj.data:
    >>>     print el

    """
    def __init__(self, obj, filter=None):
        self.obj = obj

        #Get a list of geometry data objects for a given drawing object
        count = self.obj.instance.app.getGeometryCount(self.obj.ref)
        for idx in range(count):
            g = self.obj.instance.app.getGeometry(self.obj.ref, idx)
            #By default all elements are returned, even if object has multiple types 
            #Filter can be set to a type name to exclude other geometry types
            if filter is None or g.type == geomtypes[filter]:
                self.append(GeomData(g, obj.instance))

    def __str__(self):
        return '[' + ', '.join([str(i) for i in self]) + ']'

#Wrapper class for GeomData geometry object
class GeomData(object):
    """  
    The GeomData class provides an interface to a single object data element
    GeomData instances are created internally from the Geometry class

    copy(), get() and set() methods provide access to the data types

    Example
    -------
        
    Get the most recently used object data element

    >>> data = obj.data()
    >>> el = data[-1]
    
    Get a copy of the rgba colours (if any)

    >>> colours = el.copy("rgba")

    Get a view of the vertex data

    >>> verts = el.get("vertices")

    WARNING: this reference may be deleted by other calls, 
    only use get() if you are processing the data immediately 
    and not relying on it continuing to exist later

    Get a copy of some value data by label
    >>> verts = el.copy("colourvals")

    Load some new values for these vertices

    >>> el.set("sampledfield", newdata)

    """
    def __init__(self, data, instance):
        self.data = data
        self.instance = instance

    def get(self, typename):
        #Warning... other than for internal use, should always make copies of data
        # there is no guarantee memory will not be released
        array = None
        if typename in datatypes:
            if typename in ["luminance", "rgb"]:
                #Get uint8 data
                array = self.instance.app.geometryArrayViewUChar(self.data, datatypes[typename])
            elif typename in ["indices", "colours"]:
                #Get uint32 data
                array = self.instance.app.geometryArrayViewUInt(self.data, datatypes[typename])
            else:
                #Get float32 data
                array = self.instance.app.geometryArrayViewFloat(self.data, datatypes[typename])
        else:
            #Get float32 data
            array = self.instance.app.geometryArrayViewFloat(self.data, typename)
        
        return array

    def copy(self, typename):
        #Safer data access, makes a copy to ensure we still have access 
        #to the data no matter what viewer does with it
        return numpy.copy(self.get(typename))

    def set(self, typename, array):
        if typename in datatypes:
            if typename in ["luminance", "rgb"]:
                #Set uint8 data
                self.instance.app.geometryArrayUInt8(self.data, array, datatypes[typename])
            elif typename in ["indices", "colours"]:
                #Set uint32 data
                self.instance.app.geometryArrayUInt32(self.data, array, datatypes[typename])
            else:
                #Set float32 data
                self.instance.app.geometryArrayFloat(self.data, array, datatypes[typename])
        else:
            #Set float32 data
            self.instance.app.geometryArrayFloat(self.data, array, typename)
        
    def __str__(self):
        return [key for key, value in geomtypes.items() if value == self.data.type][0]



def cubeHelix(samples=16, start=0.5, rot=-0.9, sat=1.0, gamma=1., alpha=False):
    """
    Create CubeHelix spectrum colourmap with monotonically increasing/descreasing intensity

    Implemented from FORTRAN 77 code from D.A. Green, 2011, BASI, 39, 289.
    "A colour scheme for the display of astronomical intensity images"
    http://adsabs.harvard.edu/abs/2011arXiv1108.5083G

    samples: number of colour samples to produce
    start: start colour [0,3] 1=red,2=green,3=blue
    rot: rotations through spectrum, negative to reverse direction
    sat: colour saturation grayscale to full [0,1], >1 to oversaturate
    gamma: gamma correction [0,1]
    alpha: set true for transparent to opaque alpha

    """

    colours = []

    for i in range(0,samples+1):
        fract = i / float(samples)
        angle = 2.0 * math.pi * (start / 3.0 + 1.0 + rot * fract)
        amp = sat * fract * (1 - fract)
        fract = pow(fract, gamma)

        r = fract + amp * (-0.14861 * math.cos(angle) + 1.78277 * math.sin(angle))
        g = fract + amp * (-0.29227 * math.cos(angle) - 0.90649 * math.sin(angle))
        b = fract + amp * (+1.97294 * math.cos(angle))
                
        r = max(min(r, 1.0), 0.0)
        g = max(min(g, 1.0), 0.0)
        b = max(min(b, 1.0), 0.0)
        a = fract if alpha else 1.0

        colours.append((fract, 'rgba(%d,%d,%d,%d)' % (r*0xff, g*0xff, b*0xff, a*0xff)))

    return colours

def getVariableName(var):
    """
    Attempt to find the name of a variable from the main module namespace
    """
    import __main__ as main_mod
    for name in dir(main_mod):
        val = getattr(main_mod, name)
        if val is var:
            return name
    return None

