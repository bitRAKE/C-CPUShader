#pragma once

#include "shader_defines.h"

vec4_t master_class_scrgb_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MASTER_CLASS_SCRGB_SHADER(X) X( \
    master_class_scrgb, "Master Class scRGB", master_class_scrgb_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SCENE_LINEAR, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "HDR-authored master class variant with scene-linear output, a brighter area light, and saturated accent emitters for scRGB / CCCS presentation." \
)
