//flat varying vec4 vColour;
varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec3 vVertex;
uniform float uOpacity;
uniform bool uLighting;
uniform bool uTextured;
uniform bool uCalcNormal;
uniform sampler2D uTexture;

#define HUGEVAL 1e20

void main(void)
{
  //Clip planes in X/Y/Z
  vec3 clipMin = vec3(-HUGEVAL,-HUGEVAL,-HUGEVAL);
  vec3 clipMax = vec3(HUGEVAL,HUGEVAL,HUGEVAL);
  if (any(lessThan(vVertex, clipMin)) || any(greaterThan(vVertex, clipMax))) discard;

  vec4 fColour = vColour;
  if (uTextured) 
    fColour = texture2D(uTexture, gl_TexCoord[0].xy);

  if (!uLighting) 
  {
    gl_FragColor = fColour;
    return;
  }
  
  //TODO: uniforms for light params!
  const float ambient = 0.4;
  const float diffuseIntensity = 0.8;
  const float specIntensity = 0.0;
  const float shininess = 100.0; //Size of highlight (higher is smaller)
  const vec3 light = vec3(1.0, 1.0, 1.0);  //Colour of light

  //Head light, lightPos=(0,0,0) - vPosEye
  vec3 lightDir = normalize(-vPosEye);

  //Calculate diffuse lighting
  vec3 N = normalize(vNormal);

  //Default normal...
  if (length(N) < 0.9 || uCalcNormal)
  {
    vec3 fdx = vec3(dFdx(vPosEye.x),dFdx(vPosEye.y),dFdx(vPosEye.z));    
    vec3 fdy = vec3(dFdy(vPosEye.x),dFdy(vPosEye.y),dFdy(vPosEye.z));
    N = normalize(cross(fdx,fdy)); 
  }

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

