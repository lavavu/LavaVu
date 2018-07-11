varying vec4 vColour;
varying vec3 vNormal;
varying vec3 vPosEye;
varying vec3 vVertex;
varying vec2 vTexCoord;

uniform float uOpacity;
uniform bool uLighting;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform float uShininess;

uniform bool uTextured;
uniform sampler2D uTexture;
uniform vec3 uClipMin;
uniform vec3 uClipMax;
uniform bool uOpaque;
uniform vec3 uLightPos;

#ifdef WEBGL
varying float vObjectID;
uniform int uCullFace[64];
#endif


uniform bool uCalcNormal;

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

  gl_FragColor = vec4(colour, alpha);
}

//Until we can use OpenGL 3+ we need our own isnan function
bool isnan(vec3 val)
{
  if (!(val.x < 0.0 || 0.0 < val.x || val.x == 0.0)) return true;
  if (!(val.y < 0.0 || 0.0 < val.y || val.y == 0.0)) return true;
  if (!(val.z < 0.0 || 0.0 < val.z || val.z == 0.0)) return true;
  return false;
}

void main(void)
{
  //Clip planes in X/Y/Z
  if (any(lessThan(vVertex, uClipMin)) || any(greaterThan(vVertex, uClipMax))) discard;

  vec4 fColour = vColour;
  if (uTextured) 
    fColour = texture2D(uTexture, vTexCoord);

  float alpha = fColour.a;
  if (uOpacity > 0.0) alpha *= uOpacity;
  if (uOpaque) alpha = 1.0;
  if (alpha < 0.01) discard;

  if (!uLighting) 
  {
    calcColour(fColour.rgb, alpha);
    return;
  }
  
  const vec3 light = vec3(1.0);  //Colour of light

  //Head light, lightPos=(0,0,0) - vPosEye
  vec3 lightDir = normalize(uLightPos - vPosEye);

  //Calculate diffuse lighting
  vec3 N = normalize(vNormal);

  //Default normal...
  if (uCalcNormal || dot(N,N) < 0.01 || isnan(N))
  //if (uCalcNormal || dot(N,N) < 0.01 || any(isnan(N)))
  {
    //Requires extension in WebGL: OES_standard_derivatives
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
    float shininess = 250.0 * (1.0 - uShininess);
    specular = specolour * pow(NdotHV, shininess);
    calcColour(fColour.rgb * light * (uAmbient + diffuse * uDiffuse) + (specular * uSpecular), alpha);
  }
  else
    calcColour(fColour.rgb * light * (uAmbient + diffuse * uDiffuse), alpha);
}

