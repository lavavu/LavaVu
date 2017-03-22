#Install path
PREFIX ?= bin
APREFIX = $(realpath $(PREFIX))
PROGNAME = LavaVu
PROGRAM = $(PREFIX)/$(PROGNAME)
LIBRARY = $(PREFIX)/lib$(PROGNAME).$(LIBEXT)
SWIGLIB = $(PREFIX)/_$(PROGNAME)Python.so

#Object files path
OPATH ?= tmp
#HTML files path
HTMLPATH = $(PREFIX)/html

#Compilers
CPP=g++
CC=gcc
SWIG=$(shell command -v swig 2> /dev/null)

#Default flags
CFLAGS = $(FLAGS) -fPIC -Isrc
CPPFLAGS = $(CFLAGS) -std=c++0x
#For external dependencies
EXTCFLAGS = -O3 -DNDEBUG -fPIC -Isrc
EXTCPPFLAGS = $(EXTCFLAGS) -std=c++0x
SWIGFLAGS=

#Default to X11 enabled unless explicitly disabled
X11 ?= 1

# Separate compile options per configuration
ifeq ($(CONFIG),debug)
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O3 -DNDEBUG
endif

#Linux/Mac specific libraries/flags for offscreen & interactive
OS := $(shell uname)
ifeq ($(OS), Darwin)
  SWIGFLAGS= -undefined suppress -flat_namespace
  CFLAGS += -Wno-c++14-extensions -Wno-shift-negative-value
  #Mac OS X with Cocoa + CGL
  CFLAGS += -FCocoa -FOpenGL -I/usr/include/malloc -stdlib=libc++
  LIBS=-lc++ -ldl -lpthread -framework Cocoa -framework Quartz -framework OpenGL -lobjc -lm -lz
  DEFINES += -DUSE_FONTS -DHAVE_CGL
  APPLEOBJ=$(OPATH)/CocoaViewer.o
  LIBEXT=dylib
  LIBBUILD=-dynamiclib
  LIBINSTALL=-dynamiclib -install_name @rpath/lib$(PROGNAME).$(LIBEXT)
  LIBLINK=-Wl,-rpath $(APREFIX)
else
  #Linux 
  LIBS=-ldl -lpthread -lm -lGL -lz
  DEFINES += -DUSE_FONTS
  LIBEXT=so
  LIBBUILD=-shared
  LIBLINK=-Wl,-rpath=$(APREFIX)
ifeq ($(GLUT), 1)
  #GLUT optional
  LIBS+= -lglut
  DEFINES += -DHAVE_GLUT
else ifeq ($(SDL), 1)
  #SDL optional
  LIBS+= -lSDL
  DEFINES += -DHAVE_SDL
else ifeq ($(X11), 1)
  #X11 default
  LIBS+= -lX11
  DEFINES += -DHAVE_X11
else
  #Assume providing own context
  DEFINES += -DHAVE_GLCONTEXT
endif
endif

#Extra defines passed
DEFINES += $(DEFS)
ifdef SHADER_PATH
DEFINES += -DSHADER_PATH=\"$(SHADER_PATH)\"
endif

#Add a libpath (useful for linking specific libGL)
ifdef LIBDIR
  LIBS+= -L$(LIBDIR) -Wl,-rpath=$(LIBDIR)
endif

#Other optional components
ifeq ($(VIDEO), 1)
  CFLAGS += -DHAVE_LIBAVCODEC -DHAVE_SWSCALE
  LIBS += -lavcodec -lavutil -lavformat -lswscale
endif
#Default libpng disabled, use built in png support
LIBPNG ?= 0
ifeq ($(LIBPNG), 1)
  CFLAGS += -DHAVE_LIBPNG
  LIBS += -lpng
else
  CFLAGS += -DUSE_ZLIB
endif
ifeq ($(TIFF), 1)
  CFLAGS += -DHAVE_LIBTIFF
  LIBS += -ltiff
endif

#Source search paths
vpath %.cpp src:src/Main:src:src/jpeg:src/png
vpath %.h src/Main:src:src/jpeg:src/png:src/sqlite3
vpath %.c src/mongoose:src/sqlite3
vpath %.cc src

