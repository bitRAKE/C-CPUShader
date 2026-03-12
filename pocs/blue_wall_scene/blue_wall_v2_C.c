#include "blue_wall_v2_C.h"

#include "blue_wall_v2_common.h"

static void blue_wall_v2_C_commit_candidate(bw2_hit_t *hit, bw2_hit_t candidate)
{
    if (candidate.surface_type != BW2_SURFACE_NONE && candidate.distance < hit->distance) {
        *hit = candidate;
    }
}

static vec3_t blue_wall_v2_C_painting_albedo(vec3_t point, bw2_box_t bounds)
{
    float u = saturate((point.x - (bounds.center.x - bounds.half_extent.x)) / (bounds.half_extent.x * 2.0f));
    float v = saturate((point.y - (bounds.center.y - bounds.half_extent.y)) / (bounds.half_extent.y * 2.0f));
    vec3_t top = vec3(0.96f, 0.44f, 0.62f);
    vec3_t mid = vec3(0.98f, 0.63f, 0.32f);
    vec3_t low = vec3(0.92f, 0.78f, 0.38f);
    vec3_t color = (v > 0.50f) ? v3_lerp(mid, top, (v - 0.50f) / 0.50f) : v3_lerp(low, mid, v / 0.50f);
    float trunk = expf(-((u - 0.24f) * (u - 0.24f) * 120.0f));
    float branch = expf(-((u - 0.72f) * (u - 0.72f) * 140.0f));
    float bloom = expf(-((u - 0.55f) * (u - 0.55f) * 82.0f + (v - 0.58f) * (v - 0.58f) * 128.0f));

    if (v < 0.16f + 0.10f * sinf(u * 7.0f)) {
        color = vec3(0.20f, 0.14f, 0.10f);
    }

    color = v3_lerp(color, vec3(0.17f, 0.11f, 0.08f), saturate(trunk * 0.80f));
    color = v3_lerp(color, vec3(0.16f, 0.10f, 0.08f), saturate(branch * 0.50f));
    color = v3_add(color, v3_mul1(vec3(1.0f, 0.92f, 0.94f), bloom * 0.82f));
    return color;
}

static void blue_wall_v2_C_try_painting(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    const blue_wall_generated_group_t *group = bw2_find_group("painting");
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    float frame;
    float front;
    vec3_t wood = vec3(0.24f, 0.13f, 0.08f);
    vec3_t point;

    if (group == NULL) {
        return;
    }

    bounds = bw2_group_box(group);
    frame = fminf(bounds.half_extent.x, bounds.half_extent.y) * 0.12f;
    front = bounds.center.z - bounds.half_extent.z;

    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - bounds.half_extent.x, bounds.center.y - bounds.half_extent.y, front), vec3(bounds.center.x + bounds.half_extent.x, bounds.center.y - bounds.half_extent.y + frame, bounds.center.z + bounds.half_extent.z), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - bounds.half_extent.x, bounds.center.y + bounds.half_extent.y - frame, front), vec3(bounds.center.x + bounds.half_extent.x, bounds.center.y + bounds.half_extent.y, bounds.center.z + bounds.half_extent.z), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - bounds.half_extent.x, bounds.center.y - bounds.half_extent.y + frame, front), vec3(bounds.center.x - bounds.half_extent.x + frame, bounds.center.y + bounds.half_extent.y - frame, bounds.center.z + bounds.half_extent.z), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x + bounds.half_extent.x - frame, bounds.center.y - bounds.half_extent.y + frame, front), vec3(bounds.center.x + bounds.half_extent.x, bounds.center.y + bounds.half_extent.y - frame, bounds.center.z + bounds.half_extent.z), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - bounds.half_extent.x + frame, bounds.center.y - bounds.half_extent.y + frame, front), vec3(bounds.center.x + bounds.half_extent.x - frame, bounds.center.y + bounds.half_extent.y - frame, front + bounds.half_extent.z * 0.40f), BW2_SURFACE_DIFFUSE, vec3(0.82f, 0.60f, 0.34f), vec3_o(0.0f), 0.0f);

    if (candidate.surface_type != BW2_SURFACE_NONE && candidate.distance < 9999.0f) {
        point = v3_add(ro, v3_mul1(rd, candidate.distance));
        if (fabsf(candidate.normal.z + 1.0f) < 0.001f) {
            candidate.albedo = blue_wall_v2_C_painting_albedo(point, (bw2_box_t){ vec3(bounds.center.x, bounds.center.y, front), vec3(bounds.half_extent.x - frame, bounds.half_extent.y - frame, bounds.half_extent.z * 0.20f) });
        }
    }

    blue_wall_v2_C_commit_candidate(hit, candidate);
}

