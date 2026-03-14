#include "backend_dx12.h"

#include "dx12_internal.h"

#include <string.h>

dx12_state_t g_dx12 = {0};

static bool dx12_supports_adapter(IDXGIAdapter1 *candidate, void *user_data)
{
    (void)user_data;
    return SUCCEEDED(D3D12CreateDevice((IUnknown *)candidate, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL));
}

void dx12_set_error_hr(const char *what, HRESULT hr)
{
    snprintf(g_dx12.error, sizeof(g_dx12.error), "%s failed (hr=0x%08X).", what, (unsigned)hr);
}

void dx12_set_error_text(const char *text)
{
    snprintf(g_dx12.error, sizeof(g_dx12.error), "%s", text);
}

D3D12_CPU_DESCRIPTOR_HANDLE dx12_get_rtv_handle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(g_dx12.rtv_heap, &handle);
    handle.ptr += (SIZE_T)index * (SIZE_T)g_dx12.rtv_descriptor_size;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE dx12_get_srv_cpu_handle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(g_dx12.srv_heap, &handle);
    handle.ptr += (SIZE_T)index * (SIZE_T)g_dx12.srv_descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE dx12_get_srv_gpu_handle(UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(g_dx12.srv_heap, &handle);
    handle.ptr += (UINT64)index * (UINT64)g_dx12.srv_descriptor_size;
    return handle;
}

D3D12_HEAP_PROPERTIES dx12_heap_properties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES properties;
    ZeroMemory(&properties, sizeof(properties));
    properties.Type = type;
    properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    properties.CreationNodeMask = 1;
    properties.VisibleNodeMask = 1;
    return properties;
}

D3D12_RESOURCE_DESC dx12_buffer_desc(UINT64 size_bytes)
{
    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size_bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
}

