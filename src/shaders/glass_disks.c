//Glass Disks Tracing Shader

#include "glass_disks.h"

#define MAX_BOUNCES 8
#define LIGHT_INTENSITY 50.0f

enum {
    SURFACE_NONE = 0,
    SURFACE_LIGHT = 1,
    SURFACE_GLASS = 2
};

typedef struct {
    int surface_type;
    bool is_entering;
    vec2_t normal;
    float distance;
    float ior;
    float absorption;
    vec3_t emission;
} HitData;

typedef struct {
    vec2_t center;
    float radius;
    float ior_scale;
    float absorption_scale;
} DiskSpec;

static float ray_circle(vec2_t ro, vec2_t rd, vec2_t center, float radius)
{
    vec2_t oc = v2_sub(ro, center);
    float b = v2_dot(oc, rd);
    float c = v2_dot(oc, oc) - radius * radius;
    float h = b * b - c;

    if (h < 0.0f) {
        return -1.0f;
    }

    h = sqrtf(h);

    float near_hit = -b - h;
    if (near_hit > 0.0001f) {
        return near_hit;
    }

    float far_hit = -b + h;
    if (far_hit > 0.0001f) {
        return far_hit;
    }

    return -1.0f;
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

static float spectral_lobe(float wavelength, float center, float width)
{
    float t = 1.0f - fabsf(wavelength - center) / width;
    return smoothstepf(0.0f, 1.0f, t);
}

static vec3_t get_dispersed_color(float wavelength)
{
    float r = spectral_lobe(wavelength, 0.78f, 0.24f);
    float g = spectral_lobe(wavelength, 0.52f, 0.22f);
    float b = spectral_lobe(wavelength, 0.24f, 0.20f);
    return vec3(r, g, b);
}

static float wavelength_ior(float wavelength)
{
    return 1.28f + 0.24f * wavelength;
}

static float wavelength_absorption(float wavelength, float base_density)
{
    return base_density * lerpf(1.15f, 0.75f, wavelength);
}

static float schlick(float cosine, float eta_i, float eta_t)
{
    float r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

static HitData make_empty_hit(void)
{
    HitData hit;
    hit.surface_type = SURFACE_NONE;
    hit.is_entering = true;
    hit.normal = vec2_o(0.0f);
    hit.distance = 10000.0f;
    hit.ior = 1.0f;
    hit.absorption = 0.0f;
    hit.emission = vec3_o(0.0f);
    return hit;
}

static const vec2_t LIGHT_CENTER = { -0.72f, 0.04f };
static const float LIGHT_RADIUS = 0.05f;

// This slightly skewed cluster creates more near-tangent crossings than the old
// concentric ring layout, which gives the tracer more opportunities to build up
// multi-bounce caustic structure.
static const DiskSpec GLASS_DISKS[] = {
    { { -0.34f,  0.00f }, 0.14f, 1.05f, 0.95f },
    { { -0.10f,  0.18f }, 0.12f, 1.02f, 0.75f },
    { { -0.06f, -0.19f }, 0.12f, 1.03f, 0.75f },
    { {  0.14f,  0.02f }, 0.13f, 1.08f, 1.10f },
    { {  0.36f,  0.18f }, 0.12f, 0.99f, 0.65f },
    { {  0.40f, -0.17f }, 0.12f, 1.00f, 0.65f },
    { {  0.19f, -0.22f }, 0.07f, 1.10f, 0.85f },
    { {  0.22f,  0.30f }, 0.06f, 1.07f, 0.80f }
};

#define GLASS_DISK_COUNT ((int)(sizeof(GLASS_DISKS) / sizeof(GLASS_DISKS[0])))

static void set_light_hit(HitData* hit, vec2_t ro, vec2_t rd)
{
    float distance = ray_circle(ro, rd, LIGHT_CENTER, LIGHT_RADIUS);

    if (distance > 0.0f && distance < hit->distance) {
        hit->surface_type = SURFACE_LIGHT;
        hit->distance = distance;
        hit->emission = vec3_o(LIGHT_INTENSITY);
    }
}

static void set_glass_hit(
    HitData* hit,
    vec2_t ro,
    vec2_t rd,
    vec2_t center,
    float radius,
    float ior,
    float absorption)
{
    float distance = ray_circle(ro, rd, center, radius);

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    vec2_t point = v2_add(ro, v2_mul1(rd, distance));
    vec2_t outward_normal = v2_normalize(v2_sub(point, center));
    bool is_entering = v2_length_sq(v2_sub(ro, center)) > radius * radius;

    hit->surface_type = SURFACE_GLASS;
    hit->is_entering = is_entering;
    hit->normal = is_entering ? outward_normal : v2_mul1(outward_normal, -1.0f);
    hit->distance = distance;
    hit->ior = ior;
    hit->absorption = absorption;
}

static HitData intersect(vec2_t ro, vec2_t rd, float wavelength)
{
    HitData hit = make_empty_hit();
    float base_ior = wavelength_ior(wavelength);

    set_light_hit(&hit, ro, rd);

    for (int i = 0; i < GLASS_DISK_COUNT; i++) {
        const DiskSpec* disk = &GLASS_DISKS[i];
        float absorption = wavelength_absorption(wavelength, disk->absorption_scale);
        set_glass_hit(
            &hit,
            ro,
            rd,
            disk->center,
            disk->radius,
            base_ior * disk->ior_scale,
            absorption);
    }

    return hit;
}

static vec3_t trace(vec2_t ro, vec2_t rd, float wavelength, uint* state)
{
    const float eps = 0.0001f;
    float throughput = 1.0f;

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        HitData hit = intersect(ro, rd, wavelength);

        if (hit.distance > 9999.0f) {
            break;
        }

        if (hit.surface_type == SURFACE_LIGHT) {
            return v3_mul1(hit.emission, throughput);
        }

        ro = v2_add(ro, v2_mul1(rd, hit.distance));

        // The segment leading to an exit event was traveled inside the medium.
        if (!hit.is_entering) {
            throughput *= expf(-hit.absorption * hit.distance);
            if (throughput < 0.0001f) {
                break;
            }
        }

        float eta_i = hit.is_entering ? 1.0f : hit.ior;
        float eta_t = hit.is_entering ? hit.ior : 1.0f;
        vec2_t reflected = v2_reflect(rd, hit.normal);
        vec2_t refracted = v2_refract(rd, hit.normal, eta_i / eta_t);
        bool can_refract = v2_length_sq(refracted) > 0.0f;
        float cosine = saturate(v2_dot(v2_mul1(rd, -1.0f), hit.normal));
        float reflectance = can_refract ? schlick(cosine, eta_i, eta_t) : 1.0f;

        // We sample one branch using Fresnel as the PDF, so throughput should not
        // be multiplied by Fresnel again here.
        if (rand_1(state) < reflectance) {
            rd = reflected;
            ro = v2_add(ro, v2_mul1(hit.normal, eps));
        } else {
            rd = refracted;
            ro = v2_sub(ro, v2_mul1(hit.normal, eps));
        }
    }

    return vec3_o(0.0f);
}

#define DIRECTIONAL_SAMPLES 360

vec4_t glass_disks_main(vec2_t fragCoord, const shader_uniforms_t *uniforms) {
    const vec2_t resolution = uniforms->resolution;
    const uint frame = uniforms->frame;

    vec2_t uv = vec2(fragCoord.x - resolution.x * 0.5f, fragCoord.y - resolution.y * 0.5f);
    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    uint state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) + frame * 78423;

    float angle = 2.0f * PI * ((float)frame + rand_1(&state)) / DIRECTIONAL_SAMPLES;
    vec2_t dir = vec2(cosf(angle), sinf(angle));

    float wavelength = rand_1(&state);
    vec3_t color_mask = get_dispersed_color(wavelength);
    vec3_t light = trace(uv, dir, wavelength, &state);

    light = v3_mul(light, color_mask);
    return vec4(light.x, light.y, light.z, 1.0f);
}
