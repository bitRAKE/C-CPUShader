#pragma once

#include "host/host_options.h"
#include "shader_catalog.h"
#include "shader_catalog_runtime.h"

typedef struct {
    const shader_desc_t       *(*get_catalog)(int *count_out);
    const shader_collection_t *(*get_collections)(int *count_out);
    int                        (*get_shader_collection)(int shader_index);
    int                        (*get_selected_index)(void);
    const shader_desc_t       *(*get_selected_shader)(void);
    const shader_desc_t       *(*get_active_shader)(void);
    void                       (*select_shader)(int index);
    bool                       (*execute_selected_shader)(void);
    void                       (*stop_active_shader)(void);
} shader_host_callbacks_t;

bool  window_create(const char *title, int width, int height, const host_options_t *options);
void  window_set_shader_host(const shader_host_callbacks_t *callbacks);
void  window_push_startup_diagnostic(const char *text);
bool  window_execute_selected_shader(void);
int   window_run(int num_threads);
