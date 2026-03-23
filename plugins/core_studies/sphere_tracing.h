#pragma once

#include "shader_defines.h"

vec4_t sphere_tracing_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define SPHERE_TRACING_SHADER(X) X( \
    sphere_tracing, "Sphere Tracing", sphere_tracing_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SCENE_LINEAR, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "Progressive 3D study with stochastic diffuse transport." \
)
