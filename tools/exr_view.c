/*
 * exr_view.exe -- Minimal HDR viewer for CPU_Shader EXR captures.
 *
 * Displays a single uncompressed 32-bit float RGBA EXR file through a DXGI
 * scRGB swap chain (R16G16B16A16_FLOAT + DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709)
 * so the OS compositor can present HDR values to an HDR-capable monitor.
 *
 * Requires an HDR display.  If the output does not report HDR capability,
 * a message box is shown and the program exits.  There is no SDR fallback --
 * displaying HDR linear data through an SDR pipeline would produce incorrect
 * colors, defeating the purpose of the EXR capture format.
 *
 * Only reads the specific EXR subset produced by capture_exr.c:
 *   - Single-part, scanline, INCREASING_Y
 *   - NO_COMPRESSION
 *   - 4 FLOAT channels (A, B, G, R in alphabetical order)
 *
 * Usage: exr_view.exe <file.exr>
 * Exits silently on any error.  Close the window or press Escape to quit.
 *
 * Build:
 *   clang -std=c17 -Wall -Wextra -Werror -O2 -DUNICODE -D_UNICODE
 *         -DWIN32_LEAN_AND_MEAN -target x86_64-pc-windows-msvc
 *         -o tools\exr_view.exe tools\exr_view.c
 *         -ld3d11 -ldxgi -ldxguid -luser32 -lkernel32 -lshell32 -fuse-ld=lld
 */

#define _CRT_SECURE_NO_WARNINGS

#ifndef COBJMACROS
#define COBJMACROS
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "kernel32.lib")

/* ---- EXR reader (our subset only) --------------------------------------- */

typedef struct {
    uint32_t width;
    uint32_t height;
    float   *rgba;          /* interleaved RGBA, top-down, width*height*4 floats */
} exr_image_t;

