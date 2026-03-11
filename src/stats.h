#pragma once

#include "defines.h"

typedef struct {
    const char *shader_name;
    bool        temporal_accumulation;
    double      fps;
    double      milliseconds;
    int         frame_sample_count;
    double      time_seconds;
    int         total_workers;
    int         background_workers;
    int         display_width;
    int         display_height;
    bool        vsync_enabled;
} stats_state_t;

typedef struct {
    bool (*on_keydown)(HWND hwnd, WPARAM key, void *user_data);
    void (*on_cycle_shader)(int direction, void *user_data);
    void (*on_reset)(void *user_data);
    void (*on_set_vsync)(bool enabled, void *user_data);
    void (*on_close)(HWND hwnd, void *user_data);
} stats_callbacks_t;

bool stats_create(HINSTANCE instance, const char *title, const stats_callbacks_t *callbacks, void *user_data);
void stats_destroy(void);
void stats_show(void);
void stats_update(const stats_state_t *state);
bool stats_is_dialog_message(MSG *msg);
bool stats_get_window_rect(RECT *rect_out);
bool stats_is_window(HWND hwnd);
HWND stats_window(void);
