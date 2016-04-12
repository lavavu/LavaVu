#Install path
PREFIX = bin
APREFIX = $(realpath $(PREFIX))
PROGNAME = LavaVu
PROGRAM = $(PREFIX)/$(PROGNAME)
LIBNAME = lib$(PROGNAME).$(LIBEXT)
INDEX = $(PREFIX)/html/index.html

#Object files path
#OPATH = /tmp
OPATH = tmp

#Compilers
CPP=g++
CC=gcc

#Default flags
CFLAGS = $(FLAGS) -fPIC -Isrc
CPPFLAGS = $(CFLAGS) -std=c++0x

# Separate compile options per configuration
ifeq ($(CONFIG),debug)
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O3 -DNDEBUG
endif

#Linux/Mac specific libraries/flags for offscreen & interactive
OS := $(shell uname)
ifeq ($(OS), Darwin)
  #Mac OS X with Cocoa + CGL
  CFLAGS += -FCocoa -FOpenGL -I/usr/include/malloc -stdlib=libc++
  LIBS=-lc++ -ldl -lpthread -framework Cocoa -framework Quartz -framework OpenGL -lobjc -lm -lz
  DEFINES += -DUSE_FONTS -DHAVE_COCOA -DHAVE_CGL
  LIBEXT=dylib
  LIBBUILD=-dynamiclib
  LIBINSTALL=-dynamiclib -install_name @rpath/lib$(PROGNAME).$(LIBEXT)
  LIBLINK=-Wl,-rpath $(APREFIX)
  APPLEOBJ=$(OPATH)/CocoaViewer.o
else
  #Linux with X11 (and optional GLUT, SDL)
  LIBS=-ldl -lpthread -lm -lGL -lz -lX11
  DEFINES += -DUSE_FONTS -DHAVE_X11
  LIBEXT=so
  LIBBUILD=-shared
  LIBLINK=-Wl,-rpath=$(APREFIX)
ifeq ($(GLUT), 1)
  LIBS+= -lglut
  DEFINES += -DHAVE_GLUT
endif
ifeq ($(SDL), 1)
  LIBS+= -lSDL
  DEFINES += -DHAVE_SDL
endif
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
ifeq ($(PNG), 1)
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
vpath %.cpp src:src/Main:src:src/jpeg
vpath %.h src/Main:src:src/jpeg:src/sqlite3
vpath %.c src/mongoose:src/sqlite3/src
vpath %.cc src

SRC := $(wildcard src/*.cpp) $(wildcard src/Main/*.cpp) $(wildcard src/jpeg/*.cpp)

INC := $(wildcard src/*.h)
#INC := $(SRC:%.cpp=%.h)
OBJ := $(SRC:%.cpp=%.o)
#Strip paths (src) from sources
OBJS = $(notdir $(OBJ))
#Add object path
OBJS := $(OBJS:%.o=$(OPATH)/%.o)
#Additional library objects (no cpp extension so not included above)
OBJ2 = $(OPATH)/tiny_obj_loader.o $(OPATH)/mongoose.o $(OPATH)/sqlite3.o

default: install

install: paths $(PROGRAM)
	cp src/shaders/*.* $(PREFIX)
	cp -R src/html/*.js $(PREFIX)/html
	cp -R src/html/*.css $(PREFIX)/html
	/bin/bash build-index.sh src/html/index.html $(PREFIX)/html/index.html src/shaders

paths:
	mkdir -p $(OPATH)
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/html

#Rebuild *.cpp
$(OBJS): $(OPATH)/%.o : %.cpp $(INC)
	$(CPP) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): $(OBJS) $(OBJ2) $(APPLEOBJ) paths
	$(CPP) -o $(PREFIX)/lib$(PROGNAME).$(LIBEXT) $(LIBBUILD) $(LIBINSTALL) $(OBJS) $(OBJ2) $(APPLEOBJ) $(LIBS)
	$(CPP) -o $(PROGRAM) $(LIBS) -lLavaVu -L$(PREFIX) $(LIBLINK)

$(OPATH)/tiny_obj_loader.o : tiny_obj_loader.cc
	$(CPP) $(CPPFLAGS) -o $@ -c $^ 

$(OPATH)/mongoose.o : mongoose.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

$(OPATH)/sqlite3.o : sqlite3.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

$(OPATH)/CocoaViewer.o : src/Main/CocoaViewer.mm
	$(CPP) $(CPPFLAGS) $(DEFINES) -o $@ -c $^ 

swig: $(PREFIX)/$(LIBNAME)
	swig -v -Wextra -python -ignoremissing -O -c++ -DSWIG_DO_NOT_WRAP LavaVu.i
	mv LavaVu.py bin
	#touch bin/__init__.py
	$(CPP) $(CPPFLAGS) `python-config --cflags` -c LavaVu_wrap.cxx -o $(OPATH)/LavaVu_wrap.os
	$(CPP) -o $(PREFIX)/_$(PROGNAME).so $(LIBBUILD) $(OPATH)/LavaVu_wrap.os `python-config --ldflags` -lLavaVu -L$(PREFIX) $(LIBLINK)

clean:
	/bin/rm -f *~ $(OPATH)/*.o $(PROGRAM) $(LIBNAME)
	/bin/rm $(PREFIX)/html/*
	/bin/rm $(PREFIX)/*.vert
	/bin/rm $(PREFIX)/*.frag

