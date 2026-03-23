/*
 * shader_variables.h -- Shader variable descriptors for plugins.
 *
 * SDK version of src/shader_variables.h. Includes shader_defines.h
 * instead of defines.h.
 */

#pragma once

#include <stddef.h>

#include "shader_defines.h"

typedef enum {
    SHADER_VARIABLE_TYPE_FLOAT = 1,
    SHADER_VARIABLE_TYPE_INT = 2,
    SHADER_VARIABLE_TYPE_BOOL = 3,
    SHADER_VARIABLE_TYPE_VEC2 = 4,
    SHADER_VARIABLE_TYPE_VEC3 = 5,
    SHADER_VARIABLE_TYPE_VEC4 = 6
} shader_variable_type_t;

typedef struct {
    const char            *name;
    shader_variable_type_t type;
    size_t                 offset;
    const char            *default_value;
} shader_variable_desc_t;
