#include "defines.h"
#include "win.h"
#include "host/host_options.h"
#include "shader_catalog_runtime.h"
#include "plugin_loader.h"

#include "shaders/master_class.h"
#include "shaders/master_class_scrgb.h"
#include "shaders/master_class_hdr10.h"

/* ---- Dynamic catalog registration via X-macros ---- */

#define ADD_SHADER(...) ADD_SHADER_IMPL(__VA_ARGS__)
#define ADD_SHADER_IMPL(id, display_name, entry, buffers_init, variables, variable_count, variable_struct_size, color_space, features, width, height, blurb) \
    do { \
        shader_desc_t _desc = {#id, display_name, blurb, entry, buffers_init, variables, variable_count, variable_struct_size, color_space, features, width, height}; \
        catalog_add_shader(&_desc, _collection); \
    } while (0)

static void catalog_init_builtin(void)
{
    int _collection;

    _collection = catalog_add_collection("Master Class", NULL,
        "Flagship path-tracing studies with progressive accumulation.", NULL);
    MASTER_CLASS_SHADER(ADD_SHADER);
    MASTER_CLASS_SCRGB_SHADER(ADD_SHADER);
    MASTER_CLASS_HDR10_SHADER(ADD_SHADER);
}

#undef ADD_SHADER
#undef ADD_SHADER_IMPL

/* ---- Shader catalog accessors ---- */

static int g_selected_shader_index = -1;
static int g_active_shader_index = -1;

static void write_stream_text(DWORD std_handle_kind, const char *text)
{
    static bool attached_parent_console = false;
    HANDLE handle = GetStdHandle(std_handle_kind);
    DWORD written = 0;

    if ((handle == NULL || handle == INVALID_HANDLE_VALUE) && !attached_parent_console) {
        attached_parent_console = AttachConsole(ATTACH_PARENT_PROCESS) ? true : false;
        handle = GetStdHandle(std_handle_kind);
    }

    if (handle == NULL || handle == INVALID_HANDLE_VALUE || text == NULL || text[0] == '\0') {
        return;
    }

    WriteFile(handle, text, (DWORD)strlen(text), &written, NULL);
}

static void write_stdout_text(const char *text)
{
    write_stream_text(STD_OUTPUT_HANDLE, text);
}

static void write_stderr_line(const char *text)
{
    write_stream_text(STD_ERROR_HANDLE, text);
    write_stream_text(STD_ERROR_HANDLE, "\r\n");
}

static void print_help_text(void)
{
    write_stdout_text(
        "Usage: bin.exe [options]\r\n"
        "\r\n"
        "Runtime:\r\n"
        "  --help                     Show this help text.\r\n"
        "  --list-shaders             Print shader ids for scripting.\r\n"
        "  --backend=dx12|ogldx|vk|gdi\r\n"
        "  --scale=<n>                Set the popup display multiplier.\r\n"
        "  --transparent              Use the layered alpha-respecting popup path.\r\n"
        "  --opaque                   Use the normal opaque popup path.\r\n"
        "  --plugin-dir \"<dir>\"       Add an extra plugin search directory. Repeat as needed.\r\n"
        "\r\n"
        "Scripting:\r\n"
        "  --shader=<id>              Select the shader to script.\r\n"
        "  --capture                  Run non-interactively, write PNG output, and exit.\r\n"
        "  --capture=<count>          Shorthand for --capture --frames=<count>.\r\n"
        "  --frames=<count>           Script frame count. Defaults to 1.\r\n"
        "  --var name=value           Override a selected shader variable. Repeat as needed.\r\n"
        "\r\n"
        "Notes:\r\n"
        "  Plugins are loaded from {exe}/plugins/ by default. Use --plugin-dir to add\r\n"
        "  additional directories. All directories are searched recursively.\r\n"
        "  Runtime-only options such as --backend, --scale, and --transparent apply to\r\n"
        "  interactive runs and are rejected during --capture scripting.\r\n"
        "  Scripting hides both the presentation window and the status dialog, then exits\r\n"
        "  automatically after the requested frames are captured.\r\n"
        "  Shader variables are host-parsed defaults plus optional repeated --var overrides,\r\n"
        "  and are only available in --capture scripting.\r\n"
        "  Transparent display currently uses the GDI presenter in interactive mode.\r\n"
        "  HDR-oriented DX12/Vulkan paths remain opaque popup paths.\r\n"
        "\r\n"
        "Examples:\r\n"
        "  bin.exe --backend=vk\r\n"
        "  bin.exe --shader=animated_sprite --capture --frames=48\r\n"
        "  bin.exe --shader=button_round_metal --capture --var depression=0.65 --var primary_color=0.18,0.62,0.28\r\n"
        "  bin.exe --shader=animated_sprite --capture=48\r\n"
        "  bin.exe --plugin-dir \"C:\\my_shaders\" --plugin-dir \"D:\\more_shaders\"\r\n");
}

static void list_shaders_text(void)
{
    const shader_desc_t *shaders;
    const shader_collection_t *collections;
    int shader_count = 0;
    int collection_count = 0;
    char line[512];
    int current_collection = -1;

    shaders = catalog_get_shaders(&shader_count);
    collections = catalog_get_collections(&collection_count);

    for (int index = 0; index < shader_count; index++) {
        int col = catalog_get_shader_collection(index);

        if (col != current_collection && col >= 0 && col < collection_count) {
            if (current_collection >= 0) {
                write_stdout_text("\r\n");
            }
            snprintf(line, sizeof(line), "%s\r\n",
                collections[col].name != NULL ? collections[col].name : "Unknown");
            write_stdout_text(line);
            current_collection = col;
        }

        snprintf(
            line,
            sizeof(line),
            "  %-28s  %4d x %-4d  %s\r\n",
            shaders[index].id,
            shaders[index].preferred_width,
            shaders[index].preferred_height,
            shaders[index].display_name != NULL ? shaders[index].display_name : shaders[index].id);
        write_stdout_text(line);
    }
}

static const shader_desc_t *shader_get_by_index(int index)
{
    int count = 0;
    const shader_desc_t *shaders = catalog_get_shaders(&count);

    if (index < 0 || index >= count) {
        return NULL;
    }

    return &shaders[index];
}

static int shader_get_selected_index(void)
{
    return g_selected_shader_index;
}

static const shader_desc_t *shader_get_selected_shader(void)
{
    return shader_get_by_index(g_selected_shader_index);
}

static const shader_desc_t *shader_get_active_shader(void)
{
    return shader_get_by_index(g_active_shader_index);
}

static void shader_select(int index)
{
    int count = 0;
    catalog_get_shaders(&count);

    if (index >= 0 && index < count) {
        g_selected_shader_index = index;
    }
}

static bool shader_execute_selected(void)
{
    int count = 0;
    catalog_get_shaders(&count);

    if (g_selected_shader_index < 0 || g_selected_shader_index >= count) {
        return false;
    }

    g_active_shader_index = g_selected_shader_index;
    return true;
}

static void shader_stop_active(void)
{
    g_active_shader_index = -1;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    shader_host_callbacks_t shader_host = {0};
    host_options_t options = {0};
    char option_error[256];
    char startup_error[256];
    int default_shader_index;

    (void)hInstance;
    (void)hPrevInstance;
    (void)nShowCmd;
    (void)lpCmdLine;

    if (!host_options_parse(&options, option_error, sizeof(option_error))) {
        write_stderr_line(option_error);
        return 1;
    }

    if (options.show_help) {
        catalog_init();
        catalog_init_builtin();
        plugin_loader_scan((const char **)options.plugin_dirs, options.plugin_dir_count);
        print_help_text();
        if (options.list_shaders) {
            write_stdout_text("\r\nShaders:\r\n");
            list_shaders_text();
        }
        plugin_loader_cleanup();
        catalog_cleanup();
        host_options_cleanup(&options);
        return 0;
    }

    /* Initialize the dynamic catalog with built-in shaders. */
    catalog_init();
    catalog_init_builtin();

    /* Discover and load plugin DLLs. */
    plugin_loader_scan((const char **)options.plugin_dirs, options.plugin_dir_count);

    if (options.list_shaders) {
        list_shaders_text();
        plugin_loader_cleanup();
        catalog_cleanup();
        host_options_cleanup(&options);
        return 0;
    }

    default_shader_index = catalog_find_by_id("sdf_fixed_hello_world");
    g_selected_shader_index = (default_shader_index >= 0) ? default_shader_index : 0;
    if (options.script_mode && options.shader_id[0] != '\0') {
        int shader_index = catalog_find_by_id(options.shader_id);

        if (shader_index < 0) {
            snprintf(startup_error, sizeof(startup_error), "Unknown shader id '%s'. Use --list-shaders to inspect the catalog.", options.shader_id);
            write_stderr_line(startup_error);
            plugin_loader_cleanup();
            catalog_cleanup();
            host_options_cleanup(&options);
            return 1;
        }

        g_selected_shader_index = shader_index;
    }

    if (!window_create("Renderer", 1024, 1024, &options)) {
        plugin_loader_cleanup();
        catalog_cleanup();
        host_options_cleanup(&options);
        return 1;
    }

    shader_host.get_catalog              = catalog_get_shaders;
    shader_host.get_collections          = catalog_get_collections;
    shader_host.get_shader_collection    = catalog_get_shader_collection;
    shader_host.get_selected_index       = shader_get_selected_index;
    shader_host.get_selected_shader      = shader_get_selected_shader;
    shader_host.get_active_shader        = shader_get_active_shader;
    shader_host.select_shader            = shader_select;
    shader_host.execute_selected_shader  = shader_execute_selected;
    shader_host.stop_active_shader       = shader_stop_active;
    window_set_shader_host(&shader_host);

    /* Show plugin loader diagnostics in the stats dialog. */
    {
        int diag_count = plugin_loader_diagnostic_count();
        for (int i = 0; i < diag_count; i++) {
            window_push_startup_diagnostic(plugin_loader_diagnostic(i));
        }
    }

    if (options.script_mode && !window_execute_selected_shader()) {
        plugin_loader_cleanup();
        catalog_cleanup();
        host_options_cleanup(&options);
        return 1;
    }

    {
        int exit_code = window_run(30);
        plugin_loader_cleanup();
        catalog_cleanup();
        host_options_cleanup(&options);
        return exit_code;
    }
}
