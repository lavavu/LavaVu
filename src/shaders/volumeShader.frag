/*
 * Copyright (c) 2014, Monash University. All rights reserved.
 * Author: Owen Kaluza - owen.kaluza ( at ) monash.edu
 *
 * Licensed under the GNU Lesser General Public License
 * https://www.gnu.org/licenses/lgpl.html
 * (volume shader from sharevol https://github.com/OKaluza/sharevol)
 */
#version 120
//precision highp float;

const int maxSamples = 2048;
const float depthT = 0.99; //Transmissivity threshold below which depth write applied

uniform sampler3D uVolume;
uniform sampler2D uTransferFunction;

uniform vec3 uBBMin;
uniform vec3 uBBMax;
uniform vec3 uResolution;

uniform bool uEnableColour;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uPower;

uniform mat4 uMVMatrix;
uniform mat4 uMVPMatrix;
uniform mat4 uInvMVPMatrix;
uniform mat4 uNMatrix;
uniform vec4 uViewport;
uniform int uSamples;
uniform float uDensityFactor;
uniform float uIsoValue;
uniform vec4 uIsoColour;
uniform float uIsoSmooth;
uniform int uIsoWalls;
uniform int uFilter;
uniform vec2 uRange;
uniform vec2 uDenMinMax;

uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform vec3 uLightPos;

vec3 bbMin;
vec3 bbMax;

//#define tex3D(pos) interpolate_tricubic_fast(pos)
//#define tex3D(pos) texture3Dfrom2D(pos).x

float tex3D(vec3 pos) 
{
  //if (uFilter > 0)
  //  return interpolate_tricubic_fast(pos);
  return texture3D(uVolume, pos).x; //from2D(pos).x;
}

// It seems WebGL has no transpose
mat4 transpose(in mat4 m)
{
  return mat4(
              vec4(m[0].x, m[1].x, m[2].x, m[3].x),
              vec4(m[0].y, m[1].y, m[2].y, m[3].y),
              vec4(m[0].z, m[1].z, m[2].z, m[3].z),
              vec4(m[0].w, m[1].w, m[2].w, m[3].w)
             );
}

void lighting(in vec3 pos, in vec3 normal, inout vec3 colour)
{
  vec4 vertPos = uMVMatrix * vec4(pos, 1.0);
  //Light moves with camera at specified offset
  vec3 lightDir = normalize(uLightPos - vertPos.xyz);
  float diffuse = clamp(abs(dot(normal, lightDir)), 0.1, 1.0);
  vec3 lightWeighting = vec3(uAmbient + diffuse * uDiffuse);
  colour *= lightWeighting;
}

vec3 isoNormal(in vec3 pos, in vec3 shift, in float density)
{
  //Detect bounding box hit (walls)
  if (uIsoWalls > 0)
  {
    if (pos.x <= bbMin.x) return vec3(-1.0, 0.0, 0.0);
    if (pos.x >= bbMax.x) return vec3(1.0, 0.0, 0.0);
    if (pos.y <= bbMin.y) return vec3(0.0, -1.0, 0.0);
    if (pos.y >= bbMax.y) return vec3(0.0, 1.0, 0.0);
    if (pos.z <= bbMin.z) return vec3(0.0, 0.0, -1.0);
    if (pos.z >= bbMax.z) return vec3(0.0, 0.0, 1.0);
  }

  //Calculate normal
  /*return normalize(vec3(density) - vec3(tex3D(vec3(pos.x+shift.x, pos.y, pos.z)), 
                                        tex3D(vec3(pos.x, pos.y+shift.y, pos.z)), 
                                        tex3D(vec3(pos.x, pos.y, pos.z+shift.z))));
  */
  //Compute central difference gradient 
  //(slow, faster way would be to precompute a gradient texture)
  vec3 pos1 = vec3(tex3D(vec3(pos.x+shift.x, pos.y, pos.z)), 
                   tex3D(vec3(pos.x, pos.y+shift.y, pos.z)), 
                   tex3D(vec3(pos.x, pos.y, pos.z+shift.z)));
  vec3 pos2 = vec3(tex3D(vec3(pos.x-shift.x, pos.y, pos.z)), 
                   tex3D(vec3(pos.x, pos.y-shift.y, pos.z)), 
                   tex3D(vec3(pos.x, pos.y, pos.z-shift.z)));
  return normalize(pos1 - pos2);
}

