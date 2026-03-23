#include "capture_exr.h"

#include <string.h>

/*
 * Minimal OpenEXR 2.0 writer -- single-part, scanline, uncompressed, FLOAT.
 *
 * Writes RGBA channels as 32-bit IEEE 754 floats with no compression.  This
 * preserves the full dynamic range of scene-linear shader output without any
 * clamping or gamma encoding.
 *
 * The OpenEXR spec requires channels to be stored in alphabetical order
 * within each scanline: A, B, G, R.
 *
 * Reference: https://openexr.com/en/latest/OpenEXRFileLayout.html
 */

/* ---- Write helpers ------------------------------------------------------ */

typedef struct {
    HANDLE  file;
    int64_t offset;
    bool    ok;
} exr_writer_t;

static void exr_open(exr_writer_t *w, const char *path)
{
    WCHAR wide_path[MAX_PATH];

    w->offset = 0;
    w->ok = false;
    w->file = INVALID_HANDLE_VALUE;

    if (path == NULL) {
        return;
    }

    if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide_path, MAX_PATH) <= 0) {
        return;
    }

    w->file = CreateFileW(
        wide_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    w->ok = (w->file != INVALID_HANDLE_VALUE);
}

static void exr_write_bytes(exr_writer_t *w, const void *data, uint32_t size)
{
    DWORD written = 0;

    if (!w->ok) {
        return;
    }

    if (!WriteFile(w->file, data, size, &written, NULL) || written != size) {
        w->ok = false;
        return;
    }

    w->offset += size;
}

static void exr_write_i32(exr_writer_t *w, int32_t value)
{
    exr_write_bytes(w, &value, 4);
}

static void exr_write_i64(exr_writer_t *w, int64_t value)
{
    exr_write_bytes(w, &value, 8);
}

static void exr_write_f32(exr_writer_t *w, float value)
{
    exr_write_bytes(w, &value, 4);
}

static void exr_close(exr_writer_t *w)
{
    if (w->file != INVALID_HANDLE_VALUE) {
        CloseHandle(w->file);
        w->file = INVALID_HANDLE_VALUE;
    }
}

/*
 * Write a null-terminated string (including the null byte).
 */
static void exr_write_str(exr_writer_t *w, const char *str)
{
    exr_write_bytes(w, str, (uint32_t)(strlen(str) + 1));
}

/* ---- EXR header attribute helpers --------------------------------------- */

/*
 * Each attribute is:
 *   name (null-terminated string)
 *   type (null-terminated string)
 *   size (int32)
 *   value (size bytes)
 */

/*
 * Channel list attribute.  Each channel entry is:
 *   name (null-terminated)
 *   pixel type (int32): 0=UINT, 1=HALF, 2=FLOAT
 *   pLinear (uint8) + 3 padding bytes
 *   xSampling (int32)
 *   ySampling (int32)
 * Terminated by a null byte.
 */
static void exr_write_channel(exr_writer_t *w, const char *name, int32_t pixel_type)
{
    uint8_t zero_pad[3] = {0, 0, 0};

    exr_write_str(w, name);
    exr_write_i32(w, pixel_type);     /* pixel type: 2 = FLOAT */
    exr_write_bytes(w, &zero_pad, 1); /* pLinear = 0 */
    exr_write_bytes(w, &zero_pad, 3); /* padding */
    exr_write_i32(w, 1);              /* xSampling */
    exr_write_i32(w, 1);              /* ySampling */
}

/* Size of one channel entry: name_len+1 + 4 + 1 + 3 + 4 + 4 = name_len + 17 */
static uint32_t exr_channel_size(const char *name)
{
    return (uint32_t)(strlen(name) + 1) + 16;
}

/* ---- Public API --------------------------------------------------------- */

