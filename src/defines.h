#pragma once

#include <stdio.h>
#include <windows.h>
#include <stdlib.h>

#include "vmath.h"

#define uint unsigned int

#define bool int
#define true 1
#define false 0

typedef struct {
    vec2_t resolution;
    float  time;
    uint   frame;
    vec4_t mouse;
} shader_uniforms_t;
