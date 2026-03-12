#include "blue_wall_v2_D.h"

#include "blue_wall_v2_common.h"
#include "../../src/u_texture.h"

static void blue_wall_v2_D_commit_candidate(bw2_hit_t *hit, bw2_hit_t candidate)
{
    if (candidate.surface_type != BW2_SURFACE_NONE && candidate.distance < hit->distance) {
        *hit = candidate;
    }
}

static bool blue_wall_v2_D_hit_group_volume(const char *group_id, vec3_t ro, vec3_t rd)
{
    const blue_wall_generated_group_t *group = bw2_find_group(group_id);
    bw2_box_t bounds;

    if (group == NULL) {
        return false;
    }

    bounds = bw2_group_box(group);
    return bw2_ray_box(ro, rd, v3_sub(bounds.center, bounds.half_extent), v3_add(bounds.center, bounds.half_extent), NULL) > 0.0f;
}

static bool blue_wall_v2_D_hit_object_volume(const blue_wall_generated_object_t *object, vec3_t ro, vec3_t rd)
{
    if (object == NULL) {
        return false;
    }

    return bw2_ray_obb(ro, rd, bw2_object_obb(object), NULL) > 0.0f;
}

static vec3_t blue_wall_v2_D_wood_tone(vec3_t point, vec3_t dark, vec3_t light)
{
    float grain = 0.5f + 0.5f * sinf(point.x * 11.0f + point.y * 5.0f + point.z * 17.0f);
    return v3_lerp(dark, light, 0.30f + 0.55f * grain);
}

static vec3_t blue_wall_v2_D_painting_albedo(vec3_t local_point, float half_width, float half_height, const shader_uniforms_t *uniforms)
{
    float u = saturate((local_point.x + half_width) / (half_width * 2.0f));
    float v = saturate((local_point.z + half_height) / (half_height * 2.0f));
    const shader_buffer_t *painting_texture = u_buffer_named(uniforms, "painting");
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

    if (u_texture_valid(painting_texture)) {
        vec4_t texel = u_texture_sample_bilinear(painting_texture, vec2(u, 1.0f - v), U_TEXTURE_ADDRESS_CLAMP);
        color = vec3(texel.x, texel.y, texel.z);
    }

    return color;
}

static float blue_wall_v2_D_front_sign_y(bw2_obb_t bounds)
{
    vec3_t camera = bw2_from_blender_xyz(g_blue_wall_camera.location);
    vec3_t to_camera = v3_normalize(v3_sub(camera, bounds.center));

    return (v3_dot(to_camera, bounds.axis_y) >= 0.0f) ? 1.0f : -1.0f;
}

