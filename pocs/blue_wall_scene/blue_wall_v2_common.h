#pragma once

#include <string.h>

#include "shader_defines.h"
#include "scene_extract.h"
#include "scene_extract_generated.h"

#if defined(__clang__) || defined(__GNUC__)
#define BW2_MAYBE_UNUSED __attribute__((unused))
#else
#define BW2_MAYBE_UNUSED
#endif

#define BW2_MAX_BOUNCES 10
#define BW2_HIT_EPSILON 0.0005f
#define BW2_SHADOW_EPSILON 0.0010f

enum {
    BW2_SURFACE_NONE = 0,
    BW2_SURFACE_DIFFUSE = 1,
    BW2_SURFACE_METAL = 2,
    BW2_SURFACE_LIGHT = 3
};

typedef struct {
    int    surface_type;
    vec3_t albedo;
    vec3_t emission;
    vec3_t normal;
    float  distance;
    float  roughness;
} bw2_hit_t;

typedef bw2_hit_t (*bw2_intersect_fn)(const shader_uniforms_t *uniforms, vec3_t ro, vec3_t rd);

typedef struct {
    vec3_t center;
    vec3_t half_extent;
} bw2_box_t;

typedef struct {
    vec3_t center;
    vec3_t half_extent;
    vec3_t axis_x;
    vec3_t axis_y;
    vec3_t axis_z;
} bw2_obb_t;

static vec3_t bw2_from_blender_xyz(const float value[3])
{
    return vec3(value[0], value[2], value[1]);
}

static vec3_t bw2_from_blender_dir(const float value[3])
{
    return vec3(value[0], value[2], value[1]);
}

static vec3_t bw2_from_blender_local_extent(const float value[3])
{
    return vec3(value[0], value[1], value[2]);
}

static BW2_MAYBE_UNUSED bw2_box_t bw2_group_box(const blue_wall_generated_group_t *group)
{
    bw2_box_t bounds;
    bounds.center = bw2_from_blender_xyz(group->center);
    bounds.half_extent = v3_mul1(bw2_from_blender_xyz(group->dimensions), 0.5f);
    return bounds;
}

static BW2_MAYBE_UNUSED bw2_obb_t bw2_object_obb(const blue_wall_generated_object_t *object)
{
    bw2_obb_t bounds;
    bounds.center = bw2_from_blender_xyz(object->world_center);
    bounds.half_extent = v3_mul1(bw2_from_blender_local_extent(object->dimensions), 0.5f);
    bounds.axis_x = v3_normalize(bw2_from_blender_dir(object->basis_x));
    bounds.axis_y = v3_normalize(bw2_from_blender_dir(object->basis_y));
    bounds.axis_z = v3_normalize(bw2_from_blender_dir(object->basis_z));
    return bounds;
}

static BW2_MAYBE_UNUSED const blue_wall_generated_group_t *bw2_find_group(const char *id)
{
    for (int index = 0; index < BLUE_WALL_GENERATED_GROUP_COUNT; index++) {
        if (strcmp(g_blue_wall_generated_groups[index].id, id) == 0) {
            return &g_blue_wall_generated_groups[index];
        }
    }

    return NULL;
}

static BW2_MAYBE_UNUSED const blue_wall_generated_object_t *bw2_find_object(const char *name)
{
    for (int index = 0; index < BLUE_WALL_GENERATED_OBJECT_COUNT; index++) {
        if (strcmp(g_blue_wall_generated_objects[index].name, name) == 0) {
            return &g_blue_wall_generated_objects[index];
        }
    }

    return NULL;
}

static const blue_wall_extract_light_t *bw2_find_light(const char *name)
{
    for (int index = 0; index < BLUE_WALL_LIGHT_COUNT; index++) {
        if (strcmp(g_blue_wall_lights[index].name, name) == 0) {
            return &g_blue_wall_lights[index];
        }
    }

    return NULL;
}

static int bw2_object_has_stage(const blue_wall_generated_object_t *object, uint stage_bit)
{
    return (object->stage_mask & stage_bit) != 0;
}

static BW2_MAYBE_UNUSED int bw2_is_layout_object(const blue_wall_generated_object_t *object)
{
    if (strcmp(object->type, "MESH") != 0) {
        return false;
    }
    if (!bw2_object_has_stage(object, BLUE_WALL_STAGE_B_BIT)) {
        return false;
    }
    if (object->group_id[0] == '\0') {
        return false;
    }
    if (strcmp(object->group_id, "room_shell") == 0 || strcmp(object->group_id, "window_key_light") == 0 || strcmp(object->group_id, "ceiling_fill") == 0) {
        return false;
    }
    if (strcmp(object->group_id, "sideboard_props") == 0) {
        return false;
    }

    return true;
}

static BW2_MAYBE_UNUSED vec3_t bw2_role_tint(const blue_wall_generated_object_t *object)
{
    if (strcmp(object->group_id, "floor_lamp") == 0) {
        return vec3(0.94f, 0.78f, 0.33f);
    }
    if (strcmp(object->role, "painting") == 0) {
        return vec3(0.92f, 0.48f, 0.38f);
    }
    if (strcmp(object->role, "bookshelf") == 0) {
        return vec3(0.55f, 0.31f, 0.12f);
    }
    if (strcmp(object->role, "sideboard") == 0) {
        return vec3(0.70f, 0.44f, 0.20f);
    }
    if (strcmp(object->role, "chair") == 0) {
        return vec3(0.26f, 0.56f, 0.35f);
    }
    if (strcmp(object->role, "table") == 0) {
        return vec3(0.42f, 0.56f, 0.80f);
    }
    if (strcmp(object->role, "instrument") == 0) {
        return vec3(0.82f, 0.53f, 0.22f);
    }
    if (strcmp(object->role, "plant") == 0) {
        return vec3(0.32f, 0.62f, 0.30f);
    }

    return vec3(0.76f, 0.76f, 0.76f);
}

