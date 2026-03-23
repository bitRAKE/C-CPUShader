#include "shader_plugin.h"

#include "colorspace_sdr_ui.h"
#include "colorspace_hdr_linear.h"
#include "colorspace_hdr10_pq.h"

#define ENTRY(...) ENTRY_IMPL(__VA_ARGS__)
#define ENTRY_IMPL(id, display_name, entry, buffers_init, vars, var_count, var_size, \
                   color_space, features, width, height, blurb) \
    {#id, display_name, blurb, entry, buffers_init, vars, var_count, var_size, \
     color_space, features, width, height},

static const shader_desc_t g_shaders[] = {
    COLORSPACE_SDR_UI_SHADER(ENTRY)
    COLORSPACE_HDR_LINEAR_SHADER(ENTRY)
    COLORSPACE_HDR10_PQ_SHADER(ENTRY)
};

#undef ENTRY
#undef ENTRY_IMPL

static const int g_shader_count = (int)(sizeof(g_shaders) / sizeof(g_shaders[0]));

static shader_collection_info_t g_info = {0};

__declspec(dllexport)
const shader_collection_info_t *shader_collection_query(void)
{
    g_info.collection_name    = "Colorspace Probes";
    g_info.collection_version = "1.0";
    g_info.collection_blurb   = "SDR, scRGB, and HDR10 color space validation shaders.";
    g_info.abi_version        = SHADER_PLUGIN_ABI_VERSION;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)shader_collection_query,
        &g_info.module_handle);
    return &g_info;
}

__declspec(dllexport)
const shader_desc_t *shader_collection_load(int *count_out)
{
    if (count_out != NULL) {
        *count_out = g_shader_count;
    }
    return g_shaders;
}
