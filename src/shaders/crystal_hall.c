//Crystal Hall Live Shader

#include "crystal_hall.h"

#define MAX_BOUNCES 5
#define LIGHT_HALF_WIDTH 0.68f
#define LIGHT_HALF_DEPTH 0.44f

enum {
    SURFACE_NONE = 0,
    SURFACE_DIFFUSE = 1,
    SURFACE_MIRROR = 2,
    SURFACE_GLASS = 3,
    SURFACE_LIGHT = 4
};

typedef struct {
    int surface_type;
    bool is_front_face;
    vec3_t albedo;
    vec3_t emission;
    vec3_t normal;
    float distance;
    float ior;
} HitData;

static const vec3_t LIGHT_CENTER = { 0.0f, 1.28f, 1.55f };
static const vec3_t LIGHT_NORMAL = { 0.0f, -1.0f, 0.0f };
static const vec3_t LIGHT_EMISSION = { 12.0f, 10.8f, 9.4f };

static float ray_sphere(vec3_t ro, vec3_t rd, vec3_t center, float radius)
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

static float ray_plane(vec3_t ro, vec3_t rd, vec3_t plane_point, vec3_t plane_normal)
{
    float denom = v3_dot(rd, plane_normal);

    if (fabsf(denom) < 0.0001f) {
        return -1.0f;
    }

    {
        float distance = v3_dot(v3_sub(plane_point, ro), plane_normal) / denom;
        return (distance > 0.001f) ? distance : -1.0f;
    }
}

