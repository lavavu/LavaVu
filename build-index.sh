#!/bin/bash
IN=${1}
OUT=${2}
SHADERS=${3}
sed -e "/Point vertex shader/   r ${SHADERS}/pointShaderWEBGL.vert" \
    -e "/Point fragment shader/ r ${SHADERS}/pointShaderWEBGL.frag" \
    -e "/Tri vertex shader/     r ${SHADERS}/triShaderWEBGL.vert"   \
    -e "/Tri fragment shader/   r ${SHADERS}/triShaderWEBGL.frag"   \
    -e "/Line vertex shader/    r ${SHADERS}/lineShader.vert"       \
    -e "/Line fragment shader/  r ${SHADERS}/lineShader.frag" < ${IN} > ${OUT}

