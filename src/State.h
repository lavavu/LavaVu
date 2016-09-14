#ifndef State__
#define State__

#include "DrawState.h"
#include "Geometry.h"
#include "OutputInterface.h"
#include "InputInterface.h"

//Class to hold all global state
class State
{
public:
//DrawState
  DrawState drawstate;

//Model
  int now;
  //Current timestep geometry
  std::vector<Geometry*> geometry;
  //Type specific geometry pointers
  Geometry* labels;
  Points* points;
  Vectors* vectors;
  Tracers* tracers;
  QuadSurfaces* quadSurfaces;
  TriSurfaces* triSurfaces;
  Lines* lines;
  Shapes* shapes;
  Volumes* volumes;

//TimeStep
  std::vector<Geometry*> fixed;     //Static geometry
  int cachesize;

  State()
  {
    reset();
  }

  void reset()
  {
    //Active geometry containers, shared by all models for fast switching/drawing
    labels = NULL;
    points = NULL;
    vectors = NULL;
    tracers = NULL;
    quadSurfaces = NULL;
    triSurfaces = NULL;
    lines = NULL;
    shapes = NULL;
    volumes = NULL;

    cachesize = 0;
    now = -1;

    drawstate.reset();
  }

  void loadFixed()
  {
    //Insert fixed geometry records
    if (fixed.size() > 0) 
      for (unsigned int i=0; i<geometry.size(); i++)
        geometry[i]->insertFixed(fixed[i]);
  }

};

#endif // State__

