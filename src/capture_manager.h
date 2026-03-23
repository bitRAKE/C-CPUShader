#pragma once

#include "defines.h"

void  capture_manager_init(void);
void  capture_manager_shutdown(void);
bool  capture_manager_request(const char *shader_id, int frame_count);
bool  capture_manager_requested(void);
bool  capture_manager_in_progress(void);
void  capture_manager_process_frame(
    const vec4_t *frame_pixels,
    int width, int height,
    shader_color_space_t color_space,
    void (*request_frame_callback)(void));
void  capture_manager_cancel(void);
void  capture_manager_wait_idle(void);

/* Notice access -- capture thread posts notices here */
const char *capture_manager_notice(void);
uint  capture_manager_notice_version(void);
