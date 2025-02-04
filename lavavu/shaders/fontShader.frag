in vec4 vColour;
in vec2 vTexCoord;

uniform bool uTextured;
uniform sampler2D uTexture;

out vec4 outColour;

void main(void)
{
  vec4 fColour = vColour;
  if (uTextured)
  {
    vec4 tColour = texture(uTexture, vTexCoord);
    //Just use the alpha component (red in single component texture)
    fColour.a = tColour.r;
  }
  outColour = fColour;
}