static float bw2_fractf(float value)
{
    return value - floorf(value);
}

static float bw2_max_component(vec3_t value)
{
    return fmaxf(value.x, fmaxf(value.y, value.z));
}

static void bw2_build_basis(vec3_t normal, vec3_t *tangent, vec3_t *bitangent);

static bw2_hit_t bw2_make_empty_hit(void)
{
    bw2_hit_t hit;
    hit.surface_type = BW2_SURFACE_NONE;
    hit.albedo = vec3_o(1.0f);
    hit.emission = vec3_o(0.0f);
    hit.normal = vec3_o(0.0f);
    hit.distance = 10000.0f;
    hit.roughness = 0.0f;
    return hit;
}

static uint bw2_next_rand(uint *state)
{
    *state = *state * 747796405u + 2891336453u;

    {
        uint result = ((*state >> ((*state >> 28) + 4)) ^ *state) * 277803737u;
        result = (result >> 22) ^ result;
        return result;
    }
}

static float bw2_rand_1(uint *state)
{
    return bw2_next_rand(state) / 4294967295.0f;
}

static vec3_t bw2_rand_dir(uint *state)
{
    float z = bw2_rand_1(state) * 2.0f - 1.0f;
    float phi = 2.0f * PI * bw2_rand_1(state);
    float radius = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    return vec3(cosf(phi) * radius, sinf(phi) * radius, z);
}

static float bw2_ray_sphere(vec3_t ro, vec3_t rd, vec3_t center, float radius)
{
    vec3_t offset = v3_sub(ro, center);
    float b = 2.0f * v3_dot(rd, offset);
    float c = v3_dot(offset, offset) - radius * radius;
    float h = b * b - 4.0f * c;

    if (h < 0.0f) {
        return -1.0f;
    }

    h = sqrtf(h);

    {
        float near_hit = (-b - h) * 0.5f;
        if (near_hit > 0.001f) {
            return near_hit;
        }
    }

    {
        float far_hit = (-b + h) * 0.5f;
        if (far_hit > 0.001f) {
            return far_hit;
        }
    }

    return -1.0f;
}

static float bw2_ray_box(vec3_t ro, vec3_t rd, vec3_t bounds_min, vec3_t bounds_max, vec3_t *normal_out)
{
    const float eps = 0.000001f;
    float tmin = -1000000.0f;
    float tmax = 1000000.0f;
    vec3_t enter_normal = vec3_o(0.0f);
    vec3_t exit_normal = vec3_o(0.0f);

    for (int axis = 0; axis < 3; axis++) {
        float origin = (axis == 0) ? ro.x : (axis == 1) ? ro.y : ro.z;
        float direction = (axis == 0) ? rd.x : (axis == 1) ? rd.y : rd.z;
        float min_value = (axis == 0) ? bounds_min.x : (axis == 1) ? bounds_min.y : bounds_min.z;
        float max_value = (axis == 0) ? bounds_max.x : (axis == 1) ? bounds_max.y : bounds_max.z;
        vec3_t normal_min = vec3_o(0.0f);
        vec3_t normal_max = vec3_o(0.0f);

        if (axis == 0) {
            normal_min = vec3(-1.0f, 0.0f, 0.0f);
            normal_max = vec3(1.0f, 0.0f, 0.0f);
        } else if (axis == 1) {
            normal_min = vec3(0.0f, -1.0f, 0.0f);
            normal_max = vec3(0.0f, 1.0f, 0.0f);
        } else {
            normal_min = vec3(0.0f, 0.0f, -1.0f);
            normal_max = vec3(0.0f, 0.0f, 1.0f);
        }

        if (fabsf(direction) < eps) {
            if (origin < min_value || origin > max_value) {
                return -1.0f;
            }
            continue;
        }

        {
            float inv_direction = 1.0f / direction;
            float t1 = (min_value - origin) * inv_direction;
            float t2 = (max_value - origin) * inv_direction;
            vec3_t normal1 = normal_min;
            vec3_t normal2 = normal_max;

            if (t1 > t2) {
                float temp_t = t1;
                vec3_t temp_normal = normal1;
                t1 = t2;
                t2 = temp_t;
                normal1 = normal2;
                normal2 = temp_normal;
            }

            if (t1 > tmin) {
                tmin = t1;
                enter_normal = normal1;
            }
            if (t2 < tmax) {
                tmax = t2;
                exit_normal = normal2;
            }
            if (tmin > tmax || tmax <= 0.001f) {
                return -1.0f;
            }
        }
    }

    if (tmin > 0.001f) {
        if (normal_out != NULL) {
            *normal_out = enter_normal;
        }
        return tmin;
    }

    if (tmax > 0.001f) {
        if (normal_out != NULL) {
            *normal_out = exit_normal;
        }
        return tmax;
    }

    return -1.0f;
}

