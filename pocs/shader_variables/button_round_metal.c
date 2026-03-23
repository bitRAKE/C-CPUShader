#include "button_round_metal.h"

#include "u_vars.h"

#include <stddef.h>

const shader_variable_desc_t g_button_round_metal_variables[BUTTON_ROUND_METAL_VARIABLE_COUNT] = {
    {"primary_color", SHADER_VARIABLE_TYPE_VEC3, offsetof(button_round_metal_vars_t, primary_color), "0.88, 0.18, 0.24"},
    {"depression", SHADER_VARIABLE_TYPE_FLOAT, offsetof(button_round_metal_vars_t, depression), "0.0"},
};

static const button_round_metal_vars_t g_button_round_metal_fallback = {
    {0.88f, 0.18f, 0.24f},
    0.0f,
};

static float circle_sdf(vec2_t point, float radius)
{
    return v2_length(point) - radius;
}

vec4_t button_round_metal_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const button_round_metal_vars_t *vars = U_VARS_AS(button_round_metal_vars_t, uniforms);
    vec4_t result = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec2_t resolution;
    float min_axis;
    vec2_t uv;
    float aa;
    float depression;
    vec3_t primary_color;
    float outer_radius = 0.90f;
    float rim_width = 0.11f;
    float inner_radius = outer_radius - rim_width;
    float distance_to_center;
    float outer_mask;
    float inner_mask;
    float rim_mask;
    float gel_mask;
    float gel_dist;
    float dip;
    float shadow_alpha;
    float rim_point;
    vec3_t light_dir = v3_safe_normalize(vec3(0.5f, 0.5f, 1.0f));
    vec3_t view_dir = vec3(0.0f, 0.0f, 1.0f);
    vec3_t rim_normal;
    vec3_t rim_reflect;
    float rim_diff;
    float rim_spec;
    float rim_edge_highlight;
    vec3_t metal;
    vec3_t gel_normal;
    vec3_t gel_reflect;
    float fresnel;
    float gel_diff;
    float gel_spec;
    float center_pool;
    float edge_pool;
    vec3_t gel;

    if (vars == NULL) {
        vars = &g_button_round_metal_fallback;
    }

    resolution = (uniforms != NULL) ? uniforms->resolution : vec2(320.0f, 320.0f);
    min_axis = max(1.0f, min(resolution.x, resolution.y));
    uv = vec2(
        ((fragCoord.x + 0.5f) * 2.0f - resolution.x) / min_axis,
        ((fragCoord.y + 0.5f) * 2.0f - resolution.y) / min_axis);
    aa = (2.0f / min_axis) * 1.5f;

    depression = clampf(vars->depression, 0.0f, 1.0f);
    primary_color = v3_saturate(vars->primary_color);
    distance_to_center = v2_length(uv);

    outer_mask = shader_sdf_fill(circle_sdf(uv, outer_radius), aa);
    inner_mask = shader_sdf_fill(circle_sdf(uv, inner_radius), aa);
    rim_mask = saturate(outer_mask - inner_mask);
    gel_mask = inner_mask;

    shadow_alpha = shader_sdf_fill(circle_sdf(v2_sub(uv, vec2(0.02f, 0.06f - 0.02f * depression)), outer_radius * 1.04f), aa * 4.0f);
    result = shader_layer_over(result, vec4(0.020f, 0.026f, 0.036f, shadow_alpha * (0.14f - 0.02f * depression)));

    rim_point = saturate((distance_to_center - inner_radius) / max(rim_width, 0.0001f));
    rim_normal = v3_safe_normalize(vec3(uv.x * (rim_point - 0.5f) * 1.7f, uv.y * (rim_point - 0.5f) * 1.7f, 0.55f));
    rim_reflect = v3_reflect(v3_mul1(light_dir, -1.0f), rim_normal);
    rim_diff = max(v3_dot(rim_normal, light_dir), 0.0f);
    rim_spec = powf(max(v3_dot(rim_reflect, view_dir), 0.0f), 16.0f);
    rim_edge_highlight = powf(saturate(1.0f - fabsf(rim_point * 2.0f - 1.0f)), 1.8f);
    metal = vec3(0.16f, 0.17f, 0.19f);
    metal = v3_add(metal, v3_mul1(vec3(0.64f, 0.66f, 0.70f), rim_diff));
    metal = v3_add(metal, v3_mul1(vec3(0.92f, 0.94f, 0.98f), rim_spec * 0.75f));
    metal = v3_add(metal, v3_mul1(vec3(0.78f, 0.82f, 0.88f), rim_edge_highlight * 0.18f));
    result = shader_layer_over(result, vec4(saturate(metal.x), saturate(metal.y), saturate(metal.z), rim_mask));

    gel_dist = clampf(distance_to_center / max(inner_radius, 0.0001f), 0.0f, 1.0f);
    dip = gel_dist * gel_dist * depression;
    gel_normal = v3_safe_normalize(vec3(uv.x * depression, uv.y * depression, 1.0f - dip));
    gel_reflect = v3_reflect(v3_mul1(light_dir, -1.0f), gel_normal);
    fresnel = powf(1.0f - max(v3_dot(gel_normal, view_dir), 0.0f), 3.0f);
    gel_diff = max(v3_dot(gel_normal, light_dir), 0.4f);
    gel_spec = powf(max(v3_dot(gel_reflect, view_dir), 0.0f), 32.0f);
    center_pool = powf(1.0f - gel_dist, 2.2f) * (0.18f + 0.30f * depression);
    edge_pool = smoothstepf(0.52f, 1.0f, gel_dist) * depression;
    gel = v3_lerp(v3_mul1(primary_color, gel_diff), vec3(1.0f, 1.0f, 1.0f), fresnel * 0.5f);
    gel = v3_add(gel, v3_mul1(primary_color, center_pool));
    gel = v3_sub(gel, v3_mul1(vec3(0.08f, 0.08f, 0.10f), edge_pool * 0.40f));
    gel = v3_add(gel, v3_mul1(vec3(1.0f, 1.0f, 1.0f), gel_spec * 0.8f));
    result = shader_layer_over(result, vec4(saturate(gel.x), saturate(gel.y), saturate(gel.z), gel_mask));

    return vec4(saturate(result.x), saturate(result.y), saturate(result.z), saturate(result.w));
}