static float schlick(float cosine, float eta_i, float eta_t)
{
    float r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

static vec3_t sky_color(vec3_t rd)
{
    float t = saturate(0.5f + 0.5f * rd.y);
    return v3_lerp(vec3(0.03f, 0.03f, 0.06f), vec3(0.12f, 0.15f, 0.22f), t);
}

static vec3_t aces_tonemap(vec3_t color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    color = v3_mul1(color, 1.12f);
    return vec3(
        saturate((color.x * (a * color.x + b)) / (color.x * (c * color.x + d) + e)),
        saturate((color.y * (a * color.y + b)) / (color.y * (c * color.y + d) + e)),
        saturate((color.z * (a * color.z + b)) / (color.z * (c * color.z + d) + e)));
}

static vec3_t gamma_encode(vec3_t color)
{
    return vec3(
        powf(saturate(color.x), 1.0f / 2.2f),
        powf(saturate(color.y), 1.0f / 2.2f),
        powf(saturate(color.z), 1.0f / 2.2f));
}

static HitData make_empty_hit(void)
{
    HitData hit;
    hit.surface_type = SURFACE_NONE;
    hit.is_front_face = true;
    hit.albedo = vec3_o(1.0f);
    hit.emission = vec3_o(0.0f);
    hit.normal = vec3_o(0.0f);
    hit.distance = 10000.0f;
    hit.ior = 1.0f;
    return hit;
}

static void set_hit_rect_light(HitData* hit, vec3_t ro, vec3_t rd)
{
    float distance;
    vec3_t point;

    if (v3_dot(rd, LIGHT_NORMAL) >= 0.0f) {
        return;
    }

    distance = ray_plane(ro, rd, LIGHT_CENTER, LIGHT_NORMAL);
    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    point = v3_add(ro, v3_mul1(rd, distance));
    if (fabsf(point.x - LIGHT_CENTER.x) > LIGHT_HALF_WIDTH || fabsf(point.z - LIGHT_CENTER.z) > LIGHT_HALF_DEPTH) {
        return;
    }

    hit->surface_type = SURFACE_LIGHT;
    hit->is_front_face = true;
    hit->normal = LIGHT_NORMAL;
    hit->distance = distance;
    hit->albedo = vec3_o(0.0f);
    hit->emission = LIGHT_EMISSION;
    hit->ior = 1.0f;
}

static void set_hit_plane(
    HitData* hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t plane_point,
    vec3_t plane_normal,
    vec3_t albedo,
    int surface_type)
{
    float distance = ray_plane(ro, rd, plane_point, plane_normal);

    (void)ro;

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = surface_type;
    hit->is_front_face = v3_dot(rd, plane_normal) < 0.0f;
    hit->normal = hit->is_front_face ? plane_normal : v3_mul1(plane_normal, -1.0f);
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = vec3_o(0.0f);
    hit->ior = 1.0f;
}

static void set_hit_sphere(
    HitData* hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t center,
    float radius,
    vec3_t albedo,
    int surface_type,
    float ior)
{
    float distance = ray_sphere(ro, rd, center, radius);
    vec3_t point;
    vec3_t outward_normal;

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    point = v3_add(ro, v3_mul1(rd, distance));
    outward_normal = v3_normalize(v3_sub(point, center));

    hit->distance = distance;
    hit->surface_type = surface_type;
    hit->is_front_face = v3_dot(rd, outward_normal) < 0.0f;
    hit->normal = hit->is_front_face ? outward_normal : v3_mul1(outward_normal, -1.0f);
    hit->albedo = albedo;
    hit->emission = vec3_o(0.0f);
    hit->ior = ior;
}

static HitData intersect(vec3_t ro, vec3_t rd)
{
    HitData hit = make_empty_hit();

    set_hit_rect_light(&hit, ro, rd);

    set_hit_plane(&hit, ro, rd, vec3(0.0f, -1.1f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.92f, 0.92f, 0.95f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(0.0f, 1.6f, 0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(0.88f, 0.90f, 0.98f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(-2.35f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.90f, 0.35f, 0.30f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(2.35f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f), vec3(0.30f, 0.72f, 0.95f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(0.0f, 0.0f, 4.7f), vec3(0.0f, 0.0f, -1.0f), vec3(0.92f, 0.92f, 0.90f), SURFACE_DIFFUSE);

    set_hit_sphere(&hit, ro, rd, vec3(0.0f, -0.15f, 1.35f), 0.72f, vec3(0.97f, 0.99f, 1.0f), SURFACE_GLASS, 1.45f);
    set_hit_sphere(&hit, ro, rd, vec3(-1.12f, -0.48f, 2.15f), 0.58f, vec3(0.96f, 0.96f, 0.98f), SURFACE_MIRROR, 1.0f);
    set_hit_sphere(&hit, ro, rd, vec3(1.08f, -0.66f, 1.85f), 0.42f, vec3(1.0f, 0.72f, 0.42f), SURFACE_DIFFUSE, 1.0f);

    return hit;
}

static vec3_t sample_light_point(int sample_index)
{
    static const vec2_t OFFSETS[4] = {
        { -0.65f, -0.55f },
        {  0.60f, -0.18f },
        { -0.10f,  0.52f },
        {  0.74f,  0.34f }
    };

    vec2_t uv = OFFSETS[sample_index & 3];
    return vec3(
        LIGHT_CENTER.x + uv.x * LIGHT_HALF_WIDTH,
        LIGHT_CENTER.y,
        LIGHT_CENTER.z + uv.y * LIGHT_HALF_DEPTH);
}

static vec3_t estimate_direct_light(vec3_t point, vec3_t normal, vec3_t albedo)
{
    vec3_t result = vec3_o(0.0f);
    float light_area = (LIGHT_HALF_WIDTH * 2.0f) * (LIGHT_HALF_DEPTH * 2.0f);

    for (int i = 0; i < 4; i++) {
        vec3_t light_point = sample_light_point(i);
        vec3_t to_light = v3_sub(light_point, point);
        float distance_sq = v3_length_sq(to_light);
        float distance;
        vec3_t direction;
        float cos_surface;
        float cos_light;
        float weight;
        HitData shadow_hit;

        if (distance_sq <= 0.000001f) {
            continue;
        }

        distance = sqrtf(distance_sq);
        direction = v3_div1(to_light, distance);
        cos_surface = saturate(v3_dot(normal, direction));
        cos_light = saturate(v3_dot(LIGHT_NORMAL, v3_mul1(direction, -1.0f)));

        if (cos_surface <= 0.0f || cos_light <= 0.0f) {
            continue;
        }

        shadow_hit = intersect(v3_add(point, v3_mul1(normal, 0.0010f)), direction);
        if (shadow_hit.surface_type != SURFACE_LIGHT || fabsf(shadow_hit.distance - distance) > 0.03f) {
            continue;
        }

        weight = (cos_surface * cos_light * light_area) / (distance_sq * PI * 4.0f);
        result = v3_add(result, v3_mul1(v3_mul(albedo, LIGHT_EMISSION), weight));
    }

    return result;
}

static vec3_t trace(vec3_t ro, vec3_t rd)
{
    const float eps = 0.0005f;
    vec3_t radiance = vec3_o(0.0f);
    vec3_t throughput = vec3_o(1.0f);

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        HitData hit = intersect(ro, rd);
        vec3_t point;

        if (hit.distance > 9999.0f) {
            radiance = v3_add(radiance, v3_mul(throughput, sky_color(rd)));
            break;
        }

        point = v3_add(ro, v3_mul1(rd, hit.distance));

        if (hit.surface_type == SURFACE_LIGHT) {
            radiance = v3_add(radiance, v3_mul(throughput, hit.emission));
            break;
        }

        if (hit.surface_type == SURFACE_DIFFUSE) {
            vec3_t ambient = v3_mul1(v3_mul(hit.albedo, sky_color(hit.normal)), 0.08f);
            radiance = v3_add(radiance, v3_mul(throughput, v3_add(estimate_direct_light(point, hit.normal, hit.albedo), ambient)));
            break;
        }

        if (hit.surface_type == SURFACE_MIRROR) {
            throughput = v3_mul(throughput, hit.albedo);
            rd = v3_reflect(rd, hit.normal);
            ro = v3_add(point, v3_mul1(hit.normal, eps));
            continue;
        }

        if (hit.surface_type == SURFACE_GLASS) {
            vec3_t reflected = v3_reflect(rd, hit.normal);
            float eta_i = hit.is_front_face ? 1.0f : hit.ior;
            float eta_t = hit.is_front_face ? hit.ior : 1.0f;
            float cosine = saturate(v3_dot(v3_mul1(rd, -1.0f), hit.normal));
            float reflectance = schlick(cosine, eta_i, eta_t);
            vec3_t refracted = v3_refract(rd, hit.normal, eta_i / eta_t);
            bool can_refract = v3_length_sq(refracted) > 0.0f;

            if (!can_refract || reflectance > 0.28f) {
                rd = reflected;
                ro = v3_add(point, v3_mul1(hit.normal, eps));
            } else {
                throughput = v3_mul(throughput, hit.albedo);
                rd = refracted;
                ro = v3_sub(point, v3_mul1(hit.normal, eps));
            }

            continue;
        }
    }

    return radiance;
}

vec4_t crystal_hall_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    const float time = uniforms->time;
    vec2_t pixel;
    vec2_t uv;
    vec3_t ray_origin;
    vec3_t target;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    vec3_t ray_dir;
    vec3_t light;

    pixel = vec2(fragCoord.x + 0.5f, fragCoord.y + 0.5f);
    uv = vec2(pixel.x - resolution.x * 0.5f, pixel.y - resolution.y * 0.5f);
    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    ray_origin = vec3(sinf(time * 0.35f) * 0.45f, 0.10f + 0.08f * sinf(time * 0.21f), -4.8f + 0.18f * cosf(time * 0.27f));
    target = vec3(0.0f, -0.05f, 1.8f);
    forward = v3_normalize(v3_sub(target, ray_origin));
    right = v3_normalize(v3_cross(vec3(0.0f, 1.0f, 0.0f), forward));
    up = v3_cross(forward, right);
    ray_dir = v3_normalize(v3_add(forward, v3_add(v3_mul1(right, uv.x * 1.35f), v3_mul1(up, uv.y * 1.35f))));

    light = trace(ray_origin, ray_dir);
    light = aces_tonemap(light);
    light = gamma_encode(light);
    return vec4(light.x, light.y, light.z, 1.0f);
}