static void blue_wall_v2_D_try_painting(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const shader_uniforms_t *uniforms)
{
    const blue_wall_generated_object_t *object = bw2_find_object("CanvasPainting_01");
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_obb_t bounds;
    float frame;
    float front_sign;
    float front_y0;
    float front_y1;
    float front_depth;
    float inner_half_width;
    float inner_half_height;
    vec3_t wood = vec3(0.24f, 0.13f, 0.08f);
    vec3_t point;
    vec3_t local_point;
    vec3_t front_normal;

    if (object == NULL) {
        return;
    }

    bounds = bw2_object_obb(object);
    frame = fminf(bounds.half_extent.x, bounds.half_extent.z) * 0.12f;
    front_sign = blue_wall_v2_D_front_sign_y(bounds);
    front_depth = fmaxf(bounds.half_extent.y * 0.40f, 0.004f);
    front_y0 = (front_sign > 0.0f) ? (bounds.half_extent.y - front_depth) : (-bounds.half_extent.y);
    front_y1 = (front_sign > 0.0f) ? bounds.half_extent.y : (-bounds.half_extent.y + front_depth);
    inner_half_width = fmaxf(bounds.half_extent.x - frame, 0.001f);
    inner_half_height = fmaxf(bounds.half_extent.z - frame, 0.001f);
    front_normal = v3_mul1(bounds.axis_y, front_sign);

    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-bounds.half_extent.x, front_y0, -bounds.half_extent.z), vec3(bounds.half_extent.x, front_y1, -bounds.half_extent.z + frame), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-bounds.half_extent.x, front_y0, bounds.half_extent.z - frame), vec3(bounds.half_extent.x, front_y1, bounds.half_extent.z), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-bounds.half_extent.x, front_y0, -bounds.half_extent.z + frame), vec3(-bounds.half_extent.x + frame, front_y1, bounds.half_extent.z - frame), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(bounds.half_extent.x - frame, front_y0, -bounds.half_extent.z + frame), vec3(bounds.half_extent.x, front_y1, bounds.half_extent.z - frame), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-inner_half_width, front_y0, -inner_half_height), vec3(inner_half_width, front_y1, inner_half_height), BW2_SURFACE_DIFFUSE, vec3(0.82f, 0.60f, 0.34f), vec3_o(0.0f), 0.0f);

    if (candidate.surface_type != BW2_SURFACE_NONE && candidate.distance < 9999.0f) {
        point = v3_add(ro, v3_mul1(rd, candidate.distance));
        if (v3_dot(candidate.normal, front_normal) > 0.999f) {
            local_point = bw2_obb_world_to_local_point(bounds, point);
            if (fabsf(local_point.x) < inner_half_width - 0.001f && fabsf(local_point.z) < inner_half_height - 0.001f) {
                candidate.albedo = blue_wall_v2_D_painting_albedo(local_point, inner_half_width, inner_half_height, uniforms);
            }
        }
    }

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_bookshelf(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const char *group_id)
{
    const blue_wall_generated_group_t *group = bw2_find_group(group_id);
    bw2_hit_t candidate = bw2_make_empty_hit();
    bw2_box_t bounds;
    float frame = 0.05f;
    float shelf_depth;
    vec3_t wood = vec3(0.22f, 0.13f, 0.08f);
    vec3_t book_a = vec3(0.78f, 0.75f, 0.68f);
    vec3_t book_b = vec3(0.22f, 0.40f, 0.78f);
    vec3_t book_c = vec3(0.76f, 0.63f, 0.25f);
    vec3_t book_d = vec3(0.62f, 0.17f, 0.21f);
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
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.38f, shelf1 + frame, front), vec3(left + 0.50f, shelf2 + 0.05f, front + shelf_depth), BW2_SURFACE_DIFFUSE, book_d, vec3_o(0.0f), 0.0f);

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_sideboard(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
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
    vec3_t wood_dark = vec3(0.33f, 0.21f, 0.12f);
    vec3_t wood_light = vec3(0.53f, 0.33f, 0.19f);
    vec3_t metal = vec3(0.80f, 0.78f, 0.75f);
    vec3_t point;

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
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - 0.10f, bottom + 0.18f, front), vec3(bounds.center.x + 0.10f, bottom + 0.22f, front + 0.04f), BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.03f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - 0.10f, bottom + 0.42f, front), vec3(bounds.center.x + 0.10f, bottom + 0.46f, front + 0.04f), BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.03f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(bounds.center.x - 0.10f, bottom + 0.66f, front), vec3(bounds.center.x + 0.10f, bottom + 0.70f, front + 0.04f), BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.03f);

    if (candidate.surface_type != BW2_SURFACE_NONE) {
        point = v3_add(ro, v3_mul1(rd, candidate.distance));
        if (candidate.surface_type == BW2_SURFACE_DIFFUSE) {
            candidate.albedo = blue_wall_v2_D_wood_tone(point, wood_dark, wood_light);
        }
    }

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_chair(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
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
    vec3_t fabric = vec3(0.31f, 0.36f, 0.31f);
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

    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.12f, bottom + 0.24f, front + 0.12f), vec3(right - 0.12f, bottom + 0.38f, back - 0.12f), BW2_SURFACE_DIFFUSE, fabric, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.22f, bottom + 0.38f, back - 0.10f), vec3(right - 0.22f, top - 0.06f, back - 0.02f), BW2_SURFACE_DIFFUSE, fabric, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(left + 0.02f, bottom + 0.34f, back - 0.12f), vec3(left + 0.12f, bottom + 0.52f, back - 0.04f), BW2_SURFACE_DIFFUSE, fabric, vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(&candidate, ro, rd, vec3(right - 0.12f, bottom + 0.34f, back - 0.12f), vec3(right - 0.02f, bottom + 0.52f, back - 0.04f), BW2_SURFACE_DIFFUSE, fabric, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(left + 0.10f, bottom, front + 0.12f), 0.026f, bottom, bottom + 0.32f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(right - 0.10f, bottom, front + 0.12f), 0.026f, bottom, bottom + 0.32f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(left + 0.14f, bottom, back - 0.12f), 0.026f, bottom, bottom + 0.32f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_cylinder_y_hit(&candidate, ro, rd, vec3(right - 0.14f, bottom, back - 0.12f), 0.026f, bottom, bottom + 0.32f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_table(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const char *group_id, vec3_t top_color, vec3_t base_color)
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

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_ukulele(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
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

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_lantern(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const blue_wall_generated_object_t *object)
{
    bw2_obb_t bounds = bw2_object_obb(object);
    bw2_hit_t candidate = bw2_make_empty_hit();
    float ex = bounds.half_extent.x;
    float ey = bounds.half_extent.y;
    float ez = bounds.half_extent.z;
    float frame = fminf(ex, ey) * 0.18f;
    vec3_t brass = vec3(0.40f, 0.29f, 0.16f);
    vec3_t glow = vec3(4.0f, 3.0f, 1.9f);
    vec3_t glass = vec3(0.96f, 0.88f, 0.72f);

    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex, -ey, -ez), vec3(ex, ey, -ez + frame), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.06f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex, -ey, ez - frame), vec3(ex, ey, ez), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.06f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex, -ey, -ez + frame), vec3(-ex + frame, -ey + frame, ez - frame), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.06f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(ex - frame, -ey, -ez + frame), vec3(ex, -ey + frame, ez - frame), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.06f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex, ey - frame, -ez + frame), vec3(-ex + frame, ey, ez - frame), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.06f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(ex - frame, ey - frame, -ez + frame), vec3(ex, ey, ez - frame), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.06f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex * 0.68f, -ey * 0.68f, -ez * 0.25f), vec3(ex * 0.68f, ey * 0.68f, ez * 0.18f), BW2_SURFACE_LIGHT, glass, glow, 0.0f);

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_clock(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const blue_wall_generated_object_t *object)
{
    bw2_obb_t bounds = bw2_object_obb(object);
    bw2_hit_t candidate = bw2_make_empty_hit();
    float ex = bounds.half_extent.x;
    float ey = bounds.half_extent.y;
    float ez = bounds.half_extent.z;
    float front_sign = blue_wall_v2_D_front_sign_y(bounds);
    float face_y0 = (front_sign > 0.0f) ? ey - 0.010f : -ey;
    float face_y1 = (front_sign > 0.0f) ? ey : -ey + 0.010f;
    vec3_t wood = vec3(0.21f, 0.13f, 0.09f);
    vec3_t face = vec3(0.85f, 0.80f, 0.70f);
    vec3_t brass = vec3(0.48f, 0.36f, 0.19f);

    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex * 0.86f, -ey * 0.90f, -ez), vec3(ex * 0.86f, ey * 0.90f, ez * 0.30f), BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_sphere_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.0f, 0.0f, ez * 0.34f)), fminf(ex, ez) * 0.56f, BW2_SURFACE_DIFFUSE, wood, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex * 0.45f, face_y0, -ez * 0.18f), vec3(ex * 0.45f, face_y1, ez * 0.32f), BW2_SURFACE_DIFFUSE, face, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-0.010f, face_y0, -0.010f), vec3(0.010f, face_y1 + front_sign * 0.001f, ez * 0.20f), BW2_SURFACE_METAL, brass, vec3_o(0.0f), 0.03f);

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_plant(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const blue_wall_generated_object_t *object)
{
    bw2_obb_t bounds = bw2_object_obb(object);
    bw2_hit_t candidate = bw2_make_empty_hit();
    float ex = bounds.half_extent.x;
    float ey = bounds.half_extent.y;
    float ez = bounds.half_extent.z;
    float pot_half = ez * 0.28f;
    float pot_radius = fminf(ex, ey) * 0.82f;
    vec3_t pot = vec3(0.39f, 0.30f, 0.20f);
    vec3_t leaf = vec3(0.24f, 0.46f, 0.22f);

    bw2_set_cylinder_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.0f, 0.0f, -ez + pot_half)), bounds.axis_z, pot_radius, pot_half, BW2_SURFACE_DIFFUSE, pot, vec3_o(0.0f), 0.0f);
    bw2_set_sphere_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(-0.03f, 0.00f, ez * 0.10f)), ez * 0.42f, BW2_SURFACE_DIFFUSE, leaf, vec3_o(0.0f), 0.0f);
    bw2_set_sphere_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.05f, 0.02f, ez * 0.20f)), ez * 0.36f, BW2_SURFACE_DIFFUSE, leaf, vec3_o(0.0f), 0.0f);
    bw2_set_sphere_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.00f, -0.03f, ez * 0.36f)), ez * 0.30f, BW2_SURFACE_DIFFUSE, leaf, vec3_o(0.0f), 0.0f);

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_horse(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const blue_wall_generated_object_t *object)
{
    bw2_obb_t bounds = bw2_object_obb(object);
    bw2_hit_t candidate = bw2_make_empty_hit();
    float ex = bounds.half_extent.x;
    float ey = bounds.half_extent.y;
    float ez = bounds.half_extent.z;
    vec3_t stone = vec3(0.62f, 0.61f, 0.66f);

    bw2_set_sphere_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.0f, 0.0f, -ez * 0.05f)), fminf(ex, ez) * 0.60f, BW2_SURFACE_DIFFUSE, stone, vec3_o(0.0f), 0.0f);
    bw2_set_sphere_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(ex * 0.08f, 0.0f, ez * 0.50f)), fminf(ex, ez) * 0.28f, BW2_SURFACE_DIFFUSE, stone, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex * 0.35f, -ey * 0.28f, -ez), vec3(ex * 0.35f, ey * 0.28f, -ez * 0.25f), BW2_SURFACE_DIFFUSE, stone, vec3_o(0.0f), 0.0f);

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_camera(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const blue_wall_generated_object_t *body_object, const blue_wall_generated_object_t *strap_object)
{
    bw2_obb_t bounds = bw2_object_obb(body_object);
    bw2_hit_t candidate = bw2_make_empty_hit();
    float ex = bounds.half_extent.x;
    float ey = bounds.half_extent.y;
    float ez = bounds.half_extent.z;
    vec3_t body = vec3(0.16f, 0.16f, 0.17f);
    vec3_t metal = vec3(0.36f, 0.37f, 0.40f);
    vec3_t glass = vec3(0.07f, 0.08f, 0.10f);

    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex * 0.92f, -ey * 0.75f, -ez * 0.88f), vec3(ex * 0.92f, ey * 0.72f, ez * 0.88f), BW2_SURFACE_DIFFUSE, body, vec3_o(0.0f), 0.0f);
    bw2_set_local_box_hit(&candidate, ro, rd, bounds, vec3(-ex * 0.22f, -ey * 0.10f, ez * 0.15f), vec3(ex * 0.20f, ey * 0.42f, ez * 0.72f), BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.03f);
    bw2_set_cylinder_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.0f, -ey * 0.82f, 0.0f)), bounds.axis_y, fminf(ex, ez) * 0.32f, ey * 0.24f, BW2_SURFACE_METAL, metal, vec3_o(0.0f), 0.04f);
    bw2_set_cylinder_hit(&candidate, ro, rd, bw2_obb_local_to_world_point(bounds, vec3(0.0f, -ey * 1.05f, 0.0f)), bounds.axis_y, fminf(ex, ez) * 0.22f, ey * 0.08f, BW2_SURFACE_DIFFUSE, glass, vec3_o(0.0f), 0.0f);

    if (strap_object != NULL) {
        bw2_set_obb_hit(&candidate, ro, rd, bw2_object_obb(strap_object), BW2_SURFACE_DIFFUSE, vec3(0.18f, 0.14f, 0.11f), vec3_o(0.0f), 0.0f);
    }

    blue_wall_v2_D_commit_candidate(hit, candidate);
}

