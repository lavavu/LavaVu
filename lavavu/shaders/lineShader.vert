#ifdef WEBGL
attribute vec3 aVertexPosition;
attribute vec4 aVertexColour;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

#else
#define aVertexPosition gl_Vertex.xyz
#define aVertexColour gl_Color

#define uMVMatrix gl_ModelViewMatrix
#define uPMatrix gl_ProjectionMatrix

uniform int uPointDist;   // Scale by distance
#endif

uniform vec4 uColour;
uniform float uOpacity;

varying vec4 vColour;
varying vec3 vVertex;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  gl_Position = uPMatrix * mvPosition;

  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a*uOpacity);

  vVertex = aVertexPosition.xyz;
}

