in vec4 vColour;

flat in vec4 vFlatColour;
uniform bool uFlat;
out vec4 outColour;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;

void calcColour(vec3 colour, float alpha)
{
  //Brightness adjust
  colour += uBrightness;
  //Saturation & Contrast adjust
  const vec3 LumCoeff = vec3(0.2125, 0.7154, 0.0721);
  vec3 AvgLumin = vec3(0.5, 0.5, 0.5);
  vec3 intensity = vec3(dot(colour, LumCoeff));
  colour = mix(intensity, colour, uSaturation);
  colour = mix(AvgLumin, colour, uContrast);
  outColour = vec4(colour, alpha);
}

void main(void)
{
  if (uFlat)
    outColour = vFlatColour;
  else
    outColour = vColour;

  calcColour(outColour.rgb, outColour.a);
}

