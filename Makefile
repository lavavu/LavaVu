#Install path
PREFIX = bin
APREFIX = $(realpath $(PREFIX))

#Object files path
OPATH = tmp

#Compilers
CPP=g++
CC=gcc

#Default flags
CFLAGS = $(FLAGS) -fPIC -Isrc

# Separate compile options per configuration
ifeq ($(CONFIG),debug)
  CFLAGS += -g -O0
else
  CFLAGS += -O3
endif

#Linux/Mac specific libraries/flags for offscreen & interactive
OS := $(shell uname)
#Offscreen build
ifeq ($(OFFSCREEN), 1)
ifeq ($(OS), Darwin)
  #AGL offscreen config:
  CFLAGS += -FAGL -FOpenGL -I/usr/include/malloc
  LIBS=-ldl -lpthread -framework AGL -framework OpenGL -lobjc -lm -lz
  DEFINES += -DUSE_FONTS -DHAVE_AGL
else
  #OSMesa offscreen config:
  LIBS=-ldl -lpthread -lm -lOSMesa -lz
  DEFINES += -DUSE_FONTS -DHAVE_OSMESA
endif
else
#Interactive build
ifeq ($(OS), Darwin)
  #Mac OS X interactive with GLUT
  CFLAGS += -FGLUT -FOpenGL -I/usr/include/malloc
  LIBS=-ldl -lpthread -framework GLUT -framework OpenGL -lobjc -lm -lz
  DEFINES += -DUSE_FONTS -DHAVE_GLUT
else
  #Linux interactive with X11 (and optional GLUT, SDL)
  LIBS=-ldl -lpthread -lm -lGL -lz -lX11
  DEFINES += -DUSE_FONTS -DHAVE_X11
ifeq ($(GLUT), 1)
  LIBS+= -lglut
  DEFINES += -DHAVE_GLUT
endif
ifeq ($(SDL), 1)
  LIBS+= -lSDL
  DEFINES += -DHAVE_SDL
endif
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

PROGNAME = LavaVu
PROGRAM = $(PREFIX)/$(PROGNAME)
LIBNAME = $(PREFIX)/lib$(PROGNAME).so
INDEX = $(PREFIX)/html/index.html

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
	$(CPP) $(CFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): $(OBJS) $(OBJ2) paths
	$(CPP) -o $(LIBNAME) -shared $(OBJS) $(OBJ2) $(LIBS)
	$(CPP) -o $(PROGRAM) $(LIBS) -lLavaVu -L$(PREFIX) -Wl,-rpath=$(APREFIX)

$(OPATH)/tiny_obj_loader.o : tiny_obj_loader.cc
	$(CPP) $(CFLAGS) -o $@ -c $^ 

$(OPATH)/mongoose.o : mongoose.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

$(OPATH)/sqlite3.o : sqlite3.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

clean:
	/bin/rm -f *~ $(OPATH)/*.o $(PROGRAM) $(LIBNAME)
	/bin/rm $(PREFIX)/html/*
	/bin/rm $(PREFIX)/*.vert
	/bin/rm $(PREFIX)/*.frag

