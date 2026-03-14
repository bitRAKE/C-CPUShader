//Own runtime/session state: workers, buffers, timing, backend, and frame execution

#define COBJMACROS

#include "runtime_session.h"
#include "capture_wic.h"

#include "present/present_backend.h"

#include <string.h>

#define FRAME_TIME_HISTORY_COUNT 64
#define MAX_RENDER_WORKERS MAXIMUM_WAIT_OBJECTS

typedef struct {
    int               y_start;
    int               y_end;
    int               worker_index;
    shader_uniforms_t uniforms;
    RenderFunc        render;
} Worker;

static int                      g_default_width = 0;
static int                      g_default_height = 0;
static int                      g_width = 0;
static int                      g_height = 0;
static int                      g_popup_width = 0;
static int                      g_popup_height = 0;
static const char              *g_title = NULL;
static bool                     g_vsync_enabled = true;
static LARGE_INTEGER            g_perf_frequency = {0};
static LARGE_INTEGER            g_start_counter = {0};
static uint                     g_frame_counter = 0;
static bool                     g_temporal_accumulation = false;
static bool                     g_has_history = false;
static bool                     g_force_frame = false;
static vec4_t                  *g_frame_pixels = NULL;
static vec4_t                  *g_history_pixels = NULL;
static shader_buffers_t         g_shader_buffers = {0};
static ShaderBuffersCleanupFunc g_shader_buffers_cleanup = NULL;
static present_backend_kind_t   g_present_backend_kind = PRESENT_BACKEND_DX12;
static shader_color_space_t     g_active_shader_color_space = SHADER_COLOR_SPACE_SDR_DISPLAY;

static DWORD_PTR                g_affinity_masks[64];
static DWORD                    g_affinity_processors[64];
static int                      g_affinity_count = 0;

static double                   g_frame_times[FRAME_TIME_HISTORY_COUNT];
static double                   g_frame_time_sum = 0.0;
static int                      g_frame_time_index = 0;
static int                      g_frame_time_count = 0;

static int                      g_total_workers = 0;
static int                      g_background_threads = 0;
static Worker                   g_workers[MAX_RENDER_WORKERS];
static HANDLE                   g_threads[MAX_RENDER_WORKERS];
static HANDLE                   g_work_sem = NULL;
static HANDLE                   g_done_sem = NULL;
static volatile int             g_shutdown = 0;
static char                     g_runtime_error[512] = "";

static shader_keys_t            g_key_state = {{0}};
static uint                     g_key_generation = 1;
static uint                     g_last_key_generation = 0;
static uint                     g_last_mouse_generation = 0;
static int                      g_capture_frames_remaining = 0;
static int                      g_capture_frames_total = 0;
static char                     g_capture_shader_id[64] = "";
static uint                     g_capture_sequence = 0;
static char                     g_runtime_notice[512] = "";
static volatile LONG            g_runtime_notice_version = 0;

static void runtime_release_shader_buffers(void);
static void runtime_set_error_text(const char *text);
static void runtime_set_error_hr(const char *what);
static void runtime_set_notice_text(const char *text);
static void timing_init(void);
static void frame_timing_reset(void);
static void frame_timing_push(double frame_seconds);
static double frame_timing_average(void);
static void refresh_shader_quality(const shader_desc_t *shader);
static void update_key_state(uint virtual_key, bool is_down);
static void destroy_runtime_backend(void);
static void detect_affinity_targets(void);
static void pin_thread_to_worker(HANDLE thread, int worker_index);
static void setup_worker(Worker *worker, int worker_index, int total_workers);
static void worker_render(const Worker *worker);
static void pool_clear_handles(void);
static bool pool_create(int num_threads);
static void pool_destroy(void);
static bool ensure_frame_buffer(void);
static bool ensure_history_buffer(void);
static bool create_display_backend(const runtime_display_params_t *display_params, const shader_desc_t *shader, int render_width, int render_height);
static void process_capture_request(void);

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

