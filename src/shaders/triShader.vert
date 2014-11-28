#version 120
varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;

void main(void)
{
   vec4 mvPosition = gl_ModelViewMatrix * gl_Vertex;
   vPosEye = vec3(mvPosition);
   gl_Position = gl_ProjectionMatrix * mvPosition;
   vNormal = normalize(mat3(gl_NormalMatrix) * gl_Normal);

   gl_TexCoord[0] = gl_MultiTexCoord0;
   vColour = gl_Color;
}


