/*
 * shader_host_services.h -- Host services callback table for plugins.
 *
 * Plugins that need buffer operations (texture loading, byte allocation)
 * receive this table via the ShaderBuffersInitFunc services parameter.
 *
 * This header includes <windows.h> for HMODULE, which is required by
 * the resource loading callback. Plugin shaders that do not use buffers
 * never need to include this header.
 */

#pragma once

#include "shader_defines.h"
#include "shader_buffers.h"

#include <windows.h>  /* HMODULE for resource loading */

struct shader_host_services_t {
    /* Allocate a raw byte buffer in the shader buffer set. */
    shader_buffer_t *(*alloc_bytes)(
        shader_buffers_t *buffers,
        const char       *label,
        size_t            size_bytes,
        char             *error,
        size_t            error_size);

    /* Load a texture from an absolute file path. */
    shader_buffer_t *(*load_texture_file)(
        shader_buffers_t *buffers,
        const char       *label,
        const char       *path,
        uint              texel_format,
        char             *error,
        size_t            error_size);

    /* Load a texture from a path relative to the host executable. */
    shader_buffer_t *(*load_texture_relative)(
        shader_buffers_t *buffers,
        const char       *label,
        const char       *relative_path,
        uint              texel_format,
        char             *error,
        size_t            error_size);

    /* Load a texture from a Win32 resource embedded in a DLL. */
    shader_buffer_t *(*load_texture_resource)(
        shader_buffers_t *buffers,
        const char       *label,
        HMODULE           module,
        const char       *resource_name,
        uint              texel_format,
        char             *error,
        size_t            error_size);

    /* Default cleanup: frees all buffer data and resets the set.
     * Plugins should return this from buffers_init when using
     * host-allocated buffers (load_texture_resource, alloc_bytes, etc.). */
    ShaderBuffersCleanupFunc default_cleanup;
};
