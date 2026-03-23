#pragma once

#include "shader_defines.h"

vec4_t blue_wall_v2_C_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BLUE_WALL_V2_C_SHADER(X) X( \
    blue_wall_v2_C, "Blue Wall v2 C", blue_wall_v2_C_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Blue Wall v2 Stage C: generated reconstruction skeleton driven by grouped fixture and furniture proxies. <a href=\"https://blog.polyhaven.com/blue-wall-scene-file/\">Source: Greg Zaal / Poly Haven</a>" \
)
