#pragma once

#include "shader_defines.h"

vec4_t diag_gradient_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_GRADIENT_SHADER(X) X( \
    diag_gradient, "RGB Gradient", diag_gradient_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "Full-range per-channel gradient. Four rows: R, G, B isolated, then combined. Reveals banding, clamping, and channel imbalance at pixel precision." \
)
