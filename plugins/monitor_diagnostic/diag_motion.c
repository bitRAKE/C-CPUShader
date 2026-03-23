#include "diag_motion.h"

/*
 * Motion Response — four horizontal lanes with colored blocks at
 * controlled integer pixel-per-frame speeds.
 *
 *   Lane 0 (bottom): 1 px/frame (slow)   — White block
 *   Lane 1:          2 px/frame           — Red block
 *   Lane 2:          4 px/frame           — Green block
 *   Lane 3 (top):    8 px/frame (fast)    — Blue block
 *
 * Each lane also has a static reference block on the left edge.
 * Vertical grid lines at 120px intervals provide position reference.
 *
 * Improvement over web version:
 *   - Exact integer pixel speeds (no sub-pixel motion)
 *   - Frame-counter driven (uniforms->frame), not time-based
 *   - Static reference block for same-lane comparison
 *   - Dark background for high-contrast ghosting visibility
 */

vec4_t diag_motion_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    int iw = (int)w;
    int px = (int)fragCoord.x;
    int py = (int)fragCoord.y;
    float v = fragCoord.y / h;

    /* Speeds in pixels per frame */
    static const int speeds[] = { 1, 2, 4, 8 };
    static const float colors[][3] = {
        { 1.0f, 1.0f, 1.0f },  /* White */
        { 1.0f, 0.15f, 0.1f }, /* Red */
        { 0.1f, 1.0f, 0.15f }, /* Green */
        { 0.15f, 0.4f, 1.0f }  /* Blue */
    };

    vec3_t color = vec3(0.02f, 0.02f, 0.025f);

    /* Determine lane */
    int lane = (int)(v * 4.0f);
    if (lane > 3) lane = 3;
    float lane_frac = v * 4.0f - (float)lane;
    float lane_h = h / 4.0f;

    /* Block dimensions */
    int block_w = 80;
    int block_h_px = (int)(lane_h * 0.6f);
    int block_y_start = (int)((float)lane * lane_h + lane_h * 0.2f);
    int block_y_end = block_y_start + block_h_px;

    /* Moving block position — wraps at screen width */
    int frame = (int)uniforms->frame;
    int block_x = (frame * speeds[lane]) % iw;
    bool in_block_y = (py >= block_y_start && py < block_y_end);

    /* Moving block */
    if (in_block_y) {
        int dx = px - block_x;
        if (dx < 0) dx += iw;  /* wrap-around */
        if (dx < block_w) {
            vec3_t bc = vec3(colors[lane][0], colors[lane][1], colors[lane][2]);
            /* Slight edge softening at block boundary */
            float edge_x = (float)dx / (float)block_w;
            float edge = smoothstepf(0.0f, 0.05f, edge_x) *
                         (1.0f - smoothstepf(0.95f, 1.0f, edge_x));
            color = v3_mul1(bc, edge);
        }
    }

    /* Static reference block at left edge */
    if (in_block_y && px >= 10 && px < 10 + block_w / 2) {
        vec3_t bc = vec3(colors[lane][0], colors[lane][1], colors[lane][2]);
        color = v3_mul1(bc, 0.5f);
    }

    /* Vertical grid lines every 120px */
    if (px % 120 == 0) {
        color = v3_add(color, vec3_o(0.06f));
    }

    /* Lane separators */
    if (lane_frac * lane_h < 1.0f && lane > 0) {
        color = vec3(0.12f, 0.14f, 0.18f);
    }

    /* Horizontal center line per lane */
    float center_y = ((float)lane + 0.5f) * lane_h;
    if (fabsf(fragCoord.y - center_y) < 0.5f) {
        color = v3_add(color, vec3(0.04f, 0.04f, 0.06f));
    }

    return vec4(saturate(color.x), saturate(color.y), saturate(color.z), 1.0f);
}
