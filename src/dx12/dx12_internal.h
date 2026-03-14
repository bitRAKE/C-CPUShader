#pragma once

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include "../defines.h"
#include "../dxgi/dxgi_support.h"
#include "../present/f32_surface.h"

#include <d3d12.h>
#pragma warning(push)
#pragma warning(disable : 4115)
#include <d3dcompiler.h>
#pragma warning(pop)

#define DX12_FRAME_COUNT 2

typedef struct {
    ID3D12CommandAllocator *command_allocator;
    ID3D12Resource         *back_buffer;
    ID3D12Resource         *upload_buffer;
    vec4_t                 *mapped_pixels;
    UINT64                  fence_value;
} dx12_frame_t;

typedef struct {
    HWND                         hwnd;
    int                          image_width;
    int                          image_height;
    int                          window_width;
    int                          window_height;
    int                          current_frame_index;
    bool                         vsync_enabled;
    bool                         hdr_presenting;
    bool                         ready;
    char                         hdr_status[512];
    char                         error[512];
    shader_color_space_t         shader_color_space;
    dxgi_surface_kind_t          surface_kind;
    DXGI_FORMAT                  swap_chain_format;
    DXGI_COLOR_SPACE_TYPE        swap_chain_color_space;
    dxgi_support_t               dxgi;

    ID3D12Device                *device;
    ID3D12CommandQueue          *command_queue;
    IDXGISwapChain4             *swap_chain;

    ID3D12DescriptorHeap        *rtv_heap;
    ID3D12DescriptorHeap        *srv_heap;
    UINT                         rtv_descriptor_size;
    UINT                         srv_descriptor_size;

    dx12_frame_t                 frames[DX12_FRAME_COUNT];
    ID3D12GraphicsCommandList   *command_list;

    ID3D12RootSignature         *root_signature;
    ID3D12PipelineState         *pipeline_state;

    ID3D12Fence                 *fence;
    HANDLE                       fence_event;
    UINT64                       next_fence_value;
} dx12_state_t;

extern dx12_state_t g_dx12;

void    dx12_set_error_hr(const char *what, HRESULT hr);
void    dx12_set_error_text(const char *text);
D3D12_CPU_DESCRIPTOR_HANDLE dx12_get_rtv_handle(UINT index);
D3D12_CPU_DESCRIPTOR_HANDLE dx12_get_srv_cpu_handle(UINT index);
D3D12_GPU_DESCRIPTOR_HANDLE dx12_get_srv_gpu_handle(UINT index);
D3D12_HEAP_PROPERTIES       dx12_heap_properties(D3D12_HEAP_TYPE type);
D3D12_RESOURCE_DESC         dx12_buffer_desc(UINT64 size_bytes);
D3D12_RESOURCE_BARRIER      dx12_transition_barrier(
    ID3D12Resource *resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after);
void    dx12_wait_for_frame(UINT frame_index);
void    dx12_wait_for_gpu(void);
HRESULT dx12_compile_shader(
    const char *source,
    const D3D_SHADER_MACRO *macros,
    const char *entry_point,
    const char *target,
    ID3DBlob **blob_out);
HRESULT dx12_upload_buffer_srv_create(void);
bool    dx12_upload_buffer_srv_present(const f32x4_surface_t *surface);
void    dx12_upload_buffer_srv_destroy(void);
