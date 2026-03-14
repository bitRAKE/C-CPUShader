#pragma once

#include "../defines.h"
#include "f32_surface.h"

typedef enum {
    PRESENT_BACKEND_DX12 = 0,
    PRESENT_BACKEND_OGL,
    PRESENT_BACKEND_VK,
    PRESENT_BACKEND_GDI
} present_backend_kind_t;

typedef enum {
    PRESENT_CONVERSION_PRESENT_LAYER_FALLBACK = 0,
    PRESENT_CONVERSION_DEVICE_SHADER
} present_conversion_mode_t;

typedef struct {
    HWND hwnd;
    int  render_width;
    int  render_height;
    int  popup_width;
    int  popup_height;
    shader_color_space_t shader_color_space;
    bool vsync_enabled;
} present_backend_desc_t;

bool                    present_backend_create(present_backend_kind_t kind, const present_backend_desc_t *desc);
void                    present_backend_destroy(void);
bool                    present_backend_is_ready(void);
bool                    present_backend_present(const f32x4_surface_t *surface);
void                    present_backend_set_vsync(bool enabled);
bool                    present_backend_get_vsync(void);
const char             *present_backend_error(void);
present_backend_kind_t  present_backend_kind(void);
const char             *present_backend_name(present_backend_kind_t kind);
const char             *present_backend_display_name(present_backend_kind_t kind);
bool                    present_backend_is_hdr_presenting(void);
present_conversion_mode_t present_backend_conversion_mode(void);
const char             *present_backend_conversion_name(present_conversion_mode_t mode);
const char             *present_backend_notice(void);
uint                    present_backend_notice_version(void);
