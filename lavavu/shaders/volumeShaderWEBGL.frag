/*
 * Copyright (c) 2014, Monash University. All rights reserved.
 * Author: Owen Kaluza - owen.kaluza ( at ) monash.edu
 *
 * Licensed under the GNU Lesser General Public License
 * https://www.gnu.org/licenses/lgpl.html
 */
precision highp float;

//Defined dynamically before compile...
//const vec2 slices = vec2(16.0,16.0);
//const int maxSamples = 256;

uniform sampler2D uVolume;
uniform sampler2D uTransferFunction;

uniform vec3 uBBMin;
uniform vec3 uBBMax;
uniform vec3 uResolution;

uniform bool uEnableColour;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uPower;

uniform mat4 uPMatrix;
uniform mat4 uInvPMatrix;
uniform mat4 uMVMatrix;
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

//#define tex3D(pos) interpolate_tricubic_fast(pos)
//#define tex3D(pos) texture3Dfrom2D(pos).x

vec2 islices = vec2(1.0 / slices.x, 1.0 / slices.y);

vec4 texture3Dfrom2D(vec3 pos)
{
  //Get z slice index and position between two slices
  float Z = pos.z * (slices.x * slices.y - 1.0);
  int slice = int(Z); //Index of first slice

  //X & Y coords of sample scaled to slice size
  vec2 sampleOffset = pos.xy * islices;
  //Offsets in 2D texture of given slice indices
  //(add offsets to scaled position within slice to get sample positions)
  float A = float(slice) * islices.x;
  float B = float(slice+1) * islices.x;
  vec2 z1offset = vec2(fract(A), floor(A) / slices.y) + sampleOffset;
  vec2 z2offset = vec2(fract(B), floor(B) / slices.y) + sampleOffset;
  
  //Interpolate the final value by position between slices [0,1]
  return mix(texture2D(uVolume, z1offset), texture2D(uVolume, z2offset), fract(Z));
}

float interpolate_tricubic_fast(vec3 coord);

