#pragma once

typedef vec4_t (*RenderFunc)(vec2_t fragCoord, const shader_uniforms_t *uniforms);
typedef void (*ShaderCycleFunc)(int direction);
typedef const char *(*ShaderNameFunc)(void);
typedef bool (*ShaderAccumulationFunc)(void);

bool  window_create   (const char *title, int width, int height);
void  window_set_shader_switcher(ShaderCycleFunc cycle_func, ShaderNameFunc name_func, ShaderAccumulationFunc accumulation_func);
void  window_run      (RenderFunc render, int num_threads);
