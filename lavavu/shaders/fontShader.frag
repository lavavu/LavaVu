varying vec4 vColour;
varying vec2 vTexCoord;

uniform bool uTextured;
uniform sampler2D uTexture;

void main(void)
{
  vec4 fColour = vColour;
  if (uTextured)
  {
    vec4 tColour = texture2D(uTexture, vTexCoord);
    //Just use the alpha component
    fColour.a = tColour.a;
  }
  gl_FragColor = fColour;
}

