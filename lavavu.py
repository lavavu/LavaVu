#LavaVu python interface: viewer utils & wrapper
import control
import json
import math
import sys
import os
import glob

#Attempt to import swig module
try:
    #This file should be found one dir above bin dir containing built modules
    binpath = os.path.join(os.path.dirname(control.__file__), 'bin')
    sys.path.append(binpath)
    import LavaVuPython
    libpath = os.path.abspath(os.path.dirname(LavaVuPython.__file__))
    #Expect html files in same path as viewer lib
    control.htmlpath = os.path.join(libpath, "html")
    if not os.path.isdir(control.htmlpath):
        control.htmlpath = None
        print("Can't locate html dir, interactive view disabled")
    #Import javascript for controls (requires htmlpath)
    control.loadscripts()
except Exception,e:
    print "LavaVu visualisation module load failed: " + str(e)
    raise

#Type lookups
geomtypes={}
geomtypes["label"] = geomtypes["min"] = LavaVuPython.lucLabelType
geomtypes["point"] = LavaVuPython.lucPointType
geomtypes["grid"] = LavaVuPython.lucGridType
geomtypes["triangle"] = LavaVuPython.lucTriangleType
geomtypes["vector"] = LavaVuPython.lucVectorType
geomtypes["tracer"] = LavaVuPython.lucTracerType
geomtypes["line"] = LavaVuPython.lucLineType
geomtypes["shape"] = LavaVuPython.lucShapeType
geomtypes["volume"] = geomtypes["max"] = LavaVuPython.lucVolumeType

datatypes={}
datatypes["vertex"] = datatypes["min"] = LavaVuPython.lucVertexData
datatypes["normal"] = LavaVuPython.lucNormalData
datatypes["vector"] = LavaVuPython.lucVectorData
datatypes["value"] = LavaVuPython.lucColourValueData
datatypes["opacity"] = LavaVuPython.lucOpacityValueData
datatypes["red"] = LavaVuPython.lucRedValueData
datatypes["green"] = LavaVuPython.lucGreenValueData
datatypes["blue"] = LavaVuPython.lucBlueValueData
datatypes["index"] = LavaVuPython.lucIndexData
datatypes["width"] = LavaVuPython.lucXWidthData
datatypes["height"] = LavaVuPython.lucYHeightData
datatypes["length"] = LavaVuPython.lucZLengthData
datatypes["rgba"] = LavaVuPython.lucRGBAData
datatypes["texcoord"] = LavaVuPython.lucTexCoordData
datatypes["size"] = LavaVuPython.lucSizeData
datatypes["luminance"] = LavaVuPython.lucLuminanceData
datatypes["rgb"] = datatypes["max"] = LavaVuPython.lucRGBData

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

#Wrapper class for drawing object
#handles property updating via internal dict
class Obj():
    def __init__(self, idict, instance, *args, **kwargs):
        self.dict = idict
        self.instance = instance
        self.name = str(self.dict["name"])

    def get(self):
        #Retrieve updated props
        props = json.loads(self.instance.app.getObject(self.name))
        self.dict.clear()
        self.dict.update(props)
        self.name = str(self.dict["name"])

    def set(self):
        #Send updated props
        self.instance.app.setObject(self.name, json.dumps(self.dict))
        self.get()

    def __getitem__(self, key):
        self.get()
        return self.dict[key]

    def __setitem__(self, key, value):
        self.get()
        #Set new value and send
        self.dict[key] = value
        self.set()

    def __contains__(self, key):
        return key in self.dict

    def __str__(self):
        self.get()
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
        self.set()
        return len(self["filters"])-1

    def data(self):
        #Return json data set list
        return json.dumps(self.dict["data"])

    def vertices(self, data):
        self.instance.app.parseCommands("select " + self.name)
        self.instance.app.loadVectors(data, LavaVuPython.lucVertexData)

    def vectors(self, data):
        self.instance.app.parseCommands("select " + self.name)
        self.instance.app.loadVectors(data, LavaVuPython.lucVectorData)

    def values(self, data, type=LavaVuPython.lucColourValueData, label="", min=0., max=0.):
        self.instance.app.parseCommands("select " + self.name)
        self.instance.app.loadScalars(data, type, label, min, max)

    def colours(self, data):
        self.instance.app.parseCommands("select " + self.name)
        self.instance.app.parseCommands("colours=" + str(data))
        self.instance.app.parseCommands("read colours")

    def indices(self, data):
        self.instance.app.parseCommands("select " + self.name)
        self.instance.app.loadUnsigned(data, LavaVuPython.lucIndexData)

    def labels(self, data):
        self.instance.app.parseCommands("select " + self.name)
        self.instance.app.labels(data)

    #Property control interface
    #...TODO...

