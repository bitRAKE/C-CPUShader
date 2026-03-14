#pragma once

#include "../../src/defines.h"
#include "../../src/shader_buffers.h"

vec4_t blue_wall_v2_E_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BLUE_WALL_V2_E_SHADER(X) X( \
    blue_wall_v2_E, "Blue Wall v2 E", blue_wall_v2_E_main, \
    blue_wall_v2_E_buffers_init, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Blue Wall v2 Stage E: Stage D plus shader-owned texture buffers, starting with painting.jpg. <a href=\"https://blog.polyhaven.com/blue-wall-scene-file/\">Source: Greg Zaal / Poly Haven</a>" \
)

ShaderBuffersCleanupFunc blue_wall_v2_E_buffers_init(shader_buffers_t *buffers, char *error, size_t error_size);
