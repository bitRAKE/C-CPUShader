//Glass Disks Tracing Shader

#include "glass_disks.h"

#define MAX_BOUNCES 4

typedef struct {
    int surface_type;
    vec3_t color;
    vec2_t normal;
    float distance;
} HitData;

static float ray_circle(vec2_t ro, vec2_t rd, vec2_t c, float r)
{
    vec2_t oc = v2_sub(ro, c);

    float b = v2_dot(oc, rd);
    float c0 = v2_dot(oc, oc) - r * r;
    float h = b * b - c0;

    if (h < 0.0) return -1.0;

    h = sqrt(h);

    float t = -b - h;
    if (t < 0.0) t = -b + h;
    if (t < 0.0) return -1.0;

    return t;
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
    return next_rand(state) / 4294967295.0;
}

static float rand_1_nd(uint* state)
{
    float theta = 2 * 3.1415926 * rand_1(state);
    float rho = sqrt(-2 * log(rand_1(state)));
    return rho * cos(theta);
}

__attribute__((unused))
static vec2_t rand_dir(uint* state)
{
    float x = rand_1_nd(state);
    float y = rand_1_nd(state);
    return v2_normalize(vec2(x, y));
}

#define NEAR_DISK_COUNT 3
#define FAR_DISK_COUNT 6

static HitData intersect(vec2_t ro, vec2_t rd)
{
    HitData hit;
    hit.distance = 10000.0;
    
    float light = ray_circle(ro, rd, vec2(-0.6, 0.0), 0.03);
    if(light > 0.0)
    {
        hit.distance = light;
        hit.color = vec3_o(6.0);
        hit.surface_type = 0;
    }
    
    for(int i = 0; i < NEAR_DISK_COUNT; i++)
    {
        float angle = 2 * PI * (float)i / NEAR_DISK_COUNT;
        vec2_t pos = v2_mul1(vec2(cos(angle), sin(angle)), 0.125);
        float radius = 0.075;
        
        float t = ray_circle(ro, rd, pos, radius);
        
        if(t < hit.distance && t > 0.0)
        {
            hit.distance = light;
            
            bool isEntering = v2_length(v2_sub(ro, pos)) > radius;
                
            vec2_t p = v2_add(ro, v2_mul1(rd, t));

            hit.normal = v2_mul1(v2_normalize(v2_sub(p, pos)), (isEntering ? 1.0 : -1.0));
            hit.surface_type = isEntering ? 1 : 2;
        }
    }
    
    for(int i = 0; i < FAR_DISK_COUNT; i++)
    {
        float angle = 2 * PI * (i + 0.25) / FAR_DISK_COUNT;
        vec2_t pos = v2_mul1(vec2(cos(angle), sin(angle)), 0.32);
        float radius = 0.1;
        
        float t = ray_circle(ro, rd, pos, radius);
        
        if(t < hit.distance && t > 0.0)
        {
            hit.distance = light;
            
            bool isEntering = v2_length(v2_sub(ro, pos)) > radius;
                
            vec2_t p = v2_add(ro, v2_mul1(rd, t));

            hit.normal = v2_mul1(v2_normalize(v2_sub(p, pos)), (isEntering ? 1.0 : -1.0));
            hit.surface_type = isEntering ? 1 : 2;
        }
    }
    
    return hit;
}

static float get_reflectance(vec2_t i, vec2_t t, vec2_t nor, float iora, float iorb)
{
    float cosi = v2_dot(i, nor);
    float cost = v2_dot(t, nor);
    float rs = powf((iora * cosi - iorb * cost) / (iora * cosi + iorb * cost), 2.0f);
    float rp = powf((iorb * cosi - iora * cost) / (iorb * cosi + iora * cost), 2.0f);
    return (rs + rp) * 0.5f;
}

static vec3_t trace(vec2_t ro, vec2_t rd, float ior, uint* state)
{
    const float eps = 0.0001;
    vec3_t transmittance = vec3_o(1.0);

    for(int i = 0; i < MAX_BOUNCES; i++)
    {
        HitData hit = intersect(ro, rd);

        if(hit.distance > 9999.0) {
            break;
        }

        if(hit.surface_type == 0)
        {
            return v3_mul(hit.color, transmittance);
        }

        if(hit.surface_type == 1 || hit.surface_type == 2)
        {
            bool isEntering = hit.surface_type == 1;//entering or exiting the glass
            ro = v2_add(ro, v2_mul1(rd, hit.distance));

            vec2_t reflected = v2_reflect(rd, hit.normal);
            vec2_t refracted = v2_refract(rd, hit.normal, isEntering ? 1.0/ior : ior);
            float reflectance  = get_reflectance(rd, refracted, hit.normal, isEntering ? 1.0 : ior, isEntering ? ior : 1.0);
            bool reflect_it = rand_1(state) < reflectance;
            
            if(!isEntering){
                //Absorbtion
                //trasmittance *= exp(-hit.distance * hit.color * ABSORBTION_STRENGTH);
            }

            if(reflect_it)
            {
                ro = v2_add(ro, v2_mul1(hit.normal, eps));
                rd = reflected;
                transmittance = v3_mul1(transmittance, reflectance);
            }
            else{
                ro = v2_sub(ro, v2_mul1(hit.normal, eps));
                rd = refracted;
                transmittance = v3_mul1(transmittance, 1.0 - reflectance);
            }
        }
    }

    return vec3_o(0.0);
}

vec3_t get_dispersed_color( float w ) {
    float r = sin(w * PI * 2.0);
    float g = sin((w - 0.25) * PI * 2.0);
    float b = sin((w - 0.5) * PI * 2.0);
    return vec3(saturate(r), saturate(g), saturate(b));
}

#define DIRECTIONAL_SAMPLES 360

vec4_t glass_disks_main(vec2_t fragCoord, const shader_uniforms_t* uniforms) {
    const vec2_t resolution = uniforms->resolution;
    const uint frame = uniforms->frame;

    vec2_t uv = vec2(fragCoord.x - resolution.x * 0.5, fragCoord.y - resolution.y * 0.5);
    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    uint state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) + frame * 78423;

    float angle = 2 * PI * (frame + rand_1(&state)) / DIRECTIONAL_SAMPLES;
    vec2_t dir = vec2(cos(angle), sin(angle));

    float spec = rand_1(&state);
    float ior = 1.3 + 0.2 * spec;
    vec3_t color_mask = get_dispersed_color(spec);

    vec3_t t = trace(uv, dir, ior, &state);
    t = v3_mul(t, color_mask);

    return vec4(t.x, t.y, t.z, 1.0);
}