#Wrapper dict+list of objects
class Objects(dict):
  def __init__(self, instance):
    self.instance = instance
    pass

  def update(self):
    self.list = []
    for obj in self.instance.state["objects"]:
        if obj["name"] in self:
            self[obj["name"]].get()
            self.list.append(self[obj["name"]])
        else:
            o = Obj(obj, self.instance)
            self[obj["name"]] = o
            self.list.append(o)
        #Save the id number
        self.list[-1].id = len(self.list)

  def __str__(self):
    return '[' + ', '.join(self.keys()) + ']'

class Viewer(object):
    app = None
    binary = ""
    res = (640,480)

    def __init__(self, reuse=False, binary="bin/LavaVu", *args, **kwargs):
        self.binary = binary
        try:
            #TODO: re-using instance causes multiple interactive viewer instances to die
            #ensure not doing this doesn't cause issues (will leak memory if many created)
            #Create a new instance, always if cache disabled (reuse)
            if not self.app or not reuse:
                self.app = LavaVuPython.LavaVu(binary)
            #    print "Launching LavaVu, instance: " + str(id(self.app))
            #else:
            #    print "Re-using LavaVu, instance: " + str(id(self.app))
            self.setup(*args, **kwargs)
        except RuntimeError,e:
            print "LavaVu Init error: " + str(e)
            pass
        #Copy control module ref
        self.control = control

    def setup(self, arglist=None, database=None, figure=None, timestep=None, 
         port=0, verbose=False, interactive=False, hidden=True, cache=False,
         quality=2, writeimage=False, res=None, script=None, initscript=False, usequeue=False):
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
        #Output resolution
        if res != None and isinstance(res,tuple):
          args += ["-x" + str(res[0]) + "," + str(res[1])]
          self.res = res
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
                self.get()
        except RuntimeError,e:
            print "LavaVu Run error: " + str(e)
            pass

    #dict methods
    def __getitem__(self, key):
        #Get view/global property
        #self.state = json.loads(self.getState())
        view = self.state["views"][0]
        if key in self.state:
            return self.state[key]
        if key in view:
            return view[key]
        else:
            return self.state["properties"][key]
        return None

    def __setitem__(self, key, item):
        #Set view/global property
        #self.app.parseCommands("select") #Ensure no object selected
        #self.app.parseCommands(key + '=' + str(item))
        #self.get()
        self.state = json.loads(self.app.getState())
        view = self.state["views"][0]
        if key in self.state:
            self.state[key] = item
        if key in view:
            view[key] = item
        else:
            self.state["properties"][key] = item
        self.set()

    def __contains__(self, key):
        return key in self.state or key in self.state["properties"] or key in self.state["views"][0]

    def __str__(self):
        #View/global props to string
        self.get()
        self.properties = self.state["properties"]
        self.properties.update(self.state["views"][0])
        return str('\n'.join(['    %s=%s' % (k,json.dumps(v)) for k,v in self.properties.iteritems()]))

    def get(self):
        #Import state from lavavu
        self.state = json.loads(self.app.getState())
        if not isinstance(self.objects, Objects):
            self.objects = Objects(self)
        self.objects.update()

    def set(self):
        #Export state to lavavu
        #(include current object list state)
        #self.state["objects"] = [obj.dict for obj in self.objects.list]
        self.app.setState(json.dumps(self.state))

    def commands(self, cmds):
        if isinstance(cmds, list):
            cmds = '\n'.join(cmds)
        if self.queue: #Thread safe queue requested
            self.app.queueCommands(cmds)
        else:
            self.app.parseCommands(cmds)
        #Always sync the state after running commands
        self.get()

    #Callable with commands...
    def __call__(self, cmds):
        self.commands(cmds)

    #Undefined methods supported directly as LavaVu commands
    def __getattr__(self, key):
        #__getattr__ called if no attrib/method found
        def any_method(*args, **kwargs):
            #If member function exists on LavaVu, call it
            has = hasattr(self.app, key)
            method = getattr(self.app, key, None)
            if method and callable(method):
                return method(*args, **kwargs)
            #Otherwise, pass args as command string
            argstr = key
            for arg in args:
                argstr += " " + str(arg)
            self.commands(argstr)
        return any_method

    def add(self, name, properties={}):
        if not self.app.amodel:
            self.defaultModel()

        if isinstance(self.objects, Objects) and name in self.objects:
            print "Object exists: " + name
            return self.objects[name]

        #Adds a new object
        self.app.addObject(name, json.dumps(properties))

        #Get state and update object list
        self.get()

        #Read any property data sets (allows object creation and load with single prop dict)
        for key in properties:
            if key in ["vertices", "normals", "vectors", "colours", "values"]:
                self.app.parseCommands("read " + key)

        #Return wrapper obj
        return self.objects.list[-1]

    def file(self, filename, name=None, properties={}):
        if not self.app.amodel:
            self.app.defaultModel()

        if isinstance(self.objects, Objects) and name in self.objects:
            print "Object exists: " + name
            return self.objects[name]

        #Load a new object from file
        #self.app.loadFile(filename)
        self.app.parseCommands("file " + filename)
        if name:
            properties["name"] = name

        #Get state and update object list
        self.get()

        #Update properties and return wrapper object
        if len(self.objects.list) == 0:
            print "WARNING: No object created after loading: " + filename
            return None
        obj = self.objects.list[-1] #Use last in list, most recently added
        self.app.setObject(obj.name, json.dumps(properties))
        return obj
    
    def files(self, filespec, name=None, properties={}):
        #Load list of files with glob
        filelist = glob.glob(filespec)
        obj = None
        for infile in sorted(filelist):
            obj = self.file(infile)
        return obj

    def clear(self):
        self.close()

    def timesteps(self):
        return json.loads(self.app.getTimeSteps())

    def addstep(self):
        return self.app.parseCommands("newstep")

    def frame(self, resolution=None):
        #self.set() #Sync state first?
        #Jpeg encoded frame data
        if not resolution: resolution = self.res
        return self.app.image("", resolution[0], resolution[1], True);

    def display(self, resolution=(640,480)):
        """        
        Shows the generated image inline within an ipython notebook.
        
        If IPython is installed, displays the result image content inline

        If IPython is not installed, this method will call the default image 
        output routines to save the result with a default filename in the current directory

        """
        #Sync state first
        self.set()

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
                from IPython.display import display,Image,HTML
                #Write files to disk first, can be passed directly on url but is slow for large datasets
                filename = "input.json"
                text_file = open("html/" + filename, "w")
                text_file.write(self.web());
                text_file.close()
                from IPython.display import IFrame
                display(IFrame("html/index.html#" + filename, width=resolution[0], height=resolution[1]))
        except NameError, ImportError:
            self.app.web(True)
        except Exception,e:
            print "WebGL output error: " + str(e)
            pass

    def testimages(self, imagelist, tolerance=0.001, expectedPath='expected/', outputPath='./'):
        results = []
        for f in imagelist:
            outfile = outputPath+f
            expfile = expectedPath+f
            results.append(self.testimage(outfile, expfile, tolerance))
        #Combined result
        overallResult = all(results)
        if not overallResult:
            raise RuntimeError("Image tests failed due to one or more image comparisons above tolerance level!")
        print "-------------\nTests Passed!\n-------------"

    def testimage(self, outfile, expfile, tolerance=0.001):
        if len(expfile) and not os.path.exists(expfile):
            print "Test skipped, Reference image '%s' not found!" % expfile
            return 0
        if len(outfile) and not os.path.exists(outfile):
            raise RuntimeError("Generated image '%s' not found!" % outfile)

        diff = self.imageDiff(outfile, expfile)
        result = diff <= tolerance
        if not result:
            print "FAIL: %s Image comp errors %f, not"\
                  " within tolerance %g of reference image."\
                % (outfile, diff, tolerance)
        else:
            print "PASS: %s Image comp errors %f, within tolerance %f"\
                  " of ref image."\
                % (outfile, diff, tolerance)
        return result

    def clearimages(self, imagelist):
        try:
            for f in imagelist:
                os.remove(f)
        except:
            pass

    def serve(self):
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
        #Only allow a single interactive viewer instance
        if not isinstance(self.winid, int):
            self.winid = control.window(self)

    def redisplay(self):
        if isinstance(self.winid, int):
            control.redisplay(self.winid)

