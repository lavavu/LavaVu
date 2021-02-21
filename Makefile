#Install path
PREFIX ?= lavavu
PROGNAME = LavaVu
PROGRAM = $(PREFIX)/$(PROGNAME)
LIBRARY = $(PREFIX)/lib$(PROGNAME).$(LIBEXT)
SWIGLIB = $(PREFIX)/_$(PROGNAME)Python.so

PYTHON ?= python
PYLIB ?= -L$(shell $(PYTHON) -c 'from distutils import sysconfig; print(sysconfig.get_python_lib())')
PYINC ?= -I$(shell $(PYTHON) -c 'from distutils import sysconfig; print(sysconfig.get_python_inc())')

#Object files path
OPATH ?= /tmp
#HTML files path
HTMLPATH = $(PREFIX)/html
#Use OSMESA?
ifeq ($(LV_OSMESA), 1)
OSMESA = 1
endif

#Compilers
CXX ?= g++
CC ?= gcc

#Default flags
DEFINES = -DUSE_ZLIB
CFLAGS = $(FLAGS) -fPIC -Isrc -Wall
CPPFLAGS = $(CFLAGS) -std=c++0x
#For external dependencies
EXTCFLAGS = -O3 -DNDEBUG -fPIC -Isrc
SWIGFLAGS=

#Default to X11 enabled unless explicitly disabled
X11 ?= 1

# Separate compile options per configuration
ifeq ($(CONFIG),debug)
  CPPFLAGS += -g -O0 -DDEBUG
else
  CPPFLAGS += -O3 -DNDEBUG
endif

#Linux/Mac specific libraries/flags for offscreen & interactive
OS := $(shell uname)
ifeq ($(OS), Darwin)
  SWIGFLAGS= -undefined suppress -flat_namespace
  CPPFLAGS += -Wno-unknown-warning-option -Wno-c++14-extensions -Wno-shift-negative-value
  #Mac OS X with Cocoa + CGL
  CPPFLAGS += -FCocoa -FOpenGL -stdlib=libc++
  LIBS=-lc++ -ldl -lpthread -framework Cocoa -framework Quartz -framework OpenGL -lobjc -lm -lz
  DEFINES += -DUSE_FONTS -DHAVE_CGL
  APPLEOBJ=$(OPATH)/CocoaViewer.o
  LIBEXT=dylib
  LIBBUILD=-dynamiclib
  LIBINSTALL=-install_name @loader_path/lib$(PROGNAME).$(LIBEXT)
else
  #Linux 
  LIBS=-ldl -lpthread -lm -lz
  DEFINES += -DUSE_FONTS
  LIBEXT=so
  LIBBUILD=-shared
  LIBLINK=-Wl,-rpath='$$ORIGIN'
#Which window type?
ifeq ($(GLFW), 1)
  #GLFW optional
  LIBS+= -lGL -lglfw
  DEFINES += -DHAVE_GLFW
else ifeq ($(EGL), 1)

#EGL (optionally with GLESv2)
ifeq ($(GLES2), 1)
  LIBS+= -lEGL -lGLESv2
  DEFINES += -DHAVE_EGL -DGLES2
else
  LIBS+= -lEGL -lOpenGL
  DEFINES += -DHAVE_EGL
endif #GLES2

else ifeq ($(OSMESA), 1)
  #OSMesa
  LIBS+= -lOSMesa
  DEFINES += -DHAVE_OSMESA
else ifeq ($(X11), 1)
  #X11
  LIBS+= -lGL -lX11
  DEFINES += -DHAVE_X11
endif
endif #Linux

#Extra defines passed
DEFINES += $(DEFS)
ifdef SHADER_PATH
DEFINES += -DSHADER_PATH=\"$(SHADER_PATH)\"
endif

#Use LV_LIB_DIRS and LV_INC_DIRS variables if defined
ifdef LV_LIB_DIRS
RP=-Wl,-rpath=
LIBS += -L$(subst :, -L,${LV_LIB_DIRS})
LIBS += ${RP}$(subst :, ${RP},${LV_LIB_DIRS})
endif
ifdef LV_INC_DIRS
CPPFLAGS += -I$(subst :, -I,${LV_INC_DIRS})
endif

#Other optional components
ifeq ($(VIDEO), 1)
  CPPFLAGS += -DHAVE_LIBAVCODEC -DHAVE_SWSCALE
  LIBS += -lavcodec -lavutil -lavformat -lswscale
endif
#Default libpng disabled, use built in png support
LIBPNG ?= 0
ifeq ($(LIBPNG), 1)
  CPPFLAGS += -DHAVE_LIBPNG
  LIBS += -lpng
endif
ifeq ($(TIFF), 1)
  CPPFLAGS += -DHAVE_LIBTIFF
  LIBS += -ltiff
endif

