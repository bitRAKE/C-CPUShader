#include "diag_gradient.h"

/*
 * RGB Gradient — four horizontal rows:
 *   Row 0 (bottom): Red 0..255
 *   Row 1:          Green 0..255
 *   Row 2:          Blue 0..255
 *   Row 3 (top):    Combined R+G+B sweep (white ramp + hue shift)
 *
 * Improvement over web version: per-channel isolation exposes
 * individual gamma curves and dead zones. Exact 8-bit stepping
 * with no sub-pixel filtering.
 */

static vec3_t channel_gradient(float u, int row)
{
    float v = saturate(u);

    switch (row) {
    case 0: return vec3(v, 0.0f, 0.0f);
    case 1: return vec3(0.0f, v, 0.0f);
    case 2: return vec3(0.0f, 0.0f, v);
    default: {
        /* Combined: luminance ramp with subtle hue rotation */
        float r = saturate(v + 0.04f * sinf(v * PI * 2.0f));
        float g = v;
        float b = saturate(v - 0.04f * sinf(v * PI * 2.0f));
        return vec3(r, g, b);
    }
    }
}

vec4_t diag_gradient_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    float u = fragCoord.x / w;
    float v = fragCoord.y / h;

    /* Four equal rows */
    int row = (int)(v * 4.0f);
    if (row > 3) row = 3;

    /* 1-pixel separator between rows */
    float row_frac = v * 4.0f - (float)row;
    float row_h_px = h / 4.0f;
    float sep = (row_frac * row_h_px < 1.0f) ? 1.0f : 0.0f;

    vec3_t color = channel_gradient(u, row);

    /* Thin separator line */
    if (sep > 0.0f && row > 0) {
        color = vec3(0.15f, 0.15f, 0.15f);
    }

    /* Vertical tick marks every 32/256 of the width (every 32 levels) */
    float level = u * 255.0f;
    float tick = level - floorf(level / 32.0f) * 32.0f;
    if (tick < (256.0f / w) && row_frac > 0.92f) {
        float inv = 1.0f - (color.x + color.y + color.z) / 3.0f;
        color = v3_lerp(color, vec3_o(inv > 0.5f ? 1.0f : 0.0f), 0.4f);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
