varying vec4 vColour;
varying vec3 vVertex;

void main(void)
{
  vec4 mvPosition = gl_ModelViewMatrix * gl_Vertex;
  gl_Position = gl_ProjectionMatrix * mvPosition;
  vColour = gl_Color;
  vVertex = gl_Vertex.xyz;
}

