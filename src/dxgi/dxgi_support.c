#include "dxgi_support.h"

#include <string.h>

void dxgi_support_destroy(dxgi_support_t *support)
{
    if (support == NULL) {
        return;
    }

    SAFE_RELEASE(support->adapter);
    SAFE_RELEASE(support->factory);
    ZeroMemory(support, sizeof(*support));
}

HRESULT dxgi_support_create_factory(dxgi_support_t *support)
{
    if (support == NULL) {
        return E_INVALIDARG;
    }

    SAFE_RELEASE(support->factory);
    return CreateDXGIFactory2(0, &IID_IDXGIFactory6, (void **)&support->factory);
}

HRESULT dxgi_support_pick_adapter(dxgi_support_t *support, dxgi_support_adapter_probe_fn probe_fn, void *user_data)
{
    UINT index;

    if (support == NULL || support->factory == NULL) {
        return E_INVALIDARG;
    }

    SAFE_RELEASE(support->adapter);

    for (index = 0; ; ++index) {
        IDXGIAdapter1 *candidate = NULL;
        DXGI_ADAPTER_DESC1 desc;

        if (IDXGIFactory6_EnumAdapterByGpuPreference(
                support->factory,
                index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                &IID_IDXGIAdapter1,
                (void **)&candidate) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        ZeroMemory(&desc, sizeof(desc));
        IDXGIAdapter1_GetDesc1(candidate, &desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            SAFE_RELEASE(candidate);
            continue;
        }

        if (probe_fn == NULL || probe_fn(candidate, user_data)) {
            support->adapter = candidate;
            return S_OK;
        }

        SAFE_RELEASE(candidate);
    }

    return E_FAIL;
}

const char *dxgi_format_name(DXGI_FORMAT format)
{
    switch (format) {
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return "DXGI_FORMAT_R16G16B16A16_FLOAT";

        case DXGI_FORMAT_R10G10B10A2_UNORM:
            return "DXGI_FORMAT_R10G10B10A2_UNORM";

        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return "DXGI_FORMAT_R8G8B8A8_UNORM";

        default:
            return "DXGI_FORMAT_UNKNOWN";
    }
}

const char *dxgi_color_space_name(DXGI_COLOR_SPACE_TYPE color_space)
{
    switch (color_space) {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
            return "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709";

        case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
            return "DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709";

        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
            return "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020";

        default:
            return "DXGI_COLOR_SPACE_UNKNOWN";
    }
}

const char *dxgi_shader_color_space_name(shader_color_space_t color_space)
{
    switch (color_space) {
        case SHADER_COLOR_SPACE_SDR_DISPLAY:
            return "SDR display";

        case SHADER_COLOR_SPACE_SCENE_LINEAR:
            return "scene linear";

        case SHADER_COLOR_SPACE_HDR10_ST2084:
            return "HDR10 (ST.2084)";
    }

    return "unknown";
}

const char *dxgi_surface_kind_name(dxgi_surface_kind_t surface_kind)
{
    switch (surface_kind) {
        case DXGI_SURFACE_KIND_SDR:
            return "SDR";

        case DXGI_SURFACE_KIND_HDR_SCRGB:
            return "scRGB";

        case DXGI_SURFACE_KIND_HDR10:
            return "HDR10";
    }

    return "unknown";
}

DXGI_FORMAT dxgi_surface_format(dxgi_surface_kind_t surface_kind)
{
    switch (surface_kind) {
        case DXGI_SURFACE_KIND_HDR_SCRGB:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case DXGI_SURFACE_KIND_HDR10:
            return DXGI_FORMAT_R10G10B10A2_UNORM;

        case DXGI_SURFACE_KIND_SDR:
        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

DXGI_COLOR_SPACE_TYPE dxgi_surface_color_space(dxgi_surface_kind_t surface_kind)
{
    switch (surface_kind) {
        case DXGI_SURFACE_KIND_HDR_SCRGB:
            return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;

        case DXGI_SURFACE_KIND_HDR10:
            return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;

        case DXGI_SURFACE_KIND_SDR:
        default:
            return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    }
}

bool dxgi_output_reports_hdr(DXGI_COLOR_SPACE_TYPE color_space)
{
    return color_space == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
}

static bool dxgi_find_target_output_on_adapter(
    IDXGIAdapter1 *adapter,
    HMONITOR target_monitor,
    DXGI_OUTPUT_DESC1 *desc_out,
    DXGI_OUTPUT_DESC1 *fallback_desc,
    bool *fallback_valid)
{
    UINT output_index;

    if (adapter == NULL || desc_out == NULL || fallback_desc == NULL || fallback_valid == NULL) {
        return false;
    }

    for (output_index = 0; ; ++output_index) {
        IDXGIOutput *output = NULL;
        IDXGIOutput6 *output6 = NULL;
        DXGI_OUTPUT_DESC1 output_desc;
        HRESULT hr;

        hr = IDXGIAdapter1_EnumOutputs(adapter, output_index, &output);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr) || output == NULL) {
            continue;
        }

        hr = IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput6, (void **)&output6);
        if (SUCCEEDED(hr) && output6 != NULL) {
            ZeroMemory(&output_desc, sizeof(output_desc));
            hr = IDXGIOutput6_GetDesc1(output6, &output_desc);
            if (SUCCEEDED(hr)) {
                if (!*fallback_valid) {
                    *fallback_desc = output_desc;
                    *fallback_valid = true;
                }

                if (output_desc.Monitor == target_monitor) {
                    *desc_out = output_desc;
                    SAFE_RELEASE(output6);
                    SAFE_RELEASE(output);
                    return true;
                }
            }
        }

        SAFE_RELEASE(output6);
        SAFE_RELEASE(output);
    }

    return false;
}

