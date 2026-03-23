#include "shader_plugin.h"

#include "toggle_switch.h"
#include "radial_gauge.h"
#include "status_indicator.h"
#include "button_round_metal.h"
#include "button_template.h"
#include "variable_probe.h"

#define ENTRY(...) ENTRY_IMPL(__VA_ARGS__)
#define ENTRY_IMPL(id, display_name, entry, buffers_init, vars, var_count, var_size, \
                   color_space, features, width, height, blurb) \
    {#id, display_name, blurb, entry, buffers_init, vars, var_count, var_size, \
     color_space, features, width, height},

static const shader_desc_t g_shaders[] = {
    TOGGLE_SWITCH_SHADER(ENTRY)
    RADIAL_GAUGE_SHADER(ENTRY)
    STATUS_INDICATOR_SHADER(ENTRY)
    BUTTON_ROUND_METAL_SHADER(ENTRY)
    BUTTON_TEMPLATE_SHADER(ENTRY)
    VARIABLE_PROBE_SHADER(ENTRY)
};

#undef ENTRY
#undef ENTRY_IMPL

static const int g_shader_count = (int)(sizeof(g_shaders) / sizeof(g_shaders[0]));

static shader_collection_info_t g_info = {0};

__declspec(dllexport)
const shader_collection_info_t *shader_collection_query(void)
{
    g_info.collection_name    = "Shader Variables";
    g_info.collection_version = "1.0";
    g_info.collection_blurb   = "Variable-driven parametric shader experiments.";
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