static float bw2_ray_obb(vec3_t ro, vec3_t rd, bw2_obb_t bounds, vec3_t *normal_out)
{
    vec3_t offset = v3_sub(ro, bounds.center);
    vec3_t local_ro = vec3(v3_dot(offset, bounds.axis_x), v3_dot(offset, bounds.axis_y), v3_dot(offset, bounds.axis_z));
    vec3_t local_rd = vec3(v3_dot(rd, bounds.axis_x), v3_dot(rd, bounds.axis_y), v3_dot(rd, bounds.axis_z));
    vec3_t local_normal = vec3_o(0.0f);
    float distance = bw2_ray_box(local_ro, local_rd, v3_mul1(bounds.half_extent, -1.0f), bounds.half_extent, &local_normal);

    if (distance <= 0.0f) {
        return distance;
    }

    if (normal_out != NULL) {
        *normal_out = v3_normalize(v3_add(v3_add(v3_mul1(bounds.axis_x, local_normal.x), v3_mul1(bounds.axis_y, local_normal.y)), v3_mul1(bounds.axis_z, local_normal.z)));
    }

    return distance;
}

static BW2_MAYBE_UNUSED vec3_t bw2_obb_local_to_world_point(bw2_obb_t bounds, vec3_t local_point)
{
    return v3_add(bounds.center, v3_add(v3_add(v3_mul1(bounds.axis_x, local_point.x), v3_mul1(bounds.axis_y, local_point.y)), v3_mul1(bounds.axis_z, local_point.z)));
}

static BW2_MAYBE_UNUSED vec3_t bw2_obb_world_to_local_point(bw2_obb_t bounds, vec3_t world_point)
{
    vec3_t offset = v3_sub(world_point, bounds.center);
    return vec3(v3_dot(offset, bounds.axis_x), v3_dot(offset, bounds.axis_y), v3_dot(offset, bounds.axis_z));
}

static BW2_MAYBE_UNUSED vec3_t bw2_obb_local_to_world_dir(bw2_obb_t bounds, vec3_t local_dir)
{
    return v3_add(v3_add(v3_mul1(bounds.axis_x, local_dir.x), v3_mul1(bounds.axis_y, local_dir.y)), v3_mul1(bounds.axis_z, local_dir.z));
}

static BW2_MAYBE_UNUSED void bw2_set_local_box_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    bw2_obb_t bounds,
    vec3_t local_min,
    vec3_t local_max,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    vec3_t offset = v3_sub(ro, bounds.center);
    vec3_t local_ro = vec3(v3_dot(offset, bounds.axis_x), v3_dot(offset, bounds.axis_y), v3_dot(offset, bounds.axis_z));
    vec3_t local_rd = vec3(v3_dot(rd, bounds.axis_x), v3_dot(rd, bounds.axis_y), v3_dot(rd, bounds.axis_z));
    vec3_t local_normal = vec3_o(0.0f);
    float distance = bw2_ray_box(local_ro, local_rd, local_min, local_max, &local_normal);

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = surface_type;
    hit->normal = v3_normalize(bw2_obb_local_to_world_dir(bounds, local_normal));
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
}

static void bw2_set_box_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t bounds_min,
    vec3_t bounds_max,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    vec3_t normal = vec3_o(0.0f);
    float distance = bw2_ray_box(ro, rd, bounds_min, bounds_max, &normal);

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = surface_type;
    hit->normal = normal;
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
}

static BW2_MAYBE_UNUSED void bw2_set_obb_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    bw2_obb_t bounds,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    vec3_t normal = vec3_o(0.0f);
    float distance = bw2_ray_obb(ro, rd, bounds, &normal);

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = surface_type;
    hit->normal = normal;
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
}

static BW2_MAYBE_UNUSED void bw2_set_sphere_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t center,
    float radius,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    float distance = bw2_ray_sphere(ro, rd, center, radius);
    vec3_t point;
    vec3_t normal;

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    point = v3_add(ro, v3_mul1(rd, distance));
    normal = v3_normalize(v3_sub(point, center));
    if (v3_dot(rd, normal) > 0.0f) {
        normal = v3_mul1(normal, -1.0f);
    }

    hit->surface_type = surface_type;
    hit->normal = normal;
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
}

static BW2_MAYBE_UNUSED void bw2_set_cylinder_y_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t center,
    float radius,
    float y0,
    float y1,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    float ocx = ro.x - center.x;
    float ocz = ro.z - center.z;
    float a = rd.x * rd.x + rd.z * rd.z;
    float best_t = hit->distance;
    vec3_t best_normal = vec3_o(0.0f);
    bool found = false;

    if (a > 0.000001f) {
        float b = 2.0f * (ocx * rd.x + ocz * rd.z);
        float c = ocx * ocx + ocz * ocz - radius * radius;
        float h = b * b - 4.0f * a * c;

        if (h >= 0.0f) {
            float root = sqrtf(h);
            float candidates[2] = {
                (-b - root) / (2.0f * a),
                (-b + root) / (2.0f * a)
            };

            for (int i = 0; i < 2; i++) {
                float t = candidates[i];
                if (t > 0.001f && t < best_t) {
                    float y = ro.y + rd.y * t;
                    if (y >= y0 && y <= y1) {
                        vec3_t point = v3_add(ro, v3_mul1(rd, t));
                        best_t = t;
                        best_normal = v3_normalize(vec3(point.x - center.x, 0.0f, point.z - center.z));
                        found = true;
                    }
                }
            }
        }
    }

    if (fabsf(rd.y) > 0.000001f) {
        float cap_y[2] = { y0, y1 };
        vec3_t cap_n[2] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f) };

        for (int i = 0; i < 2; i++) {
            float t = (cap_y[i] - ro.y) / rd.y;
            if (t > 0.001f && t < best_t) {
                vec3_t point = v3_add(ro, v3_mul1(rd, t));
                float dx = point.x - center.x;
                float dz = point.z - center.z;
                if (dx * dx + dz * dz <= radius * radius) {
                    best_t = t;
                    best_normal = cap_n[i];
                    found = true;
                }
            }
        }
    }

    if (!found) {
        return;
    }

    hit->surface_type = surface_type;
    hit->normal = best_normal;
    hit->distance = best_t;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
}

