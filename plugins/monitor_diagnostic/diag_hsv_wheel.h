#pragma once

#include "shader_defines.h"

vec4_t diag_hsv_wheel_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_HSV_WHEEL_SHADER(X) X( \
    diag_hsv_wheel, "HSV Wheel", diag_hsv_wheel_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1080, 1080, \
    "Full HSV color wheel with saturation gradient, primary/secondary hue markers, and value ring. Exposes gamut gaps and hue-shift artifacts." \
)
