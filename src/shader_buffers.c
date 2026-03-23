#define COBJMACROS

#include "shader_buffers.h"

#include <stdint.h>
#include <objbase.h>
#include <stdarg.h>
#include <string.h>
#include <wincodec.h>

static void shader_buffers_set_error(char *error, size_t error_size, const char *format, ...)
{
    va_list args;

    if (error == NULL || error_size == 0) {
        return;
    }

    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static int shader_buffer_bytes_per_pixel(uint texel_format)
{
    if (texel_format == SHADER_TEXEL_FORMAT_R8_UNORM) {
        return 1;
    }
    if (texel_format == SHADER_TEXEL_FORMAT_RGBA8_UNORM || texel_format == SHADER_TEXEL_FORMAT_BGRA8_UNORM) {
        return 4;
    }

    return 0;
}

static const WICPixelFormatGUID *shader_buffer_wic_format(uint texel_format)
{
    if (texel_format == SHADER_TEXEL_FORMAT_R8_UNORM) {
        return &GUID_WICPixelFormat8bppGray;
    }
    if (texel_format == SHADER_TEXEL_FORMAT_RGBA8_UNORM) {
        return &GUID_WICPixelFormat32bppRGBA;
    }
    if (texel_format == SHADER_TEXEL_FORMAT_BGRA8_UNORM) {
        return &GUID_WICPixelFormat32bppBGRA;
    }

    return NULL;
}

static WCHAR *shader_buffer_wide_path(const char *path)
{
    int length;
    UINT code_page = CP_UTF8;
    WCHAR *wide_path;

    length = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (length <= 0) {
        code_page = CP_ACP;
        length = MultiByteToWideChar(code_page, 0, path, -1, NULL, 0);
        if (length <= 0) {
            return NULL;
        }
    }

    wide_path = (WCHAR *)malloc((size_t)length * sizeof(WCHAR));
    if (wide_path == NULL) {
        return NULL;
    }

    if (MultiByteToWideChar(code_page, 0, path, -1, wide_path, length) <= 0) {
        free(wide_path);
        return NULL;
    }

    return wide_path;
}

static bool shader_buffer_is_absolute_path(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return false;
    }

    if ((path[0] == '\\' && path[1] == '\\') || (path[0] == '/' && path[1] == '/')) {
        return true;
    }

    return path[1] == ':';
}

static bool shader_buffer_resolve_module_relative_path(
    const char *relative_path,
    char *resolved_path,
    size_t resolved_path_size,
    char *error,
    size_t error_size)
{
    DWORD length;
    char *slash = NULL;
    size_t base_length;
    size_t relative_length;

    if (resolved_path == NULL || resolved_path_size == 0) {
        shader_buffers_set_error(error, error_size, "Resolved path buffer is invalid.");
        return false;
    }

    resolved_path[0] = '\0';

    if (relative_path == NULL || relative_path[0] == '\0') {
        shader_buffers_set_error(error, error_size, "Relative texture path is empty.");
        return false;
    }

    if (shader_buffer_is_absolute_path(relative_path)) {
        snprintf(resolved_path, resolved_path_size, "%s", relative_path);
        return true;
    }

    length = GetModuleFileNameA(NULL, resolved_path, (DWORD)resolved_path_size);
    if (length == 0 || length >= resolved_path_size) {
        shader_buffers_set_error(error, error_size, "Failed to resolve module path for '%s'.", relative_path);
        resolved_path[0] = '\0';
        return false;
    }

    slash = strrchr(resolved_path, '\\');
    if (slash == NULL) {
        slash = strrchr(resolved_path, '/');
    }
    if (slash == NULL) {
        shader_buffers_set_error(error, error_size, "Module directory is unavailable for '%s'.", relative_path);
        resolved_path[0] = '\0';
        return false;
    }

    slash[1] = '\0';
    base_length = strlen(resolved_path);
    relative_length = strlen(relative_path);

    if (base_length + relative_length >= resolved_path_size) {
        shader_buffers_set_error(error, error_size, "Resolved texture path is too long for '%s'.", relative_path);
        resolved_path[0] = '\0';
        return false;
    }

    memcpy(resolved_path + base_length, relative_path, relative_length + 1);
    return true;
}

/*
 * Shared WIC decode helper: given an already-created decoder, decode frame 0
 * into a malloc'd pixel buffer in the requested texel format.
 */
