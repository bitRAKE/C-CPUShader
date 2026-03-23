/*
 * shader_catalog.h -- Shader descriptor for plugins.
 *
 * SDK version of src/shader_catalog.h. Includes SDK headers
 * instead of src/ headers.
 */

#pragma once

#include "shader_defines.h"
#include "shader_buffers.h"
#include "shader_variables.h"

typedef vec4_t (*RenderFunc)(vec2_t fragCoord, const shader_uniforms_t *uniforms);

typedef struct {
    const char *id;
    const char *display_name;
    const char *blurb;
    RenderFunc  render;
    ShaderBuffersInitFunc buffers_init;
    const shader_variable_desc_t *variables;
    int         variable_count;
    size_t      variable_struct_size;
    shader_color_space_t generated_color_space;
    uint        feature_flags;
    int         preferred_width;
    int         preferred_height;
} shader_desc_t;
