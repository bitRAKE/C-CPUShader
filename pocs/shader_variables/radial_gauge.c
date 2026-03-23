#include "radial_gauge.h"

#include "u_vars.h"

#include <stddef.h>

const shader_variable_desc_t g_radial_gauge_variables[RADIAL_GAUGE_VARIABLE_COUNT] = {
    {"value",       SHADER_VARIABLE_TYPE_FLOAT, offsetof(radial_gauge_vars_t, value),       "0.72"},
    {"arc_color",   SHADER_VARIABLE_TYPE_VEC3,  offsetof(radial_gauge_vars_t, arc_color),   "0.22, 0.65, 0.88"},
    {"track_color", SHADER_VARIABLE_TYPE_VEC3,  offsetof(radial_gauge_vars_t, track_color), "0.18, 0.19, 0.22"},
    {"thickness",   SHADER_VARIABLE_TYPE_FLOAT, offsetof(radial_gauge_vars_t, thickness),   "0.10"},
};

static const radial_gauge_vars_t g_radial_gauge_fallback = {
    0.72f,
    {0.22f, 0.65f, 0.88f},
    {0.18f, 0.19f, 0.22f},
    0.10f,
};

/*
 * The gauge is a 270-degree arc with a 90-degree gap at the bottom.
 *
 * Angular layout (standard atan2, y-up):
 *
 *       pi/2 (top, gauge midpoint)
 *        |
 *  pi ---+--- 0 (right)
 *        |
 *      -pi/2 (bottom, gap center)
 *
 * Arc start:  -3pi/4  (lower-left,  225 degrees)
 * Arc end:    -pi/4   (lower-right, 315 degrees)
 * Gap:        -3pi/4 to -pi/4 through bottom
 *
 * Fill direction:  CW from lower-left to lower-right (through top).
 *   CW means decreasing atan2 angle (with wrap at +/-pi).
 */

#define GAUGE_ARC_START (-0.75f * PI)   /* -3pi/4 = lower-left  */
#define GAUGE_ARC_SPAN  ( 1.50f * PI)   /* 270 degrees CW       */
#define GAUGE_RADIUS    0.72f

/*
 * Arc SDF with rounded endcaps.
 *
 * start_angle:  atan2 angle of the arc's starting point
 * span:         angular extent going clockwise (positive value)
 * radius:       arc centerline radius
 * half_thick:   half the stroke width
 */
static float arc_sdf(vec2_t p, float start_angle, float span,
                     float radius, float half_thick)
{
    float r = v2_length(p);
    float angle = atan2f(p.y, p.x);
    float a_from_start, end_angle;
    float ring_dist;
    vec2_t p_start, p_end;
    float d_start, d_end;

    /* Angular distance from start, measured clockwise (decreasing angle). */
    a_from_start = start_angle - angle;
    if (a_from_start < 0.0f)        a_from_start += 2.0f * PI;
    if (a_from_start > 2.0f * PI)   a_from_start -= 2.0f * PI;

    if (a_from_start >= 0.0f && a_from_start <= span) {
        /* Within the arc's angular range: ring distance only. */
        ring_dist = fabsf(r - radius) - half_thick;
        return ring_dist;
    }

    /* Outside the arc: distance to the nearest rounded endcap. */
    p_start.x = cosf(start_angle) * radius;
    p_start.y = sinf(start_angle) * radius;

    end_angle = start_angle - span;
    p_end.x   = cosf(end_angle) * radius;
    p_end.y   = sinf(end_angle) * radius;

    d_start = v2_length(v2_sub(p, p_start)) - half_thick;
    d_end   = v2_length(v2_sub(p, p_end))   - half_thick;
    return min(d_start, d_end);
}

/*
 * Compute the normalized position [0,1] along the gauge arc.
 * Returns -1 if the pixel is in the gap.
 */
static float gauge_arc_t(vec2_t p)
{
    float angle = atan2f(p.y, p.x);
    float a;

    a = GAUGE_ARC_START - angle;
    if (a < 0.0f)      a += 2.0f * PI;
    if (a > 2.0f * PI) a -= 2.0f * PI;

    if (a > GAUGE_ARC_SPAN) return -1.0f;
    return a / GAUGE_ARC_SPAN;
}