static bool shader_buffer_decode_from_decoder(
    IWICImagingFactory *factory,
    IWICBitmapDecoder *decoder,
    uint texel_format,
    const char *source_desc,
    void **data_out,
    size_t *size_bytes_out,
    int *width_out,
    int *height_out,
    int *row_stride_out,
    char *error,
    size_t error_size)
{
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    const WICPixelFormatGUID *target_format = shader_buffer_wic_format(texel_format);
    int bytes_per_pixel = shader_buffer_bytes_per_pixel(texel_format);
    HRESULT hr;
    UINT width = 0;
    UINT height = 0;
    size_t row_stride_bytes;
    size_t size_bytes;
    void *data = NULL;

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to read frame 0 from '%s' (hr=0x%08lX).", source_desc, (unsigned long)hr);
        goto fail;
    }

    hr = IWICBitmapFrameDecode_GetSize(frame, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        shader_buffers_set_error(error, error_size, "Failed to query texture size for '%s'.", source_desc);
        goto fail;
    }

    if ((size_t)width > (SIZE_MAX / (size_t)bytes_per_pixel) || (size_t)height > (SIZE_MAX / ((size_t)width * (size_t)bytes_per_pixel))) {
        shader_buffers_set_error(error, error_size, "Texture '%s' is too large.", source_desc);
        goto fail;
    }

    row_stride_bytes = (size_t)width * (size_t)bytes_per_pixel;
    size_bytes = row_stride_bytes * (size_t)height;

    data = malloc(size_bytes);
    if (data == NULL) {
        shader_buffers_set_error(error, error_size, "Failed to allocate %zu bytes for texture '%s'.", size_bytes, source_desc);
        goto fail;
    }

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to create WIC format converter (hr=0x%08lX).", (unsigned long)hr);
        goto fail;
    }

    hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame, target_format, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to convert texture '%s' to requested format (hr=0x%08lX).", source_desc, (unsigned long)hr);
        goto fail;
    }

    hr = IWICFormatConverter_CopyPixels(converter, NULL, (UINT)row_stride_bytes, (UINT)size_bytes, (BYTE *)data);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to copy pixels from '%s' (hr=0x%08lX).", source_desc, (unsigned long)hr);
        goto fail;
    }

    if (data_out != NULL) {
        *data_out = data;
    }
    if (size_bytes_out != NULL) {
        *size_bytes_out = size_bytes;
    }
    if (width_out != NULL) {
        *width_out = (int)width;
    }
    if (height_out != NULL) {
        *height_out = (int)height;
    }
    if (row_stride_out != NULL) {
        *row_stride_out = (int)row_stride_bytes;
    }

    IWICFormatConverter_Release(converter);
    IWICBitmapFrameDecode_Release(frame);
    return true;

fail:
    free(data);
    if (converter != NULL) {
        IWICFormatConverter_Release(converter);
    }
    if (frame != NULL) {
        IWICBitmapFrameDecode_Release(frame);
    }
    return false;
}

/*
 * Decode a texture from a file path. Creates a WIC decoder from the filename,
 * then delegates to the shared decode helper.
 */
static bool shader_buffer_decode_texture_file(
    const char *path,
    uint texel_format,
    void **data_out,
    size_t *size_bytes_out,
    int *width_out,
    int *height_out,
    int *row_stride_out,
    char *error,
    size_t error_size)
{
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    WCHAR *wide_path = NULL;
    HRESULT hr;
    bool should_uninitialize = false;
    bool result = false;
    int bytes_per_pixel = shader_buffer_bytes_per_pixel(texel_format);
    const WICPixelFormatGUID *target_format = shader_buffer_wic_format(texel_format);

    if (path == NULL || path[0] == '\0') {
        shader_buffers_set_error(error, error_size, "Texture path is empty.");
        return false;
    }

    if (target_format == NULL || bytes_per_pixel <= 0) {
        shader_buffers_set_error(error, error_size, "Unsupported texture format %u.", texel_format);
        return false;
    }

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        should_uninitialize = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        shader_buffers_set_error(error, error_size, "CoInitializeEx failed (hr=0x%08lX).", (unsigned long)hr);
        return false;
    }

    wide_path = shader_buffer_wide_path(path);
    if (wide_path == NULL) {
        shader_buffers_set_error(error, error_size, "Failed to convert texture path to UTF-16.");
        goto cleanup;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (LPVOID *)&factory);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "CoCreateInstance(WIC) failed (hr=0x%08lX).", (unsigned long)hr);
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateDecoderFromFilename(factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to open texture '%s' (hr=0x%08lX).", path, (unsigned long)hr);
        goto cleanup;
    }

    result = shader_buffer_decode_from_decoder(factory, decoder, texel_format, path,
        data_out, size_bytes_out, width_out, height_out, row_stride_out, error, error_size);

