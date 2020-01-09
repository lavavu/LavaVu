in vec4 vColour;
in vec3 vNormal;
in vec3 vPosEye;
in vec3 vVertex;
in vec2 vTexCoord;
in vec3 vLightPos;

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
uniform vec4 uLight;

#ifdef WEBGL
#define outColour gl_FragColor
#define texture(a,b) texture2D(a,b)

//Before OpenGL 3+ we need our own isnan function
bool isnan3(vec3 val)
{
  if (!(val.x < 0.0 || 0.0 < val.x || val.x == 0.0)) return true;
  if (!(val.y < 0.0 || 0.0 < val.y || val.y == 0.0)) return true;
  if (!(val.z < 0.0 || 0.0 < val.z || val.z == 0.0)) return true;
  return false;
}

#else
#define isnan3(v) any(isnan(v))
flat in vec4 vFlatColour;
uniform bool uFlat;
out vec4 outColour;
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

  //Gamma correction
  //https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_reflection_model#OpenGL_Shading_Language_code_sample
  //const float screenGamma = 2.2; // Assume the monitor is calibrated to the sRGB color space
  //vec3 colorGammaCorrected = pow(color, vec3(1.0 / screenGamma));

  outColour = vec4(colour, alpha);
}

void main(void)
{
  //Clip planes in X/Y/Z
  if (any(lessThan(vVertex, uClipMin)) || any(greaterThan(vVertex, uClipMax))) discard;

  vec4 fColour = vColour;
#ifndef WEBGL
  if (uFlat)
    fColour = vFlatColour;
#endif
  float alpha = fColour.a;
  if (uTextured && vTexCoord.x > -1.0) //Null texcoord (-1,-1)
  {
    //With this blending mode, texture is blended over base colour,
    //and colour opacity has no effect on texture opacity
    //All desired texture opacity must be built in to the texture data
    //(Could add another blend mode if we want a dynamic texture opacity)
    vec4 tColour = texture(uTexture, vTexCoord);
    if (fColour.a > 0.01)
    {
      //Additive blend alpha channel
      alpha = fColour.a + tColour.a * (1.0 - fColour.a);

      //Blend the texure colour with the fragment colour using texture alpha
      fColour.rgb = vec3(mix(fColour.rgb, tColour.rgb, tColour.a));
    }
    else
    {
      //Disable all blending if the base colour opacity <= 0.01
      fColour = tColour;
      alpha = tColour.a;
    }
  }

  if (uOpacity > 0.0) alpha *= uOpacity;
  if (uOpaque) alpha = 1.0;
  if (alpha < 0.01) discard;

  if (!uLighting) 
  {
    calcColour(fColour.rgb, alpha);
    return;
  }

  vec3 lightColour = uLight.xyz;
  
  //Light direction
  vec3 lightDir = normalize(vLightPos - vPosEye);

  //Calculate diffuse lighting
  vec3 N = normalize(vNormal);

  //Default normal...
  if (uCalcNormal || dot(N,N) < 0.01 || isnan3(N))
  {
    //Requires extension in WebGL: OES_standard_derivatives
    vec3 fdx = vec3(dFdx(vPosEye.x),dFdx(vPosEye.y),dFdx(vPosEye.z));    
    vec3 fdy = vec3(dFdy(vPosEye.x),dFdy(vPosEye.y),dFdy(vPosEye.z));
    N = normalize(cross(fdx,fdy)); 
  }

  //Modified to use energy conservation adjustment
  //https://learnopengl.com/Advanced-Lighting/Advanced-Lighting
  const float kPi8 = 3.14159265 * 8.0;

  //Calculate diffuse component
  //Single side or two-sided lighting with abs()?
  float diffuse = dot(N, lightDir);
  if (uLight.w < 0.5)
    diffuse = abs(diffuse);
  else
    diffuse = max(diffuse, 0.0);

  //Compute the specular term
  if (diffuse > 0.0 && uSpecular > 0.0)
  {
    //Specular power, higher is more focused/shiny
    float shininess = 256.0 * clamp(uShininess, 0.0, 1.0);
    vec3 specolour = lightColour; //Color of light - use the same as diffuse/ambient
    //Blinn-Phong
    vec3 viewDir = normalize(-vPosEye);
    //Normalize the half-vector
    vec3 halfVector = normalize(lightDir + viewDir);

    //Compute cosine (dot product) with the normal
    float NdotHV = dot(N, halfVector);
    //Single side or two-sided lighting with abs()?
    if (uLight.w < 0.5)
      NdotHV = abs(NdotHV);
    else
      NdotHV = max(NdotHV, 0.0);

    //Energy conservation adjustment (more focused/shiny highlight will be brighter)
    float energyConservation = ( 8.0 + shininess) / kPi8;
    //Multiplying specular by diffuse prevents bands at edges for low shininess
    float spec = diffuse * uSpecular * energyConservation * pow(NdotHV, shininess);

    //Final colour - specular + diffuse + ambient
    calcColour(lightColour * (fColour.rgb * (uAmbient + uDiffuse * diffuse) + vec3(spec)), alpha);
  }
  else
    //Final colour - diffuse + ambient only
    calcColour(lightColour * fColour.rgb * (uAmbient + diffuse * uDiffuse), alpha);
}

