#include "diag_hsv_wheel.h"

/*
 * HSV Wheel — full-screen circular color wheel.
 *
 * Improvement over web version:
 *   - Precise saturation gradient from center (S=0) to edge (S=1)
 *   - Primary (R/G/B) and secondary (C/M/Y) hue markers at 60° intervals
 *   - Inner ring at V=0.5 and outer ring at V=1.0 for value comparison
 *   - Pure white center reference point
 *   - Anti-aliased edges at exact pixel boundaries
 */

vec4_t diag_hsv_wheel_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    float w = uniforms->resolution.x;
    float h = uniforms->resolution.y;
    float dim = fminf(w, h);

    /* Center the wheel in the viewport.
     * Scale 0.42 leaves margin for hue dots at r=1.08. */
    float cx = (fragCoord.x - w * 0.5f) / (dim * 0.42f);
    float cy = (fragCoord.y - h * 0.5f) / (dim * 0.42f);
    float radius = sqrtf(cx * cx + cy * cy);
    float angle = atan2f(cy, cx);
    float hue = (angle + PI) / (2.0f * PI);

    vec3_t color = vec3(0.04f, 0.04f, 0.05f);

    /* Main wheel: saturation = radius, value = 1.0 */
    float wheel_edge = smoothstepf(1.02f, 1.0f, radius);
    if (radius <= 1.02f) {
        float sat = saturate(radius);
        vec3_t wheel = shader_hsv_to_rgb(hue, sat, 1.0f);
        color = v3_lerp(color, wheel, wheel_edge);
    }

    /* Inner value ring at r ≈ 0.55: shows V=0.5 */
    {
        float ring_r = fabsf(radius - 0.55f);
        float ring = smoothstepf(0.012f, 0.008f, ring_r);
        if (ring > 0.0f) {
            vec3_t ring_color = shader_hsv_to_rgb(hue, 1.0f, 0.5f);
            color = v3_lerp(color, ring_color, ring * 0.7f);
        }
    }

    /* Outer reference ring at r ≈ 0.92: full saturation/value */
    {
        float ring_r = fabsf(radius - 0.92f);
        float ring = smoothstepf(0.012f, 0.008f, ring_r);
        if (ring > 0.0f) {
            vec3_t ring_color = shader_hsv_to_rgb(hue, 1.0f, 1.0f);
            color = v3_lerp(color, ring_color, ring * 0.6f);
        }
    }

    /* Hue markers at 0°,60°,120°,180°,240°,300° */
    for (int i = 0; i < 6; i++) {
        float marker_angle = (float)i * PI / 3.0f - PI;
        float da = angle - marker_angle;
        /* Wrap */
        if (da > PI) da -= 2.0f * PI;
        if (da < -PI) da += 2.0f * PI;
        float angular_dist = fabsf(da);

        /* Thin radial line */
        if (angular_dist < 0.008f && radius > 0.06f && radius < 1.0f) {
            float line = smoothstepf(0.008f, 0.004f, angular_dist);
            float bright = (i < 3) ? 1.0f : 0.7f; /* primaries brighter */
            color = v3_lerp(color, vec3_o(bright), line * 0.5f);
        }

        /* Dot at r=1.05 outside wheel */
        float dx = cosf(marker_angle) * 1.08f - cx;
        float dy = sinf(marker_angle) * 1.08f - cy;
        float dot_r = sqrtf(dx * dx + dy * dy);
        float dot = smoothstepf(0.04f, 0.03f, dot_r);
        if (dot > 0.0f) {
            vec3_t dot_color = shader_hsv_to_rgb((float)i / 6.0f, 1.0f, 1.0f);
            color = v3_lerp(color, dot_color, dot);
        }
    }

    /* Center white reference */
    {
        float center_r = radius;
        float white = smoothstepf(0.04f, 0.03f, center_r);
        color = v3_lerp(color, vec3(1.0f, 1.0f, 1.0f), white);
    }

    return vec4(color.x, color.y, color.z, 1.0f);
}
