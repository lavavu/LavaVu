/*
 * Copyright (c) 2014, Monash University. All rights reserved.
 * Author: Owen Kaluza - owen.kaluza ( at ) monash.edu
 *
 * Licensed under the GNU Lesser General Public License
 * https://www.gnu.org/licenses/lgpl.html
 * (volume shader from sharevol https://github.com/OKaluza/sharevol)
 */
#ifdef WEBGL
uniform sampler2D uVolume;
#define NO_DEPTH_WRITE
#define outColour gl_FragColor
#define texture(a,b) texture2D(a,b)
#else
//Included dynamically before compile in WebGL mode...
const int maxSamples = 2048;
uniform sampler3D uVolume;
out vec4 outColour;
#endif

const float depthT = 0.99; //Transmissivity threshold below which depth write applied

uniform sampler2D uTransferFunction;

uniform vec3 uBBMin;
uniform vec3 uBBMax;
uniform vec3 uResolution;

uniform bool uEnableColour;

uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform float uPower;
uniform float uBloom;

uniform mat4 uMVMatrix;
uniform mat4 uTMVMatrix;
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
uniform bool uLighting;

vec3 bbMin;
vec3 bbMax;

#ifdef ENABLE_TRICUBIC
float interpolate_tricubic_fast(vec3 coord);
#endif

#ifdef WEBGL

vec2 islices = vec2(1.0 / slices.x, 1.0 / slices.y);
float maxslice = slices.x * slices.y - 1.0;
//Clamp to a bit before halfway for the edge voxels
vec2 cmin = vec2(0.55/(slices.x*slices.y), 0.55/(slices.x*slices.y));
vec2 cmax = vec2(1.0, 1.0) - cmin;

float sample(vec3 pos)
{
  //Get z slice index and position between two slices
  float Z = pos.z * maxslice;
  float slice = floor(Z); //Index of first slice
  Z = fract(Z);
  //Edge case at z min (possible with tricubic filtering)
  if (int(slice) < 0)
  {
    slice = 0.0;
    Z = 0.0;
  }
  //Edge case at z max
  else if (int(slice) > int(maxslice)-1)
  {
    slice = maxslice-1.0;
    Z = 1.0;
  }
  //Only start interpolation with next Z slice 1/3 from edges at first & last z slice
  //(this approximates how 3d texture volume is sampled at edges with linear filtering
  // due to edge sample being included in weighted average twice)
  // - min z slice
  else if (int(slice) == 0)
  {
    Z = max(0.0, (Z-0.33) * 1.5);
  }
  // - max z slice
  else if (int(slice) == int(maxslice)-1)
  {
    Z = min(1.0, Z*1.5);
  }

  //X & Y coords of sample scaled to slice size
  //(Clamp range at borders to prevent bleeding between tiles due to linear filtering)
  vec2 sampleOffset = clamp(pos.xy, cmin, cmax) * islices;
  //Offsets in 2D texture of given slice indices
  //(add offsets to scaled position within slice to get sample positions)
  float A = slice * islices.x;
  float B = (slice+1.0) * islices.x;
  vec2 z1offset = vec2(fract(A), floor(A) / slices.y) + sampleOffset;
  vec2 z2offset = vec2(fract(B), floor(B) / slices.y) + sampleOffset;

  //Interpolate the final value by position between slices [0,1]
  return mix(texture2D(uVolume, z1offset).x, texture2D(uVolume, z2offset).x, Z);
}

#else
#define sample(pos) (texture(uVolume, pos).x)
#endif

