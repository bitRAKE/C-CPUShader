//Own runtime/session state: workers, buffers, timing, backend, and frame execution

#define COBJMACROS

#include "runtime_session.h"
#include "frame_timing.h"
#include "capture_manager.h"
#include "render_workers.h"
#include "shader_host_services.h"

#include "present/present_backend.h"

#include <string.h>

static int                      g_default_width = 0;
static int                      g_default_height = 0;
static int                      g_width = 0;
static int                      g_height = 0;
static int                      g_popup_width = 0;
static int                      g_popup_height = 0;
static const char              *g_title = NULL;
static bool                     g_vsync_enabled = true;
static bool                     g_transparent_display = false;
static bool                     g_headless_mode = false;
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
static void                    *g_shader_variables = NULL;
static const shader_variable_override_t *g_shader_variable_overrides = NULL;
static int                      g_shader_variable_override_count = 0;
static present_backend_kind_t   g_present_backend_kind = PRESENT_BACKEND_DX12;
static shader_color_space_t     g_active_shader_color_space = SHADER_COLOR_SPACE_SDR_DISPLAY;

static char                     g_runtime_error[512] = "";

static shader_keys_t            g_key_state = {{0}};
static uint                     g_key_generation = 1;
static uint                     g_last_key_generation = 0;
static uint                     g_last_mouse_generation = 0;
static char                     g_runtime_notice[512] = "";
static volatile LONG            g_runtime_notice_version = 0;

static void runtime_release_shader_buffers(void);
static void runtime_release_shader_variables(void);
static void runtime_set_error_text(const char *text);
static void runtime_set_error_hr(const char *what);
static void runtime_set_notice_text(const char *text);
static void timing_init(void);
static void refresh_shader_quality(const shader_desc_t *shader);
static void update_key_state(uint virtual_key, bool is_down);
static void destroy_runtime_backend(void);
static bool ensure_frame_buffer(void);
static bool ensure_history_buffer(void);
static bool create_display_backend(const runtime_display_params_t *display_params, const shader_desc_t *shader, int render_width, int render_height);
static bool prepare_shader_buffers(const shader_desc_t *shader, shader_buffers_t *buffers_out, ShaderBuffersCleanupFunc *cleanup_out, char *error_text, size_t error_text_size);
static bool validate_shader_variables(const shader_desc_t *shader, char *error_text, size_t error_text_size);
static const shader_variable_desc_t *find_shader_variable(const shader_desc_t *shader, const char *name);
static bool prepare_shader_variables(const shader_desc_t *shader, void **variables_out, size_t *variable_size_out, char *error_text, size_t error_text_size);

static void runtime_release_shader_buffers(void)
{
    if (g_shader_buffers_cleanup != NULL) {
        g_shader_buffers_cleanup(&g_shader_buffers);
    } else {
        shader_buffers_reset(&g_shader_buffers);
    }

    g_shader_buffers_cleanup = NULL;
}

static void runtime_release_shader_variables(void)
{
    free(g_shader_variables);
    g_shader_variables = NULL;
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

    if (g_frame_pixels == NULL || g_width <= 0 || g_height <= 0) {
        runtime_set_notice_text("Capture requested with no active render surface.");
        return false;
    }

    if (!capture_manager_request(shader_id, frame_count)) {
        return false;
    }

    runtime_session_request_frame();
    return true;
}

bool runtime_session_capture_requested(void)
{
    return capture_manager_requested();
}

