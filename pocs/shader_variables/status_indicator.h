#pragma once

#include "shader_defines.h"
#include "shader_variables.h"

typedef struct {
    vec3_t color;           /* LED color */
    float  brightness;      /* 0.0 = off (dark dome), 1.0 = full glow */
    vec3_t bezel_color;     /* metal bezel tint */
} status_indicator_vars_t;

enum {
    STATUS_INDICATOR_VARIABLE_COUNT = 3
};

extern const shader_variable_desc_t g_status_indicator_variables[STATUS_INDICATOR_VARIABLE_COUNT];

vec4_t status_indicator_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define STATUS_INDICATOR_SHADER(X) X( \
    status_indicator, "Status Indicator", status_indicator_main, \
    SHADER_BUFFER_NONE, \
    g_status_indicator_variables, STATUS_INDICATOR_VARIABLE_COUNT, sizeof(status_indicator_vars_t), \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    80, 80, \
    "LED indicator with metal bezel and glass dome. " \
    "<code>brightness</code> drives emission from dark to full glow. Transparent background." \
)
