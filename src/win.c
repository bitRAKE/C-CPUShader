//Coordinate the host shell: dialog, selection, execute/stop, and message loop

#include "defines.h"
#include "win.h"
#include "runtime_session.h"
#include "display.h"
#include "stats.h"

#include <limits.h>
#include <string.h>

#define WINDOW_GAP 24
#define WINDOW_MARGIN 32

typedef struct {
    int x;
    int y;
    int width;
    int height;
} display_layout_t;

static const char             *g_title = NULL;
static int                     g_is_open = 0;
static bool                    g_is_closing = false;
static double                  g_last_status_update = 0.0;
static bool                    g_stats_positioned = false;
static shader_host_callbacks_t g_shader_host = {0};

static void update_status_window(double frame_end_seconds, bool force);
static bool shared_on_keydown(HWND hwnd, WPARAM key, void *user_data);
static void shared_on_close(HWND hwnd, void *user_data);
static void compute_display_layout(int render_width, int render_height, display_layout_t *layout_out);

static const shader_desc_t *selected_shader(void)
{
    if (g_shader_host.get_selected_shader == NULL) {
        return NULL;
    }

    return g_shader_host.get_selected_shader();
}

static const shader_desc_t *active_shader(void)
{
    if (g_shader_host.get_active_shader == NULL) {
        return NULL;
    }

    return g_shader_host.get_active_shader();
}

static int selected_shader_index(void)
{
    if (g_shader_host.get_selected_index == NULL) {
        return -1;
    }

    return g_shader_host.get_selected_index();
}

static HWND status_parent_window(void)
{
    if (stats_window() != NULL) {
        return stats_window();
    }

    return display_window();
}

static void close_application(void)
{
    if (g_is_closing) {
        return;
    }

    g_is_closing = true;
    g_is_open = 0;
    runtime_session_shutdown();
    stats_destroy();
}

static void format_feature_list(uint feature_flags, char *buffer, size_t buffer_size)
{
    bool first = true;

    buffer[0] = '\0';

    if ((feature_flags & SHADER_FEATURE_TEMPORAL_ACCUMULATION) != 0) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%saccumulation", first ? "" : ", ");
        first = false;
    }
    if ((feature_flags & SHADER_FEATURE_TIME) != 0) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%stime", first ? "" : ", ");
        first = false;
    }
    if ((feature_flags & SHADER_FEATURE_MOUSE) != 0) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%smouse", first ? "" : ", ");
        first = false;
    }
    if ((feature_flags & SHADER_FEATURE_KEYS) != 0) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%skeys", first ? "" : ", ");
        first = false;
    }
    if ((feature_flags & SHADER_FEATURE_FRAME) != 0) {
        snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%sframe", first ? "" : ", ");
        first = false;
    }

    if (first) {
        snprintf(buffer, buffer_size, "none");
    }
}

static void format_expectations_text(const shader_desc_t *shader, char *buffer, size_t buffer_size)
{
    char features[96];

    if (shader == NULL) {
        snprintf(buffer, buffer_size, "Select a shader.");
        return;
    }

    format_feature_list(shader->feature_flags, features, sizeof(features));
    snprintf(buffer, buffer_size, "features: %s", features);
}

static void fit_popup_size(int render_width, int render_height, int avail_width, int avail_height, int *popup_width_out, int *popup_height_out)
{
    int popup_width;
    int popup_height;

    if (popup_width_out == NULL || popup_height_out == NULL) {
        return;
    }

    if (render_width <= 0 || render_height <= 0) {
        *popup_width_out = 1;
        *popup_height_out = 1;
        return;
    }

    if (avail_width <= 0) {
        avail_width = render_width;
    }
    if (avail_height <= 0) {
        avail_height = render_height;
    }

    if (render_width <= avail_width && render_height <= avail_height) {
        int scale_x = avail_width / render_width;
        int scale_y = avail_height / render_height;
        int scale = min(scale_x, scale_y);

        if (scale < 1) {
            scale = 1;
        }

        *popup_width_out = render_width * scale;
        *popup_height_out = render_height * scale;
        return;
    }

    if ((LONGLONG)avail_width * (LONGLONG)render_height <= (LONGLONG)avail_height * (LONGLONG)render_width) {
        popup_width = max(1, avail_width);
        popup_height = max(1, (int)(((LONGLONG)popup_width * (LONGLONG)render_height) / (LONGLONG)render_width));
    } else {
        popup_height = max(1, avail_height);
        popup_width = max(1, (int)(((LONGLONG)popup_height * (LONGLONG)render_width) / (LONGLONG)render_height));
    }

    *popup_width_out = popup_width;
    *popup_height_out = popup_height;
}

