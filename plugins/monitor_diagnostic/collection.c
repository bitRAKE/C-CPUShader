#include "shader_plugin.h"

#include "diag_gradient.h"
#include "diag_hsv_wheel.h"
#include "diag_banding.h"
#include "diag_gamma_ramp.h"
#include "diag_motion.h"
#include "diag_strobe.h"
#include "diag_subpixel.h"
#include "diag_hdr_clipping.h"
#include "diag_hdr_gamut.h"

#define ENTRY(...) ENTRY_IMPL(__VA_ARGS__)
#define ENTRY_IMPL(id, display_name, entry, buffers_init, vars, var_count, var_size, \
                   color_space, features, width, height, blurb) \
    {#id, display_name, blurb, entry, buffers_init, vars, var_count, var_size, \
     color_space, features, width, height},

static const shader_desc_t g_shaders[] = {
    DIAG_GRADIENT_SHADER(ENTRY)
    DIAG_HSV_WHEEL_SHADER(ENTRY)
    DIAG_BANDING_SHADER(ENTRY)
    DIAG_GAMMA_RAMP_SHADER(ENTRY)
    DIAG_MOTION_SHADER(ENTRY)
    DIAG_STROBE_SHADER(ENTRY)
    DIAG_SUBPIXEL_SHADER(ENTRY)
    DIAG_HDR_CLIPPING_SHADER(ENTRY)
    DIAG_HDR_GAMUT_SHADER(ENTRY)
};

#undef ENTRY
#undef ENTRY_IMPL

static const int g_shader_count = (int)(sizeof(g_shaders) / sizeof(g_shaders[0]));

static shader_collection_info_t g_info = {0};

__declspec(dllexport)
const shader_collection_info_t *shader_collection_query(void)
{
    g_info.collection_name    = "Monitor Diagnostic";
    g_info.collection_version = "1.0";
    g_info.collection_blurb   = "Pixel-precise monitor calibration tests: gradients, gamma, banding, motion, flicker, sub-pixel patterns, HDR clipping, and wide-gamut probes.";
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
