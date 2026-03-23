//  [HSV Picker Shader](https://fragcoord.xyz/s/uczjbb99)
#include "hsv_picker.h"

#define OUTER_RADIUS 0.99f
#define INNER_RADIUS 0.78f
#define TRIANGLE_CIRCUMRADIUS 0.77f
#define INDICATOR_RADIUS 0.04f
#define INDICATOR_RING_WIDTH 0.008f
#define HUE_CURSOR_RADIUS 0.032f
#define AA 0.006f
#define TAU 6.28318530718f

static float ring(float radius, float inner_radius, float outer_radius)
{
    return smoothstepf(outer_radius + AA, outer_radius - AA, radius) *
           smoothstepf(inner_radius - AA, inner_radius + AA, radius);
}

static float disk(float distance, float radius)
{
    return smoothstepf(radius + AA, radius - AA, distance);
}

static vec3_t barycentric(vec2_t p, vec2_t v0, vec2_t v1, vec2_t v2)
{
    vec2_t d0 = v2_sub(v1, v0);
    vec2_t d1 = v2_sub(v2, v0);
    vec2_t dp = v2_sub(p, v0);
    float denom = d0.x * d1.y - d0.y * d1.x;
    float u;
    float v;

    if (fabsf(denom) < 0.000001f) {
        return vec3(1.0f, 0.0f, 0.0f);
    }

    u = (dp.x * d1.y - dp.y * d1.x) / denom;
    v = (d0.x * dp.y - d0.y * dp.x) / denom;
    return vec3(1.0f - u - v, u, v);
}

static vec2_t picker_uv(vec2_t pixel, vec2_t resolution)
{
    vec2_t uv = vec2(pixel.x / resolution.x, pixel.y / resolution.y);
    uv = v2_sub(v2_mul1(uv, 2.0f), vec2_o(1.0f));
    uv.x *= resolution.x / resolution.y;
    return uv;
}

static bool point_on_ring(vec2_t uv)
{
    float radius = v2_length(uv);
    return radius >= INNER_RADIUS - AA && radius <= OUTER_RADIUS + AA;
}

static float hue_from_uv(vec2_t uv)
{
    float angle = atan2f(uv.y, uv.x);
    return (angle + PI) / TAU;
}

static float hue_angle(float hue)
{
    return hue * TAU - PI;
}

static void build_triangle(float hue, vec2_t *v0_out, vec2_t *v1_out, vec2_t *v2_out, vec2_t *hue_cursor_out)
{
    vec2_t v0;
    vec2_t v1;
    vec2_t v2;
    vec2_t hue_cursor_pos;
    float R = TRIANGLE_CIRCUMRADIUS;
    float h_ang = hue_angle(hue);

    v0 = v2_mul1(vec2(cosf(h_ang), sinf(h_ang)), R);
    v1 = v2_mul1(vec2(cosf(h_ang + TAU / 3.0f), sinf(h_ang + TAU / 3.0f)), R);
    v2 = v2_mul1(vec2(cosf(h_ang + 2.0f * TAU / 3.0f), sinf(h_ang + 2.0f * TAU / 3.0f)), R);
    hue_cursor_pos = v2_mul1(vec2(cosf(h_ang), sinf(h_ang)), (OUTER_RADIUS + INNER_RADIUS) * 0.5f);

    if (v0_out != NULL) {
        *v0_out = v0;
    }
    if (v1_out != NULL) {
        *v1_out = v1;
    }
    if (v2_out != NULL) {
        *v2_out = v2;
    }
    if (hue_cursor_out != NULL) {
        *hue_cursor_out = hue_cursor_pos;
    }
}

static bool point_in_triangle(vec2_t uv, float hue, vec3_t *bary_out)
{
    vec2_t v0;
    vec2_t v1;
    vec2_t v2;
    vec3_t bary;
    float tri_sdf;

    build_triangle(hue, &v0, &v1, &v2, NULL);
    bary = barycentric(uv, v0, v1, v2);
    tri_sdf = min(min(bary.x, bary.y), bary.z);

    if (bary_out != NULL) {
        *bary_out = bary;
    }

    return tri_sdf >= -AA;
}

hsv_picker_state_t hsv_picker_default_state(void)
{
    hsv_picker_state_t state;

    state.hue = 0.75f;
    state.sat = 0.80f;
    state.val = 0.735f;
    return state;
}

vec3_t hsv_picker_rgb(const hsv_picker_state_t *state)
{
    hsv_picker_state_t value = (state != NULL) ? *state : hsv_picker_default_state();
    return shader_hsv_to_rgb_v(vec3(value.hue, value.sat, value.val));
}

hsv_picker_region_t hsv_picker_hit_test(vec2_t pixel, vec2_t resolution, const hsv_picker_state_t *state)
{
    hsv_picker_state_t value = (state != NULL) ? *state : hsv_picker_default_state();
    vec2_t uv = picker_uv(pixel, resolution);

    if (point_on_ring(uv)) {
        return HSV_PICKER_REGION_HUE_RING;
    }

    if (point_in_triangle(uv, value.hue, NULL)) {
        return HSV_PICKER_REGION_TRIANGLE;
    }

    return HSV_PICKER_REGION_NONE;
}

