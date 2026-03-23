//Kinetic Orbs Shader

#include "kinetic_orbs.h"

static float fractf_local(float x)
{
    return x - floorf(x);
}

static float hash1(float n)
{
    return fractf_local(sinf(n) * 43758.5453f);
}

static float noise2(vec2_t p)
{
    vec2_t cell = v2_floor(p);
    vec2_t frac = v2_fract(p);
    float base = cell.x + cell.y * 57.0f;

    frac.x = frac.x * frac.x * (3.0f - 2.0f * frac.x);
    frac.y = frac.y * frac.y * (3.0f - 2.0f * frac.y);

    return lerpf(
        lerpf(hash1(base + 0.0f), hash1(base + 1.0f), frac.x),
        lerpf(hash1(base + 57.0f), hash1(base + 58.0f), frac.x),
        frac.y);
}

static float smin(float a, float b, float k)
{
    float h = saturate(0.5f + 0.5f * (b - a) / k);
    return lerpf(b, a, h) - k * h * (1.0f - h);
}

vec4_t kinetic_orbs_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    const float time = uniforms->time;
    vec2_t uv = vec2((fragCoord.x - resolution.x * 0.5f) / resolution.y,
                     (fragCoord.y - resolution.y * 0.5f) / resolution.y);
    vec2_t flow_uv = uv;
    vec2_t centers[4];
    vec3_t tints[4];
    vec3_t glow_acc = vec3_o(0.0f);
    vec3_t core_acc = vec3_o(0.0f);
    vec3_t final_color;
    vec3_t background;
    float dist_field = 1000000.0f;
    float flow_a;
    float flow_b;
    float nebula;
    float edge;
    float core_mask;
    float vignette;

    flow_a = noise2(v2_add(v2_mul1(uv, 3.0f), vec2(time * 0.20f, time * 0.15f)));
    flow_b = noise2(v2_add(v2_mul1(uv, 5.5f), vec2(-time * 0.14f, time * 0.11f)));

    flow_uv.x += 0.05f * sinf(flow_a * 6.28318f + time) + 0.02f * cosf(flow_b * 9.0f - time * 0.6f);
    flow_uv.y += 0.05f * cosf(flow_a * 6.28318f - time) + 0.02f * sinf(flow_b * 8.0f + time * 0.5f);

    centers[0] = vec2(sinf(time * 0.8f) * 0.4f, cosf(time * 0.9f) * 0.2f);
    centers[1] = vec2(cosf(time * 0.6f) * 0.5f, sinf(time * 0.7f) * 0.3f);
    centers[2] = vec2(sinf(time * 1.1f) * 0.3f, sinf(time * 0.5f) * 0.4f);
    centers[3] = vec2(cosf(time * 0.5f) * 0.6f, cosf(time * 0.8f) * 0.1f);

    tints[0] = vec3(1.0f, 0.4f, 0.1f);
    tints[1] = vec3(0.1f, 0.7f, 1.0f);
    tints[2] = vec3(0.8f, 0.2f, 1.0f);
    tints[3] = vec3(0.9f, 0.9f, 0.2f);

    for (int i = 0; i < 4; i++) {
        float pulse = 0.5f + 0.5f * sinf(time * 2.0f + (float)i * 1.37f);
        float radius = 0.12f + 0.02f * pulse;
        float distance = v2_distance(flow_uv, centers[i]);
        float falloff = 1.0f / (1.0f + distance * distance * 60.0f);
        float shell = expf(-fabsf(distance - radius) * (18.0f - 5.0f * pulse));
        float core = smoothstepf(radius * 0.72f, 0.0f, distance);

        dist_field = smin(dist_field, distance - radius, 0.15f);
        glow_acc = v3_add(glow_acc, v3_mul1(tints[i], falloff * (0.8f + 0.4f * pulse) + shell * 0.55f));
        core_acc = v3_add(core_acc, v3_mul1(tints[i], core * (0.35f + 0.25f * pulse)));
    }

    nebula = noise2(v2_add(v2_mul1(flow_uv, 2.4f), vec2(time * 0.05f, -time * 0.03f)));
    background = v3_lerp(vec3(0.01f, 0.02f, 0.05f), vec3(0.05f, 0.02f, 0.06f), nebula);
    background = v3_add(background, vec3(0.02f * powf(flow_a, 3.0f), 0.01f * powf(nebula, 2.0f), 0.05f * powf(flow_b, 4.0f)));

    edge = smoothstepf(0.01f, -0.02f, dist_field);
    core_mask = smoothstepf(-0.02f, -0.14f, dist_field);

    final_color = v3_add(background, glow_acc);
    final_color = v3_add(final_color, v3_mul1(core_acc, 0.35f));
    final_color = v3_lerp(final_color, v3_add(v3_mul1(glow_acc, 0.45f), v3_mul1(core_acc, 0.65f)), edge);
    final_color = v3_add(final_color, v3_mul1(vec3_o(1.0f), core_mask * 0.60f));

    vignette = saturate(1.0f - 0.7f * v2_dot(uv, uv));
    final_color = v3_mul1(final_color, vignette);
    final_color = shader_aces_tonemap(v3_mul1(final_color, 2.5f));
    final_color = shader_gamma_encode(final_color);

    return vec4(final_color.x, final_color.y, final_color.z, 1.0f);
}
