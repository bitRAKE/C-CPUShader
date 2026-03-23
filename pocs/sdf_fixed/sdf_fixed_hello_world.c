#include "sdf_fixed_hello_world.h"

#include "u_texture.h"

#ifdef SHADER_PLUGIN_BUILD
#include "shader_host_services.h"
HMODULE plugin_get_module(void);
#endif

#define SDF_GRID_TEXTURE_LABEL "ascii_sdf_grid"
#define SDF_GRID_TEXTURE_RESOURCE "ASCII_SDF_GRID_PNG"
#define SDF_GRID_TEXTURE_PATH "pocs/sdf_fixed/ascii_sdf_grid.png"

#define SDF_GRID_ATLAS_WIDTH 1024.0f
#define SDF_GRID_ATLAS_HEIGHT 576.0f
#define SDF_GRID_CELL_WIDTH 64.0f
#define SDF_GRID_CELL_HEIGHT 96.0f
#define SDF_GRID_COLUMNS 16
#define SDF_GRID_ROWS 6
#define SDF_GRID_SPACE_CHAR 32
#define SDF_GRID_FIRST_DRAWN_CHAR 33
#define SDF_GRID_LAST_CHAR 126
#define SDF_GRID_EM_SIZE 64.0f
#define SDF_GRID_PX_RANGE 6.1f
#define SDF_GRID_QUAD_ASPECT (SDF_GRID_CELL_WIDTH / SDF_GRID_CELL_HEIGHT)
#define SDF_GRID_ADVANCE_RATIO 0.52f

static vec3_t sdf_fixed_background(vec2_t fragCoord, vec2_t resolution)
{
    vec2_t centered = vec2(
        (fragCoord.x - resolution.x * 0.5f) / resolution.y,
        (fragCoord.y - resolution.y * 0.5f) / resolution.y);
    float vignette = saturate(1.1f - v2_dot(centered, centered) * 1.55f);
    float sweep = 0.5f + 0.5f * sinf(centered.x * 5.5f - centered.y * 2.8f);
    vec3_t dark = vec3(0.025f, 0.045f, 0.085f);
    vec3_t mid = vec3(0.060f, 0.100f, 0.180f);
    vec3_t glow = vec3(0.100f, 0.180f, 0.300f);
    vec3_t color = v3_lerp(dark, mid, sweep * 0.35f + 0.2f);

    color = v3_add(color, v3_mul1(glow, vignette * 0.22f));
    color = v3_mul1(color, vignette);
    return color;
}

static vec3_t sdf_fixed_blend_over(vec3_t base, vec3_t layer, float alpha)
{
    return v3_lerp(base, layer, saturate(alpha));
}

static bool sdf_fixed_cell_uv(unsigned char ch, vec2_t local_uv, vec2_t *uv_out)
{
    int index;
    int column;
    int row;

    if (uv_out == NULL) {
        return false;
    }

    if (ch == SDF_GRID_SPACE_CHAR) {
        return false;
    }

    if (ch < SDF_GRID_FIRST_DRAWN_CHAR || ch > SDF_GRID_LAST_CHAR) {
        return false;
    }

    index = (int)ch - SDF_GRID_FIRST_DRAWN_CHAR;
    column = index % SDF_GRID_COLUMNS;
    row = index / SDF_GRID_COLUMNS;

    uv_out->x = ((float)column * SDF_GRID_CELL_WIDTH + local_uv.x * SDF_GRID_CELL_WIDTH) / SDF_GRID_ATLAS_WIDTH;
    uv_out->y = (((float)(row + 1) * SDF_GRID_CELL_HEIGHT) - local_uv.y * SDF_GRID_CELL_HEIGHT) / SDF_GRID_ATLAS_HEIGHT;
    return true;
}

static bool sdf_fixed_sample(
    const shader_buffer_t *atlas,
    unsigned char ch,
    vec2_t point,
    float x0,
    float y0,
    float x1,
    float y1,
    float quad_height_px,
    float *distance_px_out,
    vec2_t *local_uv_out)
{
    vec2_t local_uv;
    vec2_t atlas_uv;
    vec4_t texel;
    float screen_px_range;

    if (distance_px_out == NULL || local_uv_out == NULL || !u_texture_valid(atlas)) {
        return false;
    }

    if (point.x < x0 || point.x > x1 || point.y < y0 || point.y > y1) {
        return false;
    }

    local_uv.x = (point.x - x0) / fmaxf(x1 - x0, 0.0001f);
    local_uv.y = (point.y - y0) / fmaxf(y1 - y0, 0.0001f);

    if (!sdf_fixed_cell_uv(ch, local_uv, &atlas_uv)) {
        return false;
    }

    texel = u_texture_sample_bilinear(atlas, atlas_uv, U_TEXTURE_ADDRESS_CLAMP);
    screen_px_range = fmaxf(SDF_GRID_PX_RANGE * quad_height_px / SDF_GRID_EM_SIZE, 1.0f);

    *distance_px_out = (texel.x - 0.5f) * screen_px_range;
    *local_uv_out = local_uv;
    return true;
}

