![# logo](http://owen.kaluza.id.au/Slides/2017-08-15/LavaVu.png)

[![Build Status](https://github.com/lavavu/LavaVu/workflows/Test/badge.svg)](https://github.com/lavavu/LavaVu/actions?query=workflow:Test)
[![Deploy Status](https://github.com/lavavu/LavaVu/workflows/Deploy/badge.svg?branch=1.7.3)](https://github.com/lavavu/LavaVu/actions?query=workflow:Deploy)
[![DOI](https://zenodo.org/badge/45163055.svg)](https://zenodo.org/badge/latestdoi/45163055)
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/lavavu/LavaVu/1.9.8)

A scientific visualisation tool with a python interface for fast and flexible visual analysis.

Documentation available here [LavaVu Documentation](https://lavavu.github.io/Documentation/)

![examplevis](http://owen.kaluza.id.au/Slides/2017-08-15/combined.png)

LavaVu development is supported by [ACCESS-NRI](https://www.access-nri.org.au/).
Prior development was funded by the Monash Immersive Visualisation Plaform at [Monash eResearch](https://www.monash.edu/researchinfrastructure/eresearch) and the Simulation, Analysis & Modelling component of the [NCRIS AuScope](http://www.auscope.org.au/ncris/) capability.

The acronym stands for: lightweight, automatable  visualisation and analysis viewing utility, but "lava" is also a reference to its primary application as a viewer for geophysical simulations. It was also chosen to be unique enough to find the repository with google.

The project started as a replacement rendering library for the gLucifer<sup>1</sup> framework, visualising geodynamics simulations. The new OpenGL visualisation code was re-implemented as a more general purpose visualisation tool. gLucifer continues as a set of sampling tools for Underworld simulations as part of the [Underworld2](https://github.com/underworldcode/underworld2/) code. LavaVu provides the rendering library for creating 2d and 3d visualisations to view this sampled data, inline within interactive Jupyter notebooks and offline through saved visualisation databases and images/movies.

As a standalone tool it is a scriptable 3D visualisation tool capable of producing publication quality high res images and video output from time varying data sets along with HTML5 3D visualisations in WebGL.
Rendering features include correctly and efficiently rendering large numbers of opaque and transparent points and surfaces and volume rendering by GPU ray-marching. There are also features for drawing vector fields and tracers (streamlines).

Control is via python and a set of simple verbose scripting commands along with mouse/keyboard interaction.
GUI components can be generated for use from a web browser via the python "control" module and a built in web server.

Widgets for interactive use in the Jupyter notebook environment allow use for remote visualisation, eg: on supercomputing environments.

A native data format called GLDB is used to store and visualisations in a compact single file, using SQLite for storage and fast loading. A small number of other data formats are supported for import (OBJ surfaces, TIFF stacks etc). 
Further data import formats are supported with python scripts, with the numpy interface allowing rapid loading and manipulation of data.

### This repository ###

This is the public source code repository for all development on the project.
Development happens in the "master" branch with stable releases tagged, so if you just check out master, be aware that things can be unstable or broken from time to time.

### How do I get set up? ###

It's now in the python package index, so you can install with *pip*:

```
python -m pip install lavavu
```

> Currently binary wheels are provided for Linux x86_64, MacOS x86_64 and ARM64 and Windows x86_64. 

To try it out:

```
python
> import lavavu
> lv = lavavu.Viewer() #Create a viewer
> lv.test()            #Plot some sample data
> lv.interactive()     #Open an interactive viewer window
```

Alternatively, clone this repository with *git* and build from source:

```
  git clone https://github.com/lavavu/LavaVu
  cd LavaVu
  python -m pip install .
```
or 
```
  make -j4

```

If all goes well the viewer will be built, try running with:
  ./lavavu/LavaVu

### Dependencies ###

* OpenGL and Zlib, present on most systems, headers may need to be installed
* To use with python requires python 3.6+ and NumPy
* For video output, requires: PyAV or for built in encoding, libavcodec, libavformat, libavutil, libswscale (from FFmpeg / libav)
* To build the python interface from source requires swig (http://www.swig.org/)

### Who do I talk to? ###

* Report bugs/issues here on github: https://github.com/lavavu/LavaVu/issues
* Contact developer: Owen Kaluza (at) anu.edu.au

For further documentation / examples, see the online documentation
* https://lavavu.github.io/Documentation

### Included libraries ###
In order to avoid as many external dependencies as possible, the LavaVu sources include files from the following public domain or open source libraries, many thanks to the authors for making their code available!
* SQLite3 https://www.sqlite.org
* JSON for Modern C++ https://github.com/nlohmann/json
* linalg https://github.com/sgorsten/linalg
* Tiny OBJ Loader https://github.com/syoyo/tinyobjloader
* LodePNG https://github.com/lvandeve/lodepng
* jpeg-compressor https://github.com/richgel999/jpeg-compressor
* miniz  https://github.com/richgel999/miniz
* GPU Cubic B-Spline Interpolation (optional) in volume shader http://www.dannyruijters.nl/cubicinterpolation/ <sup>2</sup>
* stb_image_resize http://github.com/nothings/stb

---
<sup>1</sup> [Stegman, D.R., Moresi, L., Turnbull, R., Giordani, J., Sunter, P., Lo, A. and S. Quenette, *gLucifer: Next Generation Visualization Framework for High performance computational geodynamics*, 2008, Visual Geosciences](http://dx.doi.org/10.1007/s10069-008-0010-2)  
<sup>2</sup> [Ruijters, Daniel & ter Haar Romeny, Bart & Suetens, Paul. (2008). Efficient GPU-Based Texture Interpolation using Uniform B-Splines. J. Graphics Tools. 13. 61-69.](http://dx.doi.org/10.1080/2151237X.2008.10129269)
