varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
uniform float uOpacity;
uniform bool uLighting;
uniform bool uTextured;
uniform sampler2D uTexture;

void main(void)
{
  vec4 fColour = vColour;
  if (uTextured) 
    fColour = texture2D(uTexture, gl_TexCoord[0]);

  if (!uLighting) 
  {
    gl_FragColor = fColour;
    return;
  }
  
  const float ambient = 0.4;
  float diffuse = 0.8;
  vec3 white = vec3(1.0, 1.0, 1.0);  //Colour of light

  //Head light, lightPos=(0,0,0) - vPosEye
  vec3 lightDir = normalize(-vPosEye);

  diffuse *= abs(dot(vNormal, lightDir));

  float alpha = fColour.a;
  vec3 lightWeighting = white * (ambient + diffuse);
  if (uOpacity > 0.0) alpha *= uOpacity;
  gl_FragColor = vec4(fColour.rgb * lightWeighting, alpha);
}