static BW2_MAYBE_UNUSED void bw2_set_cylinder_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t center,
    vec3_t axis,
    float radius,
    float half_height,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    vec3_t tangent;
    vec3_t bitangent;
    vec3_t normalized_axis = v3_normalize(axis);
    vec3_t offset;
    vec3_t local_ro;
    vec3_t local_rd;
    float a;
    float best_t = hit->distance;
    vec3_t best_local_normal = vec3_o(0.0f);
    bool found = false;

    if (v3_length_sq(normalized_axis) < 0.000001f) {
        bw2_set_cylinder_y_hit(hit, ro, rd, center, radius, center.y - half_height, center.y + half_height, surface_type, albedo, emission, roughness);
        return;
    }

    bw2_build_basis(normalized_axis, &tangent, &bitangent);
    offset = v3_sub(ro, center);
    local_ro = vec3(v3_dot(offset, tangent), v3_dot(offset, normalized_axis), v3_dot(offset, bitangent));
    local_rd = vec3(v3_dot(rd, tangent), v3_dot(rd, normalized_axis), v3_dot(rd, bitangent));
    a = local_rd.x * local_rd.x + local_rd.z * local_rd.z;

    if (a > 0.000001f) {
        float b = 2.0f * (local_ro.x * local_rd.x + local_ro.z * local_rd.z);
        float c = local_ro.x * local_ro.x + local_ro.z * local_ro.z - radius * radius;
        float h = b * b - 4.0f * a * c;

        if (h >= 0.0f) {
            float root = sqrtf(h);
            float candidates[2] = {
                (-b - root) / (2.0f * a),
                (-b + root) / (2.0f * a)
            };

            for (int i = 0; i < 2; i++) {
                float t = candidates[i];
                if (t > 0.001f && t < best_t) {
                    float y = local_ro.y + local_rd.y * t;
                    if (y >= -half_height && y <= half_height) {
                        vec3_t point = v3_add(local_ro, v3_mul1(local_rd, t));
                        best_t = t;
                        best_local_normal = v3_normalize(vec3(point.x, 0.0f, point.z));
                        found = true;
                    }
                }
            }
        }
    }

    if (fabsf(local_rd.y) > 0.000001f) {
        float cap_y[2] = { -half_height, half_height };
        vec3_t cap_n[2] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f) };

        for (int i = 0; i < 2; i++) {
            float t = (cap_y[i] - local_ro.y) / local_rd.y;
            if (t > 0.001f && t < best_t) {
                vec3_t point = v3_add(local_ro, v3_mul1(local_rd, t));
                if (point.x * point.x + point.z * point.z <= radius * radius) {
                    best_t = t;
                    best_local_normal = cap_n[i];
                    found = true;
                }
            }
        }
    }

    if (!found) {
        return;
    }

    hit->surface_type = surface_type;
    hit->normal = v3_normalize(v3_add(v3_add(v3_mul1(tangent, best_local_normal.x), v3_mul1(normalized_axis, best_local_normal.y)), v3_mul1(bitangent, best_local_normal.z)));
    hit->distance = best_t;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
}

static BW2_MAYBE_UNUSED void bw2_set_segment_cylinder_hit(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t a,
    vec3_t b,
    float radius,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness)
{
    vec3_t axis = v3_sub(b, a);
    float length = v3_length(axis);

    if (length <= 0.000001f) {
        return;
    }

    bw2_set_cylinder_hit(hit, ro, rd, v3_mul1(v3_add(a, b), 0.5f), v3_div1(axis, length), radius, length * 0.5f, surface_type, albedo, emission, roughness);
}

