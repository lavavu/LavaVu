uniform int uPointType;
varying vec4 vColour;
varying vec3 vPosEye;
varying float vPointType;
void main(void)
{
  // calculate normal from point coordinates
  vec3 N;
  N.xy = gl_PointCoord * vec2(2.0, -2.0) + vec2(-1.0, 1.0);
  float mag = dot(N.xy, N.xy);
  if (vColour.a < 0.05 || mag > 1.0) discard;   // kill pixels outside circle radius and transparent pixels

  int pointType = uPointType;
  if (vPointType >= 0.0) pointType = int(ceil(vPointType));

  if (pointType < 2)
  {
    if (pointType == -1)
      gl_FragColor = vColour; //vec4(vColour.rgb, 1.0);      //Simple
    else if (pointType == 1)
    //  gl_FragColor = vec4(vColour.rgb, vColour.a * 1.0-sqrt(mag));  // 
      gl_FragColor = vec4(vColour.rgb, vColour.a * exp((-mag*mag)/1.28));  //Gaussian
    else
      gl_FragColor = vec4(vColour.rgb, vColour.a * 1.0-mag);      //Linear
    return; //No lighting effects, finish here`
  }

  // calculate lighting
  vec3 specolour = vec3(1.0, 1.0, 1.0);   //Colour of specular light
  vec3 diffcolour = vec3(1.0, 1.0, 1.0);  //Colour of diffuse light
  vec3 ambcolour = vec3(0.1, 0.1, 0.1);   //Colour of ambient light
  N.z = sqrt(1.0-mag);
  vec3 lightDir = normalize(vec3(1.0,1.0,1.0) - vPosEye);
  float diffuse = max(0.0, dot(lightDir, N));

  // compute the specular term if diffuse is larger than zero 
  float specular = 0.0;
  if (pointType == 3 && diffuse > 0.0)
  {
    float shininess = 32.0;
    vec3 lightPos = lightDir*2.0;        //Fixed light position
    // normalize the half-vector, and then compute the 
    // cosine (dot product) with the normal
    vec3 halfVector = normalize(lightPos - vec3(gl_PointCoord, 0));
    float NdotHV = max(dot(N, halfVector), 0.0);
    specular = pow(NdotHV, shininess);
  }

  vec3 lightWeighting = ambcolour + diffcolour * diffuse;
  gl_FragColor = vec4(vColour.rgb * lightWeighting + specolour * specular, vColour.a);
}


