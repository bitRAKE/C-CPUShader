//Coordinate the render loop, worker pool, and window modules

#include "defines.h"
#include "win.h"
#include "dxx12.h"
#include "display.h"
#include "stats.h"

#include <string.h>

#define FRAME_TIME_HISTORY_COUNT 64
#define WINDOW_GAP 24

typedef struct {
    int        y_start;
    int        y_end;
    int        worker_index;
    shader_uniforms_t uniforms;
    RenderFunc render;
} Worker;

static int           g_width = 0;
static int           g_height = 0;
static const char   *g_title = NULL;
static int           g_is_open = 0;
static bool          g_is_closing = false;
static LARGE_INTEGER g_perf_frequency = {0};
static LARGE_INTEGER g_start_counter = {0};
static uint          g_frame_counter = 0;
static bool          g_tempolatal_accumulation = false;
static bool          g_has_history = false;
static vec4_t       *g_frame_pixels = NULL;
static vec4_t       *g_history_pixels = NULL;
static ShaderCycleFunc        g_shader_cycle = NULL;
static ShaderNameFunc         g_shader_name = NULL;
static ShaderAccumulationFunc g_shader_accumulation = NULL;

static DWORD_PTR     g_affinity_masks[64];
static DWORD         g_affinity_processors[64];
static int           g_affinity_count = 0;

static double        g_frame_times[FRAME_TIME_HISTORY_COUNT];
static double        g_frame_time_sum = 0.0;
static int           g_frame_time_index = 0;
static int           g_frame_time_count = 0;
static double        g_last_status_update = 0.0;

static int           g_total_workers = 0;
static int           g_background_threads = 0;
static Worker       *g_workers = NULL;
static HANDLE       *g_threads = NULL;
static HANDLE        g_work_sem = NULL;
static HANDLE        g_done_sem = NULL;
static volatile int  g_shutdown = 0;

static void update_status_window(double frame_end_seconds, bool force);

static void timing_init(void)
{
    QueryPerformanceFrequency(&g_perf_frequency);
    QueryPerformanceCounter(&g_start_counter);
}

static double timing_now_seconds(void)
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

static void refresh_shader_quality(void)
{
    g_tempolatal_accumulation = (g_shader_accumulation != NULL) ? g_shader_accumulation() : false;
}

static void request_status_refresh(void)
{
    g_last_status_update = 0.0;
}

static HWND status_parent_window(void)
{
    if (stats_window() != NULL) {
        return stats_window();
    }

    return display_window();
}

