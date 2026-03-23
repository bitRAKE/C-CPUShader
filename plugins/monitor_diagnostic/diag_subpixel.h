#pragma once

#include "shader_defines.h"

vec4_t diag_subpixel_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_SUBPIXEL_SHADER(X) X( \
    diag_subpixel, "Sub-pixel Grid", diag_subpixel_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "Pixel-exact test patterns: 1px on/off, 2px, 4px grids across R/G/B/W. Detects dead pixels, stuck sub-pixels, and scaling artifacts. Impossible to reproduce accurately in a browser." \
)
