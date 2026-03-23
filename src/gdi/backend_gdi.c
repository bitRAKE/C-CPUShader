#include "backend_gdi.h"

#include "../present/color_convert.h"

#include <string.h>

typedef struct {
    HWND                 hwnd;
    int                  render_width;
    int                  render_height;
    int                  popup_width;
    int                  popup_height;
    bool                 vsync_enabled;
    bool                 transparent_display;
    shader_color_space_t shader_color_space;
    bool                 ready;
    char                 error[512];
    BITMAPINFO           bitmap_info;
    BYTE                *bgra_pixels;
    size_t               bgra_size;
    HDC                  layered_dc;
    HBITMAP              layered_bitmap;
    HGDIOBJ              layered_old_bitmap;
} gdi_state_t;

static gdi_state_t g_gdi = {0};

static void gdi_set_error_text(const char *text)
{
    snprintf(g_gdi.error, sizeof(g_gdi.error), "%s", text);
}

static float gdi_clamp_unit(float value)
{
    if (value <= 0.0f) {
        return 0.0f;
    }

    if (value >= 1.0f) {
        return 1.0f;
    }

    return value;
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

static bool gdi_create_layered_surface(int width, int height)
{
    HDC screen_dc;
    void *bits = NULL;

    if (width <= 0 || height <= 0) {
        gdi_set_error_text("Transparent GDI popup size is invalid.");
        return false;
    }

    ZeroMemory(&g_gdi.bitmap_info, sizeof(g_gdi.bitmap_info));
    g_gdi.bitmap_info.bmiHeader.biSize = sizeof(g_gdi.bitmap_info.bmiHeader);
    g_gdi.bitmap_info.bmiHeader.biWidth = width;
    g_gdi.bitmap_info.bmiHeader.biHeight = -height;
    g_gdi.bitmap_info.bmiHeader.biPlanes = 1;
    g_gdi.bitmap_info.bmiHeader.biBitCount = 32;
    g_gdi.bitmap_info.bmiHeader.biCompression = BI_RGB;

    screen_dc = GetDC(NULL);
    if (screen_dc == NULL) {
        gdi_set_error_text("GetDC(screen) failed.");
        return false;
    }

    g_gdi.layered_dc = CreateCompatibleDC(screen_dc);
    if (g_gdi.layered_dc == NULL) {
        ReleaseDC(NULL, screen_dc);
        gdi_set_error_text("CreateCompatibleDC failed.");
        return false;
    }

    g_gdi.layered_bitmap = CreateDIBSection(screen_dc, &g_gdi.bitmap_info, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen_dc);
    if (g_gdi.layered_bitmap == NULL || bits == NULL) {
        gdi_set_error_text("CreateDIBSection failed.");
        return false;
    }

    g_gdi.layered_old_bitmap = SelectObject(g_gdi.layered_dc, g_gdi.layered_bitmap);
    g_gdi.bgra_pixels = (BYTE *)bits;
    g_gdi.bgra_size = (size_t)width * (size_t)height * 4u;
    return true;
}

static void gdi_pack_opaque_pixels(const f32x4_surface_t *surface)
{
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
            dst_row[x * 4 + 3] = present_float_to_byte(gdi_clamp_unit(src_pixels[x].w));
        }
    }
}

