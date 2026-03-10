#define COBJMACROS

#include "defines.h"
#include "dxx12.h"

#include <d3d12.h>
#pragma warning(push)
#pragma warning(disable : 4115)
#include <d3dcompiler.h>
#pragma warning(pop)
#include <dxgi1_6.h>

#include <string.h>

#define DXX12_FRAME_COUNT 2

#define SAFE_RELEASE(x) do { \
        if ((x) != NULL) {   \
            IUnknown_Release((IUnknown *)(x)); \
            (x) = NULL;      \
        }                    \
    } while (0)

typedef struct {
    ID3D12CommandAllocator *command_allocator;
    ID3D12Resource         *back_buffer;
    ID3D12Resource         *upload_buffer;
    vec4_t                 *mapped_pixels;
    UINT64                  fence_value;
} dxx12_frame_t;

typedef struct {
    HWND                         hwnd;
    int                          image_width;
    int                          image_height;
    int                          window_width;
    int                          window_height;
    int                          current_frame_index;
    bool                         vsync_enabled;
    bool                         ready;
    char                         error[512];

    IDXGIFactory6               *factory;
    IDXGIAdapter1               *adapter;
    ID3D12Device                *device;
    ID3D12CommandQueue          *command_queue;
    IDXGISwapChain4             *swap_chain;

    ID3D12DescriptorHeap        *rtv_heap;
    ID3D12DescriptorHeap        *srv_heap;
    UINT                         rtv_descriptor_size;
    UINT                         srv_descriptor_size;

    dxx12_frame_t                frames[DXX12_FRAME_COUNT];
    ID3D12GraphicsCommandList   *command_list;

    ID3D12RootSignature         *root_signature;
    ID3D12PipelineState         *pipeline_state;

    ID3D12Fence                 *fence;
    HANDLE                       fence_event;
    UINT64                       next_fence_value;
} dxx12_state_t;

static dxx12_state_t g_dxx12 = {0};

static const char g_shader_source[] =
    "struct VSOut\n"
    "{\n"
    "    float4 position : SV_Position;\n"
    "    float2 uv : TEXCOORD0;\n"
    "};\n"
    "\n"
    "StructuredBuffer<float4> g_image : register(t0);\n"
    "\n"
    "VSOut VSMain(uint vertex_id : SV_VertexID)\n"
    "{\n"
    "    float2 positions[3] =\n"
    "    {\n"
    "        float2(-1.0, -1.0),\n"
    "        float2(-1.0,  3.0),\n"
    "        float2( 3.0, -1.0)\n"
    "    };\n"
    "\n"
    "    float2 uvs[3] =\n"
    "    {\n"
    "        float2(0.0, 1.0),\n"
    "        float2(0.0, -1.0),\n"
    "        float2(2.0, 1.0)\n"
    "    };\n"
    "\n"
    "    VSOut output;\n"
    "    output.position = float4(positions[vertex_id], 0.0, 1.0);\n"
    "    output.uv = uvs[vertex_id];\n"
    "    return output;\n"
    "}\n"
    "\n"
    "float4 PSMain(VSOut input) : SV_Target\n"
    "{\n"
    "    float2 uv = saturate(input.uv);\n"
    "    uint x = min((uint)(uv.x * (float)IMAGE_WIDTH), (uint)(IMAGE_WIDTH - 1));\n"
    "    uint y = min((uint)(uv.y * (float)IMAGE_HEIGHT), (uint)(IMAGE_HEIGHT - 1));\n"
    "    uint flipped_y = (uint)(IMAGE_HEIGHT - 1) - y;\n"
    "    return g_image[flipped_y * IMAGE_WIDTH + x];\n"
    "}\n";

static void dxx12_set_error_hr(const char *what, HRESULT hr)
{
    snprintf(g_dxx12.error, sizeof(g_dxx12.error), "%s failed (hr=0x%08X).", what, (unsigned)hr);
}

static void dxx12_set_error_text(const char *text)
{
    snprintf(g_dxx12.error, sizeof(g_dxx12.error), "%s", text);
}

const char *dxx12_error(void)
{
    return g_dxx12.error;
}

bool dxx12_is_ready(void)
{
    return g_dxx12.ready;
}

void dxx12_set_vsync(bool enabled)
{
    g_dxx12.vsync_enabled = enabled;
}

bool dxx12_get_vsync(void)
{
    return g_dxx12.vsync_enabled;
}

