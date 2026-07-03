#include "host_options.h"

#include <limits.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static bool parse_backend_name(const wchar_t *name, present_backend_kind_t *kind_out)
{
    if (name == NULL || kind_out == NULL) {
        return false;
    }

    if (_wcsicmp(name, L"dxgi") == 0) {
        *kind_out = PRESENT_BACKEND_DX12;
        return true;
    }
    if (_wcsicmp(name, L"dx12") == 0) {
        *kind_out = PRESENT_BACKEND_DX12;
        return true;
    }
    if (_wcsicmp(name, L"ogldx") == 0) {
        *kind_out = PRESENT_BACKEND_OGLDX;
        return true;
    }
    if (_wcsicmp(name, L"ogl") == 0) {
        *kind_out = PRESENT_BACKEND_OGLDX;
        return true;
    }
    if (_wcsicmp(name, L"vk") == 0) {
        *kind_out = PRESENT_BACKEND_VK;
        return true;
    }
    if (_wcsicmp(name, L"gdi") == 0) {
        *kind_out = PRESENT_BACKEND_GDI;
        return true;
    }

    return false;
}

static bool copy_wide_text(const wchar_t *source, char *dest, size_t dest_size)
{
    int converted;

    if (source == NULL || dest == NULL || dest_size == 0) {
        return false;
    }

    converted = WideCharToMultiByte(CP_UTF8, 0, source, -1, dest, (int)dest_size, NULL, NULL);
    return converted > 0;
}

static char *copy_wide_text_alloc(const wchar_t *source)
{
    int bytes_needed;
    char *copy;

    if (source == NULL) {
        return NULL;
    }

    bytes_needed = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (bytes_needed <= 0) {
        return NULL;
    }

    copy = (char *)malloc((size_t)bytes_needed);
    if (copy == NULL) {
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, source, -1, copy, bytes_needed, NULL, NULL) <= 0) {
        free(copy);
        return NULL;
    }

    return copy;
}

static bool is_ascii_space(char ch)
{
    return ch == ' ' ||
           ch == '\t' ||
           ch == '\r' ||
           ch == '\n' ||
           ch == '\f' ||
           ch == '\v';
}

static char *duplicate_trimmed_slice(const char *start, const char *end)
{
    size_t length;
    char *copy;

    while (start < end && is_ascii_space(*start)) {
        start++;
    }
    while (end > start && is_ascii_space(*(end - 1))) {
        end--;
    }

    length = (size_t)(end - start);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0) {
        memcpy(copy, start, length);
    }
    copy[length] = '\0';
    return copy;
}

static int find_variable_override_index(const host_options_t *options, const char *name)
{
    if (options == NULL || name == NULL) {
        return -1;
    }

    for (int index = 0; index < options->variable_override_count; index++) {
        if (strcmp(options->variable_overrides[index].name, name) == 0) {
            return index;
        }
    }

    return -1;
}

static bool append_variable_override(host_options_t *options, const wchar_t *assignment, char *error_text, size_t error_text_size)
{
    shader_variable_override_t *new_items = NULL;
    char *assignment_utf8 = NULL;
    char *name = NULL;
    char *value = NULL;
    char *equals = NULL;

    if (options == NULL || assignment == NULL) {
        return false;
    }

    assignment_utf8 = copy_wide_text_alloc(assignment);
    if (assignment_utf8 == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to parse --var text.");
        }
        return false;
    }

    equals = strchr(assignment_utf8, '=');
    if (equals == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Expected --var name=value.");
        }
        free(assignment_utf8);
        return false;
    }

    name = duplicate_trimmed_slice(assignment_utf8, equals);
    value = duplicate_trimmed_slice(equals + 1, assignment_utf8 + strlen(assignment_utf8));
    free(assignment_utf8);

    if (name == NULL || value == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to allocate --var text.");
        }
        free(name);
        free(value);
        return false;
    }

    if (name[0] == '\0' || value[0] == '\0') {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Expected --var name=value.");
        }
        free(name);
        free(value);
        return false;
    }

    if (find_variable_override_index(options, name) >= 0) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Duplicate --var override for '%s'.", name);
        }
        free(name);
        free(value);
        return false;
    }

    new_items = (shader_variable_override_t *)realloc(
        options->variable_overrides,
        (size_t)(options->variable_override_count + 1) * sizeof(*options->variable_overrides));
    if (new_items == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to allocate --var override list.");
        }
        free(name);
        free(value);
        return false;
    }

    options->variable_overrides = new_items;
    options->variable_overrides[options->variable_override_count].name = name;
    options->variable_overrides[options->variable_override_count].value = value;
    options->variable_override_count++;
    return true;
}

static bool parse_positive_int(const wchar_t *text, int *value_out)
{
    wchar_t *end = NULL;
    long value;

    if (text == NULL || value_out == NULL || text[0] == L'\0') {
        return false;
    }

    value = wcstol(text, &end, 10);
    if (end == text || (end != NULL && *end != L'\0') || value <= 0 || value > INT_MAX) {
        return false;
    }

    *value_out = (int)value;
    return true;
}

