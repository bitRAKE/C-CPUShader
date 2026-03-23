#include "toggle_switch.h"

#include "u_vars.h"

#include <stddef.h>

const shader_variable_desc_t g_toggle_switch_variables[TOGGLE_SWITCH_VARIABLE_COUNT] = {
    {"state",        SHADER_VARIABLE_TYPE_FLOAT, offsetof(toggle_switch_vars_t, state),        "0.0"},
    {"active_color", SHADER_VARIABLE_TYPE_VEC3,  offsetof(toggle_switch_vars_t, active_color), "0.30, 0.78, 0.47"},
    {"knob_color",   SHADER_VARIABLE_TYPE_VEC3,  offsetof(toggle_switch_vars_t, knob_color),   "1.0, 1.0, 1.0"},
};

static const toggle_switch_vars_t g_toggle_switch_fallback = {
    0.0f,
    {0.30f, 0.78f, 0.47f},
    {1.0f, 1.0f, 1.0f},
};

/* Capsule (fully rounded pill shape). */
static float capsule_sdf(vec2_t p, vec2_t half_ext)
{
    float radius = half_ext.y;
    vec2_t q;

    q.x = fabsf(p.x) - (half_ext.x - radius);
    q.y = fabsf(p.y) - 0.0f;
    if (q.x < 0.0f) q.x = 0.0f;
    return v2_length(q) - radius;
}

static float circle_sdf(vec2_t p, float radius)
{
    return v2_length(p) - radius;
}

vec4_t toggle_switch_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const toggle_switch_vars_t *vars = U_VARS_AS(toggle_switch_vars_t, uniforms);
    vec4_t result = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec2_t resolution;
    float min_axis, aa;
    vec2_t uv;
    float t;
    vec3_t active_color, knob_color;

    /* Track geometry. */
    vec2_t track_half = vec2(1.52f, 0.54f);
    float track_sdf, track_mask;
    vec3_t track_off_color, track_color;
    float inner_shadow, bottom_highlight;

    /* Knob geometry and lighting. */
    float knob_radius   = 0.44f;
    float knob_x_travel;
    vec2_t knob_center;
    float knob_sdf, knob_mask;
    float shadow_sdf, shadow_mask;
    vec2_t rel;
    float r_norm, nz;
    float diffuse, spec, rim, fresnel_edge;
    vec3_t knob_surface;

    if (vars == NULL) {
        vars = &g_toggle_switch_fallback;
    }

    resolution = (uniforms != NULL) ? uniforms->resolution : vec2(160.0f, 80.0f);
    min_axis   = max(1.0f, min(resolution.x, resolution.y));
    uv = vec2(
        ((fragCoord.x + 0.5f) * 2.0f - resolution.x) / min_axis,
        ((fragCoord.y + 0.5f) * 2.0f - resolution.y) / min_axis);
    aa = (2.0f / min_axis) * 1.5f;

    /* Smoothstep the state for organic motion. */
    {
        float s = clampf(vars->state, 0.0f, 1.0f);
        t = s * s * (3.0f - 2.0f * s);
    }
    active_color = v3_saturate(vars->active_color);
    knob_color   = v3_saturate(vars->knob_color);

    /* --- Track --- */
    track_sdf  = capsule_sdf(uv, track_half);
    track_mask = shader_sdf_fill(track_sdf, aa);

    track_off_color = vec3(0.48f, 0.50f, 0.53f);
    track_color     = v3_lerp(track_off_color, active_color, t);

    result = shader_layer_over(result,
        vec4(track_color.x, track_color.y, track_color.z, track_mask));

    /* Inner shadow along top edge gives concavity. */
    inner_shadow = track_mask * smoothstepf(0.15f, -0.35f, uv.y) * 0.15f;
    result = shader_layer_over(result, vec4(0.0f, 0.0f, 0.0f, inner_shadow));

    /* Subtle highlight along bottom edge. */
    bottom_highlight = track_mask * shader_sdf_band(track_sdf, -0.06f, 0.02f);
    bottom_highlight *= smoothstepf(-0.20f, 0.40f, uv.y) * 0.07f;
    result = shader_layer_over(result, vec4(1.0f, 1.0f, 1.0f, bottom_highlight));

    /* --- Knob shadow --- */
    knob_x_travel = track_half.x - track_half.y;  /* ±0.98 */
    knob_center   = vec2(lerpf(-knob_x_travel, knob_x_travel, t), 0.0f);

    shadow_sdf  = circle_sdf(
        v2_sub(uv, v2_add(knob_center, vec2(0.02f, -0.05f))),
        knob_radius * 1.08f);
    shadow_mask = shader_sdf_fill(shadow_sdf, aa * 3.5f) * 0.30f;
    result = shader_layer_over(result, vec4(0.02f, 0.02f, 0.04f, shadow_mask));

    /* --- Knob body --- */
    knob_sdf  = circle_sdf(v2_sub(uv, knob_center), knob_radius);
    knob_mask = shader_sdf_fill(knob_sdf, aa);

    /*
     * Hemisphere lighting.  Compute surface normal from the knob's circular
     * footprint, then light with a directional source from upper-left.
     */
    rel    = v2_sub(uv, knob_center);
    r_norm = clampf(v2_length(rel) / knob_radius, 0.0f, 1.0f);
    nz     = 1.0f - r_norm * r_norm;       /* hemisphere z² */
    if (nz < 0.0f) nz = 0.0f;

    /* Diffuse: dot(normal, light_dir) where light = normalize(0.4, 0.5, 0.8) */
    diffuse = saturate(
        rel.x / knob_radius * 0.38f +
        rel.y / knob_radius * 0.48f +
        nz * 0.78f);

    /* Specular: sharp highlight near upper-left quadrant of dome. */
    {
        float sx = (rel.x / knob_radius - 0.28f);
        float sy = (rel.y / knob_radius - 0.32f);
        float sd = sx * sx + sy * sy;
        spec = powf(saturate(1.0f - sd * 4.0f), 18.0f) * 0.65f;
    }

    /* Fresnel rim brightening at the dome edge. */
    fresnel_edge = powf(r_norm, 4.0f) * 0.20f;

    /* Rim darkening at the very edge for contact shadow. */
    rim = smoothstepf(0.78f, 1.0f, r_norm) * 0.12f;

    knob_surface = v3_mul1(knob_color, 0.72f + 0.28f * diffuse);
    knob_surface = v3_add(knob_surface, vec3(spec, spec, spec));
    knob_surface = v3_add(knob_surface, vec3(fresnel_edge, fresnel_edge, fresnel_edge));
    knob_surface = v3_sub(knob_surface, vec3(rim, rim, rim));

    result = shader_layer_over(result,
        vec4(saturate(knob_surface.x),
             saturate(knob_surface.y),
             saturate(knob_surface.z),
             knob_mask));

    return vec4(saturate(result.x), saturate(result.y),
                saturate(result.z), saturate(result.w));
}