bool capture_exr_write(
    const char *path,
    uint32_t width,
    uint32_t height,
    const vec4_t *pixels,
    char *error_text,
    size_t error_text_size)
{
    exr_writer_t w;
    int64_t header_end_offset;
    int64_t data_start_offset;
    uint32_t channels_size;
    int32_t scanline_pixel_bytes;

    if (error_text != NULL && error_text_size > 0) {
        error_text[0] = '\0';
    }

    if (path == NULL || pixels == NULL || width == 0 || height == 0) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "EXR capture parameters are invalid.");
        }
        return false;
    }

    exr_open(&w, path);
    if (!w.ok) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to create EXR file.");
        }
        return false;
    }

    /* ---- Magic number and version ---- */

    exr_write_i32(&w, 20000630);  /* OpenEXR magic number */
    exr_write_i32(&w, 2);         /* Version 2, flags = 0 (single-part scanline) */

    /* ---- Header attributes ---- */

    /* channels (alphabetical: A, B, G, R) */
    channels_size = exr_channel_size("A")
                  + exr_channel_size("B")
                  + exr_channel_size("G")
                  + exr_channel_size("R")
                  + 1; /* null terminator */
    exr_write_str(&w, "channels");
    exr_write_str(&w, "chlist");
    exr_write_i32(&w, (int32_t)channels_size);
    exr_write_channel(&w, "A", 2);
    exr_write_channel(&w, "B", 2);
    exr_write_channel(&w, "G", 2);
    exr_write_channel(&w, "R", 2);
    exr_write_bytes(&w, "", 1);  /* channel list null terminator */

    /* compression: NO_COMPRESSION = 0 */
    exr_write_str(&w, "compression");
    exr_write_str(&w, "compression");
    exr_write_i32(&w, 1);
    {
        uint8_t comp = 0;
        exr_write_bytes(&w, &comp, 1);
    }

    /* dataWindow: box2i (xMin, yMin, xMax, yMax) */
    exr_write_str(&w, "dataWindow");
    exr_write_str(&w, "box2i");
    exr_write_i32(&w, 16);
    exr_write_i32(&w, 0);
    exr_write_i32(&w, 0);
    exr_write_i32(&w, (int32_t)(width - 1));
    exr_write_i32(&w, (int32_t)(height - 1));

    /* displayWindow: same as dataWindow */
    exr_write_str(&w, "displayWindow");
    exr_write_str(&w, "box2i");
    exr_write_i32(&w, 16);
    exr_write_i32(&w, 0);
    exr_write_i32(&w, 0);
    exr_write_i32(&w, (int32_t)(width - 1));
    exr_write_i32(&w, (int32_t)(height - 1));

    /* chromaticities: Rec.709 / sRGB primaries, D65 white point.
     * Explicitly declares the color space so EXR readers assign a linear
     * Rec.709 profile rather than guessing.  The type is "chromaticities"
     * (8 floats): red xy, green xy, blue xy, white xy. */
    exr_write_str(&w, "chromaticities");
    exr_write_str(&w, "chromaticities");
    exr_write_i32(&w, 32);
    exr_write_f32(&w, 0.6400f); exr_write_f32(&w, 0.3300f);  /* R */
    exr_write_f32(&w, 0.3000f); exr_write_f32(&w, 0.6000f);  /* G */
    exr_write_f32(&w, 0.1500f); exr_write_f32(&w, 0.0600f);  /* B */
    exr_write_f32(&w, 0.3127f); exr_write_f32(&w, 0.3290f);  /* D65 white */

    /* lineOrder: INCREASING_Y = 0 */
    exr_write_str(&w, "lineOrder");
    exr_write_str(&w, "lineOrder");
    exr_write_i32(&w, 1);
    {
        uint8_t order = 0;
        exr_write_bytes(&w, &order, 1);
    }

    /* pixelAspectRatio: 1.0 */
    exr_write_str(&w, "pixelAspectRatio");
    exr_write_str(&w, "float");
    exr_write_i32(&w, 4);
    exr_write_f32(&w, 1.0f);

    /* screenWindowCenter: (0, 0) */
    exr_write_str(&w, "screenWindowCenter");
    exr_write_str(&w, "v2f");
    exr_write_i32(&w, 8);
    exr_write_f32(&w, 0.0f);
    exr_write_f32(&w, 0.0f);

    /* screenWindowWidth: 1.0 */
    exr_write_str(&w, "screenWindowWidth");
    exr_write_str(&w, "float");
    exr_write_i32(&w, 4);
    exr_write_f32(&w, 1.0f);

    /* End of header */
    exr_write_bytes(&w, "", 1);

    header_end_offset = w.offset;

    /* ---- Offset table ----
     *
     * One int64 per scanline.  We fill these with the correct offsets
     * after computing where each scanline will land.
     *
     * Each scanline block is:
     *   int32 y_coordinate
     *   int32 pixel_data_size
     *   float pixel_data[width * 4 channels]
     *
     * Channels are stored interleaved per-scanline in alphabetical order:
     * all A values, then all B values, then all G values, then all R values.
     */
    scanline_pixel_bytes = (int32_t)(width * 4u * sizeof(float));

    /* Write offset table */
    data_start_offset = header_end_offset + (int64_t)height * 8;
    {
        int32_t scanline_block_size = 4 + 4 + scanline_pixel_bytes;

        for (uint32_t y = 0; y < height; y++) {
            int64_t scanline_offset = data_start_offset + (int64_t)y * scanline_block_size;
            exr_write_i64(&w, scanline_offset);
        }
    }

    /* ---- Scanline data ----
     *
     * Source pixels are bottom-up (OpenGL convention).  EXR is top-down
     * (INCREASING_Y).  We flip during the write.
     *
     * Within each scanline, channels are stored in alphabetical order
     * as contiguous runs: all A floats, then all B, then all G, then all R.
     */
    for (uint32_t y = 0; y < height; y++) {
        const vec4_t *src_row = pixels + (size_t)(height - 1u - y) * (size_t)width;

        exr_write_i32(&w, (int32_t)y);
        exr_write_i32(&w, scanline_pixel_bytes);

        /* A channel */
        for (uint32_t x = 0; x < width; x++) {
            exr_write_f32(&w, src_row[x].w);
        }
        /* B channel */
        for (uint32_t x = 0; x < width; x++) {
            exr_write_f32(&w, src_row[x].z);
        }
        /* G channel */
        for (uint32_t x = 0; x < width; x++) {
            exr_write_f32(&w, src_row[x].y);
        }
        /* R channel */
        for (uint32_t x = 0; x < width; x++) {
            exr_write_f32(&w, src_row[x].x);
        }
    }

    exr_close(&w);

    if (!w.ok) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to write EXR file data.");
        }
        return false;
    }

    return true;
}