D3D12_RESOURCE_BARRIER dx12_transition_barrier(
    ID3D12Resource *resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier;
    ZeroMemory(&barrier, sizeof(barrier));
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

void dx12_wait_for_frame(UINT frame_index)
{
    dx12_frame_t *frame = &g_dx12.frames[frame_index];
    UINT64 completed;

    if (g_dx12.fence == NULL || g_dx12.fence_event == NULL || frame->fence_value == 0) {
        return;
    }

    completed = ID3D12Fence_GetCompletedValue(g_dx12.fence);
    if (completed < frame->fence_value) {
        ID3D12Fence_SetEventOnCompletion(g_dx12.fence, frame->fence_value, g_dx12.fence_event);
        WaitForSingleObject(g_dx12.fence_event, INFINITE);
    }
}

void dx12_wait_for_gpu(void)
{
    UINT64 fence_value;

    if (g_dx12.command_queue == NULL || g_dx12.fence == NULL || g_dx12.fence_event == NULL) {
        return;
    }

    fence_value = g_dx12.next_fence_value++;
    if (SUCCEEDED(ID3D12CommandQueue_Signal(g_dx12.command_queue, g_dx12.fence, fence_value))) {
        if (ID3D12Fence_GetCompletedValue(g_dx12.fence) < fence_value) {
            ID3D12Fence_SetEventOnCompletion(g_dx12.fence, fence_value, g_dx12.fence_event);
            WaitForSingleObject(g_dx12.fence_event, INFINITE);
        }
    }
}

HRESULT dx12_compile_shader(
    const char *source,
    const D3D_SHADER_MACRO *macros,
    const char *entry_point,
    const char *target,
    ID3DBlob **blob_out)
{
    ID3DBlob *shader_blob = NULL;
    ID3DBlob *error_blob = NULL;
    HRESULT hr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    *blob_out = NULL;
    hr = D3DCompile(
        source,
        strlen(source),
        NULL,
        macros,
        NULL,
        entry_point,
        target,
        flags,
        0,
        &shader_blob,
        &error_blob);

    if (FAILED(hr)) {
        if (error_blob != NULL) {
            dx12_set_error_text((const char *)ID3D10Blob_GetBufferPointer((ID3D10Blob *)error_blob));
        } else {
            dx12_set_error_hr("D3DCompile", hr);
        }

        SAFE_RELEASE(error_blob);
        SAFE_RELEASE(shader_blob);
        return hr;
    }

    SAFE_RELEASE(error_blob);
    *blob_out = shader_blob;
    return S_OK;
}

static HRESULT dx12_create_back_buffers(void)
{
    UINT frame_index;
    HRESULT hr;

    for (frame_index = 0; frame_index < DX12_FRAME_COUNT; ++frame_index) {
        hr = IDXGISwapChain4_GetBuffer(
            g_dx12.swap_chain,
            frame_index,
            &IID_ID3D12Resource,
            (void **)&g_dx12.frames[frame_index].back_buffer);
        if (FAILED(hr)) {
            return hr;
        }

        ID3D12Device_CreateRenderTargetView(g_dx12.device, g_dx12.frames[frame_index].back_buffer, NULL, dx12_get_rtv_handle(frame_index));
    }

    return S_OK;
}

static void dx12_configure_hdr_status(void)
{
    g_dx12.hdr_presenting = false;
    g_dx12.swap_chain_color_space = dxgi_surface_color_space(g_dx12.surface_kind);
    dxgi_swap_chain_configure_hdr_status(
        &g_dx12.dxgi,
        g_dx12.swap_chain,
        g_dx12.shader_color_space,
        g_dx12.surface_kind,
        g_dx12.swap_chain_format,
        &g_dx12.hdr_presenting,
        g_dx12.hdr_status,
        sizeof(g_dx12.hdr_status));
}

bool backend_dx12_create(const present_backend_desc_t *desc)
{
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
    D3D12_COMMAND_QUEUE_DESC queue_desc;
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc;
    RECT client_rect;
    IDXGISwapChain1 *swap_chain1 = NULL;
    HRESULT hr;

    if (desc == NULL || desc->hwnd == NULL) {
        dx12_set_error_text("DXGI backend description is invalid.");
        return false;
    }

    backend_dx12_destroy();
    ZeroMemory(&g_dx12, sizeof(g_dx12));
    g_dx12.hwnd = desc->hwnd;
    g_dx12.image_width = desc->render_width;
    g_dx12.image_height = desc->render_height;
    g_dx12.current_frame_index = -1;
    g_dx12.vsync_enabled = desc->vsync_enabled;
    g_dx12.shader_color_space = desc->shader_color_space;
    g_dx12.hdr_presenting = false;
    g_dx12.swap_chain_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_dx12.swap_chain_color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    g_dx12.surface_kind = DXGI_SURFACE_KIND_SDR;
    g_dx12.hdr_status[0] = '\0';

    if (!GetClientRect(desc->hwnd, &client_rect)) {
        dx12_set_error_hr("GetClientRect", HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }

    g_dx12.window_width = client_rect.right - client_rect.left;
    g_dx12.window_height = client_rect.bottom - client_rect.top;
    if (g_dx12.window_width <= 0) {
        g_dx12.window_width = (desc->popup_width > 0) ? desc->popup_width : desc->render_width;
    }
    if (g_dx12.window_height <= 0) {
        g_dx12.window_height = (desc->popup_height > 0) ? desc->popup_height : desc->render_height;
    }

    hr = dxgi_support_create_factory(&g_dx12.dxgi);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateDXGIFactory2", hr);
        backend_dx12_destroy();
        return false;
    }

    hr = dxgi_support_pick_adapter(&g_dx12.dxgi, dx12_supports_adapter, NULL);
    if (FAILED(hr)) {
        dx12_set_error_hr("DXGI adapter enumeration", hr);
        backend_dx12_destroy();
        return false;
    }

    hr = D3D12CreateDevice((IUnknown *)g_dx12.dxgi.adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&g_dx12.device);
    if (FAILED(hr)) {
        dx12_set_error_hr("D3D12CreateDevice", hr);
        backend_dx12_destroy();
        return false;
    }

    dxgi_support_detect_surface_capabilities(&g_dx12.dxgi, g_dx12.hwnd);
    g_dx12.surface_kind = dxgi_choose_surface_kind(&g_dx12.dxgi, g_dx12.shader_color_space);

    ZeroMemory(&queue_desc, sizeof(queue_desc));
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = ID3D12Device_CreateCommandQueue(g_dx12.device, &queue_desc, &IID_ID3D12CommandQueue, (void **)&g_dx12.command_queue);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateCommandQueue", hr);
        backend_dx12_destroy();
        return false;
    }

    ZeroMemory(&swap_chain_desc, sizeof(swap_chain_desc));
    swap_chain_desc.Width = (UINT)g_dx12.window_width;
    swap_chain_desc.Height = (UINT)g_dx12.window_height;
    swap_chain_desc.Format = dxgi_surface_format(g_dx12.surface_kind);
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = DX12_FRAME_COUNT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    g_dx12.swap_chain_format = swap_chain_desc.Format;

    hr = IDXGIFactory6_CreateSwapChainForHwnd(
        g_dx12.dxgi.factory,
        (IUnknown *)g_dx12.command_queue,
        g_dx12.hwnd,
        &swap_chain_desc,
        NULL,
        NULL,
        &swap_chain1);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateSwapChainForHwnd", hr);
        backend_dx12_destroy();
        return false;
    }

    hr = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain4, (void **)&g_dx12.swap_chain);
    SAFE_RELEASE(swap_chain1);
    if (FAILED(hr)) {
        dx12_set_error_hr("QueryInterface(IDXGISwapChain4)", hr);
        backend_dx12_destroy();
        return false;
    }

    hr = IDXGIFactory6_MakeWindowAssociation(g_dx12.dxgi.factory, g_dx12.hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) {
        dx12_set_error_hr("MakeWindowAssociation", hr);
        backend_dx12_destroy();
        return false;
    }

    dx12_configure_hdr_status();

    ZeroMemory(&heap_desc, sizeof(heap_desc));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = DX12_FRAME_COUNT;
    hr = ID3D12Device_CreateDescriptorHeap(g_dx12.device, &heap_desc, &IID_ID3D12DescriptorHeap, (void **)&g_dx12.rtv_heap);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateDescriptorHeap(RTV)", hr);
        backend_dx12_destroy();
        return false;
    }
    g_dx12.rtv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(g_dx12.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    ZeroMemory(&heap_desc, sizeof(heap_desc));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = DX12_FRAME_COUNT;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = ID3D12Device_CreateDescriptorHeap(g_dx12.device, &heap_desc, &IID_ID3D12DescriptorHeap, (void **)&g_dx12.srv_heap);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateDescriptorHeap(SRV)", hr);
        backend_dx12_destroy();
        return false;
    }
    g_dx12.srv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(g_dx12.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    hr = dx12_create_back_buffers();
    if (FAILED(hr)) {
        dx12_set_error_hr("GetBuffer(back buffer)", hr);
        backend_dx12_destroy();
        return false;
    }

    hr = ID3D12Device_CreateFence(g_dx12.device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&g_dx12.fence);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateFence", hr);
        backend_dx12_destroy();
        return false;
    }

    g_dx12.fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (g_dx12.fence_event == NULL) {
        dx12_set_error_hr("CreateEventW", HRESULT_FROM_WIN32(GetLastError()));
        backend_dx12_destroy();
        return false;
    }

    g_dx12.next_fence_value = 1;

    hr = dx12_upload_buffer_srv_create();
    if (FAILED(hr)) {
        if (g_dx12.error[0] == '\0') {
            dx12_set_error_hr("dx12_upload_buffer_srv_create", hr);
        }
        backend_dx12_destroy();
        return false;
    }

    g_dx12.ready = true;
    g_dx12.error[0] = '\0';
    return true;
}

