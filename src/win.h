#pragma once

#include "shader_catalog.h"

typedef struct {
    const shader_desc_t *(*get_catalog)(int *count_out);
    int                  (*get_selected_index)(void);
    const shader_desc_t *(*get_selected_shader)(void);
    const shader_desc_t *(*get_active_shader)(void);
    void                 (*select_shader)(int index);
    bool                 (*execute_selected_shader)(void);
    void                 (*stop_active_shader)(void);
} shader_host_callbacks_t;

bool  window_create(const char *title, int width, int height);
void  window_set_shader_host(const shader_host_callbacks_t *callbacks);
void  window_run(int num_threads);