#Source search paths
vpath %.cpp src:src/Main:src:src/jpeg:src/png
vpath %.h src/Main:src:src/jpeg:src/png:src/sqlite3
vpath %.c src/sqlite3
vpath %.cc src

#Always run this script to update version.cpp if git version changes
#When releasing, just build with the clean version from setup.py
ifneq ($(LV_RELEASE), 1)
#Otherwise update version using git tag
TMP := $(shell $(PYTHON) _getversion.py)
endif

SRC := $(wildcard src/*.cpp) $(wildcard src/Main/*Viewer.cpp) $(wildcard src/jpeg/*.cpp)
INC := $(wildcard src/*.h) $(wildcard src/Main/*.h)

ifneq ($(LIBPNG), 1)
	SRC += $(wildcard src/png/*.cpp)
	INC += $(wildcard src/png/*.h)
endif

OBJ := $(SRC:%.cpp=%.o)
#Strip paths (src) from sources
OBJS = $(notdir $(OBJ))
#Add object path
OBJS := $(OBJS:%.o=$(OPATH)/%.o)
ALLOBJS := $(OBJS)
#Additional library objects (no cpp extension so not included above)
ALLOBJS += $(OPATH)/sqlite3.o
#Mac only
ALLOBJS += $(APPLEOBJ)

default: install

.PHONY: install
install: $(PROGRAM) $(SWIGLIB)
	@if [ $(PREFIX) != "lavavu" ]; then \
	cp -R lavavu/*.py $(PREFIX); \
	cp -R lavavu/shaders/*.* $(PREFIX)/shaders; \
	cp -R lavavu/html/*.* $(HTMLPATH); \
	cp lavavu/font.bin $(PREFIX)/; \
	cp lavavu/dict.json $(PREFIX)/; \
	fi

.PHONY: force
$(OPATH)/compiler_flags: force | paths
	@echo '$(CPPFLAGS)' | cmp -s - $@ || echo '$(CPPFLAGS)' > $@

.PHONY: paths
paths:
	@mkdir -p $(OPATH)
	@mkdir -p $(PREFIX)
	@mkdir -p $(HTMLPATH)
	@mkdir -p $(PREFIX)/shaders

#Rebuild *.cpp
$(OBJS): $(OPATH)/%.o : %.cpp $(OPATH)/compiler_flags $(INC) | src/sqlite3/sqlite3.c
	$(CXX) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): $(LIBRARY) main.cpp | paths
	$(CXX) $(CPPFLAGS) $(DEFINES) -c src/Main/main.cpp -o $(OPATH)/main.o
	$(CXX) -o $(PROGRAM) $(OPATH)/main.o $(LIBS) -l$(PROGNAME) -L$(PREFIX) $(LIBLINK)

$(LIBRARY): $(ALLOBJS) | paths
	$(CXX) -o $(LIBRARY) $(LIBBUILD) $(LIBINSTALL) $(ALLOBJS) $(LIBS)

#Emscripten build steps...
#curl -L https://github.com/emscripten-core/emsdk/archive/2.0.14.tar.gz --output emscripten.tar.gz
#tar -zxf emscripten.tar.gz
#rm emscripten.tar.gz
#cd emsdk-2.0.14/
#./emsdk install latest
#./emsdk activate latest
#source emsdk_env.sh
#cd ../LavaVu
#make emscripten -j5
#source ~/emsdk-2.0.14/emsdk_env.sh
emscripten: DEFINES = -DGLES2 -DHAVE_GLFW -DUSE_FONTS
emscripten: CXX = em++
emscripten: CC = emcc
emscripten: LIBS = -ldl -lpthread -lm -lGL -lglfw
emscripten: LINKFLAGS = -s USE_WEBGL2=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s DISABLE_EXCEPTION_CATCHING=0 -s LEGACY_GL_EMULATION=0 -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -s EXTRA_EXPORTED_RUNTIME_METHODS=[FS]
#emscripten:  -s MODULARIZE=1 -s 'EXPORT_NAME="createLavaVuModule"' #Would need this to load multiple instances on a page without IFrames
emscripten: LINKFLAGS += --preload-file lavavu/dict.json@dict.json --preload-file lavavu/shaders@/shaders  --preload-file lavavu/font.bin@font.bin
EM_FLAGS =
#EM_FLAGS = -pthread
ifeq ($(CONFIG),debug)
	#Debugging - better detection of segfaults etc
	EM_FLAGS = -fsanitize=address -s INITIAL_MEMORY=300MB
#EM_FLAGS = -fsanitize=undefined
#EM_FLAGS = -s WASM=0
endif
emscripten: CPPFLAGS += $(EM_FLAGS)
emscripten: EXTCFLAGS += $(EM_FLAGS)
emscripten: LINKFLAGS += $(EM_FLAGS)
emscripten: $(ALLOBJS) $(OPATH)/sqlite3.o $(OPATH)/miniz.o lavavu/html/emscripten.js lavavu/html/webview.html | paths
	$(CXX) $(CPPFLAGS) -c src/Main/main.cpp -o $(OPATH)/main.o
	$(CXX) -o $(PREFIX)/html/$(PROGNAME).html $(OPATH)/main.o $(LIBS) $(ALLOBJS) $(OPATH)/miniz.o $(LINKFLAGS)
	cd lavavu/html; sed 's/LAVAVU_VERSION/$(LV_VERSION)/g' webview-template.html > webview.html
	cd lavavu/html; sed 's/LAVAVU_VERSION/$(LV_VERSION)/g' emscripten-template.js > emscripten.js
	cd lavavu; python amalgamate.py

lavavu/html/emscripten.js: lavavu/html/emscripten-template.js FORCE
	cd lavavu/html; sed 's/LAVAVU_VERSION/$(LV_VERSION)/g' emscripten-template.js > emscripten.js

lavavu/html/webview.html: lavavu/html/webview-template.html FORCE
	cd lavavu/html; sed 's/LAVAVU_VERSION/$(LV_VERSION)/g' webview-template.html > webview.html

.PHONY: FORCE
FORCE:

src/sqlite3/sqlite3.c :
	#Ensure the submodule is checked out
	git submodule update --init

$(OPATH)/miniz.o : src/miniz/miniz.c
	$(CC) $(EXTCFLAGS) -o $@ -c $^

$(OPATH)/sqlite3.o : src/sqlite3/sqlite3.c
	$(CC) $(EXTCFLAGS) -DSQLITE_ENABLE_DESERIALIZE -o $@ -c $^ 

$(OPATH)/CocoaViewer.o : src/Main/CocoaViewer.mm
	$(CXX) $(CPPFLAGS) $(DEFINES) -o $@ -c $^ 

#Python interface
SWIGSRC = src/LavaVuPython_wrap.cxx
SWIGOBJ = $(OPATH)/LavaVuPython_wrap.os
NUMPYINC = $(shell $(PYTHON) -c 'import numpy; print(numpy.get_include())')
ifneq ($(NUMPYINC),)
#Skip build if numpy not found
SWIGOBJ = $(OPATH)/LavaVuPython_wrap.os
endif

.PHONY: swig
swig : $(INC) src/LavaVuPython.i | paths
	swig -v -Wextra -python -py3 -ignoremissing -O -c++ -DSWIG_DO_NOT_WRAP -outdir $(PREFIX) src/LavaVuPython.i

$(SWIGLIB) : $(LIBRARY) $(SWIGOBJ)
	-rm -f $(PREFIX)/_LavaVuPython.*
	$(CXX) -o $(SWIGLIB) $(LIBBUILD) $(SWIGOBJ) $(SWIGFLAGS) ${PYLIB} -l$(PROGNAME) -L$(PREFIX) $(LIBLINK)

$(SWIGOBJ) : $(SWIGSRC) | src/sqlite3/sqlite3.c
	$(CXX) $(CPPFLAGS) ${PYINC} -c $(SWIGSRC) -o $(SWIGOBJ) -I$(NUMPYINC)

LV_VERSION = $(shell $(PYTHON) -c 'import setup; print(setup.version)')
docs: src/LavaVu.cpp src/Session.h src/version.cpp
	$(PROGRAM) -S -h -p0 : docs:properties quit > docs/src/Property-Reference.md
	$(PROGRAM) -S -h -p0 : docs:interaction quit > docs/src/Interaction.md
	$(PROGRAM) -S -h -p0 : docs:scripting quit > docs/src/Scripting-Commands-Reference.md
	$(PROGRAM) -? > docs/src/Commandline-Arguments.md
	#pip install -r docs/src/requirements.txt
	#Update binder links
	sed -i 's/gh\/lavavu\/LavaVu\/[0-9\.]*/gh\/lavavu\/LavaVu\/$(LV_VERSION)/g' README.md
	sed -i 's/gh\/lavavu\/LavaVu\/[0-9\.]*/gh\/lavavu\/LavaVu\/$(LV_VERSION)/g' docs/src/Installation.rst
	sed -i 's/gh\/lavavu\/LavaVu\/[0-9\.]*/gh\/lavavu\/LavaVu\/$(LV_VERSION)/g' docs/src/index.rst
	sed -i 's/lavavu:[0-9\.]*/lavavu:$(LV_VERSION)/g' binder/Dockerfile
	sphinx-build docs/src docs/

clean:
	-rm -f *~ $(OPATH)/*.o
	-rm -f $(PREFIX)/*.so
	-rm -f $(PREFIX)/$(PROGNAME)
	@if [ $(PREFIX) != "lavavu" ]; then \
	-rm -rf $(PREFIX)/html; \
	-rm -rf $(PREFIX)/shaders; \
	-rm -f $(PREFIX)/*.py; \
	fi

