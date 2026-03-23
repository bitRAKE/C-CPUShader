#pragma once

#include "defines.h"

typedef struct {
    void (*on_close)(HWND hwnd, void *user_data);
} display_callbacks_t;

bool display_create(
    HINSTANCE instance,
    const char *title,
    HWND owner,
    int x,
    int y,
    int width,
    int height,
    bool transparent_display,
    const display_callbacks_t *callbacks,
    void *user_data);
void display_destroy(void);
void display_show(void);
void display_focus(void);
void display_get_mouse_uniform(vec4_t *mouse_out, int render_width, int render_height);
uint display_input_generation(void);
bool display_get_window_rect(RECT *rect_out);
bool display_is_window(HWND hwnd);
HWND display_window(void);