SRC := $(wildcard src/*.cpp) $(wildcard src/Main/*Viewer.cpp) $(wildcard src/jpeg/*.cpp) $(wildcard src/png/*.cpp)

INC := $(wildcard src/*.h)
#INC := $(SRC:%.cpp=%.h)
OBJ := $(SRC:%.cpp=%.o)
#Strip paths (src) from sources
OBJS = $(notdir $(OBJ))
#Add object path
OBJS := $(OBJS:%.o=$(OPATH)/%.o)
ALLOBJS := $(OBJS)
#Additional library objects (no cpp extension so not included above)
ALLOBJS += $(OPATH)/mongoose.o $(OPATH)/sqlite3.o
#Mac only
ALLOBJS += $(APPLEOBJ)

default: install

.PHONY: install
install: $(PROGRAM) swig
	cp src/shaders/*.* $(PREFIX)
	cp -R src/html/*.js $(HTMLPATH)
	cp -R src/html/*.css $(HTMLPATH)
	/bin/bash build-index.sh src/html/index.html $(HTMLPATH)/index.html src/shaders

.PHONY: force
$(OPATH)/compiler_flags: force
	@echo '$(CPPFLAGS)' | cmp -s - $@ || echo '$(CPPFLAGS)' > $@

.PHONY: paths
paths:
	@mkdir -p $(OPATH)
	@mkdir -p $(PREFIX)
	@mkdir -p $(HTMLPATH)

#Rebuild *.cpp
$(OBJS): $(OPATH)/%.o : %.cpp $(OPATH)/compiler_flags $(INC) | paths
	$(CPP) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): $(LIBRARY) main.cpp | paths
	$(CPP) $(CPPFLAGS) $(DEFINES) -c src/Main/main.cpp -o $(OPATH)/main.o
	$(CPP) -o $(PROGRAM) $(OPATH)/main.o $(LIBS) -lLavaVu -L$(PREFIX) $(LIBLINK)

$(LIBRARY): $(ALLOBJS) | paths
	$(CPP) -o $(LIBRARY) $(LIBBUILD) $(LIBINSTALL) $(ALLOBJS) $(LIBS)

$(OPATH)/mongoose.o : mongoose.c
	$(CC) $(EXTCFLAGS) -o $@ -c $^ 

$(OPATH)/sqlite3.o : sqlite3.c
	$(CC) $(EXTCFLAGS) -o $@ -c $^ 

$(OPATH)/CocoaViewer.o : src/Main/CocoaViewer.mm
	$(CPP) $(CPPFLAGS) $(DEFINES) -o $@ -c $^ 

swig: $(SWIGLIB)

ifeq ($(SWIG),)
$(SWIGLIB) : $(LIBRARY)
	@echo "*** Python interface build requires Swig ***"
else

SWIGSRC = LavaVuPython_wrap.cxx
SWIGOBJ = $(OPATH)/LavaVuPython_wrap.os

$(SWIGLIB) : $(LIBRARY) $(SWIGOBJ)
	$(CPP) -o $(SWIGLIB) $(LIBBUILD) $(SWIGOBJ) $(SWIGFLAGS) `python-config --ldflags` -lLavaVu -L$(PREFIX) $(LIBLINK)

$(SWIGOBJ) : $(SWIGSRC)
	$(CPP) $(CPPFLAGS) `python-config --cflags` -c $(SWIGSRC) -o $(SWIGOBJ)

$(SWIGSRC) : $(INC) LavaVuPython.i | paths
	$(SWIG) -v -Wextra -python -ignoremissing -O -c++ -DSWIG_DO_NOT_WRAP LavaVuPython.i
endif

docs: src/LavaVu.cpp src/DrawState.h
	python docparse.py
	bin/LavaVu -S -h -p0 : docs:interaction quit > docs/Interaction.md
	bin/LavaVu -S -h -p0 : docs:scripting quit > docs/Scripting-Commands-Reference.md
	bin/LavaVu -? > docs/Commandline-Arguments.md

clean:
	-rm -f *~ $(OPATH)/*.o
	-rm -rf $(PREFIX)