bool runtime_session_capture_in_progress(void)
{
    return capture_manager_in_progress();
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

void runtime_session_init(
    const char *title,
    int default_width,
    int default_height,
    present_backend_kind_t backend_kind,
    bool transparent_display,
    bool headless_mode,
    const shader_variable_override_t *variable_overrides,
    int variable_override_count)
{
    g_title = title;
    g_default_width = default_width;
    g_default_height = default_height;
    g_present_backend_kind = backend_kind;
    g_transparent_display = transparent_display;
    g_headless_mode = headless_mode;
    g_shader_variable_overrides = variable_overrides;
    g_shader_variable_override_count = max(0, variable_override_count);
    g_vsync_enabled = true;
    g_runtime_error[0] = '\0';
    shader_buffers_reset(&g_shader_buffers);
    g_shader_buffers_cleanup = NULL;
    g_shader_variables = NULL;
    timing_init();
    frame_timing_reset();
    refresh_shader_quality(NULL);
    runtime_session_reset_key_state();
    capture_manager_init();
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
    runtime_release_shader_variables();
    g_has_history = false;
    g_force_frame = false;
    g_frame_counter = 0;
    g_runtime_error[0] = '\0';
    capture_manager_cancel();
    g_active_shader_color_space = SHADER_COLOR_SPACE_SDR_DISPLAY;
    frame_timing_reset();
    refresh_shader_quality(NULL);
}

bool runtime_session_start_workers(int num_threads)
{
    return render_workers_create(num_threads);
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

static const shader_host_services_t g_host_services = {
    .alloc_bytes           = shader_buffers_alloc_bytes,
    .load_texture_file     = shader_buffers_load_texture_file,
    .load_texture_relative = shader_buffers_load_texture_module_relative,
    .load_texture_resource = shader_buffers_load_texture_resource,
    .default_cleanup       = shader_buffers_default_cleanup,
};

static bool prepare_shader_buffers(const shader_desc_t *shader, shader_buffers_t *buffers_out, ShaderBuffersCleanupFunc *cleanup_out, char *error_text, size_t error_text_size)
{
    if (buffers_out == NULL || cleanup_out == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Shader buffer output is invalid.");
        }
        return false;
    }

    if (shader == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "No shader selected.");
        }
        return false;
    }

    shader_buffers_reset(buffers_out);
    *cleanup_out = NULL;

    if (shader->buffers_init != NULL) {
        *cleanup_out = shader->buffers_init(buffers_out, &g_host_services, error_text, error_text_size);
        if (*cleanup_out == NULL) {
            shader_buffers_default_cleanup(buffers_out);
            if (error_text != NULL && error_text_size > 0 && error_text[0] == '\0') {
                snprintf(error_text, error_text_size, "Shader buffer initialization failed.");
            }
            return false;
        }
    }

    return true;
}

static bool validate_shader_variables(const shader_desc_t *shader, char *error_text, size_t error_text_size)
{
    if (shader == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "No shader selected.");
        }
        return false;
    }

    if (shader->variable_count < 0) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Shader '%s' has an invalid variable count.", shader->id);
        }
        return false;
    }

    if (shader->variable_count == 0) {
        if (shader->variables != NULL || shader->variable_struct_size != 0) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' declares no variables but left non-null variable metadata.", shader->id);
            }
            return false;
        }

        return true;
    }

    if (shader->variables == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Shader '%s' has variable_count > 0 but no variable metadata.", shader->id);
        }
        return false;
    }

    if (shader->variable_struct_size == 0) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Shader '%s' has variables but a zero-sized variable block.", shader->id);
        }
        return false;
    }

    for (int index = 0; index < shader->variable_count; index++) {
        const shader_variable_desc_t *desc = &shader->variables[index];
        size_t type_size;

        if (desc->name == NULL || desc->name[0] == '\0') {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' has a variable with no name.", shader->id);
            }
            return false;
        }

        if (desc->default_value == NULL) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' variable '%s' is missing a default value.", shader->id, desc->name);
            }
            return false;
        }

        type_size = shader_variable_type_size(desc->type);
        if (type_size == 0) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' variable '%s' uses an unknown type.", shader->id, desc->name);
            }
            return false;
        }

        if (desc->offset > shader->variable_struct_size || type_size > shader->variable_struct_size - desc->offset) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' variable '%s' does not fit in its declared variable block.", shader->id, desc->name);
            }
            return false;
        }

        for (int duplicate_index = index + 1; duplicate_index < shader->variable_count; duplicate_index++) {
            const char *other_name = shader->variables[duplicate_index].name;

            if (other_name != NULL && strcmp(desc->name, other_name) == 0) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Shader '%s' repeats variable name '%s'.", shader->id, desc->name);
                }
                return false;
            }
        }
    }

    return true;
}

