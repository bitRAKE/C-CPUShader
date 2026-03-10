#pragma once

typedef vec4_t (*RenderFunc)(vec2_t fragCoord, vec2_t resolution, float time, uint frame);
typedef void (*ShaderCycleFunc)(int direction);
typedef const char *(*ShaderNameFunc)(void);

bool  window_create   (const char *title, int width, int height);
void  window_set_shader_switcher(ShaderCycleFunc cycle_func, ShaderNameFunc name_func);
void  window_run      (RenderFunc render, int num_threads, bool temporal_accumulation);
