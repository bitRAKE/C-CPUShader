#include "shader_catalog_runtime.h"
#include <stdlib.h>
#include <string.h>

#define CATALOG_INITIAL_SHADER_CAP     32
#define CATALOG_INITIAL_COLLECTION_CAP  8

static shader_desc_t       *g_shaders            = NULL;
static int                 *g_shader_collections  = NULL;
static int                  g_shader_count        = 0;
static int                  g_shader_capacity     = 0;

static shader_collection_t *g_collections         = NULL;
static int                  g_collection_count    = 0;
static int                  g_collection_capacity = 0;

void catalog_init(void)
{
    g_shader_capacity = CATALOG_INITIAL_SHADER_CAP;
    g_shaders = (shader_desc_t *)calloc((size_t)g_shader_capacity, sizeof(shader_desc_t));
    g_shader_collections = (int *)calloc((size_t)g_shader_capacity, sizeof(int));
    g_shader_count = 0;

    g_collection_capacity = CATALOG_INITIAL_COLLECTION_CAP;
    g_collections = (shader_collection_t *)calloc((size_t)g_collection_capacity, sizeof(shader_collection_t));
    g_collection_count = 0;
}

void catalog_cleanup(void)
{
    free(g_shaders);
    free(g_shader_collections);
    free(g_collections);
    g_shaders = NULL;
    g_shader_collections = NULL;
    g_collections = NULL;
    g_shader_count = 0;
    g_shader_capacity = 0;
    g_collection_count = 0;
    g_collection_capacity = 0;
}

int catalog_add_collection(const char *name, const char *version, const char *blurb, void *module_handle)
{
    if (g_collection_count >= g_collection_capacity) {
        int new_cap = g_collection_capacity * 2;
        shader_collection_t *new_arr = (shader_collection_t *)realloc(
            g_collections, (size_t)new_cap * sizeof(shader_collection_t));
        if (new_arr == NULL) {
            return -1;
        }
        g_collections = new_arr;
        g_collection_capacity = new_cap;
    }

    int idx = g_collection_count++;
    g_collections[idx].name          = name;
    g_collections[idx].version       = version;
    g_collections[idx].blurb         = blurb;
    g_collections[idx].module_handle = module_handle;
    return idx;
}

int catalog_add_shader(const shader_desc_t *desc, int collection_index)
{
    if (desc == NULL) {
        return -1;
    }

    if (g_shader_count >= g_shader_capacity) {
        int new_cap = g_shader_capacity * 2;
        shader_desc_t *new_shaders = (shader_desc_t *)realloc(
            g_shaders, (size_t)new_cap * sizeof(shader_desc_t));
        int *new_cols = (int *)realloc(
            g_shader_collections, (size_t)new_cap * sizeof(int));
        if (new_shaders == NULL || new_cols == NULL) {
            return -1;
        }
        g_shaders = new_shaders;
        g_shader_collections = new_cols;
        g_shader_capacity = new_cap;
    }

    int idx = g_shader_count++;
    g_shaders[idx] = *desc;
    g_shader_collections[idx] = collection_index;
    return idx;
}

int catalog_find_by_id(const char *id)
{
    if (id == NULL || id[0] == '\0') {
        return -1;
    }

    for (int i = 0; i < g_shader_count; i++) {
        if (g_shaders[i].id != NULL && strcmp(g_shaders[i].id, id) == 0) {
            return i;
        }
    }

    return -1;
}

const shader_desc_t *catalog_get_shaders(int *count_out)
{
    if (count_out != NULL) {
        *count_out = g_shader_count;
    }
    return g_shaders;
}

const shader_collection_t *catalog_get_collections(int *count_out)
{
    if (count_out != NULL) {
        *count_out = g_collection_count;
    }
    return g_collections;
}

int catalog_get_shader_collection(int shader_index)
{
    if (shader_index < 0 || shader_index >= g_shader_count) {
        return -1;
    }
    return g_shader_collections[shader_index];
}
