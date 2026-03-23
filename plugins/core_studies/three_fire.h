#pragma once
#include "shader_defines.h"
vec4_t three_fire_main(vec2_t fragCoord, const shader_uniforms_t* uniforms);

#define THREE_FIRE_SHADER(X) X( \
    three_fire, "Three Fire", three_fire_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TIME, \
    128, 512, \
    "Adaptation of <a href=\"https://fragcoord.xyz/s/3zoe0vgo\">3D Fire</a>. " \
    "From <a href=\"https://x.com/XorDev\">@XorDev</a>'s " \
    "<a href=\"https://www.xordev.com/arsenal\">Shader Arsenal</a>." \
)