cleanup:
    if (decoder != NULL) {
        IWICBitmapDecoder_Release(decoder);
    }
    if (factory != NULL) {
        IWICImagingFactory_Release(factory);
    }
    free(wide_path);
    if (should_uninitialize) {
        CoUninitialize();
    }
    return result;
}

/*
 * Decode a texture from a Win32 resource embedded in a DLL module.
 * Creates a WIC decoder via IWICStream over the locked resource memory,
 * then delegates to the shared decode helper.
 */
static bool shader_buffer_decode_texture_resource(
    HMODULE module,
    const char *resource_name,
    uint texel_format,
    void **data_out,
    size_t *size_bytes_out,
    int *width_out,
    int *height_out,
    int *row_stride_out,
    char *error,
    size_t error_size)
{
    IWICImagingFactory *factory = NULL;
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    HRESULT hr;
    bool should_uninitialize = false;
    bool result = false;
    int bytes_per_pixel = shader_buffer_bytes_per_pixel(texel_format);
    const WICPixelFormatGUID *target_format = shader_buffer_wic_format(texel_format);
    HRSRC hres;
    HGLOBAL hload;
    void *res_data;
    DWORD res_size;

    if (module == NULL) {
        shader_buffers_set_error(error, error_size, "Module handle is NULL for resource '%s'.", resource_name != NULL ? resource_name : "");
        return false;
    }

    if (resource_name == NULL || resource_name[0] == '\0') {
        shader_buffers_set_error(error, error_size, "Resource name is empty.");
        return false;
    }

    if (target_format == NULL || bytes_per_pixel <= 0) {
        shader_buffers_set_error(error, error_size, "Unsupported texture format %u.", texel_format);
        return false;
    }

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        should_uninitialize = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        shader_buffers_set_error(error, error_size, "CoInitializeEx failed (hr=0x%08lX).", (unsigned long)hr);
        return false;
    }

    hres = FindResourceA(module, resource_name, MAKEINTRESOURCEA(10) /* RT_RCDATA */);

    if (hres == NULL) {
        shader_buffers_set_error(error, error_size, "Resource '%s' not found (err=%lu).", resource_name, (unsigned long)GetLastError());
        goto cleanup;
    }

    hload = LoadResource(module, hres);
    if (hload == NULL) {
        shader_buffers_set_error(error, error_size, "Failed to load resource '%s' (err=%lu).", resource_name, (unsigned long)GetLastError());
        goto cleanup;
    }

    res_data = LockResource(hload);
    res_size = SizeofResource(module, hres);
    if (res_data == NULL || res_size == 0) {
        shader_buffers_set_error(error, error_size, "Resource '%s' is empty or inaccessible.", resource_name);
        goto cleanup;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (LPVOID *)&factory);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "CoCreateInstance(WIC) failed (hr=0x%08lX).", (unsigned long)hr);
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to create WIC stream for resource '%s' (hr=0x%08lX).", resource_name, (unsigned long)hr);
        goto cleanup;
    }

    hr = IWICStream_InitializeFromMemory(stream, (BYTE *)res_data, res_size);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to initialize WIC stream from resource '%s' (hr=0x%08lX).", resource_name, (unsigned long)hr);
        goto cleanup;
    }

    hr = IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        shader_buffers_set_error(error, error_size, "Failed to create decoder from resource '%s' (hr=0x%08lX).", resource_name, (unsigned long)hr);
        goto cleanup;
    }

    result = shader_buffer_decode_from_decoder(factory, decoder, texel_format, resource_name,
        data_out, size_bytes_out, width_out, height_out, row_stride_out, error, error_size);

cleanup:
    if (decoder != NULL) {
        IWICBitmapDecoder_Release(decoder);
    }
    if (stream != NULL) {
        IWICStream_Release(stream);
    }
    if (factory != NULL) {
        IWICImagingFactory_Release(factory);
    }
    if (should_uninitialize) {
        CoUninitialize();
    }
    return result;
}

void shader_buffers_reset(shader_buffers_t *buffers)
{
    if (buffers == NULL) {
        return;
    }

    memset(buffers, 0, sizeof(*buffers));
}

void shader_buffers_default_cleanup(shader_buffers_t *buffers)
{
    if (buffers == NULL) {
        return;
    }

    for (int index = 0; index < buffers->count; index++) {
        free(buffers->items[index].data);
        buffers->items[index].data = NULL;
    }

    shader_buffers_reset(buffers);
}

