#include "blue_wall_v2_B.h"

#include "blue_wall_v2_common.h"

static void blue_wall_v2_B_set_layout_hits(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    bw2_set_floor_lamp_proxy_hits(hit, ro, rd, vec3(0.92f, 0.74f, 0.28f), vec3(0.94f, 0.82f, 0.42f), vec3_o(0.0f), vec3_o(0.0f), 0.02f);

    for (int index = 0; index < BLUE_WALL_GENERATED_OBJECT_COUNT; index++) {
        const blue_wall_generated_object_t *object = &g_blue_wall_generated_objects[index];
        bw2_obb_t bounds;
        vec3_t tint;

        if (!bw2_is_layout_object(object)) {
            continue;
        }
        if (strcmp(object->group_id, "floor_lamp") == 0) {
            continue;
        }

        bounds = bw2_object_obb(object);
        tint = bw2_role_tint(object);
        bw2_set_obb_hit(hit, ro, rd, bounds, BW2_SURFACE_DIFFUSE, tint, vec3_o(0.0f), 0.0f);
    }
}

static bw2_hit_t blue_wall_v2_B_intersect(const shader_uniforms_t *uniforms, vec3_t ro, vec3_t rd)
{
    bw2_hit_t hit = bw2_make_empty_hit();

    (void)uniforms;
    bw2_set_room_shell_hits(&hit, ro, rd);
    blue_wall_v2_B_set_layout_hits(&hit, ro, rd);
    return hit;
}

vec4_t blue_wall_v2_B_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return bw2_render_shader(fragCoord, uniforms, blue_wall_v2_B_intersect, 0xB2B2u);
}
