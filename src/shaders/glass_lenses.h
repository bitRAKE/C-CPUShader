#pragma once

#include "../defines.h"

vec4_t glass_lenses_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define GLASS_LENSES_SHADER(X) X( \
    glass_lenses, "Glass Lenses", glass_lenses_main, \
    NULL, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "2D refractive lens field with progressive spectral caustics." \
)
