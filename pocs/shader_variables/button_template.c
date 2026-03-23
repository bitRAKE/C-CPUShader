#include "button_template.h"

#include "u_vars.h"

#include <stddef.h>

const shader_variable_desc_t g_button_template_variables[BUTTON_TEMPLATE_VARIABLE_COUNT] = {
    {"press_depth", SHADER_VARIABLE_TYPE_FLOAT, offsetof(button_template_vars_t, press_depth), "0.0"},
    {"diffuse_color", SHADER_VARIABLE_TYPE_VEC3, offsetof(button_template_vars_t, diffuse_color), "0.76, 0.80, 0.88"},
};

static const button_template_vars_t g_button_template_fallback = {
    0.0f,
    {0.76f, 0.80f, 0.88f},
};

static float round_rect_sdf(vec2_t point, vec2_t half_size, float radius)
{
    vec2_t q = v2_sub(v2_abs(point), vec2(half_size.x - radius, half_size.y - radius));
    vec2_t outside = vec2(max(q.x, 0.0f), max(q.y, 0.0f));
    float inside = min(max(q.x, q.y), 0.0f);
    return v2_length(outside) + inside - radius;
}

static float face_bevel_light(vec2_t point, vec2_t half_size, float radius, vec2_t texel_size, vec2_t light_dir)
{
    vec2_t gradient;
    float gradient_length;

    gradient.x =
        round_rect_sdf(v2_add(point, vec2(texel_size.x, 0.0f)), half_size, radius) -
        round_rect_sdf(v2_sub(point, vec2(texel_size.x, 0.0f)), half_size, radius);
    gradient.y =
        round_rect_sdf(v2_add(point, vec2(0.0f, texel_size.y)), half_size, radius) -
        round_rect_sdf(v2_sub(point, vec2(0.0f, texel_size.y)), half_size, radius);

    gradient_length = v2_length(gradient);
    if (gradient_length <= 0.000001f) {
        return 0.0f;
    }

    gradient = v2_div1(gradient, gradient_length);
    return v2_dot(gradient, light_dir);
}

