uniform int uPointType;
uniform float uOpacity;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform bool uTextured;
uniform bool uOpaque;
uniform sampler2D uTexture;
uniform vec3 uClipMin;
uniform vec3 uClipMax;
uniform vec3 uLightPos;

#ifdef WEBGL
varying vec4 vColour;
#else
#define vColour gl_Color
#endif

varying vec3 vVertex;
varying float vPointSize;
varying vec3 vPosEye;
varying float vPointType;


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

  gl_FragColor = vec4(colour, alpha);
}

void main(void)
{
  //Clip planes in X/Y/Z
  if (any(lessThan(vVertex, uClipMin)) || any(greaterThan(vVertex, uClipMax))) discard;

  float alpha = vColour.a;
  if (uOpacity > 0.0) alpha *= uOpacity;
  gl_FragColor = vColour;

  int pointType = uPointType;
  if (vPointType >= 0.0) pointType = int(floor(vPointType + 0.5)); //Round back to nearest int

  //Textured?
  //if (uTextured)
  //   gl_FragColor = texture2D(uTexture, gl_PointCoord);

  //Flat, square points, fastest, skip lighting
  if (pointType == 4)
  {
    calcColour(gl_FragColor.rgb, alpha);
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

    calcColour(gl_FragColor.rgb, alpha);
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
    calcColour(gl_FragColor.rgb * diffuse + specular, alpha);
  }
  else
    calcColour(gl_FragColor.rgb * diffuse, alpha);
}
