#include "mtsdf_hello_world.h"

#include "mtsdf_text.h"

static vec3_t mtsdf_hsv_to_rgb(float hue, float saturation, float value)
{
    float h = hue - floorf(hue);
    float x = h * 6.0f;
    float sector = floorf(x);
    float fraction = x - sector;
    float p = value * (1.0f - saturation);
    float q = value * (1.0f - saturation * fraction);
    float t = value * (1.0f - saturation * (1.0f - fraction));

    switch ((int)sector % 6) {
        case 0: return vec3(value, t, p);
        case 1: return vec3(q, value, p);
        case 2: return vec3(p, value, t);
        case 3: return vec3(p, q, value);
        case 4: return vec3(t, p, value);
        default: return vec3(value, p, q);
    }
}

static vec3_t mtsdf_blend_over(vec3_t base, vec3_t layer, float alpha)
{
    return v3_lerp(base, layer, saturate(alpha));
}

static vec3_t mtsdf_background(vec2_t fragCoord, vec2_t resolution)
{
    vec2_t centered = vec2(
        (fragCoord.x - resolution.x * 0.5f) / resolution.y,
        (fragCoord.y - resolution.y * 0.5f) / resolution.y);
    float vignette = saturate(1.12f - v2_dot(centered, centered) * 1.7f);
    float sweep = 0.5f + 0.5f * sinf(centered.x * 7.0f - centered.y * 3.0f);
    vec3_t low = vec3(0.06f, 0.03f, 0.11f);
    vec3_t high = vec3(0.14f, 0.07f, 0.24f);
    vec3_t glow = vec3(0.09f, 0.17f, 0.34f);
    vec3_t color = v3_lerp(low, high, sweep * 0.45f + 0.25f);

    color = v3_add(color, v3_mul1(glow, vignette * 0.28f));
    color = v3_mul1(color, vignette);
    return color;
}

typedef struct {
    float  shell_width_px;
    float  glow_width_px;
    float  shadow_offset_x;
    float  shadow_offset_y;
    float  shadow_alpha;
    float  shell_alpha;
    float  fill_alpha;
    float  highlight_alpha;
    float  inner_glow_alpha;
    float  saturation;
    float  value;
    float  hue_offset;
    float  hue_scale_x;
    float  hue_scale_y;
} mtsdf_bubble_style_t;

static void mtsdf_draw_bubble_run(
    vec3_t *color,
    vec2_t fragCoord,
    const shader_uniforms_t *uniforms,
    const char *text,
    float baseline_x,
    float baseline_y,
    float font_px_size,
    const mtsdf_bubble_style_t *style)
{
    const shader_buffer_t *atlas = u_buffer_named(uniforms, "ascii_mtsdf");
    const mtsdf_font_t *font = &g_mtsdf_ascii_font;
    float cursor_x;

    if (color == NULL || !u_texture_valid(atlas) || text == NULL || style == NULL) {
        return;
    }

    cursor_x = baseline_x;

    for (int index = 0; text[index] != '\0'; index++) {
        const mtsdf_glyph_t *glyph = mtsdf_find_glyph(font, (unsigned char)text[index]);
        mtsdf_glyph_quad_t quad;

        if (mtsdf_build_glyph_quad(font, glyph, cursor_x, baseline_y, font_px_size, &quad)) {
            mtsdf_sample_t sample;
            mtsdf_sample_t shadow_sample;

            if (mtsdf_sample_quad(
                    atlas,
                    font,
                    &quad,
                    vec2(fragCoord.x - style->shadow_offset_x, fragCoord.y - style->shadow_offset_y),
                    font_px_size,
                    &shadow_sample))
            {
                float shadow = mtsdf_expand_alpha(&shadow_sample, style->shell_width_px) * style->shadow_alpha;
                *color = mtsdf_blend_over(*color, vec3(0.03f, 0.04f, 0.10f), shadow);
            }

            if (mtsdf_sample_quad(atlas, font, &quad, fragCoord, font_px_size, &sample)) {
                float bubble = mtsdf_expand_alpha(&sample, style->shell_width_px);
                float inner_glow = mtsdf_expand_alpha(&sample, style->glow_width_px);
                float fill = sample.fill_alpha * style->fill_alpha;
                float shell = saturate(bubble - fill);
                float highlight = shell * smoothstepf(1.05f, 0.10f, sample.glyph_uv.y + sample.glyph_uv.x * 0.34f);
                float core_highlight = fill * smoothstepf(1.0f, 0.0f, sample.glyph_uv.y * 0.92f + sample.glyph_uv.x * 0.30f) * 0.18f;
                float hue = style->hue_offset + (float)index * 0.095f + sample.glyph_uv.x * style->hue_scale_x + sample.glyph_uv.y * style->hue_scale_y;
                hue = hue - floorf(hue);
                vec3_t fill_color = mtsdf_hsv_to_rgb(hue, style->saturation, style->value);
                vec3_t shell_color = v3_lerp(vec3(1.0f, 1.0f, 1.0f), fill_color, 0.18f);
                vec3_t rim_color = v3_lerp(shell_color, vec3(1.0f, 1.0f, 1.0f), 0.55f);

                *color = mtsdf_blend_over(*color, v3_mul1(shell_color, 1.06f), shell * style->shell_alpha);
                *color = mtsdf_blend_over(*color, v3_mul1(rim_color, 1.08f), highlight * style->highlight_alpha);
                *color = mtsdf_blend_over(*color, v3_mul1(fill_color, 1.04f), fill);
                *color = mtsdf_blend_over(*color, vec3(1.0f, 1.0f, 1.0f), core_highlight * style->highlight_alpha);
                *color = mtsdf_blend_over(*color, v3_mul1(fill_color, 1.20f), saturate(inner_glow - bubble) * style->inner_glow_alpha);
            }
        }

        cursor_x += quad.advance_px;
    }
}

