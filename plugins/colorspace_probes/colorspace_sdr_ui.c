#include "colorspace_sdr_ui.h"

static float rect_mask(vec2_t uv, vec2_t min_corner, vec2_t max_corner, float feather)
{
    float left = smoothstepf(min_corner.x - feather, min_corner.x + feather, uv.x);
    float right = 1.0f - smoothstepf(max_corner.x - feather, max_corner.x + feather, uv.x);
    float bottom = smoothstepf(min_corner.y - feather, min_corner.y + feather, uv.y);
    float top = 1.0f - smoothstepf(max_corner.y - feather, max_corner.y + feather, uv.y);
    return left * right * bottom * top;
}

static float circle_mask(vec2_t uv, vec2_t center, float radius, float feather)
{
    float distance = v2_distance(uv, center);
    return 1.0f - smoothstepf(radius - feather, radius + feather, distance);
}

static vec3_t add_panel(vec3_t base, vec2_t uv, vec2_t min_corner, vec2_t max_corner, vec3_t panel_color)
{
    float mask = rect_mask(uv, min_corner, max_corner, 0.008f);
    return v3_lerp(base, panel_color, mask);
}

vec4_t colorspace_sdr_ui_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    vec2_t uv = vec2(fragCoord.x / resolution.x, fragCoord.y / resolution.y);
    vec2_t centered = vec2(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f);
    float vignette = saturate(1.05f - 0.55f * v2_dot(centered, centered));
    vec3_t color = v3_mul1(v3_lerp(vec3(0.090f, 0.096f, 0.110f), vec3(0.060f, 0.067f, 0.080f), uv.y), vignette);
    vec3_t hue_color;
    float hue_band;
    float checker;
    float ramp_mask;
    float ladder_mask;
    float badge_outer;
    float badge_inner;
    static const float dark_steps[] = {0.012f, 0.024f, 0.045f, 0.085f, 0.160f, 0.320f};

    checker = ((int)(fragCoord.x / 24.0f) + (int)(fragCoord.y / 24.0f)) & 1 ? 0.014f : 0.0f;
    color = v3_add(color, vec3_o(checker));

    color = add_panel(color, uv, vec2(0.045f, 0.12f), vec2(0.955f, 0.90f), vec3(0.115f, 0.122f, 0.138f));
    color = add_panel(color, uv, vec2(0.070f, 0.18f), vec2(0.455f, 0.84f), vec3(0.080f, 0.088f, 0.102f));
    color = add_panel(color, uv, vec2(0.515f, 0.18f), vec2(0.930f, 0.84f), vec3(0.082f, 0.086f, 0.094f));

    hue_band = rect_mask(uv, vec2(0.085f, 0.74f), vec2(0.915f, 0.82f), 0.006f);
    hue_color = shader_hsv_to_rgb_v(vec3(uv.x, 0.88f, 0.98f));
    color = v3_lerp(color, hue_color, hue_band);

    ramp_mask = rect_mask(uv, vec2(0.095f, 0.60f), vec2(0.430f, 0.67f), 0.006f);
    if (ramp_mask > 0.0f) {
        float gray = smoothstepf(0.095f, 0.430f, uv.x);
        float notch_phase = ((uv.x - 0.095f) * 24.0f) - floorf((uv.x - 0.095f) * 24.0f);
        float notch = smoothstepf(0.0f, 0.003f, fabsf(notch_phase - 0.5f));
        vec3_t ramp = vec3_o(gray);
        ramp = v3_mul1(ramp, 0.92f + 0.08f * notch);
        color = v3_lerp(color, ramp, ramp_mask);
    }

    ladder_mask = rect_mask(uv, vec2(0.095f, 0.42f), vec2(0.430f, 0.54f), 0.006f);
    if (ladder_mask > 0.0f) {
        int cell = (int)clampf((uv.x - 0.095f) / (0.335f / 6.0f), 0.0f, 5.0f);
        float cell_u = (uv.x - (0.095f + (0.335f / 6.0f) * cell)) / (0.335f / 6.0f);
        vec3_t swatch = vec3_o(dark_steps[cell]);
        float edge = smoothstepf(0.02f, 0.08f, cell_u) * (1.0f - smoothstepf(0.92f, 0.98f, cell_u));
        swatch = v3_mul1(swatch, 0.86f + 0.14f * edge);
        color = v3_lerp(color, swatch, ladder_mask);
    }

    if (rect_mask(uv, vec2(0.545f, 0.28f), vec2(0.900f, 0.72f), 0.006f) > 0.0f) {
        vec2_t local = vec2(
            (uv.x - 0.7225f) / 0.155f,
            (uv.y - 0.5000f) / 0.190f);
        float radius = v2_length(local);
        float angle = atan2f(local.y, local.x);
        float hue = (angle + PI) / (2.0f * PI);
        float ring = smoothstepf(0.94f, 0.84f, radius) * smoothstepf(0.54f, 0.64f, radius);
        float fill = 1.0f - smoothstepf(0.48f, 0.56f, radius);
        vec3_t wheel = shader_hsv_to_rgb_v(vec3(hue, saturate(radius * 1.05f), 0.98f));
        vec3_t fill_color = shader_hsv_to_rgb_v(vec3(hue, 0.48f, 0.94f));
        color = v3_lerp(color, wheel, ring);
        color = v3_lerp(color, fill_color, fill * 0.78f);
    }

    {
        static const vec3_t accent_colors[] = {
            {0.980f, 0.382f, 0.246f},
            {0.286f, 0.714f, 0.996f},
            {0.914f, 0.286f, 0.964f},
            {0.980f, 0.860f, 0.250f}
        };
        static const vec2_t accent_centers[] = {
            {0.600f, 0.225f},
            {0.695f, 0.225f},
            {0.790f, 0.225f},
            {0.885f, 0.225f}
        };

        for (int index = 0; index < 4; index++) {
            float disc = circle_mask(uv, accent_centers[index], 0.032f, 0.004f);
            color = v3_lerp(color, accent_colors[index], disc);
        }
    }

    badge_outer = circle_mask(uv, vec2(0.500f, 0.500f), 0.020f, 0.004f);
    badge_inner = circle_mask(uv, vec2(0.500f, 0.500f), 0.010f, 0.003f);
    color = v3_lerp(color, vec3(0.940f, 0.955f, 0.985f), badge_outer * 0.85f);
    color = v3_lerp(color, vec3(0.160f, 0.180f, 0.220f), badge_inner);

    return vec4(color.x, color.y, color.z, 1.0f);
}
