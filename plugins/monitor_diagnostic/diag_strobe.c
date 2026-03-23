#include "diag_strobe.h"

/*
 * Strobe / Flicker — frame-count driven alternation zones.
 *
 *   Six vertical columns, each toggling black/white at a different
 *   frame period:
 *     Col 0: every frame     (period 1 — fastest possible)
 *     Col 1: every 2 frames  (period 2)
 *     Col 2: every 3 frames  (period 3)
 *     Col 3: every 4 frames  (period 4)
 *     Col 4: every 6 frames  (period 6)
 *     Col 5: every 8 frames  (period 8 — slowest)
 *
 *   Top half: normal polarity (black → white)
 *   Bottom half: inverted polarity (white → black)
 *   Center strip: pure black-white strobe at period 2
 *
 * Improvement over web version:
 *   - Frame-exact timing (no requestAnimationFrame jitter)
 *   - Fixed integer periods, not variable-frequency sinusoid
 *   - Inverted zone for persistence comparison
 *   - No column-striping — clean full-zone fills for clear flicker detection
 */

vec4_t diag_strobe_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    float u = fragCoord.x / w;
    float v = fragCoord.y / h;
    uint frame = uniforms->frame;

    /* Six columns */
    static const int periods[] = { 1, 2, 3, 4, 6, 8 };
    int col = (int)(u * 6.0f);
    if (col > 5) col = 5;

    int period = periods[col];
    int phase = (int)(frame % (uint)(period * 2));
    float strobe = (phase < period) ? 1.0f : 0.0f;

    /* Top half: normal, bottom half: inverted */
    float val;
    if (v > 0.55f) {
        val = strobe;
    } else if (v < 0.45f) {
        val = 1.0f - strobe;
    } else {
        /* Center strip: period-2 strobe regardless of column */
        val = (frame & 1u) ? 1.0f : 0.0f;
    }

    vec3_t color = vec3_o(val);

    /* Column separators */
    float col_frac = u * 6.0f - (float)col;
    float col_px = w / 6.0f;
    if (col_frac * col_px < 1.5f && col > 0) {
        color = vec3(0.3f, 0.0f, 0.0f);
    }

    /* Center strip boundaries */
    float center_dist_top = fabsf(v - 0.55f) * h;
    float center_dist_bot = fabsf(v - 0.45f) * h;
    if (center_dist_top < 1.0f || center_dist_bot < 1.0f) {
        color = vec3(0.0f, 0.6f, 0.4f);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
