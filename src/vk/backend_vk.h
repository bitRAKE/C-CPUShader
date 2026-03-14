#pragma once

#include "../present/present_backend.h"

bool        backend_vk_create(const present_backend_desc_t *desc);
void        backend_vk_destroy(void);
bool        backend_vk_is_ready(void);
bool        backend_vk_present(const f32x4_surface_t *surface);
void        backend_vk_set_vsync(bool enabled);
bool        backend_vk_get_vsync(void);
bool        backend_vk_is_hdr_presenting(void);
present_conversion_mode_t backend_vk_conversion_mode(void);
const char *backend_vk_hdr_status(void);
const char *backend_vk_error(void);
