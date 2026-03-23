#pragma once

#include "shader_defines.h"

vec4_t diag_hdr_clipping_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define DIAG_HDR_CLIPPING_SHADER(X) X( \
    diag_hdr_clipping, "HDR Clipping", diag_hdr_clipping_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SCENE_LINEAR, \
    SHADER_FEATURE_NONE, \
    1920, 1080, \
    "Luminance step ladder from 0.05 to 10.0 linear " \
    "(~4 to ~800 nits). On SDR all patches above 1.0 clip to " \
    "identical white. On HDR each step is visibly brighter than " \
    "the last &mdash; the point where steps merge reveals the " \
    "display&rsquo;s peak luminance." \
)
