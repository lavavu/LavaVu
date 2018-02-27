"""
LavaVu python interface: viewer utils & wrapper

NOTE: regarding sync of state between python and library
- sync from python to lavavu is immediate,
    property setting must always trigger a sync to lavavu
- sync from lavavu to python is lazy, always need to call _get()
    before using state data
#TODO:
 - zoom to fit in automated image output broken, initial timestep differs, margins out
 - translation setting different if window aspect ratio changes
"""
import json
import math
import sys
import os
import glob
import control
import numpy
import re

if __name__ != 'glucifer.lavavu.lavavu' and 'glucifer.lavavu.lavavu' in sys.modules:
    #Already imported, some paths issue causes double import
    raise RuntimeError("LavaVu module provided by glucifer exists, please don't import separately")

def is_ipython():
    try:
        if __IPYTHON__:
            return True
        else:
            return False
    except:
        return False

def is_notebook():
    if 'IPython' not in sys.modules:
        # IPython hasn't been imported, definitely not
        return False
    try:
        from IPython import get_ipython
        from IPython.display import display,Image,HTML
    except:
        return False
    # check for `kernel` attribute on the IPython instance
    return getattr(get_ipython(), 'kernel', None) is not None

#import swig module
import LavaVuPython
libpath = os.path.abspath(os.path.dirname(__file__))
version = LavaVuPython.version

TOL_DEFAULT = 0.0001 #Default error tolerance for image tests

geomtypes = {"labels":    LavaVuPython.lucLabelType,
             "points":    LavaVuPython.lucPointType,
             "quads":     LavaVuPython.lucGridType,
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
    if (sys.version_info > (3, 0)):
        #Not necessary in python3...
        return dictionary
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

class CustomEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, numpy.integer):
            return int(obj)
        elif isinstance(obj, numpy.floating):
            return float(obj)
        elif isinstance(obj, numpy.ndarray):
            return obj.tolist()
        else:
            return super(CustomEncoder, self).default(obj)

def convert_args(dictionary):
    """Convert a kwargs dict to a json string argument list
       Ensure all elements can be converted by using custom encoder
    """
    return str(json.dumps(dictionary, cls=CustomEncoder))

def cubeHelix(samples=16, start=0.5, rot=-0.9, sat=1.0, gamma=1., alpha=None):
    """
    Create CubeHelix spectrum colourmap with monotonically increasing/descreasing intensity

    Implemented from FORTRAN 77 code from D.A. Green, 2011, BASI, 39, 289.
    "A colour scheme for the display of astronomical intensity images"
    http://adsabs.harvard.edu/abs/2011arXiv1108.5083G

    Parameters
    ----------
    samples: int
        Number of colour samples to produce
    start: float
        Start colour [0,3] 1=red,2=green,3=blue
    rot: float
        Rotations through spectrum, negative to reverse direction
    sat: float
        Colour saturation grayscale to full [0,1], >1 to oversaturate
    gamma: float
        Gamma correction [0,1]
    alpha: list,tuple
        Alpha [min,max] for transparency ramp

    Returns
    -------
    colours: list
        List of colours ready to be loaded by colourmap()
    """

    colours = []

    if not isinstance(alpha,list) and not isinstance(alpha,tuple):
        #Convert as boolean
        if alpha: alpha = [0,1]

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
        a = 1.0
        if alpha:
            a = alpha[0] + (alpha[1]-alpha[0]) * fract

        colours.append((fract, 'rgba(%d,%d,%d,%d)' % (r*0xff, g*0xff, b*0xff, a*0xff)))

    return colours


#Module settings
#must be an object or won't be referenced from __init__.py import
#(enures values are passed on when set externally)
settings = {}
#Default arguments for viewer creation
settings["default_args"] = []
#Dump base64 encoded images on test failure for debugging
settings["echo_fails"] = False

