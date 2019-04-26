in vec4 vColour;

#ifdef WEBGL
#define outColour gl_FragColor
#else
flat in vec4 vFlatColour;
uniform bool uFlat;
out vec4 outColour;
#endif

void main(void)
{
#ifndef WEBGL
  if (uFlat)
    outColour = vFlatColour;
  else
#endif
    outColour = vColour;
}

