#include "colorspace_hdr10_pq.h"

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

static float pq_encode_from_nits(float nits)
{
    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 32.0f;
    const float c1 = 3424.0f / 4096.0f;
    const float c2 = 2413.0f / 128.0f;
    const float c3 = 2392.0f / 128.0f;
    float normalized = saturate(nits / 10000.0f);
    float p = powf(normalized, m1);
    return powf((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

vec4_t colorspace_hdr10_pq_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    vec2_t uv = vec2(fragCoord.x / resolution.x, fragCoord.y / resolution.y);
    vec2_t centered = vec2(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f);
    float vignette = saturate(1.10f - 0.40f * v2_dot(centered, centered));
    vec3_t color_nits = vec3(
        0.03f + 0.20f * (1.0f - uv.y),
        0.03f + 0.18f * (1.0f - uv.y),
        0.06f + 0.55f * (1.0f - uv.y));
    static const float luminance_steps[] = {0.1f, 1.0f, 10.0f, 100.0f, 203.0f, 400.0f, 1000.0f};
    static const vec3_t color_blocks[] = {
        {220.0f, 40.0f, 30.0f},
        {35.0f, 280.0f, 55.0f},
        {30.0f, 120.0f, 520.0f},
        {520.0f, 90.0f, 430.0f}
    };

    color_nits = v3_mul1(color_nits, vignette);

    {
        float frame = rect_mask(uv, vec2(0.045f, 0.10f), vec2(0.955f, 0.90f), 0.008f);
        float left_panel = rect_mask(uv, vec2(0.080f, 0.18f), vec2(0.620f, 0.82f), 0.008f);
        float right_panel = rect_mask(uv, vec2(0.670f, 0.18f), vec2(0.920f, 0.82f), 0.008f);

        color_nits = v3_lerp(color_nits, vec3_o(0.35f), frame);
        color_nits = v3_lerp(color_nits, vec3_o(0.20f), left_panel);
        color_nits = v3_lerp(color_nits, vec3_o(0.20f), right_panel);
    }

    for (int index = 0; index < 7; index++) {
        float x0 = 0.100f + 0.072f * (float)index;
        float x1 = x0 + 0.056f;
        float patch_mask = rect_mask(uv, vec2(x0, 0.47f), vec2(x1, 0.74f), 0.005f);
        float base_mask = rect_mask(uv, vec2(x0, 0.25f), vec2(x1, 0.39f), 0.005f);
        float sheen = 0.92f + 0.08f * smoothstepf(0.0f, 1.0f, (uv.y - 0.47f) / 0.27f);
        vec3_t patch = vec3_o(luminance_steps[index] * sheen);
        vec3_t base = vec3_o(luminance_steps[index] * 0.08f);

        color_nits = v3_lerp(color_nits, patch, patch_mask);
        color_nits = v3_lerp(color_nits, base, base_mask);
    }

    for (int index = 0; index < 4; index++) {
        float y0 = 0.220f + 0.130f * (float)index;
        float y1 = y0 + 0.090f;
        float patch_mask = rect_mask(uv, vec2(0.700f, y0), vec2(0.890f, y1), 0.005f);
        float stripe = 0.88f + 0.12f * sinf((uv.x - 0.700f) * 48.0f);
        vec3_t patch = v3_mul1(color_blocks[index], stripe);

        color_nits = v3_lerp(color_nits, patch, patch_mask);
    }

    {
        float ramp_mask = rect_mask(uv, vec2(0.100f, 0.835f), vec2(0.900f, 0.875f), 0.004f);
        float t = smoothstepf(0.100f, 0.900f, uv.x);
        float nits = 0.05f + 1200.0f * powf(t, 3.0f);
        vec3_t ramp = vec3(nits * 0.30f, nits * 0.62f, nits);
        color_nits = v3_lerp(color_nits, ramp, ramp_mask);
    }

    {
        vec3_t orb_a = vec3(700.0f, 120.0f, 60.0f);
        vec3_t orb_b = vec3(90.0f, 460.0f, 880.0f);
        vec3_t orb_c = vec3(1100.0f, 300.0f, 950.0f);
        float disc_a = circle_mask(uv, vec2(0.215f, 0.155f), 0.060f, 0.006f);
        float disc_b = circle_mask(uv, vec2(0.365f, 0.155f), 0.060f, 0.006f);
        float disc_c = circle_mask(uv, vec2(0.515f, 0.155f), 0.060f, 0.006f);

        color_nits = v3_add(color_nits, v3_mul1(orb_a, 0.28f / (1.0f + v2_length_sq(v2_sub(uv, vec2(0.215f, 0.155f))) * 150.0f)));
        color_nits = v3_add(color_nits, v3_mul1(orb_b, 0.28f / (1.0f + v2_length_sq(v2_sub(uv, vec2(0.365f, 0.155f))) * 150.0f)));
        color_nits = v3_add(color_nits, v3_mul1(orb_c, 0.28f / (1.0f + v2_length_sq(v2_sub(uv, vec2(0.515f, 0.155f))) * 150.0f)));

        color_nits = v3_lerp(color_nits, orb_a, disc_a);
        color_nits = v3_lerp(color_nits, orb_b, disc_b);
        color_nits = v3_lerp(color_nits, orb_c, disc_c);
    }

    return vec4(
        pq_encode_from_nits(color_nits.x),
        pq_encode_from_nits(color_nits.y),
        pq_encode_from_nits(color_nits.z),
        1.0f);
}
