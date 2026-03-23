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
static uint                    g_last_backend_notice_version = 0;
static uint                    g_last_runtime_notice_version = 0;
static int                     g_display_multiplier = 1;
static int                     g_startup_capture_count = 0;
static bool                    g_show_status_window = true;
static bool                    g_show_display_window = true;
static bool                    g_show_message_boxes = true;
static bool                    g_transparent_display = false;
static bool                    g_script_mode = false;
static int                     g_exit_code = 0;
static char                    g_startup_notice[512] = "";

static COLORREF diagnostic_color_for_text(const char *text)
{
    if (text != NULL) {
        if (strstr(text, "failed") != NULL || strstr(text, "Failed") != NULL || strstr(text, "error") != NULL || strstr(text, "Error") != NULL) {
            return RGB(156, 58, 58);
        }

        if (strstr(text, "Falling back to GDI") != NULL || strstr(text, "GDI fallback") != NULL) {
            return RGB(156, 58, 58);
        }

        if (strstr(text, "HDR is not present to the surface") != NULL || strstr(text, " SDR") != NULL || strstr(text, "SDR ") != NULL) {
            return RGB(146, 110, 28);
        }
    }

    return RGB(78, 106, 156);
}

static void update_status_window(double frame_end_seconds, bool force);
static bool shared_on_keydown(HWND hwnd, WPARAM key, void *user_data);
static void shared_on_close(HWND hwnd, void *user_data);
static void compute_display_layout(int render_width, int render_height, display_layout_t *layout_out);
static void stats_on_capture(int capture_count, void *user_data);
static HWND status_parent_window(void);
static bool execute_selected_shader_internal(void);

static void push_runtime_diagnostic(const char *text)
{
    if (text != NULL && text[0] != '\0') {
        stats_prepend_message(text, diagnostic_color_for_text(text));
    }
}

static void write_console_line(const char *text)
{
    static bool attached_parent_console = false;
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written = 0;

    if ((handle == NULL || handle == INVALID_HANDLE_VALUE) && !attached_parent_console) {
        attached_parent_console = AttachConsole(ATTACH_PARENT_PROCESS) ? true : false;
        handle = GetStdHandle(STD_ERROR_HANDLE);
    }

    if (handle == NULL || handle == INVALID_HANDLE_VALUE || text == NULL || text[0] == '\0') {
        return;
    }

    WriteFile(handle, text, (DWORD)strlen(text), &written, NULL);
    WriteFile(handle, "\r\n", 2, &written, NULL);
}

