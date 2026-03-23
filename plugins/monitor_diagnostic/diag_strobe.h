#pragma once

#include "shader_defines.h"

vec4_t diag_strobe_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_STROBE_SHADER(X) X( \
    diag_strobe, "Strobe Flicker", diag_strobe_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TIME | SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Frame-accurate flicker test. Six zones at 1/2/3/4/6/8 frame periods. Exposes PWM flicker, backlight strobing, and frame-hold persistence." \
)
