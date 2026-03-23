#pragma once

#include "defines.h"
#include "shader_catalog.h"

typedef struct {
    vec4_t *frame_pixels;
    vec4_t *history_pixels;
    int     width;
    bool    temporal_accumulation;
    bool    has_history;
} render_frame_context_t;

bool  render_workers_create(int num_threads);
void  render_workers_destroy(void);
void  render_workers_setup_rows(int height);
void  render_workers_dispatch(const shader_uniforms_t *uniforms, RenderFunc render, const render_frame_context_t *context);
void  render_workers_get_counts(int *total_out, int *background_out);
