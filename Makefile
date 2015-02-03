CFLAGS = -Isrc
# Separate compile options per configuration
ifeq ($(CONFIG),debug)
CFLAGS += -g -O0
else
CFLAGS += -O3
endif

CPP=g++
CC=gcc

ifeq ($(MACHINE), Darwin)
   CFLAGS += -FGLUT -FOpenGL
   LIBS=-ldl -lpthread -framework GLUT -framework OpenGL -lobjc -lm 
else
   #LIBS=-ldl -lpthread -lm -lGL -lGLU -lglut
   #LIBS=-ldl -lpthread -lm -lGL -lGLU -lX11
   LIBS=-ldl -lpthread -lm -lGL -lGLU -lSDL -lX11 -lglut
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
OBJS = $(notdir $(OBJ))
#Only works if make invoked with makefile in cwd
CWD = $(shell pwd)

PROGRAM = gLucifer

default: $(PROGRAM)

install: $(PROGRAM)
	-mkdir bin
	cp $(PROGRAM) bin
	cp src/shaders/*.* bin

#Rebuild *.cpp
$(OBJS): %.o : %.cpp $(INC)
	$(CPP) $(CFLAGS) -DHAVE_SDL -DHAVE_X11 -DHAVE_GLUT -DUSE_FONTS -DSHADER_PATH='"${CWD}/src/shaders/"' -c $< -o $@

$(PROGRAM): $(OBJS) tiny_obj_loader.o mongoose.o sqlite3.o
	$(CPP) -o $(PROGRAM) $(OBJS) tiny_obj_loader.o mongoose.o sqlite3.o $(LIBS)

tiny_obj_loader.o : tiny_obj_loader.cc
	$(CPP) $(CFLAGS) -o $@ -c $^ 

mongoose.o : mongoose.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

sqlite3.o : sqlite3.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

clean:
	/bin/rm -f *~ *.o $(PROGRAM)

