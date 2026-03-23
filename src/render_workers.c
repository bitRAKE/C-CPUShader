#include "render_workers.h"

#include <string.h>

#define MAX_RENDER_WORKERS MAXIMUM_WAIT_OBJECTS

typedef struct {
    int               y_start;
    int               y_end;
    int               worker_index;
    shader_uniforms_t uniforms;
    RenderFunc        render;
    const render_frame_context_t *context;
    HANDLE            work_event;
    HANDLE            done_event;
} Worker;

static int           g_total_workers = 0;
static int           g_background_threads = 0;
static Worker        g_workers[MAX_RENDER_WORKERS];
static HANDLE        g_threads[MAX_RENDER_WORKERS];
static volatile int  g_shutdown = 0;

static DWORD_PTR     g_affinity_masks[64];
static DWORD         g_affinity_processors[64];
static int           g_affinity_count = 0;

static char          g_worker_error[512] = "";

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

static void setup_worker(Worker *worker, int worker_index, int total_workers, int height)
{
    LONGLONG h = height;

    worker->y_start = (int)((h * worker_index) / total_workers);
    worker->y_end = (int)((h * (worker_index + 1)) / total_workers);
    worker->worker_index = worker_index;
    ZeroMemory(&worker->uniforms, sizeof(worker->uniforms));
    worker->render = NULL;
    worker->context = NULL;
}

static void worker_render(const Worker *worker, const shader_uniforms_t *uniforms, RenderFunc render)
{
    const render_frame_context_t *ctx = worker->context;
    bool keep_history;
    bool use_history;
    vec4_t *history_pixels;
    vec4_t *frame_pixels;

    if (render == NULL || uniforms == NULL || ctx == NULL || ctx->frame_pixels == NULL) {
        return;
    }

    keep_history = ctx->temporal_accumulation && ctx->history_pixels != NULL;
    use_history = keep_history && ctx->has_history;
    history_pixels = ctx->history_pixels;
    frame_pixels = ctx->frame_pixels;

    for (int y = worker->y_start; y < worker->y_end; y++) {
        size_t row_base = (size_t)y * (size_t)ctx->width;

        for (int x = 0; x < ctx->width; x++) {
            size_t pixel_index = row_base + (size_t)x;
            vec4_t color = render(vec2((float)x, (float)y), uniforms);

            if (use_history) {
                vec4_t old_color = history_pixels[pixel_index];
                float weight = 1.0f / (uniforms->frame + 1);
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
        DWORD wait_result = WaitForSingleObject(worker->work_event, INFINITE);

        if (wait_result != WAIT_OBJECT_0) {
            break;
        }

        if (g_shutdown) {
            break;
        }

        MemoryBarrier();
        worker_render(worker, &worker->uniforms, worker->render);
        if (!SetEvent(worker->done_event)) {
            break;
        }
    }

    return 0;
}

static void pool_clear_handles(void)
{
    for (int i = 0; i < MAX_RENDER_WORKERS; i++) {
        g_threads[i] = NULL;
        g_workers[i].work_event = NULL;
        g_workers[i].done_event = NULL;
    }
}

bool render_workers_create(int num_threads)
{
    int requested_workers = (num_threads > 0) ? num_threads : 1;
    int created_threads = 0;

    if (requested_workers > MAX_RENDER_WORKERS) {
        requested_workers = MAX_RENDER_WORKERS;
    }

    g_total_workers = requested_workers;
    g_background_threads = 0;
    g_shutdown = 0;
    g_worker_error[0] = '\0';

    detect_affinity_targets();
    pool_clear_handles();
    ZeroMemory(g_workers, sizeof(g_workers));

    for (int i = 0; i < requested_workers; i++) {
        setup_worker(&g_workers[i], i, g_total_workers, 0);
        g_workers[i].work_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (g_workers[i].work_event == NULL) {
            render_workers_destroy();
            return false;
        }
        g_workers[i].done_event = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (g_workers[i].done_event == NULL) {
            render_workers_destroy();
            return false;
        }
        g_threads[i] = CreateThread(NULL, 0, worker_thread, &g_workers[i], 0, NULL);
        if (g_threads[i] == NULL) {
            g_background_threads = created_threads;
            render_workers_destroy();
            return false;
        }

        created_threads++;
        pin_thread_to_worker(g_threads[i], g_workers[i].worker_index);
    }

    g_background_threads = created_threads;
    return true;
}

void render_workers_destroy(void)
{
    g_shutdown = 1;

    if (g_background_threads > 0) {
        for (int i = 0; i < g_background_threads; i++) {
            if (g_workers[i].work_event != NULL) {
                SetEvent(g_workers[i].work_event);
            }
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

    for (int i = 0; i < MAX_RENDER_WORKERS; i++) {
        if (g_workers[i].work_event != NULL) {
            CloseHandle(g_workers[i].work_event);
            g_workers[i].work_event = NULL;
        }
        if (g_workers[i].done_event != NULL) {
            CloseHandle(g_workers[i].done_event);
            g_workers[i].done_event = NULL;
        }
    }

    ZeroMemory(g_workers, sizeof(g_workers));
    pool_clear_handles();
    g_total_workers = 0;
    g_background_threads = 0;
}

void render_workers_setup_rows(int height)
{
    for (int i = 0; i < g_background_threads; i++) {
        setup_worker(&g_workers[i], i, g_total_workers, height);
    }
}

void render_workers_dispatch(const shader_uniforms_t *uniforms, RenderFunc render, const render_frame_context_t *context)
{
    HANDLE done_handles[MAX_RENDER_WORKERS];

    for (int i = 0; i < g_background_threads; i++) {
        g_workers[i].uniforms = *uniforms;
        g_workers[i].render = render;
        g_workers[i].context = context;
        if (g_workers[i].done_event == NULL || g_workers[i].work_event == NULL) {
            return;
        }
        ResetEvent(g_workers[i].done_event);
        done_handles[i] = g_workers[i].done_event;
    }

    MemoryBarrier();

    for (int i = 0; i < g_background_threads; i++) {
        if (!SetEvent(g_workers[i].work_event)) {
            return;
        }
    }

    if (g_background_threads > 0) {
        WaitForMultipleObjects((DWORD)g_background_threads, done_handles, TRUE, INFINITE);
    }
}

void render_workers_get_counts(int *total_out, int *background_out)
{
    if (total_out != NULL) {
        *total_out = g_total_workers;
    }
    if (background_out != NULL) {
        *background_out = g_background_threads;
    }
}
