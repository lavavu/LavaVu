#ifdef WEBGL
attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
attribute vec4 aVertexColour;
attribute vec2 aVertexTexCoord;
attribute float aVertexObjectID;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat4 uNMatrix;

varying float vObjectID;

#else
#define aVertexPosition gl_Vertex.xyz
#define aVertexNormal gl_Normal
#define aVertexColour gl_Color
#define aVertexTexCoord gl_MultiTexCoord0.st;

#define uMVMatrix gl_ModelViewMatrix
#define uPMatrix gl_ProjectionMatrix
#define uNMatrix gl_NormalMatrix

#endif

uniform vec4 uColour;

varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec2 vTexCoord;
varying vec3 vVertex;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  vPosEye = vec3(mvPosition);
  gl_Position = uPMatrix * mvPosition;

  vNormal = normalize(mat3(uNMatrix) * aVertexNormal);

  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = aVertexColour;

  vTexCoord = aVertexTexCoord;
#ifdef WEBGL
  vObjectID = aVertexObjectID;
#endif
  vVertex = aVertexPosition;
}