static void mtsdf_draw_foreground_title(vec3_t *color, vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    static const char text[] = "hello_world!";
    mtsdf_text_bounds_t bounds;
    mtsdf_bubble_style_t style;
    float font_px_size;
    float baseline_x;
    float baseline_y;

    font_px_size = fminf(uniforms->resolution.x, uniforms->resolution.y) * 0.205f;
    bounds = mtsdf_measure_text(&g_mtsdf_ascii_font, text, font_px_size);
    baseline_x = uniforms->resolution.x * 0.5f - (bounds.left + bounds.right) * 0.5f;
    baseline_y = uniforms->resolution.y * 0.53f - (bounds.bottom + bounds.top) * 0.5f;

    style.shell_width_px = 6.5f;
    style.glow_width_px = 1.9f;
    style.shadow_offset_x = 10.0f;
    style.shadow_offset_y = 10.0f;
    style.shadow_alpha = 0.42f;
    style.shell_alpha = 0.92f;
    style.fill_alpha = 1.0f;
    style.highlight_alpha = 0.85f;
    style.inner_glow_alpha = 0.20f;
    style.saturation = 0.78f;
    style.value = 1.0f;
    style.hue_offset = 0.0f;
    style.hue_scale_x = 0.18f;
    style.hue_scale_y = 0.06f;
    mtsdf_draw_bubble_run(color, fragCoord, uniforms, text, baseline_x, baseline_y, font_px_size, &style);
}

static void mtsdf_draw_giant_showcase(vec3_t *color, vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    static const char source_text[] = "hello_world!";
    static const int layers = 3;
    float min_dim = fminf(uniforms->resolution.x, uniforms->resolution.y);
    float time = uniforms->time;

    for (int layer = 0; layer < layers; layer++) {
        char glyph_text[2];
        mtsdf_text_bounds_t bounds;
        mtsdf_bubble_style_t style;
        float phase = time * (0.33f + 0.07f * (float)layer) + (float)layer * 1.53f;
        float travel = 0.5f + 0.5f * sinf(phase);
        float sway = sinf(phase * 0.73f + 1.1f);
        float rise = cosf(phase * 0.61f - 0.35f);
        float font_px_size = min_dim * (0.72f + 0.45f * travel + 0.08f * (float)layer);
        float center_x = uniforms->resolution.x * (0.18f + 0.30f * (float)layer) + sway * uniforms->resolution.x * 0.07f;
        float center_y = uniforms->resolution.y * (0.48f + 0.08f * rise);
        float baseline_x;
        float baseline_y;
        int glyph_index = ((int)floorf(time * 0.5f) + layer * 4) % ((int)sizeof(source_text) - 1);

        if (glyph_index < 0) {
            glyph_index += (int)sizeof(source_text) - 1;
        }

        glyph_text[0] = source_text[glyph_index];
        glyph_text[1] = '\0';
        bounds = mtsdf_measure_text(&g_mtsdf_ascii_font, glyph_text, font_px_size);
        baseline_x = center_x - (bounds.left + bounds.right) * 0.5f;
        baseline_y = center_y - (bounds.bottom + bounds.top) * 0.5f;

        style.shell_width_px = 14.0f + 5.0f * (float)layer;
        style.glow_width_px = 5.0f + 2.5f * (float)layer;
        style.shadow_offset_x = 16.0f + 5.0f * (float)layer;
        style.shadow_offset_y = 12.0f + 4.0f * (float)layer;
        style.shadow_alpha = 0.16f;
        style.shell_alpha = 0.18f + 0.03f * (float)layer;
        style.fill_alpha = 0.10f + 0.03f * (float)layer;
        style.highlight_alpha = 0.22f;
        style.inner_glow_alpha = 0.14f;
        style.saturation = 0.68f;
        style.value = 1.0f;
        style.hue_offset = 0.16f * (float)layer + time * 0.03f;
        style.hue_scale_x = 0.11f;
        style.hue_scale_y = 0.07f;
        mtsdf_draw_bubble_run(color, fragCoord, uniforms, glyph_text, baseline_x, baseline_y, font_px_size, &style);
    }
}

ShaderBuffersCleanupFunc mtsdf_hello_world_buffers_init(shader_buffers_t *buffers, char *error, size_t error_size)
{
    if (shader_buffers_load_texture_module_relative(
            buffers,
            "ascii_mtsdf",
            g_mtsdf_ascii_font.atlas_path,
            SHADER_TEXEL_FORMAT_RGBA8_UNORM,
            error,
            error_size) == NULL)
    {
        return NULL;
    }

    return shader_buffers_default_cleanup;
}

vec4_t mtsdf_hello_world_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    vec3_t color = mtsdf_background(fragCoord, uniforms->resolution);

    mtsdf_draw_giant_showcase(&color, fragCoord, uniforms);
    mtsdf_draw_foreground_title(&color, fragCoord, uniforms);
    return vec4(color.x, color.y, color.z, 1.0f);
}
