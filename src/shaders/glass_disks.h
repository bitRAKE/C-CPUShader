#pragma once

#include "../defines.h"

vec4_t glass_disks_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define GLASS_DISKS_SHADER(X) X( \
    glass_disks, "Glass Disks", glass_disks_main, \
    NULL, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "Dense 2D refractive disk cluster emphasizing ribbon-like caustic paths." \
)
