attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
attribute vec4 aVertexColour;
attribute vec2 aVertexTexCoord;
attribute float aVertexObjectID;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat4 uNMatrix;

uniform vec4 uColour;

uniform float uAlpha;

varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec2 vTexCoord;
varying vec3 vert;

varying float vObjectID;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
    mvPosition -= 0.00001*aVertexObjectID;
  vPosEye = vec3(mvPosition);
  gl_Position = uPMatrix * mvPosition;
  vNormal = normalize(mat3(uNMatrix) * aVertexNormal);
  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a*uAlpha);

  vTexCoord = aVertexTexCoord;
  vObjectID = aVertexObjectID;
  vert = aVertexPosition;
}


