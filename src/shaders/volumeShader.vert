void main(void)
{
   vec4 mvPosition = gl_ModelViewMatrix * gl_Vertex;
   gl_Position = gl_ProjectionMatrix * mvPosition;
}
