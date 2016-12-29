//version 130
//flat varying vec4 vColour;
#version 120
varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec3 vVertex;
uniform bool uCalcNormal;

void main(void)
{
   vec4 mvPosition = gl_ModelViewMatrix * gl_Vertex;
   vPosEye = vec3(mvPosition);
   gl_Position = gl_ProjectionMatrix * mvPosition;

  if (uCalcNormal || dot(gl_Normal,gl_Normal) < 0.01)
    vNormal = vec3(0.0);
  else
    vNormal = normalize(mat3(gl_NormalMatrix) * gl_Normal);
 
   gl_TexCoord[0] = gl_MultiTexCoord0;
   vColour = gl_Color;
   vVertex = gl_Vertex.xyz;
}


