#include "present_backend.h"

#include "../dx12/backend_dx12.h"
#include "../gdi/backend_gdi.h"
#include "../ogl/backend_ogl.h"
#include "../vk/backend_vk.h"

static present_backend_kind_t g_present_backend_kind = PRESENT_BACKEND_DX12;
static char                   g_present_backend_error[256] = "";
static present_backend_desc_t  g_present_backend_desc = {0};
static bool                    g_present_backend_desc_valid = false;
static char                   g_present_backend_notice_text[512] = "";
static uint                   g_present_backend_notice_counter = 0;

static void present_backend_set_error_text(const char *text)
{
    snprintf(g_present_backend_error, sizeof(g_present_backend_error), "%s", text);
}

static void present_backend_set_notice_text(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return;
    }

    snprintf(g_present_backend_notice_text, sizeof(g_present_backend_notice_text), "%s", text);
    g_present_backend_notice_counter++;
}

const char *present_backend_conversion_name(present_conversion_mode_t mode)
{
    switch (mode) {
        case PRESENT_CONVERSION_DEVICE_SHADER:
            return "device shader";

        case PRESENT_CONVERSION_PRESENT_LAYER_FALLBACK:
        default:
            return "presentation-layer fallback";
    }
}

static const char *present_backend_hdr_notice_text(present_backend_kind_t kind)
{
    switch (kind) {
        case PRESENT_BACKEND_DX12:
            return backend_dx12_hdr_status();

        case PRESENT_BACKEND_OGL:
            return backend_ogl_hdr_status();

        case PRESENT_BACKEND_GDI:
            return "GDI Blit down-converts the ARGBF32 image to BGRA8; HDR is not present to the surface.";

        case PRESENT_BACKEND_VK:
            return backend_vk_hdr_status();
    }

    return "HDR is not present to the surface.";
}

static bool present_backend_shader_wants_hdr(void)
{
    return g_present_backend_desc_valid &&
           g_present_backend_desc.shader_color_space != SHADER_COLOR_SPACE_SDR_DISPLAY;
}

present_conversion_mode_t present_backend_conversion_mode(void)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            return PRESENT_CONVERSION_DEVICE_SHADER;

        case PRESENT_BACKEND_OGL:
            return backend_ogl_conversion_mode();

        case PRESENT_BACKEND_VK:
            return backend_vk_conversion_mode();

        case PRESENT_BACKEND_GDI:
        default:
            return PRESENT_CONVERSION_PRESENT_LAYER_FALLBACK;
    }
}

static void present_backend_note_backend_status(void)
{
    char note_text[1024];
    bool has_note = false;
    note_text[0] = '\0';

    if (g_present_backend_kind != PRESENT_BACKEND_DX12 &&
        g_present_backend_kind != PRESENT_BACKEND_GDI &&
        present_backend_conversion_mode() != PRESENT_CONVERSION_DEVICE_SHADER)
    {
        snprintf(
            note_text + strlen(note_text),
            sizeof(note_text) - strlen(note_text),
            "%s is using %s.",
            present_backend_display_name(g_present_backend_kind),
            present_backend_conversion_name(present_backend_conversion_mode()));
        has_note = true;
    }

    if (present_backend_shader_wants_hdr() && !present_backend_is_hdr_presenting()) {
        snprintf(
            note_text + strlen(note_text),
            sizeof(note_text) - strlen(note_text),
            "%s%s",
            has_note ? " " : "",
            present_backend_hdr_notice_text(g_present_backend_kind));
        has_note = true;
    }

    if (has_note) {
        present_backend_set_notice_text(note_text);
    }
}

