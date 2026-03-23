/*
 * u_vars.h -- Shader variable access helper for plugins.
 *
 * SDK version of src/u_vars.h. Provides U_VARS_AS macro for safe
 * typed access to shader variable structs.
 */

#pragma once

#include "shader_defines.h"

static inline const void *u_vars_ptr(const shader_uniforms_t *uniforms)
{
    if (uniforms == NULL) {
        return NULL;
    }

    return uniforms->variables;
}

#define U_VARS_AS(type, uniforms) ((const type *)u_vars_ptr(uniforms))
