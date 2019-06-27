in vec3 aVertexPosition;
in vec4 aVertexColour;
in vec2 aVertexTexCoord;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

out vec4 vColour;

#ifdef WEBGL
const int uPointDist = 1;   // Scale by distance
#else
uniform int uPointDist;   // Scale by distance
#endif

in float aSize;
in float aPointType;

uniform float uPointScale;   // scale to calculate size in pixels

uniform vec4 uColour;
uniform float uOpacity;

out vec3 vVertex;
out vec2 vTexCoord;
out float vPointSize;
out vec3 vPosEye;
out float vPointType;

void main(void)
{
  float pSize = abs(aSize);

  // calculate window-space point size
  vec4 mvPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
  vec3 posEye = mvPosition.xyz;
  float dist = 1.0;
  if (uPointDist > 0)
     dist = length(posEye);
  //Limit scaling, overly large points are very slow to render
  //gl_PointSize = max(1.0, min(40.0, uPointScale * pSize / dist));

  gl_PointSize = uPointScale * pSize / dist;
  gl_Position = uPMatrix * mvPosition;

  vPosEye = posEye;
  vPointType = aPointType;
  vPointSize = gl_PointSize;
  vVertex = aVertexPosition.xyz;
  vTexCoord = aVertexTexCoord;

  if (uColour.a > 0.0)
    vColour = uColour;
  else
    vColour = vec4(aVertexColour.rgb, aVertexColour.a);
}

