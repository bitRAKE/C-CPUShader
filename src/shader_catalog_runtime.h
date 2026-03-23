#pragma once

#include "shader_catalog.h"

typedef struct {
    const char *name;
    const char *version;
    const char *blurb;
    void       *module_handle;  /* HMODULE, NULL for built-in */
} shader_collection_t;

void catalog_init(void);
void catalog_cleanup(void);

int  catalog_add_collection(const char *name, const char *version, const char *blurb, void *module_handle);
int  catalog_add_shader(const shader_desc_t *desc, int collection_index);
int  catalog_find_by_id(const char *id);

const shader_desc_t       *catalog_get_shaders(int *count_out);
const shader_collection_t *catalog_get_collections(int *count_out);
int  catalog_get_shader_collection(int shader_index);