static int exr_read_i32(const uint8_t *p) {
    return (int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static float exr_read_f32(const uint8_t *p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

static const uint8_t *exr_skip_str(const uint8_t *p, const uint8_t *end)
{
    while (p < end) {
        if (*p++ == '\0') return p;
    }
    return NULL;
}

static int exr_load(const char *path, exr_image_t *img)
{
    HANDLE file;
    HANDLE mapping;
    const uint8_t *base;
    const uint8_t *p;
    const uint8_t *end;
    LARGE_INTEGER file_size;
    int32_t data_x_min = 0, data_y_min = 0, data_x_max = 0, data_y_max = 0;
    uint32_t width, height;
    int found_data_window = 0;
    int channel_count = 0;

    img->width = 0;
    img->height = 0;
    img->rgba = NULL;

    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;

    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 16) {
        CloseHandle(file);
        return 0;
    }

    mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping == NULL) {
        CloseHandle(file);
        return 0;
    }

    base = (const uint8_t *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (base == NULL) {
        CloseHandle(mapping);
        CloseHandle(file);
        return 0;
    }

    end = base + file_size.QuadPart;
    p = base;

    /* Magic number */
    if (exr_read_i32(p) != 20000630) goto fail;
    p += 4;

    /* Version -- we only support version 2, single-part scanline */
    {
        int32_t ver = exr_read_i32(p);
        if ((ver & 0xFF) < 2) goto fail;
        if ((ver & 0x00000200) != 0) goto fail;    /* tiled */
        p += 4;
    }

    /* Parse header attributes */
    while (p < end && *p != '\0') {
        const uint8_t *attr_name = p;
        const uint8_t *attr_type;
        int32_t attr_size;

        p = exr_skip_str(p, end);
        if (p == NULL) goto fail;
        attr_type = p;
        p = exr_skip_str(p, end);
        if (p == NULL || p + 4 > end) goto fail;
        attr_size = exr_read_i32(p);
        p += 4;
        if (attr_size < 0 || p + attr_size > end) goto fail;

        if (strcmp((const char *)attr_name, "dataWindow") == 0 && attr_size == 16) {
            data_x_min = exr_read_i32(p);
            data_y_min = exr_read_i32(p + 4);
            data_x_max = exr_read_i32(p + 8);
            data_y_max = exr_read_i32(p + 12);
            found_data_window = 1;
        }

        if (strcmp((const char *)attr_name, "channels") == 0 &&
            strcmp((const char *)attr_type, "chlist") == 0) {
            const uint8_t *ch = p;
            const uint8_t *ch_end = p + attr_size;
            channel_count = 0;
            while (ch < ch_end && *ch != '\0') {
                ch = exr_skip_str(ch, ch_end);
                if (ch == NULL || ch + 16 > ch_end) goto fail;
                ch += 16;
                channel_count++;
            }
        }

        p += attr_size;
    }

    if (p >= end) goto fail;
    p++;

    if (!found_data_window) goto fail;
    if (channel_count != 4) goto fail;
    if (data_x_min != 0 || data_y_min != 0) goto fail;

    width = (uint32_t)(data_x_max - data_x_min + 1);
    height = (uint32_t)(data_y_max - data_y_min + 1);
    if (width == 0 || height == 0 || width > 16384 || height > 16384) goto fail;

    /* Skip offset table */
    if (p + (size_t)height * 8 > end) goto fail;
    p += (size_t)height * 8;

    img->rgba = (float *)malloc((size_t)width * height * 4 * sizeof(float));
    if (img->rgba == NULL) goto fail;

    /* Read scanlines: channel-planar A,B,G,R -> interleaved RGBA */
    {
        size_t scanline_bytes = (size_t)width * 4 * sizeof(float);

        for (uint32_t y = 0; y < height; y++) {
            int32_t scanline_y, data_size;
            const float *ch_a, *ch_b, *ch_g, *ch_r;
            float *dst;

            if (p + 8 > end) goto fail;
            scanline_y = exr_read_i32(p); p += 4;
            data_size = exr_read_i32(p); p += 4;
            (void)scanline_y;

            if ((uint32_t)data_size != scanline_bytes) goto fail;
            if (p + data_size > end) goto fail;

            ch_a = (const float *)(p);
            ch_b = (const float *)(p + (size_t)width * sizeof(float));
            ch_g = (const float *)(p + (size_t)width * sizeof(float) * 2);
            ch_r = (const float *)(p + (size_t)width * sizeof(float) * 3);
            dst = img->rgba + (size_t)y * width * 4;

            for (uint32_t x = 0; x < width; x++) {
                dst[x * 4 + 0] = exr_read_f32((const uint8_t *)&ch_r[x]);
                dst[x * 4 + 1] = exr_read_f32((const uint8_t *)&ch_g[x]);
                dst[x * 4 + 2] = exr_read_f32((const uint8_t *)&ch_b[x]);
                dst[x * 4 + 3] = exr_read_f32((const uint8_t *)&ch_a[x]);
            }

            p += data_size;
        }
    }

    img->width = width;
    img->height = height;

    UnmapViewOfFile(base);
    CloseHandle(mapping);
    CloseHandle(file);
    return 1;

fail:
    if (img->rgba != NULL) { free(img->rgba); img->rgba = NULL; }
    UnmapViewOfFile(base);
    CloseHandle(mapping);
    CloseHandle(file);
    return 0;
}

/* ---- Float-to-half conversion ------------------------------------------- */

static uint16_t float_to_half(float value)
{
    uint32_t bits;
    uint32_t sign, exp, mantissa;
    int32_t new_exp;

    memcpy(&bits, &value, 4);
    sign = (bits >> 16) & 0x8000;
    exp = (bits >> 23) & 0xFF;
    mantissa = bits & 0x007FFFFF;

    if (exp == 0xFF) {
        return (uint16_t)(sign | 0x7C00 | (mantissa ? 0x0200 : 0));
    }

    if (exp == 0) {
        return (uint16_t)sign;
    }

    new_exp = (int32_t)exp - 127 + 15;

    if (new_exp >= 31) {
        return (uint16_t)(sign | 0x7C00);
    }

    if (new_exp <= 0) {
        if (new_exp < -10) return (uint16_t)sign;
        mantissa |= 0x00800000;
        {
            int shift = 14 - new_exp;
            uint32_t round_bit = 1u << (shift - 1);
            uint32_t result = mantissa >> shift;
            if ((mantissa & round_bit) && ((mantissa & (round_bit - 1)) || (result & 1))) {
                result++;
            }
            return (uint16_t)(sign | result);
        }
    }

    {
        uint32_t round_bit = 1u << 12;
        uint16_t half = (uint16_t)(sign | ((uint32_t)new_exp << 10) | (mantissa >> 13));
        if ((mantissa & round_bit) && ((mantissa & (round_bit - 1)) || (half & 1))) {
            half++;
        }
        return half;
    }
}

/* ---- Window procedure --------------------------------------------------- */

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---- Entry point -------------------------------------------------------- */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    exr_image_t img = {0};
    char exr_path[MAX_PATH] = "";
    WCHAR window_title[MAX_PATH + 32];
    WNDCLASSEXW wc;
    HWND hwnd;
    RECT wr;
    MSG msg;
    int argc;
    LPWSTR *argv;
    DWORD style;

    /* D3D11 / DXGI state */
    ID3D11Device            *device = NULL;
    ID3D11DeviceContext     *ctx = NULL;
    IDXGIDevice1            *dxgi_device = NULL;
    IDXGIAdapter            *adapter = NULL;
    IDXGIFactory2           *factory = NULL;
    IDXGISwapChain1         *swap_chain1 = NULL;
    IDXGISwapChain4         *swap_chain = NULL;
    IDXGIOutput             *output = NULL;
    IDXGIOutput6            *output6 = NULL;
    ID3D11Texture2D         *back_buffer = NULL;
    ID3D11Texture2D         *staging = NULL;
    DXGI_SWAP_CHAIN_DESC1    sc_desc;
    DXGI_OUTPUT_DESC1        output_desc;
    D3D11_TEXTURE2D_DESC     tex_desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    D3D_FEATURE_LEVEL        feature_level;
    HRESULT                  hr;
    UINT                     cs_support = 0;

    (void)hPrev;
    (void)lpCmd;
    (void)nShow;

    /* ---- Parse command line ---- */

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL || argc < 2) return 1;

    if (WideCharToMultiByte(CP_ACP, 0, argv[1], -1, exr_path, MAX_PATH, NULL, NULL) <= 0)
        return 1;
    LocalFree(argv);

    /* ---- Load EXR ---- */

    if (!exr_load(exr_path, &img)) return 1;

    /* ---- Create D3D11 device and detect HDR ---- */

    {
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        hr = D3D11CreateDevice(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
            levels, 1, D3D11_SDK_VERSION,
            &device, &feature_level, &ctx);
        if (FAILED(hr)) goto cleanup;
    }

    hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice1, (void **)&dxgi_device);
    if (FAILED(hr)) goto cleanup;

    hr = IDXGIDevice1_GetAdapter(dxgi_device, &adapter);
    if (FAILED(hr)) goto cleanup;

    hr = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, (void **)&factory);
    if (FAILED(hr)) goto cleanup;

    /* Probe the primary output for HDR capability */
    {
        int hdr_supported = 0;

        hr = IDXGIAdapter_EnumOutputs(adapter, 0, &output);
        if (SUCCEEDED(hr) && output != NULL) {
            hr = IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput6, (void **)&output6);
            if (SUCCEEDED(hr) && output6 != NULL) {
                ZeroMemory(&output_desc, sizeof(output_desc));
                hr = IDXGIOutput6_GetDesc1(output6, &output_desc);
                if (SUCCEEDED(hr) &&
                    output_desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                {
                    hdr_supported = 1;
                }
            }
        }

        if (!hdr_supported) {
            MessageBoxA(
                NULL,
                "This display does not support HDR.\n\n"
                "EXR captures contain scene-linear scRGB data that requires an HDR display\n"
                "for correct presentation.  Displaying through an SDR pipeline would produce\n"
                "incorrect colors.\n\n"
                "Enable HDR in Windows Display Settings and try again.",
                "EXR View - HDR Required",
                MB_OK | MB_ICONINFORMATION);
            goto cleanup;
        }
    }

    /* ---- Create window ---- */

    {
        const char *filename = strrchr(exr_path, '\\');
        if (filename == NULL) filename = strrchr(exr_path, '/');
        filename = (filename != NULL) ? filename + 1 : exr_path;
        _snwprintf(window_title, sizeof(window_title) / sizeof(window_title[0]),
            L"EXR View - %hs (%ux%u) [HDR scRGB]", filename, img.width, img.height);
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"EXRViewClass";
    if (!RegisterClassExW(&wc)) goto cleanup;

    style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    wr.left = 0; wr.top = 0;
    wr.right = (LONG)img.width;
    wr.bottom = (LONG)img.height;
    AdjustWindowRect(&wr, style, FALSE);

    hwnd = CreateWindowExW(
        0, L"EXRViewClass", window_title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) goto cleanup;

    /* ---- Create scRGB swap chain ---- */

    ZeroMemory(&sc_desc, sizeof(sc_desc));
    sc_desc.Width = img.width;
    sc_desc.Height = img.height;
    sc_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    sc_desc.SampleDesc.Count = 1;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = 2;
    sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = IDXGIFactory2_CreateSwapChainForHwnd(
        factory, (IUnknown *)device, hwnd, &sc_desc, NULL, NULL, &swap_chain1);
    if (FAILED(hr)) goto cleanup;

    hr = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain4, (void **)&swap_chain);
    if (FAILED(hr)) goto cleanup;

    hr = IDXGISwapChain4_CheckColorSpaceSupport(
        swap_chain, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, &cs_support);
    if (FAILED(hr) || !(cs_support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
        goto cleanup;

    IDXGISwapChain4_SetColorSpace1(swap_chain, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);

    /* ---- Create staging texture and fill with EXR data ---- */

    ZeroMemory(&tex_desc, sizeof(tex_desc));
    tex_desc.Width = img.width;
    tex_desc.Height = img.height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_STAGING;
    tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateTexture2D(device, &tex_desc, NULL, &staging);
    if (FAILED(hr)) goto cleanup;

    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_WRITE, 0, &mapped);
    if (FAILED(hr)) goto cleanup;

    for (uint32_t y = 0; y < img.height; y++) {
        const float *src = img.rgba + (size_t)y * img.width * 4;
        uint16_t *dst = (uint16_t *)((uint8_t *)mapped.pData + (size_t)y * mapped.RowPitch);
        for (uint32_t x = 0; x < img.width; x++) {
            dst[x * 4 + 0] = float_to_half(src[x * 4 + 0]);
            dst[x * 4 + 1] = float_to_half(src[x * 4 + 1]);
            dst[x * 4 + 2] = float_to_half(src[x * 4 + 2]);
            dst[x * 4 + 3] = float_to_half(src[x * 4 + 3]);
        }
    }

    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)staging, 0);

    /* Copy staging -> back buffer and present */
    hr = IDXGISwapChain4_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&back_buffer);
    if (FAILED(hr)) goto cleanup;

    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)back_buffer, (ID3D11Resource *)staging);

    hr = IDXGISwapChain4_Present(swap_chain, 1, 0);
    if (FAILED(hr)) goto cleanup;

    ShowWindow(hwnd, SW_SHOWDEFAULT);

    /* ---- Message loop ---- */

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

cleanup:
    if (back_buffer) IUnknown_Release((IUnknown *)back_buffer);
    if (staging) IUnknown_Release((IUnknown *)staging);
    if (swap_chain) IUnknown_Release((IUnknown *)swap_chain);
    if (swap_chain1) IUnknown_Release((IUnknown *)swap_chain1);
    if (output6) IUnknown_Release((IUnknown *)output6);
    if (output) IUnknown_Release((IUnknown *)output);
    if (factory) IUnknown_Release((IUnknown *)factory);
    if (adapter) IUnknown_Release((IUnknown *)adapter);
    if (dxgi_device) IUnknown_Release((IUnknown *)dxgi_device);
    if (ctx) IUnknown_Release((IUnknown *)ctx);
    if (device) IUnknown_Release((IUnknown *)device);
    if (img.rgba) free(img.rgba);

    return 0;
}
