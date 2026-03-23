#pragma once

#include "defines.h"
#include "../sdk/shader_buffers.h"

/* Host-only function declarations -- not available to plugins */
void shader_buffers_reset(shader_buffers_t *buffers);
void shader_buffers_default_cleanup(shader_buffers_t *buffers);
shader_buffer_t *shader_buffers_alloc_bytes(shader_buffers_t *buffers, const char *label, size_t size_bytes, char *error, size_t error_size);
shader_buffer_t *shader_buffers_load_texture_file(shader_buffers_t *buffers, const char *label, const char *path, uint texel_format, char *error, size_t error_size);
shader_buffer_t *shader_buffers_load_texture_module_relative(shader_buffers_t *buffers, const char *label, const char *relative_path, uint texel_format, char *error, size_t error_size);
shader_buffer_t *shader_buffers_load_texture_resource(shader_buffers_t *buffers, const char *label, HMODULE module, const char *resource_name, uint texel_format, char *error, size_t error_size);
const shader_buffer_t *shader_buffers_find(const shader_buffers_t *buffers, const char *label);
