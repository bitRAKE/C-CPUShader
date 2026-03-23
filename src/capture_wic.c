#define COBJMACROS

#include "capture_wic.h"

#include <wincodec.h>
#include <propidl.h>
#include <stdlib.h>
#include <string.h>

/* ---- Color space helpers ------------------------------------------------ */

/*
 * IEC 61966-2-1 sRGB transfer function (linear to non-linear).
 *
 * WIC defines GUID_WICPixelFormat128bppRGBAFloat as scRGB -- an extended-
 * range *linear* color space (IEC 61966-2-2).  When WIC's format converter
 * converts 128bppFloat to 32bppBGRA it applies the sRGB OETF automatically
 * because 32bppBGRA carries sRGB semantics.
 *
 * This means:
 *  - SDR shader output is already sRGB-encoded.  Routing it through a
 *    128bppFloat bitmap causes WIC to apply a *second* sRGB curve, producing
 *    double-gamma (washed-out) PNGs.  SDR captures therefore bypass the float
 *    bitmap and pack directly into a 32bppBGRA bitmap.
 *
 *  - Scene-linear shader output is true linear.  WIC's 128bppFloat to
 *    64bppRGBA conversion applies the sRGB OETF automatically (128bppFloat
 *    is scRGB-linear, 64bppRGBA is sRGB).  We pass raw linear values into
 *    the float bitmap and let WIC handle the gamma encoding during the
 *    format conversion.  Values above 1.0 are clamped -- HDR highlights
 *    that exceed the sRGB gamut cannot be preserved in a PNG capture.
 */
static float capture_wic_saturate(float v)
{
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    return v;
}

static BYTE capture_wic_float_to_byte(float v)
{
    return (BYTE)(capture_wic_saturate(v) * 255.0f + 0.5f);
}

/* ---- Error helpers ------------------------------------------------------ */

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

/* ---- SDR bitmap: 32bppBGRA, direct pack -------------------------------- */

/*
 * SDR shaders produce sRGB display-referred values.  We pack them directly
 * into a 32bppBGRA bitmap, bypassing the 128bppFloat intermediate that WIC
 * treats as scRGB-linear.
 */
static HRESULT capture_wic_create_sdr_bitmap(
    IWICImagingFactory *factory,
    uint32_t width,
    uint32_t height,
    const vec4_t *pixels,
    IWICBitmap **bitmap_out)
{
    IWICBitmap *bitmap = NULL;
    IWICBitmapLock *bitmap_lock = NULL;
    WICRect lock_rect;
    UINT stride = 0;
    UINT buffer_size = 0;
    BYTE *bitmap_bytes = NULL;
    HRESULT hr = E_INVALIDARG;

    if (bitmap_out != NULL) {
        *bitmap_out = NULL;
    }

    if (factory == NULL || bitmap_out == NULL || pixels == NULL || width == 0 || height == 0) {
        return E_INVALIDARG;
    }

    hr = IWICImagingFactory_CreateBitmap(
        factory,
        width,
        height,
        &GUID_WICPixelFormat32bppBGRA,
        WICBitmapCacheOnLoad,
        &bitmap);
    if (FAILED(hr)) {
        return hr;
    }

    lock_rect.X = 0;
    lock_rect.Y = 0;
    lock_rect.Width = (INT)width;
    lock_rect.Height = (INT)height;

    hr = IWICBitmap_Lock(bitmap, &lock_rect, WICBitmapLockWrite, &bitmap_lock);
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapLock_GetStride(bitmap_lock, &stride);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapLock_GetDataPointer(bitmap_lock, &buffer_size, &bitmap_bytes);
    }
    if (SUCCEEDED(hr)) {
        for (uint32_t y = 0; y < height; y++) {
            const vec4_t *src_row = pixels + (size_t)(height - 1u - y) * (size_t)width;
            BYTE *dst_row = bitmap_bytes + (size_t)y * (size_t)stride;

            for (uint32_t x = 0; x < width; x++) {
                dst_row[x * 4u + 0u] = capture_wic_float_to_byte(src_row[x].z); /* B */
                dst_row[x * 4u + 1u] = capture_wic_float_to_byte(src_row[x].y); /* G */
                dst_row[x * 4u + 2u] = capture_wic_float_to_byte(src_row[x].x); /* R */
                dst_row[x * 4u + 3u] = capture_wic_float_to_byte(src_row[x].w); /* A */
            }
        }
    }

    if (bitmap_lock != NULL) {
        IWICBitmapLock_Release(bitmap_lock);
    }

    if (FAILED(hr)) {
        if (bitmap != NULL) {
            IWICBitmap_Release(bitmap);
        }
        return hr;
    }

    *bitmap_out = bitmap;
    return hr;
}

/* ---- PNG write (SDR only) ------------------------------------------------ */

/*
 * SDR shaders produce sRGB display-referred values.  We pack them directly
 * into a 32bppBGRA bitmap, bypassing WIC's 128bppFloat intermediate whose
 * scRGB-linear semantics would cause a second sRGB curve (double gamma).
 *
 * Scene-linear captures use the OpenEXR writer (capture_exr.c) which
 * preserves the full float range without clamping or gamma encoding.
 */
bool capture_wic_write_png(
    const char *path,
    uint32_t width,
    uint32_t height,
    const vec4_t *argbf32_data,
    char *error_text,
    size_t error_text_size)
{
    IWICImagingFactory *factory = NULL;
    IWICBitmap *bitmap = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IWICStream *stream = NULL;
    IPropertyBag2 *property_bag = NULL;
    WCHAR wide_path[MAX_PATH];
    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
    HRESULT hr;

    capture_wic_set_error(error_text, error_text_size, "");

    if (path == NULL || argbf32_data == NULL || width == 0 || height == 0) {
        capture_wic_set_error(error_text, error_text_size, "PNG capture parameters are invalid.");
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
        hr = capture_wic_create_sdr_bitmap(factory, width, height, argbf32_data, &bitmap);
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
        if (SUCCEEDED(hr) && !IsEqualGUID(&pixel_format, &GUID_WICPixelFormat32bppBGRA)) {
            capture_wic_set_error(error_text, error_text_size,
                "PNG encoder does not support 32bppBGRA pixel format.");
            hr = E_FAIL;
        }
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)bitmap, NULL);
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
    if (bitmap != NULL) {
        IWICBitmap_Release(bitmap);
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