bool dxgi_support_find_target_output_desc1(const dxgi_support_t *support, HWND hwnd, DXGI_OUTPUT_DESC1 *desc_out)
{
    DXGI_OUTPUT_DESC1 fallback_desc;
    bool fallback_valid = false;
    HMONITOR target_monitor;

    if (support == NULL || desc_out == NULL || hwnd == NULL) {
        return false;
    }

    target_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    ZeroMemory(desc_out, sizeof(*desc_out));
    ZeroMemory(&fallback_desc, sizeof(fallback_desc));

    if (support->adapter != NULL) {
        if (dxgi_find_target_output_on_adapter(support->adapter, target_monitor, desc_out, &fallback_desc, &fallback_valid)) {
            return true;
        }
    }

    if (support->factory != NULL) {
        UINT adapter_index;

        for (adapter_index = 0; ; ++adapter_index) {
            IDXGIAdapter1 *adapter = NULL;
            HRESULT hr;

            hr = IDXGIFactory6_EnumAdapters1(support->factory, adapter_index, &adapter);
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr) || adapter == NULL) {
                continue;
            }

            if (dxgi_find_target_output_on_adapter(adapter, target_monitor, desc_out, &fallback_desc, &fallback_valid)) {
                SAFE_RELEASE(adapter);
                return true;
            }

            SAFE_RELEASE(adapter);
        }
    }

    if (fallback_valid) {
        *desc_out = fallback_desc;
        return true;
    }

    return false;
}

void dxgi_support_detect_surface_capabilities(dxgi_support_t *support, HWND hwnd)
{
    DXGI_OUTPUT_DESC1 output_desc;

    if (support == NULL) {
        return;
    }

    support->output_color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    support->supports_sdr_surface = true;
    support->supports_scrgb_surface = false;
    support->supports_hdr10_surface = false;

    if (dxgi_support_find_target_output_desc1(support, hwnd, &output_desc)) {
        support->output_color_space = output_desc.ColorSpace;
        if (dxgi_output_reports_hdr(output_desc.ColorSpace)) {
            support->supports_scrgb_surface = true;
            support->supports_hdr10_surface = true;
        }
    }
}

dxgi_surface_kind_t dxgi_choose_surface_kind(const dxgi_support_t *support, shader_color_space_t shader_color_space)
{
    if (support == NULL) {
        return DXGI_SURFACE_KIND_SDR;
    }

    if (shader_color_space == SHADER_COLOR_SPACE_HDR10_ST2084) {
        if (support->supports_hdr10_surface) {
            return DXGI_SURFACE_KIND_HDR10;
        }
        if (support->supports_scrgb_surface) {
            return DXGI_SURFACE_KIND_HDR_SCRGB;
        }
    }

    if (shader_color_space == SHADER_COLOR_SPACE_SCENE_LINEAR) {
        if (support->supports_scrgb_surface) {
            return DXGI_SURFACE_KIND_HDR_SCRGB;
        }
        if (support->supports_hdr10_surface) {
            return DXGI_SURFACE_KIND_HDR10;
        }
    }

    return DXGI_SURFACE_KIND_SDR;
}

void dxgi_available_surfaces_text(const dxgi_support_t *support, char *buffer, size_t buffer_size)
{
    bool wrote = false;

    if (support == NULL || buffer == NULL || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';

    if (support->supports_sdr_surface) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%sSDR", wrote ? ", " : "");
        wrote = true;
    }
    if (support->supports_scrgb_surface) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%sscRGB", wrote ? ", " : "");
        wrote = true;
    }
    if (support->supports_hdr10_surface) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%sHDR10", wrote ? ", " : "");
        wrote = true;
    }

    if (!wrote) {
        snprintf(buffer, buffer_size, "unknown");
    }
}