static int layout_area_score(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return -1;
    }

    if (width > 46340 || height > 46340) {
        return INT_MAX;
    }

    return width * height;
}

static void compute_display_layout(int render_width, int render_height, display_layout_t *layout_out)
{
    RECT work_area;
    RECT stats_rect;
    int stats_x;
    int stats_y;
    display_layout_t best = {0};
    int best_score = -1;
    int slot_x[3];
    int slot_y[3];
    int slot_width[3];
    int slot_height[3];

    if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.top = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    stats_x = work_area.left + 32;
    stats_y = work_area.top + 32;
    if (!g_stats_positioned && stats_window() != NULL) {
        SetWindowPos(stats_window(), NULL, stats_x, stats_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        g_stats_positioned = true;
    }

    if (!stats_get_window_rect(&stats_rect)) {
        stats_rect.left = stats_x;
        stats_rect.top = stats_y;
        stats_rect.right = stats_x + 372;
        stats_rect.bottom = stats_y + 272;
    }

    slot_x[0] = stats_rect.right + WINDOW_GAP;
    slot_y[0] = stats_rect.top;
    slot_width[0] = work_area.right - slot_x[0];
    slot_height[0] = work_area.bottom - slot_y[0];

    slot_x[1] = stats_rect.left;
    slot_y[1] = stats_rect.bottom + WINDOW_GAP;
    slot_width[1] = work_area.right - slot_x[1];
    slot_height[1] = work_area.bottom - slot_y[1];

    slot_x[2] = work_area.left + WINDOW_MARGIN;
    slot_y[2] = work_area.top + WINDOW_MARGIN;
    slot_width[2] = (work_area.right - work_area.left) - WINDOW_MARGIN * 2;
    slot_height[2] = (work_area.bottom - work_area.top) - WINDOW_MARGIN * 2;

    for (int i = 0; i < 3; i++) {
        display_layout_t candidate = {0};
        int score;

        fit_popup_size(render_width, render_height, slot_width[i], slot_height[i], &candidate.width, &candidate.height);
        candidate.x = slot_x[i];
        candidate.y = slot_y[i];

        if (candidate.x + candidate.width > work_area.right) {
            candidate.x = work_area.right - candidate.width;
        }
        if (candidate.y + candidate.height > work_area.bottom) {
            candidate.y = work_area.bottom - candidate.height;
        }
        if (candidate.x < work_area.left) {
            candidate.x = work_area.left;
        }
        if (candidate.y < work_area.top) {
            candidate.y = work_area.top;
        }

        score = layout_area_score(candidate.width, candidate.height);
        if (score > best_score) {
            best = candidate;
            best_score = score;
        }
    }

    if (layout_out != NULL) {
        *layout_out = best;
    }
}

static void position_windows(void)
{
    display_layout_t layout = {0};
    int render_width = 0;
    int render_height = 0;

    runtime_session_get_render_size(&render_width, &render_height);
    if (display_window() == NULL || render_width <= 0 || render_height <= 0) {
        return;
    }

    compute_display_layout(render_width, render_height, &layout);
    runtime_session_position_display(layout.x, layout.y);
}

static void update_status_window(double frame_end_seconds, bool force)
{
    stats_state_t state;
    const shader_desc_t *selected = selected_shader();
    const shader_desc_t *active = active_shader();
    char expectations[256];
    double average_frame_seconds = 0.0;
    int frame_sample_count = 0;
    int total_workers = 0;
    int background_workers = 0;
    int render_width = 0;
    int render_height = 0;
    int popup_width = 0;
    int popup_height = 0;
    display_layout_t preview_layout = {0};

    if (stats_window() == NULL) {
        return;
    }

    if (!force && frame_end_seconds - g_last_status_update < 0.1) {
        return;
    }

    runtime_session_get_frame_stats(&average_frame_seconds, &frame_sample_count);
    runtime_session_get_worker_counts(&total_workers, &background_workers);
    format_expectations_text(selected, expectations, sizeof(expectations));

    ZeroMemory(&state, sizeof(state));

    state.shader_name = (active != NULL) ? active->display_name : "idle";
    state.selected_expectations = expectations;
    state.selected_blurb = (selected != NULL) ? selected->blurb : "Select a shader to inspect its details.";
    state.fps = (active != NULL && average_frame_seconds > 0.0) ? (1.0 / average_frame_seconds) : 0.0;
    state.milliseconds = (active != NULL) ? (average_frame_seconds * 1000.0) : 0.0;
    state.frame_sample_count = (active != NULL) ? frame_sample_count : 0;
    state.time_seconds = (active != NULL) ? frame_end_seconds : 0.0;
    state.total_workers = total_workers;
    state.background_workers = background_workers;
    state.vsync_enabled = runtime_session_get_vsync();
    state.can_execute = (selected != NULL || active != NULL);
    state.can_reset = (active != NULL);
    state.shader_running = (active != NULL);

    runtime_session_get_render_size(&render_width, &render_height);
    runtime_session_get_popup_size(&popup_width, &popup_height);
    if (active != NULL && render_width > 0 && render_height > 0 && popup_width > 0 && popup_height > 0) {
        state.render_width = render_width;
        state.render_height = render_height;
        state.popup_width = popup_width;
        state.popup_height = popup_height;
    } else if (selected != NULL) {
        runtime_session_target_dimensions(selected, &state.render_width, &state.render_height);
        if (state.render_width > 0 && state.render_height > 0) {
            compute_display_layout(state.render_width, state.render_height, &preview_layout);
            state.popup_width = preview_layout.width;
            state.popup_height = preview_layout.height;
        }
    }

    stats_update(&state);
    g_last_status_update = frame_end_seconds;
}

static void set_vsync_enabled(bool enabled)
{
    runtime_session_set_vsync(enabled);
    update_status_window(runtime_session_now_seconds(), true);
}

static bool handle_keydown(HWND hwnd, WPARAM key)
{
    (void)hwnd;

    if (key == VK_ESCAPE) {
        close_application();
        return true;
    }

    if (key == 'V') {
        set_vsync_enabled(!runtime_session_get_vsync());
        return true;
    }

    if (key == 'R') {
        const shader_desc_t *shader = active_shader();

        if (shader != NULL) {
            runtime_session_reset_for_shader(shader);
            update_status_window(runtime_session_now_seconds(), true);
        }
        return true;
    }

    return false;
}

static bool shared_on_keydown(HWND hwnd, WPARAM key, void *user_data)
{
    (void)user_data;
    return handle_keydown(hwnd, key);
}

static void stats_on_select_shader(int index, void *user_data)
{
    (void)user_data;

    if (g_shader_host.select_shader != NULL) {
        g_shader_host.select_shader(index);
        stats_set_selected_shader(selected_shader_index());
        update_status_window(runtime_session_now_seconds(), true);
    }
}

static void stop_active_shader(void)
{
    runtime_session_stop_shader();

    if (g_shader_host.stop_active_shader != NULL) {
        g_shader_host.stop_active_shader();
    }

    update_status_window(runtime_session_now_seconds(), true);
}

static void stats_on_execute_shader(void *user_data)
{
    const shader_desc_t *shader = selected_shader();
    runtime_display_params_t display_params;
    display_callbacks_t display_callbacks;
    display_layout_t layout = {0};
    int render_width = 0;
    int render_height = 0;

    (void)user_data;

    if (active_shader() != NULL) {
        stop_active_shader();
        return;
    }

    if (shader == NULL) {
        MessageBoxA(status_parent_window(), "No shader selected.", g_title, MB_OK | MB_ICONERROR);
        return;
    }

    if (!runtime_session_prepare_shader_buffers(shader)) {
        MessageBoxA(status_parent_window(), runtime_session_error(), g_title, MB_OK | MB_ICONERROR);
        return;
    }

    if (g_shader_host.execute_selected_shader == NULL || !g_shader_host.execute_selected_shader()) {
        runtime_session_stop_shader();
        MessageBoxA(status_parent_window(), "Failed to activate selected shader.", g_title, MB_OK | MB_ICONERROR);
        return;
    }

    runtime_session_target_dimensions(shader, &render_width, &render_height);
    compute_display_layout(render_width, render_height, &layout);

    ZeroMemory(&display_callbacks, sizeof(display_callbacks));
    display_callbacks.on_close = shared_on_close;

    ZeroMemory(&display_params, sizeof(display_params));
    display_params.instance = GetModuleHandleA(NULL);
    display_params.title = g_title;
    display_params.owner = stats_window();
    display_params.x = layout.x;
    display_params.y = layout.y;
    display_params.width = layout.width;
    display_params.height = layout.height;
    display_params.callbacks = &display_callbacks;

    if (!runtime_session_ensure_for_shader(shader, &display_params)) {
        runtime_session_stop_shader();
        if (g_shader_host.stop_active_shader != NULL) {
            g_shader_host.stop_active_shader();
        }
        MessageBoxA(status_parent_window(), runtime_session_error(), g_title, MB_OK | MB_ICONERROR);
        update_status_window(runtime_session_now_seconds(), true);
        return;
    }

    position_windows();
    update_status_window(runtime_session_now_seconds(), true);
}

static void stats_on_reset(void *user_data)
{
    const shader_desc_t *shader = active_shader();

    (void)user_data;

    if (shader != NULL) {
        runtime_session_reset_for_shader(shader);
        update_status_window(runtime_session_now_seconds(), true);
    }
}

static void stats_on_set_vsync(bool enabled, void *user_data)
{
    (void)user_data;
    set_vsync_enabled(enabled);
}

static void shared_on_close(HWND hwnd, void *user_data)
{
    (void)hwnd;
    (void)user_data;
    close_application();
}

bool window_create(const char *title, int width, int height)
{
    HINSTANCE instance = GetModuleHandleA(NULL);
    stats_callbacks_t stats_callbacks;

    g_title = title;
    g_is_closing = false;
    g_stats_positioned = false;
    runtime_session_init(title, width, height);

    ZeroMemory(&stats_callbacks, sizeof(stats_callbacks));
    stats_callbacks.on_keydown = shared_on_keydown;
    stats_callbacks.on_select_shader = stats_on_select_shader;
    stats_callbacks.on_execute_shader = stats_on_execute_shader;
    stats_callbacks.on_reset = stats_on_reset;
    stats_callbacks.on_set_vsync = stats_on_set_vsync;
    stats_callbacks.on_close = shared_on_close;

    if (!stats_create(instance, title, &stats_callbacks, NULL)) {
        MessageBoxA(NULL, "CreateDialogParamA(status) failed.", title, MB_OK | MB_ICONERROR);
        return false;
    }

    stats_show();
    position_windows();
    g_is_open = 1;
    update_status_window(0.0, true);
    return true;
}

void window_set_shader_host(const shader_host_callbacks_t *callbacks)
{
    const shader_desc_t *catalog = NULL;
    int count = 0;

    if (callbacks != NULL) {
        g_shader_host = *callbacks;
    } else {
        ZeroMemory(&g_shader_host, sizeof(g_shader_host));
    }

    if (g_shader_host.get_catalog != NULL) {
        catalog = g_shader_host.get_catalog(&count);
    }

    stats_set_shader_catalog(catalog, count);
    stats_set_selected_shader(selected_shader_index());
    update_status_window(0.0, true);
}

void window_run(int num_threads)
{
    if (!runtime_session_start_workers(num_threads)) {
        MessageBoxA(status_parent_window(), runtime_session_error(), g_title, MB_OK | MB_ICONERROR);
        goto cleanup;
    }

    update_status_window(0.0, true);

    while (g_is_open) {
        MSG msg;
        bool had_message = false;

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            had_message = true;
            runtime_session_note_key_message(&msg);

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

        if (!runtime_session_should_render_frame(active_shader())) {
            if (!had_message) {
                WaitMessage();
            }
            continue;
        }

        if (!runtime_session_render_frame(active_shader())) {
            MessageBoxA(status_parent_window(), runtime_session_error(), g_title, MB_OK | MB_ICONERROR);
            close_application();
            break;
        }

        update_status_window(runtime_session_now_seconds(), false);
    }

cleanup:
    runtime_session_shutdown();
    stats_destroy();
    g_is_closing = false;
}
