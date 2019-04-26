in vec4 vColour;
in vec2 vTexCoord;

uniform bool uTextured;
uniform sampler2D uTexture;

#ifdef WEBGL
#define outColour gl_FragColor
#else
out vec4 outColour;
#endif

void main(void)
{
  vec4 fColour = vColour;
  if (uTextured)
  {
    vec4 tColour = texture2D(uTexture, vTexCoord);
    //Just use the alpha component (red in single component texture)
    fColour.a = tColour.r;
  }
  outColour = fColour;
}

