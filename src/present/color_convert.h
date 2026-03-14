#pragma once

#include "../defines.h"

static inline float present_saturate(float value)
{
    if (value <= 0.0f) {
        return 0.0f;
    }
    if (value >= 1.0f) {
        return 1.0f;
    }

    return value;
}

static inline float present_max0(float value)
{
    return (value >= 0.0f) ? value : 0.0f;
}

static inline float present_linear_to_srgb_channel(float value)
{
    float clamped = present_saturate(value);

    if (clamped <= 0.0031308f) {
        return clamped * 12.92f;
    }

    return 1.055f * powf(clamped, 1.0f / 2.4f) - 0.055f;
}

static inline float present_srgb_to_linear_channel(float value)
{
    float clamped = present_saturate(value);

    if (clamped <= 0.04045f) {
        return clamped / 12.92f;
    }

    return powf((clamped + 0.055f) / 1.055f, 2.4f);
}

static inline float present_pq_decode_to_nits(float encoded)
{
    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 32.0f;
    const float c1 = 3424.0f / 4096.0f;
    const float c2 = 2413.0f / 128.0f;
    const float c3 = 2392.0f / 128.0f;
    float p = powf(present_saturate(encoded), 1.0f / m2);
    float numerator = present_max0(p - c1);
    float denominator = present_max0(c2 - c3 * p);

    if (denominator < 0.000001f) {
        denominator = 0.000001f;
    }

    return 10000.0f * powf(numerator / denominator, 1.0f / m1);
}

static inline vec3_t present_rec2020_to_rec709(vec3_t value)
{
    return vec3(
        value.x * 1.6605f + value.y * -0.5876f + value.z * -0.0728f,
        value.x * -0.1246f + value.y * 1.1329f + value.z * -0.0083f,
        value.x * -0.0182f + value.y * -0.1006f + value.z * 1.1187f);
}

static inline vec3_t present_decode_to_linear_rec709(shader_color_space_t color_space, vec3_t color)
{
    switch (color_space) {
        case SHADER_COLOR_SPACE_SDR_DISPLAY:
            return vec3(
                present_srgb_to_linear_channel(color.x),
                present_srgb_to_linear_channel(color.y),
                present_srgb_to_linear_channel(color.z));

        case SHADER_COLOR_SPACE_HDR10_ST2084: {
            vec3_t nits2020 = vec3(
                present_pq_decode_to_nits(color.x),
                present_pq_decode_to_nits(color.y),
                present_pq_decode_to_nits(color.z));
            vec3_t linear2020 = v3_mul1(nits2020, 1.0f / 80.0f);
            vec3_t linear709 = present_rec2020_to_rec709(linear2020);
            return vec3(
                present_max0(linear709.x),
                present_max0(linear709.y),
                present_max0(linear709.z));
        }

        case SHADER_COLOR_SPACE_SCENE_LINEAR:
        default:
            return vec3(
                present_max0(color.x),
                present_max0(color.y),
                present_max0(color.z));
    }
}

static inline vec3_t present_aces_tonemap(vec3_t color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    return vec3(
        present_saturate((color.x * (a * color.x + b)) / (color.x * (c * color.x + d) + e)),
        present_saturate((color.y * (a * color.y + b)) / (color.y * (c * color.y + d) + e)),
        present_saturate((color.z * (a * color.z + b)) / (color.z * (c * color.z + d) + e)));
}

static inline vec3_t present_encode_sdr_display(shader_color_space_t color_space, vec3_t color)
{
    vec3_t linear709;

    if (color_space == SHADER_COLOR_SPACE_SDR_DISPLAY) {
        return vec3(
            present_saturate(color.x),
            present_saturate(color.y),
            present_saturate(color.z));
    }

    linear709 = present_decode_to_linear_rec709(color_space, color);
    linear709 = present_aces_tonemap(linear709);
    return vec3(
        present_linear_to_srgb_channel(linear709.x),
        present_linear_to_srgb_channel(linear709.y),
        present_linear_to_srgb_channel(linear709.z));
}

static inline vec3_t present_encode_scrgb_linear(shader_color_space_t color_space, vec3_t color)
{
    return present_decode_to_linear_rec709(color_space, color);
}

static inline BYTE present_float_to_byte(float value)
{
    return (BYTE)(present_saturate(value) * 255.0f + 0.5f);
}
