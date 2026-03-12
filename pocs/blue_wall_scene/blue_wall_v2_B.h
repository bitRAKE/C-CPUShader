#pragma once

#include "../../src/defines.h"

vec4_t blue_wall_v2_B_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BLUE_WALL_V2_B_SHADER(X) X( \
    blue_wall_v2_B, "Blue Wall v2 B", blue_wall_v2_B_main, \
    NULL, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Blue Wall v2 Stage B: generated layout volumes from the new grouped scene data. <a href=\"https://blog.polyhaven.com/blue-wall-scene-file/\">Source: Greg Zaal / Poly Haven</a>" \
)
