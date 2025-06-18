LavaVu Installation
===================

Python
------

LavaVu is compatible with Python 3.6+ and is available in the python package index, so you can install with *pip*:

It is recommended you install in a virtualenv (by installing Anaconda or creating your own):

::

  pip install virtualenv

  #Create a virtual env called 'python-default' and activate it:

  virtualenv python-default
  source python-default/bin/activate;

..

Now you can go ahead and install LavaVu:

::

  #In anaconda or virtualenv
  pip install lavavu

  #Or to install as a user package
  pip install --user lavavu

..

   If you don’t have pip available, you can try
   ``sudo easy_install pip`` or just install
   `Anaconda <https://www.anaconda.com/download>`__, which comes with
   pip and a whole lot of other useful packages for scientific work with
   python.

   Binary wheels are provided that include all dependencies for MacOS, Linux and Windows,
   if building from source some of the dependencies below may need to be installed.


To try it out:

::

  python
  > import lavavu
  > lv = lavavu.Viewer() #Create a viewer
  > lv.test()            #Plot some sample data
  > lv.display()         #Render an image


Dependencies
------------

-  OpenGL and Zlib, present on most systems, headers may need to be
   installed, eg: on Ubuntu
   ``sudo apt install build-essential libgl1-mesa-dev libx11-dev zlib1g-dev``
-  To use with python requires python 3.6+ and NumPy.
-  For video output, requires: pyav, or for built in encoding: libavcodec, libavformat, libavutil, libswscale (from FFmpeg / libav)
   Can be installed on Ubuntu with 
   ``sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev``
   Or on Mac with ``brew install ffmpeg``
-  TIFF loading support requires libtiff
-  To build the python interface from source requires swig (http://www.swig.org/) but the source code
   includes pre-built interface files so this is only necessary if modifying the interface or C++ library.


IPython
~~~~~~~
To use the IPython notebook integration features of LavaVu you need to install a few python packages

::

  pip install ipython jupyter


Test with IPython in Jupyter, this will open the notebook interface in a web browser window
Example notebooks can be found in the 'notebooks' directory

::

  jupyter notebook

(ctrl+c in terminal to exit)

To test in a jupyter notebook:

::

  import lavavu
  lv = lavavu.Viewer() #Create a viewer

  lv.test()            #Plot some sample data

  lv.window()          #Open an inline interactive viewer window

Remember to activate the virtual env before using at a later time:

::

  source ~/python-default/bin/activate;
  cd ~/LavaVu
  jupyter notebook

Native
------

Alternatively, clone the repository with *git* and build from source:

::

  git clone https://github.com/lavavu/LavaVu
  cd LavaVu
  make -j4

If all goes well the viewer will be built, try running with:
./lavavu/LavaVu

Build options
~~~~~~~~~~~~~

*LIBPNG=1*

- Use libpng instead of built in routines for PNG image read/write

*TIFF=1*

- Build with TIFF image read/write support (requires libtiff)

*VIDEO=1* 

- Build with MP4 video output support (requires libavcodec,libavformat,libavutil,libswscale from ffmpeg/libav)

*CONFIG=debug* 

- Debug build

*LIBDIR=/path/to/libs* 

- Adds lib path, can be used to point to a specific libGL location to use

Python bindings
~~~~~~~~~~~~~~~

The python bindings will be built automatically using the pre-generated interface files.

To test from Python:

::

    python
    > import lavavu
    > lv = lavavu.Viewer()
    > lv.test()
    > lv.display()

To allow access from outside the install directory, add it to your python path, eg:

::

    export PYTHONPATH=${PYTHONPATH}:${HOME}/LavaVu

If **swig** is installed, the interface can be rebuilt by invoking:

::

    make swig

Google Colab
------------
Experimental support for Google Colab GPU instances is provided,
a binary build for the platform is attached to each release:

::

  #Install LavaVu
  %%bash
  wget https://github.com/lavavu/LavaVu/releases/latest/download/lavavu-colab-gpu.tar.gz
  tar xzf lavavu-colab-gpu.tar.gz

Docker
------

A base dockerfile is provided in the repository root.

You can try it out on binder

.. image:: https://mybinder.org/badge_logo.svg
 :target: https://mybinder.org/v2/gh/lavavu/LavaVu/1.9.9