static D3D12_CPU_DESCRIPTOR_HANDLE dxx12_get_rtv_handle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(g_dxx12.rtv_heap, &handle);
    handle.ptr += (SIZE_T)index * (SIZE_T)g_dxx12.rtv_descriptor_size;
    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE dxx12_get_srv_cpu_handle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(g_dxx12.srv_heap, &handle);
    handle.ptr += (SIZE_T)index * (SIZE_T)g_dxx12.srv_descriptor_size;
    return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE dxx12_get_srv_gpu_handle(UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle;
    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(g_dxx12.srv_heap, &handle);
    handle.ptr += (UINT64)index * (UINT64)g_dxx12.srv_descriptor_size;
    return handle;
}

static D3D12_HEAP_PROPERTIES dxx12_heap_properties(D3D12_HEAP_TYPE type)
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

static D3D12_RESOURCE_DESC dxx12_buffer_desc(UINT64 size_bytes)
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

static D3D12_RESOURCE_BARRIER dxx12_transition_barrier(
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

static void dxx12_wait_for_frame(UINT frame_index)
{
    dxx12_frame_t *frame = &g_dxx12.frames[frame_index];
    UINT64 completed;

    if (g_dxx12.fence == NULL || g_dxx12.fence_event == NULL || frame->fence_value == 0) {
        return;
    }

    completed = ID3D12Fence_GetCompletedValue(g_dxx12.fence);
    if (completed < frame->fence_value) {
        ID3D12Fence_SetEventOnCompletion(g_dxx12.fence, frame->fence_value, g_dxx12.fence_event);
        WaitForSingleObject(g_dxx12.fence_event, INFINITE);
    }
}

static void dxx12_wait_for_gpu(void)
{
    UINT64 fence_value;

    if (g_dxx12.command_queue == NULL || g_dxx12.fence == NULL || g_dxx12.fence_event == NULL) {
        return;
    }

    fence_value = g_dxx12.next_fence_value++;
    if (SUCCEEDED(ID3D12CommandQueue_Signal(g_dxx12.command_queue, g_dxx12.fence, fence_value))) {
        if (ID3D12Fence_GetCompletedValue(g_dxx12.fence) < fence_value) {
            ID3D12Fence_SetEventOnCompletion(g_dxx12.fence, fence_value, g_dxx12.fence_event);
            WaitForSingleObject(g_dxx12.fence_event, INFINITE);
        }
    }
}

static HRESULT dxx12_compile_shader(
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
            dxx12_set_error_text((const char *)ID3D10Blob_GetBufferPointer((ID3D10Blob *)error_blob));
        } else {
            dxx12_set_error_hr("D3DCompile", hr);
        }

        SAFE_RELEASE(error_blob);
        SAFE_RELEASE(shader_blob);
        return hr;
    }

    SAFE_RELEASE(error_blob);
    *blob_out = shader_blob;
    return S_OK;
}

