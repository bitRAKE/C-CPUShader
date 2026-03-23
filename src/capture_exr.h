#pragma once

#include "defines.h"

#include <stddef.h>
#include <stdint.h>

bool capture_exr_write(
    const char *path,
    uint32_t width,
    uint32_t height,
    const vec4_t *pixels,
    char *error_text,
    size_t error_text_size);
