/*
 * shader_buffers.h -- Buffer types and init signature for plugins.
 *
 * SDK version of src/shader_buffers.h. Includes shader_defines.h
 * instead of defines.h. The ShaderBuffersInitFunc typedef includes
 * the services parameter required for plugin buffer operations.
 */

#pragma once

#include "shader_defines.h"

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

/* Forward declaration -- full definition in shader_host_services.h */
typedef struct shader_host_services_t shader_host_services_t;

typedef ShaderBuffersCleanupFunc (*ShaderBuffersInitFunc)(
    shader_buffers_t *buffers,
    const shader_host_services_t *services,
    char *error, size_t error_size);
