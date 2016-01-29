varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec2 vTexCoord;
varying float vObjectID;
varying vec3 vert;

uniform int uCullFace[64];
//uniform sampler2D Texture;

void main(void)
{
  //Wireframe from tex coord
  if (vTexCoord.x > 0.03 && vTexCoord.x < 0.97 &&
      vTexCoord.y > 0.03 && vTexCoord.y < 0.97) discard;
  float alpha = vColour.a;

  //Get properties by object ID
  int cullface = 0;
  for (int i=0; i<=64; i++)
    if (abs(vObjectID - float(i)) <= 0.001) cullface = uCullFace[i];
  //Back-face culling
  if (cullface > 0 && dot(vPosEye, vNormal) > 0.0) discard;

  //Head light
  const vec3 lightPos = vec3(0.5, 5.0, 7.0);
  const float ambient = 0.4;
  float diffuse = 0.8; //1.0 - ambient;

  vec3 diffColour = vec3(1.0, 1.0, 1.0);  //Colour of diffuse light
  vec3 ambColour = vec3(ambient, ambient, ambient);   //Colour of ambient light

  vec3 lightDir = normalize(lightPos - vPosEye);

  diffuse *= abs(dot(vNormal, lightDir));

  vec3 lightWeighting = ambColour + diffColour * diffuse;
  gl_FragColor = vec4(vColour.rgb * lightWeighting, alpha);
}
