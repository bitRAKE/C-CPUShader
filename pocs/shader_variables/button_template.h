#pragma once

#include "shader_defines.h"
#include "shader_variables.h"

typedef struct {
    float press_depth;
    vec3_t diffuse_color;
} button_template_vars_t;

enum {
    BUTTON_TEMPLATE_VARIABLE_COUNT = 2
};

extern const shader_variable_desc_t g_button_template_variables[BUTTON_TEMPLATE_VARIABLE_COUNT];

vec4_t button_template_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define BUTTON_TEMPLATE_SHADER(X) X( \
    button_template, "Button Template", button_template_main, \
    SHADER_BUFFER_NONE, \
    g_button_template_variables, BUTTON_TEMPLATE_VARIABLE_COUNT, sizeof(button_template_vars_t), \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    384, 192, \
    "Transparent beveled button in a recessed holder, driven by press_depth and diffuse_color." \
)
