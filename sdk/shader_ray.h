/*
 * shader_ray.h -- Ray intersection primitives and optics for shader plugins.
 *
 * Common ray-geometry tests and Fresnel approximation. All intersection
 * functions return the hit distance t, or -1.0f on miss.
 *
 * Define SHADER_RAY_IMPL before including this header in *one* C file
 * to create the implementation.
 */

#pragma once

#include "shader_vmath.h"

float shader_ray_sphere(vec3_t ro, vec3_t rd, vec3_t center, float radius);
float shader_ray_plane(vec3_t ro, vec3_t rd, vec3_t plane_point, vec3_t plane_normal);
float shader_ray_circle(vec2_t ro, vec2_t rd, vec2_t center, float radius);
float shader_schlick(float cosine, float eta_i, float eta_t);

#ifdef SHADER_RAY_IMPL

float shader_ray_sphere(vec3_t ro, vec3_t rd, vec3_t center, float radius)
{
    vec3_t offset = v3_sub(ro, center);
    float b = 2.0f * v3_dot(rd, offset);
    float c = v3_dot(offset, offset) - radius * radius;
    float h = b * b - 4.0f * c;

    if (h < 0.0f) {
        return -1.0f;
    }

    h = sqrtf(h);

    {
        float near_hit = (-b - h) * 0.5f;
        if (near_hit > 0.001f) {
            return near_hit;
        }
    }

    {
        float far_hit = (-b + h) * 0.5f;
        if (far_hit > 0.001f) {
            return far_hit;
        }
    }

    return -1.0f;
}

float shader_ray_plane(vec3_t ro, vec3_t rd, vec3_t plane_point, vec3_t plane_normal)
{
    float denom = v3_dot(rd, plane_normal);

    if (fabsf(denom) < 0.0001f) {
        return -1.0f;
    }

    {
        float distance = v3_dot(v3_sub(plane_point, ro), plane_normal) / denom;
        return (distance > 0.001f) ? distance : -1.0f;
    }
}

float shader_ray_circle(vec2_t ro, vec2_t rd, vec2_t center, float radius)
{
    vec2_t oc = v2_sub(ro, center);
    float b = v2_dot(oc, rd);
    float c = v2_dot(oc, oc) - radius * radius;
    float h = b * b - c;

    if (h < 0.0f) {
        return -1.0f;
    }

    h = sqrtf(h);

    float near_hit = -b - h;
    if (near_hit > 0.0001f) {
        return near_hit;
    }

    float far_hit = -b + h;
    if (far_hit > 0.0001f) {
        return far_hit;
    }

    return -1.0f;
}

float shader_schlick(float cosine, float eta_i, float eta_t)
{
    float r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

#endif /* SHADER_RAY_IMPL */
