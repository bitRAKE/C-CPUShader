#include "crystal_dodecahedron.h"

vec4_t crystal_dodecahedron_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t res = uniforms->resolution;
    const uint loop_frame = uniforms->frame % CRYSTAL_DODECAHEDRON_LOOP_FRAMES;
    const float time = 2.0f * PI * (float)loop_frame / (float)CRYSTAL_DODECAHEDRON_LOOP_FRAMES;

    float z = 0.0f;
    float d = 0.0f;
    vec4_t color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

    /* Ray direction: normalize(vec3(2*I - res.xy, -res.y))
       From GLSL: normalize(vec3(I+I, 0) - u_resolution.xyy) */
    vec3_t ray_dir = v3_normalize(vec3(
        2.0f * fragCoord.x - res.x,
        2.0f * fragCoord.y - res.y,
        -res.y));

    /* Time-varying rotation axis: normalize(cos(vec3(0,2,4) + time)) */
    vec3_t axis = v3_normalize(vec3(
        cosf(time),
        cosf(2.0f + time),
        cosf(4.0f + time)));

    for (int i = 1; i <= 100; i++) {
        /* p = z * ray_dir; p.z += 4.0 (camera offset) */
        vec3_t p = v3_mul1(ray_dir, z);
        p.z += 4.0f;

        /* Rotation: a = abs(axis * dot(axis, p) - cross(axis, p))
           Projects p onto the axis then reflects — creates symmetry planes */
        float dp = v3_dot(axis, p);
        vec3_t a = v3_abs(v3_sub(v3_mul1(axis, dp), v3_cross(axis, p)));

        /* Dodecahedral fold: a += 0.6 * a.yzx */
        vec3_t a_yzx = vec3(a.y, a.z, a.x);
        a = v3_add(a, v3_mul1(a_yzx, 0.6f));

        /* SDF: distance to dodecahedral shell */
        float max_a = fmaxf(fmaxf(a.x, a.y), a.z);
        d = 0.01f + 0.2f * fabsf(max_a - 2.0f);
        z += d;

        /* Iridescent color accumulation: cosine palette / distance */
        float fi = (float)i;
        float phase = fi * 0.2f;
        vec4_t palette = vec4(
            cosf(phase) + 1.0f,
            cosf(phase + 1.0f) + 1.0f,
            cosf(phase + 2.0f) + 1.0f,
            cosf(phase) + 1.0f);
        color = v4_add(color, v4_div1(palette, d));
    }

    /* Tonemap: tanh(color^2 / 3e7) */
    vec4_t sq = v4_mul(color, color);
    float mr = safe_tanhf(sq.x / 3e7f);
    float mg = safe_tanhf(sq.y / 3e7f);
    float mb = safe_tanhf(sq.z / 3e7f);

    /* Alpha: derived from peak RGB channel brightness.
       Crystal is bright, background is near-zero — gives clean separation.
       Slight boost ensures full opacity on the crystal body. */
    float alpha = saturate(fmaxf(fmaxf(mr, mg), mb) * 1.5f);

    return vec4(mr, mg, mb, alpha);
}
