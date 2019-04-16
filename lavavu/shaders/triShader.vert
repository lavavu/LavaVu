attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
attribute vec4 aVertexColour;
attribute vec2 aVertexTexCoord;
#ifdef WEBGL
attribute float aVertexObjectID;
varying float vObjectID;
#else
flat varying vec4 vFlatColour;
#endif

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat4 uNMatrix;

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
#else
  vFlatColour = vColour;
#endif
  vVertex = aVertexPosition;
}

