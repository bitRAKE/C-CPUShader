#include "master_class_scrgb.h"
#include "master_class_hdr_common.h"

vec4_t master_class_scrgb_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    uint state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) + uniforms->frame * 78423u;
    vec3_t color = mcx_render_linear_sample(fragCoord, resolution, &state);

    color = mcx_grade_hdr_linear(color);
    return vec4(color.x, color.y, color.z, 1.0f);
}