static HRESULT dxx12_pick_adapter(void)
{
    UINT index;

    for (index = 0; ; ++index) {
        IDXGIAdapter1 *candidate = NULL;
        DXGI_ADAPTER_DESC1 desc;

        if (IDXGIFactory6_EnumAdapterByGpuPreference(
                g_dxx12.factory,
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

        if (SUCCEEDED(D3D12CreateDevice((IUnknown *)candidate, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL))) {
            g_dxx12.adapter = candidate;
            return S_OK;
        }

        SAFE_RELEASE(candidate);
    }

    return E_FAIL;
}

static HRESULT dxx12_create_back_buffers(void)
{
    UINT frame_index;
    HRESULT hr;

    for (frame_index = 0; frame_index < DXX12_FRAME_COUNT; ++frame_index) {
        hr = IDXGISwapChain4_GetBuffer(g_dxx12.swap_chain, frame_index, &IID_ID3D12Resource, (void **)&g_dxx12.frames[frame_index].back_buffer);
        if (FAILED(hr)) {
            return hr;
        }

        ID3D12Device_CreateRenderTargetView(g_dxx12.device, g_dxx12.frames[frame_index].back_buffer, NULL, dxx12_get_rtv_handle(frame_index));
    }

    return S_OK;
}

static HRESULT dxx12_create_pipeline(void)
{
    D3D12_DESCRIPTOR_RANGE descriptor_range;
    D3D12_ROOT_PARAMETER root_parameter;
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
    ID3DBlob *serialized_root = NULL;
    ID3DBlob *root_errors = NULL;
    ID3DBlob *vertex_shader = NULL;
    ID3DBlob *pixel_shader = NULL;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc;
    D3D_SHADER_MACRO macros[3];
    char width_text[32];
    char height_text[32];
    HRESULT hr;

    snprintf(width_text, sizeof(width_text), "%d", g_dxx12.image_width);
    snprintf(height_text, sizeof(height_text), "%d", g_dxx12.image_height);

    macros[0].Name = "IMAGE_WIDTH";
    macros[0].Definition = width_text;
    macros[1].Name = "IMAGE_HEIGHT";
    macros[1].Definition = height_text;
    macros[2].Name = NULL;
    macros[2].Definition = NULL;

    ZeroMemory(&descriptor_range, sizeof(descriptor_range));
    descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptor_range.NumDescriptors = 1;
    descriptor_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ZeroMemory(&root_parameter, sizeof(root_parameter));
    root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameter.DescriptorTable.NumDescriptorRanges = 1;
    root_parameter.DescriptorTable.pDescriptorRanges = &descriptor_range;
    root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    ZeroMemory(&root_signature_desc, sizeof(root_signature_desc));
    root_signature_desc.NumParameters = 1;
    root_signature_desc.pParameters = &root_parameter;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    hr = D3D12SerializeRootSignature(
        &root_signature_desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized_root,
        &root_errors);
    if (FAILED(hr)) {
        if (root_errors != NULL) {
            dxx12_set_error_text((const char *)ID3D10Blob_GetBufferPointer((ID3D10Blob *)root_errors));
        } else {
            dxx12_set_error_hr("D3D12SerializeRootSignature", hr);
        }

        SAFE_RELEASE(root_errors);
        SAFE_RELEASE(serialized_root);
        return hr;
    }

    hr = ID3D12Device_CreateRootSignature(
        g_dxx12.device,
        0,
        ID3D10Blob_GetBufferPointer((ID3D10Blob *)serialized_root),
        ID3D10Blob_GetBufferSize((ID3D10Blob *)serialized_root),
        &IID_ID3D12RootSignature,
        (void **)&g_dxx12.root_signature);
    SAFE_RELEASE(root_errors);
    SAFE_RELEASE(serialized_root);
    if (FAILED(hr)) {
        return hr;
    }

    hr = dxx12_compile_shader(g_shader_source, macros, "VSMain", "vs_5_1", &vertex_shader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = dxx12_compile_shader(g_shader_source, macros, "PSMain", "ps_5_1", &pixel_shader);
    if (FAILED(hr)) {
        SAFE_RELEASE(vertex_shader);
        return hr;
    }

    ZeroMemory(&pipeline_desc, sizeof(pipeline_desc));
    pipeline_desc.pRootSignature = g_dxx12.root_signature;
    pipeline_desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer((ID3D10Blob *)vertex_shader);
    pipeline_desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize((ID3D10Blob *)vertex_shader);
    pipeline_desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer((ID3D10Blob *)pixel_shader);
    pipeline_desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize((ID3D10Blob *)pixel_shader);
    pipeline_desc.BlendState.AlphaToCoverageEnable = FALSE;
    pipeline_desc.BlendState.IndependentBlendEnable = FALSE;
    pipeline_desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pipeline_desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    pipeline_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    pipeline_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    pipeline_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pipeline_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pipeline_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    pipeline_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pipeline_desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    pipeline_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline_desc.SampleMask = UINT_MAX;
    pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline_desc.RasterizerState.FrontCounterClockwise = FALSE;
    pipeline_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    pipeline_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pipeline_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pipeline_desc.RasterizerState.DepthClipEnable = TRUE;
    pipeline_desc.DepthStencilState.DepthEnable = FALSE;
    pipeline_desc.DepthStencilState.StencilEnable = FALSE;
    pipeline_desc.InputLayout.pInputElementDescs = NULL;
    pipeline_desc.InputLayout.NumElements = 0;
    pipeline_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_desc.NumRenderTargets = 1;
    pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline_desc.SampleDesc.Count = 1;

    hr = ID3D12Device_CreateGraphicsPipelineState(
        g_dxx12.device,
        &pipeline_desc,
        &IID_ID3D12PipelineState,
        (void **)&g_dxx12.pipeline_state);

    SAFE_RELEASE(vertex_shader);
    SAFE_RELEASE(pixel_shader);
    return hr;
}

bool dxx12_create(HWND hwnd, int image_width, int image_height)
{
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
    D3D12_COMMAND_QUEUE_DESC queue_desc;
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc;
    D3D12_HEAP_PROPERTIES upload_heap;
    D3D12_RESOURCE_DESC upload_desc;
    RECT client_rect;
    IDXGISwapChain1 *swap_chain1 = NULL;
    HRESULT hr;
    UINT frame_index;

    ZeroMemory(&g_dxx12, sizeof(g_dxx12));
    g_dxx12.hwnd = hwnd;
    g_dxx12.image_width = image_width;
    g_dxx12.image_height = image_height;
    g_dxx12.current_frame_index = -1;
    g_dxx12.vsync_enabled = true;

    if (!GetClientRect(hwnd, &client_rect)) {
        dxx12_set_error_hr("GetClientRect", HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }

    g_dxx12.window_width = client_rect.right - client_rect.left;
    g_dxx12.window_height = client_rect.bottom - client_rect.top;
    if (g_dxx12.window_width <= 0) {
        g_dxx12.window_width = image_width;
    }
    if (g_dxx12.window_height <= 0) {
        g_dxx12.window_height = image_height;
    }

    hr = CreateDXGIFactory2(0, &IID_IDXGIFactory6, (void **)&g_dxx12.factory);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateDXGIFactory2", hr);
        dxx12_destroy();
        return false;
    }

    hr = dxx12_pick_adapter();
    if (FAILED(hr)) {
        dxx12_set_error_hr("DXGI adapter enumeration", hr);
        dxx12_destroy();
        return false;
    }

    hr = D3D12CreateDevice((IUnknown *)g_dxx12.adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&g_dxx12.device);
    if (FAILED(hr)) {
        dxx12_set_error_hr("D3D12CreateDevice", hr);
        dxx12_destroy();
        return false;
    }

    ZeroMemory(&queue_desc, sizeof(queue_desc));
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = ID3D12Device_CreateCommandQueue(g_dxx12.device, &queue_desc, &IID_ID3D12CommandQueue, (void **)&g_dxx12.command_queue);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateCommandQueue", hr);
        dxx12_destroy();
        return false;
    }

    ZeroMemory(&swap_chain_desc, sizeof(swap_chain_desc));
    swap_chain_desc.Width = (UINT)g_dxx12.window_width;
    swap_chain_desc.Height = (UINT)g_dxx12.window_height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = DXX12_FRAME_COUNT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = IDXGIFactory6_CreateSwapChainForHwnd(
        g_dxx12.factory,
        (IUnknown *)g_dxx12.command_queue,
        g_dxx12.hwnd,
        &swap_chain_desc,
        NULL,
        NULL,
        &swap_chain1);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateSwapChainForHwnd", hr);
        dxx12_destroy();
        return false;
    }

    hr = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain4, (void **)&g_dxx12.swap_chain);
    SAFE_RELEASE(swap_chain1);
    if (FAILED(hr)) {
        dxx12_set_error_hr("QueryInterface(IDXGISwapChain4)", hr);
        dxx12_destroy();
        return false;
    }

    hr = IDXGIFactory6_MakeWindowAssociation(g_dxx12.factory, g_dxx12.hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) {
        dxx12_set_error_hr("MakeWindowAssociation", hr);
        dxx12_destroy();
        return false;
    }

    ZeroMemory(&heap_desc, sizeof(heap_desc));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = DXX12_FRAME_COUNT;
    hr = ID3D12Device_CreateDescriptorHeap(g_dxx12.device, &heap_desc, &IID_ID3D12DescriptorHeap, (void **)&g_dxx12.rtv_heap);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateDescriptorHeap(RTV)", hr);
        dxx12_destroy();
        return false;
    }
    g_dxx12.rtv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(g_dxx12.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    ZeroMemory(&heap_desc, sizeof(heap_desc));
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = DXX12_FRAME_COUNT;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = ID3D12Device_CreateDescriptorHeap(g_dxx12.device, &heap_desc, &IID_ID3D12DescriptorHeap, (void **)&g_dxx12.srv_heap);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateDescriptorHeap(SRV)", hr);
        dxx12_destroy();
        return false;
    }
    g_dxx12.srv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(g_dxx12.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    hr = dxx12_create_back_buffers();
    if (FAILED(hr)) {
        dxx12_set_error_hr("GetBuffer(back buffer)", hr);
        dxx12_destroy();
        return false;
    }

    upload_heap = dxx12_heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    upload_desc = dxx12_buffer_desc((UINT64)image_width * (UINT64)image_height * sizeof(vec4_t));

    for (frame_index = 0; frame_index < DXX12_FRAME_COUNT; ++frame_index) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;

        hr = ID3D12Device_CreateCommandAllocator(
            g_dxx12.device,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (void **)&g_dxx12.frames[frame_index].command_allocator);
        if (FAILED(hr)) {
            dxx12_set_error_hr("CreateCommandAllocator", hr);
            dxx12_destroy();
            return false;
        }

        hr = ID3D12Device_CreateCommittedResource(
            g_dxx12.device,
            &upload_heap,
            D3D12_HEAP_FLAG_NONE,
            &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource,
            (void **)&g_dxx12.frames[frame_index].upload_buffer);
        if (FAILED(hr)) {
            dxx12_set_error_hr("CreateCommittedResource(upload buffer)", hr);
            dxx12_destroy();
            return false;
        }

        hr = ID3D12Resource_Map(g_dxx12.frames[frame_index].upload_buffer, 0, NULL, (void **)&g_dxx12.frames[frame_index].mapped_pixels);
        if (FAILED(hr)) {
            dxx12_set_error_hr("Map(upload buffer)", hr);
            dxx12_destroy();
            return false;
        }

        ZeroMemory(&srv_desc, sizeof(srv_desc));
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.NumElements = (UINT)(image_width * image_height);
        srv_desc.Buffer.StructureByteStride = sizeof(vec4_t);
        ID3D12Device_CreateShaderResourceView(
            g_dxx12.device,
            g_dxx12.frames[frame_index].upload_buffer,
            &srv_desc,
            dxx12_get_srv_cpu_handle(frame_index));
    }

    hr = ID3D12Device_CreateCommandList(
        g_dxx12.device,
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_dxx12.frames[0].command_allocator,
        NULL,
        &IID_ID3D12GraphicsCommandList,
        (void **)&g_dxx12.command_list);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateCommandList", hr);
        dxx12_destroy();
        return false;
    }

    hr = ID3D12GraphicsCommandList_Close(g_dxx12.command_list);
    if (FAILED(hr)) {
        dxx12_set_error_hr("Close(command list)", hr);
        dxx12_destroy();
        return false;
    }

    hr = ID3D12Device_CreateFence(g_dxx12.device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&g_dxx12.fence);
    if (FAILED(hr)) {
        dxx12_set_error_hr("CreateFence", hr);
        dxx12_destroy();
        return false;
    }

    g_dxx12.fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (g_dxx12.fence_event == NULL) {
        dxx12_set_error_hr("CreateEventW", HRESULT_FROM_WIN32(GetLastError()));
        dxx12_destroy();
        return false;
    }

    g_dxx12.next_fence_value = 1;

    hr = dxx12_create_pipeline();
    if (FAILED(hr)) {
        if (g_dxx12.error[0] == '\0') {
            dxx12_set_error_hr("CreateGraphicsPipelineState", hr);
        }
        dxx12_destroy();
        return false;
    }

    g_dxx12.ready = true;
    g_dxx12.error[0] = '\0';
    return true;
}