shader_buffer_t *shader_buffers_alloc_bytes(shader_buffers_t *buffers, const char *label, size_t size_bytes, char *error, size_t error_size)
{
    shader_buffer_t slot;

    if (buffers == NULL) {
        shader_buffers_set_error(error, error_size, "Buffer set is NULL.");
        return NULL;
    }
    if (buffers->count >= SHADER_BUFFER_MAX) {
        shader_buffers_set_error(error, error_size, "Shader buffer limit reached (%d).", SHADER_BUFFER_MAX);
        return NULL;
    }
    if (size_bytes == 0) {
        shader_buffers_set_error(error, error_size, "Byte buffer '%s' has zero size.", label != NULL ? label : "");
        return NULL;
    }

    memset(&slot, 0, sizeof(slot));
    slot.data = malloc(size_bytes);
    if (slot.data == NULL) {
        shader_buffers_set_error(error, error_size, "Failed to allocate %zu bytes for buffer '%s'.", size_bytes, label != NULL ? label : "");
        return NULL;
    }

    memset(slot.data, 0, size_bytes);
    slot.size_bytes = size_bytes;
    slot.type = SHADER_BUFFER_TYPE_BYTES;
    if (label != NULL) {
        snprintf(slot.label, sizeof(slot.label), "%s", label);
    }

    buffers->items[buffers->count] = slot;
    buffers->count++;
    return &buffers->items[buffers->count - 1];
}

shader_buffer_t *shader_buffers_load_texture_file(shader_buffers_t *buffers, const char *label, const char *path, uint texel_format, char *error, size_t error_size)
{
    shader_buffer_t slot;

    if (buffers == NULL) {
        shader_buffers_set_error(error, error_size, "Buffer set is NULL.");
        return NULL;
    }
    if (buffers->count >= SHADER_BUFFER_MAX) {
        shader_buffers_set_error(error, error_size, "Shader buffer limit reached (%d).", SHADER_BUFFER_MAX);
        return NULL;
    }

    memset(&slot, 0, sizeof(slot));
    if (!shader_buffer_decode_texture_file(path, texel_format, &slot.data, &slot.size_bytes, &slot.width, &slot.height, &slot.row_stride_bytes, error, error_size)) {
        return NULL;
    }

    slot.type = SHADER_BUFFER_TYPE_TEXTURE2D;
    slot.texel_format = texel_format;
    if (label != NULL) {
        snprintf(slot.label, sizeof(slot.label), "%s", label);
    }

    buffers->items[buffers->count] = slot;
    buffers->count++;
    return &buffers->items[buffers->count - 1];
}

shader_buffer_t *shader_buffers_load_texture_module_relative(
    shader_buffers_t *buffers,
    const char *label,
    const char *relative_path,
    uint texel_format,
    char *error,
    size_t error_size)
{
    char resolved_path[4096];

    if (!shader_buffer_resolve_module_relative_path(relative_path, resolved_path, sizeof(resolved_path), error, error_size)) {
        return NULL;
    }

    return shader_buffers_load_texture_file(buffers, label, resolved_path, texel_format, error, error_size);
}

shader_buffer_t *shader_buffers_load_texture_resource(
    shader_buffers_t *buffers,
    const char *label,
    HMODULE module,
    const char *resource_name,
    uint texel_format,
    char *error,
    size_t error_size)
{
    shader_buffer_t slot;

    if (buffers == NULL) {
        shader_buffers_set_error(error, error_size, "Buffer set is NULL.");
        return NULL;
    }
    if (buffers->count >= SHADER_BUFFER_MAX) {
        shader_buffers_set_error(error, error_size, "Shader buffer limit reached (%d).", SHADER_BUFFER_MAX);
        return NULL;
    }

    memset(&slot, 0, sizeof(slot));
    if (!shader_buffer_decode_texture_resource(module, resource_name, texel_format, &slot.data, &slot.size_bytes, &slot.width, &slot.height, &slot.row_stride_bytes, error, error_size)) {
        return NULL;
    }

    slot.type = SHADER_BUFFER_TYPE_TEXTURE2D;
    slot.texel_format = texel_format;
    if (label != NULL) {
        snprintf(slot.label, sizeof(slot.label), "%s", label);
    }

    buffers->items[buffers->count] = slot;
    buffers->count++;
    return &buffers->items[buffers->count - 1];
}

const shader_buffer_t *shader_buffers_find(const shader_buffers_t *buffers, const char *label)
{
    if (buffers == NULL || label == NULL || label[0] == '\0') {
        return NULL;
    }

    for (int index = 0; index < buffers->count; index++) {
        if (strcmp(buffers->items[index].label, label) == 0) {
            return &buffers->items[index];
        }
    }

    return NULL;
}