static BW2_MAYBE_UNUSED void bw2_set_floor_lamp_proxy_hits(
    bw2_hit_t *hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t stand_albedo,
    vec3_t shade_albedo,
    vec3_t bulb_albedo,
    vec3_t bulb_emission,
    float metal_roughness)
{
    const blue_wall_generated_object_t *stand_object = NULL;
    const blue_wall_generated_object_t *shade_objects[8];
    int shade_count = 0;

    for (int index = 0; index < BLUE_WALL_GENERATED_OBJECT_COUNT; index++) {
        const blue_wall_generated_object_t *object = &g_blue_wall_generated_objects[index];

        if (strcmp(object->group_id, "floor_lamp") != 0) {
            continue;
        }

        if (strcmp(object->proxy_family, "floor_lamp_stand") == 0) {
            stand_object = object;
        } else if (strcmp(object->proxy_family, "floor_lamp_box_shade") == 0) {
            if (shade_count < (int)(sizeof(shade_objects) / sizeof(shade_objects[0]))) {
                shade_objects[shade_count++] = object;
            }
        } else if (strcmp(object->type, "LIGHT") == 0 && bw2_max_component(bulb_emission) > 0.0f) {
            bw2_set_sphere_hit(hit, ro, rd, bw2_from_blender_xyz(object->world_center), 0.040f, BW2_SURFACE_LIGHT, bulb_albedo, bulb_emission, 0.0f);
        }
    }

    if (stand_object != NULL) {
        bw2_obb_t stand_bounds = bw2_object_obb(stand_object);
        vec3_t stand_axis = stand_bounds.axis_z;
        float pole_radius = fmaxf(0.022f, fminf(stand_bounds.half_extent.x, stand_bounds.half_extent.y) * 0.18f);
        vec3_t stand_base = v3_sub(stand_bounds.center, v3_mul1(stand_axis, stand_bounds.half_extent.z));
        vec3_t hub_center = v3_add(stand_base, v3_mul1(stand_axis, stand_bounds.half_extent.z * 1.58f));
        vec3_t pole_start = v3_add(stand_base, v3_mul1(stand_axis, pole_radius * 1.1f));
        vec3_t pole_end = v3_sub(hub_center, v3_mul1(stand_axis, pole_radius * 1.2f));

        bw2_set_segment_cylinder_hit(hit, ro, rd, pole_start, pole_end, pole_radius, BW2_SURFACE_METAL, stand_albedo, vec3_o(0.0f), metal_roughness);
        bw2_set_cylinder_hit(hit, ro, rd, v3_add(stand_base, v3_mul1(stand_axis, pole_radius * 0.55f)), stand_axis, pole_radius * 1.9f, pole_radius * 0.55f, BW2_SURFACE_METAL, stand_albedo, vec3_o(0.0f), metal_roughness);
        bw2_set_sphere_hit(hit, ro, rd, hub_center, pole_radius * 1.45f, BW2_SURFACE_METAL, stand_albedo, vec3_o(0.0f), metal_roughness);

        for (int shade_index = 0; shade_index < shade_count; shade_index++) {
            bw2_obb_t shade_bounds = bw2_object_obb(shade_objects[shade_index]);
            vec3_t arm_target = v3_sub(shade_bounds.center, v3_mul1(shade_bounds.axis_z, shade_bounds.half_extent.z * 0.48f));
            vec3_t arm_start = v3_add(hub_center, v3_mul1(v3_normalize(v3_sub(arm_target, hub_center)), pole_radius * 1.15f));

            bw2_set_segment_cylinder_hit(hit, ro, rd, arm_start, arm_target, pole_radius * 0.50f, BW2_SURFACE_METAL, stand_albedo, vec3_o(0.0f), metal_roughness);
            bw2_set_obb_hit(hit, ro, rd, shade_bounds, BW2_SURFACE_DIFFUSE, shade_albedo, vec3_o(0.0f), 0.0f);
        }
        return;
    }

    for (int shade_index = 0; shade_index < shade_count; shade_index++) {
        bw2_set_obb_hit(hit, ro, rd, bw2_object_obb(shade_objects[shade_index]), BW2_SURFACE_DIFFUSE, shade_albedo, vec3_o(0.0f), 0.0f);
    }
}

static vec3_t bw2_sky_color(vec3_t rd)
{
    float t = saturate(0.5f + 0.5f * rd.y);
    return v3_lerp(vec3(0.83f, 0.78f, 0.71f), vec3(0.17f, 0.23f, 0.33f), t);
}

static vec3_t bw2_blue_wall_albedo(vec3_t point)
{
    float vertical = smoothstepf(0.0f, 2.4f, point.y);
    float plaster = 0.5f + 0.5f * sinf(point.x * 6.2f + point.y * 9.3f + point.z * 2.7f);
    return v3_lerp(vec3(0.08f, 0.16f, 0.31f), vec3(0.13f, 0.29f, 0.56f), 0.42f + 0.20f * vertical + 0.06f * plaster);
}

static vec3_t bw2_plaster_albedo(vec3_t point, vec3_t light, vec3_t dark)
{
    float wash = 0.5f + 0.5f * sinf(point.y * 7.0f + point.z * 4.1f);
    return v3_lerp(dark, light, 0.58f + 0.10f * wash);
}

static vec3_t bw2_floor_albedo(vec3_t point)
{
    float board = floorf((point.x + 6.0f) * 2.15f);
    float seam = fabsf(bw2_fractf((point.x + 6.0f) * 2.15f) - 0.5f);
    float grain = 0.5f + 0.5f * sinf(point.z * 17.0f + board * 1.9f + sinf(point.x * 3.0f) * 0.8f);
    vec3_t color = v3_lerp(vec3(0.42f, 0.29f, 0.18f), vec3(0.60f, 0.43f, 0.26f), 0.22f + 0.55f * grain);
    return v3_mul1(color, 0.94f - 0.10f * smoothstepf(0.0f, 0.05f, seam));
}

