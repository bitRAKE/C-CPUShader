#include "defines.h"
#include "plugin_loader.h"
#include "shader_catalog_runtime.h"

#include <string.h>
#include <stdio.h>
#include <windows.h>

/* Import the plugin contract types from the SDK header. */
#define SHADER_PLUGIN_ABI_VERSION 1

typedef struct {
    const char *collection_name;
    const char *collection_version;
    const char *collection_blurb;
    int         abi_version;
    HMODULE     module_handle;
} plugin_collection_info_t;

typedef const plugin_collection_info_t *(*PluginQueryFunc)(void);
typedef const shader_desc_t *(*PluginLoadFunc)(int *count_out);

#define PLUGIN_QUERY_NAME "shader_collection_query"
#define PLUGIN_LOAD_NAME  "shader_collection_load"

#define MAX_LOADED_MODULES 64
#define MAX_SCAN_DEPTH      4
#define MAX_DIAGNOSTICS     32

typedef struct {
    char text[256];
} plugin_diagnostic_t;

static HMODULE              g_modules[MAX_LOADED_MODULES];
static int                  g_module_count = 0;
static plugin_diagnostic_t  g_diagnostics[MAX_DIAGNOSTICS];
static int                  g_diagnostic_count = 0;
static int                  g_plugins_loaded = 0;

static void push_diagnostic(const char *fmt, ...)
{
    if (g_diagnostic_count >= MAX_DIAGNOSTICS) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(
        g_diagnostics[g_diagnostic_count].text,
        sizeof(g_diagnostics[g_diagnostic_count].text),
        fmt, args);
    va_end(args);
    g_diagnostic_count++;
}

static bool should_skip_directory(const char *dir_name)
{
    static const char *skip_names[] = {
        ".git", ".AGENTS", "obj", "sdk", "docs", "build", "captures", ".vs"
    };

    for (int i = 0; i < (int)(sizeof(skip_names) / sizeof(skip_names[0])); i++) {
        if (_stricmp(dir_name, skip_names[i]) == 0) {
            return true;
        }
    }

    return false;
}

static bool try_load_plugin(const char *dll_path)
{
    const plugin_collection_info_t *info;
    const shader_desc_t *shaders;
    PluginQueryFunc query;
    PluginLoadFunc  load;
    HMODULE module;
    int shader_count = 0;
    int collection_index;
    int added = 0;

    module = LoadLibraryExA(dll_path, NULL,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (module == NULL) {
        return false;
    }

    query = (PluginQueryFunc)GetProcAddress(module, PLUGIN_QUERY_NAME);
    if (query == NULL) {
        /* Not a shader plugin -- silently skip. */
        FreeLibrary(module);
        return false;
    }

    info = query();
    if (info == NULL) {
        push_diagnostic("Plugin query returned NULL: %s -- skipped", dll_path);
        FreeLibrary(module);
        return false;
    }

    if (info->abi_version != SHADER_PLUGIN_ABI_VERSION) {
        push_diagnostic("Plugin ABI mismatch: %s (ABI %d, expected %d) -- skipped",
            info->collection_name != NULL ? info->collection_name : dll_path,
            info->abi_version, SHADER_PLUGIN_ABI_VERSION);
        FreeLibrary(module);
        return false;
    }

    load = (PluginLoadFunc)GetProcAddress(module, PLUGIN_LOAD_NAME);
    if (load == NULL) {
        push_diagnostic("Plugin missing load export: %s -- skipped",
            info->collection_name != NULL ? info->collection_name : dll_path);
        FreeLibrary(module);
        return false;
    }

    shaders = load(&shader_count);
    if (shaders == NULL || shader_count <= 0) {
        push_diagnostic("Plugin returned no shaders: %s -- skipped",
            info->collection_name != NULL ? info->collection_name : dll_path);
        FreeLibrary(module);
        return false;
    }

    collection_index = catalog_add_collection(
        info->collection_name,
        info->collection_version,
        info->collection_blurb,
        (void *)info->module_handle);

    for (int i = 0; i < shader_count; i++) {
        if (catalog_find_by_id(shaders[i].id) >= 0) {
            push_diagnostic("Plugin shader ID '%s' conflicts with existing -- skipped",
                shaders[i].id);
            continue;
        }
        catalog_add_shader(&shaders[i], collection_index);
        added++;
    }

    if (added == 0) {
        push_diagnostic("Plugin '%s' had all shaders skipped due to ID conflicts",
            info->collection_name != NULL ? info->collection_name : dll_path);
        FreeLibrary(module);
        return false;
    }

    /* Keep the module loaded. */
    if (g_module_count < MAX_LOADED_MODULES) {
        g_modules[g_module_count++] = module;
    }

    push_diagnostic("Loaded plugin: %s%s%s (%d shader%s)",
        info->collection_name != NULL ? info->collection_name : "unnamed",
        info->collection_version != NULL ? " v" : "",
        info->collection_version != NULL ? info->collection_version : "",
        added, added == 1 ? "" : "s");

    return true;
}

static void scan_directory(const char *dir_path, int depth)
{
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle;
    char pattern[MAX_PATH];
    char full_path[MAX_PATH];

    if (depth > MAX_SCAN_DEPTH) {
        return;
    }

    /* Enumerate *.dll in this directory. */
    snprintf(pattern, sizeof(pattern), "%s\\*.dll", dir_path);
    find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
            if (try_load_plugin(full_path)) {
                g_plugins_loaded++;
            }
        } while (FindNextFileA(find_handle, &find_data));
        FindClose(find_handle);
    }

    /* Recurse into subdirectories. */
    snprintf(pattern, sizeof(pattern), "%s\\*", dir_path);
    find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                continue;
            }
            if (find_data.cFileName[0] == '.' &&
                (find_data.cFileName[1] == '\0' ||
                 (find_data.cFileName[1] == '.' && find_data.cFileName[2] == '\0'))) {
                continue;
            }
            if (should_skip_directory(find_data.cFileName)) {
                continue;
            }
            snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
            scan_directory(full_path, depth + 1);
        } while (FindNextFileA(find_handle, &find_data));
        FindClose(find_handle);
    }
}

