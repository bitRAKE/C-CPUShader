#pragma once

#include "../defines.h"

vec4_t kinetic_orbs_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define KINETIC_ORBS_SHADER(X) X( \
    kinetic_orbs, "Kinetic Orbs", kinetic_orbs_main, \
    NULL, \
    SHADER_FEATURE_TIME, \
    1024, 1024, \
    "Animated metaball-style color study with no temporal accumulation." \
)
