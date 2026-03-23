/*
 * shader_composite.h -- Alpha compositing and SDF helpers for shader plugins.
 *
 * Porter-Duff "over" operation and SDF-to-alpha conversion utilities.
 *
 * Define SHADER_COMPOSITE_IMPL before including this header in *one* C file
 * to create the implementation.
 */

#pragma once

#include "shader_vmath.h"

vec4_t shader_layer_over(vec4_t dst, vec4_t src);
float  shader_sdf_fill(float sdf, float aa);
float  shader_sdf_band(float sdf, float inner, float outer);

#ifdef SHADER_COMPOSITE_IMPL

vec4_t shader_layer_over(vec4_t dst, vec4_t src)
{
    float out_alpha = src.w + dst.w * (1.0f - src.w);
    vec3_t premul;

    if (out_alpha <= 0.000001f) {
        return vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    premul = v3_add(
        v3_mul1(vec3(src.x, src.y, src.z), src.w),
        v3_mul1(vec3(dst.x, dst.y, dst.z), dst.w * (1.0f - src.w)));

    return vec4(premul.x / out_alpha, premul.y / out_alpha, premul.z / out_alpha, out_alpha);
}

float shader_sdf_fill(float sdf, float aa)
{
    return smoothstepf(aa, -aa, sdf);
}

float shader_sdf_band(float sdf, float inner, float outer)
{
    return smoothstepf(inner, outer, sdf);
}

#endif /* SHADER_COMPOSITE_IMPL */
