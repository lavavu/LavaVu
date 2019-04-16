in vec4 aVertexPosition;
in vec4 aVertexColour;
uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
out vec4 vColour;
#ifndef WEBGL
flat out vec4 vFlatColour;
#endif
void main(void)
{
  gl_Position = uPMatrix * uMVMatrix * aVertexPosition;
  vColour = aVertexColour;
#ifndef WEBGL
  vFlatColour = aVertexColour;
#endif
}

