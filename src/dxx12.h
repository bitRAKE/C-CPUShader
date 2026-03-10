#pragma once

#include "defines.h"

bool        dxx12_create      (HWND hwnd, int image_width, int image_height);
void        dxx12_destroy     (void);
bool        dxx12_is_ready    (void);
bool        dxx12_begin_frame (vec4_t **pixels_out);
bool        dxx12_end_frame   (void);
void        dxx12_set_vsync   (bool enabled);
bool        dxx12_get_vsync   (void);
const char *dxx12_error       (void);
