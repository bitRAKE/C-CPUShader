#include "animated_sprite.h"

#define SPRITE_SIZE 16
#define SPRITE_FRAME_COUNT 4

static const char *k_sprite_frames[SPRITE_FRAME_COUNT][SPRITE_SIZE] = {
    {
        "................",
        "................",
        "......BB........",
        "....WWBBBB......",
        "...WWWWBBBR.....",
        "..OYWWWBBBBWR...",
        ".OOYWWWWWWWWW...",
        "OOYWWWWWWWWWWWW",
        "OOYWWWWWWWWWWWW",
        ".OOYWWWWWWWWW...",
        "..OYWWWBBBBWR...",
        "...WWWWBBBR.....",
        "....WWBBBB......",
        "......BB........",
        "................",
        "................",
    },
    {
        "................",
        "................",
        "......BB........",
        "....WWBBBB......",
        "...WWWWBBBR.....",
        "...YWWWBBBBWR...",
        "..OYWWWWWWWWW...",
        ".OOYWWWWWWWWWWW.",
        ".OOYWWWWWWWWWWW.",
        "..OYWWWWWWWWW...",
        "...YWWWBBBBWR...",
        "...WWWWBBBR.....",
        "....WWBBBB......",
        "......BB........",
        "................",
        "................",
    },
    {
        "................",
        "................",
        "......BB........",
        "....WWBBBB......",
        "...WWWWBBBR.....",
        ".OOYWWWBBBBWR...",
        "OOOYWWWWWWWWW...",
        "OOYWWWWWWWWWWWW",
        "OOYWWWWWWWWWWWW",
        "OOOYWWWWWWWWW...",
        ".OOYWWWBBBBWR...",
        "...WWWWBBBR.....",
        "....WWBBBB......",
        "......BB........",
        "................",
        "................",
    },
    {
        "................",
        "................",
        "......BB........",
        "....WWBBBB......",
        "...WWWWBBBR.....",
        "..OYWWWBBBBWR...",
        ".OOYWWWWWWWWW...",
        "OOYWWWWWWWWWWWW",
        ".OOYWWWWWWWWWWW.",
        "..OYWWWWWWWWW...",
        "...YWWWBBBBWR...",
        "...WWWWBBBR.....",
        "....WWBBBB......",
        "......BB........",
        "................",
        "................",
    },
};

static vec4_t layer_over(vec4_t dst, vec4_t src)
{
    float out_alpha = src.w + dst.w * (1.0f - src.w);
    vec3_t premul;

    if (out_alpha <= 0.000001f) {
        return vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    premul = v3_add(
        v3_mul1(vec3(src.x, src.y, src.z), src.w),
        v3_mul1(vec3(dst.x, dst.y, dst.z), dst.w * (1.0f - src.w)));

    return vec4(premul.x / out_alpha, premul.y / out_alpha, premul.z / out_alpha, out_alpha);
}

static vec4_t sprite_palette(char texel)
{
    switch (texel) {
        case 'W':
            return vec4(0.93f, 0.96f, 1.00f, 1.00f);

        case 'B':
            return vec4(0.18f, 0.72f, 1.00f, 1.00f);

        case 'R':
            return vec4(1.00f, 0.34f, 0.26f, 1.00f);

        case 'Y':
            return vec4(1.00f, 0.90f, 0.28f, 0.96f);

        case 'O':
            return vec4(1.00f, 0.52f, 0.10f, 0.82f);
    }

    return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

static vec4_t sample_sprite(int frame_index, int x, int y, bool flip_x)
{
    int sample_x = flip_x ? (SPRITE_SIZE - 1 - x) : x;
    int sample_y = SPRITE_SIZE - 1 - y;
    char texel;

    if (x < 0 || x >= SPRITE_SIZE || y < 0 || y >= SPRITE_SIZE) {
        return vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    texel = k_sprite_frames[frame_index][sample_y][sample_x];
    return sprite_palette(texel);
}

static float ellipse_alpha(vec2_t p, vec2_t center, vec2_t radius, float strength)
{
    vec2_t q = vec2((p.x - center.x) / radius.x, (p.y - center.y) / radius.y);
    float distance2 = q.x * q.x + q.y * q.y;
    return saturate((1.0f - distance2) * strength);
}

vec4_t animated_sprite_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const float tau = 2.0f * PI;
    const uint loop_frame = uniforms->frame % ANIMATED_SPRITE_LOOP_FRAMES;
    const float frame = (float)loop_frame;
    const int sprite_frame = ((int)(loop_frame / 4u)) % SPRITE_FRAME_COUNT;
    const float motion_phase = tau * (frame / (float)ANIMATED_SPRITE_LOOP_FRAMES);
    const float bob_phase = tau * (frame / 24.0f);
    const float base_x = uniforms->resolution.x * 0.5f + sinf(motion_phase) * 34.0f;
    const float base_y = uniforms->resolution.y * 0.5f + sinf(bob_phase) * 4.0f;
    const bool facing_left = cosf(motion_phase) < 0.0f;
    const float sprite_scale = 2.0f;
    const vec2_t sprite_min = vec2(
        floorf(base_x - (SPRITE_SIZE * sprite_scale) * 0.5f),
        floorf(base_y - (SPRITE_SIZE * sprite_scale) * 0.5f));
    const int local_x = (int)floorf((fragCoord.x - sprite_min.x) / sprite_scale);
    const int local_y = (int)floorf((fragCoord.y - sprite_min.y) / sprite_scale);
    vec4_t result = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec2_t p = vec2(fragCoord.x + 0.5f, fragCoord.y + 0.5f);
    vec2_t shadow_center = vec2(base_x, 10.0f);
    float shadow_alpha = ellipse_alpha(p, shadow_center, vec2(18.0f, 4.5f), 0.18f);
    vec4_t sprite_texel = sample_sprite(sprite_frame, local_x, local_y, facing_left);

    result = layer_over(result, vec4(0.03f, 0.02f, 0.05f, shadow_alpha));

    for (int i = 0; i < 3; i++) {
        float puff_phase = (float)((loop_frame + (uint)(i * 8)) % 24u) / 24.0f;
        float direction = facing_left ? 1.0f : -1.0f;
        vec2_t puff_center = vec2(
            base_x + direction * (12.0f + puff_phase * 18.0f),
            base_y + ((float)i - 1.0f) * 2.8f + sinf(tau * (frame / 18.0f) + (float)i) * 1.2f);
        float puff_alpha = ellipse_alpha(p, puff_center, vec2(4.0f + puff_phase * 3.0f, 2.0f + puff_phase), 0.42f * (1.0f - puff_phase));
        vec4_t puff = vec4(1.00f, 0.62f, 0.16f, puff_alpha);
        result = layer_over(result, puff);
    }

    result = layer_over(result, sprite_texel);
    return result;
}
