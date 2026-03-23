#include "colorspace_hdr_linear.h"

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

static float glow_falloff(vec2_t uv, vec2_t center, float radius, float strength)
{
    float distance = v2_distance(uv, center);
    float scaled = distance / fmaxf(radius, 0.0001f);
    return strength / (1.0f + scaled * scaled * 10.0f);
}

vec4_t colorspace_hdr_linear_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    vec2_t uv = vec2(fragCoord.x / resolution.x, fragCoord.y / resolution.y);
    vec2_t centered = vec2(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f);
    vec3_t color = vec3(
        0.010f + 0.030f * (1.0f - uv.y),
        0.011f + 0.024f * (1.0f - uv.y),
        0.016f + 0.060f * (1.0f - uv.y));
    float vignette = saturate(1.12f - 0.42f * v2_dot(centered, centered));
    static const float gray_levels[] = {0.18f, 0.50f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
    static const vec3_t tint_levels[] = {
        {0.60f, 0.12f, 0.12f},
        {0.14f, 0.52f, 0.16f},
        {0.14f, 0.28f, 0.74f},
        {0.62f, 0.16f, 0.58f}
    };

    color = v3_mul1(color, vignette);

    color = v3_lerp(color, vec3(0.030f, 0.032f, 0.040f), rect_mask(uv, vec2(0.050f, 0.10f), vec2(0.950f, 0.90f), 0.008f));
    color = v3_lerp(color, vec3(0.018f, 0.019f, 0.024f), rect_mask(uv, vec2(0.080f, 0.18f), vec2(0.620f, 0.82f), 0.008f));
    color = v3_lerp(color, vec3(0.018f, 0.020f, 0.026f), rect_mask(uv, vec2(0.670f, 0.18f), vec2(0.920f, 0.82f), 0.008f));

    for (int index = 0; index < 7; index++) {
        float x0 = 0.100f + 0.072f * (float)index;
        float x1 = x0 + 0.056f;
        float patch_mask = rect_mask(uv, vec2(x0, 0.47f), vec2(x1, 0.74f), 0.005f);
        float base_mask = rect_mask(uv, vec2(x0, 0.25f), vec2(x1, 0.39f), 0.005f);
        vec3_t patch_color = vec3_o(gray_levels[index]);
        float sheen = 0.92f + 0.08f * smoothstepf(0.0f, 1.0f, (uv.y - 0.47f) / 0.27f);

        color = v3_lerp(color, v3_mul1(patch_color, sheen), patch_mask);
        color = v3_lerp(color, vec3_o(gray_levels[index] * 0.12f), base_mask);
    }

    for (int index = 0; index < 4; index++) {
        float x0 = 0.700f;
        float x1 = 0.890f;
        float y0 = 0.220f + 0.130f * (float)index;
        float y1 = y0 + 0.090f;
        float patch_mask = rect_mask(uv, vec2(x0, y0), vec2(x1, y1), 0.005f);
        vec3_t patch_color = v3_mul1(tint_levels[index], 6.0f + 2.5f * (float)index);
        float stripe = 0.88f + 0.12f * sinf((uv.x - x0) * 48.0f);

        color = v3_lerp(color, v3_mul1(patch_color, stripe), patch_mask);
    }

    {
        vec3_t orb_a = vec3(7.5f, 1.8f, 0.7f);
        vec3_t orb_b = vec3(0.9f, 5.8f, 10.5f);
        vec3_t orb_c = vec3(12.0f, 2.8f, 11.0f);
        float disc_a = circle_mask(uv, vec2(0.215f, 0.155f), 0.060f, 0.006f);
        float disc_b = circle_mask(uv, vec2(0.365f, 0.155f), 0.060f, 0.006f);
        float disc_c = circle_mask(uv, vec2(0.515f, 0.155f), 0.060f, 0.006f);

        color = v3_add(color, v3_mul1(orb_a, glow_falloff(uv, vec2(0.215f, 0.155f), 0.070f, 0.45f)));
        color = v3_add(color, v3_mul1(orb_b, glow_falloff(uv, vec2(0.365f, 0.155f), 0.070f, 0.45f)));
        color = v3_add(color, v3_mul1(orb_c, glow_falloff(uv, vec2(0.515f, 0.155f), 0.070f, 0.45f)));

        color = v3_lerp(color, orb_a, disc_a);
        color = v3_lerp(color, orb_b, disc_b);
        color = v3_lerp(color, orb_c, disc_c);
    }

    {
        float ramp_mask = rect_mask(uv, vec2(0.100f, 0.835f), vec2(0.900f, 0.875f), 0.004f);
        float t = smoothstepf(0.100f, 0.900f, uv.x);
        float value = 0.02f + 12.0f * powf(t, 3.0f);
        vec3_t ramp = vec3(value * 0.32f, value * 0.65f, value);
        color = v3_lerp(color, ramp, ramp_mask);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