static DWORD WINAPI capture_thread(LPVOID arg)
{
    capture_job_t *job = (capture_job_t *)arg;
    char error_text[512] = "";
    char notice_text[768];

    if (job != NULL) {
        if (!capture_wic_write_png(
                job->path,
                (uint32_t)job->width,
                (uint32_t)job->height,
                job->pixels,
                job->shader_color_space,
                job->use_16bpc,
                error_text,
                sizeof(error_text)))
        {
            char debug_text[1024];
            snprintf(
                notice_text,
                sizeof(notice_text),
                "Capture failed: %s",
                (error_text[0] != '\0') ? error_text : job->path);
            runtime_set_notice_text(notice_text);
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
                job->use_16bpc ? " (16-bit RGBA PNG)" : " (8-bit BGRA PNG)");
            runtime_set_notice_text(notice_text);
        }

        free(job->pixels);
        free(job);
    }

    return 0;
}

static void process_capture_request(void)
{
    capture_job_t *job;
    HANDLE thread;
    size_t pixel_count;

    if (g_capture_frames_remaining <= 0 || g_frame_pixels == NULL || g_width <= 0 || g_height <= 0) {
        return;
    }

    pixel_count = (size_t)g_width * (size_t)g_height;
    job = (capture_job_t *)malloc(sizeof(*job));
    if (job == NULL) {
        runtime_set_notice_text("Capture failed: unable to allocate capture job.");
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    ZeroMemory(job, sizeof(*job));
    job->pixels = (vec4_t *)malloc(pixel_count * sizeof(vec4_t));
    if (job->pixels == NULL) {
        free(job);
        runtime_set_notice_text("Capture failed: unable to allocate capture pixels.");
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    memcpy(job->pixels, g_frame_pixels, pixel_count * sizeof(vec4_t));
    job->width = g_width;
    job->height = g_height;
    job->capture_total = max(1, g_capture_frames_total);
    job->capture_index = job->capture_total - g_capture_frames_remaining + 1;
    job->shader_color_space = g_active_shader_color_space;
    job->use_16bpc = (g_active_shader_color_space != SHADER_COLOR_SPACE_SDR_DISPLAY);
    if (!capture_make_path(job->path, sizeof(job->path), g_capture_shader_id, ++g_capture_sequence, "png")) {
        free(job->pixels);
        free(job);
        runtime_set_notice_text("Capture failed: unable to build output path.");
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    g_capture_frames_remaining--;
    if (g_capture_frames_remaining == 0) {
        g_capture_frames_total = 0;
    }

    thread = CreateThread(NULL, 0, capture_thread, job, 0, NULL);
    if (thread == NULL) {
        runtime_set_notice_text("Capture failed: unable to create PNG writer thread.");
        free(job->pixels);
        free(job);
        g_capture_frames_remaining = 0;
        g_capture_frames_total = 0;
        return;
    }

    CloseHandle(thread);
    if (g_capture_frames_remaining > 0) {
        runtime_session_request_frame();
    }
}

static void runtime_release_shader_buffers(void)
{
    if (g_shader_buffers_cleanup != NULL) {
        g_shader_buffers_cleanup(&g_shader_buffers);
    } else {
        shader_buffers_reset(&g_shader_buffers);
    }

    g_shader_buffers_cleanup = NULL;
}

static void runtime_set_error_text(const char *text)
{
    snprintf(g_runtime_error, sizeof(g_runtime_error), "%s", text);
}

static void runtime_set_error_hr(const char *what)
{
    DWORD error = GetLastError();
    snprintf(g_runtime_error, sizeof(g_runtime_error), "%s failed (error=%lu).", what, (unsigned long)error);
}

static void runtime_set_notice_text(const char *text)
{
    snprintf(g_runtime_notice, sizeof(g_runtime_notice), "%s", text != NULL ? text : "");
    InterlockedIncrement(&g_runtime_notice_version);
}

const char *runtime_session_error(void)
{
    return g_runtime_error;
}

static void timing_init(void)
{
    QueryPerformanceFrequency(&g_perf_frequency);
    QueryPerformanceCounter(&g_start_counter);
}

double runtime_session_now_seconds(void)
{
    LARGE_INTEGER counter;

    QueryPerformanceCounter(&counter);
    return (double)(counter.QuadPart - g_start_counter.QuadPart) / (double)g_perf_frequency.QuadPart;
}

static void frame_timing_reset(void)
{
    g_frame_time_sum = 0.0;
    g_frame_time_index = 0;
    g_frame_time_count = 0;
}

void runtime_session_request_frame(void)
{
    g_force_frame = true;
}

bool runtime_session_request_capture(const char *shader_id, int frame_count)
{
    if (frame_count <= 0) {
        runtime_set_notice_text("Capture count must be at least 1.");
        return false;
    }

    if (!present_backend_is_ready() || g_frame_pixels == NULL || g_width <= 0 || g_height <= 0) {
        runtime_set_notice_text("Capture requested with no active presentation surface.");
        return false;
    }

    if (g_capture_frames_remaining > 0) {
        runtime_set_notice_text("A capture sequence is already in progress.");
        return false;
    }

    if (shader_id != NULL) {
        snprintf(g_capture_shader_id, sizeof(g_capture_shader_id), "%s", shader_id);
    } else {
        g_capture_shader_id[0] = '\0';
    }

    g_capture_frames_remaining = frame_count;
    g_capture_frames_total = frame_count;

    runtime_session_request_frame();
    return true;
}

static void frame_timing_push(double frame_seconds)
{
    if (g_frame_time_count == FRAME_TIME_HISTORY_COUNT) {
        g_frame_time_sum -= g_frame_times[g_frame_time_index];
    } else {
        g_frame_time_count++;
    }

    g_frame_times[g_frame_time_index] = frame_seconds;
    g_frame_time_sum += frame_seconds;
    g_frame_time_index = (g_frame_time_index + 1) % FRAME_TIME_HISTORY_COUNT;
}

static double frame_timing_average(void)
{
    if (g_frame_time_count <= 0) {
        return 0.0;
    }

    return g_frame_time_sum / (double)g_frame_time_count;
}

bool runtime_session_shader_uses_feature(const shader_desc_t *shader, uint feature_flag)
{
    return shader != NULL && (shader->feature_flags & feature_flag) != 0;
}

static void refresh_shader_quality(const shader_desc_t *shader)
{
    g_temporal_accumulation = runtime_session_shader_uses_feature(shader, SHADER_FEATURE_TEMPORAL_ACCUMULATION);
}

void runtime_session_target_dimensions(const shader_desc_t *shader, int *width_out, int *height_out)
{
    int width = 0;
    int height = 0;

    if (shader != NULL) {
        width = (shader->preferred_width > 0) ? shader->preferred_width : g_default_width;
        height = (shader->preferred_height > 0) ? shader->preferred_height : g_default_height;
    }

    if (width_out != NULL) {
        *width_out = width;
    }
    if (height_out != NULL) {
        *height_out = height;
    }
}

static void update_key_state(uint virtual_key, bool is_down)
{
    uint word_index;
    uint bit_index;
    uint mask;
    uint old_value;

    if (virtual_key >= 256u) {
        return;
    }

    word_index = virtual_key >> 5;
    bit_index = virtual_key & 31u;
    mask = 1u << bit_index;
    old_value = g_key_state.words[word_index];

    if (is_down) {
        g_key_state.words[word_index] |= mask;
    } else {
        g_key_state.words[word_index] &= ~mask;
    }

    if (g_key_state.words[word_index] != old_value) {
        g_key_generation++;
        runtime_session_request_frame();
    }
}

void runtime_session_note_key_message(const MSG *msg)
{
    if (msg == NULL) {
        return;
    }

    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) {
        update_key_state((uint)msg->wParam, true);
    } else if (msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP) {
        update_key_state((uint)msg->wParam, false);
    }
}

void runtime_session_reset_key_state(void)
{
    ZeroMemory(&g_key_state, sizeof(g_key_state));
    g_key_generation++;
    g_last_key_generation = 0;
}

void runtime_session_init(const char *title, int default_width, int default_height, present_backend_kind_t backend_kind)
{
    g_title = title;
    g_default_width = default_width;
    g_default_height = default_height;
    g_present_backend_kind = backend_kind;
    g_vsync_enabled = true;
    g_runtime_error[0] = '\0';
    shader_buffers_reset(&g_shader_buffers);
    g_shader_buffers_cleanup = NULL;
    timing_init();
    frame_timing_reset();
    refresh_shader_quality(NULL);
    runtime_session_reset_key_state();
}

static void destroy_runtime_backend(void)
{
    present_backend_destroy();
    display_destroy();
    free(g_frame_pixels);
    g_frame_pixels = NULL;
    free(g_history_pixels);
    g_history_pixels = NULL;
    g_width = 0;
    g_height = 0;
    g_popup_width = 0;
    g_popup_height = 0;
}

void runtime_session_stop_shader(void)
{
    destroy_runtime_backend();
    runtime_release_shader_buffers();
    g_has_history = false;
    g_force_frame = false;
    g_frame_counter = 0;
    g_runtime_error[0] = '\0';
    g_capture_frames_remaining = 0;
    g_capture_frames_total = 0;
    g_capture_shader_id[0] = '\0';
    g_active_shader_color_space = SHADER_COLOR_SPACE_SDR_DISPLAY;
    frame_timing_reset();
    refresh_shader_quality(NULL);
}

static void detect_affinity_targets(void)
{
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;

    g_affinity_count = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) || process_mask == 0) {
        return;
    }

    for (DWORD bit = 0; bit < (DWORD)(sizeof(DWORD_PTR) * 8) && g_affinity_count < (int)(sizeof(g_affinity_masks) / sizeof(g_affinity_masks[0])); ++bit) {
        DWORD_PTR mask = ((DWORD_PTR)1) << bit;
        if ((process_mask & mask) != 0) {
            g_affinity_masks[g_affinity_count] = mask;
            g_affinity_processors[g_affinity_count] = bit;
            g_affinity_count++;
        }
    }
}

