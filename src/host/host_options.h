#pragma once

#include "../present/present_backend.h"
#include "../shader_variables.h"

typedef struct {
    present_backend_kind_t backend_kind;
    char                   shader_id[64];
    int                    script_frame_count;
    int                    display_multiplier;
    shader_variable_override_t *variable_overrides;
    int                    variable_override_count;
    char                 **plugin_dirs;
    int                    plugin_dir_count;
    bool                   transparent_display;
    bool                   script_mode;
    bool                   list_shaders;
    bool                   show_help;
} host_options_t;

bool host_options_parse(host_options_t *options_out, char *error_text, size_t error_text_size);
void host_options_cleanup(host_options_t *options);
