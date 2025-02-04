in vec4 aVertexPosition;
in vec4 aVertexColour;
in vec2 aVertexTexCoord;
uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
out vec4 vColour;
out vec2 vTexCoord;
flat out vec4 vFlatColour;

void main(void)
{
  gl_Position = uPMatrix * uMVMatrix * aVertexPosition;
  vColour = aVertexColour;
  vTexCoord = aVertexTexCoord;
  vFlatColour = aVertexColour;
}

