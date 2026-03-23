/*
 * shader_random.h -- Deterministic PRNG for shader plugins.
 *
 * PCG-based random number generation. Deterministic given the same seed,
 * suitable for per-pixel randomness seeded from coordinates.
 *
 * Define SHADER_RANDOM_IMPL before including this header in *one* C file
 * to create the implementation.
 */

#pragma once

#include "shader_vmath.h"

typedef unsigned int uint;

uint  shader_next_rand(uint *state);
float shader_rand_1(uint *state);
float shader_rand_1_nd(uint *state);
vec3_t shader_rand_dir(uint *state);

#ifdef SHADER_RANDOM_IMPL

uint shader_next_rand(uint *state)
{
    *state = *state * 747796405 + 2891336453;
    uint result = ((*state >> ((*state >> 28) + 4)) ^ *state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}

float shader_rand_1(uint *state)
{
    return shader_next_rand(state) / 4294967295.0f;
}

float shader_rand_1_nd(uint *state)
{
    float theta = 2.0f * PI * shader_rand_1(state);
    float rho = sqrtf(-2.0f * logf(shader_rand_1(state)));
    return rho * cosf(theta);
}

vec3_t shader_rand_dir(uint *state)
{
    float x = shader_rand_1_nd(state);
    float y = shader_rand_1_nd(state);
    float z = shader_rand_1_nd(state);
    return v3_normalize(vec3(x, y, z));
}

#endif /* SHADER_RANDOM_IMPL */
