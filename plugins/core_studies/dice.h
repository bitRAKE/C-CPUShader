#pragma once

#include "shader_defines.h"

vec4_t dice_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DICE_SHADER(X) X( \
    dice, "Dice", dice_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SCENE_LINEAR, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 768, \
    "Classic PNG dice test image. Four translucent colored dice on a " \
    "neutral surface, rendered via SDF sphere-tracing with Beer-Lambert " \
    "glass absorption, Fresnel reflection/refraction, and soft shadows. " \
    "Converges over 64\xe2\x80\x93""256 frames." \
)