static void pin_thread_to_worker(HANDLE thread, int worker_index)
{
    int affinity_index;

    if (thread == NULL || g_affinity_count <= 0) {
        return;
    }

    affinity_index = worker_index % g_affinity_count;
    SetThreadAffinityMask(thread, g_affinity_masks[affinity_index]);
    SetThreadIdealProcessor(thread, g_affinity_processors[affinity_index]);
}

static void setup_worker(Worker *worker, int worker_index, int total_workers)
{
    LONGLONG height = g_height;

    worker->y_start = (int)((height * worker_index) / total_workers);
    worker->y_end = (int)((height * (worker_index + 1)) / total_workers);
    worker->worker_index = worker_index;
    worker->render = NULL;
}

static void worker_render(const Worker *worker)
{
    const bool keep_history = g_temporal_accumulation && g_history_pixels != NULL;
    const bool use_history = keep_history && g_has_history;
    vec4_t *history_pixels = g_history_pixels;
    vec4_t *frame_pixels = g_frame_pixels;

    if (worker->render == NULL || frame_pixels == NULL) {
        return;
    }

    for (int y = worker->y_start; y < worker->y_end; y++) {
        size_t row_base = (size_t)y * (size_t)g_width;

        for (int x = 0; x < g_width; x++) {
            size_t pixel_index = row_base + (size_t)x;
            vec4_t color = worker->render(vec2((float)x, (float)y), &worker->uniforms);

            if (use_history) {
                vec4_t old_color = history_pixels[pixel_index];
                float weight = 1.0f / (worker->uniforms.frame + 1);
                color = v4_add(v4_mul1(old_color, 1.0f - weight), v4_mul1(color, weight));
            }

            if (keep_history) {
                history_pixels[pixel_index] = color;
            }

            frame_pixels[pixel_index] = color;
        }
    }
}

