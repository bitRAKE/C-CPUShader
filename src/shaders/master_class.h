#pragma once

#include "shader_defines.h"

vec4_t master_class_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MASTER_CLASS_SHADER(X) X( \
    master_class, "Master Class", master_class_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "Flagship static interior path-tracing study with progressive accumulation." \
)
