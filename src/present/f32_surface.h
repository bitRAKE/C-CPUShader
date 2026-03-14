#pragma once

#include "../defines.h"

typedef struct {
    int           width;
    int           height;
    int           stride_bytes;
    const vec4_t *pixels;
} f32x4_surface_t;
