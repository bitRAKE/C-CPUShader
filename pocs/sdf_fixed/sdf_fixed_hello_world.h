#pragma once

#include "shader_defines.h"
#include "shader_buffers.h"

vec4_t sdf_fixed_hello_world_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define SDF_FIXED_HELLO_WORLD_SHADER(X) X( \
    sdf_fixed_hello_world, "SDF Grid Hello World", sdf_fixed_hello_world_main, \
    sdf_fixed_hello_world_buffers_init, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "Low-tech SDF text POC: fixed ASCII sub-image grid, no JSON metrics, and a simple shader-side map. <a href=\"https://github.com/Chlumsky/msdfgen\">Source: Viktor Chlumsky / msdfgen</a>" \
)

ShaderBuffersCleanupFunc sdf_fixed_hello_world_buffers_init(shader_buffers_t *buffers, const shader_host_services_t *services, char *error, size_t error_size);
