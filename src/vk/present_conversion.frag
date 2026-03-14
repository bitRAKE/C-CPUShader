#version 450

layout(binding = 0) uniform sampler2D u_image;

layout(push_constant) uniform PresentParams {
    int image_width;
    int image_height;
    int shader_color_space;
    int surface_kind;
} u_params;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

float saturate1(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 linear_to_srgb(vec3 value)
{
    vec3 clamped = clamp(value, vec3(0.0), vec3(1.0));
    vec3 lower = clamped * 12.92;
    vec3 upper = 1.055 * pow(clamped, vec3(1.0 / 2.4)) - 0.055;
    return mix(upper, lower, step(clamped, vec3(0.0031308)));
}

vec3 srgb_to_linear(vec3 value)
{
    vec3 clamped = clamp(value, vec3(0.0), vec3(1.0));
    vec3 lower = clamped / 12.92;
    vec3 upper = pow((clamped + 0.055) / 1.055, vec3(2.4));
    return mix(upper, lower, step(clamped, vec3(0.04045)));
}

vec3 rec709_to_rec2020(vec3 value)
{
    return vec3(
        dot(value, vec3(0.6274040, 0.3292820, 0.0433136)),
        dot(value, vec3(0.0690970, 0.9195400, 0.0113612)),
        dot(value, vec3(0.0163916, 0.0880132, 0.8955950)));
}

vec3 rec2020_to_rec709(vec3 value)
{
    return vec3(
        dot(value, vec3(1.6605, -0.5876, -0.0728)),
        dot(value, vec3(-0.1246, 1.1329, -0.0083)),
        dot(value, vec3(-0.0182, -0.1006, 1.1187)));
}

float pq_encode_from_nits(float nits)
{
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;
    float normalized = saturate1(nits / 10000.0);
    float p = pow(normalized, m1);
    return pow((c1 + c2 * p) / (1.0 + c3 * p), m2);
}

float pq_decode_to_nits(float encoded)
{
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;
    float p = pow(saturate1(encoded), 1.0 / m2);
    float numerator = max(p - c1, 0.0);
    float denominator = max(c2 - c3 * p, 0.000001);
    return 10000.0 * pow(numerator / denominator, 1.0 / m1);
}

vec3 aces_tonemap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), vec3(0.0), vec3(1.0));
}

vec3 decode_to_linear_rec709(vec3 color)
{
    if (u_params.shader_color_space == 0) {
        return srgb_to_linear(color);
    }

    if (u_params.shader_color_space == 2) {
        vec3 nits2020 = vec3(
            pq_decode_to_nits(color.r),
            pq_decode_to_nits(color.g),
            pq_decode_to_nits(color.b));
        vec3 linear2020 = nits2020 / 80.0;
        return max(rec2020_to_rec709(linear2020), vec3(0.0));
    }

    return max(color, vec3(0.0));
}

vec3 encode_for_surface(vec3 color)
{
    vec3 linear709 = decode_to_linear_rec709(color);

    if (u_params.surface_kind == 1) {
        return linear709;
    }

    if (u_params.surface_kind == 2) {
        if (u_params.shader_color_space == 2) {
            return clamp(color, vec3(0.0), vec3(1.0));
        }

        vec3 rec2020 = max(rec709_to_rec2020(linear709), vec3(0.0));
        vec3 nits = rec2020 * 80.0;
        return vec3(
            pq_encode_from_nits(nits.r),
            pq_encode_from_nits(nits.g),
            pq_encode_from_nits(nits.b));
    }

    if (u_params.shader_color_space == 0) {
        return clamp(color, vec3(0.0), vec3(1.0));
    }

    return linear_to_srgb(aces_tonemap(linear709));
}

void main(void)
{
    vec2 uv = clamp(v_uv, vec2(0.0), vec2(1.0));
    int x = min(int(floor(uv.x * float(u_params.image_width))), u_params.image_width - 1);
    int y = min(int(floor(uv.y * float(u_params.image_height))), u_params.image_height - 1);
    int flipped_y = (u_params.image_height - 1) - y;
    vec4 sampled_color = texelFetch(u_image, ivec2(x, flipped_y), 0);
    out_color = vec4(encode_for_surface(sampled_color.rgb), sampled_color.a);
}
