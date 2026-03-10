//Initialize the window and handle rendering in multiple threads

#include "defines.h"
#include "win.h"
#include "dxx12.h"

#include <string.h>

#define FRAME_TIME_HISTORY_COUNT 64

static int         g_width   = 0;
static int         g_height  = 0;
static const char *g_title   = NULL;
static HWND        g_hwnd    = NULL;
static int         g_is_open = 0;
static LARGE_INTEGER g_perf_frequency = {0};
static LARGE_INTEGER g_start_counter  = {0};
static uint        g_frame_counter = 0;
static bool        g_tempolatal_accumulation = false;
static bool        g_has_history = false;
static vec4_t     *g_frame_pixels = NULL;
static vec4_t     *g_history_pixels = NULL;
static ShaderCycleFunc g_shader_cycle = NULL;
static ShaderNameFunc  g_shader_name = NULL;

static DWORD_PTR   g_affinity_masks[64];
static DWORD       g_affinity_processors[64];
static int         g_affinity_count = 0;

static double      g_frame_times[FRAME_TIME_HISTORY_COUNT];
static double      g_frame_time_sum = 0.0;
static int         g_frame_time_index = 0;
static int         g_frame_time_count = 0;
static double      g_last_title_update = 0.0;

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

static void runtime_reset(void)
{
    timing_init();
    frame_timing_reset();
    g_last_title_update = 0.0;
    g_frame_counter = 0;
    g_has_history = false;

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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if ((wp == VK_PRIOR || wp == VK_NEXT) && g_shader_cycle != NULL) {
                g_shader_cycle((wp == VK_PRIOR) ? -1 : 1);
                runtime_reset();
                return 0;
            }
            if (wp == 'V') {
                dxx12_set_vsync(!dxx12_get_vsync());
                frame_timing_reset();
                g_last_title_update = 0.0;
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_is_open = 0;
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool window_create(const char *title, int width, int height)
{
    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSA wc    = {0};
    RECT rect = { 0, 0, width, height };

    g_width  = width;
    g_height = height;
    g_title  = title;
    timing_init();

    wc.lpfnWndProc  = WndProc;
    wc.hInstance    = hInst;
    wc.hCursor      = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = title;
    RegisterClassA(&wc);

    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowA(
        title, title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, hInst, NULL);

    if (g_hwnd == NULL) {
        MessageBoxA(NULL, "CreateWindowA failed.", title, MB_OK | MB_ICONERROR);
        return false;
    }

    if (!dxx12_create(g_hwnd, width, height)) {
        MessageBoxA(g_hwnd, dxx12_error(), title, MB_OK | MB_ICONERROR);
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        return false;
    }

    g_is_open = 1;
    return true;
}

void window_set_shader_switcher(ShaderCycleFunc cycle_func, ShaderNameFunc name_func)
{
    g_shader_cycle = cycle_func;
    g_shader_name = name_func;
}

typedef struct {
    int        y_start;
    int        y_end;
    int        worker_index;
    uint       current_frame;
    ULONGLONG  current_time;
    double     current_time_seconds;
    RenderFunc render;
} Worker;

static int      g_total_workers = 0;
static int      g_background_threads = 0;
static Worker  *g_workers     = NULL;
static Worker   g_main_worker = {0};
static HANDLE  *g_threads     = NULL;
static HANDLE   g_work_sem    = NULL;
static HANDLE   g_done_sem    = NULL;
static volatile int g_shutdown = 0;

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
    worker->y_end   = (int)((height * (worker_index + 1)) / total_workers);
    worker->worker_index = worker_index;
    worker->render = render;
}

static void worker_render(const Worker *w)
{
    const bool use_history = g_tempolatal_accumulation && g_has_history && g_history_pixels != NULL;
    vec4_t *history_pixels = g_history_pixels;
    vec4_t *frame_pixels = g_frame_pixels;

    for (int y = w->y_start; y < w->y_end; y++) {
            size_t row_base = (size_t)y * (size_t)g_width;
        for (int x = 0; x < g_width; x++) {
            size_t pixel_index = row_base + (size_t)x;
            vec4_t color = w->render(vec2(x, y), vec2(g_width, g_height), (float)w->current_time_seconds, w->current_frame);

            if (use_history) {
                vec4_t old_color = history_pixels[pixel_index];
                float weight = 1.0f / (w->current_frame + 1);
                color = v4_add(v4_mul1(old_color, 1.0f - weight), v4_mul1(color, weight));
            }

            if (history_pixels != NULL) {
                history_pixels[pixel_index] = color;
            }
            frame_pixels[pixel_index] = color;
        }
    }
}

static DWORD WINAPI worker_thread(LPVOID arg)
{
    Worker *w = (Worker *)arg;
    while (1) {
        WaitForSingleObject(g_work_sem, INFINITE);
        if (g_shutdown) {
            break;
        }

        worker_render(w);
        ReleaseSemaphore(g_done_sem, 1, NULL);
    }
    return 0;
}

static void pool_create(int num_threads, RenderFunc render)
{
    g_total_workers = (num_threads > 0) ? num_threads : 1;
    g_background_threads = (g_total_workers > 1) ? (g_total_workers - 1) : 0;
    g_shutdown = 0;

    detect_affinity_targets();
    setup_worker(&g_main_worker, g_total_workers - 1, g_total_workers, render);
    pin_thread_to_worker(GetCurrentThread(), g_main_worker.worker_index);

    if (g_background_threads <= 0) {
        return;
    }

    g_workers     = (Worker *)malloc((size_t)g_background_threads * sizeof(Worker));
    g_threads     = (HANDLE *)malloc((size_t)g_background_threads * sizeof(HANDLE));
    g_work_sem    = CreateSemaphore(NULL, 0, g_background_threads, NULL);
    g_done_sem    = CreateSemaphore(NULL, 0, g_background_threads, NULL);

    for (int i = 0; i < g_background_threads; i++) {
        setup_worker(&g_workers[i], i, g_total_workers, render);
        g_threads[i] = CreateThread(NULL, 0, worker_thread, &g_workers[i], 0, NULL);
        pin_thread_to_worker(g_threads[i], g_workers[i].worker_index);
    }
}

static bool pool_render_frame(void) {
    ULONGLONG current_time = GetTickCount64();
    double current_time_seconds = timing_now_seconds();

    if (!dxx12_begin_frame(&g_frame_pixels)) {
        return false;
    }

    g_main_worker.current_frame = g_frame_counter;
    g_main_worker.current_time = current_time;
    g_main_worker.current_time_seconds = current_time_seconds;

    for (int i = 0; i < g_background_threads; i++) {
        g_workers[i].current_frame = g_frame_counter;
        g_workers[i].current_time = current_time;
        g_workers[i].current_time_seconds = current_time_seconds;
    }

    if (g_background_threads > 0) {
        ReleaseSemaphore(g_work_sem, g_background_threads, NULL);
    }

    worker_render(&g_main_worker);

    for (int i = 0; i < g_background_threads; i++) {
        WaitForSingleObject(g_done_sem, INFINITE);
    }

    if (g_history_pixels != NULL) {
        g_has_history = true;
    }

    return true;
}

static void pool_destroy(void) {
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

void window_run(RenderFunc render, int num_threads, bool temporal_accumulation)
{
    size_t pixel_count = (size_t)g_width * (size_t)g_height;

    g_tempolatal_accumulation = temporal_accumulation;
    g_has_history = false;

    if (g_tempolatal_accumulation) {
        g_history_pixels = (vec4_t *)malloc(pixel_count * sizeof(vec4_t));
        if (g_history_pixels == NULL) {
            MessageBoxA(g_hwnd, "Failed to allocate CPU history buffer.", g_title, MB_OK | MB_ICONERROR);
            dxx12_destroy();
            return;
        }
    }

    pool_create(num_threads, render);

    runtime_reset();

    while (g_is_open) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!g_is_open) {
            break;
        }

        if (IsIconic(g_hwnd)) {
            Sleep(10);
            continue;
        }

        double frame_start_seconds = timing_now_seconds();

        if (!pool_render_frame()) {
            MessageBoxA(g_hwnd, dxx12_error(), g_title, MB_OK | MB_ICONERROR);
            g_is_open = 0;
            break;
        }

        if (!dxx12_end_frame()) {
            MessageBoxA(g_hwnd, dxx12_error(), g_title, MB_OK | MB_ICONERROR);
            g_is_open = 0;
            break;
        }

        double frame_end_seconds = timing_now_seconds();
        frame_timing_push(frame_end_seconds - frame_start_seconds);

        //Update title with FPS and render time
        if (frame_end_seconds - g_last_title_update >= 0.1) {
            char title[256];
            double average_frame_seconds = frame_timing_average();
            double ms = average_frame_seconds * 1000.0;
            double fps = (average_frame_seconds > 0.0) ? (1.0 / average_frame_seconds) : 0.0;
            const char *shader_name = (g_shader_name != NULL) ? g_shader_name() : "unknown";

            snprintf(title, sizeof(title), "%s | Shader : %s | Rendering : %.1ffps, %.2fms avg/%d | VSync : %s | Time : %.2fs",
                g_title,
                shader_name,
                fps,
                ms,
                g_frame_time_count,
                dxx12_get_vsync() ? "on" : "off",
                frame_end_seconds);

            SetWindowTextA(g_hwnd, title);
            g_last_title_update = frame_end_seconds;
        }

        g_frame_counter ++;
    }

    pool_destroy();
    free(g_history_pixels);
    g_history_pixels = NULL;
    dxx12_destroy();
}
