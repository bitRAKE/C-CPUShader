#pragma once

#include "shader_defines.h"
#include "shader_variables.h"

typedef struct {
    float  state;           /* 0.0 = off, 1.0 = on (animatable) */
    vec3_t active_color;    /* track color when on */
    vec3_t knob_color;      /* knob surface tint */
} toggle_switch_vars_t;

enum {
    TOGGLE_SWITCH_VARIABLE_COUNT = 3
};

extern const shader_variable_desc_t g_toggle_switch_variables[TOGGLE_SWITCH_VARIABLE_COUNT];

vec4_t toggle_switch_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define TOGGLE_SWITCH_SHADER(X) X( \
    toggle_switch, "Toggle Switch", toggle_switch_main, \
    SHADER_BUFFER_NONE, \
    g_toggle_switch_variables, TOGGLE_SWITCH_VARIABLE_COUNT, sizeof(toggle_switch_vars_t), \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    160, 80, \
    "iOS-style toggle switch. Animate <code>state</code> from 0&ndash;1 to " \
    "slide the knob and blend the track color. Transparent background." \
)
