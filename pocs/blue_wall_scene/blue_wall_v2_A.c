#include "blue_wall_v2_A.h"

#include "blue_wall_v2_common.h"

static bw2_hit_t blue_wall_v2_A_intersect(const shader_uniforms_t *uniforms, vec3_t ro, vec3_t rd)
{
    bw2_hit_t hit = bw2_make_empty_hit();

    (void)uniforms;
    bw2_set_room_shell_hits(&hit, ro, rd);
    return hit;
}

vec4_t blue_wall_v2_A_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return bw2_render_shader(fragCoord, uniforms, blue_wall_v2_A_intersect, 0xA2A1u);
}
