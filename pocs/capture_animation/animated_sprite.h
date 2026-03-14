#pragma once

#include "../../src/defines.h"

#define ANIMATED_SPRITE_LOOP_FRAMES 48u

vec4_t animated_sprite_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define ANIMATED_SPRITE_SHADER(X) X( \
    animated_sprite, "Animated Sprite", animated_sprite_main, \
    NULL, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_FRAME, \
    128, 64, \
    "Deterministic sprite-animation POC for frame capture and sequence-to-animation workflows, with a fixed 48-frame loop, transparent output, and animated alpha." \
)
