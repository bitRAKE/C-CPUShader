#include "backend_ogl.h"

#include "../dx12/backend_dx12.h"

#include <string.h>

typedef struct {
    char error[512];
    char hdr_status[1024];
} ogl_state_t;

static ogl_state_t g_ogl = {0};

static void ogl_update_hdr_status(void)
{
    const char *dx12_status = backend_dx12_hdr_status();

    snprintf(
        g_ogl.hdr_status,
        sizeof(g_ogl.hdr_status),
        "OpenGL mode is using the shared DXGI/DirectX 12 presentation path because the host does not treat WGL window presentation as the canonical Windows HDR path. %s",
        dx12_status != NULL ? dx12_status : "DXGI HDR status is not available.");
}

bool backend_ogl_create(const present_backend_desc_t *desc)
{
    ZeroMemory(&g_ogl, sizeof(g_ogl));

    if (!backend_dx12_create(desc)) {
        snprintf(
            g_ogl.error,
            sizeof(g_ogl.error),
            "OpenGL requested DXGI presentation, but the shared DirectX 12 presenter failed (%s).",
            backend_dx12_error());
        return false;
    }

    ogl_update_hdr_status();
    return true;
}

void backend_ogl_destroy(void)
{
    backend_dx12_destroy();
    ZeroMemory(&g_ogl, sizeof(g_ogl));
}

bool backend_ogl_is_ready(void)
{
    return backend_dx12_is_ready();
}

bool backend_ogl_present(const f32x4_surface_t *surface)
{
    if (!backend_dx12_present(surface)) {
        snprintf(
            g_ogl.error,
            sizeof(g_ogl.error),
            "OpenGL requested DXGI presentation, but the shared DirectX 12 presenter failed (%s).",
            backend_dx12_error());
        return false;
    }

    return true;
}

void backend_ogl_set_vsync(bool enabled)
{
    backend_dx12_set_vsync(enabled);
}

bool backend_ogl_get_vsync(void)
{
    return backend_dx12_get_vsync();
}

bool backend_ogl_is_hdr_presenting(void)
{
    return backend_dx12_is_hdr_presenting();
}

present_conversion_mode_t backend_ogl_conversion_mode(void)
{
    return PRESENT_CONVERSION_DEVICE_SHADER;
}

const char *backend_ogl_hdr_status(void)
{
    ogl_update_hdr_status();
    return g_ogl.hdr_status;
}

const char *backend_ogl_error(void)
{
    if (g_ogl.error[0] != '\0') {
        return g_ogl.error;
    }

    return backend_dx12_error();
}
