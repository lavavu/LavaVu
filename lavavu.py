#LavaVu python interface: viewer utils & wrapper
import control
import json
import math
import sys
import os
import glob

#Attempt to import swig module
libpath = "bin"
try:
    #This file should be found one dir above bin dir containing built modules
    binpath = os.path.join(os.path.dirname(control.__file__), 'bin')
    sys.path.append(binpath)
    import LavaVuPython
    modpath = os.path.abspath(os.path.dirname(control.__file__))
    libpath = os.path.join(modpath, "bin")
except Exception,e:
    print "LavaVu visualisation module load failed: " + str(e)
    raise

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

#Wrapper class for drawing object
#handles property updating via internal dict
class Obj(object):
    def __init__(self, idict, instance, *args, **kwargs):
        self.dict = idict
        self.instance = instance

    def name(self):
        return str(self.dict["name"])

    def get(self):
        #Retrieve updated props
        props = json.loads(self.instance.app.getObject(self.name()))
        self.dict.clear()
        self.dict.update(props)

    def set(self):
        #Send updated props (via original name)
        self.instance.setupobject(**self.dict)
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
        self.instance.app.loadVectors(data, LavaVuPython.lucVertexData, self.name())

    def normals(self, data):
        self.instance.app.loadVectors(data, LavaVuPython.lucNormalData, self.name())

    def vectors(self, data):
        self.instance.app.loadVectors(data, LavaVuPython.lucVectorData, self.name())

    def values(self, data, label=""):
        self.instance.app.loadValues(data, label, self.name())

    def colours(self, data):
        #Convert to list of strings
        if isinstance(data, str):
            data = data.split()
        for i in range(len(data)):
            if not isinstance(data[i], str): data[i] = str(data[i])
        self.instance.app.loadColours(data, self.name())

    def indices(self, data):
        self.instance.app.loadUnsigned(data, LavaVuPython.lucIndexData, self.name())

    def labels(self, data):
        self.instance.app.labels(data)

    def colourmap(self, data, **kwargs):
        #Load colourmap and set property on this object
        cmap = self.instance.colourmap(self.name() + '-default', data, **kwargs)
        self["colourmap"] = cmap
        return cmap
    
    def file(self, *args, **kwargs):
        #Load file with this object selected (import)
        self.instance.select('"' + self.name() + '"')
        self.instance.file(*args, name=self.name(), **kwargs)
        self.instance.select()

    def colourbar(self, name=None, **kwargs):
        #Create a new colourbar for this object
        name = self.instance.app.colourBar(self.name())
        if len(name) == 0: return
        #Update list
        self.get()
        #Setups up new object, all other args passed to properties dict
        return self.instance.setupobject(name, **kwargs)

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
    resolution = (640,480)

    def __init__(self, reuse=False, binpath=libpath, *args, **kwargs):
        try:
            #TODO: re-using instance causes multiple interactive viewer instances to die
            #ensure not doing this doesn't cause issues (could leak memory if many created)
            #Create a new instance, always if cache disabled (reuse)
            if not self.app or not reuse:
                self.app = LavaVuPython.LavaVu(binpath)
            #    print "Launching LavaVu, instance: " + str(id(self.app))
            #else:
            #    print "Re-using LavaVu, instance: " + str(id(self.app))
            self.setup(*args, **kwargs)

            #Control setup, expect html files in same path as viewer binary
            control.htmlpath = os.path.join(binpath, "html")
            if not os.path.isdir(control.htmlpath):
                control.htmlpath = None
                print("Can't locate html dir, interactive view disabled")
        except RuntimeError,e:
            print "LavaVu Init error: " + str(e)
            pass
        #Copy control module ref
        self.control = control

    def setup(self, arglist=None, database=None, figure=None, timestep=None, 
         port=0, verbose=False, interactive=False, hidden=True, cache=False,
         quality=2, writeimage=False, resolution=None, script=None, initscript=False, usequeue=False):
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
            #Check for add object by geom type shortcut
            if key in ["labels", "points", "quads", "triangles", "vectors", "tracers", "lines", "shapes", "volume"]:
                #Allows calling add by geometry type, eg: obj = lavavu.lines()
                return self.addtype(key, *args, **kwargs)
            #Otherwise, pass args as command string
            argstr = key
            for arg in args:
                argstr += " " + str(arg)
            self.commands(argstr)
        return any_method

    def setupobject(self, name, **kwargs):
        #Strip data keys from kwargs and put aside for loading
        datasets = {}
        for key in kwargs.keys():
            if key in ["vertices", "normals", "vectors", "colours", "indices", "values", "labels"]:
                datasets[key] = kwargs.pop(key, None)

        #Call function to add/setup the object, all other args passed to properties dict
        self.app.setObject(str(name), str(json.dumps(kwargs)))

        #Get the created/update object
        obj = self.getobject(name)

        #Read any property data sets (allows object creation and load with single prop dict)
        for key in datasets:
            #Get the load function matching the data set (eg: vertices() ) and call on data
            func = getattr(obj, key)
            func(datasets[key])

        #Return wrapper obj
        return obj

    def add(self, name, **kwargs):
        if isinstance(self.objects, Objects) and name in self.objects:
            print "Object exists: " + name
            return self.objects[name]

        #Adds a new object, all other args passed to properties dict
        return self.setupobject(name, **kwargs)

    #Shortcut for adding specific geometry types
    def addtype(self, typename, name=None, **kwargs):
        #Set name to typename if none provided
        if not name: name = typename
        kwargs["geometry"] = typename
        return self.add(name, **kwargs)

    def getobject(self, identifier=None):
        #Return object by name/number or last in list if none provided
        #Get state and update object list
        self.get()
        if len(self.objects.list) == 0:
            print "WARNING: No objects"
            return None
        #If name passed, find this object in updated list, if not just use the last
        obj = None
        if isinstance(identifier, str):
            for obj in self.objects.list:
                if obj["name"] == identifier: break
                obj = None
        if isinstance(identifier, int):
            if len(self.objects.list) >= identifier:
                obj = self.objects.list[identifier-1]
        if not obj:
            obj = self.objects.list[-1]

        return obj

    def file(self, filename, name=None, **kwargs):
        #Load a new object from file
        self.app.loadFile(filename)

        #Get object
        obj = self.getobject(name)

        #Setups up new object, all other args passed to properties dict
        return self.setupobject(obj.name(), **kwargs)
    
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
        #self.set() #Sync state first?
        #Jpeg encoded frame data
        if not resolution: resolution = self.resolution
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

    def testimages(self, imagelist=None, tolerance=TOL_DEFAULT, expectedPath='expected/', outputPath='./', clear=False):
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
        if clear:
            try:
                for f in imagelist:
                    os.remove(f)
            except:
                pass
        if not overallResult:
            raise RuntimeError("Image tests failed due to one or more image comparisons above tolerance level!")
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
        #Interactive viewer instance
        control.window(self)

    def redisplay(self):
        if isinstance(self.winid, int):
            control.redisplay(self.winid)

