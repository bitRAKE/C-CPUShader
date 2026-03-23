/*
 * diag_hdr_clipping -- Luminance step ladder for HDR proof.
 *
 * Scene-linear output.  In scRGB, 1.0 = 80 nits (SDR reference white).
 * Values above 1.0 should produce visibly brighter pixels on an HDR display.
 * On an SDR display, everything above 1.0 clips to the same white -- that
 * clipping is exactly what this diagnostic exposes.
 *
 * Layout (top to bottom):
 *
 *   Row 0 (top):     Smooth gradient 0.0 -> 10.0 (full HDR ramp)
 *   Row 1:           12 luminance patches (gray)
 *   Row 2:           12 luminance patches (warm tint -- surface color test)
 *   Row 3:           SDR reference: 12 patches clamped to [0, 1]
 *   Row 4 (bottom):  Smooth gradient 0.0 -> 10.0 (repeat for edge comparison)
 *
 * The SDR reference row lets you compare directly: if rows 1-2 look
 * identical to row 3, HDR is not active.
 */

#include "diag_hdr_clipping.h"

/*
 * 12 luminance levels spanning the useful HDR range.
 * scRGB 1.0 = 80 nits on a reference HDR display.
 */
static const float g_levels[] = {
    0.05f,      /*   ~4 nits  - near black              */
    0.18f,      /*  ~14 nits  - 18% gray (photographic) */
    0.50f,      /*  ~40 nits  - dim midtone              */
    1.00f,      /*  ~80 nits  - SDR reference white      */
    1.50f,      /* ~120 nits                              */
    2.00f,      /* ~160 nits                              */
    2.54f,      /* ~203 nits  - SDR content white (HDR)   */
    4.00f,      /* ~320 nits                              */
    6.00f,      /* ~480 nits                              */
    8.00f,      /* ~640 nits                              */
   10.00f,      /* ~800 nits                              */
   12.50f,      /*~1000 nits  - typical HDR peak          */
};

#define LEVEL_COUNT ((int)(sizeof(g_levels) / sizeof(g_levels[0])))

static float rect_mask(vec2_t uv, float x0, float y0, float x1, float y1,
                       float feather)
{
    float l = smoothstepf(x0 - feather, x0 + feather, uv.x);
    float r = 1.0f - smoothstepf(x1 - feather, x1 + feather, uv.x);
    float b = smoothstepf(y0 - feather, y0 + feather, uv.y);
    float t = 1.0f - smoothstepf(y1 - feather, y1 + feather, uv.y);
    return l * r * b * t;
}

vec4_t diag_hdr_clipping_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    vec2_t resolution = uniforms->resolution;
    vec2_t uv = vec2(
        (fragCoord.x + 0.5f) / resolution.x,
        (fragCoord.y + 0.5f) / resolution.y);

    float feather = 1.2f / resolution.x;

    /* Dark background: near-black so HDR patches pop. */
    vec3_t color = vec3(0.012f, 0.013f, 0.016f);

    float margin  = 0.04f;
    float col_gap = 0.008f;
    float col_w   = (1.0f - 2.0f * margin - (float)(LEVEL_COUNT - 1) * col_gap)
                  / (float)LEVEL_COUNT;

    /* --- Row 0 & Row 4: smooth HDR gradient --- */
    {
        float ramp_t = saturate((uv.x - margin) / (1.0f - 2.0f * margin));
        /* Cubic ramp: more resolution in the lower range. */
        float value = 0.005f + 12.5f * ramp_t * ramp_t * ramp_t;

        float top_mask = rect_mask(uv, margin, 0.86f, 1.0f - margin, 0.95f,
                                   feather);
        float bot_mask = rect_mask(uv, margin, 0.04f, 1.0f - margin, 0.13f,
                                   feather);

        color = v3_lerp(color, vec3(value, value, value), top_mask);
        color = v3_lerp(color, vec3(value, value, value), bot_mask);
    }

    /* --- Row 1: neutral gray luminance patches --- */
    for (int i = 0; i < LEVEL_COUNT; i++) {
        float x0 = margin + (float)i * (col_w + col_gap);
        float x1 = x0 + col_w;
        float mask = rect_mask(uv, x0, 0.56f, x1, 0.82f, feather);
        float v = g_levels[i];

        /* Subtle sheen: slightly brighter at the top edge. */
        float sheen = 0.94f + 0.06f * smoothstepf(0.56f, 0.82f, uv.y);

        color = v3_lerp(color, vec3(v * sheen, v * sheen, v * sheen), mask);
    }

    /* --- Row 2: warm-tinted patches (same levels, colored) --- */
    for (int i = 0; i < LEVEL_COUNT; i++) {
        float x0 = margin + (float)i * (col_w + col_gap);
        float x1 = x0 + col_w;
        float mask = rect_mask(uv, x0, 0.33f, x1, 0.52f, feather);
        float v = g_levels[i];

        /* Warm tint: slightly emphasize red, reduce blue. */
        vec3_t tinted = vec3(v * 1.05f, v * 0.88f, v * 0.62f);

        color = v3_lerp(color, tinted, mask);
    }

    /* --- Row 3: SDR reference (clamped to 1.0) --- */
    for (int i = 0; i < LEVEL_COUNT; i++) {
        float x0 = margin + (float)i * (col_w + col_gap);
        float x1 = x0 + col_w;
        float mask = rect_mask(uv, x0, 0.16f, x1, 0.29f, feather);
        float v = g_levels[i];

        if (v > 1.0f) v = 1.0f;  /* explicit SDR clamp */

        color = v3_lerp(color, vec3(v, v, v), mask);
    }

    /* --- Divider lines between rows --- */
    {
        float line_w = 1.0f / resolution.y;
        float d1 = smoothstepf(0.545f - line_w, 0.545f + line_w, uv.y)
                  * (1.0f - smoothstepf(0.555f - line_w, 0.555f + line_w, uv.y));
        float d2 = smoothstepf(0.315f - line_w, 0.315f + line_w, uv.y)
                  * (1.0f - smoothstepf(0.325f - line_w, 0.325f + line_w, uv.y));
        float d3 = smoothstepf(0.145f - line_w, 0.145f + line_w, uv.y)
                  * (1.0f - smoothstepf(0.155f - line_w, 0.155f + line_w, uv.y));
        float lines = saturate(d1 + d2 + d3) * 0.15f;
        color = v3_add(color, vec3(lines, lines, lines));
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