static bool present_backend_fallback_to_gdi(const char *reason)
{
    char fallback_text[512];
    char combined_text[512];

    if (!g_present_backend_desc_valid) {
        present_backend_set_error_text(reason);
        return false;
    }

    snprintf(fallback_text, sizeof(fallback_text), "%s Falling back to GDI.", reason);
    OutputDebugStringA(fallback_text);
    OutputDebugStringA("\n");

    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            backend_dx12_destroy();
            break;

        case PRESENT_BACKEND_OGL:
            backend_ogl_destroy();
            break;

        case PRESENT_BACKEND_GDI:
            backend_gdi_destroy();
            break;

        case PRESENT_BACKEND_VK:
            backend_vk_destroy();
            break;
    }

    if (!backend_gdi_create(&g_present_backend_desc)) {
        snprintf(
            g_present_backend_error,
            sizeof(g_present_backend_error),
            "%s GDI fallback also failed (%s).",
            reason,
            backend_gdi_error());
        return false;
    }

    g_present_backend_kind = PRESENT_BACKEND_GDI;
    g_present_backend_error[0] = '\0';
    if (present_backend_shader_wants_hdr()) {
        snprintf(combined_text, sizeof(combined_text), "%s %s", fallback_text, present_backend_hdr_notice_text(g_present_backend_kind));
        present_backend_set_notice_text(combined_text);
    } else {
        present_backend_set_notice_text(fallback_text);
    }
    return true;
}

const char *present_backend_name(present_backend_kind_t kind)
{
    switch (kind) {
        case PRESENT_BACKEND_DX12: return "dx12";
        case PRESENT_BACKEND_OGL: return "ogl";
        case PRESENT_BACKEND_VK: return "vk";
        case PRESENT_BACKEND_GDI: return "gdi";
    }

    return "unknown";
}

const char *present_backend_display_name(present_backend_kind_t kind)
{
    switch (kind) {
        case PRESENT_BACKEND_DX12: return "DirectX 12";
        case PRESENT_BACKEND_OGL: return "OpenGL";
        case PRESENT_BACKEND_VK: return "Vulkan";
        case PRESENT_BACKEND_GDI: return "GDI Blit";
    }

    return "Unknown";
}

bool present_backend_create(present_backend_kind_t kind, const present_backend_desc_t *desc)
{
    present_backend_destroy();
    g_present_backend_kind = kind;
    g_present_backend_error[0] = '\0';
    g_present_backend_desc_valid = false;
    g_present_backend_notice_text[0] = '\0';

    if (desc != NULL) {
        g_present_backend_desc = *desc;
        g_present_backend_desc_valid = true;
    }

    switch (kind) {
        case PRESENT_BACKEND_DX12:
            if (!backend_dx12_create(desc)) {
                char reason[512];
                snprintf(reason, sizeof(reason), "DirectX 12 backend failed (%s).", backend_dx12_error());
                return present_backend_fallback_to_gdi(reason);
            }
            present_backend_note_backend_status();
            return true;

        case PRESENT_BACKEND_OGL:
            if (!backend_ogl_create(desc)) {
                char reason[512];
                snprintf(reason, sizeof(reason), "OpenGL backend failed (%s).", backend_ogl_error());
                return present_backend_fallback_to_gdi(reason);
            }
            present_backend_note_backend_status();
            return true;

        case PRESENT_BACKEND_VK:
            if (!backend_vk_create(desc)) {
                char reason[512];
                snprintf(reason, sizeof(reason), "Vulkan backend failed (%s).", backend_vk_error());
                return present_backend_fallback_to_gdi(reason);
            }
            present_backend_note_backend_status();
            return true;

        case PRESENT_BACKEND_GDI:
            if (!backend_gdi_create(desc)) {
                present_backend_set_error_text(backend_gdi_error());
                return false;
            }
            present_backend_note_backend_status();
            return true;
    }

    present_backend_set_error_text("Unknown backend selection.");
    return false;
}

void present_backend_destroy(void)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            backend_dx12_destroy();
            break;

        case PRESENT_BACKEND_GDI:
            backend_gdi_destroy();
            break;

        case PRESENT_BACKEND_OGL:
            backend_ogl_destroy();
            break;

        case PRESENT_BACKEND_VK:
            backend_vk_destroy();
            break;
    }
}

bool present_backend_is_ready(void)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            return backend_dx12_is_ready();

        case PRESENT_BACKEND_OGL:
            return backend_ogl_is_ready();

        case PRESENT_BACKEND_GDI:
            return backend_gdi_is_ready();

        case PRESENT_BACKEND_VK:
            return backend_vk_is_ready();
    }

    return false;
}

