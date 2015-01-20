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

SRC := $(wildcard src/*.cpp) $(wildcard src/Main/*.cpp) $(wildcard src/jpeg/*.cpp)
#SRC := GLuciferViewer.cpp GLuciferServer.cpp InteractiveViewer.cpp OpenGLViewer.cpp base64.cpp ColourMap.cpp DrawingObject.cpp Extensions.cpp FontSans.cpp Geometry.cpp GraphicsUtil.cpp Lines.cpp Model.cpp Points.cpp QuadSurfaces.cpp Shaders.cpp Shapes.cpp Tracers.cpp TriSurfaces.cpp Vectors.cpp Volumes.cpp VideoEncoder.cpp View.cpp Win.cpp jpge.cpp X11Viewer.cpp GlutViewer.cpp SDLViewer.cpp main.cpp

INC := $(wildcard src/*.h)
#INC := $(SRC:%.cpp=%.h)
OBJ := $(SRC:%.cpp=%.o)

PROGRAM = gLucifer

default: $(PROGRAM)

install: $(PROGRAM)
	-mkdir bin
	cp $(PROGRAM) bin
	cp src/shaders/*.* bin

#Rebuild *.cpp
$(OBJ): %.o : %.cpp $(INC)
	$(CPP) $(CFLAGS) -DHAVE_SDL -DHAVE_X11 -DHAVE_GLUT -DUSE_FONTS -DSHADER_PATH='"src/shaders/"' -c $< -o $@

$(PROGRAM): $(OBJ) mongoose.o sqlite3.o
	echo $(OBJ)
	$(CPP) -o $(PROGRAM) $(OBJ) mongoose.o sqlite3.o $(LIBS)

mongoose.o : mongoose.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

sqlite3.o : sqlite3.c
	$(CC) $(CFLAGS) -o $@ -c $^ 

clean:
	/bin/rm -f *~ *.o $(PROGRAM)

