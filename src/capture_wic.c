#define COBJMACROS

#include "capture_wic.h"

#include <wincodec.h>
#include <propidl.h>
#include <stdlib.h>
#include <string.h>

static void capture_wic_set_error(char *error_text, size_t error_text_size, const char *text)
{
    if (error_text != NULL && error_text_size > 0) {
        snprintf(error_text, error_text_size, "%s", text != NULL ? text : "");
    }
}

static void capture_wic_set_error_hr(char *error_text, size_t error_text_size, const char *what, HRESULT hr)
{
    if (error_text != NULL && error_text_size > 0) {
        snprintf(error_text, error_text_size, "%s (hr=0x%08lX).", what, (unsigned long)hr);
    }
}

static bool capture_wic_make_utf16_path(const char *path, WCHAR *wide_path, size_t wide_path_count, char *error_text, size_t error_text_size)
{
    if (path == NULL || wide_path == NULL || wide_path_count == 0) {
        capture_wic_set_error(error_text, error_text_size, "Capture path parameters are invalid.");
        return false;
    }

    if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, (int)wide_path_count) <= 0) {
        capture_wic_set_error(error_text, error_text_size, "Failed to convert capture path to UTF-16.");
        return false;
    }

    return true;
}

static HRESULT capture_wic_create_float_bitmap(
    IWICImagingFactory *factory,
    uint32_t width,
    uint32_t height,
    const vec4_t *argbf32_data,
    IWICBitmap **bitmap_out)
{
    float *rgba_pixels = NULL;
    UINT stride;
    size_t buffer_size;
    HRESULT hr = E_INVALIDARG;

    if (bitmap_out != NULL) {
        *bitmap_out = NULL;
    }

    if (factory == NULL || bitmap_out == NULL || argbf32_data == NULL || width == 0 || height == 0) {
        return E_INVALIDARG;
    }

    stride = width * 16u;
    buffer_size = (size_t)stride * (size_t)height;
    rgba_pixels = (float *)malloc(buffer_size);
    if (rgba_pixels == NULL) {
        return E_OUTOFMEMORY;
    }

    for (uint32_t y = 0; y < height; y++) {
        const vec4_t *src_row = argbf32_data + (size_t)(height - 1u - y) * (size_t)width;
        float *dst_row = rgba_pixels + (size_t)y * (size_t)width * 4u;

        for (uint32_t x = 0; x < width; x++) {
            dst_row[x * 4u + 0u] = src_row[x].x;
            dst_row[x * 4u + 1u] = src_row[x].y;
            dst_row[x * 4u + 2u] = src_row[x].z;
            dst_row[x * 4u + 3u] = src_row[x].w;
        }
    }

    /*
    WIC's 128bpp float bitmap source is treated here as linear scRGB / CCCS-style
    data: sRGB primaries with unclamped linear float values. That makes it a good
    raw/reference source for scene-linear capture paths.
    */
    hr = IWICImagingFactory_CreateBitmapFromMemory(
        factory,
        width,
        height,
        &GUID_WICPixelFormat128bppRGBAFloat,
        stride,
        (UINT)buffer_size,
        (BYTE *)rgba_pixels,
        bitmap_out);

    free(rgba_pixels);
    return hr;
}

static HRESULT capture_wic_create_png_converter(
    IWICImagingFactory *factory,
    IWICBitmap *float_bitmap,
    bool high_precision,
    IWICFormatConverter **converter_out)
{
    WICPixelFormatGUID pixel_format = high_precision ? GUID_WICPixelFormat64bppRGBA : GUID_WICPixelFormat32bppBGRA;
    HRESULT hr;

    if (converter_out != NULL) {
        *converter_out = NULL;
    }

    if (factory == NULL || float_bitmap == NULL || converter_out == NULL) {
        return E_INVALIDARG;
    }

    hr = IWICImagingFactory_CreateFormatConverter(factory, converter_out);
    if (SUCCEEDED(hr)) {
        hr = IWICFormatConverter_Initialize(
            *converter_out,
            (IWICBitmapSource *)float_bitmap,
            &pixel_format,
            WICBitmapDitherTypeNone,
            NULL,
            0.0f,
            WICBitmapPaletteTypeCustom);
    }

    if (FAILED(hr) && *converter_out != NULL) {
        IWICFormatConverter_Release(*converter_out);
        *converter_out = NULL;
    }

    return hr;
}

