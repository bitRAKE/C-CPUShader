#pragma once

#include "shader_defines.h"

vec4_t master_class_hdr10_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MASTER_CLASS_HDR10_SHADER(X) X( \
    master_class_hdr10, "Master Class HDR10", master_class_hdr10_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_HDR10_ST2084, \
    SHADER_FEATURE_NONE, \
    1024, 1024, \
    "HDR10-authored master class variant that encodes the shared scene into BT.2020 / ST.2084 with fixed in-shader multi-sampling." \
)