static void bw2_build_basis(vec3_t normal, vec3_t *tangent, vec3_t *bitangent)
{
    vec3_t helper = (fabsf(normal.y) < 0.999f) ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
    *tangent = v3_normalize(v3_cross(helper, normal));
    *bitangent = v3_cross(normal, *tangent);
}

static vec3_t bw2_sample_cosine_hemisphere(vec3_t normal, uint *state)
{
    vec3_t tangent;
    vec3_t bitangent;
    float r1 = bw2_rand_1(state);
    float r2 = bw2_rand_1(state);
    float phi = 2.0f * PI * r1;
    float radius = sqrtf(r2);
    float x = cosf(phi) * radius;
    float y = sinf(phi) * radius;
    float z = sqrtf(fmaxf(0.0f, 1.0f - r2));

    bw2_build_basis(normal, &tangent, &bitangent);
    return v3_normalize(v3_add(v3_add(v3_mul1(tangent, x), v3_mul1(bitangent, y)), v3_mul1(normal, z)));
}

static vec3_t bw2_sample_glossy_lobe(vec3_t reflected, vec3_t normal, float roughness, uint *state)
{
    vec3_t direction = v3_normalize(v3_add(reflected, v3_mul1(bw2_rand_dir(state), roughness)));

    if (v3_dot(direction, normal) <= 0.0f) {
        return reflected;
    }

    return direction;
}

static vec3_t bw2_window_center(void)
{
    const blue_wall_extract_light_t *window = bw2_find_light("Window");
    if (window == NULL) {
        return vec3(2.80f, 1.21f, -1.97f);
    }

    return bw2_from_blender_xyz(window->location);
}

static vec3_t bw2_window_normal(void)
{
    return vec3(-1.0f, 0.0f, 0.0f);
}

static vec3_t bw2_window_radiance(void)
{
    const blue_wall_extract_light_t *window = bw2_find_light("Window");
    if (window == NULL) {
        return vec3(8.8f, 9.2f, 10.6f);
    }

    return v3_mul1(vec3(window->color[0], window->color[1], window->color[2]), window->energy * 0.060f);
}

static vec3_t bw2_sample_window_light(uint *state)
{
    const blue_wall_extract_light_t *window = bw2_find_light("Window");
    vec3_t center = bw2_window_center();
    float half_depth = 0.66f;
    float half_height = 1.10f;

    if (window != NULL) {
        half_depth = window->size_x * 0.5f;
        half_height = window->size_y * 0.5f;
    }

    return v3_add(center, vec3(0.0f, lerpf(-half_height, half_height, bw2_rand_1(state)), lerpf(-half_depth, half_depth, bw2_rand_1(state))));
}

static vec3_t bw2_practical_cluster_position(void)
{
    const char *const point_names[4] = { "Point", "Point.001", "Point.002", "Point.003" };
    vec3_t sum = vec3_o(0.0f);
    int count = 0;

    for (int index = 0; index < 4; index++) {
        const blue_wall_extract_light_t *light = bw2_find_light(point_names[index]);
        if (light == NULL) {
            continue;
        }
        sum = v3_add(sum, bw2_from_blender_xyz(light->location));
        count++;
    }

    if (count == 0) {
        return vec3(-1.48f, 1.81f, -0.07f);
    }

    return v3_div1(sum, (float)count);
}

static vec3_t bw2_practical_cluster_radiance(void)
{
    return vec3(4.4f, 2.9f, 1.4f);
}

static vec3_t bw2_ceiling_fill_position(void)
{
    const blue_wall_extract_light_t *ceiling = bw2_find_light("Ceiling");
    if (ceiling == NULL) {
        return vec3(0.0f, 2.10f, -2.52f);
    }

    return bw2_from_blender_xyz(ceiling->location);
}

static vec3_t bw2_ceiling_fill_radiance(void)
{
    const blue_wall_extract_light_t *ceiling = bw2_find_light("Ceiling");
    if (ceiling == NULL) {
        return vec3(3.2f, 2.9f, 2.4f);
    }

    return v3_mul1(vec3(ceiling->color[0], ceiling->color[1], ceiling->color[2]), ceiling->energy * 0.040f);
}

static vec3_t bw2_estimate_point_light(
    vec3_t light_position,
    vec3_t light_radiance,
    vec3_t point,
    vec3_t normal,
    vec3_t albedo,
    const shader_uniforms_t *uniforms,
    bw2_intersect_fn intersect_fn)
{
    vec3_t to_light = v3_sub(light_position, point);
    float distance_sq = v3_length_sq(to_light);
    float distance;
    vec3_t direction;
    float cosine;
    bw2_hit_t shadow_hit;

    if (distance_sq <= 0.000001f) {
        return vec3_o(0.0f);
    }

    distance = sqrtf(distance_sq);
    direction = v3_div1(to_light, distance);
    cosine = saturate(v3_dot(normal, direction));
    if (cosine <= 0.0f) {
        return vec3_o(0.0f);
    }

    shadow_hit = intersect_fn(uniforms, v3_add(point, v3_mul1(normal, BW2_SHADOW_EPSILON)), direction);
    if (shadow_hit.distance < distance - 0.02f) {
        return vec3_o(0.0f);
    }

    return v3_mul1(v3_mul(albedo, light_radiance), cosine / distance_sq);
}

