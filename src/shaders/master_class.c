//Master Class Path Tracing Shader

#include "master_class.h"

#define MAX_BOUNCES 17
#define LIGHT_HALF_WIDTH 0.80f
#define LIGHT_HALF_DEPTH 0.58f
#define LIGHT_EPSILON 0.0010f

enum {
    SURFACE_NONE = 0,
    SURFACE_DIFFUSE = 1,
    SURFACE_METAL = 2,
    SURFACE_GLASS = 3,
    SURFACE_LIGHT = 4
};

typedef struct {
    int surface_type;
    bool is_front_face;
    vec3_t albedo;
    vec3_t emission;
    vec3_t absorption;
    vec3_t normal;
    float distance;
    float roughness;
    float ior;
} HitData;

static const vec3_t LIGHT_CENTER = { 0.0f, 1.58f, 1.95f };
static const vec3_t LIGHT_NORMAL = { 0.0f, -1.0f, 0.0f };
static const vec3_t LIGHT_EMISSION = { 16.0f, 13.0f, 10.0f };

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
        float t = v3_dot(v3_sub(plane_point, ro), plane_normal) / denom;
        return (t > 0.001f) ? t : -1.0f;
    }
}

static uint next_rand(uint* state)
{
    *state = *state * 747796405u + 2891336453u;

    {
        uint result = ((*state >> ((*state >> 28) + 4)) ^ *state) * 277803737u;
        result = (result >> 22) ^ result;
        return result;
    }
}

static float rand_1(uint* state)
{
    return next_rand(state) / 4294967295.0f;
}

static vec3_t rand_dir(uint* state)
{
    float z = rand_1(state) * 2.0f - 1.0f;
    float phi = 2.0f * PI * rand_1(state);
    float radius = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    return vec3(cosf(phi) * radius, sinf(phi) * radius, z);
}

static void build_basis(vec3_t normal, vec3_t* tangent, vec3_t* bitangent)
{
    vec3_t helper = (fabsf(normal.y) < 0.999f) ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
    *tangent = v3_normalize(v3_cross(helper, normal));
    *bitangent = v3_cross(normal, *tangent);
}

static vec3_t sample_cosine_hemisphere(vec3_t normal, uint* state)
{
    vec3_t tangent;
    vec3_t bitangent;
    float r1 = rand_1(state);
    float r2 = rand_1(state);
    float phi = 2.0f * PI * r1;
    float radius = sqrtf(r2);
    float x = cosf(phi) * radius;
    float y = sinf(phi) * radius;
    float z = sqrtf(fmaxf(0.0f, 1.0f - r2));

    build_basis(normal, &tangent, &bitangent);
    return v3_normalize(v3_add(v3_add(v3_mul1(tangent, x), v3_mul1(bitangent, y)), v3_mul1(normal, z)));
}

static vec3_t sample_glossy_lobe(vec3_t reflected, vec3_t normal, float roughness, uint* state)
{
    vec3_t direction = v3_normalize(v3_add(reflected, v3_mul1(rand_dir(state), roughness)));

    if (v3_dot(direction, normal) <= 0.0f) {
        return reflected;
    }

    return direction;
}

