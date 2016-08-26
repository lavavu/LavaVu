/*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
* LavaVu python interface
**~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

%module LavaVu
%include <std_string.i>
%include <std_vector.i>

%{
#include "src/LavaVu.h"
#include "src/ViewerTypes.h"
%}

%include "exception.i"
%exception {
    try {
        $action
    } catch (const std::runtime_error& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    }
}

namespace std {
%template(Line)  vector <float>;
%template(ULine) vector <unsigned int> ;
%template(Array) vector < vector <float> >;
%template(List) vector <string>;
}

%pythoncode %{
#Helper functions
def load(app=None, arglist=[], binary="LavaVu", database=None, figure=None, startstep=None, endstep=None, 
         port=0, verbose=False, interactive=False, hidden=True, cache=False,
         quality=2, writeimage=False, res=None, script=None):
  args = [] + arglist
  #Convert options to args
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
  if startstep != None:
    args += ["-" + str(startstep)]
    if endstep != None:
      args += ["-" + str(endstep)]
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
  #Save image and quit
  if writeimage:
    args += ["-I"]
  if script and isinstance(script,list):
    args += script

  if not app:
    app = LavaVu(binary)
  app.run(args)
  return app

def propstring(data):
  import json
  return '\n'.join(['%s=%s' % (k,json.dumps(v)) for k,v in data.iteritems()])


#Wrapper class for drawing object
#handles property updating
class Obj(dict):
  def __init__(self, drobj, lv, idict, *args, **kwargs):
    self.drobj = drobj
    self.lv = lv
    self.update(idict)
    super(Obj, self).__init__(*args, **kwargs)

  def __setitem__(self, key, value):
    super(Obj, self).__setitem__(key, value)
    self.lv.commands(propstring(self))

%}

%include "src/ViewerTypes.h"

class LavaVu
{
public:
  Model* amodel;
  View* aview;
  DrawingObject* aobject;
  std::string binpath;

  LavaVu(std::string binary="");
  ~LavaVu();

  void run(std::vector<std::string> args={});

  bool loadFile(const std::string& file);
  bool parseCommands(std::string cmd);
  std::string image(std::string filename="", int width=0, int height=0);
  std::string web(bool tofile=false);
  void defaultModel();
  std::string addObject(std::string name, std::string properties);
  void setState(std::string state);
  std::string getStates();
  std::string getTimeSteps();
  void loadVectors(std::vector< std::vector <float> > array, lucGeometryDataType type=lucVertexData);
  void loadScalars(std::vector <float> array, lucGeometryDataType type=lucColourValueData, std::string label="", float minimum=0, float maximum=0);
  void loadUnsigned(std::vector <unsigned int> array, lucGeometryDataType type=lucIndexData);
  void close();

%pythoncode %{
  def commands(self, cmds):
    if isinstance(cmds, list):
      self.parseCommands('\n'.join(cmds))
    else:
      self.parseCommands(cmds)

  def add(self, name="(unnamed)", properties={}):
    if not self.amodel:
      self.defaultModel()

    #Adds a new object
    objprops = self.addObject(name, propstring(properties))

    #Read any property data sets
    self.parseCommands("read vertices\nread normals\nread vectors\nread colours\nread values")

    #Return wrapper obj
    import json
    return Obj(self.aobject, self, json.loads(objprops))

  def file(self, filename, name=None, properties={}):
    #Load a new object from file
    self.loadFile(filename)
    if name:
      properties["name"] = name

    #Update properties
    self.commands(propstring(properties))

    #Return wrapper obj
    return Obj(self.aobject, self, properties)

  def clear(self):
    self.close()

  def vertices(self, data):
    self.loadVectors(data, lucVertexData)

  def vectors(self, data):
    self.loadVectors(data, lucVectorData)

  def values(self, data, type=lucColourValueData, label="", min=0., max=0.):
    self.loadScalars(data, type, label, min, max)

  def colours(self, data):
    self.parseCommands("colours=" + str(data))
    self.parseCommands("read colours")

  def indices(self, data):
    self.loadUnsigned(data, lucIndexData)

  def show(self, resolution=(640,480)):
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
          img = self.image("", resolution[0], resolution[1])
          display(HTML("<img src='%s'>" % img))
      except NameError, ImportError:
        #Not in IPython, call default image save routines (autogenerated filenames)
        self.image("test", resolution[0], resolution[1])

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
          #Create link to web content directory
          import os
          if not os.path.isdir("html"):
            os.symlink(os.path.join(self.binpath, 'html'), 'html')
          text_file = open("html/" + filename, "w")
          text_file = open("html/" + filename, "w")
          text_file.write(self.web());
          text_file.close()
          from IPython.display import IFrame
          display(IFrame("html/index.html#" + filename, width=resolution[0], height=resolution[1]))
      except NameError, ImportError:
        self.web(True)
      except Exception,e:
        print "WebGL output error: " + str(e)
        pass
%}
};

