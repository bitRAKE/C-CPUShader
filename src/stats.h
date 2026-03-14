#pragma once

#include "shader_catalog.h"

typedef struct {
    const char *shader_name;
    const char *selected_expectations;
    const char *selected_blurb;
    double      fps;
    double      milliseconds;
    int         frame_sample_count;
    double      time_seconds;
    int         total_workers;
    int         background_workers;
    int         render_width;
    int         render_height;
    int         popup_width;
    int         popup_height;
    int         display_multiplier;
    bool        vsync_enabled;
    bool        can_execute;
    bool        can_capture;
    bool        can_reset;
    bool        shader_running;
} stats_state_t;

typedef struct {
    bool (*on_keydown)(HWND hwnd, WPARAM key, void *user_data);
    void (*on_select_shader)(int index, void *user_data);
    void (*on_execute_shader)(void *user_data);
    void (*on_capture)(int capture_count, void *user_data);
    void (*on_reset)(void *user_data);
    void (*on_set_vsync)(bool enabled, void *user_data);
    void (*on_close)(HWND hwnd, void *user_data);
} stats_callbacks_t;

bool stats_create(HINSTANCE instance, const char *title, const stats_callbacks_t *callbacks, void *user_data);
void stats_destroy(void);
void stats_show(void);
void stats_set_backend_name(const char *backend_name);
void stats_prepend_message(const char *text, COLORREF color);
void stats_clear_diagnostics(void);
void stats_set_shader_catalog(const shader_desc_t *catalog, int count);
void stats_set_selected_shader(int index);
void stats_update(const stats_state_t *state);
bool stats_is_dialog_message(MSG *msg);
bool stats_get_window_rect(RECT *rect_out);
bool stats_is_window(HWND hwnd);
HWND stats_window(void);
