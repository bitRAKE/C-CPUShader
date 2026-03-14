#pragma once

#include "../present/present_backend.h"

bool        backend_dx12_create(const present_backend_desc_t *desc);
void        backend_dx12_destroy(void);
bool        backend_dx12_is_ready(void);
bool        backend_dx12_present(const f32x4_surface_t *surface);
void        backend_dx12_set_vsync(bool enabled);
bool        backend_dx12_get_vsync(void);
bool        backend_dx12_is_hdr_presenting(void);
const char *backend_dx12_hdr_status(void);
const char *backend_dx12_error(void);
