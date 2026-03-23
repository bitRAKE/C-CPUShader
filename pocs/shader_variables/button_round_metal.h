#pragma once

#include "shader_defines.h"
#include "shader_variables.h"

typedef struct {
    vec3_t primary_color;
    float  depression;
} button_round_metal_vars_t;

enum {
    BUTTON_ROUND_METAL_VARIABLE_COUNT = 2
};

extern const shader_variable_desc_t g_button_round_metal_variables[BUTTON_ROUND_METAL_VARIABLE_COUNT];

vec4_t button_round_metal_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BUTTON_ROUND_METAL_SHADER(X) X( \
    button_round_metal, "Button Round Metal", button_round_metal_main, \
    SHADER_BUFFER_NONE, \
    g_button_round_metal_variables, BUTTON_ROUND_METAL_VARIABLE_COUNT, sizeof(button_round_metal_vars_t), \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    64, 64, \
    "Transparent round metal-rimmed gel button driven by primary_color and depression." \
)