void backend_dx12_destroy(void)
{
    UINT frame_index;

    dx12_wait_for_gpu();
    dx12_upload_buffer_srv_destroy();

    for (frame_index = 0; frame_index < DX12_FRAME_COUNT; ++frame_index) {
        SAFE_RELEASE(g_dx12.frames[frame_index].back_buffer);
        g_dx12.frames[frame_index].fence_value = 0;
    }

    SAFE_RELEASE(g_dx12.rtv_heap);
    SAFE_RELEASE(g_dx12.srv_heap);
    SAFE_RELEASE(g_dx12.swap_chain);
    SAFE_RELEASE(g_dx12.fence);
    SAFE_RELEASE(g_dx12.command_queue);
    SAFE_RELEASE(g_dx12.device);
    dxgi_support_destroy(&g_dx12.dxgi);

    if (g_dx12.fence_event != NULL) {
        CloseHandle(g_dx12.fence_event);
        g_dx12.fence_event = NULL;
    }

    g_dx12.current_frame_index = -1;
    g_dx12.ready = false;
}

bool backend_dx12_is_ready(void)
{
    return g_dx12.ready;
}

bool backend_dx12_present(const f32x4_surface_t *surface)
{
    if (!g_dx12.ready) {
        dx12_set_error_text("DXGI backend is not ready.");
        return false;
    }

    return dx12_upload_buffer_srv_present(surface);
}

void backend_dx12_set_vsync(bool enabled)
{
    g_dx12.vsync_enabled = enabled;
}

bool backend_dx12_get_vsync(void)
{
    return g_dx12.vsync_enabled;
}

bool backend_dx12_is_hdr_presenting(void)
{
    return g_dx12.ready && g_dx12.hdr_presenting;
}

const char *backend_dx12_hdr_status(void)
{
    return g_dx12.hdr_status[0] != '\0' ? g_dx12.hdr_status : "DXGI HDR status is not available.";
}

const char *backend_dx12_error(void)
{
    return g_dx12.error;
}
