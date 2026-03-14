#pragma once

#include "../defines.h"

vec4_t colorspace_hdr10_pq_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define COLORSPACE_HDR10_PQ_SHADER(X) X( \
    colorspace_hdr10_pq, "Color Space HDR10 PQ", colorspace_hdr10_pq_main, \
    NULL, \
    SHADER_COLOR_SPACE_HDR10_ST2084, \
    SHADER_FEATURE_NONE, \
    1280, 720, \
    "HDR10-authored probe in ST.2084/PQ space. Nit-stepped patches and BT.2020 color blocks should stay composed on an HDR10 surface and look obviously wrong if treated as linear." \
)
