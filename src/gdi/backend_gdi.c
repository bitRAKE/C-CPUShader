#include "backend_gdi.h"

#include "../present/color_convert.h"

#include <string.h>

typedef struct {
    HWND                 hwnd;
    int                  render_width;
    int                  render_height;
    bool                 vsync_enabled;
    shader_color_space_t shader_color_space;
    bool                 ready;
    char                 error[512];
    BITMAPINFO           bitmap_info;
    BYTE                *bgra_pixels;
    size_t               bgra_size;
} gdi_state_t;

static gdi_state_t g_gdi = {0};

static void gdi_set_error_text(const char *text)
{
    snprintf(g_gdi.error, sizeof(g_gdi.error), "%s", text);
}

static bool gdi_allocate_pixels(int width, int height)
{
    size_t size_bytes;

    if (width <= 0 || height <= 0) {
        gdi_set_error_text("GDI backend dimensions are invalid.");
        return false;
    }

    size_bytes = (size_t)width * (size_t)height * 4u;
    g_gdi.bgra_pixels = (BYTE *)malloc(size_bytes);
    if (g_gdi.bgra_pixels == NULL) {
        gdi_set_error_text("Failed to allocate GDI presentation buffer.");
        return false;
    }

    g_gdi.bgra_size = size_bytes;
    return true;
}

bool backend_gdi_create(const present_backend_desc_t *desc)
{
    if (desc == NULL || desc->hwnd == NULL) {
        gdi_set_error_text("GDI backend description is invalid.");
        return false;
    }

    backend_gdi_destroy();
    ZeroMemory(&g_gdi, sizeof(g_gdi));
    g_gdi.hwnd = desc->hwnd;
    g_gdi.render_width = desc->render_width;
    g_gdi.render_height = desc->render_height;
    g_gdi.vsync_enabled = desc->vsync_enabled;
    g_gdi.shader_color_space = desc->shader_color_space;

    if (!gdi_allocate_pixels(desc->render_width, desc->render_height)) {
        backend_gdi_destroy();
        return false;
    }

    ZeroMemory(&g_gdi.bitmap_info, sizeof(g_gdi.bitmap_info));
    g_gdi.bitmap_info.bmiHeader.biSize = sizeof(g_gdi.bitmap_info.bmiHeader);
    g_gdi.bitmap_info.bmiHeader.biWidth = desc->render_width;
    g_gdi.bitmap_info.bmiHeader.biHeight = desc->render_height;
    g_gdi.bitmap_info.bmiHeader.biPlanes = 1;
    g_gdi.bitmap_info.bmiHeader.biBitCount = 32;
    g_gdi.bitmap_info.bmiHeader.biCompression = BI_RGB;

    g_gdi.ready = true;
    g_gdi.error[0] = '\0';
    return true;
}

void backend_gdi_destroy(void)
{
    free(g_gdi.bgra_pixels);
    g_gdi.bgra_pixels = NULL;
    g_gdi.bgra_size = 0;
    g_gdi.ready = false;
}

bool backend_gdi_is_ready(void)
{
    return g_gdi.ready;
}

bool backend_gdi_present(const f32x4_surface_t *surface)
{
    HDC hdc;
    RECT client_rect;
    int client_width;
    int client_height;

    if (!g_gdi.ready || g_gdi.hwnd == NULL) {
        gdi_set_error_text("GDI backend is not ready.");
        return false;
    }

    if (surface == NULL || surface->pixels == NULL) {
        gdi_set_error_text("GDI present surface is invalid.");
        return false;
    }

    if (surface->width != g_gdi.render_width || surface->height != g_gdi.render_height) {
        gdi_set_error_text("GDI present surface dimensions do not match the backend.");
        return false;
    }

    if (surface->stride_bytes < (int)((size_t)surface->width * sizeof(vec4_t))) {
        gdi_set_error_text("GDI present surface stride is invalid.");
        return false;
    }

    for (int y = 0; y < surface->height; y++) {
        const char *src_row = (const char *)surface->pixels + (size_t)y * (size_t)surface->stride_bytes;
        BYTE *dst_row = g_gdi.bgra_pixels + (size_t)y * (size_t)surface->width * 4u;
        const vec4_t *src_pixels = (const vec4_t *)src_row;

        for (int x = 0; x < surface->width; x++) {
            vec3_t sdr = present_encode_sdr_display(
                g_gdi.shader_color_space,
                vec3(src_pixels[x].x, src_pixels[x].y, src_pixels[x].z));
            dst_row[x * 4 + 0] = present_float_to_byte(sdr.z);
            dst_row[x * 4 + 1] = present_float_to_byte(sdr.y);
            dst_row[x * 4 + 2] = present_float_to_byte(sdr.x);
            dst_row[x * 4 + 3] = present_float_to_byte(src_pixels[x].w);
        }
    }

    if (!GetClientRect(g_gdi.hwnd, &client_rect)) {
        gdi_set_error_text("GetClientRect(display) failed.");
        return false;
    }

    client_width = client_rect.right - client_rect.left;
    client_height = client_rect.bottom - client_rect.top;
    if (client_width <= 0 || client_height <= 0) {
        gdi_set_error_text("GDI display client size is invalid.");
        return false;
    }

    hdc = GetDC(g_gdi.hwnd);
    if (hdc == NULL) {
        gdi_set_error_text("GetDC(display) failed.");
        return false;
    }

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(
        hdc,
        0,
        0,
        client_width,
        client_height,
        0,
        0,
        surface->width,
        surface->height,
        g_gdi.bgra_pixels,
        &g_gdi.bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY);
    ReleaseDC(g_gdi.hwnd, hdc);
    return true;
}

void backend_gdi_set_vsync(bool enabled)
{
    g_gdi.vsync_enabled = enabled;
}

bool backend_gdi_get_vsync(void)
{
    return g_gdi.vsync_enabled;
}

const char *backend_gdi_error(void)
{
    return g_gdi.error;
}
