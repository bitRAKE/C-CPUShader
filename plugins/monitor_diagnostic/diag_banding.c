#include "diag_banding.h"

/*
 * Banding Steps — quantized gradient matrix.
 *
 * Layout: 6 rows × 4 columns (channel groups).
 *   Rows (bottom to top): 8, 16, 32, 64, 128, 256 quantization levels
 *   Columns (left to right): Gray, Red, Green, Blue
 *
 * Improvement over web version:
 *   - Per-channel banding reveals unequal panel bit depth (common on VA/TN)
 *   - 128-step row catches 7-bit panels
 *   - Pure quantization, no animation overlay that masks banding
 *   - Exact step boundaries at pixel level
 */

static float quantize(float v, float steps)
{
    return floorf(v * steps + 0.5f) / steps;
}

vec4_t diag_banding_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    float u = fragCoord.x / w;
    float v = fragCoord.y / h;

    /* 6 rows */
    static const float steps[] = { 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f };
    int row = (int)(v * 6.0f);
    if (row > 5) row = 5;

    /* 4 columns: gray, red, green, blue */
    int col = (int)(u * 4.0f);
    if (col > 3) col = 3;

    float local_u = (u - (float)col * 0.25f) / 0.25f;
    float q = quantize(local_u, steps[row]);

    vec3_t color;
    switch (col) {
    case 0: color = vec3(q, q, q);       break; /* Gray */
    case 1: color = vec3(q, 0.0f, 0.0f); break; /* Red */
    case 2: color = vec3(0.0f, q, 0.0f); break; /* Green */
    case 3: color = vec3(0.0f, 0.0f, q); break; /* Blue */
    default: color = vec3_o(0.0f);        break;
    }

    /* Row separator: 1px dark line */
    float row_frac = v * 6.0f - (float)row;
    float row_px = h / 6.0f;
    if (row_frac * row_px < 1.0f && row > 0) {
        color = vec3(0.08f, 0.08f, 0.08f);
    }

    /* Column separator: 1px dark line */
    float col_frac = u * 4.0f - (float)col;
    float col_px = w / 4.0f;
    if (col_frac * col_px < 1.0f && col > 0) {
        color = vec3(0.08f, 0.08f, 0.08f);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
