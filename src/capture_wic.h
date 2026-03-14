#pragma once

#include "defines.h"

#include <stddef.h>
#include <stdint.h>

bool capture_wic_write_png(
    const char *path,
    uint32_t width,
    uint32_t height,
    const vec4_t *argbf32_data,
    shader_color_space_t shader_color_space,
    bool high_precision,
    char *error_text,
    size_t error_text_size);
