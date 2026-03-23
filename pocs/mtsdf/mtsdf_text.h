#pragma once

#include "shader_defines.h"
#include "u_texture.h"
#include "ascii_mtsdf_font.h"

typedef mtsdf_ascii_glyph_t mtsdf_glyph_t;
typedef mtsdf_ascii_font_t  mtsdf_font_t;

typedef struct {
    float left;
    float bottom;
    float right;
    float top;
} mtsdf_text_bounds_t;

typedef struct {
    const mtsdf_glyph_t *glyph;
    float                x0;
    float                y0;
    float                x1;
    float                y1;
    float                advance_px;
} mtsdf_glyph_quad_t;

typedef struct {
    float msdf_distance_px;
    float sdf_distance_px;
    float fill_alpha;
    float soft_alpha;
    vec2_t glyph_uv;
} mtsdf_sample_t;

static inline float mtsdf_median3(float a, float b, float c)
{
    return fmaxf(fminf(a, b), fminf(fmaxf(a, b), c));
}

static inline const mtsdf_glyph_t *mtsdf_find_glyph(const mtsdf_font_t *font, uint codepoint)
{
    int index;

    if (font == NULL) {
        return NULL;
    }

    if (codepoint < 128u) {
        index = font->ascii_lookup[codepoint];
        if (index >= 0 && index < font->glyph_count) {
            return &font->glyphs[index];
        }
        return NULL;
    }

    for (index = 0; index < font->glyph_count; index++) {
        if (font->glyphs[index].unicode == codepoint) {
            return &font->glyphs[index];
        }
    }

    return NULL;
}

static inline float mtsdf_glyph_advance_px(const mtsdf_font_t *font, const mtsdf_glyph_t *glyph, float font_px_size)
{
    if (font == NULL || glyph == NULL) {
        return font_px_size * 0.5f;
    }

    return glyph->advance * font_px_size;
}

static inline float mtsdf_screen_px_range(const mtsdf_font_t *font, float font_px_size)
{
    if (font == NULL || font->atlas_em_size <= 0.0f) {
        return 1.0f;
    }

    return fmaxf(font->distance_range * font_px_size / font->atlas_em_size, 1.0f);
}

static inline mtsdf_text_bounds_t mtsdf_measure_text(const mtsdf_font_t *font, const char *text, float font_px_size)
{
    mtsdf_text_bounds_t bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    float cursor_x = 0.0f;
    bool has_visible = false;

    if (font == NULL || text == NULL) {
        return bounds;
    }

    for (int index = 0; text[index] != '\0'; index++) {
        const mtsdf_glyph_t *glyph = mtsdf_find_glyph(font, (unsigned char)text[index]);
        float advance_px = mtsdf_glyph_advance_px(font, glyph, font_px_size);

        if (glyph != NULL && glyph->has_bounds) {
            float left = cursor_x + glyph->plane_left * font_px_size;
            float bottom = glyph->plane_bottom * font_px_size;
            float right = cursor_x + glyph->plane_right * font_px_size;
            float top = glyph->plane_top * font_px_size;

            if (!has_visible) {
                bounds.left = left;
                bounds.bottom = bottom;
                bounds.right = right;
                bounds.top = top;
                has_visible = true;
            } else {
                bounds.left = fminf(bounds.left, left);
                bounds.bottom = fminf(bounds.bottom, bottom);
                bounds.right = fmaxf(bounds.right, right);
                bounds.top = fmaxf(bounds.top, top);
            }
        }

        cursor_x += advance_px;
    }

    if (!has_visible) {
        bounds.right = cursor_x;
        bounds.top = font_px_size;
    } else {
        bounds.right = fmaxf(bounds.right, cursor_x);
    }

    return bounds;
}

static inline bool mtsdf_build_glyph_quad(
    const mtsdf_font_t *font,
    const mtsdf_glyph_t *glyph,
    float cursor_x,
    float baseline_y,
    float font_px_size,
    mtsdf_glyph_quad_t *quad_out)
{
    if (quad_out == NULL) {
        return false;
    }

    quad_out->glyph = glyph;
    quad_out->advance_px = mtsdf_glyph_advance_px(font, glyph, font_px_size);
    quad_out->x0 = cursor_x;
    quad_out->y0 = baseline_y;
    quad_out->x1 = cursor_x;
    quad_out->y1 = baseline_y;

    if (glyph == NULL || !glyph->has_bounds) {
        return false;
    }

    quad_out->x0 = cursor_x + glyph->plane_left * font_px_size;
    quad_out->y0 = baseline_y + glyph->plane_bottom * font_px_size;
    quad_out->x1 = cursor_x + glyph->plane_right * font_px_size;
    quad_out->y1 = baseline_y + glyph->plane_top * font_px_size;
    return true;
}

static inline bool mtsdf_sample_quad(
    const shader_buffer_t *atlas,
    const mtsdf_font_t *font,
    const mtsdf_glyph_quad_t *quad,
    vec2_t point,
    float font_px_size,
    mtsdf_sample_t *sample_out)
{
    vec4_t texel;
    float tx;
    float ty;
    float u;
    float v;
    float screen_px_range;
    float msdf_signed_distance;
    float sdf_signed_distance;

    if (sample_out == NULL) {
        return false;
    }

    sample_out->msdf_distance_px = 0.0f;
    sample_out->sdf_distance_px = 0.0f;
    sample_out->fill_alpha = 0.0f;
    sample_out->soft_alpha = 0.0f;
    sample_out->glyph_uv = vec2_o(0.0f);

    if (font == NULL || quad == NULL || quad->glyph == NULL || !quad->glyph->has_bounds || !u_texture_valid(atlas)) {
        return false;
    }

    if (point.x < quad->x0 || point.x > quad->x1 || point.y < quad->y0 || point.y > quad->y1) {
        return false;
    }

    tx = (point.x - quad->x0) / fmaxf(quad->x1 - quad->x0, 0.0001f);
    ty = (point.y - quad->y0) / fmaxf(quad->y1 - quad->y0, 0.0001f);
    u = lerpf(quad->glyph->atlas_left / font->atlas_width, quad->glyph->atlas_right / font->atlas_width, tx);
    v = lerpf(1.0f - quad->glyph->atlas_bottom / font->atlas_height, 1.0f - quad->glyph->atlas_top / font->atlas_height, ty);
    texel = u_texture_sample_bilinear(atlas, vec2(u, v), U_TEXTURE_ADDRESS_CLAMP);
    screen_px_range = mtsdf_screen_px_range(font, font_px_size);
    msdf_signed_distance = mtsdf_median3(texel.x, texel.y, texel.z) - 0.5f;
    sdf_signed_distance = texel.w - 0.5f;

    sample_out->msdf_distance_px = msdf_signed_distance * screen_px_range;
    sample_out->sdf_distance_px = sdf_signed_distance * screen_px_range;
    sample_out->fill_alpha = saturate(sample_out->msdf_distance_px + 0.5f);
    sample_out->soft_alpha = saturate(sample_out->sdf_distance_px + 0.5f);
    sample_out->glyph_uv = vec2(tx, ty);
    return true;
}

static inline float mtsdf_expand_alpha(const mtsdf_sample_t *sample, float extra_px)
{
    if (sample == NULL) {
        return 0.0f;
    }

    return saturate(sample->sdf_distance_px + extra_px + 0.5f);
}
