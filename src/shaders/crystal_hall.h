#pragma once

#include "../defines.h"

vec4_t crystal_hall_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define CRYSTAL_HALL_SHADER(X) X( \
    crystal_hall, "Crystal Hall", crystal_hall_main, \
    NULL, \
    SHADER_FEATURE_TIME, \
    1024, 1024, \
    "Animated 3D hall study with live-preview lighting and tone mapping." \
)