static DWORD WINAPI worker_thread(LPVOID arg)
{
    Worker *worker = (Worker *)arg;

    while (1) {
        DWORD wait_result = WaitForSingleObject(g_work_sem, INFINITE);

        if (wait_result != WAIT_OBJECT_0) {
            break;
        }

        if (g_shutdown) {
            break;
        }

        worker_render(worker);
        if (!ReleaseSemaphore(g_done_sem, 1, NULL)) {
            break;
        }
    }

    return 0;
}

static void pool_clear_handles(void)
{
    for (int i = 0; i < MAX_RENDER_WORKERS; i++) {
        g_threads[i] = NULL;
    }
}

static bool pool_create(int num_threads)
{
    int requested_workers = (num_threads > 0) ? num_threads : 1;
    int created_threads = 0;

    if (requested_workers > MAX_RENDER_WORKERS) {
        requested_workers = MAX_RENDER_WORKERS;
    }

    g_total_workers = requested_workers;
    g_background_threads = 0;
    g_shutdown = 0;
    g_runtime_error[0] = '\0';

    detect_affinity_targets();
    pool_clear_handles();
    ZeroMemory(g_workers, sizeof(g_workers));

    g_work_sem = CreateSemaphore(NULL, 0, requested_workers, NULL);
    g_done_sem = CreateSemaphore(NULL, 0, requested_workers, NULL);

    if (g_work_sem == NULL) {
        runtime_set_error_hr("CreateSemaphore(work)");
        pool_destroy();
        return false;
    }

    if (g_done_sem == NULL) {
        runtime_set_error_hr("CreateSemaphore(done)");
        pool_destroy();
        return false;
    }

    for (int i = 0; i < requested_workers; i++) {
        setup_worker(&g_workers[i], i, g_total_workers);
        g_threads[i] = CreateThread(NULL, 0, worker_thread, &g_workers[i], 0, NULL);
        if (g_threads[i] == NULL) {
            runtime_set_error_hr("CreateThread");
            g_background_threads = created_threads;
            pool_destroy();
            return false;
        }

        created_threads++;
        pin_thread_to_worker(g_threads[i], g_workers[i].worker_index);
    }

    g_background_threads = created_threads;
    return true;
}

