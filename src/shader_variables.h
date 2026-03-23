#pragma once

#include "defines.h"
#include "../sdk/shader_variables.h"

/* Host-only types */
typedef struct {
    char *name;
    char *value;
} shader_variable_override_t;

/* Host-only function declarations */
size_t      shader_variable_type_size(shader_variable_type_t type);
const char *shader_variable_type_name(shader_variable_type_t type);
bool        shader_variable_parse_value(shader_variable_type_t type, const char *text, void *dest, char *error_text, size_t error_text_size);
