attribute vec3 aVertexPosition;
attribute vec4 aVertexColour;
attribute float aVertexSize;
attribute float aPointType;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

uniform float uPointScale;
uniform float uOpacity;
uniform vec4 uColour;

varying vec4 vColour;
varying vec3 vPosEye;
varying float vPointType;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  vPosEye = vec3(mvPosition);
  gl_Position = uPMatrix * mvPosition;
  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a*uOpacity);
  // calculate window-space point size
  float eyeDist = length(mvPosition);
  float size = max(1.0, aVertexSize);
  gl_PointSize = uPointScale * size / eyeDist;
  vPointType = aPointType;
}