static void pool_destroy(void)
{
    g_shutdown = 1;

    if (g_background_threads > 0) {
        if (g_work_sem != NULL) {
            ReleaseSemaphore(g_work_sem, g_background_threads, NULL);
        }

        if (g_threads[0] != NULL) {
            WaitForMultipleObjects((DWORD)g_background_threads, g_threads, TRUE, INFINITE);
        }

        for (int i = 0; i < g_background_threads; i++) {
            if (g_threads[i] != NULL) {
                CloseHandle(g_threads[i]);
                g_threads[i] = NULL;
            }
        }
    }

    if (g_work_sem != NULL) {
        CloseHandle(g_work_sem);
        g_work_sem = NULL;
    }
    if (g_done_sem != NULL) {
        CloseHandle(g_done_sem);
        g_done_sem = NULL;
    }

    ZeroMemory(g_workers, sizeof(g_workers));
    pool_clear_handles();
    g_total_workers = 0;
    g_background_threads = 0;
}

bool runtime_session_start_workers(int num_threads)
{
    return pool_create(num_threads);
}

static bool ensure_history_buffer(void)
{
    size_t pixel_count;

    if (g_width <= 0 || g_height <= 0) {
        return false;
    }

    if (g_history_pixels != NULL) {
        return true;
    }

    pixel_count = (size_t)g_width * (size_t)g_height;
    g_history_pixels = (vec4_t *)malloc(pixel_count * sizeof(vec4_t));
    if (g_history_pixels == NULL) {
        runtime_set_error_text("Failed to allocate CPU history buffer.");
        return false;
    }

    memset(g_history_pixels, 0, pixel_count * sizeof(vec4_t));
    return true;
}

static bool ensure_frame_buffer(void)
{
    size_t pixel_count;

    if (g_width <= 0 || g_height <= 0) {
        return false;
    }

    if (g_frame_pixels != NULL) {
        return true;
    }

    pixel_count = (size_t)g_width * (size_t)g_height;
    g_frame_pixels = (vec4_t *)malloc(pixel_count * sizeof(vec4_t));
    if (g_frame_pixels == NULL) {
        runtime_set_error_text("Failed to allocate CPU presentation buffer.");
        return false;
    }

    memset(g_frame_pixels, 0, pixel_count * sizeof(vec4_t));
    return true;
}

bool runtime_session_prepare_shader_buffers(const shader_desc_t *shader)
{
    shader_buffers_t next_buffers;
    ShaderBuffersCleanupFunc next_cleanup = NULL;
    char error_text[512] = "";

    shader_buffers_reset(&next_buffers);

    if (shader == NULL) {
        runtime_set_error_text("No shader selected.");
        return false;
    }

    if (shader->buffers_init != NULL) {
        next_cleanup = shader->buffers_init(&next_buffers, error_text, sizeof(error_text));
        if (next_cleanup == NULL) {
            shader_buffers_default_cleanup(&next_buffers);
            if (error_text[0] != '\0') {
                runtime_set_error_text(error_text);
            } else {
                runtime_set_error_text("Shader buffer initialization failed.");
            }
            return false;
        }
    }

    runtime_release_shader_buffers();
    g_shader_buffers = next_buffers;
    g_shader_buffers_cleanup = next_cleanup;
    return true;
}

