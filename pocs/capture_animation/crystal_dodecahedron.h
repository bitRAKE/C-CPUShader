#pragma once

#include "shader_defines.h"

#define CRYSTAL_DODECAHEDRON_LOOP_FRAMES 90u

vec4_t crystal_dodecahedron_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define CRYSTAL_DODECAHEDRON_SHADER(X) X( \
    crystal_dodecahedron, "Crystal Dodecahedron", crystal_dodecahedron_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_FRAME, \
    256, 256, \
    "Rotating iridescent hollow dodecahedron with correct alpha. Adaptation of <a href=\"https://fragcoord.xyz/s/cq9lkc9h\">Crystal</a> by @XorDev. Deterministic 90-frame seamless loop for capture to APNG/WEBP." \
)
