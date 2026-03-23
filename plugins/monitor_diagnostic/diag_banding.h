#pragma once

#include "shader_defines.h"

vec4_t diag_banding_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_BANDING_SHADER(X) X( \
    diag_banding, "Banding Steps", diag_banding_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "Quantized gradient rows at 8/16/32/64/128/256 levels across R, G, B, and gray. Exposes per-channel bit depth and dithering artifacts." \
)