static bool append_plugin_dir(host_options_t *options, const wchar_t *dir, char *error_text, size_t error_text_size)
{
    char *dir_utf8 = NULL;
    char **new_dirs = NULL;

    if (options == NULL || dir == NULL) {
        return false;
    }

    dir_utf8 = copy_wide_text_alloc(dir);
    if (dir_utf8 == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to parse --plugin-dir path.");
        }
        return false;
    }

    if (dir_utf8[0] == '\0') {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Expected a directory path after --plugin-dir.");
        }
        free(dir_utf8);
        return false;
    }

    new_dirs = (char **)realloc(
        options->plugin_dirs,
        (size_t)(options->plugin_dir_count + 1) * sizeof(*options->plugin_dirs));
    if (new_dirs == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Failed to allocate --plugin-dir list.");
        }
        free(dir_utf8);
        return false;
    }

    options->plugin_dirs = new_dirs;
    options->plugin_dirs[options->plugin_dir_count] = dir_utf8;
    options->plugin_dir_count++;
    return true;
}

void host_options_cleanup(host_options_t *options)
{
    if (options == NULL) {
        return;
    }

    for (int index = 0; index < options->variable_override_count; index++) {
        free(options->variable_overrides[index].name);
        free(options->variable_overrides[index].value);
    }

    free(options->variable_overrides);
    options->variable_overrides = NULL;
    options->variable_override_count = 0;

    for (int index = 0; index < options->plugin_dir_count; index++) {
        free(options->plugin_dirs[index]);
    }

    free(options->plugin_dirs);
    options->plugin_dirs = NULL;
    options->plugin_dir_count = 0;
}