float tex3D(vec3 pos)
{
  float density;
//TODO: this can't be enabled outside WebGL currently
#ifdef ENABLE_TRICUBIC
  if (uFilter > 0)
    density = interpolate_tricubic_fast(pos);
  else
#endif
    density = sample(pos);

  //Normalise the density over provided range
  //(used for float textures only, all other formats are already [0,1])
  density = (density - uRange.x) / (uRange.y - uRange.x);
  //density = (density - uRange.x) * irange;
  return density;
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

vec3 isoNormal(in vec3 pos, in vec3 shift)
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
  //by central difference gradient 
  //(slower, faster way would be to precompute a gradient texture)
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
    if (any(greaterThan(uBBMin, uBBMax)))
    {
      bbMin = vec3(0.0);
      bbMax = vec3(1.0);
    }
    else
    {
      bbMin = clamp(uBBMin, vec3(0.0), vec3(1.0));
      bbMax = clamp(uBBMax, vec3(0.0), vec3(1.0));
    }

    //Compute eye space coord from window space to get the ray direction
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
    vec3 rayOrigin = (uTMVMatrix * camPos).xyz;

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
    float irange = uRange.y - uRange.x;
    if (irange <= 0.0) irange = 1.0;
    irange = 1.0 / irange;
  
    //Map colour over valid density range only
    float dRange = uDenMinMax[1] - uDenMinMax[0];

    //Raymarch, front to back
    vec3 depthHit = rayStart;
    for (int i=0; i < maxSamples; ++i)
    {
      //Render samples until we pass out back of cube or fully opaque
      if (i == samples || T < 0.01) break;
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
            vec3 light = vec3(1.0, 1.0, 1.0);
            if (uLighting)
            {
              vec3 normal = normalize((uNMatrix * vec4(isoNormal(pos, shift), 1.0)).xyz);
              light = value.rgb;
              lighting(pos, normal, light);
            }

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
          density = clamp(density, 0.0, 1.0);
          if (density < uDenMinMax[0] || density > uDenMinMax[1])
          {
            //Skip to next sample...
            pos += step;
            continue;
          }

          //Scale to density range - fits colourmap to only the displayed data range
          density = density / dRange;

          density = pow(density, uPower); //Apply power

          vec4 value;
          if (uEnableColour)
          {
            value = texture(uTransferFunction, vec2(density, 0.5));
            //Premultiply alpha
            value.rgb *= value.a;
            //Apply bloom power, makes the blending more additive
            value.a = pow(value.a, 1.0 + 3.0*uBloom);
          }
          else
            value = vec4(density);

          value *= uDensityFactor * stepSize;

          //Blending is front to back
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
    outColour = vec4(colour, 1.0 - T);

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

#ifdef ENABLE_TRICUBIC
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
*  Daniel Ruijters and Philippe Thevenaz,
   GPU Prefilter for Accurate Cubic B-Spline Interpolation, 
   The Computer Journal, vol. 55, no. 1, pp. 15-20, January 2012.
*  Daniel Ruijters, Bart M. ter Haar Romeny, and Paul Suetens,
   Efficient GPU-Based Texture Interpolation using Uniform B-Splines,
   Journal of Graphics Tools, vol. 13, no. 4, pp. 61-69, 2008.
*/
  // shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
  vec3 nrOfVoxels = uResolution;
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
  float tex000 = sample(h0);
  float tex100 = sample(vec3(h1.x, h0.y, h0.z));
  tex000 = mix(tex100, tex000, g0.x);  //weigh along the x-direction
  float tex010 = sample(vec3(h0.x, h1.y, h0.z));
  float tex110 = sample(vec3(h1.x, h1.y, h0.z));
  tex010 = mix(tex110, tex010, g0.x);  //weigh along the x-direction
  tex000 = mix(tex010, tex000, g0.y);  //weigh along the y-direction
  float tex001 = sample(vec3(h0.x, h0.y, h1.z));
  float tex101 = sample(vec3(h1.x, h0.y, h1.z));
  tex001 = mix(tex101, tex001, g0.x);  //weigh along the x-direction
  float tex011 = sample(vec3(h0.x, h1.y, h1.z));
  float tex111 = sample(h1);
  tex011 = mix(tex111, tex011, g0.x);  //weigh along the x-direction
  tex001 = mix(tex011, tex001, g0.y);  //weigh along the y-direction

  return mix(tex001, tex000, g0.z);  //weigh along the z-direction
}
#endif
