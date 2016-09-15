attribute vec3 aVertexPosition;
attribute vec4 aVertexColour;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

uniform vec4 uColour;
uniform float uOpacity;

varying vec4 vColour;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  gl_Position = uPMatrix * mvPosition;
  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a*uOpacity);
}