int plugin_loader_scan(const char **extra_dirs, int extra_dir_count)
{
    char exe_path[MAX_PATH];
    char plugins_path[MAX_PATH];
    char *last_sep;
    DWORD len;
    DWORD attr;

    g_plugins_loaded = 0;
    g_diagnostic_count = 0;

    len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return 0;
    }

    /* Strip the executable filename to get the directory. */
    last_sep = strrchr(exe_path, '\\');
    if (last_sep != NULL) {
        *last_sep = '\0';
    }

    /* Default search: {exe}/plugins/ */
    snprintf(plugins_path, sizeof(plugins_path), "%s\\plugins", exe_path);
    attr = GetFileAttributesA(plugins_path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        scan_directory(plugins_path, 0);
    }

    /* Additional directories from --plugin-dir.
     * Relative paths are resolved against the exe directory. */
    for (int i = 0; i < extra_dir_count; i++) {
        char resolved[MAX_PATH];
        const char *dir;

        if (extra_dirs[i] == NULL || extra_dirs[i][0] == '\0') {
            continue;
        }

        dir = extra_dirs[i];

        /* Resolve relative paths against the exe directory. */
        if (dir[0] != '\\' && dir[0] != '/' && !(dir[0] != '\0' && dir[1] == ':')) {
            snprintf(resolved, sizeof(resolved), "%s\\%s", exe_path, dir);
            dir = resolved;
        }

        attr = GetFileAttributesA(dir);
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            push_diagnostic("Plugin directory not found: %s -- skipped", dir);
            continue;
        }
        scan_directory(dir, 0);
    }

    return g_plugins_loaded;
}

void plugin_loader_cleanup(void)
{
    for (int i = 0; i < g_module_count; i++) {
        if (g_modules[i] != NULL) {
            FreeLibrary(g_modules[i]);
            g_modules[i] = NULL;
        }
    }
    g_module_count = 0;
    g_diagnostic_count = 0;
    g_plugins_loaded = 0;
}

int plugin_loader_diagnostic_count(void)
{
    return g_diagnostic_count;
}

const char *plugin_loader_diagnostic(int index)
{
    if (index < 0 || index >= g_diagnostic_count) {
        return NULL;
    }
    return g_diagnostics[index].text;
}
