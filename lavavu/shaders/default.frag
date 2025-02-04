in vec4 vColour;

flat in vec4 vFlatColour;
uniform bool uFlat;
out vec4 outColour;

void main(void)
{
  if (uFlat)
    outColour = vFlatColour;
  else
    outColour = vColour;
}

