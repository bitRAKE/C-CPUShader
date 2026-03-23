// Adaptation of 3D Fire. From @XorDev's Shader Arsenal.
#include "three_fire.h"

vec4_t three_fire_main(vec2_t fragCoord, const shader_uniforms_t* uniforms) {
    vec2_t resolution = uniforms->resolution;
    float u_time = uniforms->time;

    vec4_t fragColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    float alphaAccum = 0.0f;  // Separate alpha accumulator for better control

    // Shader variables initialization
    float i = 0.0f;
    float z = 0.0f;
    float d = 0.0f;
    float j = 0.0f;

    // Ray marching loop (50 iterations)
    for (i = 0.0f; i < 50.0f; i++) {
        // Ray direction calculation
        vec3_t rd_raw = vec3(fragCoord.x * 2.0f - resolution.x,
            fragCoord.y * 2.0f - resolution.y,
            -resolution.y);
        vec3_t p = v3_mul1(v3_normalize(rd_raw), z);

        // Transformation and movement
        p.z += 5.0f + cosf(u_time);

        // Rotation matrix application
        float rot_val = u_time + p.y * 0.5f;
        float c = cosf(rot_val);
        float s = sinf(rot_val);
        float scale = fmaxf(p.y * 0.01f + 1.0f, 0.01f);

        float old_x = p.x;
        p.x = (old_x * c - p.z * s) / scale;
        p.z = (old_x * s + p.z * c) / scale;

        // Fractal/Noise displacement loop
        for (j = 2.0f; j < 15.0f; j /= 0.6f) {
            vec3_t p_swiz = vec3(p.y, p.z, p.x);
            vec3_t shift = vec3(u_time / 0.1f, 0.0f, 0.0f);

            vec3_t cos_input = v3_add1(v3_mul1(v3_sub(p_swiz, shift), j), u_time);
            vec3_t cos_res = vec3(cosf(cos_input.x), cosf(cos_input.y), cosf(cos_input.z));

            p = v3_add(p, v3_div1(cos_res, j));
        }

        // Distance field calculation and step
        float p_xz_len = sqrtf(p.x * p.x + p.z * p.z);
        d = 0.01f + fabsf(p_xz_len + p.y * 0.3f - 0.5f) / 7.0f;
        z += d;

        // Color accumulation
        float color_z = z / 3.0f;
        vec4_t color_offset = vec4(7.0f, 2.0f, 3.0f, 0.0f);
        vec4_t col_inc = vec4(
            (sinf(color_z + color_offset.x) + 1.1f) / d,
            (sinf(color_z + color_offset.y) + 1.1f) / d,
            (sinf(color_z + color_offset.z) + 1.1f) / d,
            0.0f  // Alpha will be calculated separately
        );

        // Dynamic alpha based on fire intensity and distance
        float intensity = (col_inc.x + col_inc.y + col_inc.z) / 3.0f;
        float dist_factor = 1.0f / (d * d);  // Squared inverse distance for sharper falloff

        // Fire is more opaque at the core, transparent at edges
        float alpha_contribution = intensity * dist_factor * 0.0002f;

        // Add temporal flickering effect
        float flicker = 0.7f + 0.3f * sinf(u_time * 8.0f + i * 0.5f + p.x * 2.0f);
        alpha_contribution *= flicker;

        // Height-based falloff (fire fades upward)
        float height_factor = fmaxf(0.0f, 1.0f - (p.y + 0.5f) * 0.3f);
        alpha_contribution *= height_factor;

        fragColor = v4_add(fragColor, col_inc);
        alphaAccum += alpha_contribution;
    }

    // Final Tone Mapping
    fragColor = v4_div1(fragColor, 1000.0f);

    // Apply tanh to RGB
    vec4_t result = vec4(
        tanhf(fragColor.x),
        tanhf(fragColor.y),
        tanhf(fragColor.z),
        0.0f  // Temporary
    );

    // Dynamic alpha processing
    float base_alpha = tanhf(alphaAccum);

    // Enhance alpha with fire luminance (brighter = more opaque)
    float luminance = 0.299f * result.x + 0.587f * result.y + 0.114f * result.z;
    float luminance_alpha = fminf(1.0f, luminance * 1.8f);

    // Combine both alpha sources
    float final_alpha = fminf(1.0f, base_alpha * 0.7f + luminance_alpha * 0.3f);

    // Add subtle pulsing effect at the edges
    float edge_pulse = 0.85f + 0.15f * sinf(u_time * 3.0f);
    final_alpha *= edge_pulse;

    // Ensure bright core is fully opaque, edges transparent
    final_alpha = fmaxf(0.1f, fminf(1.0f, final_alpha));

    result.w = final_alpha;

    return result;
}