static bool create_display_backend(const runtime_display_params_t *display_params, const shader_desc_t *shader, int render_width, int render_height)
{
    display_callbacks_t empty_callbacks;
    present_backend_desc_t backend_desc;
    HINSTANCE instance;
    const char *title;
    HWND owner;
    int x;
    int y;
    int width;
    int height;
    const display_callbacks_t *callbacks;
    void *user_data;

    if (display_params == NULL) {
        runtime_set_error_text("Display parameters are invalid.");
        return false;
    }

    ZeroMemory(&empty_callbacks, sizeof(empty_callbacks));

    instance = (display_params->instance != NULL) ? display_params->instance : GetModuleHandleA(NULL);
    title = (display_params->title != NULL) ? display_params->title : g_title;
    owner = display_params->owner;
    x = display_params->x;
    y = display_params->y;
    width = display_params->width;
    height = display_params->height;
    callbacks = (display_params->callbacks != NULL) ? display_params->callbacks : &empty_callbacks;
    user_data = display_params->user_data;

    if (!display_create(instance, title, owner, x, y, width, height, callbacks, user_data)) {
        runtime_set_error_text("CreateWindowExA(display) failed.");
        return false;
    }

    ZeroMemory(&backend_desc, sizeof(backend_desc));
    backend_desc.hwnd = display_window();
    backend_desc.render_width = render_width;
    backend_desc.render_height = render_height;
    backend_desc.popup_width = width;
    backend_desc.popup_height = height;
    backend_desc.shader_color_space = (shader != NULL) ? shader->generated_color_space : SHADER_COLOR_SPACE_SDR_DISPLAY;
    backend_desc.vsync_enabled = g_vsync_enabled;

    if (!present_backend_create(g_present_backend_kind, &backend_desc)) {
        runtime_set_error_text(present_backend_error());
        display_destroy();
        return false;
    }

    display_show();
    return true;
}

bool runtime_session_ensure_for_shader(const shader_desc_t *shader, const runtime_display_params_t *display_params)
{
    int width;
    int height;
    bool needs_history;

    if (shader == NULL) {
        runtime_set_error_text("No shader selected.");
        return false;
    }

    runtime_session_target_dimensions(shader, &width, &height);
    needs_history = runtime_session_shader_uses_feature(shader, SHADER_FEATURE_TEMPORAL_ACCUMULATION);
    if (width <= 0 || height <= 0) {
        runtime_set_error_text("Shader dimensions are invalid.");
        return false;
    }

    if (display_params == NULL || display_params->width <= 0 || display_params->height <= 0) {
        runtime_set_error_text("Display layout is invalid.");
        return false;
    }

    if (g_width != width || g_height != height || g_popup_width != display_params->width || g_popup_height != display_params->height || !present_backend_is_ready() || display_window() == NULL) {
        destroy_runtime_backend();
        g_width = width;
        g_height = height;
        g_popup_width = display_params->width;
        g_popup_height = display_params->height;

        if (!ensure_frame_buffer()) {
            return false;
        }

        if (needs_history && !ensure_history_buffer()) {
            return false;
        }
        if (!needs_history) {
            free(g_history_pixels);
            g_history_pixels = NULL;
            g_has_history = false;
        }

        if (!create_display_backend(display_params, shader, width, height)) {
            destroy_runtime_backend();
            return false;
        }

        for (int i = 0; i < g_background_threads; i++) {
            setup_worker(&g_workers[i], i, g_total_workers);
        }
    } else {
        if (!ensure_frame_buffer()) {
            return false;
        }
        if (needs_history) {
            if (!ensure_history_buffer()) {
                return false;
            }
        } else {
            free(g_history_pixels);
            g_history_pixels = NULL;
            g_has_history = false;
        }
    }

    runtime_session_reset_for_shader(shader);
    return true;
}