static void runtime_reset(void)
{
    timing_init();
    frame_timing_reset();
    g_frame_counter = 0;
    g_has_history = false;
    request_status_refresh();

    if (g_history_pixels != NULL) {
        memset(g_history_pixels, 0, (size_t)g_width * (size_t)g_height * sizeof(vec4_t));
    }
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

static void close_application(void)
{
    if (g_is_closing) {
        return;
    }

    g_is_closing = true;
    g_is_open = 0;
    display_destroy();
    stats_destroy();
}

static void cycle_shader(int direction)
{
    if (g_shader_cycle == NULL) {
        return;
    }

    g_shader_cycle(direction);
    refresh_shader_quality();
    runtime_reset();
    update_status_window(timing_now_seconds(), true);
}

static void set_vsync_enabled(bool enabled)
{
    dxx12_set_vsync(enabled);
    frame_timing_reset();
    request_status_refresh();
    update_status_window(timing_now_seconds(), true);
}

static bool handle_keydown(HWND hwnd, WPARAM key)
{
    (void)hwnd;

    if (key == VK_ESCAPE) {
        close_application();
        return true;
    }

    if (key == VK_PRIOR) {
        cycle_shader(-1);
        return true;
    }

    if (key == VK_NEXT) {
        cycle_shader(1);
        return true;
    }

    if (key == 'V') {
        set_vsync_enabled(!dxx12_get_vsync());
        return true;
    }

    if (key == 'R') {
        runtime_reset();
        update_status_window(timing_now_seconds(), true);
        return true;
    }

    return false;
}

static bool shared_on_keydown(HWND hwnd, WPARAM key, void *user_data)
{
    (void)user_data;
    return handle_keydown(hwnd, key);
}

static void shared_on_close(HWND hwnd, void *user_data)
{
    (void)hwnd;
    (void)user_data;
    close_application();
}

static void stats_on_cycle_shader(int direction, void *user_data)
{
    (void)user_data;
    cycle_shader(direction);
}

static void stats_on_reset(void *user_data)
{
    (void)user_data;
    runtime_reset();
    update_status_window(timing_now_seconds(), true);
}

static void stats_on_set_vsync(bool enabled, void *user_data)
{
    (void)user_data;
    set_vsync_enabled(enabled);
}

static void update_status_window(double frame_end_seconds, bool force)
{
    stats_state_t state;
    double average_frame_seconds;

    if (stats_window() == NULL) {
        return;
    }

    if (!force && frame_end_seconds - g_last_status_update < 0.1) {
        return;
    }

    average_frame_seconds = frame_timing_average();
    state.shader_name = (g_shader_name != NULL) ? g_shader_name() : "unknown";
    state.temporal_accumulation = g_tempolatal_accumulation;
    state.fps = (average_frame_seconds > 0.0) ? (1.0 / average_frame_seconds) : 0.0;
    state.milliseconds = average_frame_seconds * 1000.0;
    state.frame_sample_count = g_frame_time_count;
    state.time_seconds = frame_end_seconds;
    state.total_workers = g_total_workers;
    state.background_workers = g_background_threads;
    state.display_width = g_width;
    state.display_height = g_height;
    state.vsync_enabled = dxx12_get_vsync();

    stats_update(&state);
    g_last_status_update = frame_end_seconds;
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

static void setup_worker(Worker *worker, int worker_index, int total_workers, RenderFunc render)
{
    LONGLONG height = g_height;

    worker->y_start = (int)((height * worker_index) / total_workers);
    worker->y_end = (int)((height * (worker_index + 1)) / total_workers);
    worker->worker_index = worker_index;
    worker->render = render;
}

static void worker_render(const Worker *worker)
{
    const bool keep_history = g_tempolatal_accumulation && g_history_pixels != NULL;
    const bool use_history = keep_history && g_has_history;
    vec4_t *history_pixels = g_history_pixels;
    vec4_t *frame_pixels = g_frame_pixels;

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
        WaitForSingleObject(g_work_sem, INFINITE);
        if (g_shutdown) {
            break;
        }

        worker_render(worker);
        ReleaseSemaphore(g_done_sem, 1, NULL);
    }

    return 0;
}

static void pool_create(int num_threads, RenderFunc render)
{
    g_total_workers = (num_threads > 0) ? num_threads : 1;
    g_background_threads = g_total_workers;
    g_shutdown = 0;

    detect_affinity_targets();

    g_workers = (Worker *)malloc((size_t)g_background_threads * sizeof(Worker));
    g_threads = (HANDLE *)malloc((size_t)g_background_threads * sizeof(HANDLE));
    g_work_sem = CreateSemaphore(NULL, 0, g_background_threads, NULL);
    g_done_sem = CreateSemaphore(NULL, 0, g_background_threads, NULL);

    for (int i = 0; i < g_background_threads; i++) {
        setup_worker(&g_workers[i], i, g_total_workers, render);
        g_threads[i] = CreateThread(NULL, 0, worker_thread, &g_workers[i], 0, NULL);
        pin_thread_to_worker(g_threads[i], g_workers[i].worker_index);
    }
}

static bool pool_render_frame(void)
{
    shader_uniforms_t uniforms;

    if (!dxx12_begin_frame(&g_frame_pixels)) {
        return false;
    }

    uniforms.resolution = vec2((float)g_width, (float)g_height);
    uniforms.time = (float)timing_now_seconds();
    uniforms.frame = g_frame_counter;
    display_get_mouse_uniform(&uniforms.mouse, g_height);

    for (int i = 0; i < g_background_threads; i++) {
        g_workers[i].uniforms = uniforms;
    }

    ReleaseSemaphore(g_work_sem, g_background_threads, NULL);

    for (int i = 0; i < g_background_threads; i++) {
        WaitForSingleObject(g_done_sem, INFINITE);
    }

    if (g_tempolatal_accumulation && g_history_pixels != NULL) {
        g_has_history = true;
    }

    return true;
}

static void pool_destroy(void)
{
    g_shutdown = 1;

    if (g_background_threads > 0) {
        ReleaseSemaphore(g_work_sem, g_background_threads, NULL);
        WaitForMultipleObjects(g_background_threads, g_threads, TRUE, INFINITE);

        for (int i = 0; i < g_background_threads; i++) {
            CloseHandle(g_threads[i]);
        }

        CloseHandle(g_work_sem);
        CloseHandle(g_done_sem);
    }

    free(g_workers);
    free(g_threads);

    g_workers = NULL;
    g_threads = NULL;
    g_work_sem = NULL;
    g_done_sem = NULL;
    g_total_workers = 0;
    g_background_threads = 0;
}

static void position_windows(void)
{
    RECT work_area;
    RECT stats_rect;
    int stats_x;
    int stats_y;
    int display_x;
    int display_y;

    if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.top = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    stats_x = work_area.left + 32;
    stats_y = work_area.top + 32;
    SetWindowPos(stats_window(), NULL, stats_x, stats_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);

    if (!stats_get_window_rect(&stats_rect)) {
        stats_rect.left = stats_x;
        stats_rect.top = stats_y;
        stats_rect.right = stats_x + 320;
        stats_rect.bottom = stats_y + 220;
    }

    display_x = stats_rect.right + WINDOW_GAP;
    display_y = stats_rect.top;

    if (display_x + g_width > work_area.right) {
        display_x = work_area.right - g_width;
    }
    if (display_y + g_height > work_area.bottom) {
        display_y = work_area.bottom - g_height;
    }
    if (display_x < work_area.left) {
        display_x = work_area.left;
    }
    if (display_y < work_area.top) {
        display_y = work_area.top;
    }

    SetWindowPos(display_window(), NULL, display_x, display_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

bool window_create(const char *title, int width, int height)
{
    HINSTANCE instance = GetModuleHandleA(NULL);
    stats_callbacks_t stats_callbacks;
    display_callbacks_t display_callbacks;

    g_width = width;
    g_height = height;
    g_title = title;
    g_is_closing = false;
    timing_init();

    ZeroMemory(&stats_callbacks, sizeof(stats_callbacks));
    stats_callbacks.on_keydown = shared_on_keydown;
    stats_callbacks.on_cycle_shader = stats_on_cycle_shader;
    stats_callbacks.on_reset = stats_on_reset;
    stats_callbacks.on_set_vsync = stats_on_set_vsync;
    stats_callbacks.on_close = shared_on_close;

    if (!stats_create(instance, title, &stats_callbacks, NULL)) {
        MessageBoxA(NULL, "CreateDialogParamA(status) failed.", title, MB_OK | MB_ICONERROR);
        return false;
    }

    ZeroMemory(&display_callbacks, sizeof(display_callbacks));
    display_callbacks.on_keydown = shared_on_keydown;
    display_callbacks.on_close = shared_on_close;

    if (!display_create(instance, title, stats_window(), 0, 0, width, height, &display_callbacks, NULL)) {
        MessageBoxA(stats_window(), "CreateWindowExA(display) failed.", title, MB_OK | MB_ICONERROR);
        stats_destroy();
        return false;
    }

    position_windows();

    if (!dxx12_create(display_window(), width, height)) {
        MessageBoxA(status_parent_window(), dxx12_error(), title, MB_OK | MB_ICONERROR);
        display_destroy();
        stats_destroy();
        return false;
    }

    stats_show();
    display_show();
    display_focus();

    g_is_open = 1;
    update_status_window(0.0, true);
    return true;
}

void window_set_shader_switcher(ShaderCycleFunc cycle_func, ShaderNameFunc name_func, ShaderAccumulationFunc accumulation_func)
{
    g_shader_cycle = cycle_func;
    g_shader_name = name_func;
    g_shader_accumulation = accumulation_func;
    refresh_shader_quality();
    update_status_window(0.0, true);
}

void window_run(RenderFunc render, int num_threads)
{
    size_t pixel_count = (size_t)g_width * (size_t)g_height;

    g_has_history = false;
    refresh_shader_quality();

    g_history_pixels = (vec4_t *)malloc(pixel_count * sizeof(vec4_t));
    if (g_history_pixels == NULL) {
        MessageBoxA(status_parent_window(), "Failed to allocate CPU history buffer.", g_title, MB_OK | MB_ICONERROR);
        dxx12_destroy();
        return;
    }

    pool_create(num_threads, render);
    runtime_reset();
    update_status_window(0.0, true);

    while (g_is_open) {
        MSG msg;

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_is_open = 0;
                break;
            }

            if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) && handle_keydown(msg.hwnd, msg.wParam)) {
                continue;
            }

            if (stats_is_dialog_message(&msg)) {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!g_is_open) {
            break;
        }

        {
            double frame_start_seconds = timing_now_seconds();

            if (!pool_render_frame()) {
                MessageBoxA(status_parent_window(), dxx12_error(), g_title, MB_OK | MB_ICONERROR);
                close_application();
                break;
            }

            if (!dxx12_end_frame()) {
                MessageBoxA(status_parent_window(), dxx12_error(), g_title, MB_OK | MB_ICONERROR);
                close_application();
                break;
            }

            {
                double frame_end_seconds = timing_now_seconds();
                frame_timing_push(frame_end_seconds - frame_start_seconds);
                update_status_window(frame_end_seconds, false);
            }
        }

        g_frame_counter++;
    }

    pool_destroy();
    free(g_history_pixels);
    g_history_pixels = NULL;
    dxx12_destroy();
    display_destroy();
    stats_destroy();
    g_is_closing = false;
}