bool dxx12_begin_frame(vec4_t **pixels_out)
{
    UINT frame_index;

    if (!g_dxx12.ready) {
        dxx12_set_error_text("DX12 backend is not ready.");
        return false;
    }

    frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(g_dxx12.swap_chain);
    dxx12_wait_for_frame(frame_index);

    g_dxx12.current_frame_index = (int)frame_index;
    *pixels_out = g_dxx12.frames[frame_index].mapped_pixels;
    return true;
}

bool dxx12_end_frame(void)
{
    dxx12_frame_t *frame;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
    D3D12_RESOURCE_BARRIER barrier;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    ID3D12DescriptorHeap *descriptor_heaps[1];
    ID3D12CommandList *command_lists[1];
    float clear_color[4] = {0.02f, 0.02f, 0.02f, 1.0f};
    HRESULT hr;

    if (!g_dxx12.ready || g_dxx12.current_frame_index < 0) {
        dxx12_set_error_text("No active frame to present.");
        return false;
    }

    frame = &g_dxx12.frames[g_dxx12.current_frame_index];

    hr = ID3D12CommandAllocator_Reset(frame->command_allocator);
    if (FAILED(hr)) {
        dxx12_set_error_hr("Reset(command allocator)", hr);
        return false;
    }

    hr = ID3D12GraphicsCommandList_Reset(g_dxx12.command_list, frame->command_allocator, g_dxx12.pipeline_state);
    if (FAILED(hr)) {
        dxx12_set_error_hr("Reset(command list)", hr);
        return false;
    }

    barrier = dxx12_transition_barrier(frame->back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12GraphicsCommandList_ResourceBarrier(g_dxx12.command_list, 1, &barrier);

    rtv_handle = dxx12_get_rtv_handle((UINT)g_dxx12.current_frame_index);
    ID3D12GraphicsCommandList_OMSetRenderTargets(g_dxx12.command_list, 1, &rtv_handle, FALSE, NULL);
    ID3D12GraphicsCommandList_ClearRenderTargetView(g_dxx12.command_list, rtv_handle, clear_color, 0, NULL);

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)g_dxx12.window_width;
    viewport.Height = (float)g_dxx12.window_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    scissor_rect.left = 0;
    scissor_rect.top = 0;
    scissor_rect.right = g_dxx12.window_width;
    scissor_rect.bottom = g_dxx12.window_height;

    ID3D12GraphicsCommandList_RSSetViewports(g_dxx12.command_list, 1, &viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(g_dxx12.command_list, 1, &scissor_rect);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(g_dxx12.command_list, g_dxx12.root_signature);

    descriptor_heaps[0] = g_dxx12.srv_heap;
    ID3D12GraphicsCommandList_SetDescriptorHeaps(g_dxx12.command_list, 1, descriptor_heaps);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
        g_dxx12.command_list,
        0,
        dxx12_get_srv_gpu_handle((UINT)g_dxx12.current_frame_index));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(g_dxx12.command_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_DrawInstanced(g_dxx12.command_list, 3, 1, 0, 0);

    barrier = dxx12_transition_barrier(frame->back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ID3D12GraphicsCommandList_ResourceBarrier(g_dxx12.command_list, 1, &barrier);

    hr = ID3D12GraphicsCommandList_Close(g_dxx12.command_list);
    if (FAILED(hr)) {
        dxx12_set_error_hr("Close(command list)", hr);
        return false;
    }

    command_lists[0] = (ID3D12CommandList *)g_dxx12.command_list;
    ID3D12CommandQueue_ExecuteCommandLists(g_dxx12.command_queue, 1, command_lists);

    hr = IDXGISwapChain4_Present(g_dxx12.swap_chain, g_dxx12.vsync_enabled ? 1u : 0u, 0);
    if (FAILED(hr)) {
        dxx12_set_error_hr("Present", hr);
        return false;
    }

    frame->fence_value = g_dxx12.next_fence_value++;
    hr = ID3D12CommandQueue_Signal(g_dxx12.command_queue, g_dxx12.fence, frame->fence_value);
    if (FAILED(hr)) {
        dxx12_set_error_hr("Signal(fence)", hr);
        return false;
    }
    g_dxx12.current_frame_index = -1;
    return true;
}

void dxx12_destroy(void)
{
    UINT frame_index;

    dxx12_wait_for_gpu();

    for (frame_index = 0; frame_index < DXX12_FRAME_COUNT; ++frame_index) {
        if (g_dxx12.frames[frame_index].upload_buffer != NULL && g_dxx12.frames[frame_index].mapped_pixels != NULL) {
            ID3D12Resource_Unmap(g_dxx12.frames[frame_index].upload_buffer, 0, NULL);
            g_dxx12.frames[frame_index].mapped_pixels = NULL;
        }

        SAFE_RELEASE(g_dxx12.frames[frame_index].upload_buffer);
        SAFE_RELEASE(g_dxx12.frames[frame_index].back_buffer);
        SAFE_RELEASE(g_dxx12.frames[frame_index].command_allocator);
        g_dxx12.frames[frame_index].fence_value = 0;
    }

    SAFE_RELEASE(g_dxx12.pipeline_state);
    SAFE_RELEASE(g_dxx12.root_signature);
    SAFE_RELEASE(g_dxx12.command_list);
    SAFE_RELEASE(g_dxx12.srv_heap);
    SAFE_RELEASE(g_dxx12.rtv_heap);
    SAFE_RELEASE(g_dxx12.swap_chain);
    SAFE_RELEASE(g_dxx12.fence);
    SAFE_RELEASE(g_dxx12.command_queue);
    SAFE_RELEASE(g_dxx12.device);
    SAFE_RELEASE(g_dxx12.adapter);
    SAFE_RELEASE(g_dxx12.factory);

    if (g_dxx12.fence_event != NULL) {
        CloseHandle(g_dxx12.fence_event);
        g_dxx12.fence_event = NULL;
    }

    g_dxx12.ready = false;
}