void dxgi_swap_chain_configure_hdr_status(
    const dxgi_support_t *support,
    IDXGISwapChain4 *swap_chain,
    shader_color_space_t shader_color_space,
    dxgi_surface_kind_t surface_kind,
    DXGI_FORMAT swap_chain_format,
    bool *hdr_presenting_out,
    char *status_buffer,
    size_t status_buffer_size)
{
    IDXGIOutput *containing_output = NULL;
    IDXGIOutput6 *output6 = NULL;
    DXGI_OUTPUT_DESC1 output_desc;
    DXGI_COLOR_SPACE_TYPE swap_chain_color_space;
    DXGI_COLOR_SPACE_TYPE output_color_space;
    UINT color_space_support = 0;
    HRESULT hr;
    bool output_desc_valid = false;
    bool color_space_set = false;
    char available_surfaces[64];

    if (status_buffer != NULL && status_buffer_size > 0) {
        snprintf(status_buffer, status_buffer_size, "%s", "DXGI HDR status is not available.");
    }

    if (hdr_presenting_out != NULL) {
        *hdr_presenting_out = false;
    }

    if (support == NULL || swap_chain == NULL || status_buffer == NULL || status_buffer_size == 0) {
        return;
    }

    output_color_space = support->output_color_space;
    swap_chain_color_space = dxgi_surface_color_space(surface_kind);

    hr = IDXGISwapChain4_GetContainingOutput(swap_chain, &containing_output);
    if (SUCCEEDED(hr) && containing_output != NULL) {
        hr = IDXGIOutput_QueryInterface(containing_output, &IID_IDXGIOutput6, (void **)&output6);
        if (SUCCEEDED(hr) && output6 != NULL) {
            ZeroMemory(&output_desc, sizeof(output_desc));
            hr = IDXGIOutput6_GetDesc1(output6, &output_desc);
            if (SUCCEEDED(hr)) {
                output_color_space = output_desc.ColorSpace;
                output_desc_valid = true;
            }
        }
    }

    hr = IDXGISwapChain4_CheckColorSpaceSupport(swap_chain, swap_chain_color_space, &color_space_support);
    if (SUCCEEDED(hr) && (color_space_support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0) {
        hr = IDXGISwapChain4_SetColorSpace1(swap_chain, swap_chain_color_space);
        color_space_set = SUCCEEDED(hr);
    }

    dxgi_available_surfaces_text(support, available_surfaces, sizeof(available_surfaces));

    if ((surface_kind == DXGI_SURFACE_KIND_HDR_SCRGB || surface_kind == DXGI_SURFACE_KIND_HDR10) &&
        color_space_set &&
        output_desc_valid &&
        dxgi_output_reports_hdr(output_color_space))
    {
        if (hdr_presenting_out != NULL) {
            *hdr_presenting_out = true;
        }
        snprintf(
            status_buffer,
            status_buffer_size,
            "DXGI is presenting %s shader output through a %s surface (%s + %s) on HDR output %s. Available surfaces: %s. HDR is present to the surface.",
            dxgi_shader_color_space_name(shader_color_space),
            dxgi_surface_kind_name(surface_kind),
            dxgi_format_name(swap_chain_format),
            dxgi_color_space_name(swap_chain_color_space),
            dxgi_color_space_name(output_color_space),
            available_surfaces);
    } else {
        snprintf(
            status_buffer,
            status_buffer_size,
            "DXGI is presenting %s shader output through a %s surface (%s + %s) on output %s. Available surfaces: %s. HDR is not present to the surface.",
            dxgi_shader_color_space_name(shader_color_space),
            dxgi_surface_kind_name(surface_kind),
            dxgi_format_name(swap_chain_format),
            dxgi_color_space_name(swap_chain_color_space),
            output_desc_valid ? dxgi_color_space_name(output_color_space) : "DXGI_OUTPUT_DESC1 unavailable",
            available_surfaces);

        if (surface_kind != DXGI_SURFACE_KIND_SDR && !color_space_set) {
            snprintf(
                status_buffer,
                status_buffer_size,
                "DXGI created a %s surface (%s) for %s shader output but could not enable %s. Available surfaces: %s. HDR is not present to the surface.",
                dxgi_surface_kind_name(surface_kind),
                dxgi_format_name(swap_chain_format),
                dxgi_shader_color_space_name(shader_color_space),
                dxgi_color_space_name(swap_chain_color_space),
                available_surfaces);
        }
    }

    SAFE_RELEASE(output6);
    SAFE_RELEASE(containing_output);
}
