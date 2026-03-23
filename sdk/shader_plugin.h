/*
 * shader_plugin.h -- Plugin contract for shader collection DLLs.
 *
 * A plugin DLL exports two functions:
 *   - shader_collection_query: returns collection identity and ABI version.
 *   - shader_collection_load:  returns the shader descriptor array.
 *
 * The host calls shader_collection_query first. If the ABI version does
 * not match SHADER_PLUGIN_ABI_VERSION, the plugin is skipped.
 */

#pragma once

#include "shader_catalog.h"

#include <windows.h>  /* HMODULE */

#define SHADER_PLUGIN_ABI_VERSION 1

typedef struct {
    const char *collection_name;
    const char *collection_version;
    const char *collection_blurb;
    int         abi_version;
    HMODULE     module_handle;
} shader_collection_info_t;

/*
 * Returns collection identity and ABI version.
 * The host calls this first. If abi_version does not match
 * SHADER_PLUGIN_ABI_VERSION, the host skips the plugin.
 *
 * The plugin should set module_handle to its own HMODULE
 * (e.g. via DllMain hinstDLL or GetModuleHandleEx).
 */
typedef const shader_collection_info_t *(*ShaderCollectionQueryFunc)(void);

/*
 * Returns the shader descriptor array and its count.
 * The host calls this only after the ABI check passes.
 * The returned array must be static or otherwise valid for the
 * lifetime of the DLL.
 */
typedef const shader_desc_t *(*ShaderCollectionLoadFunc)(int *count_out);

/* Export names the host resolves via GetProcAddress. */
#define SHADER_COLLECTION_QUERY_NAME "shader_collection_query"
#define SHADER_COLLECTION_LOAD_NAME  "shader_collection_load"
