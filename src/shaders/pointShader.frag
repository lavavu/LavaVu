#version 120 //Required to use gl_PointCoord
//uniform sampler2D texture;
varying float vSmooth;
varying vec3 vPosEye;
varying float vPointType;
uniform int uPointType;
uniform float uOpacity;
uniform bool uTextured;
uniform sampler2D uTexture;

void main(void)
{
   float alpha = gl_Color.a;
   if (uOpacity > 0.0) alpha *= uOpacity;
   gl_FragColor = gl_Color;
   float pointType = uPointType;
   if (vPointType >= 0) pointType = vPointType;

   //Textured?
   if (uTextured)
      gl_FragColor = texture2D(uTexture, gl_PointCoord);

   //Flat, square points, fastest
   if (pointType == 4 || vSmooth < 0.0) 
      return;

   // calculate normal from point/tex coordinates
   vec3 N;
   N.xy = gl_PointCoord * 2.0 - vec2(1.0);    
   float mag = dot(N.xy, N.xy);
   if (alpha < 0.01 || mag > 1.0) discard;   // kill pixels outside circle radius and transparent pixels

   if (pointType < 2)
   {
      if (pointType == 0)
         gl_FragColor.a = alpha * 1.0-sqrt(mag);  //Gaussian
      else
         gl_FragColor.a = alpha * 1.0-mag;      //Linear
      return;
   }
   N.z = sqrt(1.0-mag);

   // calculate diffuse lighting
   vec3 lightDir = normalize(vec3(0,0,0) - vPosEye);
   float diffuse = max(0.0, dot(lightDir, N));

   // compute the specular term 
   vec3 specular = vec3(0.0,0.0,0.0);
   if (pointType == 3 && diffuse > 0.0)
   {
      float shininess = 200; //Size of highlight
      vec3 specolour = vec3(1.0, 1.0, 1.0);   //Color of light
      // normalize the half-vector, and then compute the 
      // cosine (dot product) with the normal
      //vec3 halfVector = normalize(vPosEye + lightDir);
      vec3 halfVector = normalize(vec3(0.0, 0.0, 1.0) + lightDir);
      float NdotHV = max(dot(N, halfVector), 0.0);
      specular = specolour * pow(NdotHV, shininess);
      //specular = vec3(1.0, 0.0, 0.0);
   }

   gl_FragColor = vec4(gl_FragColor.rgb * diffuse + specular, alpha);
}
