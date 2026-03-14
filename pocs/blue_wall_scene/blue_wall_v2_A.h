#pragma once

#include "../../src/defines.h"

vec4_t blue_wall_v2_A_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BLUE_WALL_V2_A_SHADER(X) X( \
    blue_wall_v2_A, "Blue Wall v2 A", blue_wall_v2_A_main, \
    NULL, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1920, 1080, \
    "Blue Wall v2 Stage A: honest room, camera, and light baseline rebuilt from the new extraction pipeline. <a href=\"https://blog.polyhaven.com/blue-wall-scene-file/\">Source: Greg Zaal / Poly Haven</a>" \
)
