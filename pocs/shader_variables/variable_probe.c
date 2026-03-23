#include "variable_probe.h"

#include "u_vars.h"

#include <stddef.h>

const shader_variable_desc_t g_variable_probe_variables[VARIABLE_PROBE_VARIABLE_COUNT] = {
    {"exposure", SHADER_VARIABLE_TYPE_FLOAT, offsetof(variable_probe_vars_t, exposure), "1.0"},
    {"stripe_count", SHADER_VARIABLE_TYPE_INT, offsetof(variable_probe_vars_t, stripe_count), "7"},
    {"invert", SHADER_VARIABLE_TYPE_BOOL, offsetof(variable_probe_vars_t, invert), "false"},
    {"offset", SHADER_VARIABLE_TYPE_VEC2, offsetof(variable_probe_vars_t, offset), "0.0, 0.0"},
    {"tint", SHADER_VARIABLE_TYPE_VEC3, offsetof(variable_probe_vars_t, tint), "0.95, 0.55, 0.20"},
    {"vignette", SHADER_VARIABLE_TYPE_VEC4, offsetof(variable_probe_vars_t, vignette), "0.02, 0.04, 0.10, 0.85"},
};

static const variable_probe_vars_t g_variable_probe_fallback = {
    1.0f,
    7,
    false,
    {0.0f, 0.0f},
    {0.95f, 0.55f, 0.20f},
    {0.02f, 0.04f, 0.10f, 0.85f},
};

vec4_t variable_probe_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const variable_probe_vars_t *vars = U_VARS_AS(variable_probe_vars_t, uniforms);
    vec2_t resolution;
    float x;
    float y;
    vec2_t shifted_uv;
    int stripe_count;
    float stripe_wave;
    float band_wave;
    vec3_t color;
    float edge_distance;
    float vignette_strength;

    if (vars == NULL) {
        vars = &g_variable_probe_fallback;
    }

    resolution = (uniforms != NULL) ? uniforms->resolution : vec2(1.0f, 1.0f);
    x = (fragCoord.x + 0.5f) / max(1.0f, resolution.x);
    y = (fragCoord.y + 0.5f) / max(1.0f, resolution.y);
    shifted_uv = vec2(x + vars->offset.x, y + vars->offset.y);

    stripe_count = vars->stripe_count;
    if (stripe_count < 0) {
        stripe_count = -stripe_count;
    }
    if (stripe_count < 1) {
        stripe_count = 1;
    }

    stripe_wave = 0.5f + 0.5f * sinf(shifted_uv.x * PI * 2.0f * (float)stripe_count);
    band_wave = 0.5f + 0.5f * cosf(shifted_uv.y * PI * 2.0f * ((float)stripe_count * 0.5f + 1.0f));

    color = v3_lerp(vec3(0.05f, 0.07f, 0.11f), vars->tint, stripe_wave);
    color = v3_mul1(color, 0.30f + 0.70f * band_wave);

    edge_distance = fmaxf(fabsf(x - 0.5f) * 2.0f, fabsf(y - 0.5f) * 2.0f);
    vignette_strength = smoothstepf(0.15f, 1.0f, edge_distance) * saturate(vars->vignette.w);
    color = v3_lerp(color, vec3(vars->vignette.x, vars->vignette.y, vars->vignette.z), vignette_strength);
    color = v3_mul1(color, vars->exposure);

    if (vars->invert) {
        color = vec3(1.0f - color.x, 1.0f - color.y, 1.0f - color.z);
    }

    return vec4(saturate(color.x), saturate(color.y), saturate(color.z), 1.0f);
}