static const shader_variable_desc_t *find_shader_variable(const shader_desc_t *shader, const char *name)
{
    if (shader == NULL || shader->variables == NULL || name == NULL) {
        return NULL;
    }

    for (int index = 0; index < shader->variable_count; index++) {
        if (strcmp(shader->variables[index].name, name) == 0) {
            return &shader->variables[index];
        }
    }

    return NULL;
}

static bool prepare_shader_variables(const shader_desc_t *shader, void **variables_out, size_t *variable_size_out, char *error_text, size_t error_text_size)
{
    char parse_error[128] = "";
    unsigned char *variable_bytes = NULL;

    if (variables_out == NULL || variable_size_out == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Shader variable output is invalid.");
        }
        return false;
    }

    *variables_out = NULL;
    *variable_size_out = 0;

    if (!validate_shader_variables(shader, error_text, error_text_size)) {
        return false;
    }

    if (shader->variable_count == 0) {
        if (g_shader_variable_override_count > 0) {
            const shader_variable_override_t *override = &g_shader_variable_overrides[0];
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' does not define variable '%s'.", shader->id, override->name);
            }
            return false;
        }

        return true;
    }

    variable_bytes = (unsigned char *)calloc(1, shader->variable_struct_size);
    if (variable_bytes == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to allocate variable block for shader '%s'.", shader->id);
        }
        return false;
    }

    for (int index = 0; index < shader->variable_count; index++) {
        const shader_variable_desc_t *desc = &shader->variables[index];

        if (!shader_variable_parse_value(desc->type, desc->default_value, variable_bytes + desc->offset, parse_error, sizeof(parse_error))) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' variable '%s' has an invalid default '%s': %s", shader->id, desc->name, desc->default_value, parse_error);
            }
            free(variable_bytes);
            return false;
        }
    }

    for (int index = 0; index < g_shader_variable_override_count; index++) {
        const shader_variable_override_t *override = &g_shader_variable_overrides[index];
        const shader_variable_desc_t *desc = find_shader_variable(shader, override->name);

        if (desc == NULL) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' does not define variable '%s'.", shader->id, override->name);
            }
            free(variable_bytes);
            return false;
        }

        if (!shader_variable_parse_value(desc->type, override->value, variable_bytes + desc->offset, parse_error, sizeof(parse_error))) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Shader '%s' variable '%s' could not parse override '%s': %s", shader->id, override->name, override->value, parse_error);
            }
            free(variable_bytes);
            return false;
        }
    }

    *variables_out = variable_bytes;
    *variable_size_out = shader->variable_struct_size;
    return true;
}