static vec3_t bw2_estimate_window_light(vec3_t point, vec3_t normal, vec3_t albedo, uint *state, const shader_uniforms_t *uniforms, bw2_intersect_fn intersect_fn)
{
    vec3_t light_point = bw2_sample_window_light(state);
    vec3_t to_light = v3_sub(light_point, point);
    float distance_sq = v3_length_sq(to_light);
    float distance;
    vec3_t direction;
    float cos_surface;
    float cos_light;
    float light_area = 1.32f * 2.201f;
    bw2_hit_t shadow_hit;
    const blue_wall_extract_light_t *window = bw2_find_light("Window");

    if (window != NULL) {
        light_area = window->size_x * window->size_y;
    }

    if (distance_sq <= 0.000001f) {
        return vec3_o(0.0f);
    }

    distance = sqrtf(distance_sq);
    direction = v3_div1(to_light, distance);
    cos_surface = saturate(v3_dot(normal, direction));
    cos_light = saturate(v3_dot(bw2_window_normal(), v3_mul1(direction, -1.0f)));

    if (cos_surface <= 0.0f || cos_light <= 0.0f) {
        return vec3_o(0.0f);
    }

    shadow_hit = intersect_fn(uniforms, v3_add(point, v3_mul1(normal, BW2_SHADOW_EPSILON)), direction);
    if (shadow_hit.distance < distance - 0.02f) {
        return vec3_o(0.0f);
    }

    return v3_mul1(v3_mul(albedo, bw2_window_radiance()), (cos_surface * cos_light * light_area) / (distance_sq * PI));
}

static vec3_t bw2_estimate_direct_light(vec3_t point, vec3_t normal, vec3_t albedo, uint *state, const shader_uniforms_t *uniforms, bw2_intersect_fn intersect_fn)
{
    vec3_t color = vec3_o(0.0f);
    color = v3_add(color, bw2_estimate_window_light(point, normal, albedo, state, uniforms, intersect_fn));
    color = v3_add(color, bw2_estimate_point_light(bw2_practical_cluster_position(), bw2_practical_cluster_radiance(), point, normal, albedo, uniforms, intersect_fn));
    color = v3_add(color, bw2_estimate_point_light(bw2_ceiling_fill_position(), bw2_ceiling_fill_radiance(), point, normal, albedo, uniforms, intersect_fn));
    return color;
}

static void bw2_make_camera_ray(vec2_t fragCoord, const shader_uniforms_t *uniforms, uint *state, vec3_t *ray_origin, vec3_t *ray_dir)
{
    vec2_t resolution = uniforms->resolution;
    vec2_t jitter = vec2(bw2_rand_1(state) - 0.5f, bw2_rand_1(state) - 0.5f);
    vec2_t pixel = vec2(fragCoord.x + jitter.x, fragCoord.y + jitter.y);
    float ndc_x = ((pixel.x + 0.5f) / resolution.x) * 2.0f - 1.0f;
    float ndc_y = ((pixel.y + 0.5f) / resolution.y) * 2.0f - 1.0f;
    float sx = tanf(g_blue_wall_camera.angle_x * 0.5f);
    float sy = tanf(g_blue_wall_camera.angle_y * 0.5f);

    *ray_origin = bw2_from_blender_xyz(g_blue_wall_camera.location);
    *ray_dir = v3_normalize(vec3(ndc_x * sx, ndc_y * sy, 1.0f));
}

static vec3_t bw2_trace(vec3_t ro, vec3_t rd, uint *state, const shader_uniforms_t *uniforms, bw2_intersect_fn intersect_fn)
{
    vec3_t radiance = vec3_o(0.0f);
    vec3_t throughput = vec3_o(1.0f);
    bool last_bounce_specular = true;

    for (int bounce = 0; bounce < BW2_MAX_BOUNCES; bounce++) {
        bw2_hit_t hit = intersect_fn(uniforms, ro, rd);
        vec3_t point;

        if (hit.distance > 9999.0f) {
            radiance = v3_add(radiance, v3_mul(throughput, bw2_sky_color(rd)));
            break;
        }

        point = v3_add(ro, v3_mul1(rd, hit.distance));

        if (last_bounce_specular || bounce == 0) {
            radiance = v3_add(radiance, v3_mul(throughput, hit.emission));
        }

        if (hit.surface_type == BW2_SURFACE_LIGHT) {
            break;
        }

        if (hit.surface_type == BW2_SURFACE_DIFFUSE) {
            radiance = v3_add(radiance, v3_mul(throughput, bw2_estimate_direct_light(point, hit.normal, hit.albedo, state, uniforms, intersect_fn)));
            throughput = v3_mul(throughput, hit.albedo);
            rd = bw2_sample_cosine_hemisphere(hit.normal, state);
            ro = v3_add(point, v3_mul1(hit.normal, BW2_HIT_EPSILON));
            last_bounce_specular = false;
        } else if (hit.surface_type == BW2_SURFACE_METAL) {
            vec3_t reflected = v3_reflect(rd, hit.normal);
            throughput = v3_mul(throughput, hit.albedo);
            rd = bw2_sample_glossy_lobe(reflected, hit.normal, hit.roughness, state);
            ro = v3_add(point, v3_mul1(hit.normal, BW2_HIT_EPSILON));
            last_bounce_specular = true;
        }

        if (bounce >= 3) {
            float survive = clampf(bw2_max_component(throughput), 0.12f, 0.95f);
            if (bw2_rand_1(state) > survive) {
                break;
            }
            throughput = v3_div1(throughput, survive);
        }
    }

    return radiance;
}

