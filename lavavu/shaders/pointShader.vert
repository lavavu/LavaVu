#ifdef WEBGL
attribute vec3 aVertexPosition;
attribute vec4 aVertexColour;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

const int uPointDist = 1;   // Scale by distance

varying vec4 vColour;

#else
#define aVertexPosition gl_Vertex.xyz
#define aVertexColour gl_Color

#define uMVMatrix gl_ModelViewMatrix
#define uPMatrix gl_ProjectionMatrix

uniform int uPointDist;   // Scale by distance

#define vColour gl_FrontColor
#endif

attribute float aSize;
attribute float aPointType;

uniform float uPointScale;   // scale to calculate size in pixels

uniform vec4 uColour;
uniform float uOpacity;

varying vec3 vVertex;
varying float vPointSize;
varying vec3 vPosEye;
varying float vPointType;

void main(void)
{
  float pSize = abs(aSize);

  // calculate window-space point size
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  vec3 posEye = mvPosition.xyz;
  float dist = 1.0;
  if (uPointDist > 0)
     dist = length(posEye);
  //Limit scaling, overly large points are very slow to render
  //gl_PointSize = max(1.0, min(40.0, uPointScale * pSize / dist));

  gl_PointSize = uPointScale * pSize / dist;
  gl_Position = uPMatrix * mvPosition;

  vPosEye = posEye;
  vPointType = aPointType;
  vPointSize = gl_PointSize;
  vVertex = aVertexPosition.xyz;

  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a);
}

