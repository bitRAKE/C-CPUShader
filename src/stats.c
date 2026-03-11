#include "defines.h"
#include "stats.h"
#include "resource.h"

#include <commctrl.h>
#include <string.h>

static HWND              g_stats_hwnd = NULL;
static stats_callbacks_t g_callbacks = {0};
static void             *g_user_data = NULL;

static char              g_shader_cache[128];
static char              g_accum_cache[64];
static char              g_perf_cache[128];
static char              g_time_cache[64];
static char              g_workers_cache[64];
static char              g_display_cache[128];
static bool              g_vsync_cache_valid = false;
static bool              g_vsync_cache = false;

static void stats_reset_cache(void)
{
    g_shader_cache[0] = '\0';
    g_accum_cache[0] = '\0';
    g_perf_cache[0] = '\0';
    g_time_cache[0] = '\0';
    g_workers_cache[0] = '\0';
    g_display_cache[0] = '\0';
    g_vsync_cache_valid = false;
    g_vsync_cache = false;
}

static bool stats_update_text(int control_id, char *cache, size_t cache_size, const char *text)
{
    if (strcmp(cache, text) == 0) {
        return false;
    }

    SetDlgItemTextA(g_stats_hwnd, control_id, text);
    snprintf(cache, cache_size, "%s", text);
    return true;
}

static INT_PTR CALLBACK StatsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;

    switch (msg) {
        case WM_INITDIALOG:
            g_stats_hwnd = hwnd;
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_BUTTON_PREV:
                    if (g_callbacks.on_cycle_shader != NULL) {
                        g_callbacks.on_cycle_shader(-1, g_user_data);
                    }
                    return TRUE;

                case IDC_BUTTON_NEXT:
                    if (g_callbacks.on_cycle_shader != NULL) {
                        g_callbacks.on_cycle_shader(1, g_user_data);
                    }
                    return TRUE;

                case IDC_BUTTON_RESET:
                    if (g_callbacks.on_reset != NULL) {
                        g_callbacks.on_reset(g_user_data);
                    }
                    return TRUE;

                case IDC_CHECK_VSYNC:
                    if (HIWORD(wp) == BN_CLICKED && g_callbacks.on_set_vsync != NULL) {
                        g_callbacks.on_set_vsync(IsDlgButtonChecked(hwnd, IDC_CHECK_VSYNC) == BST_CHECKED, g_user_data);
                    }
                    return TRUE;

                case IDCANCEL:
                    if (g_callbacks.on_close != NULL) {
                        g_callbacks.on_close(hwnd, g_user_data);
                    }
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            if (g_callbacks.on_close != NULL) {
                g_callbacks.on_close(hwnd, g_user_data);
            }
            return TRUE;

        case WM_DESTROY:
            if (hwnd == g_stats_hwnd) {
                g_stats_hwnd = NULL;
            }
            return TRUE;
    }

    if (g_callbacks.on_keydown != NULL && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)) {
        if (g_callbacks.on_keydown(hwnd, wp, g_user_data)) {
            return TRUE;
        }
    }

    return FALSE;
}

bool stats_create(HINSTANCE instance, const char *title, const stats_callbacks_t *callbacks, void *user_data)
{
    INITCOMMONCONTROLSEX common_controls = {0};
    char caption[256];

    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&common_controls);

    if (callbacks != NULL) {
        g_callbacks = *callbacks;
    } else {
        ZeroMemory(&g_callbacks, sizeof(g_callbacks));
    }
    g_user_data = user_data;
    stats_reset_cache();

    g_stats_hwnd = CreateDialogParamA(instance, MAKEINTRESOURCEA(IDD_STATUS_DIALOG), NULL, StatsDlgProc, 0);
    if (g_stats_hwnd == NULL) {
        return false;
    }

    snprintf(caption, sizeof(caption), "%s Status", title);
    SetWindowTextA(g_stats_hwnd, caption);
    return true;
}

void stats_destroy(void)
{
    HWND hwnd = g_stats_hwnd;

    if (hwnd != NULL && IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }
}

void stats_show(void)
{
    if (g_stats_hwnd != NULL) {
        ShowWindow(g_stats_hwnd, SW_SHOW);
        UpdateWindow(g_stats_hwnd);
    }
}

void stats_update(const stats_state_t *state)
{
    char shader_text[128];
    char accum_text[64];
    char perf_text[128];
    char time_text[64];
    char workers_text[64];
    char display_text[128];

    if (g_stats_hwnd == NULL || state == NULL) {
        return;
    }

    snprintf(shader_text, sizeof(shader_text), "%s", state->shader_name != NULL ? state->shader_name : "unknown");
    snprintf(accum_text, sizeof(accum_text), "%s", state->temporal_accumulation ? "on" : "off");
    snprintf(perf_text, sizeof(perf_text), "%.1f fps, %.2f ms avg/%d", state->fps, state->milliseconds, state->frame_sample_count);
    snprintf(time_text, sizeof(time_text), "%.2f s", state->time_seconds);
    snprintf(workers_text, sizeof(workers_text), "%d render + gui", state->total_workers);
    snprintf(display_text, sizeof(display_text), "%d x %d popup | right-drag to move", state->display_width, state->display_height);

    stats_update_text(IDC_STATUS_SHADER, g_shader_cache, sizeof(g_shader_cache), shader_text);
    stats_update_text(IDC_STATUS_ACCUM, g_accum_cache, sizeof(g_accum_cache), accum_text);
    stats_update_text(IDC_STATUS_PERF, g_perf_cache, sizeof(g_perf_cache), perf_text);
    stats_update_text(IDC_STATUS_TIME, g_time_cache, sizeof(g_time_cache), time_text);
    stats_update_text(IDC_STATUS_WORKERS, g_workers_cache, sizeof(g_workers_cache), workers_text);
    stats_update_text(IDC_STATUS_DISPLAY, g_display_cache, sizeof(g_display_cache), display_text);

    if (!g_vsync_cache_valid || g_vsync_cache != state->vsync_enabled) {
        CheckDlgButton(g_stats_hwnd, IDC_CHECK_VSYNC, state->vsync_enabled ? BST_CHECKED : BST_UNCHECKED);
        g_vsync_cache_valid = true;
        g_vsync_cache = state->vsync_enabled;
    }
}

bool stats_is_dialog_message(MSG *msg)
{
    if (g_stats_hwnd == NULL || msg == NULL) {
        return false;
    }

    return IsDialogMessageA(g_stats_hwnd, msg);
}

bool stats_get_window_rect(RECT *rect_out)
{
    if (g_stats_hwnd == NULL || rect_out == NULL) {
        return false;
    }

    return GetWindowRect(g_stats_hwnd, rect_out);
}

bool stats_is_window(HWND hwnd)
{
    return hwnd != NULL && hwnd == g_stats_hwnd;
}

HWND stats_window(void)
{
    return g_stats_hwnd;
}
