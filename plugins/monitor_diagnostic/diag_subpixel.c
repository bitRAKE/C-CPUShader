#include "diag_subpixel.h"

/*
 * Sub-pixel Grid — pixel-exact test patterns.
 *
 * Layout: 4 rows × 3 columns = 12 zones
 *   Rows (bottom to top): White, Red, Green, Blue
 *   Columns (left to right): 1px, 2px, 4px grid spacing
 *
 * Each zone shows a checkerboard at the specified pixel spacing
 * using the specified color channel. The 1px checkerboard is the
 * ultimate test: if the monitor is at native resolution with no
 * scaling, it should appear as a uniform 50% gray (or 50% color).
 * Any moire, color fringing, or dead pixels will be immediately visible.
 *
 * Improvement over web version:
 *   - True 1:1 pixel mapping (no CSS DPR scaling)
 *   - Integer pixel coordinates guarantee exact patterns
 *   - Per-channel isolation exposes stuck sub-pixels
 *   - Multiple grid densities for scaling detection
 */

vec4_t diag_subpixel_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    int px = (int)fragCoord.x;
    int py = (int)fragCoord.y;
    float u = fragCoord.x / w;
    float v = fragCoord.y / h;

    /* 4 rows: White, Red, Green, Blue (bottom to top) */
    int row = (int)(v * 4.0f);
    if (row > 3) row = 3;

    /* 3 columns: 1px, 2px, 4px spacing */
    int col = (int)(u * 3.0f);
    if (col > 2) col = 2;

    static const int spacings[] = { 1, 2, 4 };
    int spacing = spacings[col];

    /* Checkerboard pattern at the given spacing */
    int cx = (px / spacing) & 1;
    int cy = (py / spacing) & 1;
    int checker = cx ^ cy;

    float val = checker ? 1.0f : 0.0f;

    vec3_t color;
    switch (row) {
    case 0: color = vec3(val, val, val);   break; /* White */
    case 1: color = vec3(val, 0.0f, 0.0f); break; /* Red */
    case 2: color = vec3(0.0f, val, 0.0f); break; /* Green */
    case 3: color = vec3(0.0f, 0.0f, val); break; /* Blue */
    default: color = vec3_o(0.0f);          break;
    }

    /* Row separators: 2px lines for visibility */
    float row_frac = v * 4.0f - (float)row;
    float row_px = h / 4.0f;
    if (row_frac * row_px < 2.0f && row > 0) {
        color = vec3(0.25f, 0.25f, 0.25f);
    }

    /* Column separators: 2px lines */
    float col_frac = u * 3.0f - (float)col;
    float col_px = w / 3.0f;
    if (col_frac * col_px < 2.0f && col > 0) {
        color = vec3(0.25f, 0.25f, 0.25f);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
