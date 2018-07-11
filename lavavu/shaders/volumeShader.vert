#ifdef WEBGL
attribute vec3 aVertexPosition;
void main(void)
{
  gl_Position = vec4(aVertexPosition, 1.0);
}
#else
void main(void)
{
  gl_Position = gl_Vertex;
}
#endif
