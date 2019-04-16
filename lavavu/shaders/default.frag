varying vec4 vColour;
#ifndef WEBGL
flat in vec4 vFlatColour;
uniform bool uFlat;
#endif
void main(void)
{
#ifndef WEBGL
  if (uFlat)
    gl_FragColor = vFlatColour;
  else
#endif
    gl_FragColor = vColour;
}

