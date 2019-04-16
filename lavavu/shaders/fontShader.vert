in vec4 aVertexPosition;
in vec2 aVertexTexCoord;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

uniform vec4 uColour;

out vec4 vColour;
out vec2 vTexCoord;

void main(void)
{
  gl_Position = uPMatrix * uMVMatrix * aVertexPosition;
  vColour = uColour;
  vTexCoord = aVertexTexCoord;
}