static void blue_wall_v2_C_try_bookshelf(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const char *group_id)
{
    const blue_wall_generated_group_t *group = bw2_find_group(group_id);
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    float frame = 0.05f;
    float shelf_depth;
    vec3_t wood = vec3(0.23f, 0.14f, 0.08f);
    vec3_t book_a = vec3(0.78f, 0.75f, 0.68f);
    vec3_t book_b = vec3(0.24f, 0.42f, 0.83f);
    vec3_t book_c = vec3(0.76f, 0.65f, 0.28f);
    float left;
    float right;
    float bottom;
    float top;
    float front;
    float back;
    float shelf0;
    float shelf1;
    float shelf2;

    if (group == NULL) {
        return;
    }

    bounds = bw2_group_box(group);
    shelf_depth = bounds.half_extent.z * 0.55f;
    left = bounds.center.x - bounds.half_extent.x;
    right = bounds.center.x + bounds.half_extent.x;
    bottom = bounds.center.y - bounds.half_extent.y;
    top = bounds.center.y + bounds.half_extent.y;
    front = bounds.center.z - bounds.half_extent.z;
    back = bounds.center.z + bounds.half_extent.z;
    shelf0 = bottom + 0.22f;
    shelf1 = bottom + bounds.half_extent.y * 0.25f;
    shelf2 = bottom + bounds.half_extent.y * 0.85f;

    bw2_set_box_hit(&candidate, ro, rd, vec3(left, bottom, front), vec3(left + frame, top, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(right - frame, bottom, front), vec3(right, top, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left, bottom, back - frame), vec3(right, top, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left, bottom, front), vec3(right, bottom + frame, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left, top - frame, front), vec3(right, top, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);

    bw2_set_box_hit(&candidate, ro, rd, vec3(left + frame, shelf0, front), vec3(right - frame, shelf0 + frame, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + frame, shelf1, front), vec3(right - frame, shelf1 + frame, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + frame, shelf2, front), vec3(right - frame, shelf2 + frame, back), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);

    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.08f, bottom + 0.04f, front), vec3(left + 0.18f, shelf0, front + shelf_depth), BW2_SURFACE_DIFFUSE, book_a, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.20f, bottom + 0.04f, front), vec3(left + 0.32f, shelf0 + 0.06f, front + shelf_depth), BW2_SURFACE_DIFFUSE, book_b, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.34f, bottom + 0.04f, front), vec3(left + 0.44f, shelf0 + 0.03f, front + shelf_depth), BW2_SURFACE_DIFFUSE, book_c, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.10f, shelf1 + frame, front), vec3(left + 0.22f, shelf2, front + shelf_depth), BW2_SURFACE_DIFFUSE, book_b, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.24f, shelf1 + frame, front), vec3(left + 0.36f, shelf2 - 0.02f, front + shelf_depth), BW2_SURFACE_DIFFUSE, book_c, vec3_o(0.0f), 0.0f);

    blue_wall_v2_C_commit_candidate(hit, candidate);
}

static void blue_wall_v2_C_try_sideboard(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    const blue_wall_generated_group_t *group = bw2_find_group("sideboard");
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    float left;
    float right;
    float bottom;
    float top;
    float front;
    float back;
    vec3_t wood_dark = vec3(0.35f, 0.22f, 0.13f);
    vec3_t wood_light = vec3(0.54f, 0.34f, 0.20f);
    vec3_t metal = vec3(0.80f, 0.78f, 0.75f);

    if (group == NULL) {
        return;
    }

    bounds = bw2_group_box(group);
    left = bounds.center.x - bounds.half_extent.x;
    right = bounds.center.x + bounds.half_extent.x;
    bottom = bounds.center.y - bounds.half_extent.y;
    top = bounds.center.y + bounds.half_extent.y;
    front = bounds.center.z - bounds.half_extent.z;
    back = bounds.center.z + bounds.half_extent.z;

    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.04f, bottom, front + 0.02f), vec3(right - 0.04f, top - 0.10f, back), BW2_SURFACE_DIFFUSE, wood_dark, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left, top - 0.10f, front), vec3(right, top, back), BW2_SURFACE_DIFFUSE, wood_light, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.56f, bottom + 0.14f, front), vec3(left + 0.60f, top - 0.12f, front + 0.04f), BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.03f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(right - 0.60f, bottom + 0.14f, front), vec3(right - 0.56f, top - 0.12f, front + 0.04f), BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.03f);

    blue_wall_v2_C_commit_candidate(hit, candidate);
}

static void blue_wall_v2_C_try_chair(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    const blue_wall_generated_group_t *group = bw2_find_group("chair");
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    float left;
    float right;
    float bottom;
    float top;
    float front;
    float back;
    vec3_t fabric = vec3(0.32f, 0.40f, 0.31f);
    vec3_t wood = vec3(0.25f, 0.17f, 0.12f);

    if (group == NULL) {
        return;
    }

    bounds = bw2_group_box(group);
    left = bounds.center.x - bounds.half_extent.x;
    right = bounds.center.x + bounds.half_extent.x;
    bottom = bounds.center.y - bounds.half_extent.y;
    top = bounds.center.y + bounds.half_extent.y;
    front = bounds.center.z - bounds.half_extent.z;
    back = bounds.center.z + bounds.half_extent.z;

    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.18f, bottom + 0.24f, front + 0.16f), vec3(right - 0.18f, bottom + 0.40f, back - 0.14f), BW2_SURFACE_DIFFUSE, fabric, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.22f, bottom + 0.40f, back - 0.10f), vec3(right - 0.18f, top - 0.06f, back - 0.02f), BW2_SURFACE_DIFFUSE, fabric, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(left + 0.16f, bottom, front + 0.18f), 0.028f, bottom, bottom + 0.34f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(right - 0.16f, bottom, front + 0.18f), 0.028f, bottom, bottom + 0.34f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(left + 0.20f, bottom, back - 0.18f), 0.028f, bottom, bottom + 0.34f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(right - 0.20f, bottom, back - 0.18f), 0.028f, bottom, bottom + 0.34f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);

    blue_wall_v2_C_commit_candidate(hit, candidate);
}

