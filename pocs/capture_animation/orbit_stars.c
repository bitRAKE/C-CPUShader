#include "orbit_stars.h"

static float sd_star(vec2_t p, float radius, float pointiness)
{
    const vec2_t k1 = { 0.809016994375f, -0.587785252292f };
    const vec2_t k2 = { -0.809016994375f, -0.587785252292f };
    vec2_t ba;
    float h;

    p.x = fabsf(p.x);
    p = v2_sub(p, v2_mul1(k1, 2.0f * fmaxf(v2_dot(k1, p), 0.0f)));
    p = v2_sub(p, v2_mul1(k2, 2.0f * fmaxf(v2_dot(k2, p), 0.0f)));
    p.x = fabsf(p.x);
    p.y -= radius;

    ba = v2_sub(v2_mul1(vec2(-k1.y, k1.x), pointiness), vec2(0.0f, 1.0f));
    h = clampf(v2_dot(p, ba) / v2_dot(ba, ba), 0.0f, radius);
    return v2_length(v2_sub(p, v2_mul1(ba, h))) * ((p.y * ba.x - p.x * ba.y) < 0.0f ? -1.0f : 1.0f);
}

vec4_t orbit_stars_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    const uint loop_frame = uniforms->frame % ORBIT_STARS_LOOP_FRAMES;
    const float tau = 2.0f * PI;
    const float orbit_phase = tau * ((float)loop_frame / (float)ORBIT_STARS_LOOP_FRAMES);
    vec2_t uv = vec2(
        (fragCoord.x - resolution.x * 0.5f) / resolution.y,
        (fragCoord.y - resolution.y * 0.5f) / resolution.y);
    vec2_t centered = vec2(uv.x * (resolution.y / resolution.x), uv.y * 1.6f);
    vec3_t color = vec3(0.018f, 0.026f, 0.060f);
    float alpha = 0.0f;
    const int star_count = 5;
    float vignette = saturate(1.08f - 0.52f * v2_dot(centered, centered));

    color = v3_mul1(color, 0.78f + 0.22f * vignette);

    for (int i = 0; i < star_count; i++) {
        float phase = tau * ((float)i / (float)star_count);
        float angle = orbit_phase + phase;
        vec2_t orbit_center = vec2(cosf(angle) * 0.40f, sinf(angle) * 0.15f);
        vec2_t tangent = vec2(-sinf(angle) * 0.40f, cosf(angle) * 0.15f);
        vec2_t velocity_dir = v2_normalize(tangent);
        vec2_t side_dir = vec2(-velocity_dir.y, velocity_dir.x);
        float depth_scale = smoothstepf(-0.15f, 0.15f, orbit_center.y) * 0.45f + 0.72f;
        float star_angle = -angle * 1.4f + phase * 0.35f;
        vec2_t local = v2_rotate(v2_div1(v2_sub(uv, orbit_center), depth_scale), star_angle);
        float d = sd_star(local, 0.072f, 0.42f);
        float fill = smoothstepf(0.010f, 0.0f, d);
        float rim = smoothstepf(0.050f, 0.0f, fabsf(d));
        vec2_t delta = v2_sub(uv, orbit_center);
        float back = fmaxf(0.0f, -v2_dot(delta, velocity_dir));
        float side = fabsf(v2_dot(delta, side_dir));
        float trail_seed = smoothstepf(0.012f, 0.050f, back);
        float trail = trail_seed *
                      expf(-back * (11.0f / depth_scale)) *
                      expf(-(side * side) * (210.0f / depth_scale));
        vec3_t star_tint = v3_lerp(
            vec3(1.00f, 0.88f, 0.24f),
            vec3(1.00f, 0.70f, 0.18f),
            (float)i / (float)(star_count - 1));
        vec3_t core_color = v3_mul1(star_tint, fill * (1.05f + 0.40f * depth_scale));
        vec3_t rim_color = v3_mul1(vec3(1.0f, 0.98f, 0.82f), rim * 0.18f * depth_scale);
        vec3_t trail_color = v3_mul1(v3_lerp(star_tint, vec3(1.0f, 0.56f, 0.12f), 0.35f), trail * 0.22f * depth_scale);
        float star_alpha = fill * 0.92f + rim * 0.18f + trail * 0.45f;

        color = v3_add(color, core_color);
        color = v3_add(color, rim_color);
        color = v3_add(color, trail_color);
        alpha = fmaxf(alpha, saturate(star_alpha));
    }

    color = v3_add(color, vec3(0.01f, 0.008f, 0.020f));
    color = v3_mul1(color, vignette);
    alpha = saturate(alpha * vignette);
    return vec4(color.x, color.y, color.z, alpha);
}
