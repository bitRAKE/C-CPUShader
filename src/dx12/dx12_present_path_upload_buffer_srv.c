#include "dx12_internal.h"

#include <string.h>

static const char g_shader_source[] =
    "#ifndef SHADER_COLOR_SPACE\r\n"
    "#define SHADER_COLOR_SPACE 0\r\n"
    "#endif\r\n"
    "#ifndef SURFACE_KIND\r\n"
    "#define SURFACE_KIND 0\r\n"
    "#endif\r\n"
    "\r\n"
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
    "float3 linear_to_srgb(float3 value)\n"
    "{\n"
    "    float3 clamped = saturate(value);\n"
    "    float3 lower = clamped * 12.92;\n"
    "    float3 upper = 1.055 * pow(clamped, 1.0 / 2.4) - 0.055;\n"
    "    return lerp(upper, lower, step(clamped, float3(0.0031308, 0.0031308, 0.0031308)));\n"
    "}\n"
    "\n"
    "float3 srgb_to_linear(float3 value)\n"
    "{\n"
    "    float3 clamped = saturate(value);\n"
    "    float3 lower = clamped / 12.92;\n"
    "    float3 upper = pow((clamped + 0.055) / 1.055, 2.4);\n"
    "    return lerp(upper, lower, step(clamped, float3(0.04045, 0.04045, 0.04045)));\n"
    "}\n"
    "\n"
    "float3 rec709_to_rec2020(float3 value)\n"
    "{\n"
    "    return float3(\n"
    "        dot(value, float3(0.6274040, 0.3292820, 0.0433136)),\n"
    "        dot(value, float3(0.0690970, 0.9195400, 0.0113612)),\n"
    "        dot(value, float3(0.0163916, 0.0880132, 0.8955950)));\n"
    "}\n"
    "\n"
    "float3 rec2020_to_rec709(float3 value)\n"
    "{\n"
    "    return float3(\n"
    "        dot(value, float3(1.6605, -0.5876, -0.0728)),\n"
    "        dot(value, float3(-0.1246, 1.1329, -0.0083)),\n"
    "        dot(value, float3(-0.0182, -0.1006, 1.1187)));\n"
    "}\n"
    "\n"
    "float pq_encode_from_nits(float nits)\n"
    "{\n"
    "    const float m1 = 2610.0 / 16384.0;\n"
    "    const float m2 = 2523.0 / 32.0;\n"
    "    const float c1 = 3424.0 / 4096.0;\n"
    "    const float c2 = 2413.0 / 128.0;\n"
    "    const float c3 = 2392.0 / 128.0;\n"
    "    float normalized = saturate(nits / 10000.0);\n"
    "    float p = pow(normalized, m1);\n"
    "    return pow((c1 + c2 * p) / (1.0 + c3 * p), m2);\n"
    "}\n"
    "\n"
    "float pq_decode_to_nits(float encoded)\n"
    "{\n"
    "    const float m1 = 2610.0 / 16384.0;\n"
    "    const float m2 = 2523.0 / 32.0;\n"
    "    const float c1 = 3424.0 / 4096.0;\n"
    "    const float c2 = 2413.0 / 128.0;\n"
    "    const float c3 = 2392.0 / 128.0;\n"
    "    float p = pow(saturate(encoded), 1.0 / m2);\n"
    "    float numerator = max(p - c1, 0.0);\n"
    "    float denominator = max(c2 - c3 * p, 0.000001);\n"
    "    return 10000.0 * pow(numerator / denominator, 1.0 / m1);\n"
    "}\n"
    "\n"
    "float3 decode_to_linear_rec709(float3 color)\n"
    "{\n"
    "#if SHADER_COLOR_SPACE == 0\n"
    "    return srgb_to_linear(color);\n"
    "#elif SHADER_COLOR_SPACE == 2\n"
    "    float3 nits2020 = float3(\n"
    "        pq_decode_to_nits(color.r),\n"
    "        pq_decode_to_nits(color.g),\n"
    "        pq_decode_to_nits(color.b));\n"
    "    float3 linear2020 = nits2020 / 80.0;\n"
    "    return max(rec2020_to_rec709(linear2020), 0.0);\n"
    "#else\n"
    "    return max(color, 0.0);\n"
    "#endif\n"
    "}\n"
    "\n"
    "float3 encode_for_surface(float3 color)\n"
    "{\n"
    "    float3 linear709 = decode_to_linear_rec709(color);\n"
    "#if SURFACE_KIND == 1\n"
    "    return linear709;\n"
    "#elif SURFACE_KIND == 2\n"
    "#if SHADER_COLOR_SPACE == 2\n"
    "    return saturate(color);\n"
    "#else\n"
    "    float3 rec2020 = max(rec709_to_rec2020(linear709), 0.0);\n"
    "    float3 nits = rec2020 * 80.0;\n"
    "    return float3(\n"
    "        pq_encode_from_nits(nits.r),\n"
    "        pq_encode_from_nits(nits.g),\n"
    "        pq_encode_from_nits(nits.b));\n"
    "#endif\n"
    "#else\n"
    "#if SHADER_COLOR_SPACE == 0\n"
    "    return saturate(color);\n"
    "#else\n"
    "    return linear_to_srgb(linear709);\n"
    "#endif\n"
    "#endif\n"
    "}\n"
    "\n"
    "float4 PSMain(VSOut input) : SV_Target\n"
    "{\n"
    "    float2 uv = saturate(input.uv);\n"
    "    uint x = min((uint)(uv.x * (float)IMAGE_WIDTH), (uint)(IMAGE_WIDTH - 1));\n"
    "    uint y = min((uint)(uv.y * (float)IMAGE_HEIGHT), (uint)(IMAGE_HEIGHT - 1));\n"
    "    uint flipped_y = (uint)(IMAGE_HEIGHT - 1) - y;\n"
    "    float4 sample = g_image[flipped_y * IMAGE_WIDTH + x];\n"
    "    return float4(encode_for_surface(sample.rgb), sample.a);\n"
    "}\n";

