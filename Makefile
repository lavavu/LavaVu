#Install path
PREFIX = bin
#Object files path
#OPATH = /tmp
OPATH = tmp

#Compilers
CPP=g++
CC=gcc

#Default flags
CFLAGS = -Isrc

# Separate compile options per configuration
ifeq ($(CONFIG),debug)
CFLAGS += -g -O0
else
CFLAGS += -O3
endif

#Linux/Mac specific libraries/flags
ifeq ($(MACHINE), Darwin)
   CFLAGS += -FGLUT -FOpenGL
   LIBS=-ldl -lpthread -framework GLUT -framework OpenGL -lobjc -lm -lz
else
   LIBS=-ldl -lpthread -lm -lGL -lz -lX11 #-lglut -lSDL
#   LIBS=-ldl -lpthread -lm -lGL -lOSMesa -lz
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

#Additional flags for building non-library sources only
DEFINES += -DUSE_FONTS -DUSE_ZLIB -DHAVE_X11
#DEFINES += -DUSE_FONTS -DUSE_ZLIB -DHAVE_X11 #-DHAVE_GLUT -DHAVE_SDL
#DEFINES += -DHAVE_OSMESA -DUSE_FONTS -DUSE_ZLIB

PROGRAM = $(PREFIX)/LavaVu

default: install

install: $(PROGRAM)
	cp src/shaders/*.* $(PREFIX)
	cp -R src/html $(PREFIX)

paths:
	mkdir -p $(OPATH)
	mkdir -p $(PREFIX)

#Rebuild *.cpp
$(OBJS): $(OPATH)/%.o : %.cpp $(INC) paths
	$(CPP) $(CFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): $(OBJS) $(OBJ2) paths
	$(CPP) -o $(PROGRAM) $(OBJS) $(OBJ2) $(LIBS)

$(OPATH)/tiny_obj_loader.o : tiny_obj_loader.cc
	$(CPP) $(CFLAGS) -o $@ -c $^ 

$(OPATH)/mongoose.o : mongoose.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

$(OPATH)/sqlite3.o : sqlite3.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

clean:
	/bin/rm -f *~ $(OPATH)/*.o $(PROGRAM)
	/bin/rm $(PREFIX)/html/*
	/bin/rm $(PREFIX)/LavaVu
	/bin/rm $(PREFIX)/*.vert
	/bin/rm $(PREFIX)/*.frag

