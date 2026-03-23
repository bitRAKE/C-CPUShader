#pragma once

#include "display.h"
#include "shader_catalog.h"
#include "present/present_backend.h"

typedef struct {
    HINSTANCE                instance;
    const char              *title;
    HWND                     owner;
    int                      x;
    int                      y;
    int                      width;
    int                      height;
    bool                     show_window;
    bool                     transparent_display;
    const display_callbacks_t *callbacks;
    void                    *user_data;
} runtime_display_params_t;

void  runtime_session_init(
    const char *title,
    int default_width,
    int default_height,
    present_backend_kind_t backend_kind,
    bool transparent_display,
    bool headless_mode,
    const shader_variable_override_t *variable_overrides,
    int variable_override_count);
void  runtime_session_shutdown(void);
bool  runtime_session_start_workers(int num_threads);
void  runtime_session_request_frame(void);
bool  runtime_session_request_capture(const char *shader_id, int frame_count);
bool  runtime_session_capture_requested(void);
bool  runtime_session_capture_in_progress(void);
void  runtime_session_reset_key_state(void);
void  runtime_session_note_key_message(const MSG *msg);
bool  runtime_session_prepare_shader_contract(const shader_desc_t *shader);
void  runtime_session_stop_shader(void);
bool  runtime_session_ensure_for_shader(const shader_desc_t *shader, const runtime_display_params_t *display_params);
void  runtime_session_reset_for_shader(const shader_desc_t *shader);
void  runtime_session_position_display(int x, int y);
bool  runtime_session_resize_display(const shader_desc_t *shader, const runtime_display_params_t *display_params);
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
const char *runtime_session_get_backend_name(void);
const char *runtime_session_get_backend_notice(void);
uint  runtime_session_get_backend_notice_version(void);
const char *runtime_session_get_notice(void);
uint  runtime_session_get_notice_version(void);
