#pragma once

#include "shader_buffers.h"

enum {
    U_TEXTURE_ADDRESS_CLAMP = 0,
    U_TEXTURE_ADDRESS_WRAP = 1
};

static inline const shader_buffer_t *u_buffer_named(const shader_uniforms_t *uniforms, const char *label)
{
    if (uniforms == NULL || uniforms->buffers == NULL) {
        return NULL;
    }

    return shader_buffers_find(uniforms->buffers, label);
}

static inline bool u_texture_valid(const shader_buffer_t *texture)
{
    return texture != NULL &&
           texture->type == SHADER_BUFFER_TYPE_TEXTURE2D &&
           texture->data != NULL &&
           texture->width > 0 &&
           texture->height > 0;
}

static inline int u_texture_bytes_per_pixel(const shader_buffer_t *texture)
{
    if (texture == NULL) {
        return 0;
    }

    if (texture->texel_format == SHADER_TEXEL_FORMAT_R8_UNORM) {
        return 1;
    }
    if (texture->texel_format == SHADER_TEXEL_FORMAT_RGBA8_UNORM || texture->texel_format == SHADER_TEXEL_FORMAT_BGRA8_UNORM) {
        return 4;
    }

    return 0;
}

static inline int u_texture_address(int coord, int extent, uint address_mode)
{
    if (extent <= 0) {
        return 0;
    }

    if (address_mode == U_TEXTURE_ADDRESS_WRAP) {
        coord %= extent;
        if (coord < 0) {
            coord += extent;
        }
        return coord;
    }

    if (coord < 0) {
        return 0;
    }
    if (coord >= extent) {
        return extent - 1;
    }
    return coord;
}

static inline vec4_t u_texture_decode(const shader_buffer_t *texture, const unsigned char *pixel)
{
    if (texture == NULL || pixel == NULL) {
        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    if (texture->texel_format == SHADER_TEXEL_FORMAT_R8_UNORM) {
        float value = pixel[0] / 255.0f;
        return vec4(value, value, value, 1.0f);
    }
    if (texture->texel_format == SHADER_TEXEL_FORMAT_RGBA8_UNORM) {
        return vec4(pixel[0] / 255.0f, pixel[1] / 255.0f, pixel[2] / 255.0f, pixel[3] / 255.0f);
    }
    if (texture->texel_format == SHADER_TEXEL_FORMAT_BGRA8_UNORM) {
        return vec4(pixel[2] / 255.0f, pixel[1] / 255.0f, pixel[0] / 255.0f, pixel[3] / 255.0f);
    }

    return vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

static inline vec4_t u_texture_texel(const shader_buffer_t *texture, int x, int y, uint address_mode)
{
    const unsigned char *row;
    const unsigned char *pixel;
    int bytes_per_pixel;

    if (!u_texture_valid(texture)) {
        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    bytes_per_pixel = u_texture_bytes_per_pixel(texture);
    x = u_texture_address(x, texture->width, address_mode);
    y = u_texture_address(y, texture->height, address_mode);
    row = (const unsigned char *)texture->data + (size_t)y * (size_t)texture->row_stride_bytes;
    pixel = row + (size_t)x * (size_t)bytes_per_pixel;
    return u_texture_decode(texture, pixel);
}

static inline float u_texture_wrap_uv(float uv)
{
    uv = uv - floorf(uv);
    if (uv < 0.0f) {
        uv += 1.0f;
    }
    return uv;
}

static inline float u_texture_clamp_uv(float uv)
{
    return clampf(uv, 0.0f, 1.0f);
}

static inline vec4_t u_texture_sample_nearest(const shader_buffer_t *texture, vec2_t uv, uint address_mode)
{
    float u;
    float v;
    int x;
    int y;

    if (!u_texture_valid(texture)) {
        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    u = (address_mode == U_TEXTURE_ADDRESS_WRAP) ? u_texture_wrap_uv(uv.x) : u_texture_clamp_uv(uv.x);
    v = (address_mode == U_TEXTURE_ADDRESS_WRAP) ? u_texture_wrap_uv(uv.y) : u_texture_clamp_uv(uv.y);
    x = (int)floorf(u * (float)(texture->width - 1) + 0.5f);
    y = (int)floorf(v * (float)(texture->height - 1) + 0.5f);
    return u_texture_texel(texture, x, y, address_mode);
}

static inline vec4_t u_texture_sample_bilinear(const shader_buffer_t *texture, vec2_t uv, uint address_mode)
{
    float u;
    float v;
    float x;
    float y;
    int x0;
    int y0;
    vec4_t c00;
    vec4_t c10;
    vec4_t c01;
    vec4_t c11;
    vec4_t cx0;
    vec4_t cx1;

    if (!u_texture_valid(texture)) {
        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    u = (address_mode == U_TEXTURE_ADDRESS_WRAP) ? u_texture_wrap_uv(uv.x) : u_texture_clamp_uv(uv.x);
    v = (address_mode == U_TEXTURE_ADDRESS_WRAP) ? u_texture_wrap_uv(uv.y) : u_texture_clamp_uv(uv.y);
    x = u * (float)(texture->width - 1);
    y = v * (float)(texture->height - 1);
    x0 = (int)floorf(x);
    y0 = (int)floorf(y);

    c00 = u_texture_texel(texture, x0, y0, address_mode);
    c10 = u_texture_texel(texture, x0 + 1, y0, address_mode);
    c01 = u_texture_texel(texture, x0, y0 + 1, address_mode);
    c11 = u_texture_texel(texture, x0 + 1, y0 + 1, address_mode);
    cx0 = v4_lerp(c00, c10, x - (float)x0);
    cx1 = v4_lerp(c01, c11, x - (float)x0);
    return v4_lerp(cx0, cx1, y - (float)y0);
}