static vec4_t bw2_render_shader(vec2_t fragCoord, const shader_uniforms_t *uniforms, bw2_intersect_fn intersect_fn, uint stage_seed)
{
    uint state = (uint)fragCoord.x + (uint)(fragCoord.y * uniforms->resolution.x) + uniforms->frame * 92821u + stage_seed;
    vec3_t ray_origin;
    vec3_t ray_dir;
    vec3_t color;

    bw2_make_camera_ray(fragCoord, uniforms, &state, &ray_origin, &ray_dir);
    color = bw2_trace(ray_origin, ray_dir, &state, uniforms, intersect_fn);
    color = shader_aces_tonemap(color);
    color = shader_gamma_encode(color);
    return vec4(color.x, color.y, color.z, 1.0f);
}

static void bw2_set_room_shell_hits(bw2_hit_t *hit, vec3_t ro, vec3_t rd)
{
    const float room_left = -2.72f;
    const float room_right = 2.72f;
    const float room_back = 0.18f;
    const float room_front = -4.20f;
    const float floor_min = -0.05f;
    const float floor_max = 0.00f;
    const float ceiling_min = 2.64f;
    const float ceiling_max = 2.69f;
    const float wall_thickness = 0.04f;
    const float window_z0 = -2.72f;
    const float window_z1 = -1.18f;
    const float window_y0 = 0.38f;
    const float window_y1 = 2.18f;
    vec3_t point;

    bw2_set_box_hit(hit, ro, rd, vec3(room_left, floor_min, room_front), vec3(room_right, floor_max, room_back), BW2_SURFACE_DIFFUSE, vec3(0.50f, 0.36f, 0.22f), vec3_o(0.0f), 0.0f);
    if (hit->distance < 9999.0f && fabsf(hit->normal.y - 1.0f) < 0.001f) {
        point = v3_add(ro, v3_mul1(rd, hit->distance));
        hit->albedo = bw2_floor_albedo(point);
    }

    bw2_set_box_hit(hit, ro, rd, vec3(room_left, ceiling_min, room_front), vec3(room_right, ceiling_max, room_back), BW2_SURFACE_DIFFUSE, vec3(0.86f, 0.83f, 0.79f), vec3_o(0.0f), 0.0f);
    if (hit->distance < 9999.0f && fabsf(hit->normal.y + 1.0f) < 0.001f) {
        point = v3_add(ro, v3_mul1(rd, hit->distance));
        hit->albedo = bw2_plaster_albedo(point, vec3(0.86f, 0.83f, 0.79f), vec3(0.70f, 0.66f, 0.62f));
    }

    bw2_set_box_hit(hit, ro, rd, vec3(room_left, floor_max, room_front), vec3(room_left + wall_thickness, ceiling_min, room_back), BW2_SURFACE_DIFFUSE, vec3(0.80f, 0.78f, 0.73f), vec3_o(0.0f), 0.0f);
    if (hit->distance < 9999.0f && fabsf(hit->normal.x - 1.0f) < 0.001f) {
        point = v3_add(ro, v3_mul1(rd, hit->distance));
        hit->albedo = bw2_plaster_albedo(point, vec3(0.82f, 0.80f, 0.76f), vec3(0.60f, 0.58f, 0.53f));
    }

    bw2_set_box_hit(hit, ro, rd, vec3(room_right - wall_thickness, floor_max, room_front), vec3(room_right, window_y0, room_back), BW2_SURFACE_DIFFUSE, vec3(0.78f, 0.77f, 0.74f), vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(hit, ro, rd, vec3(room_right - wall_thickness, window_y1, room_front), vec3(room_right, ceiling_min, room_back), BW2_SURFACE_DIFFUSE, vec3(0.78f, 0.77f, 0.74f), vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(hit, ro, rd, vec3(room_right - wall_thickness, window_y0, room_front), vec3(room_right, window_y1, window_z0), BW2_SURFACE_DIFFUSE, vec3(0.78f, 0.77f, 0.74f), vec3_o(0.0f), 0.0f);
    bw2_set_box_hit(hit, ro, rd, vec3(room_right - wall_thickness, window_y0, window_z1), vec3(room_right, window_y1, room_back), BW2_SURFACE_DIFFUSE, vec3(0.78f, 0.77f, 0.74f), vec3_o(0.0f), 0.0f);
    if (hit->distance < 9999.0f && fabsf(hit->normal.x + 1.0f) < 0.001f) {
        point = v3_add(ro, v3_mul1(rd, hit->distance));
        hit->albedo = bw2_plaster_albedo(point, vec3(0.82f, 0.81f, 0.77f), vec3(0.61f, 0.59f, 0.56f));
    }

    bw2_set_box_hit(hit, ro, rd, vec3(room_left, floor_max, room_back - wall_thickness), vec3(room_right, ceiling_min, room_back), BW2_SURFACE_DIFFUSE, vec3(0.12f, 0.24f, 0.44f), vec3_o(0.0f), 0.0f);
    if (hit->distance < 9999.0f && fabsf(hit->normal.z + 1.0f) < 0.001f) {
        point = v3_add(ro, v3_mul1(rd, hit->distance));
        hit->albedo = bw2_blue_wall_albedo(point);
    }
}