static void report_host_error(const char *text)
{
    push_runtime_diagnostic(text);
    write_console_line(text);

    if (g_show_message_boxes && text != NULL && text[0] != '\0') {
        MessageBoxA(status_parent_window(), text, g_title, MB_OK | MB_ICONERROR);
    }
}

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
    if (g_show_status_window && stats_window() != NULL) {
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

static const char *shader_color_space_name(shader_color_space_t color_space)
{
    switch (color_space) {
        case SHADER_COLOR_SPACE_SDR_DISPLAY:
            return "SDR display";

        case SHADER_COLOR_SPACE_SCENE_LINEAR:
            return "scene linear";

        case SHADER_COLOR_SPACE_HDR10_ST2084:
            return "HDR10 (ST.2084)";
    }

    return "unknown";
}

static void format_expectations_text(const shader_desc_t *shader, char *buffer, size_t buffer_size)
{
    char features[96];

    if (shader == NULL) {
        snprintf(buffer, buffer_size, "Select a shader.");
        return;
    }

    format_feature_list(shader->feature_flags, features, sizeof(features));
    snprintf(buffer, buffer_size, "color: %s | features: %s", shader_color_space_name(shader->generated_color_space), features);
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

    popup_width = max(1, render_width * max(1, g_display_multiplier));
    popup_height = max(1, render_height * max(1, g_display_multiplier));

    if (popup_width <= avail_width && popup_height <= avail_height) {
        *popup_width_out = popup_width;
        *popup_height_out = popup_height;
        return;
    }

    if ((LONGLONG)avail_width * (LONGLONG)popup_height <= (LONGLONG)avail_height * (LONGLONG)popup_width) {
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
    int slot_count = 0;
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
    if (g_show_status_window && !g_stats_positioned && stats_window() != NULL) {
        SetWindowPos(stats_window(), NULL, stats_x, stats_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        g_stats_positioned = true;
    }

    if (g_show_status_window && stats_get_window_rect(&stats_rect)) {
        slot_x[slot_count] = stats_rect.right + WINDOW_GAP;
        slot_y[slot_count] = stats_rect.top;
        slot_width[slot_count] = work_area.right - slot_x[slot_count];
        slot_height[slot_count] = work_area.bottom - slot_y[slot_count];
        slot_count++;

        slot_x[slot_count] = stats_rect.left;
        slot_y[slot_count] = stats_rect.bottom + WINDOW_GAP;
        slot_width[slot_count] = work_area.right - slot_x[slot_count];
        slot_height[slot_count] = work_area.bottom - slot_y[slot_count];
        slot_count++;
    }

    slot_x[slot_count] = work_area.left + WINDOW_MARGIN;
    slot_y[slot_count] = work_area.top + WINDOW_MARGIN;
    slot_width[slot_count] = (work_area.right - work_area.left) - WINDOW_MARGIN * 2;
    slot_height[slot_count] = (work_area.bottom - work_area.top) - WINDOW_MARGIN * 2;
    slot_count++;

    for (int i = 0; i < slot_count; i++) {
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

    stats_set_backend_name(runtime_session_get_backend_name());
    if (runtime_session_get_backend_notice_version() != g_last_backend_notice_version) {
        g_last_backend_notice_version = runtime_session_get_backend_notice_version();
        push_runtime_diagnostic(runtime_session_get_backend_notice());
    }
    if (runtime_session_get_notice_version() != g_last_runtime_notice_version) {
        g_last_runtime_notice_version = runtime_session_get_notice_version();
        push_runtime_diagnostic(runtime_session_get_notice());
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
    state.can_capture = (active != NULL || selected != NULL);
    state.can_reset = (active != NULL);
    state.shader_running = (active != NULL);
    state.display_multiplier = g_display_multiplier;

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

static void adjust_display_multiplier(int delta)
{
    const shader_desc_t *shader = active_shader();
    runtime_display_params_t display_params;
    display_callbacks_t display_callbacks;
    display_layout_t layout = {0};
    int old_multiplier = g_display_multiplier;
    int new_multiplier = max(1, g_display_multiplier + delta);
    int render_width = 0;
    int render_height = 0;

    if (new_multiplier == g_display_multiplier) {
        return;
    }

    g_display_multiplier = new_multiplier;
    if (shader == NULL) {
        update_status_window(runtime_session_now_seconds(), true);
        return;
    }

    runtime_session_target_dimensions(shader, &render_width, &render_height);
    compute_display_layout(render_width, render_height, &layout);

    ZeroMemory(&display_callbacks, sizeof(display_callbacks));
    display_callbacks.on_close = shared_on_close;

    ZeroMemory(&display_params, sizeof(display_params));
    display_params.instance = GetModuleHandleA(NULL);
    display_params.title = g_title;
    display_params.owner = g_show_status_window ? stats_window() : NULL;
    display_params.x = layout.x;
    display_params.y = layout.y;
    display_params.width = layout.width;
    display_params.height = layout.height;
    display_params.show_window = g_show_display_window;
    display_params.transparent_display = g_transparent_display;
    display_params.callbacks = &display_callbacks;

    if (!runtime_session_resize_display(shader, &display_params)) {
        g_display_multiplier = old_multiplier;
        push_runtime_diagnostic(runtime_session_error());
    }

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

    if (key == VK_PRIOR) {
        adjust_display_multiplier(+1);
        return true;
    }

    if (key == VK_NEXT) {
        adjust_display_multiplier(-1);
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
    g_startup_capture_count = 0;

    if (g_shader_host.stop_active_shader != NULL) {
        g_shader_host.stop_active_shader();
    }

    update_status_window(runtime_session_now_seconds(), true);
}

static void stats_on_execute_shader(void *user_data)
{
    (void)user_data;
    execute_selected_shader_internal();
}

static bool execute_selected_shader_internal(void)
{
    const shader_desc_t *shader = selected_shader();
    runtime_display_params_t display_params;
    display_callbacks_t display_callbacks;
    display_layout_t layout = {0};
    int render_width = 0;
    int render_height = 0;

    if (active_shader() != NULL) {
        stop_active_shader();
        return true;
    }

    if (shader == NULL) {
        report_host_error("No shader selected.");
        return false;
    }

    if (!runtime_session_prepare_shader_contract(shader)) {
        report_host_error(runtime_session_error());
        return false;
    }

    if (g_shader_host.execute_selected_shader == NULL || !g_shader_host.execute_selected_shader()) {
        runtime_session_stop_shader();
        report_host_error("Failed to activate selected shader.");
        return false;
    }

    runtime_session_target_dimensions(shader, &render_width, &render_height);
    compute_display_layout(render_width, render_height, &layout);

    ZeroMemory(&display_callbacks, sizeof(display_callbacks));
    display_callbacks.on_close = shared_on_close;

    ZeroMemory(&display_params, sizeof(display_params));
    display_params.instance = GetModuleHandleA(NULL);
    display_params.title = g_title;
    display_params.owner = g_show_status_window ? stats_window() : NULL;
    display_params.x = layout.x;
    display_params.y = layout.y;
    display_params.width = layout.width;
    display_params.height = layout.height;
    display_params.show_window = g_show_display_window;
    display_params.transparent_display = g_transparent_display;
    display_params.callbacks = &display_callbacks;

    if (!runtime_session_ensure_for_shader(shader, &display_params)) {
        runtime_session_stop_shader();
        g_startup_capture_count = 0;
        if (g_shader_host.stop_active_shader != NULL) {
            g_shader_host.stop_active_shader();
        }
        report_host_error(runtime_session_error());
        update_status_window(runtime_session_now_seconds(), true);
        return false;
    }

    if (g_startup_capture_count > 0) {
        if (!runtime_session_request_capture(shader->id, g_startup_capture_count)) {
            push_runtime_diagnostic(runtime_session_get_notice());
            if (g_script_mode) {
                stop_active_shader();
                report_host_error(runtime_session_get_notice());
                return false;
            }
        } else {
            char message[128];
            snprintf(
                message,
                sizeof(message),
                "Startup capture armed: saving %d sequential PNG frame%s from frame 0.",
                g_startup_capture_count,
                (g_startup_capture_count == 1) ? "" : "s");
            push_runtime_diagnostic(message);
        }
        g_startup_capture_count = 0;
    }

    position_windows();
    update_status_window(runtime_session_now_seconds(), true);
    return true;
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

static void stats_on_capture(int capture_count, void *user_data)
{
    const shader_desc_t *selected = selected_shader();
    const shader_desc_t *shader = active_shader();
    char message[128];

    (void)user_data;

    if (capture_count <= 0) {
        push_runtime_diagnostic("Capture count must be at least 1.");
        return;
    }

    if (shader != NULL) {
        if (!runtime_session_request_capture(shader->id, capture_count)) {
            push_runtime_diagnostic(runtime_session_get_notice());
        } else {
            snprintf(
                message,
                sizeof(message),
                "Capture armed: saving %d sequential %s frame%s for the active shader.",
                capture_count,
                (shader->generated_color_space == SHADER_COLOR_SPACE_SDR_DISPLAY) ? "PNG" : "EXR",
                (capture_count == 1) ? "" : "s");
            push_runtime_diagnostic(message);
        }
        return;
    }

    if (selected == NULL) {
        push_runtime_diagnostic("Select a shader before arming capture.");
        return;
    }

    g_startup_capture_count = capture_count;
    snprintf(
        message,
        sizeof(message),
        "Capture armed for next execute: %d sequential %s frame%s starting at frame 0.",
        capture_count,
        (selected->generated_color_space == SHADER_COLOR_SPACE_SDR_DISPLAY) ? "PNG" : "EXR",
        (capture_count == 1) ? "" : "s");
    push_runtime_diagnostic(message);
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

bool window_create(const char *title, int width, int height, const host_options_t *options)
{
    HINSTANCE instance = GetModuleHandleA(NULL);
    stats_callbacks_t stats_callbacks;
    present_backend_kind_t backend_kind = PRESENT_BACKEND_DX12;

    g_title = title;
    g_is_open = 0;
    g_is_closing = false;
    g_stats_positioned = false;
    g_last_backend_notice_version = 0;
    g_last_runtime_notice_version = 0;
    g_display_multiplier = 1;
    g_startup_capture_count = 0;
    g_show_status_window = true;
    g_show_display_window = true;
    g_show_message_boxes = true;
    g_transparent_display = false;
    g_script_mode = false;
    g_exit_code = 0;
    g_startup_notice[0] = '\0';

    if (options != NULL) {
        backend_kind = options->backend_kind;
        g_display_multiplier = max(1, options->display_multiplier);
        g_startup_capture_count = options->script_mode ? max(1, options->script_frame_count) : 0;
        g_show_status_window = !options->script_mode;
        g_show_display_window = !options->script_mode;
        g_transparent_display = options->transparent_display;
        g_script_mode = options->script_mode;
        g_show_message_boxes = !options->script_mode;
    }

    if (g_transparent_display && backend_kind != PRESENT_BACKEND_GDI) {
        snprintf(
            g_startup_notice,
            sizeof(g_startup_notice),
            "Transparent display currently uses layered GDI. Requested %s, switching to GDI for this run.",
            present_backend_name(backend_kind));
        backend_kind = PRESENT_BACKEND_GDI;
    }

    runtime_session_init(
        title,
        width,
        height,
        backend_kind,
        g_transparent_display,
        g_script_mode,
        (options != NULL) ? options->variable_overrides : NULL,
        (options != NULL) ? options->variable_override_count : 0);

    ZeroMemory(&stats_callbacks, sizeof(stats_callbacks));
    stats_callbacks.on_keydown = shared_on_keydown;
    stats_callbacks.on_select_shader = stats_on_select_shader;
    stats_callbacks.on_execute_shader = stats_on_execute_shader;
    stats_callbacks.on_capture = stats_on_capture;
    stats_callbacks.on_reset = stats_on_reset;
    stats_callbacks.on_set_vsync = stats_on_set_vsync;
    stats_callbacks.on_close = shared_on_close;

    if (g_show_status_window) {
        if (!stats_create(instance, title, &stats_callbacks, NULL)) {
            report_host_error("CreateDialogParamA(status) failed.");
            return false;
        }

        stats_clear_diagnostics();
    }
    if (g_startup_notice[0] != '\0') {
        push_runtime_diagnostic(g_startup_notice);
        write_console_line(g_startup_notice);
    }
    if (g_show_status_window) {
        stats_set_backend_name(runtime_session_get_backend_name());
    }
    if (g_show_status_window) {
        stats_show();
    }
    position_windows();
    g_is_open = 1;
    update_status_window(0.0, true);
    return true;
}

void window_set_shader_host(const shader_host_callbacks_t *callbacks)
{
    const shader_desc_t *catalog = NULL;
    const shader_collection_t *collections = NULL;
    int count = 0;
    int collection_count = 0;

    if (callbacks != NULL) {
        g_shader_host = *callbacks;
    } else {
        ZeroMemory(&g_shader_host, sizeof(g_shader_host));
    }

    if (g_shader_host.get_catalog != NULL) {
        catalog = g_shader_host.get_catalog(&count);
    }

    if (g_shader_host.get_collections != NULL && g_shader_host.get_shader_collection != NULL) {
        collections = g_shader_host.get_collections(&collection_count);
        if (collections != NULL && collection_count > 0) {
            stats_set_shader_catalog_grouped(
                catalog, count,
                collections, collection_count,
                g_shader_host.get_shader_collection);
        } else {
            stats_set_shader_catalog(catalog, count);
        }
    } else {
        stats_set_shader_catalog(catalog, count);
    }

    stats_set_selected_shader(selected_shader_index());
    update_status_window(0.0, true);
}

void window_push_startup_diagnostic(const char *text)
{
    push_runtime_diagnostic(text);
}

bool window_execute_selected_shader(void)
{
    return execute_selected_shader_internal();
}

int window_run(int num_threads)
{
    if (!runtime_session_start_workers(num_threads)) {
        g_exit_code = 1;
        report_host_error(runtime_session_error());
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

        if (g_script_mode && active_shader() != NULL && !runtime_session_capture_requested()) {
            if (!runtime_session_capture_in_progress()) {
                close_application();
                break;
            }

            Sleep(10);
            continue;
        }

        if (!runtime_session_should_render_frame(active_shader())) {
            if (!had_message) {
                WaitMessage();
            }
            continue;
        }

        if (!runtime_session_render_frame(active_shader())) {
            g_exit_code = 1;
            report_host_error(runtime_session_error());
            close_application();
            break;
        }

        update_status_window(runtime_session_now_seconds(), false);
    }

cleanup:
    runtime_session_shutdown();
    stats_destroy();
    g_is_closing = false;
    return g_exit_code;
}
