#include "status_indicator.h"

#include "u_vars.h"

#include <stddef.h>

const shader_variable_desc_t g_status_indicator_variables[STATUS_INDICATOR_VARIABLE_COUNT] = {
    {"color",       SHADER_VARIABLE_TYPE_VEC3,  offsetof(status_indicator_vars_t, color),       "0.18, 0.85, 0.32"},
    {"brightness",  SHADER_VARIABLE_TYPE_FLOAT, offsetof(status_indicator_vars_t, brightness),  "0.85"},
    {"bezel_color", SHADER_VARIABLE_TYPE_VEC3,  offsetof(status_indicator_vars_t, bezel_color), "0.50, 0.52, 0.56"},
};

static const status_indicator_vars_t g_status_indicator_fallback = {
    {0.18f, 0.85f, 0.32f},
    0.85f,
    {0.50f, 0.52f, 0.56f},
};

static float circle_sdf(vec2_t p, float radius)
{
    return v2_length(p) - radius;
}

vec4_t status_indicator_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const status_indicator_vars_t *vars = U_VARS_AS(status_indicator_vars_t, uniforms);
    vec4_t result = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vec2_t resolution;
    float min_axis, aa;
    vec2_t uv;
    float brightness;
    vec3_t led_color, bezel_color;

    /* Geometry constants. */
    float outer_radius = 0.88f;
    float bezel_width  = 0.12f;
    float dome_radius  = outer_radius - bezel_width;

    float dist;
    float outer_mask, dome_mask, bezel_mask;

    /* Bezel lighting. */
    float bezel_point, bezel_ndotl;
    vec3_t bezel_surface;

    /* Dome lighting. */
    float r_norm, nz;
    float dome_diffuse, dome_spec, dome_fresnel;
    vec3_t dome_ambient, dome_lit, dome_emissive, dome_surface;

    /* Glow. */
    float glow_dist, glow_mask;

    if (vars == NULL) {
        vars = &g_status_indicator_fallback;
    }

    resolution = (uniforms != NULL) ? uniforms->resolution : vec2(80.0f, 80.0f);
    min_axis   = max(1.0f, min(resolution.x, resolution.y));
    uv = vec2(
        ((fragCoord.x + 0.5f) * 2.0f - resolution.x) / min_axis,
        ((fragCoord.y + 0.5f) * 2.0f - resolution.y) / min_axis);
    aa = (2.0f / min_axis) * 1.5f;

    brightness  = clampf(vars->brightness, 0.0f, 1.0f);
    led_color   = v3_saturate(vars->color);
    bezel_color = v3_saturate(vars->bezel_color);

    dist = v2_length(uv);

    /* --- Soft shadow behind the whole unit --- */
    {
        float shadow_sdf = circle_sdf(
            v2_sub(uv, vec2(0.02f, -0.04f)), outer_radius * 1.06f);
        float shadow = shader_sdf_fill(shadow_sdf, aa * 4.0f) * 0.18f;
        result = shader_layer_over(result, vec4(0.02f, 0.02f, 0.04f, shadow));
    }

    /* --- Glow halo (drawn behind bezel, visible outside) --- */
    glow_dist = circle_sdf(uv, outer_radius * 1.02f);
    glow_mask = shader_sdf_fill(glow_dist, aa * 8.0f);
    glow_mask *= (1.0f - shader_sdf_fill(circle_sdf(uv, outer_radius), aa));
    glow_mask *= brightness * 0.25f;
    result = shader_layer_over(result,
        vec4(led_color.x, led_color.y, led_color.z, glow_mask));

    /* --- Metal bezel ring --- */
    outer_mask = shader_sdf_fill(circle_sdf(uv, outer_radius), aa);
    dome_mask  = shader_sdf_fill(circle_sdf(uv, dome_radius), aa);
    bezel_mask = saturate(outer_mask - dome_mask);

    /*
     * Bezel lighting: approximate a toroidal cross-section.
     * Map radial position within the bezel to a [-1,1] profile,
     * then compute dot(normal, light) for a directional light.
     */
    bezel_point = clampf(
        (dist - dome_radius) / max(bezel_width, 0.001f), 0.0f, 1.0f);
    bezel_point = bezel_point * 2.0f - 1.0f;   /* [-1, 1] */

    {
        /* Normal: blend radial outward direction with profile tilt. */
        float nx = (dist > 0.001f) ? uv.x / dist : 0.0f;
        float ny = (dist > 0.001f) ? uv.y / dist : 0.0f;
        float profile_tilt = bezel_point * 0.6f;

        /* light_dir ≈ normalize(0.45, 0.55, 0.75) */
        bezel_ndotl = saturate(
            (nx * profile_tilt) * 0.45f +
            (ny * profile_tilt) * 0.55f +
            0.75f * (1.0f - fabsf(bezel_point) * 0.4f));
    }

    bezel_surface = v3_mul1(bezel_color, 0.28f + 0.52f * bezel_ndotl);

    /* Specular band along the inner edge of the bezel. */
    {
        float inner_spec = powf(
            saturate(1.0f - fabsf(bezel_point + 0.6f) * 2.5f), 8.0f) * 0.30f;
        bezel_surface = v3_add(bezel_surface,
            vec3(inner_spec, inner_spec, inner_spec));
    }

    /* Outer edge darkening. */
    {
        float edge_dark = smoothstepf(0.6f, 1.0f, bezel_point) * 0.10f;
        bezel_surface = v3_sub(bezel_surface,
            vec3(edge_dark, edge_dark, edge_dark));
    }

    result = shader_layer_over(result,
        vec4(saturate(bezel_surface.x),
             saturate(bezel_surface.y),
             saturate(bezel_surface.z),
             bezel_mask));

    /* --- Glass dome with emissive LED --- */

    /*
     * Hemisphere normal: nx,ny from pixel position, nz from sqrt(1-r²).
     * When brightness = 0, the dome is a dark neutral sphere.
     * When brightness = 1, the dome is a brightly lit, colored LED.
     */
    r_norm = clampf(dist / dome_radius, 0.0f, 1.0f);
    {
        float r2 = r_norm * r_norm;
        nz = (r2 < 1.0f) ? (1.0f - r2) : 0.0f;
    }

    /* Diffuse: light from upper-left. */
    dome_diffuse = saturate(
        uv.x / dome_radius * 0.35f +
        uv.y / dome_radius * 0.45f +
        nz * 0.65f);

    /* Specular highlight: concentrated near upper-left. */
    {
        float sx = uv.x / dome_radius - 0.30f;
        float sy = uv.y / dome_radius - 0.35f;
        dome_spec = powf(saturate(1.0f - (sx * sx + sy * sy) * 3.5f), 24.0f)
                  * 0.55f;
    }

    /* Fresnel brightening at dome edge. */
    dome_fresnel = powf(r_norm, 5.0f) * 0.25f;

    /* Dark dome (brightness = 0): neutral gray sphere. */
    dome_ambient = vec3(0.06f, 0.07f, 0.08f);
    dome_lit = v3_add(dome_ambient,
        v3_mul1(vec3(0.08f, 0.09f, 0.10f), dome_diffuse));

    /* Emissive dome (brightness > 0): LED color floods through. */
    dome_emissive = v3_mul1(led_color, brightness);
    dome_emissive = v3_add(dome_emissive,
        v3_mul1(led_color, brightness * dome_diffuse * 0.30f));

    /* Center pool: brighter at dome apex. */
    {
        float center_bright = powf(saturate(1.0f - r_norm), 1.8f)
                            * brightness * 0.25f;
        dome_emissive = v3_add(dome_emissive,
            v3_mul1(led_color, center_bright));
    }

    dome_surface = v3_lerp(dome_lit, dome_emissive, brightness);

    /* Glass specular and fresnel on top of everything. */
    dome_surface = v3_add(dome_surface,
        vec3(dome_spec, dome_spec, dome_spec));
    dome_surface = v3_add(dome_surface,
        v3_mul1(vec3(1.0f, 1.0f, 1.0f), dome_fresnel * (0.10f + 0.15f * brightness)));

    result = shader_layer_over(result,
        vec4(saturate(dome_surface.x),
             saturate(dome_surface.y),
             saturate(dome_surface.z),
             dome_mask));

    return vec4(saturate(result.x), saturate(result.y),
                saturate(result.z), saturate(result.w));
}