hsv_picker_state_t hsv_picker_preview_state(
    const shader_uniforms_t *uniforms,
    const hsv_picker_state_t *base_state,
    hsv_picker_region_t active_region)
{
    hsv_picker_state_t value = (base_state != NULL) ? *base_state : hsv_picker_default_state();

    if (uniforms != NULL) {
        const vec2_t resolution = uniforms->resolution;
        const vec4_t mouse = uniforms->mouse;
        bool has_current = mouse.x >= 0.0f && mouse.y >= 0.0f;
        vec2_t current_pixel;
        hsv_picker_region_t region = active_region;

        if (has_current) {
            current_pixel = (mouse.z >= 0.0f && mouse.w >= 0.0f) ? vec2(mouse.z, mouse.w) : vec2(mouse.x, mouse.y);

            if (region == HSV_PICKER_REGION_NONE) {
                region = hsv_picker_hit_test(current_pixel, resolution, &value);
            }

            if (region == HSV_PICKER_REGION_HUE_RING) {
                vec2_t current_uv = picker_uv(current_pixel, resolution);

                if (point_on_ring(current_uv)) {
                    value.hue = hue_from_uv(current_uv);
                }
            } else if (region == HSV_PICKER_REGION_TRIANGLE) {
                vec2_t current_uv = picker_uv(current_pixel, resolution);
                vec3_t bary;

                if (point_in_triangle(current_uv, value.hue, &bary)) {
                    float v_component = saturate(bary.x + bary.y);
                    float s_component = (v_component > 0.0001f) ? saturate(bary.x / v_component) : 0.0f;

                    value.sat = s_component;
                    value.val = v_component;
                }
            }
        }
    }

    return value;
}

vec4_t hsv_picker_render(
    vec2_t fragCoord,
    const shader_uniforms_t *uniforms,
    const hsv_picker_state_t *base_state,
    hsv_picker_region_t active_region)
{
    const vec2_t resolution = uniforms->resolution;
    hsv_picker_state_t value = hsv_picker_preview_state(uniforms, base_state, active_region);
    vec2_t uv = picker_uv(fragCoord, resolution);
    float radius = v2_length(uv);
    float angle = atan2f(uv.y, uv.x);
    float hue_on_ring = (angle + PI) / TAU;
    vec3_t ring_col = shader_hsv_to_rgb_v(vec3(hue_on_ring, 1.0f, 1.0f));
    float ring_mask = ring(radius, INNER_RADIUS, OUTER_RADIUS);
    vec3_t bg = vec3(0.08f, 0.08f, 0.09f);
    vec2_t v0;
    vec2_t v1;
    vec2_t v2;
    vec3_t b;
    float tri_sdf;
    float tri_mask;
    vec3_t hue_col;
    vec3_t tri_col;
    vec2_t ind_pos;
    float ind_dist;
    vec3_t sel_col;
    vec3_t comp_col;
    float dot_mask;
    float ring_out;
    float comp_mask;
    vec3_t col;
    float inner_circle_mask;
    vec2_t hue_cursor_pos;
    float hue_cursor_mask;

    build_triangle(value.hue, &v0, &v1, &v2, &hue_cursor_pos);
    b = barycentric(uv, v0, v1, v2);
    tri_sdf = min(min(b.x, b.y), b.z);
    tri_mask = smoothstepf(-AA, AA, tri_sdf);

    hue_col = shader_hsv_to_rgb_v(vec3(value.hue, 1.0f, 1.0f));
    tri_col = v3_add(v3_mul1(hue_col, b.x), vec3_o(b.y));

    ind_pos = v2_add(
        v2_add(v2_mul1(v0, value.val * value.sat), v2_mul1(v1, value.val * (1.0f - value.sat))),
        v2_mul1(v2, 1.0f - value.val));
    ind_dist = v2_distance(uv, ind_pos);
    sel_col = shader_hsv_to_rgb_v(vec3(value.hue, value.sat, value.val));
    comp_col = shader_hsv_to_rgb_v(vec3((value.hue + 0.5f) - floorf(value.hue + 0.5f), 1.0f, 1.0f));
    dot_mask = disk(ind_dist, INDICATOR_RADIUS);
    ring_out = INDICATOR_RADIUS + INDICATOR_RING_WIDTH;
    comp_mask = ring(ind_dist, INDICATOR_RADIUS, ring_out);

    inner_circle_mask = smoothstepf(INNER_RADIUS + AA, INNER_RADIUS - AA, radius);
    hue_cursor_mask = disk(v2_distance(uv, hue_cursor_pos), HUE_CURSOR_RADIUS);

    col = bg;
    col = v3_lerp(col, tri_col, tri_mask * inner_circle_mask);
    col = v3_lerp(col, ring_col, ring_mask);
    col = v3_lerp(col, comp_col, comp_mask);
    col = v3_lerp(col, vec3_o(0.98f), hue_cursor_mask * ring_mask);
    col = v3_lerp(col, sel_col, dot_mask);

    return vec4(col.x, col.y, col.z, 1.0f);
}

vec4_t hsv_picker_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return hsv_picker_render(fragCoord, uniforms, NULL, HSV_PICKER_REGION_NONE);
}