static HRESULT dx12_upload_buffer_srv_create_pipeline(void)
{
    D3D12_DESCRIPTOR_RANGE descriptor_range;
    D3D12_ROOT_PARAMETER root_parameter;
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
    ID3DBlob *serialized_root = NULL;
    ID3DBlob *root_errors = NULL;
    ID3DBlob *vertex_shader = NULL;
    ID3DBlob *pixel_shader = NULL;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc;
    D3D_SHADER_MACRO macros[5];
    char width_text[32];
    char height_text[32];
    char shader_color_space_text[16];
    char surface_kind_text[16];
    HRESULT hr;

    snprintf(width_text, sizeof(width_text), "%d", g_dx12.image_width);
    snprintf(height_text, sizeof(height_text), "%d", g_dx12.image_height);
    snprintf(shader_color_space_text, sizeof(shader_color_space_text), "%d", (int)g_dx12.shader_color_space);
    snprintf(surface_kind_text, sizeof(surface_kind_text), "%d", (int)g_dx12.surface_kind);

    macros[0].Name = "IMAGE_WIDTH";
    macros[0].Definition = width_text;
    macros[1].Name = "IMAGE_HEIGHT";
    macros[1].Definition = height_text;
    macros[2].Name = "SHADER_COLOR_SPACE";
    macros[2].Definition = shader_color_space_text;
    macros[3].Name = "SURFACE_KIND";
    macros[3].Definition = surface_kind_text;
    macros[4].Name = NULL;
    macros[4].Definition = NULL;

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
            dx12_set_error_text((const char *)ID3D10Blob_GetBufferPointer((ID3D10Blob *)root_errors));
        } else {
            dx12_set_error_hr("D3D12SerializeRootSignature", hr);
        }

        SAFE_RELEASE(root_errors);
        SAFE_RELEASE(serialized_root);
        return hr;
    }

    hr = ID3D12Device_CreateRootSignature(
        g_dx12.device,
        0,
        ID3D10Blob_GetBufferPointer((ID3D10Blob *)serialized_root),
        ID3D10Blob_GetBufferSize((ID3D10Blob *)serialized_root),
        &IID_ID3D12RootSignature,
        (void **)&g_dx12.root_signature);
    SAFE_RELEASE(root_errors);
    SAFE_RELEASE(serialized_root);
    if (FAILED(hr)) {
        return hr;
    }

    hr = dx12_compile_shader(g_shader_source, macros, "VSMain", "vs_5_1", &vertex_shader);
    if (FAILED(hr)) {
        return hr;
    }

    hr = dx12_compile_shader(g_shader_source, macros, "PSMain", "ps_5_1", &pixel_shader);
    if (FAILED(hr)) {
        SAFE_RELEASE(vertex_shader);
        return hr;
    }

    ZeroMemory(&pipeline_desc, sizeof(pipeline_desc));
    pipeline_desc.pRootSignature = g_dx12.root_signature;
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
    pipeline_desc.RTVFormats[0] = g_dx12.swap_chain_format;
    pipeline_desc.SampleDesc.Count = 1;

    hr = ID3D12Device_CreateGraphicsPipelineState(
        g_dx12.device,
        &pipeline_desc,
        &IID_ID3D12PipelineState,
        (void **)&g_dx12.pipeline_state);

    SAFE_RELEASE(vertex_shader);
    SAFE_RELEASE(pixel_shader);
    return hr;
}