static float sdf_fixed_text_width(const char *text, float font_px_size)
{
    float quad_width;
    float advance;
    int count = 0;

    if (text == NULL || text[0] == '\0') {
        return 0.0f;
    }

    while (text[count] != '\0') {
        count++;
    }

    quad_width = font_px_size * SDF_GRID_QUAD_ASPECT;
    advance = font_px_size * SDF_GRID_ADVANCE_RATIO;
    return quad_width + (float)(count - 1) * advance;
}

static void sdf_fixed_draw_text(
    vec3_t *color,
    const shader_uniforms_t *uniforms,
    vec2_t fragCoord,
    const char *text,
    float center_x,
    float center_y,
    float font_px_size,
    float outline_width_px,
    float shadow_offset_px,
    float shadow_alpha,
    float fill_alpha,
    float outline_alpha,
    float hue_bias)
{
    const shader_buffer_t *atlas = u_buffer_named(uniforms, SDF_GRID_TEXTURE_LABEL);
    float quad_width;
    float advance;
    float total_width;
    float x;
    float y0;

    if (color == NULL || text == NULL || !u_texture_valid(atlas)) {
        return;
    }

    quad_width = font_px_size * SDF_GRID_QUAD_ASPECT;
    advance = font_px_size * SDF_GRID_ADVANCE_RATIO;
    total_width = sdf_fixed_text_width(text, font_px_size);
    x = center_x - total_width * 0.5f;
    y0 = center_y - font_px_size * 0.5f;

    for (int index = 0; text[index] != '\0'; index++) {
        float x0 = x;
        float x1 = x0 + quad_width;
        float y1 = y0 + font_px_size;
        float shadow_distance_px;
        float distance_px;
        vec2_t shadow_uv;
        vec2_t local_uv;

        if (sdf_fixed_sample(
                atlas,
                (unsigned char)text[index],
                vec2(fragCoord.x - shadow_offset_px, fragCoord.y - shadow_offset_px),
                x0,
                y0,
                x1,
                y1,
                font_px_size,
                &shadow_distance_px,
                &shadow_uv))
        {
            float shadow = saturate(shadow_distance_px + outline_width_px + 0.5f) * shadow_alpha;
            *color = sdf_fixed_blend_over(*color, vec3(0.01f, 0.02f, 0.05f), shadow);
        }

        if (sdf_fixed_sample(
                atlas,
                (unsigned char)text[index],
                fragCoord,
                x0,
                y0,
                x1,
                y1,
                font_px_size,
                &distance_px,
                &local_uv))
        {
            float fill = saturate(distance_px + 0.5f);
            float outline = saturate(distance_px + outline_width_px + 0.5f) - fill;
            float highlight = fill * smoothstepf(1.0f, 0.0f, local_uv.y * 0.85f + local_uv.x * 0.20f) * 0.22f;
            float hue = hue_bias + (float)index * 0.08f + local_uv.x * 0.10f + local_uv.y * 0.05f;
            vec3_t fill_color;
            vec3_t outline_color;

            hue = hue - floorf(hue);
            fill_color = shader_hsv_to_rgb(hue, 0.74f, 1.0f);
            outline_color = v3_lerp(vec3(1.0f, 1.0f, 1.0f), fill_color, 0.15f);

            *color = sdf_fixed_blend_over(*color, outline_color, outline * outline_alpha);
            *color = sdf_fixed_blend_over(*color, fill_color, fill * fill_alpha);
            *color = sdf_fixed_blend_over(*color, vec3(1.0f, 1.0f, 1.0f), highlight);
        }

        x += advance;
    }
}

ShaderBuffersCleanupFunc sdf_fixed_hello_world_buffers_init(shader_buffers_t *buffers, const shader_host_services_t *services, char *error, size_t error_size)
{
#ifdef SHADER_PLUGIN_BUILD
    HMODULE module = plugin_get_module();
    if (services->load_texture_resource(
            buffers,
            SDF_GRID_TEXTURE_LABEL,
            module,
            SDF_GRID_TEXTURE_RESOURCE,
            SHADER_TEXEL_FORMAT_R8_UNORM,
            error,
            error_size) == NULL)
    {
        return NULL;
    }
    return services->default_cleanup;
#else
    (void)services;
    if (shader_buffers_load_texture_module_relative(
            buffers,
            SDF_GRID_TEXTURE_LABEL,
            SDF_GRID_TEXTURE_PATH,
            SHADER_TEXEL_FORMAT_R8_UNORM,
            error,
            error_size) == NULL)
    {
        return NULL;
    }
    return shader_buffers_default_cleanup;
#endif
}

vec4_t sdf_fixed_hello_world_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    vec3_t color = sdf_fixed_background(fragCoord, uniforms->resolution);

    sdf_fixed_draw_text(
        &color,
        uniforms,
        fragCoord,
        "SDF",
        uniforms->resolution.x * 0.52f,
        uniforms->resolution.y * 0.63f,
        uniforms->resolution.y * 0.72f,
        8.0f,
        16.0f,
        0.18f,
        0.18f,
        0.22f,
        0.58f);

    sdf_fixed_draw_text(
        &color,
        uniforms,
        fragCoord,
        "hello_world!",
        uniforms->resolution.x * 0.5f,
        uniforms->resolution.y * 0.30f,
        uniforms->resolution.y * 0.24f,
        4.0f,
        8.0f,
        0.30f,
        1.0f,
        0.90f,
        0.0f);

    return vec4(color.x, color.y, color.z, 1.0f);
}
