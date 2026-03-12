#pragma once

#include "../defines.h"

vec4_t master_class_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MASTER_CLASS_SHADER(X) X( \
    master_class, "Master Class", master_class_main, \
    NULL, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "Flagship static interior path-tracing study with progressive accumulation." \
)
