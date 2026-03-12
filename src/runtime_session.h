#pragma once

#include "display.h"
#include "shader_catalog.h"

typedef struct {
    HINSTANCE                instance;
    const char              *title;
    HWND                     owner;
    int                      x;
    int                      y;
    int                      width;
    int                      height;
    const display_callbacks_t *callbacks;
    void                    *user_data;
} runtime_display_params_t;

void  runtime_session_init(const char *title, int default_width, int default_height);
void  runtime_session_shutdown(void);
bool  runtime_session_start_workers(int num_threads);
void  runtime_session_request_frame(void);
void  runtime_session_reset_key_state(void);
void  runtime_session_note_key_message(const MSG *msg);
bool  runtime_session_prepare_shader_buffers(const shader_desc_t *shader);
void  runtime_session_stop_shader(void);
bool  runtime_session_ensure_for_shader(const shader_desc_t *shader, const runtime_display_params_t *display_params);
void  runtime_session_reset_for_shader(const shader_desc_t *shader);
void  runtime_session_position_display(int x, int y);
void  runtime_session_set_vsync(bool enabled);
bool  runtime_session_get_vsync(void);
bool  runtime_session_should_render_frame(const shader_desc_t *shader);
bool  runtime_session_render_frame(const shader_desc_t *shader);
const char *runtime_session_error(void);
double runtime_session_now_seconds(void);
bool  runtime_session_shader_uses_feature(const shader_desc_t *shader, uint feature_flag);
void  runtime_session_target_dimensions(const shader_desc_t *shader, int *width_out, int *height_out);
void  runtime_session_get_frame_stats(double *average_seconds_out, int *sample_count_out);
void  runtime_session_get_worker_counts(int *total_workers_out, int *background_workers_out);
void  runtime_session_get_render_size(int *width_out, int *height_out);
void  runtime_session_get_popup_size(int *width_out, int *height_out);