static void blue_wall_v2_C_try_table(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const char *group_id, vec3_t top_color, vec3_t base_color)
{
    const blue_wall_generated_group_t *group = bw2_find_group(group_id);
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    float bottom;
    float top;
    float top_radius;
    float stem_radius;
    float base_radius;

    if (group == NULL) {
        return;
    }

    bounds = bw2_group_box(group);
    bottom = bounds.center.y - bounds.half_extent.y;
    top = bounds.center.y + bounds.half_extent.y;
    top_radius = fminf(bounds.half_extent.x, bounds.half_extent.z) * 0.96f;
    stem_radius = top_radius * 0.18f;
    base_radius = top_radius * 0.46f;

    bw2_set_cylinder_y_hit(&candidate, ro, rd, bounds.center, top_radius, top - 0.045f, top, BW2_SURFACE_DIFFUSE, top_color, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, bounds.center, stem_radius, bottom + 0.05f, top - 0.045f, BW2_SURFACE_METAL, base_color, vec3_o(0.0f), 0.04f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, bounds.center, base_radius, bottom, bottom + 0.045f, BW2_SURFACE_METAL, base_color, vec3_o(0.0f), 0.04f);

    blue_wall_v2_C_commit_candidate(hit, candidate);
}

static void blue_wall_v2_C_try_ukulele(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    const blue_wall_generated_group_t *group = bw2_find_group("ukulele");
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    vec3_t wood = vec3(0.56f, 0.37f, 0.23f);
    vec3_t dark = vec3(0.22f, 0.12f, 0.06f);
    vec3_t center;

    if (group == NULL) {
        return;
    }

    bounds = bw2_group_box(group);
    center = bounds.center;

    bw2_set_sphere_hit(&candidate, ro, rd, v3_add(center, vec3(0.0f, -0.10f, 0.0f)), bounds.half_extent.y * 0.95f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_sphere_hit(&candidate, ro, rd, v3_add(center, vec3(0.0f, 0.04f, 0.0f)), bounds.half_extent.y * 0.72f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(center.x - bounds.half_extent.x * 0.28f, center.y + 0.08f, center.z - bounds.half_extent.z * 0.25f), vec3(center.x + bounds.half_extent.x * 0.28f, center.y + bounds.half_extent.y * 1.85f, center.z + bounds.half_extent.z * 0.25f), BW2_SURFACE_DIFFUSE, dark, vec3_o(0.0f), 0.0f);

    blue_wall_v2_C_commit_candidate(hit, candidate);
}

static void blue_wall_v2_C_try_floor_lamp(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    bw2_set_floor_lamp_proxy_hits(hit, ro, rd, vec3(0.76f, 0.74f, 0.72f), vec3(0.86f, 0.81f, 0.73f), vec3(1.0f, 0.93f, 0.80f), vec3(4.0f, 2.8f, 1.5f), 0.05f);
}

static bw2_hit_t blue_wall_v2_C_intersect(const shader_uniforms_t *uniforms, vec3_t ro, vec3_t rd)
{
    bw2_hit_t hit = bw2_make_empty_hit();

    (void)uniforms;
    bw2_set_room_shell_hits(&hit, ro, rd);
    blue_wall_v2_C_try_floor_lamp(&hit, ro, rd);
    blue_wall_v2_C_try_painting(&hit, ro, rd);
    blue_wall_v2_C_try_bookshelf(&hit, ro, rd, "bookshelf_left");
    blue_wall_v2_C_try_bookshelf(&hit, ro, rd, "bookshelf_right");
    blue_wall_v2_C_try_sideboard(&hit, ro, rd);
    blue_wall_v2_C_try_chair(&hit, ro, rd);
    blue_wall_v2_C_try_table(&hit, ro, rd, "table_tall", vec3(0.33f, 0.42f, 0.60f), vec3(0.28f, 0.30f, 0.34f));
    blue_wall_v2_C_try_table(&hit, ro, rd, "table_small", vec3(0.36f, 0.22f, 0.14f), vec3(0.26f, 0.17f, 0.11f));
    blue_wall_v2_C_try_ukulele(&hit, ro, rd);
    return hit;
}

vec4_t blue_wall_v2_C_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return bw2_render_shader(fragCoord, uniforms, blue_wall_v2_C_intersect, 0xC2C3u);
}
