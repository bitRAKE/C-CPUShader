#pragma once

#include "defines.h"

#define U_MOUSE_HAS_POSITION(u) ((u) != NULL && (u)->mouse.x >= 0.0f && (u)->mouse.y >= 0.0f)
#define U_MOUSE_IS_DRAGGING(u)  ((u) != NULL && (u)->mouse.z >= 0.0f && (u)->mouse.w >= 0.0f)

static inline vec2_t u_mouse_current(const shader_uniforms_t *uniforms)
{
    if (uniforms == NULL) {
        return vec2(-1.0f, -1.0f);
    }

    if (U_MOUSE_IS_DRAGGING(uniforms)) {
        return vec2(uniforms->mouse.z, uniforms->mouse.w);
    }

    return vec2(uniforms->mouse.x, uniforms->mouse.y);
}

static inline vec2_t u_mouse_anchor(const shader_uniforms_t *uniforms)
{
    if (uniforms == NULL) {
        return vec2(-1.0f, -1.0f);
    }

    return vec2(uniforms->mouse.x, uniforms->mouse.y);
}
