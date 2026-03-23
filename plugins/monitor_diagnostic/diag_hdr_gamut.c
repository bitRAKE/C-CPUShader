/*
 * diag_hdr_gamut -- Wide-gamut proof via scRGB negative primaries.
 *
 * scRGB uses BT.709 primaries with an extended range.  Colors outside the
 * BT.709 triangle are representable through negative channel values.
 * For example, a DCI-P3 saturated green has negative R and B in scRGB.
 *
 * Layout:
 *
 *   Row 0 (top):     BT.709 primaries and secondaries at 1.0 intensity
 *   Row 1 (middle):  DCI-P3 primaries/secondaries in scRGB coordinates
 *   Row 2 (bottom):  BT.2020 primaries/secondaries in scRGB coordinates
 *
 * Each row has 6 patches: Red, Green, Blue, Cyan, Magenta, Yellow.
 * A right-hand column shows the white point at the row's luminance.
 *
 * On a standard sRGB display:
 *   All three rows look identical -- negatives clamp to zero, collapsing
 *   extended gamut to BT.709.
 *
 * On a wide-gamut HDR display:
 *   P3 row is more vivid than BT.709.  BT.2020 row is more vivid still
 *   (assuming the panel covers part of that gamut).  The DIFFERENCE
 *   between rows is the proof.
 *
 * Conversion matrices derived from CIE chromaticity coordinates:
 *
 *   BT.709:  R(0.640, 0.330)  G(0.300, 0.600)  B(0.150, 0.060)  D65
 *   DCI-P3:  R(0.680, 0.320)  G(0.265, 0.690)  B(0.150, 0.060)  D65
 *   BT.2020: R(0.708, 0.292)  G(0.170, 0.797)  B(0.131, 0.046)  D65
 */

#include "diag_hdr_gamut.h"

/*
 * Saturated primaries and secondaries for each gamut, expressed in scRGB
 * (linear BT.709).  These are the result of transforming the (1,0,0),
 * (0,1,0), (0,0,1) unit vectors from each gamut's linear space into BT.709
 * linear.  Values outside [0,1] are expected -- that's the whole point.
 */

/* BT.709 (identity -- these are the scRGB primaries themselves). */
static const vec3_t g_709_primaries[] = {
    { 1.000f,  0.000f,  0.000f},  /* Red     */
    { 0.000f,  1.000f,  0.000f},  /* Green   */
    { 0.000f,  0.000f,  1.000f},  /* Blue    */
    { 0.000f,  1.000f,  1.000f},  /* Cyan    */
    { 1.000f,  0.000f,  1.000f},  /* Magenta */
    { 1.000f,  1.000f,  0.000f},  /* Yellow  */
};

/*
 * DCI-P3 (D65) primaries in scRGB.
 * Computed from the P3-to-BT.709 conversion matrix.
 */
static const vec3_t g_p3_primaries[] = {
    { 1.2249f, -0.0420f, -0.0197f},  /* Red     */
    {-0.0420f,  1.0419f, -0.0197f},  /* Green   */
    {-0.0197f, -0.0197f,  1.0484f},  /* Blue    */
    {-0.0617f,  1.0222f,  1.0287f},  /* Cyan    */
    { 1.2052f, -0.0617f,  1.0287f},  /* Magenta */
    { 1.1829f,  0.9999f, -0.0394f},  /* Yellow  */
};

/*
 * BT.2020 primaries in scRGB.
 * Significantly outside BT.709 -- large negative values.
 */
static const vec3_t g_2020_primaries[] = {
    { 1.7167f, -0.3557f, -0.2534f},  /* Red     */
    {-0.6667f,  1.6165f,  0.0158f},  /* Green   */
    { 0.0176f, -0.0428f,  1.2369f},  /* Blue    */
    {-0.6491f,  1.5737f,  1.2527f},  /* Cyan    */
    { 1.7343f, -0.3985f,  0.9835f},  /* Magenta */
    { 1.0500f,  1.2608f, -0.2376f},  /* Yellow  */
};