#Wrapper class for drawing object
#handles property updating via internal dict
class Object(dict):
    """  
    The Object class provides an interface to a LavaVu drawing object
    Object instances are created internally in the Viewer class and can
    be retrieved from the object list

    New objects are also created using viewer methods
    
    Parameters
    ----------
    **kwargs:
        Initial set of properties passed to the created object

    Example
    -------
        
    Create a viewer, load some test data, get objects
    
    >>> import lavavu
    >>> lv = lavavu.Viewer()
    >>> lv.test()
    >>> print(lv.objects)

    Object properties can be passed in when created or set by using as a dictionary:

    >>> obj = lv.points(pointtype="sphere")
    >>> obj["pointsize"] = 5

    A list of available properties can be found here: https://github.com/OKaluza/LavaVu/wiki/Property-Reference
    or by using the online help:

    >>> obj.help('opacity')

    """
    def __init__(self, instance, *args, **kwargs):
        self.dict = kwargs
        self.instance = instance
        if not "filters" in self.dict: self.dict["filters"] = []

        #Create a control factory
        self.control = control.ControlFactory(self)

        #Init prop dict for tab completion
        super(Object, self).__init__(**self.instance.properties)

    @property
    def name(self):
        """
        Get the object's name property

        Returns
        -------
        name: str
            The name of the object
        """
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
        if key in self.dict:
            return self.dict[key]
        if not key in self.instance.properties:
            raise ValueError(key + " : Invalid property name")
        #Default to the property lookup dict (default is first element)
        prop = super(Object, self).__getitem__(key)
        return prop[0]

    def __setitem__(self, key, value):
        if not key in self.instance.properties:
            raise ValueError(key + " : Invalid property name")
        if key == "colourmap" and isinstance(value, LavaVuPython.ColourMap) or isinstance(value, ColourMap):
            value = value.name #Use name instead of object when setting colourmap on object
        self.instance.app.parseProperty(key + '=' + str(value), self.ref)
        self.instance._get() #Ensure in sync

    def __contains__(self, key):
        return key in self.dict

    def __repr__(self):
        return str(self.ref)

    def __str__(self):
        #Default string representation
        self.instance._get() #Ensure in sync
        return str('\n'.join(['%s=%s' % (k,json.dumps(v)) for k,v in self.dict.items()]))
        #return '[' + ', '.join(self.dict.keys()) + ']'

    #Interface for setting filters
    def include(self, *args, **kwargs):
        """
        Filter data by including a range of values
        shortcut for:
            filter(... , out=False)

        Parameters
        ----------
        label: str
            Data label to filter on
        values: number,list,tuple
            value range single value, list or tuple
            if a single value the filter applies to only this value: x == value
            if a list  eg: [0,1] range is inclusive 0 <= x <= 1
            if a tuple eg: (0,1) range is exclusive 0 < x < 1

        Returns
        -------
        filter: int
            The filter id created
        """
        return self.filter(*args, out=False, **kwargs)

    def includemap(self, *args, **kwargs):
        """
        Filter data by including a range of mapped values
        shortcut for:
            filter(... , out=False, map=True)

        Parameters
        ----------
        label: str
            Data label to filter on
        values: number,list,tuple
            value range single value, list or tuple
            if a single value the filter applies to only this value: x == value
            if a list  eg: [0,1] range is inclusive 0 <= x <= 1
            if a tuple eg: (0,1) range is exclusive 0 < x < 1

        Returns
        -------
        filter: int
            The filter id created
        """
        return self.filter(*args, out=False, map=True, **kwargs)

    def exclude(self, *args, **kwargs):
        """
        Filter data by excluding a range of values
        shortcut for:
            filter(... , out=True)

        Parameters
        ----------
        label: str
            Data label to filter on
        values: number,list,tuple
            value range single value, list or tuple
            if a single value the filter applies to only this value: x == value
            if a list  eg: [0,1] range is inclusive 0 <= x <= 1
            if a tuple eg: (0,1) range is exclusive 0 < x < 1

        Returns
        -------
        filter: int
            The filter id created
        """
        return self.filter(*args, out=True, **kwargs)
            
    def excludemap(self, *args, **kwargs):
        """
        Filter data by excluding a range of mapped values
        shortcut for:
            filter(... , out=True, map=True)

        Parameters
        ----------
        label: str
            Data label to filter on
        values: number,list,tuple
            value range single value, list or tuple
            if a single value the filter applies to only this value: x == value
            if a list  eg: [0,1] range is inclusive 0 <= x <= 1
            if a tuple eg: (0,1) range is exclusive 0 < x < 1

        Returns
        -------
        filter: int
            The filter id created
        """
        return self.filter(*args, out=True, map=True, **kwargs)

    def filter(self, label, values, out=False, map=False):
        """
        Filter data by including a range of values

        Parameters
        ----------
        label: str
            Data label to filter on
        values: number,list,tuple
            value range single value, list or tuple
            if a single value the filter applies to only this value: x == value
            if a list  eg: [0,1] range is inclusive 0 <= x <= 1
            if a tuple eg: (0,1) range is exclusive 0 < x < 1
        out: boolean
            Set this flag to filter out values instead of including them
        map: boolean
            Set this flag to filter by normalised values mapped to [0,1]
            instead of actual min/max of the data range

        Returns
        -------
        filter: int
            The filter id created
        """
        #Pass a single value to include/exclude exact value
        #Pass a tuple for exclusive range (min < val < max)
        # list for inclusive range (min <= val <= max)
        self.instance._get() #Ensure have latest data
        filterlist = []
        if "filters" in self:
            filterlist = self["filters"]

        if isinstance(values, float) or isinstance(values, int):
            values = [values,values]
        newfilter = {"by" : label, "minimum" : values[0], "maximum" : values[1], "map" : map, "out" : out, "inclusive" : False}
        if isinstance(values, list) or values[0] == values[1]:
            newfilter["inclusive"] = True

        filterlist.append(newfilter)

        self.instance.app.parseProperty('filters=' + json.dumps(filterlist), self.ref)
        self.instance._get() #Ensure in sync
        return len(self["filters"])-1

    def datasets(self):
        """
        Retrieve available data sets on this object

        Returns
        -------
        data: str
            A string representation of the data objects available
        """
        #Return json data set list
        #TODO: use Geometry wrapper?
        return json.dumps(self.dict["data"])

    def append(self):
        """
        Append a new data element to the object

        Object data is sometimes dependant on individual elements to
        determine where one part ends and another starts, eg: line segments, grids

        This allows manually closing the active element so all new data is loaded into a new element
        """
        self.instance.app.appendToObject(self.ref)

    def triangles(self, data, split=0):
        """
        Load triangle data,

        This is the same as loading vertices into a triangle mesh object but allows
        decomposing the mesh into smaller triangles with the split option

        Parameters
        ----------
        split: int
            Split triangles this many times on loading
        """
        if split > 1:
            self.instance.app.loadTriangles(self.ref, data, self.name, split)
        else:
            self.vertices(data)

    def _convert(self, data, dtype=None):
        #Prepare a data set
        if not isinstance(data, numpy.ndarray):
            #Convert to numpy array first
            data = numpy.asarray(data)
        #Transform to requested data type if provided
        if dtype != None and data.dtype != dtype:
            data = data.astype(dtype)

        #Always convert float64 to float32
        if data.dtype == numpy.float64:
            data = data.astype(numpy.float32)

        #Masked array? Set fill value to NaN
        if numpy.ma.is_masked(data):
            if data.fill_value != numpy.nan:
                print("Warning: setting masked array fill to NaN, was: ", data.fill_value)
                numpy.ma.set_fill_value(data, numpy.nan)
            data = data.filled()

        return data

    def _loadScalar(self, data, geomdtype):
        #Passes a scalar dataset (float/uint8/uint32)
        data = self._convert(data)
        #Load as flattened 1d array
        #(ravel() returns view rather than copy if possible, flatten() always copies)
        if data.dtype == numpy.float32:
            self.instance.app.arrayFloat(self.ref, data.ravel(), geomdtype)
        elif data.dtype == numpy.uint32:
            self.instance.app.arrayUInt(self.ref, data.ravel(), geomdtype)
        elif data.dtype == numpy.uint8:
            self.instance.app.arrayUChar(self.ref, data.ravel(), geomdtype)

    def _loadVector(self, data, geomdtype, magnitude=None):
        #Passes a vector dataset (float)
        data = self._convert(data, numpy.float32)

        #Reshape vector data if first dim is 3 and last dim is not 3
        shape = data.shape
        #Data provided as separate x,y,z columns?
        if len(shape) >= 2 and shape[-1] != 3 and shape[0] == 3:
            #Re-arrange to array of [x,y,z] triples
            data = numpy.vstack((data[0],data[1],data[2])).reshape([3, -1]).transpose()

        if magnitude is not None:
            axis = len(data.shape)-1
            mag = numpy.linalg.norm(data,axis=axis)
            if isinstance(magnitude, str):
                label = magnitude
            else:
                label = "magnitude"
            self.instance.app.arrayFloat(self.ref, mag.ravel(), label)

        #Load as flattened 1d array
        #(ravel() returns view rather than copy if possible, flatten() always copies)
        self.instance.app.arrayFloat(self.ref, data.ravel(), geomdtype)

    def data(self, filter=None):
        """
        Return internal geometry data
        Returns a Geometry() object that can be iterated through containing all data elements
        Elements contain vertex/normal/value etc. data as numpy arrays

        Parameters
        ----------
        filter: str
            Limit the data returned to this type
            (labels, points, grid, triangles, vectors, tracers, lines, shapes, volume)

        Returns
        -------
        data: Geometry
            An object holding the data elements retrieved

        Example
        -------
        >>> data = obj.data()
        >>> for el in obj.data:
        >>>     print(el)
        """
        return Geometry(self, filter)

    def vertices(self, data=None):
        """
        Load 3d vertex data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy float32 3d array of vertices
        """
        self._loadVector(data, LavaVuPython.lucVertexData)

    def normals(self, data):
        """
        Load 3d normal data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy float32 3d array of normals
        """
        self._loadVector(data, LavaVuPython.lucNormalData)

    def vectors(self, data, magnitude=False):
        """
        Load 3d vector data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy float32 3d array of vectors
        magnitude: str
            Pass a label to calculate the magnitude and save under provided name (for use in colouring etc)
        """
        self._loadVector(data, LavaVuPython.lucVectorData, magnitude)

    def values(self, data, label="default"):
        """
        Load value data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy float32 array of values
        label: str
            Label for this data set
        """
        data = self._convert(data, numpy.float32)
        self.instance.app.arrayFloat(self.ref, data.ravel(), label)

    def colours(self, data):
        """
        Load colour data for object

        Parameters
        ----------
        data: str,list,array
            Pass a list or numpy uint32 array of colours
            if a string or list of strings is provided, colours are parsed as html colour string values
            if a numpy array is passed, colours are loaded as 4 byte ARGB unsigned integer values
        """
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

    def indices(self, data, offset=0):
        """
        Load index data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy uint32 array of indices
            indices are loaded as 32 bit unsigned integer values
        offset: integer
            Specify an initial index offset, for 1-based indices pass offset=1
            Default is zero-based
        """

        #Accepts only uint32 indices
        data = self._convert(data, numpy.uint32)
        if offset > 0:
            #Convert indices to offset 0 before loading by subtracting offset
            data = numpy.subtract(data, offset)
        #Load indices
        self._loadScalar(data, LavaVuPython.lucIndexData)

    def rgb(self, data):
        """
        Load rgb data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy uint8 array of rgb values
            values are loaded as 8 bit unsigned integer values
        """

        #Accepts only uint8 rgb triples
        data = self._convert(data, numpy.uint8)
        self._loadScalar(data, LavaVuPython.lucRGBData)

    def texture(self, data, width, height, channels=4, flip=True, mipmaps=True, bgr=False):
        """
        Load raw texture data for object

        Parameters
        ----------
        data: list,array
            Pass a list or numpy uint32 or uint8 array
            texture data is loaded as raw image data
        width: int
            image width in pixels
        height: int
            image height in pixels
        channels: int
            colour channels/depth in bytes (1=luminance, 3=RGB, 4=RGBA)
        flip: boolean
            flip the texture vertically after loading
            (default is enabled as usually required for OpenGL but can be disabled)
        mipmaps: boolean
            generate mipmaps (slow)
        bgr: boolean
            rgb data is in BGR/BGRA format instead of RGB/RGBA
        """
        if not isinstance(data, numpy.ndarray):
            data = self._convert(data, numpy.uint32)
        if data.dtype == numpy.uint32:
            self.instance.app.textureUInt(self.ref, data.ravel(), width, height, channels, flip, mipmaps, bgr)
        elif data.dtype == numpy.uint8:
            self.instance.app.textureUChar(self.ref, data.ravel(), width, height, channels, flip, mipmaps, bgr)

    def labels(self, data):
        """
        Load label data for object

        Parameters
        ----------
        data: list,str
            Pass a label or list of labels to be applied, one per vertex
        """
        if isinstance(data, str):
            data = [data]
        self.instance.app.loadLabels(self.ref, data)

    def colourmap(self, data=None, reverse=False, monochrome=False, **kwargs):
        """
        Load colour map data for object

        Parameters
        ----------
        data: list,str
            If not provided, just returns the colourmap
            (A default is created if none exists)
            Provided colourmap data can be
            - a string,
            - list of colour strings,
            - list of position,value tuples
            - or a built in colourmap name
            Creates a colourmap named objectname_colourmap if object
            doesn't already have a colourmap
        reverse: boolean
            Reverse the order of the colours after loading
        monochrome: boolean
            Convert to greyscale

        Returns
        -------
        colourmap: ColourMap(dict)
            The wrapper object of the colourmap loaded/created
        """
        if data is None:
            cmid = self["colourmap"]
            if cmid:
                #Just return the existing map
                return ColourMap(cmid, self.instance)
            else:
                #Proceeed to create a new map with default data
                data = cubeHelix()

        #Load colourmap on this object
        ret = None
        if self.ref.colourMap is None:
            self.ref.colourMap = self.instance.app.addColourMap(self.name + "_colourmap")
            self["colourmap"] = self.ref.colourMap.name
        c = ColourMap(self.ref.colourMap, self.instance)
        c.update(data, reverse, monochrome, **kwargs)
        return c
        
    def reload(self):
        """
        Fully reload the object's data, recalculating all parameters such as colours
        that may be cached on the GPU, required after changing some properties
        so the changes are reflected in the visualisation
        """
        self.instance.app.reloadObject(self.ref)

    def select(self):
        """
        Set this object as the selected object
        """
        self.instance.app.aobject = self.ref
    
    def file(self, *args, **kwargs):
        """
        Load data from a file into this object

        Parameters
        ----------
        filename: str
            Name of the file to load
        """
        #Load file with this object selected (import)
        self.select()
        self.instance.file(*args, obj=self, **kwargs)
        self.instance.app.aobject = None

    def files(self, *args, **kwargs):
        """
        Load data from a series of files into this object (using wildcards or a list)

        Parameters
        ----------
        files: str
            Specification of the files to load
        """
        #Load file with this object selected (import)
        self.select()
        self.instance.files(*args, obj=self, **kwargs)
        self.instance.app.aobject = None

    def colourbar(self, **kwargs):
        """
        Create a new colourbar using this object's colourmap

        Returns
        -------
        colourbar: Object
            The colourbar object created
        """
        #Create a new colourbar for this object
        return self.instance.colourbar(self, **kwargs)

    def clear(self):
        """
        Clear all visualisation data from this object
        """
        self.instance.app.clearObject(self.ref)

    def cleardata(self, typename=""):
        """
        Clear specific visualisation data/values from this object

        Parameters
        ----------
        typename: str
            Optional filter naming type of data to be cleared,
            Either a built in type:
              (vertices/normals/vectors/indices/colours/texcoords/luminance/rgb/values)
            or a user defined data label
        """
        if typename in datatypes:
            #Found data type name
            dtype = datatypes[typename]
            self.instance.app.clearData(self.ref, dtype)
        else:
            #Assume values by label (or all values if blank)
            self.instance.app.clearValues(self.ref, typename)

    def update(self, filter=None, compress=True):
        """
        Write the objects's visualisation data back to the database

        Parameters
        ----------
        filter: str
            Optional filter to type of geometry to be updated, if omitted all will be written
            (labels, points, grid, triangles, vectors, tracers, lines, shapes, volume)
        compress: boolean
            Use zlib compression when writing the geometry data
        """
        #Update object data at current timestep
        if filter is None:
            #Re-writes all data to db for this object
            self.instance.app.update(self.ref, compress)
        else:
            #Re-writes data to db for this object and geom type
            self.instance.app.update(self.ref, geomtypes[filter], compress)

    def getcolourmap(self, string=True):
        """
        Return the colour map data from the object as a string or list
        Either return format can be used to create/modify a colourmap
        with colourmap()

        Parameters
        ----------
        string: boolean
            The default is to return the data as a string of colours separated by semi-colons
            To instead return a list of (position,[R,G,B,A]) tuples for easier automated processing in python,
            set this to False

        Returns
        -------
        mapdata: str/list
            The formatted colourmap data
        """
        cmid = self["colourmap"]
        return self.instance.getcolourmap(cmid, string)

    def isosurface(self, name=None, convert=False, updatedb=False, compress=True, **kwargs):
        """
        Generate an isosurface from a volume data set using the marching cubes algorithm

        Parameters
        ----------
        name: str
            Name of the created object, automatically assigned if not provided
        convert: bool
            Setting this flag to True will replace the existing volume object with the
            newly created isosurface by deleting the volume data and loading the mesh
            data into the preexisting object
        updatedb: bool
            Setting this flag to True will write the newly created/modified data
            to the database when done
        compress: boolean
            Use zlib compression when writing the geometry data
        **kwargs:
            Initial set of properties passed to the created object
            (Must include "isovalues=[]")

        Returns
        -------
        obj: Object
            The isosurface object created/converted
        """
        #Generate and return an isosurface object, 
        #pass properties as kwargs (eg: isovalues=[])
        isobj = self
        if not convert:
            #Create a new object for the surface
            if name is None: name = self.name + "_surface"
            isobj = self.instance.add(name, **kwargs)
            isobj["geometry"] = "triangles"
        else:
            #Convert existing object (self) set properties 
            self.instance._setupobject(self.ref, **kwargs)
        #Create surface, If requested, write the new data to the database
        self.instance.app.isoSurface(isobj.ref, self.ref, convert)
        #Re-write modified types to the database
        if updatedb:
            self.instance.app.update(isobj.ref, LavaVuPython.lucVolumeType, compress)
            self.instance.app.update(isobj.ref, LavaVuPython.lucTriangleType, compress)
        return isobj

    def help(self, cmd=""):
        """
        Get help on an object method or property

        Parameters
        ----------
        cmd: str
            Command to get help with, if ommitted displays general introductory help
            If cmd is a property or is preceded with '@' will display property help
        """
        self.instance.help(cmd, self)

