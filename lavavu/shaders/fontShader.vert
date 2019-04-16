attribute vec4 aVertexPosition;
attribute vec2 aVertexTexCoord;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

uniform vec4 uColour;

varying vec4 vColour;
varying vec2 vTexCoord;

void main(void)
{
  gl_Position = uPMatrix * uMVMatrix * aVertexPosition;
  vColour = uColour;
  vTexCoord = aVertexTexCoord;
}

