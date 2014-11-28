####################################################################################################################### 
# Omega Lib build for gLucifer
#---------------------------------------------------------------------------------------------------------------------
# Copyright 2013							Monash University
# Authors:										
#  Owen Kaluza							owen.kaluza@monash.edu
#######################################################################################################################
cmake_minimum_required(VERSION 2.8.1) 
project(FractalViewer)

find_package(Omegalib)


# Source files
SET( srcs
        GLucifer.cpp
        src/GLuciferViewer.cpp
        src/GLuciferServer.cpp
        src/InteractiveViewer.cpp
        src/OpenGLViewer.cpp
        src/base64.cpp
        src/ColourMap.cpp
        src/DrawingObject.cpp
        src/Extensions.cpp
        src/FontSans.cpp
        src/Geometry.cpp
        src/GraphicsUtil.cpp
        src/Lines.cpp
        src/Model.cpp
        src/Points.cpp
        src/QuadSurfaces.cpp
        src/Shaders.cpp
        src/Shapes.cpp
        src/Tracers.cpp
        src/TriSurfaces.cpp
        src/Vectors.cpp
        src/Volumes.cpp
        src/VideoEncoder.cpp
        src/View.cpp
        src/Win.cpp
        src/jpeg/jpge.cpp
        src/mongoose/mongoose.c
        src/sqlite3/src/sqlite3.c
   )
    
#######################################################################################################################
# Headers
SET( headers 
        src/ViewerApp.h
        src/OpenGLViewer.h
   ) 
        
#######################################################################################################################
# Setup compile info

add_definitions(-DUSE_OMEGALIB -DUSE_ZLIB -DHAVE_LIBPNG -DSHADER_PATH="${CMAKE_BINARY_DIR}/src/shaders/")
#add_definitions(-DDEBUG -g)
add_definitions(-O3)

#add_library(common OBJECT ${srcs})

include_directories(${OMEGA_INCLUDE_DIRS})

#add_executable(gLucifer GLucifer.cpp src/GLuciferViewer.cpp src/InteractiveViewer.cpp ${headers} $<TARGET_OBJECTS:common>)
add_executable(gLucifer ${srcs} ${headers})

set_target_properties(gLucifer PROPERTIES FOLDER examples)
#target_link_libraries(gLucifer dl	omega)

target_link_libraries(gLucifer dl z png ${OMEGA_LIB} ${OMEGA_TOOLKIT_LIB})



