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
  
  //TODO: uniforms for light params!
  const float ambient = 0.4;
  const float diffuseIntensity = 0.8;
  const float specIntensity = 0.0;
  const float shininess = 100; //Size of highlight (higher is smaller)
  const vec3 light = vec3(1.0, 1.0, 1.0);  //Colour of light

  //Head light, lightPos=(0,0,0) - vPosEye
  vec3 lightDir = normalize(-vPosEye);

  //Calculate diffuse lighting
  vec3 N = normalize(vNormal);
  float diffuse = abs(dot(N, lightDir));

   //Compute the specular term
   vec3 specular = vec3(0.0,0.0,0.0);
   if (diffuse > 0.0 && specIntensity > 0.0)
   {
      vec3 specolour = vec3(1.0, 1.0, 1.0);   //Color of light
      //Normalize the half-vector
      //vec3 halfVector = normalize(vPosEye + lightDir);
      vec3 halfVector = normalize(vec3(0.0, 0.0, 1.0) + lightDir);
      //Compute cosine (dot product) with the normal (abs for two-sided)
      float NdotHV = abs(dot(N, halfVector));
      specular = specolour * pow(NdotHV, shininess);
   }

  vec3 lightWeighting = light * (ambient + diffuse * diffuseIntensity + specular * specIntensity);
  float alpha = fColour.a;
  if (uOpacity > 0.0) alpha *= uOpacity;
  vec4 colour = vec4(fColour.rgb * lightWeighting, alpha);

  gl_FragColor = colour;
}