static void blue_wall_v2_D_try_cheese_boxes(bw2_hit_t *hit, vec3_t ro, vec3_t rd, const blue_wall_generated_object_t *box_object, const blue_wall_generated_object_t *lid_object)
{
    if (box_object != NULL) {
        bw2_set_obb_hit(hit, ro, rd, bw2_object_obb(box_object), BW2_SURFACE_DIFFUSE, vec3(0.70f, 0.58f, 0.38f), vec3_o(0.0f), 0.0f);
    }
    if (lid_object != NULL) {
        bw2_set_obb_hit(hit, ro, rd, bw2_object_obb(lid_object), BW2_SURFACE_DIFFUSE, vec3(0.82f, 0.71f, 0.48f), vec3_o(0.0f), 0.0f);
    }
}

static void blue_wall_v2_D_try_floor_lamp(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    bw2_set_floor_lamp_proxy_hits(hit, ro, rd, vec3(0.76f, 0.74f, 0.72f), vec3(0.86f, 0.81f, 0.73f), vec3(1.0f, 0.93f, 0.80f), vec3(4.0f, 2.8f, 1.5f), 0.05f);
}

static bw2_hit_t blue_wall_v2_D_intersect(const shader_uniforms_t *uniforms, vec3_t ro, vec3_t rd)
{
    bw2_hit_t hit = bw2_make_empty_hit();

    bw2_set_room_shell_hits(&hit, ro, rd);

    if (blue_wall_v2_D_hit_group_volume("floor_lamp", ro, rd)) {
        blue_wall_v2_D_try_floor_lamp(&hit, ro, rd);
    }
    if (blue_wall_v2_D_hit_group_volume("painting", ro, rd)) {
        blue_wall_v2_D_try_painting(&hit, ro, rd, uniforms);
    }
    if (blue_wall_v2_D_hit_group_volume("bookshelf_left", ro, rd)) {
        blue_wall_v2_D_try_bookshelf(&hit, ro, rd, "bookshelf_left");
    }
    if (blue_wall_v2_D_hit_group_volume("bookshelf_right", ro, rd)) {
        blue_wall_v2_D_try_bookshelf(&hit, ro, rd, "bookshelf_right");
    }
    if (blue_wall_v2_D_hit_group_volume("sideboard", ro, rd)) {
        blue_wall_v2_D_try_sideboard(&hit, ro, rd);
    }
    if (blue_wall_v2_D_hit_group_volume("chair", ro, rd)) {
        blue_wall_v2_D_try_chair(&hit, ro, rd);
    }
    if (blue_wall_v2_D_hit_group_volume("table_tall", ro, rd)) {
        blue_wall_v2_D_try_table(&hit, ro, rd, "table_tall", vec3(0.33f, 0.42f, 0.60f), vec3(0.28f, 0.30f, 0.34f));
    }
    if (blue_wall_v2_D_hit_group_volume("table_small", ro, rd)) {
        blue_wall_v2_D_try_table(&hit, ro, rd, "table_small", vec3(0.36f, 0.22f, 0.14f), vec3(0.26f, 0.17f, 0.11f));
    }
    if (blue_wall_v2_D_hit_group_volume("ukulele", ro, rd)) {
        blue_wall_v2_D_try_ukulele(&hit, ro, rd);
    }
    if (blue_wall_v2_D_hit_group_volume("sideboard_props", ro, rd)) {
        const blue_wall_generated_object_t *cheese_box = bw2_find_object("CheeseBox_01");
        const blue_wall_generated_object_t *cheese_lid = bw2_find_object("CheeseBox_01_lid");
        const blue_wall_generated_object_t *lantern = bw2_find_object("Lantern_01.002");
        const blue_wall_generated_object_t *camera_body = bw2_find_object("Camera_01");
        const blue_wall_generated_object_t *camera_strap = bw2_find_object("Camera_01_strap");
        const blue_wall_generated_object_t *plant = bw2_find_object("potted_plant_04.001");
        const blue_wall_generated_object_t *clock = bw2_find_object("mantel_clock_01.001");
        const blue_wall_generated_object_t *horse = bw2_find_object("horse_statue_01.001");

        if (blue_wall_v2_D_hit_object_volume(cheese_box, ro, rd) || blue_wall_v2_D_hit_object_volume(cheese_lid, ro, rd)) {
            blue_wall_v2_D_try_cheese_boxes(&hit, ro, rd, cheese_box, cheese_lid);
        }
        if (blue_wall_v2_D_hit_object_volume(lantern, ro, rd)) {
            blue_wall_v2_D_try_lantern(&hit, ro, rd, lantern);
        }
        if (blue_wall_v2_D_hit_object_volume(camera_body, ro, rd) || blue_wall_v2_D_hit_object_volume(camera_strap, ro, rd)) {
            blue_wall_v2_D_try_camera(&hit, ro, rd, camera_body, camera_strap);
        }
        if (blue_wall_v2_D_hit_object_volume(clock, ro, rd)) {
            blue_wall_v2_D_try_clock(&hit, ro, rd, clock);
        }
        if (blue_wall_v2_D_hit_object_volume(plant, ro, rd)) {
            blue_wall_v2_D_try_plant(&hit, ro, rd, plant);
        }
        if (blue_wall_v2_D_hit_object_volume(horse, ro, rd)) {
            blue_wall_v2_D_try_horse(&hit, ro, rd, horse);
        }
    }

    return hit;
}

vec4_t blue_wall_v2_D_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return bw2_render_shader(fragCoord, uniforms, blue_wall_v2_D_intersect, 0xD2D4u);
}
