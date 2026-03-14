#include "host_options.h"

#include <shellapi.h>
#include <string.h>

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
    if (_wcsicmp(name, L"ogl") == 0) {
        *kind_out = PRESENT_BACKEND_OGL;
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

bool host_options_parse(host_options_t *options_out, char *error_text, size_t error_text_size)
{
    LPWSTR *argv = NULL;
    int argc = 0;

    if (options_out == NULL) {
        return false;
    }

    options_out->backend_kind = PRESENT_BACKEND_DX12;
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

        if (_wcsnicmp(arg, L"--backend=", 10) == 0) {
            if (!parse_backend_name(arg + 10, &options_out->backend_kind)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Unknown backend '%S'. Use dx12, ogl, vk, or gdi.", arg + 10);
                }
                LocalFree(argv);
                return false;
            }
        } else if (_wcsicmp(arg, L"--backend") == 0) {
            if (index + 1 >= argc || !parse_backend_name(argv[index + 1], &options_out->backend_kind)) {
                if (error_text != NULL && error_text_size > 0) {
                    snprintf(error_text, error_text_size, "Expected backend name after --backend. Use dx12, ogl, vk, or gdi.");
                }
                LocalFree(argv);
                return false;
            }
            index++;
        }
    }

    LocalFree(argv);
    return true;
}