HRESULT dx12_upload_buffer_srv_create(void)
{
    D3D12_HEAP_PROPERTIES upload_heap;
    D3D12_RESOURCE_DESC upload_desc;
    UINT frame_index;
    HRESULT hr;

    upload_heap = dx12_heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    upload_desc = dx12_buffer_desc((UINT64)g_dx12.image_width * (UINT64)g_dx12.image_height * sizeof(vec4_t));

    for (frame_index = 0; frame_index < DX12_FRAME_COUNT; ++frame_index) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;

        hr = ID3D12Device_CreateCommandAllocator(
            g_dx12.device,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (void **)&g_dx12.frames[frame_index].command_allocator);
        if (FAILED(hr)) {
            dx12_set_error_hr("CreateCommandAllocator", hr);
            return hr;
        }

        hr = ID3D12Device_CreateCommittedResource(
            g_dx12.device,
            &upload_heap,
            D3D12_HEAP_FLAG_NONE,
            &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &IID_ID3D12Resource,
            (void **)&g_dx12.frames[frame_index].upload_buffer);
        if (FAILED(hr)) {
            dx12_set_error_hr("CreateCommittedResource(upload buffer)", hr);
            return hr;
        }

        hr = ID3D12Resource_Map(g_dx12.frames[frame_index].upload_buffer, 0, NULL, (void **)&g_dx12.frames[frame_index].mapped_pixels);
        if (FAILED(hr)) {
            dx12_set_error_hr("Map(upload buffer)", hr);
            return hr;
        }

        ZeroMemory(&srv_desc, sizeof(srv_desc));
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.NumElements = (UINT)(g_dx12.image_width * g_dx12.image_height);
        srv_desc.Buffer.StructureByteStride = sizeof(vec4_t);
        ID3D12Device_CreateShaderResourceView(
            g_dx12.device,
            g_dx12.frames[frame_index].upload_buffer,
            &srv_desc,
            dx12_get_srv_cpu_handle(frame_index));
    }

    hr = ID3D12Device_CreateCommandList(
        g_dx12.device,
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_dx12.frames[0].command_allocator,
        NULL,
        &IID_ID3D12GraphicsCommandList,
        (void **)&g_dx12.command_list);
    if (FAILED(hr)) {
        dx12_set_error_hr("CreateCommandList", hr);
        return hr;
    }

    hr = ID3D12GraphicsCommandList_Close(g_dx12.command_list);
    if (FAILED(hr)) {
        dx12_set_error_hr("Close(command list)", hr);
        return hr;
    }

    hr = dx12_upload_buffer_srv_create_pipeline();
    if (FAILED(hr) && g_dx12.error[0] == '\0') {
        dx12_set_error_hr("CreateGraphicsPipelineState", hr);
    }

    return hr;
}

static void dx12_copy_surface_to_upload(vec4_t *dst_pixels, const f32x4_surface_t *surface)
{
    size_t row_bytes = (size_t)surface->width * sizeof(vec4_t);

    if (surface->stride_bytes == (int)row_bytes) {
        memcpy(dst_pixels, surface->pixels, row_bytes * (size_t)surface->height);
        return;
    }

    for (int y = 0; y < surface->height; y++) {
        const char *src_row = (const char *)surface->pixels + (size_t)y * (size_t)surface->stride_bytes;
        char *dst_row = (char *)dst_pixels + (size_t)y * row_bytes;
        memcpy(dst_row, src_row, row_bytes);
    }
}

