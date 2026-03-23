#pragma once

#include "shader_defines.h"

vec4_t kinetic_orbs_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define KINETIC_ORBS_SHADER(X) X( \
    kinetic_orbs, "Kinetic Orbs", kinetic_orbs_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TIME, \
    1024, 1024, \
    "Animated metaball-style color study with no temporal accumulation." \
)
