#include "capture_manager.h"
#include "capture_wic.h"
#include "capture_exr.h"

#include <string.h>

static int            g_capture_frames_remaining = 0;
static int            g_capture_frames_total = 0;
static char           g_capture_shader_id[64] = "";
static uint           g_capture_sequence = 0;
static volatile LONG  g_capture_jobs_in_flight = 0;
static HANDLE         g_capture_idle_event = NULL;
static char           g_capture_notice[768] = "";
static volatile LONG  g_capture_notice_version = 0;

typedef struct {
    char                 path[MAX_PATH];
    int                  width;
    int                  height;
    int                  capture_index;
    int                  capture_total;
    vec4_t              *pixels;
    shader_color_space_t shader_color_space;
    bool                 use_16bpc;
} capture_job_t;

static void capture_set_notice(const char *text)
{
    snprintf(g_capture_notice, sizeof(g_capture_notice), "%s", text != NULL ? text : "");
    InterlockedIncrement(&g_capture_notice_version);
}

static void capture_sanitize_label(char *buffer, size_t buffer_size, const char *text)
{
    size_t length = 0;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    if (text == NULL || text[0] == '\0') {
        snprintf(buffer, buffer_size, "shader");
        return;
    }

    while (*text != '\0' && length + 1 < buffer_size) {
        char ch = *text++;
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' ||
            ch == '-')
        {
            buffer[length++] = ch;
        } else {
            buffer[length++] = '_';
        }
    }

    if (length == 0) {
        snprintf(buffer, buffer_size, "shader");
        return;
    }

    buffer[length] = '\0';
}