bool present_backend_present(const f32x4_surface_t *surface)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            if (!backend_dx12_present(surface)) {
                char reason[512];
                snprintf(reason, sizeof(reason), "DirectX 12 present failed (%s).", backend_dx12_error());
                if (!present_backend_fallback_to_gdi(reason)) {
                    present_backend_set_error_text(reason);
                    return false;
                }
                if (!backend_gdi_present(surface)) {
                    present_backend_set_error_text(backend_gdi_error());
                    return false;
                }
                return true;
            }
            return true;

        case PRESENT_BACKEND_OGL:
            if (!backend_ogl_present(surface)) {
                char reason[512];
                snprintf(reason, sizeof(reason), "OpenGL present failed (%s).", backend_ogl_error());
                if (!present_backend_fallback_to_gdi(reason)) {
                    present_backend_set_error_text(reason);
                    return false;
                }
                if (!backend_gdi_present(surface)) {
                    present_backend_set_error_text(backend_gdi_error());
                    return false;
                }
                return true;
            }
            return true;

        case PRESENT_BACKEND_GDI:
            if (!backend_gdi_present(surface)) {
                present_backend_set_error_text(backend_gdi_error());
                return false;
            }
            return true;

        case PRESENT_BACKEND_VK:
            if (!backend_vk_present(surface)) {
                char reason[512];
                snprintf(reason, sizeof(reason), "Vulkan present failed (%s).", backend_vk_error());
                if (!present_backend_fallback_to_gdi(reason)) {
                    present_backend_set_error_text(reason);
                    return false;
                }
                if (!backend_gdi_present(surface)) {
                    present_backend_set_error_text(backend_gdi_error());
                    return false;
                }
                return true;
            }
            return true;
    }

    present_backend_set_error_text("No active backend.");
    return false;
}

void present_backend_set_vsync(bool enabled)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            backend_dx12_set_vsync(enabled);
            break;

        case PRESENT_BACKEND_OGL:
            backend_ogl_set_vsync(enabled);
            break;

        case PRESENT_BACKEND_GDI:
            backend_gdi_set_vsync(enabled);
            break;

        case PRESENT_BACKEND_VK:
            backend_vk_set_vsync(enabled);
            break;
    }
}

bool present_backend_get_vsync(void)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            return backend_dx12_get_vsync();

        case PRESENT_BACKEND_OGL:
            return backend_ogl_get_vsync();

        case PRESENT_BACKEND_GDI:
            return backend_gdi_get_vsync();

        case PRESENT_BACKEND_VK:
            return backend_vk_get_vsync();
    }

    return false;
}

const char *present_backend_error(void)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            if (backend_dx12_error()[0] != '\0') {
                return backend_dx12_error();
            }
            break;

        case PRESENT_BACKEND_OGL:
            if (backend_ogl_error()[0] != '\0') {
                return backend_ogl_error();
            }
            break;

        case PRESENT_BACKEND_GDI:
            if (backend_gdi_error()[0] != '\0') {
                return backend_gdi_error();
            }
            break;

        case PRESENT_BACKEND_VK:
            if (backend_vk_error()[0] != '\0') {
                return backend_vk_error();
            }
            break;
    }

    return g_present_backend_error;
}

present_backend_kind_t present_backend_kind(void)
{
    return g_present_backend_kind;
}

bool present_backend_is_hdr_presenting(void)
{
    switch (g_present_backend_kind) {
        case PRESENT_BACKEND_DX12:
            return backend_dx12_is_hdr_presenting();

        case PRESENT_BACKEND_OGL:
            return backend_ogl_is_hdr_presenting();

        case PRESENT_BACKEND_GDI:
            return false;

        case PRESENT_BACKEND_VK:
            return backend_vk_is_hdr_presenting();
    }

    return false;
}

const char *present_backend_notice(void)
{
    return g_present_backend_notice_text;
}

uint present_backend_notice_version(void)
{
    return g_present_backend_notice_counter;
}
