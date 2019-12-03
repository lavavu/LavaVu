uniform int uPointType;
uniform float uOpacity;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform bool uOpaque;
uniform bool uTextured;
uniform sampler2D uTexture;
uniform vec3 uClipMin;
uniform vec3 uClipMax;
uniform vec3 uLightPos;

in vec4 vColour;
in vec3 vVertex;
in vec2 vTexCoord;
in float vPointSize;
in vec3 vPosEye;
in float vPointType;

#ifdef WEBGL
#define outColour gl_FragColor
#define texture(a,b) texture2D(a,b)
#else
out vec4 outColour;
#endif

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

  if (uOpaque)
    alpha = 1.0;

  outColour = vec4(colour, alpha);
}

void main(void)
{
  //Clip planes in X/Y/Z
  if (any(lessThan(vVertex, uClipMin)) || any(greaterThan(vVertex, uClipMax))) discard;

  float alpha = vColour.a;
  if (uOpacity > 0.0) alpha *= uOpacity;
  outColour = vColour;

  int pointType = uPointType;
  if (vPointType >= 0.0) pointType = int(floor(vPointType + 0.5)); //Round back to nearest int

  //Textured?
  if (uTextured && vTexCoord.y >= 0.0)
  {
    vec4 tColour;
    if (vTexCoord.x < 0.0)
      tColour = texture(uTexture, gl_PointCoord); //Point sprite mode
    else
      tColour = texture(uTexture, vTexCoord);

    //Blend with colour
    outColour = mix(tColour, outColour, 1.0-tColour.a);
    alpha = outColour.a;
  }

  //Flat, square points, fastest, skip lighting
  if (pointType == 4)
  {
    calcColour(outColour.rgb, alpha);
    return;
  }

  //Circular points...
  //Calculate normal from point/tex coordinates
  vec3 N;
  N.xy = gl_PointCoord * 2.0 - vec2(1.0);
  float R = dot(N.xy, N.xy);
  //Discard if outside circle
  if (R > 1.0) discard;
  //Anti-aliased edges for sphere types
  if (pointType > 1)
  {
    float edge = vPointSize - R * vPointSize;
    if (edge <= 4.0)
      alpha *= (0.25 * edge);
  }

  //Discard if transparent (outside circle)
  if (alpha < 0.01) discard;

  if (pointType < 2)
  {
    //Circular with no light/shading, just blur
    if (pointType == 0)
      alpha *= 1.0-sqrt(R); //Gaussian
    else //TODO: allow disable blur for circular points
      alpha *= 1.0-R;       //Linear

    calcColour(outColour.rgb, alpha);
    return;
  }

  //Calculate diffuse lighting component
  N.z = sqrt(1.0-R);
  float diffuse = 1.0;
  vec3 lightDir = normalize(uLightPos - vPosEye);
  diffuse = max(0.0, dot(lightDir, N));

  //Calculate specular lighting component
  if (pointType == 3 && diffuse > 0.0)
  {
    vec3 specular = vec3(0.0,0.0,0.0);
    float shininess = 200.0; //Size of highlight
    vec3 specolour = vec3(1.0, 1.0, 1.0);   //Color of light
    //Normalize the half-vector
    //vec3 halfVector = normalize(vPosEye + lightDir);
    vec3 halfVector = normalize(vec3(0.0, 0.0, 1.0) + lightDir);
    //Compute cosine (dot product) with the normal
    float NdotHV = max(dot(N, halfVector), 0.0);
    specular = specolour * pow(NdotHV, shininess);
    calcColour(outColour.rgb * diffuse + specular, alpha);
  }
  else
    calcColour(outColour.rgb * diffuse, alpha);
}
