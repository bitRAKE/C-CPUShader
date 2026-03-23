#pragma once

#include "shader_defines.h"
#include "shader_variables.h"

typedef struct {
    float  value;           /* 0.0 to 1.0 fill amount */
    vec3_t arc_color;       /* color of the filled arc */
    vec3_t track_color;     /* color of the unfilled arc */
    float  thickness;       /* arc thickness (0.04 to 0.20) */
} radial_gauge_vars_t;

enum {
    RADIAL_GAUGE_VARIABLE_COUNT = 4
};

extern const shader_variable_desc_t g_radial_gauge_variables[RADIAL_GAUGE_VARIABLE_COUNT];

vec4_t radial_gauge_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define RADIAL_GAUGE_SHADER(X) X( \
    radial_gauge, "Radial Gauge", radial_gauge_main, \
    SHADER_BUFFER_NONE, \
    g_radial_gauge_variables, RADIAL_GAUGE_VARIABLE_COUNT, sizeof(radial_gauge_vars_t), \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    192, 192, \
    "270&deg; arc gauge with rounded endcaps. Drive <code>value</code> " \
    "from external data for scriptable visualization. Transparent background." \
)
