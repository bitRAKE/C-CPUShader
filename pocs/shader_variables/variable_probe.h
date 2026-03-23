#pragma once

#include "shader_defines.h"
#include "shader_variables.h"

typedef struct {
    float exposure;
    int   stripe_count;
    bool  invert;
    vec2_t offset;
    vec3_t tint;
    vec4_t vignette;
} variable_probe_vars_t;

enum {
    VARIABLE_PROBE_VARIABLE_COUNT = 6
};

extern const shader_variable_desc_t g_variable_probe_variables[VARIABLE_PROBE_VARIABLE_COUNT];

vec4_t variable_probe_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define VARIABLE_PROBE_SHADER(X) X( \
    variable_probe, "Variable Probe", variable_probe_main, \
    SHADER_BUFFER_NONE, \
    g_variable_probe_variables, VARIABLE_PROBE_VARIABLE_COUNT, sizeof(variable_probe_vars_t), \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    320, 180, \
    "Scripting-only variable test surface for default parsing and repeated --var overrides." \
)
