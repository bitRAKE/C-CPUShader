/*
 * shader_defines.h -- Platform-neutral shader contract for plugins.
 *
 * SDK version of src/defines.h. Removes <windows.h>, <stdio.h>, <stdlib.h>
 * and uses <stdbool.h> / <stdint.h> instead of bare macros.
 *
 * Struct layouts are ABI-compatible with the host's src/defines.h.
 */

#pragma once

/*
 * The host defines bool as int (4 bytes) via macro. The SDK must match
 * for struct layout compatibility. Do NOT use <stdbool.h> here.
 */
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif

#include <stddef.h>

#include "shader_vmath.h"
#include "shader_random.h"
#include "shader_color.h"
#include "shader_ray.h"
#include "shader_composite.h"

typedef unsigned int uint;

/* min/max -- provided by <windows.h> in the host build, needed here for plugins */
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define SHADER_BUFFER_NONE NULL
#define SHADER_VARIABLE_NONE NULL, 0, 0

enum {
    SHADER_FEATURE_NONE                  = 0,
    SHADER_FEATURE_TIME                  = 1u << 0,
    SHADER_FEATURE_MOUSE                 = 1u << 1,
    SHADER_FEATURE_KEYS                  = 1u << 2,
    SHADER_FEATURE_FRAME                 = 1u << 3,
    SHADER_FEATURE_TEMPORAL_ACCUMULATION = 1u << 4
};

typedef enum {
    SHADER_COLOR_SPACE_SDR_DISPLAY = 0,
    SHADER_COLOR_SPACE_SCENE_LINEAR,
    SHADER_COLOR_SPACE_HDR10_ST2084
} shader_color_space_t;

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
    const void    *variables;
} shader_uniforms_t;
