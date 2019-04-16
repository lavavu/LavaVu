in vec3 aVertexPosition;
in vec4 aVertexColour;
uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

#ifndef WEBGL
uniform int uPointDist;   // Scale by distance
#endif

uniform vec4 uColour;
uniform float uOpacity;

out vec4 vColour;
out vec3 vVertex;

void main(void)
{
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  gl_Position = uPMatrix * mvPosition;

  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a*uOpacity);

  vVertex = aVertexPosition.xyz;
}

