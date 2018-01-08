#extension GL_OES_standard_derivatives : enable

varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec2 vTexCoord;
varying float vObjectID;
varying vec3 vVertex;
uniform float uOpacity;
uniform bool uLighting;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform bool uTextured;
uniform sampler2D uTexture;
uniform vec3 uClipMin;
uniform vec3 uClipMax;

uniform int uCullFace[64];

void main(void)
{
  //Clip planes in X/Y/Z (shift seems to be required on nvidia)
  if (any(lessThan(vVertex, uClipMin - vec3(0.01))) || any(greaterThan(vVertex, uClipMax + vec3(0.01)))) discard;

  vec4 fColour = vColour;
  if (uTextured) 
    fColour = texture2D(uTexture, vTexCoord);

  //Get properties by object ID
  int cullface = 0;
  for (int i=0; i<=64; i++)
    if (abs(vObjectID - float(i)) <= 0.001) cullface = uCullFace[i];
  //Back-face culling
  if (cullface > 0 && dot(vPosEye, vNormal) > 0.0) discard;

  //TODO: uniforms for rest of light params!
  const float shininess = 100.0; //Size of highlight (higher is smaller)
  const vec3 light = vec3(1.0, 1.0, 1.0);  //Colour of light

  //Head light, lightPos=(0,0,0) - vPosEye
  vec3 lightDir = normalize(-vPosEye);

  //Calculate diffuse lighting
  vec3 N = vNormal;

  //Default normal...
  if (dot(N,N) < 0.01)
  {
    //Requires extension: OES_standard_derivatives
    vec3 fdx = vec3(dFdx(vPosEye.x),dFdx(vPosEye.y),dFdx(vPosEye.z));    
    vec3 fdy = vec3(dFdy(vPosEye.x),dFdy(vPosEye.y),dFdy(vPosEye.z));
    N = normalize(cross(fdx,fdy)); 
  }

  float diffuse = abs(dot(N, lightDir));

   //Compute the specular term
   vec3 specular = vec3(0.0,0.0,0.0);
   if (diffuse > 0.0 && uSpecular > 0.0)
   {
      vec3 specolour = vec3(1.0, 1.0, 1.0);   //Color of light
      //Normalize the half-vector
      //vec3 halfVector = normalize(vPosEye + lightDir);
      vec3 halfVector = normalize(vec3(0.0, 0.0, 1.0) + lightDir);
      //Compute cosine (dot product) with the normal (abs for two-sided)
      float NdotHV = abs(dot(N, halfVector));
      specular = specolour * pow(NdotHV, shininess);
   }

  vec3 lightWeighting = light * (uAmbient + diffuse * uDiffuse + specular * uSpecular);
  float alpha = fColour.a;
  if (uOpacity > 0.0) alpha *= uOpacity;
  vec4 colour = vec4(fColour.rgb * lightWeighting, alpha);

  //Brightness adjust
  colour += uBrightness;
  //Saturation & Contrast adjust
  const vec4 LumCoeff = vec4(0.2125, 0.7154, 0.0721, 0.0);
  vec4 AvgLumin = vec4(0.5, 0.5, 0.5, 0.0);
  vec4 intensity = vec4(dot(colour, LumCoeff));
  colour = mix(intensity, colour, uSaturation);
  colour = mix(AvgLumin, colour, uContrast);
  colour.a = alpha;

  if (alpha < 0.01) discard;

  gl_FragColor = colour;
}