static void gdi_pack_transparent_pixels(const f32x4_surface_t *surface)
{
    for (int y = 0; y < g_gdi.popup_height; y++) {
        int src_y = surface->height - 1 - (int)(((LONGLONG)y * (LONGLONG)surface->height) / (LONGLONG)g_gdi.popup_height);
        BYTE *dst_row = g_gdi.bgra_pixels + (size_t)y * (size_t)g_gdi.popup_width * 4u;
        const char *src_row;

        if (src_y < 0) {
            src_y = 0;
        }
        if (src_y >= surface->height) {
            src_y = surface->height - 1;
        }

        src_row = (const char *)surface->pixels + (size_t)src_y * (size_t)surface->stride_bytes;

        for (int x = 0; x < g_gdi.popup_width; x++) {
            int src_x = (int)(((LONGLONG)x * (LONGLONG)surface->width) / (LONGLONG)g_gdi.popup_width);
            const vec4_t *src_pixel;
            vec3_t sdr;
            float alpha;

            if (src_x < 0) {
                src_x = 0;
            }
            if (src_x >= surface->width) {
                src_x = surface->width - 1;
            }

            src_pixel = (const vec4_t *)src_row + src_x;
            sdr = present_encode_sdr_display(g_gdi.shader_color_space, vec3(src_pixel->x, src_pixel->y, src_pixel->z));
            alpha = gdi_clamp_unit(src_pixel->w);

            dst_row[x * 4 + 0] = present_float_to_byte(sdr.z * alpha);
            dst_row[x * 4 + 1] = present_float_to_byte(sdr.y * alpha);
            dst_row[x * 4 + 2] = present_float_to_byte(sdr.x * alpha);
            dst_row[x * 4 + 3] = present_float_to_byte(alpha);
        }
    }
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
    g_gdi.popup_width = desc->popup_width;
    g_gdi.popup_height = desc->popup_height;
    g_gdi.vsync_enabled = desc->vsync_enabled;
    g_gdi.transparent_display = desc->transparent_display;
    g_gdi.shader_color_space = desc->shader_color_space;

    ZeroMemory(&g_gdi.bitmap_info, sizeof(g_gdi.bitmap_info));
    if (g_gdi.transparent_display) {
        if (!gdi_create_layered_surface(g_gdi.popup_width, g_gdi.popup_height)) {
            backend_gdi_destroy();
            return false;
        }
    } else {
        if (!gdi_allocate_pixels(desc->render_width, desc->render_height)) {
            backend_gdi_destroy();
            return false;
        }

        g_gdi.bitmap_info.bmiHeader.biSize = sizeof(g_gdi.bitmap_info.bmiHeader);
        g_gdi.bitmap_info.bmiHeader.biWidth = desc->render_width;
        g_gdi.bitmap_info.bmiHeader.biHeight = desc->render_height;
        g_gdi.bitmap_info.bmiHeader.biPlanes = 1;
        g_gdi.bitmap_info.bmiHeader.biBitCount = 32;
        g_gdi.bitmap_info.bmiHeader.biCompression = BI_RGB;
    }

    g_gdi.ready = true;
    g_gdi.error[0] = '\0';
    return true;
}

void backend_gdi_destroy(void)
{
    bool has_layered_bitmap = (g_gdi.layered_bitmap != NULL);

    if (g_gdi.layered_dc != NULL && g_gdi.layered_old_bitmap != NULL) {
        SelectObject(g_gdi.layered_dc, g_gdi.layered_old_bitmap);
        g_gdi.layered_old_bitmap = NULL;
    }
    if (g_gdi.layered_bitmap != NULL) {
        DeleteObject(g_gdi.layered_bitmap);
        g_gdi.layered_bitmap = NULL;
    }
    if (g_gdi.layered_dc != NULL) {
        DeleteDC(g_gdi.layered_dc);
        g_gdi.layered_dc = NULL;
    }
    if (!has_layered_bitmap) {
        free(g_gdi.bgra_pixels);
    }
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

    if (g_gdi.transparent_display) {
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        POINT source_point = {0, 0};
        POINT dest_point = {0, 0};
        SIZE window_size = {g_gdi.popup_width, g_gdi.popup_height};

        gdi_pack_transparent_pixels(surface);
        if (!GetWindowRect(g_gdi.hwnd, &client_rect)) {
            gdi_set_error_text("GetWindowRect(display) failed.");
            return false;
        }
        dest_point.x = client_rect.left;
        dest_point.y = client_rect.top;

        hdc = GetDC(NULL);
        if (hdc == NULL) {
            gdi_set_error_text("GetDC(screen) failed.");
            return false;
        }

        if (!UpdateLayeredWindow(g_gdi.hwnd, hdc, &dest_point, &window_size, g_gdi.layered_dc, &source_point, 0, &blend, ULW_ALPHA)) {
            ReleaseDC(NULL, hdc);
            gdi_set_error_text("UpdateLayeredWindow failed.");
            return false;
        }

        ReleaseDC(NULL, hdc);
        return true;
    }

    gdi_pack_opaque_pixels(surface);

    if (!GetClientRect(g_gdi.hwnd, &client_rect)) {
        gdi_set_error_text("GetClientRect(display) failed.");
        return false;
    }

    if (client_rect.right <= client_rect.left || client_rect.bottom <= client_rect.top) {
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
        client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top,
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