bool runtime_session_resize_display(const shader_desc_t *shader, const runtime_display_params_t *display_params)
{
    present_backend_desc_t backend_desc;
    RECT old_rect = {0};
    int old_popup_width;
    int old_popup_height;
    bool had_rect;
    char saved_error[512];

    if (shader == NULL) {
        runtime_set_error_text("No active shader selected for display resize.");
        return false;
    }

    if (display_window() == NULL || !present_backend_is_ready()) {
        runtime_set_error_text("Display resize requested with no active presenter.");
        return false;
    }

    if (display_params == NULL || display_params->width <= 0 || display_params->height <= 0) {
        runtime_set_error_text("Display resize parameters are invalid.");
        return false;
    }

    had_rect = display_get_window_rect(&old_rect);
    old_popup_width = g_popup_width;
    old_popup_height = g_popup_height;

    if (!SetWindowPos(
            display_window(),
            NULL,
            display_params->x,
            display_params->y,
            display_params->width,
            display_params->height,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE))
    {
        runtime_set_error_hr("SetWindowPos(display)");
        return false;
    }

    present_backend_destroy();
    g_popup_width = display_params->width;
    g_popup_height = display_params->height;

    ZeroMemory(&backend_desc, sizeof(backend_desc));
    backend_desc.hwnd = display_window();
    backend_desc.render_width = g_width;
    backend_desc.render_height = g_height;
    backend_desc.popup_width = g_popup_width;
    backend_desc.popup_height = g_popup_height;
    backend_desc.shader_color_space = shader->generated_color_space;
    backend_desc.vsync_enabled = g_vsync_enabled;

    if (present_backend_create(g_present_backend_kind, &backend_desc)) {
        runtime_session_request_frame();
        return true;
    }

    snprintf(saved_error, sizeof(saved_error), "%s", present_backend_error());

    if (had_rect) {
        SetWindowPos(
            display_window(),
            NULL,
            old_rect.left,
            old_rect.top,
            old_popup_width,
            old_popup_height,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }

    g_popup_width = old_popup_width;
    g_popup_height = old_popup_height;
    backend_desc.popup_width = g_popup_width;
    backend_desc.popup_height = g_popup_height;

    if (!present_backend_create(g_present_backend_kind, &backend_desc)) {
        runtime_set_error_text(saved_error);
        return false;
    }

    runtime_set_error_text(saved_error);
    return false;
}

void runtime_session_reset_for_shader(const shader_desc_t *shader)
{
    refresh_shader_quality(shader);
    g_active_shader_color_space = (shader != NULL) ? shader->generated_color_space : SHADER_COLOR_SPACE_SDR_DISPLAY;
    timing_init();
    frame_timing_reset();
    g_frame_counter = 0;
    g_has_history = false;
    g_runtime_error[0] = '\0';
    g_last_mouse_generation = display_input_generation();
    g_last_key_generation = g_key_generation;
    runtime_session_request_frame();

    if (g_history_pixels != NULL) {
        memset(g_history_pixels, 0, (size_t)g_width * (size_t)g_height * sizeof(vec4_t));
    }
}

void runtime_session_position_display(int x, int y)
{
    if (display_window() == NULL) {
        return;
    }

    SetWindowPos(display_window(), NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

void runtime_session_set_vsync(bool enabled)
{
    g_vsync_enabled = enabled;

    if (present_backend_is_ready()) {
        present_backend_set_vsync(enabled);
    }

    frame_timing_reset();
    runtime_session_request_frame();
}

bool runtime_session_get_vsync(void)
{
    return g_vsync_enabled;
}

bool runtime_session_should_render_frame(const shader_desc_t *shader)
{
    uint mouse_generation;

    if (shader == NULL || !present_backend_is_ready()) {
        return false;
    }

    if (g_force_frame) {
        return true;
    }

    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_TEMPORAL_ACCUMULATION)) {
        return true;
    }

    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_TIME) || runtime_session_shader_uses_feature(shader, SHADER_FEATURE_FRAME)) {
        return true;
    }

    mouse_generation = display_input_generation();
    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_MOUSE) && mouse_generation != g_last_mouse_generation) {
        return true;
    }

    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_KEYS) && g_key_generation != g_last_key_generation) {
        return true;
    }

    return false;
}

