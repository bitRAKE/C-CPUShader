#pragma once

#include "../../src/defines.h"

#define ORBIT_STARS_LOOP_FRAMES 30u

vec4_t orbit_stars_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define ORBIT_STARS_SHADER(X) X( \
    orbit_stars, "Orbit Stars", orbit_stars_main, \
    NULL, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_FRAME, \
    128, 64, \
    "Low-resolution frame-driven star orbit study for capture and timing work, with a deterministic 30-frame loop and a subtle directional trail." \
)
