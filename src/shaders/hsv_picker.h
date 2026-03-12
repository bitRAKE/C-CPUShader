#pragma once

#include "../defines.h"

typedef struct {
    float hue;
    float sat;
    float val;
} hsv_picker_state_t;

typedef enum {
    HSV_PICKER_REGION_NONE = 0,
    HSV_PICKER_REGION_HUE_RING = 1,
    HSV_PICKER_REGION_TRIANGLE = 2
} hsv_picker_region_t;

hsv_picker_state_t hsv_picker_default_state(void);
vec3_t hsv_picker_rgb(const hsv_picker_state_t *state);
hsv_picker_region_t hsv_picker_hit_test(vec2_t pixel, vec2_t resolution, const hsv_picker_state_t *state);
hsv_picker_state_t hsv_picker_preview_state(
    const shader_uniforms_t *uniforms,
    const hsv_picker_state_t *base_state,
    hsv_picker_region_t active_region);
vec4_t hsv_picker_render(
    vec2_t fragCoord,
    const shader_uniforms_t *uniforms,
    const hsv_picker_state_t *base_state,
    hsv_picker_region_t active_region);
vec4_t hsv_picker_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define HSV_PICKER_SHADER(X) X( \
    hsv_picker, "HSV Picker", hsv_picker_main, \
    NULL, \
    SHADER_FEATURE_MOUSE, \
    512, 512, \
    "Interactive color picker study driven by mouse hover and drag input." \
)
