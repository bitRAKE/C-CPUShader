#include "diag_gamma_ramp.h"

/*
 * Gamma Ramp — five transfer-function rows plus checkerboard reference.
 *
 *   Row 0 (bottom): Linear (gamma 1.0)
 *   Row 1:          sRGB transfer function (piecewise)
 *   Row 2:          Gamma 1.8 (classic Mac)
 *   Row 3:          Gamma 2.2 (CRT reference)
 *   Row 4 (top):    Gamma 2.6 (cinema DCI)
 *
 * Each row has a bottom 20% strip showing a 1×1 checkerboard of black
 * and the ramp value. At the correct gamma, the checkerboard zone at
 * u≈0.5 should appear to match the 50% gray of the smooth ramp above it.
 *
 * Improvement over web version:
 *   - True sRGB piecewise transfer (not just power 2.2)
 *   - Checkerboard half-tone strip for visual gamma verification
 *   - Five curves instead of three
 *   - No animation masking the static test
 */

static float srgb_eotf(float x)
{
    /* sRGB encoding: linear → sRGB */
    if (x <= 0.0031308f)
        return x * 12.92f;
    return 1.055f * powf(x, 1.0f / 2.4f) - 0.055f;
}

vec4_t diag_gamma_ramp_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    float u = fragCoord.x / w;
    float v = fragCoord.y / h;

    /* Five rows */
    int row = (int)(v * 5.0f);
    if (row > 4) row = 4;
    float row_frac = v * 5.0f - (float)row;

    /* Apply transfer function */
    float x = saturate(u);
    float val;
    switch (row) {
    case 0: val = x;                     break; /* Linear */
    case 1: val = srgb_eotf(x);          break; /* sRGB */
    case 2: val = powf(x, 1.0f / 1.8f);  break; /* Gamma 1.8 */
    case 3: val = powf(x, 1.0f / 2.2f);  break; /* Gamma 2.2 */
    case 4: val = powf(x, 1.0f / 2.6f);  break; /* Gamma 2.6 */
    default: val = x;                     break;
    }

    vec3_t color;

    /* Bottom 20% of each row: checkerboard half-tone */
    if (row_frac < 0.20f) {
        int px = (int)fragCoord.x;
        int py = (int)fragCoord.y;
        int checker = (px + py) & 1;
        /* Alternate between black and the ramp value */
        float check_val = checker ? val : 0.0f;
        color = vec3_o(check_val);
    } else {
        color = vec3_o(val);
    }

    /* Row separator */
    float row_px = h / 5.0f;
    if (row_frac * row_px < 1.0f && row > 0) {
        color = vec3(0.0f, 0.5f, 0.8f);
    }

    /* Checkerboard / smooth boundary line — 3px for visibility */
    float boundary = fabsf(row_frac - 0.20f) * row_px;
    if (boundary < 1.5f) {
        color = vec3(0.25f, 0.25f, 0.25f);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
