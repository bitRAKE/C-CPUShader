#pragma once

#include "../present/present_backend.h"

bool        backend_ogl_create(const present_backend_desc_t *desc);
void        backend_ogl_destroy(void);
bool        backend_ogl_is_ready(void);
bool        backend_ogl_present(const f32x4_surface_t *surface);
void        backend_ogl_set_vsync(bool enabled);
bool        backend_ogl_get_vsync(void);
bool        backend_ogl_is_hdr_presenting(void);
present_conversion_mode_t backend_ogl_conversion_mode(void);
const char *backend_ogl_hdr_status(void);
const char *backend_ogl_error(void);