float tex3D(vec3 pos) 
{
  if (uFilter > 0)
    return interpolate_tricubic_fast(pos);
  return texture3Dfrom2D(pos).x;
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

//Light moves with camera
const vec3 lightPos = vec3(0.5, 0.5, 5.0);
const float ambient = 0.2;
const float diffuse = 0.8;
const vec3 diffColour = vec3(1.0, 1.0, 1.0);  //Colour of diffuse light
const vec3 ambColour = vec3(0.2, 0.2, 0.2);   //Colour of ambient light

void lighting(in vec3 pos, in vec3 normal, inout vec3 colour)
{
  vec4 vertPos = uMVMatrix * vec4(pos, 1.0);
  //vec3 lightDir = normalize(lightPos - vertPos.xyz);
  vec3 lightDir = normalize(-vertPos.xyz);
  vec3 lightWeighting = ambColour + diffColour * diffuse * clamp(abs(dot(normal, lightDir)), 0.1, 1.0);

  colour *= lightWeighting;
}

vec3 isoNormal(in vec3 pos, in vec3 shift, in float density)
{
  //Detect bounding box hit (walls)
  if (uIsoWalls > 0)
  {
    if (pos.x <= uBBMin.x) return vec3(-1.0, 0.0, 0.0);
    if (pos.x >= uBBMax.x) return vec3(1.0, 0.0, 0.0);
    if (pos.y <= uBBMin.y) return vec3(0.0, -1.0, 0.0);
    if (pos.y >= uBBMax.y) return vec3(0.0, 1.0, 0.0);
    if (pos.z <= uBBMin.z) return vec3(0.0, 0.0, -1.0);
    if (pos.z >= uBBMax.z) return vec3(0.0, 0.0, 1.0);
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
  vec3 bbMinDiff = (uBBMin - rayOrigin) * rayInvDirection;
  vec3 bbMaxDiff = (uBBMax - rayOrigin) * rayInvDirection;
  vec3 imax = max(bbMaxDiff, bbMinDiff);
  vec3 imin = min(bbMaxDiff, bbMinDiff);
  float back = min(imax.x, min(imax.y, imax.z));
  float front = max(max(imin.x, 0.0), max(imin.y, imin.z));
  return vec2(back, front);
}

void main()
{
    //Compute eye space coord from window space to get the ray direction
    mat4 invMVMatrix = transpose(uMVMatrix);
    //ObjectSpace *[MV] = EyeSpace *[P] = Clip /w = Normalised device coords ->VP-> Window
    //Window ->[VP^]-> NDC ->[/w]-> Clip ->[P^]-> EyeSpace ->[MV^]-> ObjectSpace
    vec4 ndcPos;
    ndcPos.xy = ((2.0 * gl_FragCoord.xy) - (2.0 * uViewport.xy)) / (uViewport.zw) - 1.0;
    ndcPos.z = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) /
               (gl_DepthRange.far - gl_DepthRange.near);
    ndcPos.w = 1.0;
    vec4 clipPos = ndcPos / gl_FragCoord.w;
    //vec4 eyeSpacePos = uInvPMatrix * clipPos;
    vec3 rayDirection = normalize((invMVMatrix * uInvPMatrix * clipPos).xyz);

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
    //Scale isoValue
    float isoValue = uRange.x + uIsoValue * range;
  
    //Raymarch, front to back
    for (int i=0; i < maxSamples; ++i)
    {
      //Render samples until we pass out back of cube or fully opaque
#ifndef IE11
      if (i == samples || T < 0.01) break;
#else
      //This is slower but allows IE 11 to render, break on non-uniform condition causes it to fail
      if (i == uSamples) break;
      if (all(greaterThanEqual(pos, uBBMin)) && all(lessThanEqual(pos, uBBMax)))
#endif
      {
        //Get density 
        float density = tex3D(pos);

#define ISOSURFACE
#ifdef ISOSURFACE
        //Passed through isosurface?
        if (isoValue > uRange.x && ((!inside && density >= isoValue) || (inside && density < isoValue)))
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
            if (density - isoValue < 0.0)
              b = exact;
            else
              a = exact;
          }

          //Skip edges unless flagged to draw
          if (uIsoWalls > 0 || all(greaterThanEqual(pos, uBBMin)) && all(lessThanEqual(pos, uBBMax)))
          {
            vec4 value = vec4(uIsoColour.rgb, 1.0);

            vec3 normal = mat3(uNMatrix) * isoNormal(pos, shift, density);

            vec3 light = value.rgb;
            lighting(pos, normal, light);
            //Front-to-back blend equation
            colour += T * uIsoColour.a * light;
            //Render normals
            //colour += T * abs(isoNormal(pos, shift, density));
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

    if (T > 0.95) discard;
    gl_FragColor = vec4(colour, 1.0 - T);

#ifdef WRITE_DEPTH
    /* Write the depth !Not supported in WebGL without extension */
    vec4 clip_space_pos = uPMatrix * uMVMatrix * vec4(rayStart, 1.0);
    float ndc_depth = clip_space_pos.z / clip_space_pos.w;
    float depth = (((gl_DepthRange.far - gl_DepthRange.near) * ndc_depth) + 
                     gl_DepthRange.near + gl_DepthRange.far) / 2.0;
    gl_FragDepth = depth;
#endif
}

float interpolate_tricubic_fast(vec3 coord)
{
/* License applicable to this function:
Copyright (c) 2008-2013, Danny Ruijters. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
*  Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
*  Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
*  Neither the name of the copyright holders nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied.

When using this code in a scientific project, please cite one or all of the
following papers:
*  Daniel Ruijters and Philippe Thévenaz,
   GPU Prefilter for Accurate Cubic B-Spline Interpolation, 
   The Computer Journal, vol. 55, no. 1, pp. 15-20, January 2012.
*  Daniel Ruijters, Bart M. ter Haar Romeny, and Paul Suetens,
   Efficient GPU-Based Texture Interpolation using Uniform B-Splines,
   Journal of Graphics Tools, vol. 13, no. 4, pp. 61-69, 2008.
*/
  // shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
  vec3 nrOfVoxels = uResolution; //textureSize3D(tex, 0));
  vec3 coord_grid = coord * nrOfVoxels - 0.5;
  vec3 index = floor(coord_grid);
  vec3 fraction = coord_grid - index;
  vec3 one_frac = 1.0 - fraction;

  vec3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
  vec3 w1 = 2.0/3.0 - 0.5 * fraction*fraction*(2.0-fraction);
  vec3 w2 = 2.0/3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
  vec3 w3 = 1.0/6.0 * fraction*fraction*fraction;

  vec3 g0 = w0 + w1;
  vec3 g1 = w2 + w3;
  vec3 mult = 1.0 / nrOfVoxels;
  vec3 h0 = mult * ((w1 / g0) - 0.5 + index);  //h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
  vec3 h1 = mult * ((w3 / g1) + 1.5 + index);  //h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]

  // fetch the eight linear interpolations
  // weighting and fetching is interleaved for performance and stability reasons
  float tex000 = texture3Dfrom2D(h0).r;
  float tex100 = texture3Dfrom2D(vec3(h1.x, h0.y, h0.z)).r;
  tex000 = mix(tex100, tex000, g0.x);  //weigh along the x-direction
  float tex010 = texture3Dfrom2D(vec3(h0.x, h1.y, h0.z)).r;
  float tex110 = texture3Dfrom2D(vec3(h1.x, h1.y, h0.z)).r;
  tex010 = mix(tex110, tex010, g0.x);  //weigh along the x-direction
  tex000 = mix(tex010, tex000, g0.y);  //weigh along the y-direction
  float tex001 = texture3Dfrom2D(vec3(h0.x, h0.y, h1.z)).r;
  float tex101 = texture3Dfrom2D(vec3(h1.x, h0.y, h1.z)).r;
  tex001 = mix(tex101, tex001, g0.x);  //weigh along the x-direction
  float tex011 = texture3Dfrom2D(vec3(h0.x, h1.y, h1.z)).r;
  float tex111 = texture3Dfrom2D(h1).r;
  tex011 = mix(tex111, tex011, g0.x);  //weigh along the x-direction
  tex001 = mix(tex011, tex001, g0.y);  //weigh along the y-direction

  return mix(tex001, tex000, g0.z);  //weigh along the z-direction
}

