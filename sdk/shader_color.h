/*
 * shader_color.h -- Color space conversions and tonemapping for shader plugins.
 *
 * HSV, ACES filmic tonemapping, gamma encoding/decoding.
 *
 * Define SHADER_COLOR_IMPL before including this header in *one* C file
 * to create the implementation.
 */

#pragma once

#include "shader_vmath.h"

vec3_t shader_hsv_to_rgb(float h, float s, float v);
vec3_t shader_hsv_to_rgb_v(vec3_t hsv);
vec3_t shader_aces_tonemap(vec3_t color);
vec3_t shader_gamma_encode(vec3_t color);
vec3_t shader_gamma_decode(vec3_t color);

#ifdef SHADER_COLOR_IMPL

vec3_t shader_hsv_to_rgb(float h, float s, float v)
{
    float c = v * s;
    float hp = h * 6.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;
    vec3_t rgb;

    if      (hp < 1.0f) rgb = vec3(c, x, 0.0f);
    else if (hp < 2.0f) rgb = vec3(x, c, 0.0f);
    else if (hp < 3.0f) rgb = vec3(0.0f, c, x);
    else if (hp < 4.0f) rgb = vec3(0.0f, x, c);
    else if (hp < 5.0f) rgb = vec3(x, 0.0f, c);
    else                rgb = vec3(c, 0.0f, x);

    return v3_add1(rgb, m);
}

vec3_t shader_hsv_to_rgb_v(vec3_t hsv)
{
    return shader_hsv_to_rgb(hsv.x, hsv.y, hsv.z);
}

vec3_t shader_aces_tonemap(vec3_t color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    color = v3_mul1(color, 1.12f);
    return vec3(
        saturate((color.x * (a * color.x + b)) / (color.x * (c * color.x + d) + e)),
        saturate((color.y * (a * color.y + b)) / (color.y * (c * color.y + d) + e)),
        saturate((color.z * (a * color.z + b)) / (color.z * (c * color.z + d) + e)));
}

vec3_t shader_gamma_encode(vec3_t color)
{
    return vec3(
        powf(saturate(color.x), 1.0f / 2.2f),
        powf(saturate(color.y), 1.0f / 2.2f),
        powf(saturate(color.z), 1.0f / 2.2f));
}

vec3_t shader_gamma_decode(vec3_t color)
{
    return vec3(
        powf(saturate(color.x), 2.2f),
        powf(saturate(color.y), 2.2f),
        powf(saturate(color.z), 2.2f));
}

#endif /* SHADER_COLOR_IMPL */