bool capture_wic_write_png(
    const char *path,
    uint32_t width,
    uint32_t height,
    const vec4_t *argbf32_data,
    shader_color_space_t shader_color_space,
    bool high_precision,
    char *error_text,
    size_t error_text_size)
{
    IWICImagingFactory *factory = NULL;
    IWICBitmap *float_bitmap = NULL;
    IWICFormatConverter *converter = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IWICStream *stream = NULL;
    IPropertyBag2 *property_bag = NULL;
    WCHAR wide_path[MAX_PATH];
    WICPixelFormatGUID pixel_format = high_precision ? GUID_WICPixelFormat64bppRGBA : GUID_WICPixelFormat32bppBGRA;
    HRESULT hr;

    capture_wic_set_error(error_text, error_text_size, "");

    if (path == NULL || argbf32_data == NULL || width == 0 || height == 0) {
        capture_wic_set_error(error_text, error_text_size, "PNG capture parameters are invalid.");
        return false;
    }

    if (shader_color_space == SHADER_COLOR_SPACE_HDR10_ST2084) {
        /*
        WIC can carry our raw float bitmap source as linear scRGB-style data, and it
        can convert that to legacy SDR formats. It does not natively implement the
        HDR10 ST.2084 transfer function correctly for PNG export. DXGI presentation
        paths can convert HDR10-authored shader data for display, but WIC PNG capture
        intentionally leaves HDR10 unsupported for now.
        */
        capture_wic_set_error(
            error_text,
            error_text_size,
            "WIC PNG capture does not support HDR10 (ST.2084) shader output. DXGI presentation can convert this data for display, but WIC capture leaves HDR10 unsupported.");
        return false;
    }

    if (!capture_wic_make_utf16_path(path, wide_path, sizeof(wide_path) / sizeof(wide_path[0]), error_text, error_text_size)) {
        return false;
    }

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        capture_wic_set_error_hr(error_text, error_text_size, "CoInitializeEx failed", hr);
        return false;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory);
    if (SUCCEEDED(hr)) {
        hr = capture_wic_create_float_bitmap(factory, width, height, argbf32_data, &float_bitmap);
    }
    if (SUCCEEDED(hr)) {
        /*
        The PNG path currently preserves scene-linear/scRGB-style float data through a
        straight format conversion into 64bpp RGBA for reference capture. SDR shaders
        already produce display-referred values, so the 32bpp BGRA path stays a direct
        conversion too. If a future export mode needs a true legacy-SDR transform from
        linear float data, prefer an IWICColorTransform path here instead of treating
        format conversion as color management.
        */
        hr = capture_wic_create_png_converter(factory, float_bitmap, high_precision, &converter);
    }
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (SUCCEEDED(hr)) hr = IWICStream_InitializeFromFilename(stream, wide_path, GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &property_bag);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(frame, property_bag);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(frame, width, height);
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &pixel_format);
        if (SUCCEEDED(hr) &&
            ((high_precision && !IsEqualGUID(&pixel_format, &GUID_WICPixelFormat64bppRGBA)) ||
             (!high_precision && !IsEqualGUID(&pixel_format, &GUID_WICPixelFormat32bppBGRA))))
        {
            hr = E_FAIL;
        }
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)converter, NULL);
    }
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Commit(frame);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(encoder);

    if (property_bag != NULL) {
        IPropertyBag2_Release(property_bag);
    }
    if (frame != NULL) {
        IWICBitmapFrameEncode_Release(frame);
    }
    if (encoder != NULL) {
        IWICBitmapEncoder_Release(encoder);
    }
    if (stream != NULL) {
        IWICStream_Release(stream);
    }
    if (converter != NULL) {
        IWICFormatConverter_Release(converter);
    }
    if (float_bitmap != NULL) {
        IWICBitmap_Release(float_bitmap);
    }
    if (factory != NULL) {
        IWICImagingFactory_Release(factory);
    }
    CoUninitialize();

    if (FAILED(hr)) {
        capture_wic_set_error_hr(error_text, error_text_size, "WIC PNG encode failed", hr);
    }

    return SUCCEEDED(hr);
}
