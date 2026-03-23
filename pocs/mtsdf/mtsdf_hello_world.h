#pragma once

#include "shader_defines.h"
#include "shader_buffers.h"

vec4_t mtsdf_hello_world_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MTSDF_HELLO_WORLD_SHADER(X) X( \
    mtsdf_hello_world, "MTSDF Hello World", mtsdf_hello_world_main, \
    mtsdf_hello_world_buffers_init, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TIME, \
    1920, 1080, \
    "MTSDF text POC: ASCII atlas generated from msdfgen and rendered as rainbow bubble text with giant animated glyphs from local shader buffers. <a href=\"https://github.com/Chlumsky/msdfgen\">Source: Viktor Chlumsky / msdfgen</a>" \
)

ShaderBuffersCleanupFunc mtsdf_hello_world_buffers_init(shader_buffers_t *buffers, const shader_host_services_t *services, char *error, size_t error_size);