vec4_t radial_gauge_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const radial_gauge_vars_t *vars = U_VARS_AS(radial_gauge_vars_t, uniforms);
    vec4_t result = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec2_t resolution;
    float min_axis, aa;
    vec2_t uv;
    float value, half_thick;
    vec3_t arc_color, track_color;

    float track_sdf, track_mask;
    float value_span, value_sdf, value_mask;
    float pixel_t;
    float pip_sdf, pip_mask;
    vec2_t pip_center;
    float pip_angle;
    float edge_glow;
    vec3_t value_surface;

    if (vars == NULL) {
        vars = &g_radial_gauge_fallback;
    }

    resolution = (uniforms != NULL) ? uniforms->resolution : vec2(192.0f, 192.0f);
    min_axis   = max(1.0f, min(resolution.x, resolution.y));
    uv = vec2(
        ((fragCoord.x + 0.5f) * 2.0f - resolution.x) / min_axis,
        ((fragCoord.y + 0.5f) * 2.0f - resolution.y) / min_axis);
    aa = (2.0f / min_axis) * 1.5f;

    value       = clampf(vars->value, 0.0f, 1.0f);
    half_thick  = clampf(vars->thickness, 0.04f, 0.20f) * 0.5f;
    arc_color   = v3_saturate(vars->arc_color);
    track_color = v3_saturate(vars->track_color);

    /* --- Background track (full 270-degree arc) --- */
    track_sdf  = arc_sdf(uv, GAUGE_ARC_START, GAUGE_ARC_SPAN,
                         GAUGE_RADIUS, half_thick);
    track_mask = shader_sdf_fill(track_sdf, aa);

    result = shader_layer_over(result,
        vec4(track_color.x, track_color.y, track_color.z, track_mask));

    /* Subtle inner bevel on track: lighter top edge, darker bottom. */
    {
        float bevel = track_mask * shader_sdf_band(track_sdf, -half_thick * 0.7f, 0.0f);
        float top_light = bevel * smoothstepf(-0.20f, 0.60f, uv.y) * 0.06f;
        float bot_dark  = bevel * smoothstepf(0.10f, -0.50f, uv.y) * 0.04f;
        result = shader_layer_over(result, vec4(1.0f, 1.0f, 1.0f, top_light));
        result = shader_layer_over(result, vec4(0.0f, 0.0f, 0.0f, bot_dark));
    }

    if (value > 0.001f) {
        /* --- Value arc (partial fill) --- */
        value_span = value * GAUGE_ARC_SPAN;
        value_sdf  = arc_sdf(uv, GAUGE_ARC_START, value_span,
                             GAUGE_RADIUS, half_thick);
        value_mask = shader_sdf_fill(value_sdf, aa);

        /* Slight brightness gradient: brighter toward the leading edge. */
        pixel_t = gauge_arc_t(uv);
        if (pixel_t < 0.0f) pixel_t = 0.0f;
        {
            float brightness = 0.85f + 0.15f * smoothstepf(
                value * 0.5f, value, pixel_t);
            value_surface = v3_mul1(arc_color, brightness);
        }

        /* Subtle highlight along the outer radius. */
        {
            float r_from_center = v2_length(uv);
            float outer_highlight = smoothstepf(
                GAUGE_RADIUS - half_thick * 0.2f,
                GAUGE_RADIUS + half_thick * 0.5f,
                r_from_center) * 0.08f;
            value_surface = v3_add(value_surface,
                vec3(outer_highlight, outer_highlight, outer_highlight));
        }

        result = shader_layer_over(result,
            vec4(saturate(value_surface.x),
                 saturate(value_surface.y),
                 saturate(value_surface.z),
                 value_mask));

        /* --- Pip at the value endpoint --- */
        pip_angle  = GAUGE_ARC_START - value_span;
        pip_center = vec2(
            cosf(pip_angle) * GAUGE_RADIUS,
            sinf(pip_angle) * GAUGE_RADIUS);
        pip_sdf  = v2_length(v2_sub(uv, pip_center)) - half_thick * 1.6f;
        pip_mask = shader_sdf_fill(pip_sdf, aa);

        result = shader_layer_over(result,
            vec4(saturate(arc_color.x * 1.15f),
                 saturate(arc_color.y * 1.15f),
                 saturate(arc_color.z * 1.15f),
                 pip_mask));

        /* Pip specular dot. */
        {
            float pip_dist = v2_length(v2_sub(uv, pip_center));
            float pip_spec = powf(
                saturate(1.0f - pip_dist / (half_thick * 1.6f)),
                6.0f) * 0.35f;
            result = shader_layer_over(result,
                vec4(1.0f, 1.0f, 1.0f, pip_mask * pip_spec));
        }
    }

    /* --- Soft glow behind the value arc --- */
    if (value > 0.001f) {
        value_span = value * GAUGE_ARC_SPAN;
        edge_glow  = arc_sdf(uv, GAUGE_ARC_START, value_span,
                             GAUGE_RADIUS, half_thick * 3.0f);
        edge_glow  = shader_sdf_fill(edge_glow, aa * 6.0f) * 0.08f;
        result = shader_layer_over(result,
            vec4(arc_color.x, arc_color.y, arc_color.z, edge_glow));
    }

    return vec4(saturate(result.x), saturate(result.y),
                saturate(result.z), saturate(result.w));
}