bool runtime_session_render_frame(const shader_desc_t *shader)
{
    f32x4_surface_t surface;
    shader_uniforms_t uniforms;
    double frame_start_seconds;
    double frame_end_seconds;

    if (shader == NULL || shader->render == NULL) {
        runtime_set_error_text("No active shader to render.");
        return false;
    }

    frame_start_seconds = runtime_session_now_seconds();
    if (g_frame_pixels == NULL) {
        runtime_set_error_text("CPU presentation buffer is not available.");
        return false;
    }

    ZeroMemory(&uniforms, sizeof(uniforms));
    uniforms.resolution = vec2((float)g_width, (float)g_height);
    uniforms.mouse = vec4(-1.0f, -1.0f, -1.0f, -1.0f);

    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_TIME)) {
        uniforms.time = (float)runtime_session_now_seconds();
    }
    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_FRAME)) {
        uniforms.frame = g_frame_counter;
    }
    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_MOUSE)) {
        display_get_mouse_uniform(&uniforms.mouse, g_width, g_height);
    }
    if (runtime_session_shader_uses_feature(shader, SHADER_FEATURE_KEYS)) {
        uniforms.keys = g_key_state;
    }
    uniforms.buffers = &g_shader_buffers;

    for (int i = 0; i < g_background_threads; i++) {
        g_workers[i].uniforms = uniforms;
        g_workers[i].render = shader->render;
    }

    if (!ReleaseSemaphore(g_work_sem, g_background_threads, NULL)) {
        runtime_set_error_hr("ReleaseSemaphore(work)");
        return false;
    }

    for (int i = 0; i < g_background_threads; i++) {
        DWORD wait_result = WaitForSingleObject(g_done_sem, INFINITE);

        if (wait_result != WAIT_OBJECT_0) {
            runtime_set_error_text("Worker completion wait failed.");
            return false;
        }
    }

    if (g_temporal_accumulation && g_history_pixels != NULL) {
        g_has_history = true;
    }

    surface.width = g_width;
    surface.height = g_height;
    surface.stride_bytes = (int)((size_t)g_width * sizeof(vec4_t));
    surface.pixels = g_frame_pixels;

    if (!present_backend_present(&surface)) {
        runtime_set_error_text(present_backend_error());
        return false;
    }

    g_force_frame = false;
    g_last_mouse_generation = display_input_generation();
    g_last_key_generation = g_key_generation;
    g_frame_counter++;
    process_capture_request();

    frame_end_seconds = runtime_session_now_seconds();
    frame_timing_push(frame_end_seconds - frame_start_seconds);
    return true;
}

void runtime_session_get_frame_stats(double *average_seconds_out, int *sample_count_out)
{
    if (average_seconds_out != NULL) {
        *average_seconds_out = frame_timing_average();
    }
    if (sample_count_out != NULL) {
        *sample_count_out = g_frame_time_count;
    }
}

void runtime_session_get_worker_counts(int *total_workers_out, int *background_workers_out)
{
    if (total_workers_out != NULL) {
        *total_workers_out = g_total_workers;
    }
    if (background_workers_out != NULL) {
        *background_workers_out = g_background_threads;
    }
}

void runtime_session_get_render_size(int *width_out, int *height_out)
{
    if (width_out != NULL) {
        *width_out = g_width;
    }
    if (height_out != NULL) {
        *height_out = g_height;
    }
}

void runtime_session_get_popup_size(int *width_out, int *height_out)
{
    if (width_out != NULL) {
        *width_out = g_popup_width;
    }
    if (height_out != NULL) {
        *height_out = g_popup_height;
    }
}

const char *runtime_session_get_backend_name(void)
{
    if (present_backend_is_ready()) {
        return present_backend_display_name(present_backend_kind());
    }

    return present_backend_display_name(g_present_backend_kind);
}

const char *runtime_session_get_backend_notice(void)
{
    return present_backend_notice();
}

uint runtime_session_get_backend_notice_version(void)
{
    return present_backend_notice_version();
}

const char *runtime_session_get_notice(void)
{
    return g_runtime_notice;
}

uint runtime_session_get_notice_version(void)
{
    return (uint)g_runtime_notice_version;
}

void runtime_session_shutdown(void)
{
    pool_destroy();
    runtime_session_stop_shader();
}