#Wrapper dict+list of objects
class Objects(dict):
    """  
    The Objects class is used internally to manage and synchronise the drawing object list
    """
    def __init__(self, instance):
        self.instance = instance

    def _sync(self):
        #Sync the object list with the viewer
        self.list = []
        #Loop through retrieved object list
        for obj in self.instance.state["objects"]:
            #Exists in our own list?
            if obj["name"] in self:
                #Update object with new properties
                self[obj["name"]]._setprops(obj)
                self.list.append(self[obj["name"]])
            else:
                #Create a new object wrapper
                o = Object(self.instance, **obj)
                self[obj["name"]] = o
                self.list.append(o)
            #Flag sync
            self[obj["name"]].found = True
            #Save the object id and reference (use id # to get)
            _id = len(self.list)
            self.list[-1].id = _id
            self.list[-1].ref = self.instance.app.getObject(_id)
            
        #Delete any objects from stored dict that are no longer present
        for name in list(self):
            if not self[name].found:
                del self[name]
            else:
                self[name].found = False

    def __repr__(self):
        rep = '{\n'
        for key in self.keys():
            rep += '"' + key + '": {},'
        rep += '}\n'

    def __str__(self):
        #Default string representation is a comma separated list
        return ', '.join(self.keys())

class _ColourComponents():
    """Class to allow modifying colour components directly as an array
    """
    def __init__(self, key, parent):
        self.parent = parent
        self.key = key
        self.list = self.parent.list[self.key][1]

    def __getitem__(self, key):
        self.list = self.parent.list[self.key][1]
        return self.list[key]

    def __setitem__(self, key, value):
        self.list = self.parent.list[self.key][1]
        self.list[key] = value
        self.parent[self.key] = self.list

    def __str__(self):
        return str(self.list)

class _ColourList():
    """Class to allow modifying colour list directly as an array
    """
    def __init__(self, parent):
        self.parent = parent
        self.list = self.parent.tolist()

    def __getitem__(self, key):
        self.parent._get() #Ensure in sync
        self.list = self.parent.tolist()
        return _ColourComponents(key, self)

    def __setitem__(self, key, value):
        self.list = self.parent.tolist()
        self.list[key] = (self.list[key][0], value)
        self.parent.update(self.list)

    def __delitem__(self, key):
        self.list = self.parent.tolist()
        del self.list[key]
        self.parent.update(self.list)

    def __iadd__(self, value):
        self.append(value)

    def __add__(self, value):
        self.append(value)

    def append(self, value, position=1.0):
        self.list = self.parent.tolist()
        if isinstance(value, tuple):
            self.list.append(value)
        else:
            self.list.append((position, value))
        self.parent.update(self.list)

    def __str__(self):
        return str([c[1] for c in self.list])

class _PositionList(_ColourList):
    """Class to allow modifying position list directly as an array
    """
    def __init__(self, parent):
        self.parent = parent
        self.list = self.parent.tolist()

    def __getitem__(self, key):
        self.parent._get() #Ensure in sync
        self.list = self.parent.tolist()
        return self.list[key][0]

    def __setitem__(self, key, value):
        self.list = self.parent.tolist()
        self.list[key] = (value, self.list[key][1])
        self.parent.update(self.list)

    def __str__(self):
        return str([c[0] for c in self.list])

