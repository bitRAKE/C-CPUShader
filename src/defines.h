#pragma once

#include <stdio.h>
#include <windows.h>
#include <stdlib.h>

#include "vmath.h"

#define uint unsigned int

#define bool int
#define true 1
#define false 0

enum {
    SHADER_FEATURE_NONE                  = 0,
    SHADER_FEATURE_TIME                  = 1u << 0,
    SHADER_FEATURE_MOUSE                 = 1u << 1,
    SHADER_FEATURE_KEYS                  = 1u << 2,
    SHADER_FEATURE_FRAME                 = 1u << 3,
    SHADER_FEATURE_TEMPORAL_ACCUMULATION = 1u << 4
};

typedef struct {
    uint words[8];
} shader_keys_t;

typedef struct shader_buffers_t shader_buffers_t;

typedef struct {
    vec2_t         resolution;
    float          time;
    uint           frame;
    vec4_t         mouse;
    shader_keys_t  keys;
    const shader_buffers_t *buffers;
} shader_uniforms_t;
