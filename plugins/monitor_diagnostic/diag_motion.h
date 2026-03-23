#pragma once

#include "shader_defines.h"

vec4_t diag_motion_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_MOTION_SHADER(X) X( \
    diag_motion, "Motion Response", diag_motion_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TIME | SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Moving blocks at 1/2/4/8 pixels-per-frame across four lanes. Exact integer speed reveals ghosting, overshoot, and response-time asymmetry per color channel." \
)
