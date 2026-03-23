#pragma once

#include "shader_defines.h"

vec4_t diag_gamma_ramp_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_GAMMA_RAMP_SHADER(X) X( \
    diag_gamma_ramp, "Gamma Ramp", diag_gamma_ramp_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "Five gamma transfer rows with checkerboard half-tone reference. Rows: linear, sRGB, gamma 1.8, 2.2, 2.6. Checkerboard strip shows perceived mid-gray for visual gamma matching." \
)
