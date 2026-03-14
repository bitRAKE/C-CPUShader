#pragma once

#define MCX_MAX_BOUNCES 18
#define MCX_LIGHT_EPSILON 0.0010f

enum {
    MCX_SURFACE_NONE = 0,
    MCX_SURFACE_DIFFUSE = 1,
    MCX_SURFACE_METAL = 2,
    MCX_SURFACE_GLASS = 3,
    MCX_SURFACE_LIGHT = 4
};

typedef struct {
    vec3_t center;
    vec3_t normal;
    vec3_t tangent;
    vec3_t bitangent;
    float  half_width;
    float  half_height;
    vec3_t emission;
} mcx_rect_light_t;

typedef struct {
    int surface_type;
    int light_index;
    bool is_front_face;
    vec3_t albedo;
    vec3_t emission;
    vec3_t absorption;
    vec3_t normal;
    float distance;
    float roughness;
    float ior;
} mcx_hit_t;

static const mcx_rect_light_t MCX_LIGHTS[] = {
    {
        { 0.0f, 1.58f, 1.95f },
        { 0.0f, -1.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        0.82f,
        0.60f,
        { 32.0f, 26.0f, 20.0f }
    },
    {
        { 1.76f, 0.18f, 4.98f },
        { 0.0f, 0.0f, -1.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        0.12f,
        0.76f,
        { 5.0f, 20.0f, 54.0f }
    },
    {
        { -1.08f, -0.46f, 4.98f },
        { 0.0f, 0.0f, -1.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        0.58f,
        0.06f,
        { 52.0f, 18.0f, 6.0f }
    }
};

static const int MCX_LIGHT_COUNT = (int)(sizeof(MCX_LIGHTS) / sizeof(MCX_LIGHTS[0]));

static float mcx_ray_sphere(vec3_t ro, vec3_t rd, vec3_t center, float radius)
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

static float mcx_ray_plane(vec3_t ro, vec3_t rd, vec3_t plane_point, vec3_t plane_normal)
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

static uint mcx_next_rand(uint* state)
{
    *state = *state * 747796405u + 2891336453u;

    {
        uint result = ((*state >> ((*state >> 28) + 4)) ^ *state) * 277803737u;
        result = (result >> 22) ^ result;
        return result;
    }
}

static float mcx_rand_1(uint* state)
{
    return mcx_next_rand(state) / 4294967295.0f;
}

static vec3_t mcx_rand_dir(uint* state)
{
    float z = mcx_rand_1(state) * 2.0f - 1.0f;
    float phi = 2.0f * PI * mcx_rand_1(state);
    float radius = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    return vec3(cosf(phi) * radius, sinf(phi) * radius, z);
}

static void mcx_build_basis(vec3_t normal, vec3_t* tangent, vec3_t* bitangent)
{
    vec3_t helper = (fabsf(normal.y) < 0.999f) ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
    *tangent = v3_normalize(v3_cross(helper, normal));
    *bitangent = v3_cross(normal, *tangent);
}

static vec3_t mcx_sample_cosine_hemisphere(vec3_t normal, uint* state)
{
    vec3_t tangent;
    vec3_t bitangent;
    float r1 = mcx_rand_1(state);
    float r2 = mcx_rand_1(state);
    float phi = 2.0f * PI * r1;
    float radius = sqrtf(r2);
    float x = cosf(phi) * radius;
    float y = sinf(phi) * radius;
    float z = sqrtf(fmaxf(0.0f, 1.0f - r2));

    mcx_build_basis(normal, &tangent, &bitangent);
    return v3_normalize(v3_add(v3_add(v3_mul1(tangent, x), v3_mul1(bitangent, y)), v3_mul1(normal, z)));
}

static vec3_t mcx_sample_glossy_lobe(vec3_t reflected, vec3_t normal, float roughness, uint* state)
{
    vec3_t direction = v3_normalize(v3_add(reflected, v3_mul1(mcx_rand_dir(state), roughness)));

    if (v3_dot(direction, normal) <= 0.0f) {
        return reflected;
    }

    return direction;
}

static float mcx_schlick(float cosine, float eta_i, float eta_t)
{
    float r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

static vec3_t mcx_beer_lambert(vec3_t absorption, float distance)
{
    return vec3(
        expf(-absorption.x * distance),
        expf(-absorption.y * distance),
        expf(-absorption.z * distance));
}

static vec3_t mcx_sky_color(vec3_t rd)
{
    float t = saturate(0.5f + 0.5f * rd.y);
    vec3_t horizon = vec3(0.48f, 0.30f, 0.22f);
    vec3_t zenith = vec3(0.10f, 0.20f, 0.36f);
    return v3_lerp(horizon, zenith, t);
}

static vec3_t mcx_floor_albedo(vec3_t point)
{
    float swirl = 0.5f + 0.5f * sinf(point.x * 5.5f + sinf(point.z * 2.4f) * 0.7f);
    float vein = smoothstepf(0.25f, 0.85f, swirl);
    return v3_lerp(vec3(0.46f, 0.43f, 0.40f), vec3(0.80f, 0.76f, 0.70f), 0.16f + 0.36f * vein);
}

static mcx_hit_t mcx_make_empty_hit(void)
{
    mcx_hit_t hit;
    hit.surface_type = MCX_SURFACE_NONE;
    hit.light_index = -1;
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

static void mcx_set_hit_rect_light(mcx_hit_t* hit, vec3_t ro, vec3_t rd, int light_index)
{
    const mcx_rect_light_t *light = &MCX_LIGHTS[light_index];
    float distance;
    vec3_t point;
    vec3_t offset;
    float local_x;
    float local_y;

    if (v3_dot(rd, light->normal) >= 0.0f) {
        return;
    }

    distance = mcx_ray_plane(ro, rd, light->center, light->normal);
    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    point = v3_add(ro, v3_mul1(rd, distance));
    offset = v3_sub(point, light->center);
    local_x = v3_dot(offset, light->tangent);
    local_y = v3_dot(offset, light->bitangent);
    if (fabsf(local_x) > light->half_width || fabsf(local_y) > light->half_height) {
        return;
    }

    hit->surface_type = MCX_SURFACE_LIGHT;
    hit->light_index = light_index;
    hit->is_front_face = true;
    hit->normal = light->normal;
    hit->distance = distance;
    hit->albedo = vec3_o(0.0f);
    hit->emission = light->emission;
    hit->absorption = vec3_o(0.0f);
    hit->roughness = 0.0f;
    hit->ior = 1.0f;
}

static void mcx_set_hit_plane(
    mcx_hit_t* hit,
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
    float distance = mcx_ray_plane(ro, rd, plane_point, plane_normal);

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = surface_type;
    hit->light_index = -1;
    hit->is_front_face = v3_dot(rd, plane_normal) < 0.0f;
    hit->normal = hit->is_front_face ? plane_normal : v3_mul1(plane_normal, -1.0f);
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
    hit->ior = ior;
    hit->absorption = absorption;
}

static void mcx_set_hit_floor(mcx_hit_t* hit, vec3_t ro, vec3_t rd)
{
    float distance = mcx_ray_plane(ro, rd, vec3(0.0f, -1.18f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    hit->surface_type = MCX_SURFACE_DIFFUSE;
    hit->light_index = -1;
    hit->is_front_face = true;
    hit->normal = vec3(0.0f, 1.0f, 0.0f);
    hit->distance = distance;
    hit->albedo = mcx_floor_albedo(v3_add(ro, v3_mul1(rd, distance)));
    hit->emission = vec3_o(0.0f);
    hit->absorption = vec3_o(0.0f);
    hit->roughness = 0.0f;
    hit->ior = 1.0f;
}

static void mcx_set_hit_sphere(
    mcx_hit_t* hit,
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
    float distance = mcx_ray_sphere(ro, rd, center, radius);
    vec3_t point;
    vec3_t outward_normal;

    if (distance <= 0.0f || distance >= hit->distance) {
        return;
    }

    point = v3_add(ro, v3_mul1(rd, distance));
    outward_normal = v3_normalize(v3_sub(point, center));

    hit->surface_type = surface_type;
    hit->light_index = -1;
    hit->is_front_face = v3_dot(rd, outward_normal) < 0.0f;
    hit->normal = hit->is_front_face ? outward_normal : v3_mul1(outward_normal, -1.0f);
    hit->distance = distance;
    hit->albedo = albedo;
    hit->emission = emission;
    hit->roughness = roughness;
    hit->ior = ior;
    hit->absorption = absorption;
}

static mcx_hit_t mcx_intersect_scene(vec3_t ro, vec3_t rd)
{
    mcx_hit_t hit = mcx_make_empty_hit();

    for (int light_index = 0; light_index < MCX_LIGHT_COUNT; light_index++) {
        mcx_set_hit_rect_light(&hit, ro, rd, light_index);
    }

    mcx_set_hit_floor(&hit, ro, rd);
    mcx_set_hit_plane(&hit, ro, rd, vec3(0.0f, 1.70f, 0.0f), vec3(0.0f, -1.0f, 0.0f), MCX_SURFACE_DIFFUSE, vec3(0.85f, 0.87f, 0.93f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    mcx_set_hit_plane(&hit, ro, rd, vec3(-2.30f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), MCX_SURFACE_DIFFUSE, vec3(0.92f, 0.25f, 0.18f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    mcx_set_hit_plane(&hit, ro, rd, vec3(2.30f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f), MCX_SURFACE_DIFFUSE, vec3(0.14f, 0.46f, 0.90f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    mcx_set_hit_plane(&hit, ro, rd, vec3(0.0f, 0.0f, 5.10f), vec3(0.0f, 0.0f, -1.0f), MCX_SURFACE_DIFFUSE, vec3(0.79f, 0.81f, 0.87f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));

    mcx_set_hit_sphere(&hit, ro, rd, vec3(-0.62f, -0.33f, 2.28f), 0.83f, MCX_SURFACE_GLASS, vec3_o(1.0f), vec3_o(0.0f), 0.0f, 1.48f, vec3(0.28f, 0.09f, 0.03f));
    mcx_set_hit_sphere(&hit, ro, rd, vec3(1.18f, -0.60f, 1.95f), 0.57f, MCX_SURFACE_METAL, vec3(0.99f, 0.79f, 0.48f), vec3_o(0.0f), 0.08f, 1.0f, vec3_o(0.0f));
    mcx_set_hit_sphere(&hit, ro, rd, vec3(0.24f, -0.78f, 2.86f), 0.30f, MCX_SURFACE_DIFFUSE, vec3(0.87f, 0.80f, 0.73f), vec3_o(0.0f), 0.0f, 1.0f, vec3_o(0.0f));
    mcx_set_hit_sphere(&hit, ro, rd, vec3(-1.58f, -0.94f, 2.72f), 0.28f, MCX_SURFACE_METAL, vec3(0.92f, 0.95f, 0.99f), vec3_o(0.0f), 0.02f, 1.0f, vec3_o(0.0f));

    return hit;
}

static vec3_t mcx_sample_light_point(const mcx_rect_light_t *light, uint* state)
{
    float sx = lerpf(-light->half_width, light->half_width, mcx_rand_1(state));
    float sy = lerpf(-light->half_height, light->half_height, mcx_rand_1(state));

    return v3_add(
        light->center,
        v3_add(
            v3_mul1(light->tangent, sx),
            v3_mul1(light->bitangent, sy)));
}

static float mcx_light_area(const mcx_rect_light_t *light)
{
    return (light->half_width * 2.0f) * (light->half_height * 2.0f);
}

static vec3_t mcx_estimate_direct_light(vec3_t point, vec3_t normal, vec3_t albedo, uint* state)
{
    int light_index = (int)(mcx_rand_1(state) * (float)MCX_LIGHT_COUNT);
    const mcx_rect_light_t *light;
    vec3_t light_point;
    vec3_t to_light;
    float distance_sq;
    float distance;
    vec3_t direction;
    float cos_surface;
    float cos_light;
    float weight;
    mcx_hit_t shadow_hit;

    if (light_index < 0) {
        light_index = 0;
    }
    if (light_index >= MCX_LIGHT_COUNT) {
        light_index = MCX_LIGHT_COUNT - 1;
    }

    light = &MCX_LIGHTS[light_index];
    light_point = mcx_sample_light_point(light, state);
    to_light = v3_sub(light_point, point);
    distance_sq = v3_length_sq(to_light);
    if (distance_sq <= 0.000001f) {
        return vec3_o(0.0f);
    }

    distance = sqrtf(distance_sq);
    direction = v3_div1(to_light, distance);
    cos_surface = saturate(v3_dot(normal, direction));
    cos_light = saturate(v3_dot(light->normal, v3_mul1(direction, -1.0f)));

    if (cos_surface <= 0.0f || cos_light <= 0.0f) {
        return vec3_o(0.0f);
    }

    shadow_hit = mcx_intersect_scene(v3_add(point, v3_mul1(normal, MCX_LIGHT_EPSILON)), direction);
    if (shadow_hit.surface_type != MCX_SURFACE_LIGHT || shadow_hit.light_index != light_index || fabsf(shadow_hit.distance - distance) > 0.03f) {
        return vec3_o(0.0f);
    }

    weight = (cos_surface * cos_light * mcx_light_area(light) * (float)MCX_LIGHT_COUNT) / (distance_sq * PI);
    return v3_mul1(v3_mul(albedo, light->emission), weight);
}

static float mcx_max_component(vec3_t value)
{
    return fmaxf(value.x, fmaxf(value.y, value.z));
}

static vec3_t mcx_trace(vec3_t ro, vec3_t rd, uint* state)
{
    const float eps = 0.0005f;
    vec3_t radiance = vec3_o(0.0f);
    vec3_t throughput = vec3_o(1.0f);
    bool last_bounce_specular = true;

    for (int bounce = 0; bounce < MCX_MAX_BOUNCES; bounce++) {
        mcx_hit_t hit = mcx_intersect_scene(ro, rd);
        vec3_t point;

        if (hit.distance > 9999.0f) {
            radiance = v3_add(radiance, v3_mul(throughput, mcx_sky_color(rd)));
            break;
        }

        point = v3_add(ro, v3_mul1(rd, hit.distance));
        if (last_bounce_specular || bounce == 0) {
            radiance = v3_add(radiance, v3_mul(throughput, hit.emission));
        }

        if (hit.surface_type == MCX_SURFACE_LIGHT) {
            break;
        }

        if (hit.surface_type == MCX_SURFACE_DIFFUSE) {
            radiance = v3_add(radiance, v3_mul(throughput, mcx_estimate_direct_light(point, hit.normal, hit.albedo, state)));
            throughput = v3_mul(throughput, hit.albedo);
            rd = mcx_sample_cosine_hemisphere(hit.normal, state);
            ro = v3_add(point, v3_mul1(hit.normal, eps));
            last_bounce_specular = false;
        } else if (hit.surface_type == MCX_SURFACE_METAL) {
            vec3_t reflected = v3_reflect(rd, hit.normal);
            throughput = v3_mul(throughput, hit.albedo);
            rd = mcx_sample_glossy_lobe(reflected, hit.normal, hit.roughness, state);
            ro = v3_add(point, v3_mul1(hit.normal, eps));
            last_bounce_specular = true;
        } else if (hit.surface_type == MCX_SURFACE_GLASS) {
            float eta_i = hit.is_front_face ? 1.0f : hit.ior;
            float eta_t = hit.is_front_face ? hit.ior : 1.0f;
            float cosine = saturate(v3_dot(v3_mul1(rd, -1.0f), hit.normal));
            vec3_t reflected = v3_reflect(rd, hit.normal);
            vec3_t refracted = v3_refract(rd, hit.normal, eta_i / eta_t);
            bool can_refract = v3_length_sq(refracted) > 0.0f;
            float reflectance = can_refract ? mcx_schlick(cosine, eta_i, eta_t) : 1.0f;

            if (!hit.is_front_face) {
                throughput = v3_mul(throughput, mcx_beer_lambert(hit.absorption, hit.distance));
            }

            if (mcx_rand_1(state) < reflectance) {
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
            float survive = clampf(mcx_max_component(throughput), 0.10f, 0.95f);
            if (mcx_rand_1(state) > survive) {
                break;
            }

            throughput = v3_div1(throughput, survive);
        }
    }

    return radiance;
}

static vec3_t mcx_render_linear_sample(vec2_t fragCoord, vec2_t resolution, uint* state)
{
    vec2_t jitter = vec2(mcx_rand_1(state) - 0.5f, mcx_rand_1(state) - 0.5f);
    vec2_t pixel = vec2(fragCoord.x + jitter.x, fragCoord.y + jitter.y);
    vec2_t uv = vec2(pixel.x - resolution.x * 0.5f, pixel.y - resolution.y * 0.5f);
    vec3_t ray_origin;
    vec3_t target;
    vec3_t forward;
    vec3_t right;
    vec3_t up;
    vec3_t ray_dir;

    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    ray_origin = vec3(0.15f, 0.02f, -1.95f);
    target = vec3(0.02f, -0.35f, 2.15f);
    forward = v3_normalize(v3_sub(target, ray_origin));
    right = v3_normalize(v3_cross(vec3(0.0f, 1.0f, 0.0f), forward));
    up = v3_cross(forward, right);
    ray_dir = v3_normalize(v3_add(forward, v3_add(v3_mul1(right, uv.x * 1.28f), v3_mul1(up, uv.y * 1.28f))));

    return mcx_trace(ray_origin, ray_dir, state);
}

static vec3_t mcx_grade_hdr_linear(vec3_t color)
{
    return v3_mul1(color, 1.08f);
}
