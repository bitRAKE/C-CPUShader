#pragma once

#include "shader_defines.h"

vec4_t colorspace_sdr_ui_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define COLORSPACE_SDR_UI_SHADER(X) X( \
    colorspace_sdr_ui, "Color Space SDR UI", colorspace_sdr_ui_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1280, 720, \
    "Display-referred SDR probe. Dark ramps and vivid UI swatches should stay balanced; if treated as scene-linear, midtones lift and the image washes out." \
)
