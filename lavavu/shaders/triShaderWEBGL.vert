attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
attribute vec4 aVertexColour;
attribute vec2 aVertexTexCoord;
attribute float aVertexObjectID;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat4 uNMatrix;

uniform vec4 uColour;
uniform bool uCalcNormal;

varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec2 vTexCoord;
varying vec3 vVertex;

varying float vObjectID;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
    mvPosition -= 0.00001*aVertexObjectID;
  vPosEye = vec3(mvPosition);
  gl_Position = uPMatrix * mvPosition;

  if (uCalcNormal || dot(aVertexNormal,aVertexNormal) < 0.01)
    vNormal = vec3(0.0);
  else
    vNormal = normalize(mat3(uNMatrix) * aVertexNormal);

  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = aVertexColour;

  vTexCoord = aVertexTexCoord;
  vObjectID = aVertexObjectID;
  vVertex = aVertexPosition;
}