vec4_t button_template_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const button_template_vars_t *vars = U_VARS_AS(button_template_vars_t, uniforms);
    vec4_t result = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec2_t resolution;
    vec2_t uv;
    vec2_t local_uv;
    vec2_t texel_size;
    float press_depth;
    vec3_t diffuse_color;
    vec2_t holder_center = vec2(0.0f, 0.0f);
    vec2_t holder_outer_half = vec2(1.12f, 0.24f);
    float holder_outer_radius = 0.12f;
    vec2_t cavity_half = vec2(0.98f, 0.15f);
    float cavity_radius = 0.09f;
    vec2_t press_shift;
    vec2_t face_center;
    vec2_t face_half = vec2(0.92f, 0.105f);
    float face_radius = 0.085f;
    float aa;
    float holder_outer_sdf;
    float cavity_sdf;
    float face_sdf;
    float holder_outer_mask;
    float cavity_mask;
    float face_mask;
    float holder_shell_mask;
    float holder_glow;
    float holder_shadow;
    float cavity_inner_shadow;
    float cavity_rim_highlight;
    float top_gap_shadow;
    float face_shadow;
    float face_glow;
    float face_bevel;
    float face_bevel_mask;
    float face_gloss;
    float face_gloss_mask;
    vec3_t holder_color;
    vec3_t cavity_color;
    vec3_t shadow_color = vec3(0.020f, 0.026f, 0.038f);
    vec3_t holder_highlight = vec3(0.72f, 0.76f, 0.84f);
    vec3_t holder_shadow_color = vec3(0.055f, 0.062f, 0.080f);
    vec3_t face_color;
    float button_top;
    vec2_t light_dir = v2_normalize(vec2(-0.82f, 0.92f));

    if (vars == NULL) {
        vars = &g_button_template_fallback;
    }

    resolution = (uniforms != NULL) ? uniforms->resolution : vec2(384.0f, 192.0f);
    uv = vec2((fragCoord.x + 0.5f) / max(1.0f, resolution.x), (fragCoord.y + 0.5f) / max(1.0f, resolution.y));
    local_uv = vec2(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f);
    local_uv.x *= resolution.x / max(1.0f, resolution.y);
    texel_size = vec2(2.0f / max(1.0f, resolution.x), 2.0f / max(1.0f, resolution.y));
    aa = max(texel_size.x, texel_size.y) * 1.8f;

    press_depth = clampf(vars->press_depth, 0.0f, 1.0f);
    diffuse_color = v3_saturate(vars->diffuse_color);
    press_shift = vec2(0.0f, -0.072f * press_depth);
    face_center = v2_add(holder_center, press_shift);
    face_color = v3_lerp(v3_mul1(diffuse_color, 0.90f), v3_mul1(diffuse_color, 1.05f), 0.55f);
    holder_color = v3_lerp(vec3(0.18f, 0.20f, 0.24f), v3_mul1(diffuse_color, 0.34f), 0.45f);
    cavity_color = v3_lerp(vec3(0.06f, 0.07f, 0.09f), v3_mul1(diffuse_color, 0.18f), 0.35f);

    holder_outer_sdf = round_rect_sdf(v2_sub(local_uv, holder_center), holder_outer_half, holder_outer_radius);
    cavity_sdf = round_rect_sdf(v2_sub(local_uv, holder_center), cavity_half, cavity_radius);
    face_sdf = round_rect_sdf(v2_sub(local_uv, face_center), face_half, face_radius);
    holder_outer_mask = shader_sdf_fill(holder_outer_sdf, aa);
    cavity_mask = shader_sdf_fill(cavity_sdf, aa);
    face_mask = shader_sdf_fill(face_sdf, aa);
    holder_shell_mask = saturate(holder_outer_mask - cavity_mask);

    holder_shadow = shader_sdf_fill(round_rect_sdf(v2_sub(local_uv, v2_add(holder_center, vec2(0.0f, -0.070f))), vec2(1.18f, 0.28f), 0.16f), aa * 3.2f) * 0.22f;
    holder_glow = shader_sdf_fill(round_rect_sdf(v2_sub(local_uv, holder_center), vec2(1.18f, 0.28f), 0.16f), aa * 3.6f) * 0.06f;
    result = shader_layer_over(result, vec4(shadow_color.x, shadow_color.y, shadow_color.z, holder_shadow));
    result = shader_layer_over(result, vec4(diffuse_color.x, diffuse_color.y, diffuse_color.z, holder_glow * 0.35f));

    result = shader_layer_over(result, vec4(holder_color.x, holder_color.y, holder_color.z, holder_shell_mask * 0.96f));
    result = shader_layer_over(result, vec4(cavity_color.x, cavity_color.y, cavity_color.z, cavity_mask * 0.88f));

    cavity_inner_shadow = cavity_mask * (1.0f - face_mask) * shader_sdf_band(cavity_sdf, -0.050f, 0.030f);
    result = shader_layer_over(result, vec4(holder_shadow_color.x, holder_shadow_color.y, holder_shadow_color.z, cavity_inner_shadow * 0.20f));
    cavity_rim_highlight = holder_shell_mask * shader_sdf_band(holder_outer_sdf, -0.040f, 0.028f);
    result = shader_layer_over(result, vec4(holder_highlight.x, holder_highlight.y, holder_highlight.z, cavity_rim_highlight * 0.10f));

    button_top = face_center.y + face_half.y - 0.010f;
    top_gap_shadow = cavity_mask * (1.0f - face_mask);
    top_gap_shadow *= smoothstepf(button_top - aa, button_top + 0.075f + 0.045f * press_depth, local_uv.y);
    top_gap_shadow *= 1.0f - smoothstepf(cavity_half.y - 0.035f, cavity_half.y + aa, local_uv.y);
    result = shader_layer_over(result, vec4(shadow_color.x, shadow_color.y, shadow_color.z, top_gap_shadow * (0.18f + 0.42f * press_depth)));

    face_shadow = shader_sdf_fill(round_rect_sdf(v2_sub(local_uv, v2_add(face_center, vec2(0.0f, -0.022f))), vec2(0.98f, 0.13f), 0.10f), aa * 2.6f);
    face_shadow *= cavity_mask * (1.0f - face_mask) * (1.0f - 0.35f * press_depth) * 0.40f;
    result = shader_layer_over(result, vec4(shadow_color.x, shadow_color.y, shadow_color.z, face_shadow));

    result = shader_layer_over(result, vec4(face_color.x, face_color.y, face_color.z, face_mask));

    face_bevel = face_bevel_light(v2_sub(local_uv, face_center), face_half, face_radius, texel_size, light_dir);
    face_bevel_mask = face_mask * shader_sdf_band(face_sdf, -0.065f, -0.010f);
    result = shader_layer_over(
        result,
        vec4(0.985f, 0.990f, 1.000f, face_bevel_mask * saturate(0.18f * face_bevel + 0.18f * (1.0f - press_depth))));
    result = shader_layer_over(
        result,
        vec4(0.055f, 0.060f, 0.078f, face_bevel_mask * saturate(-0.22f * face_bevel + 0.10f + 0.10f * press_depth)));

    face_gloss_mask = face_mask * (1.0f - shader_sdf_band(face_sdf, -0.080f, -0.025f));
    face_gloss = smoothstepf(0.18f, 0.86f, uv.y + uv.x * 0.08f);
    result = shader_layer_over(result, vec4(1.0f, 1.0f, 1.0f, face_gloss_mask * face_gloss * (0.05f + 0.08f * (1.0f - press_depth))));

    face_glow = shader_sdf_fill(round_rect_sdf(v2_sub(local_uv, face_center), vec2(0.99f, 0.13f), 0.11f), aa * 3.0f) * (1.0f - face_mask);
    result = shader_layer_over(result, vec4(diffuse_color.x, diffuse_color.y, diffuse_color.z, face_glow * 0.05f * (1.0f - 0.30f * press_depth)));

    return vec4(saturate(result.x), saturate(result.y), saturate(result.z), saturate(result.w));
}
