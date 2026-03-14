#pragma once

#include "../present/present_backend.h"

bool        backend_gdi_create(const present_backend_desc_t *desc);
void        backend_gdi_destroy(void);
bool        backend_gdi_is_ready(void);
bool        backend_gdi_present(const f32x4_surface_t *surface);
void        backend_gdi_set_vsync(bool enabled);
bool        backend_gdi_get_vsync(void);
const char *backend_gdi_error(void);
