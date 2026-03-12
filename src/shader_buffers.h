#pragma once

#include "defines.h"

enum {
    SHADER_BUFFER_TYPE_NONE      = 0,
    SHADER_BUFFER_TYPE_BYTES     = 1,
    SHADER_BUFFER_TYPE_TEXTURE2D = 2
};

enum {
    SHADER_TEXEL_FORMAT_NONE        = 0,
    SHADER_TEXEL_FORMAT_R8_UNORM    = 1,
    SHADER_TEXEL_FORMAT_RGBA8_UNORM = 2,
    SHADER_TEXEL_FORMAT_BGRA8_UNORM = 3
};

enum {
    SHADER_BUFFER_LABEL_CAPACITY = 64,
    SHADER_BUFFER_MAX = 16
};

typedef struct {
    char   label[SHADER_BUFFER_LABEL_CAPACITY];
    void  *data;
    size_t size_bytes;
    uint   type;
    uint   texel_format;
    int    width;
    int    height;
    int    row_stride_bytes;
} shader_buffer_t;

struct shader_buffers_t {
    int             count;
    shader_buffer_t items[SHADER_BUFFER_MAX];
};

typedef void (*ShaderBuffersCleanupFunc)(shader_buffers_t *buffers);
typedef ShaderBuffersCleanupFunc (*ShaderBuffersInitFunc)(shader_buffers_t *buffers, char *error, size_t error_size);

void shader_buffers_reset(shader_buffers_t *buffers);
void shader_buffers_default_cleanup(shader_buffers_t *buffers);
shader_buffer_t *shader_buffers_alloc_bytes(shader_buffers_t *buffers, const char *label, size_t size_bytes, char *error, size_t error_size);
shader_buffer_t *shader_buffers_load_texture_file(shader_buffers_t *buffers, const char *label, const char *path, uint texel_format, char *error, size_t error_size);
shader_buffer_t *shader_buffers_load_texture_module_relative(shader_buffers_t *buffers, const char *label, const char *relative_path, uint texel_format, char *error, size_t error_size);
const shader_buffer_t *shader_buffers_find(const shader_buffers_t *buffers, const char *label);
