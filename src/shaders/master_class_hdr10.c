#include "master_class_hdr10.h"
#include "master_class_hdr_common.h"

static vec3_t mcx_rec709_to_rec2020(vec3_t value)
{
    return vec3(
        value.x * 0.6274040f + value.y * 0.3292820f + value.z * 0.0433136f,
        value.x * 0.0690970f + value.y * 0.9195400f + value.z * 0.0113612f,
        value.x * 0.0163916f + value.y * 0.0880132f + value.z * 0.8955950f);
}

static float mcx_pq_encode_from_nits(float nits)
{
    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 32.0f;
    const float c1 = 3424.0f / 4096.0f;
    const float c2 = 2413.0f / 128.0f;
    const float c3 = 2392.0f / 128.0f;
    float normalized = saturate(nits / 10000.0f);
    float p = powf(normalized, m1);

    return powf((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

static vec3_t mcx_encode_hdr10(vec3_t linear709)
{
    vec3_t linear2020 = mcx_rec709_to_rec2020(vec3(
        fmaxf(linear709.x, 0.0f),
        fmaxf(linear709.y, 0.0f),
        fmaxf(linear709.z, 0.0f)));
    vec3_t nits = v3_mul1(linear2020, 80.0f);

    return vec3(
        mcx_pq_encode_from_nits(nits.x),
        mcx_pq_encode_from_nits(nits.y),
        mcx_pq_encode_from_nits(nits.z));
}

vec4_t master_class_hdr10_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    uint base_state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) * 9781u + 0x9E3779B9u;
    vec3_t accum = vec3_o(0.0f);
    const int sample_count = 4;

    for (int sample_index = 0; sample_index < sample_count; sample_index++) {
        uint state = base_state + (uint)(sample_index + 1) * 2246822519u;
        accum = v3_add(accum, mcx_render_linear_sample(fragCoord, resolution, &state));
    }

    accum = v3_div1(accum, (float)sample_count);
    accum = mcx_grade_hdr_linear(accum);

    {
        vec3_t hdr10 = mcx_encode_hdr10(accum);
        return vec4(hdr10.x, hdr10.y, hdr10.z, 1.0f);
    }
}
