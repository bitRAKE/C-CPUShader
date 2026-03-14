#pragma once

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include "../defines.h"

#include <dxgi1_6.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) do { \
        if ((x) != NULL) {   \
            IUnknown_Release((IUnknown *)(x)); \
            (x) = NULL;      \
        }                    \
    } while (0)
#endif

typedef enum {
    DXGI_SURFACE_KIND_SDR = 0,
    DXGI_SURFACE_KIND_HDR_SCRGB,
    DXGI_SURFACE_KIND_HDR10
} dxgi_surface_kind_t;

typedef struct {
    IDXGIFactory6        *factory;
    IDXGIAdapter1        *adapter;
    DXGI_COLOR_SPACE_TYPE output_color_space;
    bool                  supports_sdr_surface;
    bool                  supports_scrgb_surface;
    bool                  supports_hdr10_surface;
} dxgi_support_t;

typedef bool (*dxgi_support_adapter_probe_fn)(IDXGIAdapter1 *candidate, void *user_data);

void                    dxgi_support_destroy(dxgi_support_t *support);
HRESULT                 dxgi_support_create_factory(dxgi_support_t *support);
HRESULT                 dxgi_support_pick_adapter(dxgi_support_t *support, dxgi_support_adapter_probe_fn probe_fn, void *user_data);
bool                    dxgi_support_find_target_output_desc1(const dxgi_support_t *support, HWND hwnd, DXGI_OUTPUT_DESC1 *desc_out);
void                    dxgi_support_detect_surface_capabilities(dxgi_support_t *support, HWND hwnd);
dxgi_surface_kind_t     dxgi_choose_surface_kind(const dxgi_support_t *support, shader_color_space_t shader_color_space);
void                    dxgi_available_surfaces_text(const dxgi_support_t *support, char *buffer, size_t buffer_size);
bool                    dxgi_output_reports_hdr(DXGI_COLOR_SPACE_TYPE color_space);
DXGI_FORMAT             dxgi_surface_format(dxgi_surface_kind_t surface_kind);
DXGI_COLOR_SPACE_TYPE   dxgi_surface_color_space(dxgi_surface_kind_t surface_kind);
const char             *dxgi_format_name(DXGI_FORMAT format);
const char             *dxgi_color_space_name(DXGI_COLOR_SPACE_TYPE color_space);
const char             *dxgi_shader_color_space_name(shader_color_space_t color_space);
const char             *dxgi_surface_kind_name(dxgi_surface_kind_t surface_kind);
void                    dxgi_swap_chain_configure_hdr_status(
                            const dxgi_support_t *support,
                            IDXGISwapChain4 *swap_chain,
                            shader_color_space_t shader_color_space,
                            dxgi_surface_kind_t surface_kind,
                            DXGI_FORMAT swap_chain_format,
                            bool *hdr_presenting_out,
                            char *status_buffer,
                            size_t status_buffer_size);
