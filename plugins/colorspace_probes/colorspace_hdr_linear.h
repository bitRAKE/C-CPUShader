#pragma once

#include "shader_defines.h"

vec4_t colorspace_hdr_linear_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define COLORSPACE_HDR_LINEAR_SHADER(X) X( \
    colorspace_hdr_linear, "Color Space HDR Linear", colorspace_hdr_linear_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SCENE_LINEAR, \
    SHADER_FEATURE_NONE, \
    1280, 720, \
    "Scene-linear HDR probe. Highlight ladders and overbright color patches should separate on an HDR surface and collapse toward flat clipping on SDR." \
)