static bool capture_make_path(char *buffer, size_t buffer_size, const char *shader_id, uint sequence, const char *extension)
{
    char module_path[MAX_PATH];
    char capture_dir[MAX_PATH];
    char safe_label[64];
    char *slash;
    SYSTEMTIME local_time;
    DWORD path_length;

    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    path_length = GetModuleFileNameA(NULL, module_path, (DWORD)sizeof(module_path));
    if (path_length == 0 || path_length >= sizeof(module_path)) {
        return false;
    }

    slash = strrchr(module_path, '\\');
    if (slash == NULL) {
        return false;
    }
    *slash = '\0';

    snprintf(capture_dir, sizeof(capture_dir), "%s\\captures", module_path);
    if (!CreateDirectoryA(capture_dir, NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }

    capture_sanitize_label(safe_label, sizeof(safe_label), shader_id);
    GetLocalTime(&local_time);
    snprintf(
        buffer,
        buffer_size,
        "%s\\%04u%02u%02u_%02u%02u%02u_%03u_%s_%04u.%s",
        capture_dir,
        (unsigned)local_time.wYear,
        (unsigned)local_time.wMonth,
        (unsigned)local_time.wDay,
        (unsigned)local_time.wHour,
        (unsigned)local_time.wMinute,
        (unsigned)local_time.wSecond,
        (unsigned)local_time.wMilliseconds,
        safe_label,
        (unsigned)sequence,
        (extension != NULL && extension[0] != '\0') ? extension : "png");
    return true;
}

static void capture_signal_idle_if_ready(void)
{
    if (g_capture_idle_event != NULL &&
        g_capture_frames_remaining <= 0 &&
        InterlockedCompareExchange(&g_capture_jobs_in_flight, 0, 0) == 0)
    {
        SetEvent(g_capture_idle_event);
    }
}

static DWORD WINAPI capture_thread(LPVOID arg)
{
    capture_job_t *job = (capture_job_t *)arg;
    char error_text[512] = "";
    char notice_text[768];

    if (job != NULL) {
        bool write_ok;

        if (job->use_16bpc) {
            /* Scene-linear: write OpenEXR with raw 32-bit float RGBA. */
            write_ok = capture_exr_write(
                job->path,
                (uint32_t)job->width,
                (uint32_t)job->height,
                job->pixels,
                error_text,
                sizeof(error_text));
        } else {
            /* SDR: write 8-bit BGRA PNG via WIC. */
            write_ok = capture_wic_write_png(
                job->path,
                (uint32_t)job->width,
                (uint32_t)job->height,
                job->pixels,
                error_text,
                sizeof(error_text));
        }

        if (!write_ok) {
            char debug_text[1024];
            snprintf(
                notice_text,
                sizeof(notice_text),
                "Capture failed: %s",
                (error_text[0] != '\0') ? error_text : job->path);
            capture_set_notice(notice_text);
            snprintf(debug_text, sizeof(debug_text), "%s\n", notice_text);
            OutputDebugStringA(debug_text);
        } else {
            snprintf(
                notice_text,
                sizeof(notice_text),
                "Captured frame %d/%d to %s%s",
                job->capture_index,
                job->capture_total,
                job->path,
                job->use_16bpc ? " (32-bit float EXR)" : " (8-bit BGRA PNG)");
            capture_set_notice(notice_text);
        }

        free(job->pixels);
        free(job);
    }

    if (InterlockedDecrement(&g_capture_jobs_in_flight) == 0) {
        capture_signal_idle_if_ready();
    }

    return 0;
}

void capture_manager_init(void)
{
    g_capture_frames_remaining = 0;
    g_capture_frames_total = 0;
    g_capture_shader_id[0] = '\0';
    g_capture_jobs_in_flight = 0;
    g_capture_notice[0] = '\0';
    g_capture_notice_version = 0;

    if (g_capture_idle_event != NULL) {
        CloseHandle(g_capture_idle_event);
        g_capture_idle_event = NULL;
    }
    g_capture_idle_event = CreateEventW(NULL, TRUE, TRUE, NULL);
}

void capture_manager_shutdown(void)
{
    if (g_capture_idle_event != NULL) {
        WaitForSingleObject(g_capture_idle_event, INFINITE);
        CloseHandle(g_capture_idle_event);
        g_capture_idle_event = NULL;
    }
}

bool capture_manager_request(const char *shader_id, int frame_count)
{
    if (frame_count <= 0) {
        capture_set_notice("Capture count must be at least 1.");
        return false;
    }

    if (g_capture_frames_remaining > 0) {
        capture_set_notice("A capture sequence is already in progress.");
        return false;
    }

    if (shader_id != NULL) {
        snprintf(g_capture_shader_id, sizeof(g_capture_shader_id), "%s", shader_id);
    } else {
        g_capture_shader_id[0] = '\0';
    }

    g_capture_frames_remaining = frame_count;
    g_capture_frames_total = frame_count;
    return true;
}

bool capture_manager_requested(void)
{
    return g_capture_frames_remaining > 0;
}

bool capture_manager_in_progress(void)
{
    return g_capture_frames_remaining > 0 || InterlockedCompareExchange(&g_capture_jobs_in_flight, 0, 0) > 0;
}

void capture_manager_process_frame(
    const vec4_t *frame_pixels,
    int width, int height,
    shader_color_space_t color_space,
    void (*request_frame_callback)(void))
{
    capture_job_t *job;
    HANDLE thread;
    size_t pixel_count;

    if (g_capture_frames_remaining <= 0 || frame_pixels == NULL || width <= 0 || height <= 0) {
        return;
    }

    pixel_count = (size_t)width * (size_t)height;
    job = (capture_job_t *)malloc(sizeof(*job));
    if (job == NULL) {
        capture_set_notice("Capture failed: unable to allocate capture job.");
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    ZeroMemory(job, sizeof(*job));
    job->pixels = (vec4_t *)malloc(pixel_count * sizeof(vec4_t));
    if (job->pixels == NULL) {
        free(job);
        capture_set_notice("Capture failed: unable to allocate capture pixels.");
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    memcpy(job->pixels, frame_pixels, pixel_count * sizeof(vec4_t));
    job->width = width;
    job->height = height;
    job->capture_total = max(1, g_capture_frames_total);
    job->capture_index = job->capture_total - g_capture_frames_remaining + 1;
    job->shader_color_space = color_space;
    job->use_16bpc = (color_space != SHADER_COLOR_SPACE_SDR_DISPLAY);
    if (!capture_make_path(job->path, sizeof(job->path), g_capture_shader_id, ++g_capture_sequence,
            job->use_16bpc ? "exr" : "png")) {
        free(job->pixels);
        free(job);
        capture_set_notice("Capture failed: unable to build output path.");
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    g_capture_frames_remaining--;
    if (g_capture_frames_remaining == 0) {
        g_capture_frames_total = 0;
    }

    if (g_capture_idle_event != NULL) {
        ResetEvent(g_capture_idle_event);
    }
    InterlockedIncrement(&g_capture_jobs_in_flight);
    thread = CreateThread(NULL, 0, capture_thread, job, 0, NULL);
    if (thread == NULL) {
        capture_set_notice("Capture failed: unable to create image writer thread.");
        free(job->pixels);
        free(job);
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        if (InterlockedDecrement(&g_capture_jobs_in_flight) == 0) {
            capture_signal_idle_if_ready();
        }
        return;
    }

    CloseHandle(thread);
    if (g_capture_frames_remaining > 0 && request_frame_callback != NULL) {
        request_frame_callback();
    }
}

void capture_manager_cancel(void)
{
    g_capture_frames_remaining = 0;
    g_capture_frames_total = 0;
    g_capture_shader_id[0] = '\0';
    capture_signal_idle_if_ready();
}

void capture_manager_wait_idle(void)
{
    if (g_capture_idle_event != NULL) {
        WaitForSingleObject(g_capture_idle_event, INFINITE);
    }
}

const char *capture_manager_notice(void)
{
    return g_capture_notice;
}

uint capture_manager_notice_version(void)
{
    return (uint)g_capture_notice_version;
}