static float rect_mask(vec2_t uv, float x0, float y0, float x1, float y1,
                       float feather)
{
    float l = smoothstepf(x0 - feather, x0 + feather, uv.x);
    float r = 1.0f - smoothstepf(x1 - feather, x1 + feather, uv.x);
    float b = smoothstepf(y0 - feather, y0 + feather, uv.y);
    float t = 1.0f - smoothstepf(y1 - feather, y1 + feather, uv.y);
    return l * r * b * t;
}

static void draw_row(vec2_t uv, float y0, float y1, const vec3_t *primaries,
                     float feather, vec3_t *color)
{
    float margin  = 0.04f;
    float col_gap = 0.010f;
    int   cols    = 7;  /* 6 colors + 1 white reference */
    float col_w   = (1.0f - 2.0f * margin - (float)(cols - 1) * col_gap)
                  / (float)cols;
    int i;

    for (i = 0; i < 6; i++) {
        float x0 = margin + (float)i * (col_w + col_gap);
        float x1 = x0 + col_w;
        float mask = rect_mask(uv, x0, y0, x1, y1, feather);

        if (mask > 0.001f) {
            /*
             * Output the scRGB value directly -- negative channels
             * are intentional.  The swap chain's scRGB color space
             * tells the DWM to interpret them as extended gamut.
             */
            *color = v3_lerp(*color, primaries[i], mask);
        }
    }

    /* White reference patch (D65 at 1.0 = 80 nits). */
    {
        float x0 = margin + 6.0f * (col_w + col_gap);
        float x1 = x0 + col_w;
        float mask = rect_mask(uv, x0, y0, x1, y1, feather);
        *color = v3_lerp(*color, vec3(1.0f, 1.0f, 1.0f), mask);
    }
}

vec4_t diag_hdr_gamut_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    vec2_t resolution = uniforms->resolution;
    vec2_t uv = vec2(
        (fragCoord.x + 0.5f) / resolution.x,
        (fragCoord.y + 0.5f) / resolution.y);

    float feather = 1.2f / resolution.x;

    /* Near-black background. */
    vec3_t color = vec3(0.012f, 0.013f, 0.016f);

    /* Row heights: three equal bands with gaps. */
    float row_gap    = 0.020f;
    float row_margin = 0.050f;
    float row_h      = (1.0f - 2.0f * row_margin - 2.0f * row_gap) / 3.0f;

    float y0_top = row_margin + 2.0f * (row_h + row_gap);
    float y0_mid = row_margin + row_h + row_gap;
    float y0_bot = row_margin;

    /* --- Row 0 (top): BT.709 --- */
    draw_row(uv, y0_top, y0_top + row_h, g_709_primaries, feather, &color);

    /* --- Row 1 (middle): DCI-P3 --- */
    draw_row(uv, y0_mid, y0_mid + row_h, g_p3_primaries, feather, &color);

    /* --- Row 2 (bottom): BT.2020 --- */
    draw_row(uv, y0_bot, y0_bot + row_h, g_2020_primaries, feather, &color);

    /* --- Row labels: thin tinted strips on the left edge --- */
    {
        float lx0 = 0.010f, lx1 = 0.030f;
        float lbl;

        lbl = rect_mask(uv, lx0, y0_top, lx1, y0_top + row_h, feather);
        color = v3_lerp(color, vec3(0.30f, 0.30f, 0.30f), lbl * 0.6f);

        lbl = rect_mask(uv, lx0, y0_mid, lx1, y0_mid + row_h, feather);
        color = v3_lerp(color, vec3(0.50f, 0.30f, 0.15f), lbl * 0.6f);

        lbl = rect_mask(uv, lx0, y0_bot, lx1, y0_bot + row_h, feather);
        color = v3_lerp(color, vec3(0.15f, 0.40f, 0.55f), lbl * 0.6f);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