bool runtime_session_prepare_shader_contract(const shader_desc_t *shader)
{
    shader_buffers_t next_buffers;
    ShaderBuffersCleanupFunc next_cleanup = NULL;
    void *next_variables = NULL;
    size_t next_variable_size = 0;
    char error_text[512] = "";

    shader_buffers_reset(&next_buffers);

    if (!prepare_shader_buffers(shader, &next_buffers, &next_cleanup, error_text, sizeof(error_text))) {
        runtime_set_error_text(error_text);
        return false;
    }

    if (!prepare_shader_variables(shader, &next_variables, &next_variable_size, error_text, sizeof(error_text))) {
        if (next_cleanup != NULL) {
            next_cleanup(&next_buffers);
        } else {
            shader_buffers_reset(&next_buffers);
        }
        runtime_set_error_text(error_text);
        return false;
    }

    runtime_release_shader_buffers();
    runtime_release_shader_variables();
    g_shader_buffers = next_buffers;
    g_shader_buffers_cleanup = next_cleanup;
    g_shader_variables = next_variables;
    (void)next_variable_size;
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

    if (!display_create(instance, title, owner, x, y, width, height, display_params->transparent_display, callbacks, user_data)) {
        runtime_set_error_text("CreateWindowExA(display) failed.");
        return false;
    }

    ZeroMemory(&backend_desc, sizeof(backend_desc));
    backend_desc.hwnd = display_window();
    backend_desc.render_width = render_width;
    backend_desc.render_height = render_height;
    backend_desc.popup_width = width;
    backend_desc.popup_height = height;
    backend_desc.transparent_display = display_params->transparent_display;
    backend_desc.shader_color_space = (shader != NULL) ? shader->generated_color_space : SHADER_COLOR_SPACE_SDR_DISPLAY;
    backend_desc.vsync_enabled = g_vsync_enabled;

    if (!present_backend_create(g_present_backend_kind, &backend_desc)) {
        runtime_set_error_text(present_backend_error());
        display_destroy();
        return false;
    }

    if (display_params->show_window) {
        display_show();
    }
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

    if (!g_headless_mode && (display_params == NULL || display_params->width <= 0 || display_params->height <= 0)) {
        runtime_set_error_text("Display layout is invalid.");
        return false;
    }

    if (g_headless_mode) {
        if (g_width != width || g_height != height || g_frame_pixels == NULL) {
            destroy_runtime_backend();
            g_width = width;
            g_height = height;
            g_popup_width = 0;
            g_popup_height = 0;

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

            render_workers_setup_rows(g_height);
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

        render_workers_setup_rows(g_height);
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
    backend_desc.transparent_display = g_transparent_display;
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
    backend_desc.transparent_display = g_transparent_display;

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

    if (shader == NULL) {
        return false;
    }

    if (!g_headless_mode && !present_backend_is_ready()) {
        return false;
    }

    if (g_frame_pixels == NULL || g_width <= 0 || g_height <= 0) {
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
    render_frame_context_t context;
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
    uniforms.variables = g_shader_variables;

    context.frame_pixels = g_frame_pixels;
    context.history_pixels = g_history_pixels;
    context.width = g_width;
    context.temporal_accumulation = g_temporal_accumulation;
    context.has_history = g_has_history;

    /* Ensure worker row ranges match the current height.  This is
       idempotent and resolves ordering issues when ensure_for_shader
       sets up the height before render_workers_create runs. */
    render_workers_setup_rows(g_height);
    render_workers_dispatch(&uniforms, shader->render, &context);

    if (g_temporal_accumulation && g_history_pixels != NULL) {
        g_has_history = true;
    }

    surface.width = g_width;
    surface.height = g_height;
    surface.stride_bytes = (int)((size_t)g_width * sizeof(vec4_t));
    surface.pixels = g_frame_pixels;

    if (!g_headless_mode) {
        if (!present_backend_present(&surface)) {
            runtime_set_error_text(present_backend_error());
            return false;
        }
    }

    g_force_frame = false;
    g_last_mouse_generation = display_input_generation();
    g_last_key_generation = g_key_generation;
    g_frame_counter++;
    capture_manager_process_frame(g_frame_pixels, g_width, g_height, g_active_shader_color_space, runtime_session_request_frame);

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
        *sample_count_out = frame_timing_sample_count();
    }
}

void runtime_session_get_worker_counts(int *total_workers_out, int *background_workers_out)
{
    render_workers_get_counts(total_workers_out, background_workers_out);
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
    uint capture_ver = capture_manager_notice_version();
    uint runtime_ver = (uint)g_runtime_notice_version;

    if (capture_ver > runtime_ver) {
        return capture_manager_notice();
    }

    return g_runtime_notice;
}

uint runtime_session_get_notice_version(void)
{
    uint capture_ver = capture_manager_notice_version();
    uint runtime_ver = (uint)g_runtime_notice_version;

    return (capture_ver > runtime_ver) ? capture_ver : runtime_ver;
}

void runtime_session_shutdown(void)
{
    render_workers_destroy();
    runtime_session_stop_shader();
    capture_manager_shutdown();
}