#Wrapper class for colourmap
#handles property updating via internal dict
class ColourMap(dict):
    """
    The ColourMap class provides an interface to a LavaVu ColourMap
    ColourMap instances are created internally in the Viewer class and can
    be retrieved from the colourmaps list

    New colourmaps are also created using viewer methods

    Parameters
    ----------
    **kwargs:
        Initial set of properties passed to the created colourmap

    """
    def __init__(self, ref, instance, *args, **kwargs):
        self.instance = instance
        if isinstance(ref, LavaVuPython.ColourMap):
            self.ref = ref
        else:
            self.ref = self.instance.app.getColourMap(ref)

        self.dict = kwargs
        self._get() #Sync

        #Init prop dict for tab completion
        super(ColourMap, self).__init__(**self.instance.properties)

    @property
    def name(self):
        """
        Get the colourmap name property

        Returns
        -------
        name: str
            The name of the colourmap
        """
        return self.ref.name

    @property
    def colours(self):
        """
        Returns the list of colours

        Returns
        -------
        colours: (list)
            A list of colours
         """
        self._get()
        return _ColourList(self)

    @colours.setter
    def colours(self, value):
        #Dummy setter to allow +/+= on colours list
        pass

    @property
    def positions(self):
        """
        Returns the list of colour positions

        Returns
        -------
        positions: (list)
            A list of colour positions [0,1]
         """
        self._get()
        return _PositionList(self)

    def _set(self):
        #Send updated props (via ref in case name changed)
        self.instance.app.setColourMap(self.ref, convert_args(self.dict))

    def _get(self):
        self.instance._get() #Ensure in sync
        #Update prop dict
        for cm in self.instance.state["colourmaps"]:
            if cm["name"] == self.ref.name:
                #self.colours = cm["colours"]
                self.dict.update(cm)
                #self.dict.pop("colours", None)
                #print self.dict

    def __getitem__(self, key):
        self._get() #Ensure in sync
        if not key in self.instance.properties:
            raise ValueError(key + " : Invalid property name")
        if key in self.dict:
            return self.dict[key]
        #Default to the property lookup dict (default is first element)
        prop = super(ColourMap, self).__getitem__(key)
        return prop[0]

    def __setitem__(self, key, value):
        if not key in self.instance.properties:
            raise ValueError(key + " : Invalid property name")
        #Set new value and send
        self.dict[key] = value
        self._set()

    def __contains__(self, key):
        return key in self.dict

    def tolist(self):
        """
        Get the colourmap data as a python list

        Returns
        -------
        list:
            The colour and position data which can be used to re-create the colourmap
        """
        arr = []
        for c in self["colours"]:
            comp = re.findall(r"[\d\.]+", c["colour"])
            comp = [int(comp[0]), int(comp[1]), int(comp[2]), int(255*float(comp[3]))]
            arr.append((c["position"], comp))
        return arr

    def __repr__(self):
        return self.__str__().replace(';', '\n')

    def __str__(self):
        #Default string representation
        cmstr = '"""'
        for c in self["colours"]:
            cmstr += "%6.4f=%s; " % (c["position"],c["colour"])
        cmstr += '"""\n'
        return cmstr

    def update(self, data, reverse=False, monochrome=False, **kwargs):
        """
        Update the colour map data

        Parameters
        ----------
        data: list,str
            Provided colourmap data can be
            - a string,
            - list of colour strings,
            - list of position,value tuples
            - or a built in colourmap name
        reverse: boolean
            Reverse the order of the colours after loading
        monochrome: boolean
            Convert to greyscale
        """
        if isinstance(data, list) and len(data) > 1:
            #Sort by position
            data = sorted(data, key=lambda tup: tup[0])
            #Ensure first and last positions of list data are always 0 and 1
            if data[0][0]  != 0.0: data[0]  = (0.0, data[0][1])
            if data[-1][0] != 1.0: data[-1] = (1.0, data[-1][1])
        if not isinstance(data, str):
            #Convert iterable maps to string format
            data = ['='.join([str(i) for i in item]) if not isinstance(item, str) else str(item) for item in data]
            data = '\n'.join(data)
        #Load colourmap data
        self.instance.app.updateColourMap(self.ref, data, convert_args(kwargs))
        if reverse:
            self.ref.flip()
        if monochrome:
            self.ref.monochrome()
        self._get() #Ensure in sync

class Fig(dict):
    """  
    The Fig class wraps a figure
    """
    def __init__(self, instance, name):
        self.instance = instance
        self.name = name
        #Init prop dict for tab completion
        super(Fig, self).__init__(**self.instance.properties)

    def __getitem__(self, key):
        if not key in self.instance.properties:
            raise ValueError(key + " : Invalid property name")
        #Activate this figure on viewer
        self.load()
        #Return key on viewer instance
        if key in self.instance:
            return self.instance[key]
        #Default to the property lookup dict (default is first element)
        prop = super(Fig, self).__getitem__(key)
        return prop[0]

    def __setitem__(self, key, value):
        if not key in self.instance.properties:
            raise ValueError(key + " : Invalid property name")
        #Activate this figure on viewer
        self.load()
        #Set new value
        self.instance[key] = value
        #Save changes
        self.save()

    def __repr__(self):
        return '{"' + self.name + '"}'

    def __str__(self):
        return self.name

    def load(self):
        #Activate this figure on viewer
        self.instance.figure(self.name)

    def save(self):
        #Save changes
        self.instance.savefigure(self.name)

    def show(self, *args, **kwargs):
        #Activate this figure on viewer
        self.load()
        #Render
        self.instance.display(*args, **kwargs)

    def image(self, *args, **kwargs):
        #Activate this figure on viewer
        self.load()
        #Render
        return self.instance.image(*args, **kwargs)