static float schlick(float cosine, float eta_i, float eta_t)
{
    float r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

static vec3_t beer_lambert(vec3_t absorption, float distance)
{
    return vec3(
        expf(-absorption.x * distance),
        expf(-absorption.y * distance),
        expf(-absorption.z * distance));
}

static vec3_t sky_color(vec3_t rd)
{
    float t = saturate(0.5f + 0.5f * rd.y);
    vec3_t horizon = vec3(0.38f, 0.29f, 0.24f);
    vec3_t zenith = vec3(0.10f, 0.16f, 0.28f);
    return v3_lerp(horizon, zenith, t);
}

static vec3_t floor_albedo(vec3_t point)
{
    float swirl = 0.5f + 0.5f * sinf(point.x * 5.5f + sinf(point.z * 2.4f) * 0.7f);
    float vein = smoothstepf(0.25f, 0.85f, swirl);
    return v3_lerp(vec3(0.48f, 0.45f, 0.42f), vec3(0.76f, 0.72f, 0.66f), 0.18f + 0.32f * vein);
}

static HitData make_empty_hit(void)
{
    HitData hit;
    hit.surface_type = SURFACE_NONE;
    hit.is_front_face = true;
    hit.albedo = vec3_o(1.0f);
    hit.emission = vec3_o(0.0f);
    hit.absorption = vec3_o(0.0f);
    hit.normal = vec3_o(0.0f);
    hit.distance = 10000.0f;
    hit.roughness = 0.0f;
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
    hit->absorption = vec3_o(0.0f);
    hit->roughness = 0.0f;
    hit->ior = 1.0f;
}

static void set_hit_plane(
    HitData* hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t plane_point,
    vec3_t plane_normal,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness,
    float ior,
    vec3_t absorption)
{
    float distance = ray_plane(ro, rd, plane_point, plane_normal);

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = surface_type;
    hit->is_front_face = v3_dot(rd, plane_normal) < 0.0f;
    hit->normal = hit->is_front_face ? plane_normal : v3_mul1(plane_normal, -1.0f);
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
    hit->ior = ior;
    hit->absorption = absorption;
}

static void set_hit_floor(HitData* hit, vec3_t ro, vec3_t rd)
{
    float distance = ray_plane(ro, rd, vec3(0.0f, -1.18f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = SURFACE_DIFFUSE;
    hit->is_front_face = true;
    hit->normal = vec3(0.0f, 1.0f, 0.0f);
    hit->distance = distance;
    hit->albedo = floor_albedo(v3_add(ro, v3_mul1(rd, distance)));
    hit->emission = vec3_o(0.0f);
    hit->absorption = vec3_o(0.0f);
    hit->roughness = 0.0f;
    hit->ior = 1.0f;
}

static void set_hit_sphere(
    HitData* hit,
    vec3_t ro,
    vec3_t rd,
    vec3_t center,
    float radius,
    int surface_type,
    vec3_t albedo,
    vec3_t emission,
    float roughness,
    float ior,
    vec3_t absorption)
{
    float distance = ray_sphere(ro, rd, center, radius);
    vec3_t point;
    vec3_t outward_normal;

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    point = v3_add(ro, v3_mul1(rd, distance));
    outward_normal = v3_normalize(v3_sub(point, center));

    hit->surface_type = surface_type;
    hit->is_front_face = v3_dot(rd, outward_normal) < 0.0f;
    hit->normal = hit->is_front_face ? outward_normal : v3_mul1(outward_normal, -1.0f);
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
    hit->ior = ior;
    hit->absorption = absorption;
}

static HitData intersect_scene(vec3_t ro, vec3_t rd)
{
    HitData hit = make_empty_hit();

    set_hit_rect_light(&hit, ro, rd);

    set_hit_floor(&hit, ro, rd);
    set_hit_plane(&hit, ro, rd, vec3(0.0f, 1.70f, 0.0f), vec3(0.0f, -1.0f, 0.0f), SURFACE_DIFFUSE, vec3(0.84f, 0.86f, 0.92f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    set_hit_plane(&hit, ro, rd, vec3(-2.30f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), SURFACE_DIFFUSE, vec3(0.82f, 0.21f, 0.16f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    set_hit_plane(&hit, ro, rd, vec3(2.30f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f), SURFACE_DIFFUSE, vec3(0.16f, 0.48f, 0.84f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    set_hit_plane(&hit, ro, rd, vec3(0.0f, 0.0f, 5.10f), vec3(0.0f, 0.0f, -1.0f), SURFACE_DIFFUSE, vec3(0.76f, 0.78f, 0.83f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));

    // Foreground spheres stay dominant, but the rear accents are staged to read as
    // separate silhouettes rather than disappearing behind the hero glass.
    set_hit_sphere(&hit, ro, rd, vec3(-0.62f, -0.33f, 2.28f), 0.83f, SURFACE_GLASS, vec3_o(1.0f), vec3_o(0.0f), 0.0f, 1.48f, vec3(0.28f, 0.09f, 0.03f));
    set_hit_sphere(&hit, ro, rd, vec3(1.18f, -0.60f, 1.95f), 0.57f, SURFACE_METAL, vec3(0.98f, 0.77f, 0.47f), vec3_o(0.0f), 0.08f, 1.0f, vec3_o(0.0f));
    set_hit_sphere(&hit, ro, rd, vec3(0.24f, -0.78f, 2.86f), 0.30f, SURFACE_DIFFUSE, vec3(0.86f, 0.79f, 0.72f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    set_hit_sphere(&hit, ro, rd, vec3(-1.58f, -0.94f, 2.72f), 0.28f, SURFACE_METAL, vec3(0.90f, 0.93f, 0.97f), vec3_o(0.0f), 0.02f, 1.0f, vec3_o(0.0f));

    return hit;
}

static vec3_t sample_light_point(uint* state)
{
    float sx = lerpf(-LIGHT_HALF_WIDTH, LIGHT_HALF_WIDTH, rand_1(state));
    float sz = lerpf(-LIGHT_HALF_DEPTH, LIGHT_HALF_DEPTH, rand_1(state));
    return vec3(LIGHT_CENTER.x + sx, LIGHT_CENTER.y, LIGHT_CENTER.z + sz);
}

static vec3_t estimate_direct_light(vec3_t point, vec3_t normal, vec3_t albedo, uint* state)
{
    vec3_t light_point = sample_light_point(state);
    vec3_t to_light = v3_sub(light_point, point);
    float distance_sq = v3_length_sq(to_light);
    float distance;
    vec3_t direction;
    float cos_surface;
    float cos_light;
    float light_area;
    float weight;
    HitData shadow_hit;

    if (distance_sq <= 0.000001f) {
        return vec3_o(0.0f);
    }

    distance = sqrtf(distance_sq);
    direction = v3_div1(to_light, distance);
    cos_surface = saturate(v3_dot(normal, direction));
    cos_light = saturate(v3_dot(LIGHT_NORMAL, v3_mul1(direction, -1.0f)));

    if (cos_surface <= 0.0f || cos_light <= 0.0f) {
        return vec3_o(0.0f);
    }

    shadow_hit = intersect_scene(v3_add(point, v3_mul1(normal, LIGHT_EPSILON)), direction);
    if (shadow_hit.surface_type != SURFACE_LIGHT || fabsf(shadow_hit.distance - distance) > 0.02f) {
        return vec3_o(0.0f);
    }

    light_area = (LIGHT_HALF_WIDTH * 2.0f) * (LIGHT_HALF_DEPTH * 2.0f);
    weight = (cos_surface * cos_light * light_area) / (distance_sq * PI);
    return v3_mul1(v3_mul(albedo, LIGHT_EMISSION), weight);
}

static vec3_t aces_tonemap(vec3_t color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    color = v3_mul1(color, 1.15f);
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

static float max_component(vec3_t value)
{
    return fmaxf(value.x, fmaxf(value.y, value.z));
}

static vec3_t trace(vec3_t ro, vec3_t rd, uint* state)
{
    const float eps = 0.0005f;
    vec3_t radiance = vec3_o(0.0f);
    vec3_t throughput = vec3_o(1.0f);
    bool last_bounce_specular = true;

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        HitData hit = intersect_scene(ro, rd);
        vec3_t point;

        if (hit.distance > 9999.0f) {
            radiance = v3_add(radiance, v3_mul(throughput, sky_color(rd)));
            break;
        }

        point = v3_add(ro, v3_mul1(rd, hit.distance));
        if (last_bounce_specular || bounce == 0) {
            radiance = v3_add(radiance, v3_mul(throughput, hit.emission));
        }

        if (hit.surface_type == SURFACE_LIGHT) {
            break;
        }

        if (hit.surface_type == SURFACE_DIFFUSE) {
            radiance = v3_add(radiance, v3_mul(throughput, estimate_direct_light(point, hit.normal, hit.albedo, state)));
            throughput = v3_mul(throughput, hit.albedo);
            rd = sample_cosine_hemisphere(hit.normal, state);
            ro = v3_add(point, v3_mul1(hit.normal, eps));
            last_bounce_specular = false;
        } else if (hit.surface_type == SURFACE_METAL) {
            vec3_t reflected = v3_reflect(rd, hit.normal);
            throughput = v3_mul(throughput, hit.albedo);
            rd = sample_glossy_lobe(reflected, hit.normal, hit.roughness, state);
            ro = v3_add(point, v3_mul1(hit.normal, eps));
            last_bounce_specular = true;
        } else if (hit.surface_type == SURFACE_GLASS) {
            float eta_i = hit.is_front_face ? 1.0f : hit.ior;
            float eta_t = hit.is_front_face ? hit.ior : 1.0f;
            float cosine = saturate(v3_dot(v3_mul1(rd, -1.0f), hit.normal));
            vec3_t reflected = v3_reflect(rd, hit.normal);
            vec3_t refracted = v3_refract(rd, hit.normal, eta_i / eta_t);
            bool can_refract = v3_length_sq(refracted) > 0.0f;
            float reflectance = can_refract ? schlick(cosine, eta_i, eta_t) : 1.0f;

            if (!hit.is_front_face) {
                throughput = v3_mul(throughput, beer_lambert(hit.absorption, hit.distance));
            }

            if (rand_1(state) < reflectance) {
                rd = reflected;
                ro = v3_add(point, v3_mul1(hit.normal, eps));
            } else {
                throughput = v3_mul(throughput, hit.albedo);
                rd = refracted;
                ro = v3_sub(point, v3_mul1(hit.normal, eps));
            }

            last_bounce_specular = true;
        }

        if (bounce >= 3) {
            float survive = clampf(max_component(throughput), 0.10f, 0.95f);
            if (rand_1(state) > survive) {
                break;
            }

            throughput = v3_div1(throughput, survive);
        }
    }

    return radiance;
}

vec4_t master_class_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const vec2_t resolution = uniforms->resolution;
    const uint frame = uniforms->frame;
    uint state;
    vec2_t jitter;
    vec2_t pixel;
    vec2_t uv;
    vec3_t ray_origin;
    vec3_t target;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    vec3_t ray_dir;
    vec3_t color;

    state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) + frame * 78423u;
    jitter = vec2(rand_1(&state) - 0.5f, rand_1(&state) - 0.5f);
    pixel = vec2(fragCoord.x + jitter.x, fragCoord.y + jitter.y);
    uv = vec2(pixel.x - resolution.x * 0.5f, pixel.y - resolution.y * 0.5f);
    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    ray_origin = vec3(0.15f, 0.02f, -1.95f);
    target = vec3(0.02f, -0.35f, 2.15f);
    forward = v3_normalize(v3_sub(target, ray_origin));
    right = v3_normalize(v3_cross(vec3(0.0f, 1.0f, 0.0f), forward));
    up = v3_cross(forward, right);
    ray_dir = v3_normalize(v3_add(forward, v3_add(v3_mul1(right, uv.x * 1.28f), v3_mul1(up, uv.y * 1.28f))));

    color = trace(ray_origin, ray_dir, &state);
    color = aces_tonemap(color);
    color = gamma_encode(color);
    return vec4(color.x, color.y, color.z, 1.0f);
}