bool dx12_upload_buffer_srv_present(const f32x4_surface_t *surface)
{
    dx12_frame_t *frame;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
    D3D12_RESOURCE_BARRIER barrier;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    ID3D12DescriptorHeap *descriptor_heaps[1];
    ID3D12CommandList *command_lists[1];
    float clear_color[4] = {0.02f, 0.02f, 0.02f, 1.0f};
    UINT frame_index;
    HRESULT hr;

    if (surface == NULL || surface->pixels == NULL) {
        dx12_set_error_text("DXGI present surface is invalid.");
        return false;
    }

    if (surface->width != g_dx12.image_width || surface->height != g_dx12.image_height) {
        dx12_set_error_text("DXGI present surface dimensions do not match the backend.");
        return false;
    }

    if (surface->stride_bytes < (int)((size_t)surface->width * sizeof(vec4_t))) {
        dx12_set_error_text("DXGI present surface stride is invalid.");
        return false;
    }

    frame_index = IDXGISwapChain4_GetCurrentBackBufferIndex(g_dx12.swap_chain);
    dx12_wait_for_frame(frame_index);
    g_dx12.current_frame_index = (int)frame_index;
    frame = &g_dx12.frames[frame_index];

    dx12_copy_surface_to_upload(frame->mapped_pixels, surface);

    hr = ID3D12CommandAllocator_Reset(frame->command_allocator);
    if (FAILED(hr)) {
        dx12_set_error_hr("Reset(command allocator)", hr);
        return false;
    }

    hr = ID3D12GraphicsCommandList_Reset(g_dx12.command_list, frame->command_allocator, g_dx12.pipeline_state);
    if (FAILED(hr)) {
        dx12_set_error_hr("Reset(command list)", hr);
        return false;
    }

    barrier = dx12_transition_barrier(frame->back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12GraphicsCommandList_ResourceBarrier(g_dx12.command_list, 1, &barrier);

    rtv_handle = dx12_get_rtv_handle(frame_index);
    ID3D12GraphicsCommandList_OMSetRenderTargets(g_dx12.command_list, 1, &rtv_handle, FALSE, NULL);
    ID3D12GraphicsCommandList_ClearRenderTargetView(g_dx12.command_list, rtv_handle, clear_color, 0, NULL);

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)g_dx12.window_width;
    viewport.Height = (float)g_dx12.window_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    scissor_rect.left = 0;
    scissor_rect.top = 0;
    scissor_rect.right = g_dx12.window_width;
    scissor_rect.bottom = g_dx12.window_height;

    ID3D12GraphicsCommandList_RSSetViewports(g_dx12.command_list, 1, &viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(g_dx12.command_list, 1, &scissor_rect);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(g_dx12.command_list, g_dx12.root_signature);

    descriptor_heaps[0] = g_dx12.srv_heap;
    ID3D12GraphicsCommandList_SetDescriptorHeaps(g_dx12.command_list, 1, descriptor_heaps);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(g_dx12.command_list, 0, dx12_get_srv_gpu_handle(frame_index));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(g_dx12.command_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_DrawInstanced(g_dx12.command_list, 3, 1, 0, 0);

    barrier = dx12_transition_barrier(frame->back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ID3D12GraphicsCommandList_ResourceBarrier(g_dx12.command_list, 1, &barrier);

    hr = ID3D12GraphicsCommandList_Close(g_dx12.command_list);
    if (FAILED(hr)) {
        dx12_set_error_hr("Close(command list)", hr);
        return false;
    }

    command_lists[0] = (ID3D12CommandList *)g_dx12.command_list;
    ID3D12CommandQueue_ExecuteCommandLists(g_dx12.command_queue, 1, command_lists);

    hr = IDXGISwapChain4_Present(g_dx12.swap_chain, g_dx12.vsync_enabled ? 1u : 0u, 0);
    if (FAILED(hr)) {
        dx12_set_error_hr("Present", hr);
        return false;
    }

    frame->fence_value = g_dx12.next_fence_value++;
    hr = ID3D12CommandQueue_Signal(g_dx12.command_queue, g_dx12.fence, frame->fence_value);
    if (FAILED(hr)) {
        dx12_set_error_hr("Signal(fence)", hr);
        return false;
    }

    g_dx12.current_frame_index = -1;
    return true;
}

void dx12_upload_buffer_srv_destroy(void)
{
    UINT frame_index;

    for (frame_index = 0; frame_index < DX12_FRAME_COUNT; ++frame_index) {
        if (g_dx12.frames[frame_index].upload_buffer != NULL && g_dx12.frames[frame_index].mapped_pixels != NULL) {
            ID3D12Resource_Unmap(g_dx12.frames[frame_index].upload_buffer, 0, NULL);
            g_dx12.frames[frame_index].mapped_pixels = NULL;
        }

        SAFE_RELEASE(g_dx12.frames[frame_index].upload_buffer);
        SAFE_RELEASE(g_dx12.frames[frame_index].command_allocator);
    }

    SAFE_RELEASE(g_dx12.pipeline_state);
    SAFE_RELEASE(g_dx12.root_signature);
    SAFE_RELEASE(g_dx12.command_list);
}