vec2 rayIntersectBox(vec3 rayDirection, vec3 rayOrigin)
{
  //Intersect ray with bounding box
  vec3 rayInvDirection = 1.0 / rayDirection;
  vec3 bbMinDiff = (bbMin - rayOrigin) * rayInvDirection;
  vec3 bbMaxDiff = (bbMax - rayOrigin) * rayInvDirection;
  vec3 imax = max(bbMaxDiff, bbMinDiff);
  vec3 imin = min(bbMaxDiff, bbMinDiff);
  float back = min(imax.x, min(imax.y, imax.z));
  float front = max(max(imin.x, 0.0), max(imin.y, imin.z));
  return vec2(back, front);
}

void main()
{
    bbMin = clamp(uBBMin, vec3(0.0), vec3(1.0));
    bbMax = clamp(uBBMax, vec3(0.0), vec3(1.0));

    //Compute eye space coord from window space to get the ray direction
    mat4 invMVMatrix = transpose(uMVMatrix);
    //ObjectSpace *[MV] = EyeSpace *[P] = Clip /w = Normalised device coords ->VP-> Window
    //Window ->[VP^]-> NDC ->[/w]-> Clip ->[P^]-> EyeSpace ->[MV^]-> ObjectSpace
    vec4 ndcPos;
    ndcPos.xy = ((2.0 * gl_FragCoord.xy) - (2.0 * uViewport.xy)) / (uViewport.zw) - 1.0;
    //ndcPos.z = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) /
    //           (gl_DepthRange.far - gl_DepthRange.near);
    ndcPos.z = 2.0 * gl_FragCoord.z - 1.0;
    ndcPos.w = 1.0;
    vec4 clipPos = ndcPos / gl_FragCoord.w;
    //vec4 eyeSpacePos = uInvPMatrix * clipPos;
    vec3 rayDirection = normalize((uInvMVPMatrix * clipPos).xyz);

    //Ray origin from the camera position
    vec4 camPos = -vec4(uMVMatrix[3]);  //4th column of modelview
    vec3 rayOrigin = (invMVMatrix * camPos).xyz;

    //Calc step
    float stepSize = 1.732 / float(uSamples); //diagonal of [0,1] normalised coord cube = sqrt(3)

    //Intersect ray with bounding box
    vec2 intersection = rayIntersectBox(rayDirection, rayOrigin);
    //Subtract small increment to avoid errors on front boundary
    intersection.y -= 0.000001;
    //Discard points outside the box (no intersection)
    if (intersection.x <= intersection.y) discard;

    vec3 rayStart = rayOrigin + rayDirection * intersection.y;
    vec3 rayStop = rayOrigin + rayDirection * intersection.x;

    vec3 step = normalize(rayStop-rayStart) * stepSize;
    vec3 pos = rayStart;

    float T = 1.0;
    vec3 colour = vec3(0.0);
    bool inside = false;
    vec3 shift = uIsoSmooth / uResolution;
    //Number of samples to take along this ray before we pass out back of volume...
    float travel = distance(rayStop, rayStart) / stepSize;
    int samples = int(ceil(travel));
    float range = uRange.y - uRange.x;
    if (range <= 0.0) range = 1.0;
  
    //Raymarch, front to back
    vec3 depthHit = rayStart;
    for (int i=0; i < maxSamples; ++i)
    {
      //Render samples until we pass out back of cube or fully opaque
#ifndef IE11
      if (i == samples || T < 0.01) break;
#else
      //This is slower but allows IE 11 to render, break on non-uniform condition causes it to fail
      if (i == uSamples) break;
      if (all(greaterThanEqual(pos, bbMin)) && all(lessThanEqual(pos, bbMax)))
#endif
      {
        //Get density 
        float density = tex3D(pos);

        //Set the depth point to where transmissivity drops below threshold
        if (T > depthT)
          depthHit = pos;

#define ISOSURFACE
#ifdef ISOSURFACE
        //Passed through isosurface?
        if (uIsoColour.a > 0.0 && ((!inside && density >= uIsoValue) || (inside && density < uIsoValue)))
        {
          inside = !inside;
          //Find closer to exact position by iteration
          //http://sizecoding.blogspot.com.au/2008/08/isosurfaces-in-glsl.html
          float exact;
          float a = intersection.y + (float(i)*stepSize);
          float b = a - stepSize;
          for (int j = 0; j < 5; j++)
          {
            exact = (b + a) * 0.5;
            pos = rayDirection * exact + rayOrigin;
            density = tex3D(pos);
            if (density - uIsoValue < 0.0)
              b = exact;
            else
              a = exact;
          }

          //Skip edges unless flagged to draw
          if (uIsoWalls > 0 || all(greaterThanEqual(pos, bbMin)) && all(lessThanEqual(pos, bbMax)))
          {
            vec4 value = vec4(uIsoColour.rgb, 1.0);

            vec3 normal = normalize((uNMatrix * vec4(isoNormal(pos, shift, density), 1.0)).xyz);

            vec3 light = value.rgb;
            lighting(pos, normal, light);
            //Front-to-back blend equation
            colour += T * uIsoColour.a * light;
            T *= (1.0 - uIsoColour.a);
          }
        }
#endif

        if (uDensityFactor > 0.0)
        {
          //Normalise the density over provided range
          density = (density - uRange.x) / range;
          density = clamp(density, 0.0, 1.0);
          if (density < uDenMinMax[0] || density > uDenMinMax[1])
          {
            //Skip to next sample...
            pos += step;
            continue;
          }

          density = pow(density, uPower); //Apply power

          vec4 value;
          if (uEnableColour)
            value = texture2D(uTransferFunction, vec2(density, 0.5));
          else
            value = vec4(density);

          value *= uDensityFactor * stepSize;

          //Color
          colour += T * value.rgb;
          //Alpha
          T *= 1.0 - value.a;
        }
      }

      //Next sample...
      pos += step;
    }

    //Apply brightness, saturation & contrast
    colour += uBrightness;
    const vec3 LumCoeff = vec3(0.2125, 0.7154, 0.0721);
    vec3 AvgLumin = vec3(0.5, 0.5, 0.5);
    vec3 intensity = vec3(dot(colour, LumCoeff));
    colour = mix(intensity, colour, uSaturation);
    colour = mix(AvgLumin, colour, uContrast);

    //TODO: alpha threshold uniform?
    //if (T > depthT) discard;
    gl_FragColor = vec4(colour, 1.0 - T);

#ifndef NO_DEPTH_WRITE
    // Write the depth (!Not supported in WebGL without extension)
    float depth = 1.0; //Default to far limit
    if (T < depthT)
    {
      //ObjectSpace *[MV] = EyeSpace *[P] = Clip /w = Normalised device coords ->VP-> Window
      vec4 clip_space_pos = vec4(depthHit, 1.0);
      clip_space_pos = uMVPMatrix * clip_space_pos;
      //Get in normalised device coords [-1,1]
      float ndc_depth = clip_space_pos.z / clip_space_pos.w;
      //Convert to depth range, default [0,1] but may have been modified
      if (ndc_depth >= -1.0 && ndc_depth <= 1.0)
        depth = 0.5 * ndc_depth + 0.5;
      else
        depth = 0.0;

      //depth = 0.5 * (((gl_DepthRange.far - gl_DepthRange.near) * ndc_depth) + 
      //                 gl_DepthRange.near + gl_DepthRange.far);
    }

    gl_FragDepth = depth;
#endif
}
