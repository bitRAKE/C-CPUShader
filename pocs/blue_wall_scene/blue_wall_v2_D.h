#pragma once

#include "../../src/defines.h"

vec4_t blue_wall_v2_D_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BLUE_WALL_V2_D_SHADER(X) X( \
    blue_wall_v2_D, "Blue Wall v2 D", blue_wall_v2_D_main, \
    NULL, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Blue Wall v2 Stage D: authored hero pass with refined furniture and sideboard-top props, still without texture buffers. <a href=\"https://blog.polyhaven.com/blue-wall-scene-file/\">Source: Greg Zaal / Poly Haven</a>" \
)
