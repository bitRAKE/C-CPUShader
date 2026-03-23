#pragma once

#include "shader_defines.h"

vec4_t diag_hdr_gamut_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_HDR_GAMUT_SHADER(X) X( \
    diag_hdr_gamut, "HDR Wide Gamut", diag_hdr_gamut_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SCENE_LINEAR, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "scRGB wide-gamut proof. Three rows of saturated color patches: " \
    "BT.709 (standard sRGB primaries), DCI-P3, and BT.2020 &mdash; " \
    "each expressed via scRGB negative values. On a standard-gamut " \
    "display the rows look identical (negatives clamp). On a " \
    "wide-gamut display each successive row shows more vivid color, " \
    "proving the extended primaries reach the panel." \
)
