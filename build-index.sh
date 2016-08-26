#!/bin/bash
IN=${1}
OUT=${2}
SHADERS=${3}
sed -e "/Point vertex shader/    r ${SHADERS}/pointShaderWEBGL.vert" \
    -e "/Point fragment shader/  r ${SHADERS}/pointShaderWEBGL.frag" \
    -e "/Tri vertex shader/      r ${SHADERS}/triShaderWEBGL.vert"   \
    -e "/Tri fragment shader/    r ${SHADERS}/triShaderWEBGL.frag"   \
    -e "/Volume vertex shader/   r ${SHADERS}/volumeShaderWEBGL.vert"   \
    -e "/Volume fragment shader/ r ${SHADERS}/volumeShaderWEBGL.frag"   \
    -e "/Line vertex shader/     r ${SHADERS}/lineShaderWEBGL.vert"       \
    -e "/Line fragment shader/   r ${SHADERS}/lineShaderWEBGL.frag" < ${IN} > ${OUT}

