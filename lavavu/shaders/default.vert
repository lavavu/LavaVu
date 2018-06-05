varying vec4 vColour;
void main(void)
{
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
  vColour = gl_Color;
}

