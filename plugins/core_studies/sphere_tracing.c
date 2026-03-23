//Sphere RayTracing Shader

#include "sphere_tracing.h"

#define MAX_BOUNCES 4
#define SPHERES_COUNT 7

static vec4_t SPHERES[SPHERES_COUNT] = {
    {1002.0, 0.0, 0.0, 1000.0},
    {-1002.0, 0.0, 0.0, 1000.0},
    {0.0, 1002.0, 0.0, 1000.0},
    {0.0, -1002.0, 0.0, 1000.0},
    {0.0, 0.0, 1002.0, 1000.0},
    {0.0, -1.0, 0.0, 0.5},
    {0.0, 0.5, 0.0, 0.25}
};

typedef struct {
    vec3_t emission;
    vec3_t color;
    vec3_t normal;
    float rougthness;
    float distance;
} HitData;

static HitData intersect(vec3_t ro, vec3_t rd)
{
    HitData hit = {vec3_o(0.0), vec3_o(1.0), vec3_o(0.0), 1.0, 10000.0};

    for (int i = 0; i < SPHERES_COUNT; i++)
    {
        vec3_t pos = vec3(SPHERES[i].x, SPHERES[i].y, SPHERES[i].z);
        float rad = SPHERES[i].w;

        float l = shader_ray_sphere(ro, rd, pos, rad);
                
        if (l < hit.distance && l > 0.001)
        {
            vec3_t p = v3_add(ro, v3_mul1(rd, l));
            vec3_t normal = v3_normalize(v3_sub(p, pos));
            hit.normal = normal;

            if(i == 0){
                hit.color = vec3(1.0, 0.0, 0.0);
            }
            else if(i == 1){
                hit.color = vec3(0.0, 1.0, 0.0);
            }
            else{
                hit.color = vec3_o(1.0);
            }

            if(i == 5){
                hit.rougthness = 0.0;
            }
            else{
                hit.rougthness = 1.0;
            }

            if(i == SPHERES_COUNT-1){
                hit.emission = vec3_o(20.0);
            }
            else{
                hit.emission = vec3_o(0.0);
            }

            hit.distance = l;
        }
    }

    return hit;
}

static vec3_t trace(vec3_t ro, vec3_t rd, uint* state)
{
    vec3_t ray_light = vec3_o(0.0);
    vec3_t ray_color = vec3_o(1.0);

    for(int i = 0; i < MAX_BOUNCES; i++)
    {
        HitData hit = intersect(ro, rd);

        if(hit.distance > 9999.0) {
            break;
        }

        ray_light = v3_add(ray_light, v3_mul(hit.emission, ray_color));
        ray_color = v3_mul(ray_color, hit.color);

        ro = v3_add(ro, v3_mul1(rd, hit.distance));

        vec3_t diffuse_dir = v3_normalize(v3_add(hit.normal, shader_rand_dir(state)));
        vec3_t specular_dir = v3_reflect(rd, hit.normal);
        rd = v3_normalize(v3_lerp(specular_dir, diffuse_dir, hit.rougthness));
    }

    return ray_light;
}

vec4_t sphere_tracing_main(vec2_t fragCoord, const shader_uniforms_t *uniforms) {
    const vec2_t resolution = uniforms->resolution;
    const uint frame = uniforms->frame;

    vec2_t uv = vec2(fragCoord.x - resolution.x * 0.5, fragCoord.y - resolution.y * 0.5);
    uv = vec2(uv.x / resolution.y, uv.y / resolution.y);

    vec3_t ray_origin = vec3(0, 0, -3.0);
    vec3_t ray_point = vec3(uv.x * 2, uv.y * 2, -1.75);
    vec3_t ray_dir = v3_normalize(v3_sub(ray_point, ray_origin));

    uint state = (uint)(fragCoord.x) + (uint)(fragCoord.y * resolution.x) + frame * 78423;
    vec3_t t = trace(ray_origin, ray_dir, &state);

    return vec4(t.x, t.y, t.z, 1.0);
}
