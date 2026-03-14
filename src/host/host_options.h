#pragma once

#include "../present/present_backend.h"

typedef struct {
    present_backend_kind_t backend_kind;
} host_options_t;

bool host_options_parse(host_options_t *options_out, char *error_text, size_t error_text_size);