class Viewer(dict):
    """  
    *The Viewer class provides an interface to a LavaVu session*
    
    Parameters
    ----------
    arglist: list
        list of additional init arguments to pass
    database: str
        initial database (or model) file to load
    figure: int
        initial figure id to display
    timestep: int
        initial timestep to display
    port: int
        web server port
    verbose: boolean
        verbose output to command line for debugging
    interactive: boolean
        begin in interactive mode, opens gui window and passes control to event loop immediately
    hidden: boolean
        begin hidden, for offscreen rendering or web browser control
    cache: boolean
        cache all model timesteps in loaded database, everything loaded into memory on startup
        (assumes enough memory is available)
    quality: integer
        Render sampling quality, render 2^N times larger image and downsample output
        For anti-aliasing image rendering where GPU multisample anti-aliasing is not available
    writeimage: boolean
        Write images and quit, create images for all figures/timesteps in loaded database then exit
    resolution: list, tuple
        Window/image resolution in pixels [x,y]
    script: list
        List of script commands to run after initialising
    initscript: boolean
        Set to False to disable loading any "init.script" file found in current directory
    usequeue: boolean
        Set to True to add all commands to a background queue for processing rather than
        immediate execution
    **kwargs:
        Remaining keyword args will be  passed to the created viewer
        and parsed into the initial set of global properties
            
    Example
    -------
        
    Create a viewer, setting the initial background colour to white
    
    >>> import lavavu
    >>> lv = lavavu.Viewer(background="white")

    Objects can be added by loading files:

    >>> obj = lv.file('model.obj')

    Or creating empty objects and loading data:

    >>> obj = lv.points('mypoints')
    >>> obj.vertices([0,0,0], [1,1,1])

    Viewer commands can be called as methods on the viewer object:

    >>> lv.rotate('x', 45)

    Viewer properties can be set by using it like a dictionary:

    >>> lv["background"] = "grey50"

    A list of available commands and properties can be found in the wiki:
    https://github.com/OKaluza/LavaVu/wiki/Scripting-Commands-Reference
    https://github.com/OKaluza/LavaVu/wiki/Property-Reference

    or by using the online help:

    >>> lv.help('rotate')
    >>> lv.help('opacity')

    """

    def __init__(self, arglist=None, database=None, figure=None, timestep=None, 
         port=0, verbose=False, interactive=False, hidden=True, cache=False,
         quality=2, writeimage=False, resolution=None, script=None, initscript=False, usequeue=False,
         binpath=libpath, omegalib=False, *args, **kwargs):
        """
        Create and init viewer instance

        Parameters
        ----------
        (see Viewer class docs for setup args)
        binpath: str
            Override the executable path
        omegalib: boolean
            For use in VR mode, disables some conflicting functionality
            and parsed into the initial set of global properties
        """
        self.resolution = (640,480)
        self._ctr = 0
        self.app = None
        self._objects = Objects(self)
        self.state = {}
        try:
            self.app = LavaVuPython.LavaVu(binpath, omegalib)

            #Get property dict
            self.properties = convert_keys(json.loads(self.app.propertyList()))
            #Init prop dict for tab completion
            super(Viewer, self).__init__(**self.properties)

            self.setup(arglist, database, figure, timestep, port, verbose, interactive, hidden, cache,
                       quality, writeimage, resolution, script, initscript, usequeue, *args, **kwargs)

            #Control setup, expect html files in same path as viewer binary
            control.htmlpath = os.path.join(binpath, "html")
            if not os.path.isdir(control.htmlpath):
                control.htmlpath = None
                print("Can't locate html dir, interactive view disabled")

            #Create a control factory
            self.control = control.ControlFactory(self)

            #Get available commands
            self._cmdcategories = self.app.commandList()
            self._cmds = {}
            self._allcmds = []
            for c in self._cmdcategories:
                self._cmds[c] = self.app.commandList(c)
                self._allcmds += self._cmds[c]

            #Create command methods
            selfdir = dir(self)
            for key in self._allcmds:
                #Check if a method exists already
                if key in selfdir:
                    existing = getattr(self, key, None)
                    if existing:
                        #Add the lavavu doc entry to the docstring
                        doc = ""
                        if existing.__doc__:
                            if "Wraps LavaVu" in existing.__doc__: continue #Already modified
                            doc += existing.__doc__ + '\n----\nWraps LavaVu script command of the same name:\n > **' + key + '**:\n'
                        doc += self.app.helpCommand(key, False)
                        #These should all be wrapper for the matching lavavu commands
                        #(Need to ensure we don't add methods that clash)
                        existing.__func__.__doc__ = doc
                else:
                    #Use a closure to define a new method that runs this command
                    def cmdmethod(name):
                        def method(*args, **kwargs):
                            args = [name] + [str(a) for a in args]
                            self.commands(' '.join(args))
                        return method

                    #Create method that runs this command:
                    method = cmdmethod(key)

                    #Set docstring
                    method.__doc__ = self.app.helpCommand(key, False)
                    #Add the new method
                    self.__setattr__(key, method)

            #Add object by geom type shortcut methods
            #(allows calling add by geometry type, eg: obj = lavavu.lines())
            for key in geomtypes.keys():
                #Use a closure to define a new method to call addtype with this type
                def addmethod(name):
                    def method(*args, **kwargs):
                        return self._addtype(name, *args, **kwargs)
                    return method
                method = addmethod(key)
                #Set docstring
                method.__doc__ = "Add a " + key + " visualisation object,\nany data loaded into the object will be plotted as " + key
                self.__setattr__(key, method)

        except (RuntimeError) as e:
            print("LavaVu Init error: " + str(e))
            pass

    def setup(self, arglist=None, database=None, figure=None, timestep=None, 
         port=0, verbose=False, interactive=False, hidden=True, cache=False,
         quality=2, writeimage=False, resolution=None, script=None, initscript=False, usequeue=False, **kwargs):
        """
        Execute the viewer, initialising with provided arguments and
        entering event loop if requested

        Parameters
        ----------
        see __init__ docs

        """
        #Convert options to args
        args = settings["default_args"][:]
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
            if isinstance(arglist, list):
                args += arglist
            else:
                args += [str(arglist)]
        self.queue = usequeue

        #Additional keyword args as property settings
        for key in kwargs:
            if isinstance(kwargs[key], str):
                args += [key + '="' + kwargs[key] + '"']
            else:
                args += [key + '=' + str(kwargs[key])]

        if verbose:
            print(args)

        try:
            self.app.run(args)
            if database:
                #Load objects/state
                self._get()
        except (RuntimeError) as e:
            print("LavaVu Run error: " + str(e))
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
        elif key in self.state["properties"]:
            return self.state["properties"][key]
        elif key in self.properties:
            return self.properties[key][0] #Default from prop list
        else:
            raise ValueError(key + " : Invalid property name")
        return None

    def __setitem__(self, key, item):
        #Set view/global property
        if not key in self.properties:
            raise ValueError(key + " : Invalid property name")
        self.app.parseProperty(key + '=' + str(item))
        self._get()

    def __contains__(self, key):
        return key in self.state or key in self.state["properties"] or key in self.state["views"][0]

    def __repr__(self):
        self._get()
        #return '{"' + self.state["properties"]["caption"] + '"}'
        return '{}'

    def __eq__(self, other):
        #Necessary because being forced to inherit from dict to support buggy IPython key completion
        #means we get dict equality, which we _really_ don't want
        #Two Viewer objects should never be equal unless they refer to the same object
        return id(self) == id(other)

    def __str__(self):
        #View/global props to string
        self._get()
        properties = self.state["properties"]
        properties.update(self.state["views"][0])
        return str('\n'.join(['    %s=%s' % (k,json.dumps(v)) for k,v in properties.items()]))

    @property
    def objects(self):
        """
        Returns the active objects

        Returns
        -------
        objects: Objects(dict)
            A dictionary wrapper containing the list of available visualisation objects
            Can be printed, iterated or accessed as a dictionary by object name
        """
        self._get()
        return self._objects

    @property
    def colourmaps(self):
        """
        Returns the list of active colourmaps

        Returns
        -------
        colourmaps: (dict)
            A dictionary containing the available colourmaps as ColourMap() wrapper objects
         """
        self._get()
        maps = {}
        for cm in self.state["colourmaps"]:
            maps[cm["name"]] = ColourMap(cm["name"], self)
        return maps

    @property
    def figures(self):
        """
        Retrieve the saved figures from loaded model
        Dict returned contains Fig objects which can be used to modify the figure

        Returns
        -------
        figures: dict
            A dictionary of all available figures by name
        """
        if not self.app.amodel:
            return {}
        figs = self.app.amodel.fignames
        figures = {}
        for fig in figs:
            figures[fig] = Fig(self, name=fig)
        return figures

    @property
    def steps(self):
        """
        Retrieve the time step data from loaded model

        Returns
        -------
        timesteps: list
            A list of all available time steps
        """
        return self.timesteps()

    def Figure(self, name, objects=None, **kwargs):
        """
        Create a figure

        Parameters
        ----------
        name: str
            Name of the figure
        objects: list
            List of objects or object names to include in the figure, others will be hidden

        Returns
        -------
        figure: Figure
            Figure object
        """
        #Show only objects in list
        if objects and isinstance(objects, list):
            #Hide all first
            for o in self._objects:
                self._objects[o]["visible"] = False
            #Show by name or Object
            for o in objects:
                if isinstance(o, str):
                    if o in self._objects:
                        o["visible"] = True
                else:
                    if o in self._objects.list:
                        o["visible"] = True

        #Create a new figure
        self.savefigure(name)
        #Sync list
        figs = self.figures
        #Get figure wrapper object
        fig = figs[name]
        #Additional keyword args = properties
        for key in kwargs:
            fig[key] = kwargs[key]
        #Return figure
        return fig

    @property
    def step(self):
        """    
        step (int): Returns current timestep
        """
        return self.timestep()

    @step.setter
    def step(self, value):
        """    
        step (int): Sets current timestep
        """
        self.timestep(value)

    def _get(self):
        #Import state from lavavu
        self.state = convert_keys(json.loads(self.app.getState()))
        self._objects._sync()

    def _set(self):
        #Export state to lavavu
        #(include current object list state)
        #self.state["objects"] = [obj.dict for obj in self._objects.list]
        self.app.setState(json.dumps(self.state))

    def commands(self, cmds):
        """
        Execute viewer commands
        https://github.com/OKaluza/LavaVu/wiki/Scripting-Commands-Reference
        These commands can also be executed individually by calling them as methods of the viewer object

        Parameters
        ----------
        cmds: list, str
            Command(s) to execute
        """
        if isinstance(cmds, list):
            cmds = '\n'.join(cmds)
        if self.queue: #Thread safe queue requested
            self.app.queueCommands(cmds)
        else:
            self.app.parseCommands(cmds)

    def help(self, cmd="", obj=None):
        """
        Get help on a command or property

        Parameters
        ----------
        cmd: str
            Command to get help with, if ommitted displays general introductory help
            If cmd is a property or is preceded with '@' will display property help
        """
        if obj is None: obj = self
        md = ""
        if not len(cmd):
            md += _docmd(obj.__doc__)
        elif cmd in dir(obj) and callable(getattr(obj, cmd)):
            md = '### ' + cmd + '  \n'
            md += _docmd(getattr(obj, cmd).__doc__)
        else:
            if cmd[0] != '@': cmd = '@' + cmd
            md = self.app.helpCommand(cmd)
        _markdown(md)

    def __call__(self, cmds):
        """
        Run a LavaVu script

        Parameters
        ----------
        cmds: str
            String containing commands to run, separate commands with semi-colons"

        Example
        -------
        >>> lv('reset; translate -2')

        """
        self.commands(cmds)

    def _setupobject(self, ref=None, **kwargs):
        #Strip data keys from kwargs and put aside for loading
        datasets = {}
        cmapstr = None
        for key in list(kwargs):
            if key in ["vertices", "normals", "vectors", "colours", "indices", "values", "labels"]:
                datasets[key] = kwargs.pop(key, None)

        #Call function to add/setup the object, all other args passed to properties dict
        if ref is None:
            ref = self.app.createObject(convert_args(kwargs))
        else:
            self.app.setObject(ref, convert_args(kwargs))

        #Get the created/update object
        obj = self.Object(ref)

        #Read any property data sets (allows object creation and load with single prop dict)
        for key in datasets:
            #Get the load function matching the data set (eg: vertices() ) and call on data
            func = getattr(obj, key)
            func(datasets[key])

        #Return wrapper obj
        return obj

    def add(self, name, **kwargs):
        """
        Add a visualisation object

        Parameters
        ----------
        name: str
            Name to apply to the created object
            If an object of this name exists, it will be returned instead of created

        Returns
        -------
        obj: Object
            The object created
        """
        if isinstance(self._objects, Objects) and name in self._objects:
            print("Object exists: " + name)
            return self._objects[name]

        #Put provided name in properties
        if name and len(name):
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

    def grid(self, name=None, width=0, height=0, dims=[2,2], vertices=None, *args, **kwargs):
        #Creates a quads object, 
        obj = self._addtype("quads", name, dims=dims, *args, **kwargs)
        if width > 0 and height > 0 and vertices is None:
            vertices = []
            yc = 0.0
            for y in range(dims[1]):
                xc = 0.0
                for x in range(dims[0]):
                    vertices.append([xc, yc, 0])
                    xc += width / float(dims[0])
                yc += height / float(dims[1])
        obj.vertices(vertices)
        return obj

    def Object(self, identifier=None, **kwargs):
        """
        Get or create a visualisation object

        Parameters
        ----------
        identifier: str,int,Object (Optional)
            If a string, lookup an object by name
            If a number, lookup object by index
            If an object reference, lookup the Object by reference
            If omitted, return the last object in the list
            If no matching object found and string identifier provided, creates an empty object
        **kwargs:
            Set of properties passed to the object

        Returns
        -------
        obj: Object
            The object located
        """
        #Return object by name/ref or last in list if none provided
        #Get state and update object list
        self._get()
        o = None
        if len(self._objects.list) == 0:
            print("WARNING: No objects exist!")
        #If name passed, find this object in updated list, if not just use the last
        elif isinstance(identifier, str):
            for obj in self._objects.list:
                if obj.name == identifier:
                    o = obj
                    break
            #Not found? Create
            if not o:
                return self.add(identifier, **kwargs)
        elif isinstance(identifier, int):
            #Lookup by index
            if len(self._objects.list) >= identifier:
                o = self._objects.list[identifier-1]
        elif isinstance(identifier, LavaVuPython.DrawingObject):
            #Lookup by swig wrapped object
            for obj in self._objects.list:
                #Can't compare swig wrapper objects directly,
                #so use the name
                if obj.name == identifier.name():
                    o = obj
                    break
        else:
            #Last resort: last object in list
            o = self._objects.list[-1]

        if o is not None:
            self.app.setObject(o.ref, convert_args(kwargs))
            return o
        print("WARNING: Object not found and could not be created: ",identifier)
        return None

    def ColourMap(self, identifier=None, **kwargs):
        """
        Get or create a colourmap

        Parameters
        ----------
        identifier: str,int,Object (Optional)
            If a string, lookup an object by name
            If a number, lookup object by index
            If an object reference, lookup the Object by reference
            If omitted, return the last object in the list
            If no matching object found and string identifier provided, creates an empty object
        **kwargs:
            Set of properties passed to the object

        Returns
        -------
        obj: Object
            The object located
        """

    def file(self, filename, obj=None, **kwargs):
        """
        Load a database or model file

        Parameters
        ----------
        filename: str
            Name of the file to load
        obj: Object
            Vis object to load the file data into,
            if not provided a default will be created
        """

        #Load a new object from file
        self.app.loadFile(filename)

        #Get last object added if none provided
        if obj is None:
            obj = self.Object()
        if obj is None:
            if not "json" in filename and not "script" in filename:
                print("WARNING: No objects exist after file load: " + filename)
            return None

        #Setups up new object, all other args passed to properties dict
        return self._setupobject(obj.ref, **kwargs)
    
    def files(self, files, obj=None, **kwargs):
        """
        Load data from a series of files (using wildcards or a list)

        Parameters
        ----------
        files: str
            Specification of the files to load, either a list of full paths or a file spec string such as *.gldb
        obj: Object
            Vis object to load the data into,
            if not provided a default will be created
        """
        #Load list of files with glob
        filelist = []
        if isinstance(files, list) or isinstance(files, tuple):
            filelist = files
        elif isinstance(files, str):
            filelist = sorted(glob.glob(files))
        obj = None
        for infile in filelist:
            obj = self.file(infile, **kwargs)
        return obj

    def colourbar(self, obj=None, **kwargs):
        """
        Create a new colourbar

        Parameters
        ----------
        obj: Object (optional)
            Vis object the colour bar applies to

        Returns
        -------
        colourbar: Object
            The colourbar object created
        """
        #Create a new colourbar
        if obj is None:
            ref = self.app.colourBar()
        else:
            ref = self.app.colourBar(obj.ref)
        if not ref: return
        #Update list
        self._get() #Ensure in sync
        #Setups up new object, all other args passed to properties dict
        return self._setupobject(ref, **kwargs)

    def defaultcolourmaps(self):
        """
        Get the list of default colour map names

        Returns
        -------
        colourmaps: list of str
            Names of all predefined colour maps
        """
        return list(LavaVuPython.ColourMap.getDefaultMapNames())

    def defaultcolourmap(self, name):
        """
        Get content of a default colour map

        Parameters
        ----------
        name: str
            Name of the built in colourmap to return

        Returns
        -------
        data: str
            Colourmap data formatted as a string
        """
        return LavaVuPython.ColourMap.getDefaultMap(name)

    def colourmap(self, name, data=cubeHelix(), reverse=False, monochrome=False, **kwargs):
        """
        Load or create a colour map

        Parameters
        ----------
        name: str
            Name of the colourmap, if this colourmap name is found
            the data will replace the existing colourmap, otherwise
            a new colourmap will be created
        data: list,str
            Provided colourmap data can be
            - a string,
            - list of colour strings,
            - list of position,value tuples
            - or a built in colourmap name
        reverse: boolean
            Reverse the order of the colours after loading
        monochrome: boolean
            Convert to greyscale

        Returns
        -------
        colourmap: ColourMap(dict)
            The wrapper object of the colourmap loaded/created
        """
        c = ColourMap(self.app.addColourMap(name), self)
        c.update(data, reverse, monochrome, **kwargs)
        return c

    def getcolourmap(self, mapid, string=True):
        """
        Return colour map data as a string or list
        Either return format can be used to create/modify a colourmap
        with colourmap()

        Parameters
        ----------
        mapid: string/int
            Name or index of the colourmap to retrieve
        string: boolean
            The default is to return the data as a string of colours separated by semi-colons
            To instead return a list of (position,[R,G,B,A]) tuples for easier automated processing in python,
            set this to False

        Returns
        -------
        mapdata: str/list
            The formatted colourmap data
        """
        c = ColourMap(mapid, self)
        if string:
            return c.__str__()
        else:
            return c.tolist()

    def clear(self, objects=True, colourmaps=True):
        """
        Clears all data from the visualisation
        Including objects and colourmaps by default

        Parameters
        ----------
        objects: boolean
            including all objects, if set to False will clear the objects
            but not delete them
        colourmaps: boolean
            including all colourmaps

        """
        self.app.clearAll(objects, colourmaps)
        self._get() #Ensure in sync

    def store(self, filename="state.json"):
        """
        Store current visualisation state as a json file
        (Includes all properties, colourmaps and defined objects but not their geometry data)

        Parameters
        ----------
        filename: str
            Name of the file to save

        """
        with open(filename, "w") as state_file:
            state_file.write(self.app.getState())

    def restore(self, filename="state.json"):
        """
        Restore visualisation state from a json file
        (Includes all properties, colourmaps and defined objects but not their geometry data)

        Parameters
        ----------
        filename: str
            Name of the file to load

        """

        if os.path.exists(filename):
            with open(filename, "r") as state_file:
                self.app.setState(state_file.read())

    def timesteps(self):
        """
        Retrieve the time step data from loaded model

        Returns
        -------
        timesteps: list
            A list of all available time steps
        """
        return json.loads(self.app.getTimeSteps())

    def addstep(self, step=-1, **kwargs):
        """
        Add a new time step

        Parameters
        ----------
        step: int (Optional)
            Time step number, default is current + 1
        **kwargs:
            Timestep specific properties passed to the created object
        """
        self.app.addTimeStep(step, convert_args(kwargs))

    def render(self):
        """        
        Render a new frame, explicit display update
        """
        self.app.render()

    def init(self):
        """        
        Re-initialise the viewer window
        """
        self.app.init()

    def update(self, filter=None, compress=True):
        """
        Write visualisation data back to the database

        Parameters
        ----------
        filter: str
            Optional filter to type of geometry to be updated, if omitted all will be written
            (labels, points, grid, triangles, vectors, tracers, lines, shapes, volume)
        compress: boolean
            Use zlib compression when writing the geometry data
        """
        for obj in self._objects.list:
            #Update object data at current timestep
            if filter is None:
                #Re-writes all data to db for object
                self.app.update(obj.ref, compress)
            else:
                #Re-writes data to db for object and geom type
                self.app.update(obj.ref, geomtypes[filter], compress)

        #Update figure
        self.savefigure()

    def image(self, filename="", resolution=None, transparent=False):
        """
        Save or get an image of current display

        Parameters
        ----------
        filename: str
            Name of the file to save (should be .jpg or .png),
            if not provided the image will be returned as a base64 encoded data url
        resolution: list, tuple
            Image resolution in pixels [x,y]
        transparent: boolean
            Creates a PNG image with a transparent background

        Returns
        -------
        image: str
            filename of saved image or encoded image as string data
        """
        if resolution is None:
            return self.app.image(filename, 0, 0, 0, transparent);
        return self.app.image(filename, resolution[0], resolution[1], 0, transparent);

    def frame(self, resolution=None):
        """
        Get an image frame, returns current display as base64 encoded jpeg data url

        Parameters
        ----------
        resolution: list, tuple
            Image resolution in pixels [x,y]

        Returns
        -------
        image: str
            encoded image as string data
        """
        #Jpeg encoded frame data
        if not resolution: resolution = self.resolution
        return self.app.image("", resolution[0], resolution[1], 90);

    def display(self, resolution=(0,0), transparent=False):
        """        
        Show the current display as inline image within an ipython notebook.
        
        If IPython is installed, displays the result image content inline

        If IPython is not installed, will save the result with 
        a default filename in the current directory

        Parameters
        ----------
        resolution: list, tuple
            Image resolution in pixels [x,y]
        transparent: boolean
            Creates a PNG image with a transparent background
        """
        if is_notebook():
            from IPython.display import display,Image,HTML
            #Return inline image result
            img = self.app.image("", resolution[0], resolution[1], 0, transparent)
            display(HTML("<img src='%s'>" % img))
        else:
            #Not in IPython, call default image save routines (autogenerated filenames)
            self.app.image("*", resolution[0], resolution[1], 0, transparent)

    def webgl(self, resolution=(640,480)):
        """        
        Shows the generated model inline within an ipython notebook.
        
        If IPython is installed, displays the result WebGL content inline

        If IPython is not installed, will save the result with
        a default filename in the current directory

        Parameters
        ----------
        resolution: list, tuple
            Display window resolution in pixels [x,y]
        """

        try:
            if is_notebook():
                #TODO: build single html file instead with inline scripts/data/custom controls
                #Create link to web content directory
                if not os.path.isdir("html"):
                    htmldir = os.path.join(binpath, 'html')
                    os.symlink(htmldir, 'html')
                from IPython.display import display,Image,HTML
                #Write files to disk first, can be passed directly on url but is slow for large datasets
                filename = "input.json"
                text_file = open("html/" + filename, "w")
                text_file.write(self.app.web());
                text_file.close()
                from IPython.display import IFrame
                display(IFrame("html/viewer.html#" + filename, width=resolution[0], height=resolution[1]))
            else:
                self.app.web(True)
        except (Exception) as e:
            print("WebGL output error: " + str(e))
            pass

    def video(self, filename="", fps=10, resolution=(0,0)):
        """        
        Shows the generated video inline within an ipython notebook.
        
        If IPython is installed, displays the result video content inline

        If IPython is not installed, will save the result in the current directory


        Parameters
        ----------
        filename: str
            Name of the file to save, if not provided a default will be used
        fps: int
            Frames to output per second of video
        resolution: list, tuple
            Video resolution in pixels [x,y]
        """

        try:
            fn = self.app.video(filename, fps, resolution[0], resolution[1])
            if is_notebook():
                from IPython.display import display,HTML
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
        except (Exception) as e:
            print("Video output error: " + str(e))
            pass

    def imageBytes(self, width=640, height=480, channels=3):
        """
        Return raw image data

        Parameters
        ----------
        width: int
            Image width in pixels
        height: int
            Image height in pixels
        channels: int
            colour channels/depth in bytes (1=luminance, 3=RGB, 4=RGBA)

        Returns
        -------
        image: array
            Numpy array of the image data requested
        """
        img = numpy.zeros(shape=(width,height,channels), dtype=numpy.uint8)
        self.imageBuffer(img)
        return img

    def testimages(self, imagelist=None, tolerance=TOL_DEFAULT, expectedPath='expected/', outputPath='./', clear=True):
        """
        Compare a list of images to expected images for testing

        Parameters
        ----------
        imagelist: list
            List of test images to compare with expected results
            If not provided, will process all jpg and png images in working directory
        tolerance: float
            Tolerance level of difference before a comparison fails, default 0.0001
        expectedPath: str
            Where to find the expected result images (should have the same filenames as output images)
        outputPath: str
            Where to find the output images
        clear: boolean
            If the test passes the output images will be deleted, set to False to disable deletion
        """
        results = []
        if not os.path.isdir(expectedPath):
            print("No expected data, copying found images to expected folder...")
            os.makedirs(expectedPath)
            from shutil import copyfile
            if not imagelist:
                #Get all images in cwd
                imagelist = glob.glob("*.png")
                imagelist += glob.glob("*.jpg")
            print(imagelist)
            for image in imagelist:
                copyfile(image, os.path.join(expectedPath, image))

        if not imagelist:
            #Default to all png images in expected dir
            cwd = os.getcwd()
            os.chdir(expectedPath)
            imagelist = glob.glob("*.png")
            imagelist += glob.glob("*.jpg")
            imagelist.sort(key=os.path.getmtime)
            os.chdir(cwd)

        for f in sorted(imagelist):
            outfile = outputPath+f
            expfile = expectedPath+f
            results.append(self.testimage(expfile, outfile, tolerance))

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
        print("-------------\nTests Passed!\n-------------")

    def testimage(self, expfile, outfile, tolerance=TOL_DEFAULT):
        """
        Compare two images

        Parameters
        ----------
        expfile: str
            Expected result image
        outfile: str
            Test output image
        tolerance: float
            Tolerance level of difference before a comparison fails, default 0.0001

        Returns
        -------
        result: boolean
            True if test passes, difference <= tolerance
            False if test fails, difference > tolerance
        """
        if not os.path.exists(expfile):
            print("Test skipped, Reference image '%s' not found!" % expfile)
            return 0
        if len(outfile) and not os.path.exists(outfile):
            raise RuntimeError("Generated image '%s' not found!" % outfile)

        diff = self.app.imageDiff(outfile, expfile)
        result = diff <= tolerance
        reset = '\033[0m'
        red = '\033[91m'
        green = '\033[92m'
        failed = red + 'FAIL' + reset
        passed = green + 'PASS' + reset
        if not result:
            print("%s: %s Image comp errors %f, not"\
                  " within tolerance %g of reference image."\
                % (failed, outfile, diff, tolerance))
            if settings["echo_fails"]:
                print("__________________________________________")
                if len(outfile):
                    print(outfile)
                    with open(outfile, mode='rb') as f:
                        data = f.read()
                        import base64
                        print("data:image/png;base64," + base64.b64encode(data).decode('ascii'))
                else:
                    print("Buffer:")
                    print(self.app.image(""))
                print("__________________________________________")
        else:
            print("%s: %s Image comp errors %f, within tolerance %f"\
                  " of ref image."\
                % (passed, outfile, diff, tolerance))
        return result

    def serve(self):
        """
        Run a web server in python (experimental)
        This uses server.py to launch a simple web server
        Not threaded like the mongoose server used in C++ to is limited

        Currently recommended to use threaded web server by supplying
        port=#### argument when creating viewer instead
        """
        try:
            import server
            server.serve(self)
        except (Exception) as e:
            print("LavaVu error: " + str(e))
            print("Web Server failed to run")
            import traceback
            traceback.print_exc()
            pass

    def window(self):
        """
        Create and display an interactive viewer instance

        This shows an active viewer window to the visualisation
        that can be controlled with the mouse or html widgets

        """
        if self in control.windows:
            print("Viewer window exists, only one instance per Viewer permitted")
            return
        self.control.Window()
        self.control.show()

    def redisplay(self):
        """
        Update the display of any interactive viewer

        This is required after changing vis properties from python
        so the changes are reflected in the viewer

        """
        #Issue redisplay to active viewer
        self.control.redisplay()

    def camera(self):
        """
        Get the current camera viewpoint

        Displays camera in python code form that can be pasted and
        executed to restore the same camera view
        """
        self._get()
        me = getVariableName(self)
        if not me: me = "lv"
        #Also print in terminal for debugging
        self.commands("camera")
        #Export from first view
        if len(self.state["views"]) and self.state["views"][0]:
            qrot = self.state["views"][0]["rotate"]
            rot = self.state["views"][0]["xyzrotate"]
            tr = self.state["views"][0]["translate"]
            for r in range(3):
                rot[r] = round(rot[r], 3)
                tr[r] = round(tr[r], 3)
            qrot[3] = round(qrot[3], 3)
            print(me + ".translation(" + str(tr)[1:-1] + ")")
            print(me + ".rotation(" + str(rot)[1:-1] + ")")
            return {"translation" : tr, "xyzrotation" : rot, "rotation" : qrot}
        else:
            return {}

    def getview(self):
        """
        Get current view settings

        Returns
        -------
        view: str
            json string containing saved view settings
        """
        self._get()
        return json.dumps(self.state["views"][0])

    def setview(self, view):
        """
        Set current view settings

        Parameters
        ----------
        view: str
            json string containing saved view settings
        """
        self.state["views"][0] = convert_keys(json.loads(view))

    def event(self):
        """
        Process input events
        allows running interactive event loop while animating from python
        eg:

        >>> while (lv.event()):
        >>>     #...build next frame here...
        >>>     lv.render()

        Returns
        -------
        boolean:
            False if user quit program, True otherwise
        """
        return self.app.event()

    def interactive(self, force=False):
        """
        Opens an external interactive window
        Disabled from IPython without force=True as this blocks the kernel
        In MacOS will never return so kernel halts on closing the window
        TODO: Use an interactive web viewer window instead but with instant mouse/keyboard feedback

        >>> lv.interactive()
        >>> #Will not return until interactive window is closed

        Parameters
        ----------
        force: boolean
            Set to True to open the window, even if running in IPython
        """
        if force or not is_notebook():
            return self.commands("interactive")
        else:
            print("External interactive viewer window disabled from within IPython notebook")
            print("to force, use interactive(force=True)")