bool host_options_parse(host_options_t *options_out, char *error_text, size_t error_text_size)
{
    LPWSTR *argv = NULL;
    int argc = 0;
    bool frames_explicit = false;
    bool legacy_capture_count_explicit = false;
    int legacy_capture_count = 0;
    bool backend_explicit = false;
    bool scale_explicit = false;
    bool window_mode_explicit = false;

    if (options_out == NULL) {
        return false;
    }

    options_out->backend_kind = PRESENT_BACKEND_DX12;
    options_out->shader_id[0] = '\0';
    options_out->script_frame_count = 1;
    options_out->display_multiplier = 1;
    options_out->variable_overrides = NULL;
    options_out->variable_override_count = 0;
    options_out->plugin_dirs = NULL;
    options_out->plugin_dir_count = 0;
    options_out->transparent_display = false;
    options_out->script_mode = false;
    options_out->list_shaders = false;
    options_out->show_help = false;
    if (error_text != NULL && error_text_size > 0) {
        error_text[0] = '\0';
    }

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "CommandLineToArgvW failed.");
        }
        return false;
    }

    for (int index = 1; index < argc; index++) {
        const wchar_t *arg = argv[index];
        int parsed_value = 0;

        if (_wcsnicmp(arg, L"--backend=", 10) == 0) {
            if (!parse_backend_name(arg + 10, &options_out->backend_kind)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Unknown backend '%S'. Use dx12/dxgi, ogl/ogldx, vk, or gdi.", arg + 10);
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            backend_explicit = true;
        } else if (_wcsicmp(arg, L"--backend") == 0) {
            if (index + 1 >= argc || !parse_backend_name(argv[index + 1], &options_out->backend_kind)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected backend name after --backend. Use dx12/dxgi, ogl/ogldx, vk, or gdi.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            backend_explicit = true;
            index++;
        } else if (_wcsnicmp(arg, L"--shader=", 9) == 0) {
            if (!copy_wide_text(arg + 9, options_out->shader_id, sizeof(options_out->shader_id))) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Failed to parse shader id.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        } else if (_wcsicmp(arg, L"--shader") == 0) {
            if (index + 1 >= argc || !copy_wide_text(argv[index + 1], options_out->shader_id, sizeof(options_out->shader_id))) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected shader id after --shader.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            index++;
        } else if (_wcsnicmp(arg, L"--capture=", 10) == 0) {
            if (!parse_positive_int(arg + 10, &parsed_value)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected a positive integer after --capture=.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            options_out->script_mode = true;
            legacy_capture_count_explicit = true;
            legacy_capture_count = parsed_value;
            if (!frames_explicit) {
                options_out->script_frame_count = parsed_value;
            } else if (options_out->script_frame_count != parsed_value) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--capture=%d conflicts with --frames=%d. Use --frames for the script frame count.", parsed_value, options_out->script_frame_count);
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        } else if (_wcsicmp(arg, L"--capture") == 0) {
            options_out->script_mode = true;
            if (index + 1 < argc && parse_positive_int(argv[index + 1], &parsed_value)) {
                legacy_capture_count_explicit = true;
                legacy_capture_count = parsed_value;
                if (!frames_explicit) {
                    options_out->script_frame_count = parsed_value;
                } else if (options_out->script_frame_count != parsed_value) {
                    if (error_text != NULL && error_text_size > 0) {
                        snprintf(error_text, error_text_size, "--capture %d conflicts with --frames=%d. Use --frames for the script frame count.", parsed_value, options_out->script_frame_count);
                    }
                    host_options_cleanup(options_out);
                    LocalFree(argv);
                    return false;
                }
                index++;
            }
        } else if (_wcsnicmp(arg, L"--frames=", 9) == 0) {
            if (!parse_positive_int(arg + 9, &options_out->script_frame_count)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected a positive integer after --frames=.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            frames_explicit = true;
            if (legacy_capture_count_explicit && legacy_capture_count != options_out->script_frame_count) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--frames=%d conflicts with --capture=%d. Use --frames for the script frame count.", options_out->script_frame_count, legacy_capture_count);
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        } else if (_wcsicmp(arg, L"--frames") == 0 || _wcsicmp(arg, L"--frame-count") == 0) {
            if (index + 1 >= argc || !parse_positive_int(argv[index + 1], &options_out->script_frame_count)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected a positive integer after %S.", arg);
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            frames_explicit = true;
            if (legacy_capture_count_explicit && legacy_capture_count != options_out->script_frame_count) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--frames=%d conflicts with --capture=%d. Use --frames for the script frame count.", options_out->script_frame_count, legacy_capture_count);
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            index++;
        } else if (_wcsnicmp(arg, L"--var=", 6) == 0) {
            if (!append_variable_override(options_out, arg + 6, error_text, error_text_size)) {
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        } else if (_wcsicmp(arg, L"--var") == 0) {
            if (index + 1 >= argc || !append_variable_override(options_out, argv[index + 1], error_text, error_text_size)) {
                if (error_text != NULL && error_text_size > 0 && error_text[0] == '\0') {
                    snprintf(error_text, error_text_size, "Expected --var name=value.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            index++;
        } else if (_wcsnicmp(arg, L"--scale=", 8) == 0) {
            if (!parse_positive_int(arg + 8, &options_out->display_multiplier)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected a positive integer after --scale=.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            scale_explicit = true;
        } else if (_wcsicmp(arg, L"--scale") == 0 || _wcsicmp(arg, L"--display-multiplier") == 0) {
            if (index + 1 >= argc || !parse_positive_int(argv[index + 1], &options_out->display_multiplier)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected a positive integer after %S.", arg);
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            scale_explicit = true;
            index++;
        } else if (_wcsnicmp(arg, L"--display-multiplier=", 21) == 0) {
            if (!parse_positive_int(arg + 21, &options_out->display_multiplier)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected a positive integer after --display-multiplier=.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            scale_explicit = true;
        } else if (_wcsnicmp(arg, L"--plugin-dir=", 13) == 0) {
            if (!append_plugin_dir(options_out, arg + 13, error_text, error_text_size)) {
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        } else if (_wcsicmp(arg, L"--plugin-dir") == 0) {
            if (index + 1 >= argc || !append_plugin_dir(options_out, argv[index + 1], error_text, error_text_size)) {
                if (error_text != NULL && error_text_size > 0 && error_text[0] == '\0') {
                    snprintf(error_text, error_text_size, "Expected a directory path after --plugin-dir.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
            index++;
        } else if (_wcsicmp(arg, L"--transparent") == 0) {
            options_out->transparent_display = true;
            window_mode_explicit = true;
        } else if (_wcsicmp(arg, L"--opaque") == 0) {
            options_out->transparent_display = false;
            window_mode_explicit = true;
        } else if (_wcsicmp(arg, L"--list-shaders") == 0) {
            options_out->list_shaders = true;
        } else if (_wcsicmp(arg, L"--help") == 0 || _wcsicmp(arg, L"-h") == 0 || _wcsicmp(arg, L"/?") == 0) {
            options_out->show_help = true;
        } else {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "Unknown option '%S'. Use --help for usage.", arg);
            }
            host_options_cleanup(options_out);
            LocalFree(argv);
            return false;
        }
    }

    if (!options_out->show_help && !options_out->list_shaders) {
        if (options_out->script_mode) {
            if (options_out->variable_override_count > 0 && options_out->shader_id[0] == '\0') {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--var requires --shader=<id>.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }

            if (options_out->shader_id[0] == '\0') {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--capture scripting requires --shader=<id>.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }

            if (backend_explicit) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--backend has no meaning during --capture scripting.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }

            if (scale_explicit) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--scale has no meaning during --capture scripting.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }

            if (window_mode_explicit) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--transparent/--opaque have no meaning during --capture scripting.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        } else {
            if (options_out->variable_override_count > 0) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--var requires --capture scripting.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }

            if (options_out->shader_id[0] != '\0') {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "--shader=<id> is only supported with --capture scripting.");
                }
                host_options_cleanup(options_out);
                LocalFree(argv);
                return false;
            }
        }

        if (frames_explicit && !options_out->script_mode) {
            if (error_text != NULL && error_text_size > 0) {
                snprintf(error_text, error_text_size, "--frames requires --capture scripting.");
            }
            host_options_cleanup(options_out);
            LocalFree(argv);
            return false;
        }
    }

    LocalFree(argv);
    return true;
}
