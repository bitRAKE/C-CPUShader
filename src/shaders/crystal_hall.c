//Crystal Hall Path Tracing Shader

#include "crystal_hall.h"

#define MAX_BOUNCES 5

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

static float ray_sphere(vec3_t ro, vec3_t rd, vec3_t so, float sr)
{
    vec3_t v = v3_sub(ro, so);
    float b = 2.0f * v3_dot(rd, v);
    float c = v3_dot(v, v) - (sr * sr);
    float h = b * b - 4.0f * c;

    if (h < 0.0f) return -1.0f;

    h = sqrtf(h);

    float near_hit = (-b - h) * 0.5f;
    if (near_hit > 0.001f) return near_hit;

    float far_hit = (-b + h) * 0.5f;
    if (far_hit > 0.001f) return far_hit;

    return -1.0f;
}

static float ray_plane(vec3_t ro, vec3_t rd, vec3_t po, vec3_t pn)
{
    float denom = v3_dot(rd, pn);
    if (fabsf(denom) < 0.0001f) return -1.0f;

    float t = v3_dot(v3_sub(po, ro), pn) / denom;
    return (t > 0.001f) ? t : -1.0f;
}

static float schlick(float cosine, float ior)
{
    float r0 = (1.0f - ior) / (1.0f + ior);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

static uint next_rand(uint* state)
{
    *state = *state * 747796405 + 2891336453;
    uint result = ((*state >> ((*state >> 28) + 4)) ^ *state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}

static float rand_1(uint* state)
{
    return next_rand(state) / 4294967295.0f;
}

static float rand_1_nd(uint* state)
{
    float theta = 2.0f * PI * rand_1(state);
    float rho = sqrtf(-2.0f * logf(rand_1(state)));
    return rho * cosf(theta);
}

static vec3_t rand_dir(uint* state)
{
    float x = rand_1_nd(state);
    float y = rand_1_nd(state);
    float z = rand_1_nd(state);
    return v3_normalize(vec3(x, y, z));
}

static vec3_t sky_color(vec3_t rd)
{
    float t = saturate(0.5f + 0.5f * rd.y);
    return v3_lerp(vec3(0.02f, 0.02f, 0.05f), vec3(0.12f, 0.14f, 0.20f), t);
}

static void set_hit_sphere(
    HitData* hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t center,
    float radius,
    vec3_t albedo,
    vec3_t emission,
    int surface_type,
    float ior)
{
    float distance = ray_sphere(ro, rd, center, radius);

    if (distance < 0.0f || distance >= hit->distance) {
        return;
    }

    vec3_t point = v3_add(ro, v3_mul1(rd, distance));
    vec3_t outward_normal = v3_normalize(v3_sub(point, center));
    bool is_front_face = v3_dot(rd, outward_normal) < 0.0f;

    hit->distance = distance;
    hit->surface_type = surface_type;
    hit->is_front_face = is_front_face;
    hit->normal = is_front_face ? outward_normal : v3_mul1(outward_normal, -1.0f);
    hit->albedo = albedo;
    hit->emission = emission;
    hit->ior = ior;
}

static void set_hit_plane(
    HitData* hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t plane_point,
    vec3_t plane_normal,
    vec3_t albedo,
    vec3_t emission,
    int surface_type)
{
    float distance = ray_plane(ro, rd, plane_point, plane_normal);

    if (distance < 0.0f || distance >= hit->distance) {
        return;
    }

    hit->distance = distance;
    hit->surface_type = surface_type;
    hit->is_front_face = true;
    hit->normal = plane_normal;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->ior = 1.0f;
}

static HitData intersect(vec3_t ro, vec3_t rd)
{
    HitData hit;
    hit.surface_type = SURFACE_NONE;
    hit.is_front_face = true;
    hit.albedo = vec3_o(1.0f);
    hit.emission = vec3_o(0.0f);
    hit.normal = vec3_o(0.0f);
    hit.distance = 10000.0f;
    hit.ior = 1.0f;

    set_hit_plane(&hit, ro, rd, vec3(0.0f, -1.1f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.92f, 0.92f, 0.95f), vec3_o(0.0f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(0.0f, 1.6f, 0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(0.88f, 0.90f, 0.98f), vec3_o(0.0f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(-2.35f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.90f, 0.35f, 0.30f), vec3_o(0.0f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(2.35f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f), vec3(0.30f, 0.72f, 0.95f), vec3_o(0.0f), SURFACE_DIFFUSE);
    set_hit_plane(&hit, ro, rd, vec3(0.0f, 0.0f, 4.7f), vec3(0.0f, 0.0f, -1.0f), vec3(0.92f, 0.92f, 0.90f), vec3_o(0.0f), SURFACE_DIFFUSE);

    set_hit_sphere(&hit, ro, rd, vec3(0.0f, 1.05f, 1.55f), 0.22f, vec3_o(1.0f), vec3_o(18.0f), SURFACE_LIGHT, 1.0f);
    set_hit_sphere(&hit, ro, rd, vec3(0.0f, -0.15f, 1.35f), 0.72f, vec3(0.97f, 0.99f, 1.0f), vec3_o(0.0f), SURFACE_GLASS, 1.45f);
    set_hit_sphere(&hit, ro, rd, vec3(-1.12f, -0.48f, 2.15f), 0.58f, vec3(0.96f, 0.96f, 0.98f), vec3_o(0.0f), SURFACE_MIRROR, 1.0f);
    set_hit_sphere(&hit, ro, rd, vec3(1.08f, -0.66f, 1.85f), 0.42f, vec3(1.0f, 0.72f, 0.42f), vec3_o(0.0f), SURFACE_DIFFUSE, 1.0f);

    return hit;
}

static vec3_t trace(vec3_t ro, vec3_t rd, uint* state)
{
    const float eps = 0.0005f;
    vec3_t radiance = vec3_o(0.0f);
    vec3_t throughput = vec3_o(1.0f);

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        HitData hit = intersect(ro, rd);

        if (hit.distance > 9999.0f) {
            radiance = v3_add(radiance, v3_mul(throughput, sky_color(rd)));
            break;
        }

        radiance = v3_add(radiance, v3_mul(hit.emission, throughput));

        if (hit.surface_type == SURFACE_LIGHT) {
            break;
        }

        ro = v3_add(ro, v3_mul1(rd, hit.distance));

        if (hit.surface_type == SURFACE_DIFFUSE) {
            vec3_t bounce_dir = v3_add(hit.normal, rand_dir(state));
            if (v3_dot(bounce_dir, hit.normal) < 0.0f) {
                bounce_dir = v3_mul1(bounce_dir, -1.0f);
            }

            throughput = v3_mul(throughput, hit.albedo);
            rd = v3_normalize(bounce_dir);
            ro = v3_add(ro, v3_mul1(hit.normal, eps));
            continue;
        }

        if (hit.surface_type == SURFACE_MIRROR) {
            throughput = v3_mul(throughput, hit.albedo);
            rd = v3_reflect(rd, hit.normal);
            ro = v3_add(ro, v3_mul1(hit.normal, eps));
            continue;
        }

        if (hit.surface_type == SURFACE_GLASS) {
            vec3_t reflected = v3_reflect(rd, hit.normal);
            float eta = hit.is_front_face ? (1.0f / hit.ior) : hit.ior;
            float cosine = fabsf(v3_dot(v3_mul1(rd, -1.0f), hit.normal));
            float reflectance = schlick(cosine, hit.ior);
            vec3_t refracted = v3_refract(rd, hit.normal, eta);
            bool can_refract = v3_length_sq(refracted) > 0.0f;
            bool reflect_it = !can_refract || rand_1(state) < reflectance;

            if (reflect_it) {
                rd = reflected;
                ro = v3_add(ro, v3_mul1(hit.normal, eps));
            } else {
                throughput = v3_mul(throughput, hit.albedo);
                rd = refracted;
                ro = v3_sub(ro, v3_mul1(hit.normal, eps));
            }

            continue;
        }
    }

    return radiance;
}

vec4_t crystal_hall_main(vec2_t fragCoord, vec2_t resolution, float time, uint frame) {
    uint state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) + frame * 78423;
    vec2_t jitter = vec2(rand_1(&state) - 0.5f, rand_1(&state) - 0.5f);
    vec2_t pixel = vec2(fragCoord.x + jitter.x, fragCoord.y + jitter.y);
    vec2_t uv = vec2(pixel.x - resolution.x * 0.5f, pixel.y - resolution.y * 0.5f);
    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    vec3_t ray_origin = vec3(sinf(time * 0.35f) * 0.45f, 0.10f + 0.08f * sinf(time * 0.21f), -4.8f + 0.18f * cosf(time * 0.27f));
    vec3_t target = vec3(0.0f, -0.05f, 1.8f);
    vec3_t forward = v3_normalize(v3_sub(target, ray_origin));
    vec3_t right = v3_normalize(v3_cross(vec3(0.0f, 1.0f, 0.0f), forward));
    vec3_t up = v3_cross(forward, right);
    vec3_t ray_dir = v3_normalize(v3_add(forward, v3_add(v3_mul1(right, uv.x * 1.35f), v3_mul1(up, uv.y * 1.35f))));

    vec3_t light = trace(ray_origin, ray_dir, &state);
    return vec4(light.x, light.y, light.z, 1.0f);
}