#Wrapper for list of geomdata objects
class Geometry(list):
    """  
    The Geometry class provides an interface to a drawing object's internal data
    Geometry instances are created internally in the Object class with the data() method

    Example
    -------
        
    Get all object data

    >>> data = obj.data()
    
    Get only triangle data

    >>> data = obj.data("triangles")

    Loop through data

    >>> data = obj.data()
    >>> for el in obj.data:
    >>>     print(el)

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
        """
        Get a data element from a set of geometry data

        Warning... other than for internal use, should always
        immediately make copies of the data
        there is no guarantee memory will not be released!

        Parameters
        ----------
        typename: str
            Type of data to be retrieved
            (vertices/normals/vectors/indices/colours/texcoords/luminance/rgb/values)

        Returns
        -------
        data: array
            Numpy array view of the data set requested
        """
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
        """
        Get a copy of a data element from a set of geometry data

        This is a safe version of get() that copies the data
        before returning so can be assured it will remain valid

        Parameters
        ----------
        typename: str
            Type of data to be retrieved
            (vertices/normals/vectors/indices/colours/texcoords/luminance/rgb/values)

        Returns
        -------
        data: array
            Numpy array containing a copy of the data set requested
        """
        #Safer data access, makes a copy to ensure we still have access 
        #to the data no matter what viewer does with it
        return numpy.copy(self.get(typename))

    def set(self, typename, array):
        """
        Set a data element in a set of geometry data

        Parameters
        ----------
        typename: str
            Type of data to set
            (vertices/normals/vectors/indices/colours/texcoords/luminance/rgb/values)
        array: array
            Numpy array holding the data to be written
        """
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

def loadCPT(fn, positions=True):
    """
    Create a colourmap from a CPT colour table file

    Parameters
    ----------
    positions: boolean
        Set this to false to ignore any positional data and
        load only the colours from the file

    Returns
    -------
    colours: string
        Colour string ready to be loaded by colourmap()
    """
    result = ""
    values = []
    colours = []
    hexcolours = []
    hsv = False
    hinge = None

    def addColour(val, colour):
        if len(values) and val == values[-1]:
            if colour == colours[-1]:
                return #Skip duplicates
            val += 0.001 #Add a small increment
        values.append(val)
        if isinstance(colour, str):
            if '/' in colour:
                colour = colour.split('/')
            if '-' in colour:
                colour = colour.split('-')
        if isinstance(colour, list):
            if hsv:
                colour = [float(v) for v in colour]
                import colorsys
                colour = colorsys.hsv_to_rgb(colour[0]/360.0,colour[1],colour[2])
                colour = [int(v*255) for v in colour]
            else:
                colour = [int(v) for v in colour]

        if len(colours) == 0 or colour != colours[-1]:
            if isinstance(colour, str):
                hexcolours.append(colour)
            else:
                hexcolours.append("#%02x%02x%02x" % (colour[0],colour[1],colour[2]))

        colours.append(colour)

    with open(fn, "r") as cpt_file:
        for line in cpt_file:
            if "COLOR_MODEL" in line and 'hsv' in line.lower():
                hsv = True
                continue
            if "HINGE" in line:
                line = line.split('=')
                hinge = float(line[1])
                continue
            if line[0] == '#': continue
            if line[0] == 'B': continue
            if line[0] == 'F': continue
            if line[0] == 'N': continue
            line = line.split()
            #RGB/HSV space separated?
            if len(line) > 7:
                addColour(float(line[0]), [int(line[1]), int(line[2]), int(line[3])])
                addColour(float(line[4]), [int(line[5]), int(line[6]), int(line[7])])
            #Pass whole strings if / or - separated
            elif len(line) > 1:
                addColour(float(line[0]), line[1])
                if len(line) > 3:
                    addColour(float(line[2]), line[3])
            


    minval = min(values)
    maxval = max(values)
    vrange = maxval - minval
    #print "HINGE: ",hinge,"MIN",minval,"MAX",maxval
    
    if positions:
        for v in range(len(values)):
            #Centre hinge value
            if hinge is not None:
                if values[v] == hinge:
                    values[v] = 0.5
                elif values[v] < hinge:
                    values[v] = 0.5 * (values[v] - minval) / (hinge - minval)
                elif values[v] > hinge:
                    values[v] = 0.5 * (values[v] - hinge) / (maxval - hinge) + 0.5
            else:
                values[v] = (values[v] - minval) / vrange

            if isinstance(colours[v], str):
                result += "%.5f=%s; " % (values[v], colours[v])
            else:
                result += "%.5f=rgb(%d,%d,%d); " % (values[v], colours[v][0], colours[v][1], colours[v][2])
    else:
        for v in range(len(hexcolours)):
            #print "(%f)%s" % (values[v], hexcolours[v]),
            result += hexcolours[v] + " "

    return result

def getVariableName(var):
    """
    Attempt to find the name of a variable from the main module namespace

    Parameters
    ----------
    var: object
        The variable in question

    Returns
    -------
    name: str
        Name of the variable
    """
    import __main__ as main_mod
    for name in dir(main_mod):
        val = getattr(main_mod, name)
        if val is var:
            return name
    return None

def printH5(h5):
    """
    Print info about HDF5 data set (requires h5py)

    Parameters
    ----------
    h5: hdf5 object
        HDF5 Dataset loaded with h5py
    """
    print("------ ",h5.filename," ------")
    ks = h5.keys()
    for key in ks:
        print(h5[key])
    for item in h5.attrs.keys():
        print(item + ":", h5.attrs[item])

def _docmd(doc):
    """Convert a docstring to markdown"""
    if doc is None: return ''
    def codeblock(lines):
        return ['```python'] + ['    ' + l for l in lines] + ['```']
    md = []
    code = []
    indent = 0
    lastindent = 0
    for line in doc.split('\n'):
        indent = len(line)
        line = line.strip()
        indent -= len(line)
        if len(line) and len(md) and line[0] == '-' and line == len(md[-1].strip()) * '-':
            #Replace '-----' heading underline with '#### heading"
            md[-1] = "#### " + md[-1]
        elif line.startswith('>>> '):
            #Python code
            code += [line[4:]]
        else:
            #Add code block
            if len(code):
                md += codeblock(code)
                code = []
            elif len(md) and indent == lastindent + 4:
                #Indented block, preserve indent
                md += ['&nbsp;&nbsp;&nbsp;&nbsp;' + line + '  ']
                #Keep indenting at this level until indent level changes
                continue
            else:
                md += [line + '  '] #Preserve line breaks
        lastindent = indent
    if len(code):
        md += codeblock(code)
    return '\n'.join(md)

def _markdown(mdstr):
    """Display markdown in IPython if available,
    otherwise just print it"""
    if is_notebook():
        from IPython.display import display,Markdown
        display(Markdown(mdstr))
    else:
        #Not in IPython, just print
        print(mdstr)

