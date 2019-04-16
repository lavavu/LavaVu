#include "GLUtils.h"

const char* glErrorString(GLenum errorCode)
{
  switch (errorCode)
  {
  case GL_NO_ERROR:
    return "No error";
  case GL_INVALID_ENUM:
    return "Invalid enumerant";
  case GL_INVALID_VALUE:
    return "Invalid value";
  case GL_INVALID_OPERATION:
    return "Invalid operation";
  case GL_STACK_OVERFLOW:
    return "Stack overflow";
  case GL_STACK_UNDERFLOW:
    return "Stack underflow";
  case GL_OUT_OF_MEMORY:
    return "Out of memory";
  }
  return "Unknown error";
}

int gluProjectf(float objx, float objy, float objz, float* modelview, float*projection, int* viewport, float *windowCoordinate)
{
  //https://www.opengl.org/wiki/GluProject_and_gluUnProject_code
  //Transformation vectors
  float fTempo[8];
  //Modelview transform
  fTempo[0]=modelview[0]*objx+modelview[4]*objy+modelview[8]*objz+modelview[12];  //w is always 1
  fTempo[1]=modelview[1]*objx+modelview[5]*objy+modelview[9]*objz+modelview[13];
  fTempo[2]=modelview[2]*objx+modelview[6]*objy+modelview[10]*objz+modelview[14];
  fTempo[3]=modelview[3]*objx+modelview[7]*objy+modelview[11]*objz+modelview[15];
  //Projection transform, the final row of projection matrix is always [0 0 -1 0]
  //so we optimize for that.
  fTempo[4]=projection[0]*fTempo[0]+projection[4]*fTempo[1]+projection[8]*fTempo[2]+projection[12]*fTempo[3];
  fTempo[5]=projection[1]*fTempo[0]+projection[5]*fTempo[1]+projection[9]*fTempo[2]+projection[13]*fTempo[3];
  fTempo[6]=projection[2]*fTempo[0]+projection[6]*fTempo[1]+projection[10]*fTempo[2]+projection[14]*fTempo[3];
  fTempo[7]=-fTempo[2];
  //The result normalizes between -1 and 1
  if(fTempo[7]==0.0)   //The w value
    return 0;
  fTempo[7]=1.0/fTempo[7];
  //Perspective division
  fTempo[4]*=fTempo[7];
  fTempo[5]*=fTempo[7];
  fTempo[6]*=fTempo[7];
  //Window coordinates
  //Map x, y to range 0-1
  windowCoordinate[0]=(fTempo[4]*0.5+0.5)*viewport[2]+viewport[0];
  windowCoordinate[1]=(fTempo[5]*0.5+0.5)*viewport[3]+viewport[1];
  //This is only correct when glDepthRange(0.0, 1.0)
  windowCoordinate[2]=(1.0+fTempo[6])*0.5;   //Between 0 and 1
  return 1;
}

