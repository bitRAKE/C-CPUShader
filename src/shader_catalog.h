#pragma once

#include "defines.h"
#include "shader_buffers.h"

typedef vec4_t (*RenderFunc)(vec2_t fragCoord, const shader_uniforms_t *uniforms);

typedef struct {
    const char *id;
    const char *display_name;
    const char *blurb;
    RenderFunc  render;
    ShaderBuffersInitFunc buffers_init;
    uint        feature_flags;
    int         preferred_width;
    int         preferred_height;
} shader_desc_t;
