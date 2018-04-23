//version 130
//flat varying vec4 vColour;
#version 120
varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec3 vVertex;
varying vec2 vTextureCoord;

void main(void)
{
  vec4 mvPosition = gl_ModelViewMatrix * gl_Vertex;
  vPosEye = vec3(mvPosition);
  gl_Position = gl_ProjectionMatrix * mvPosition;

  vNormal = normalize(mat3(gl_NormalMatrix) * gl_Normal);
 
  vTextureCoord = gl_MultiTexCoord0.st;
  vColour = gl_Color;
  vVertex = gl_Vertex.xyz;
}

