#include "defines.h"
#include "stats.h"
#include "resource.h"

#include <commctrl.h>
#include <shellapi.h>
#include <string.h>

static HWND              g_stats_hwnd = NULL;
static stats_callbacks_t g_callbacks = {0};
static void             *g_user_data = NULL;

static char              g_shader_cache[128];
static char              g_expect_cache[256];
static char              g_blurb_cache[768];
static char              g_render_cache[64];
static char              g_perf_cache[128];
static char              g_time_cache[64];
static char              g_workers_cache[64];
static char              g_display_cache[128];
static char              g_execute_cache[32];
static bool              g_vsync_cache_valid = false;
static bool              g_vsync_cache = false;
static int               g_catalog_count = 0;

static void stats_reset_cache(void)
{
    g_shader_cache[0] = '\0';
    g_expect_cache[0] = '\0';
    g_blurb_cache[0] = '\0';
    g_render_cache[0] = '\0';
    g_perf_cache[0] = '\0';
    g_time_cache[0] = '\0';
    g_workers_cache[0] = '\0';
    g_display_cache[0] = '\0';
    g_execute_cache[0] = '\0';
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
                case IDC_SHADER_LIST:
                    if (HIWORD(wp) == LBN_SELCHANGE && g_callbacks.on_select_shader != NULL) {
                        int index = (int)SendDlgItemMessageA(hwnd, IDC_SHADER_LIST, LB_GETCURSEL, 0, 0);
                        if (index >= 0) {
                            g_callbacks.on_select_shader(index, g_user_data);
                        }
                    }
                    return TRUE;

                case IDC_BUTTON_EXECUTE:
                    if (g_callbacks.on_execute_shader != NULL) {
                        g_callbacks.on_execute_shader(g_user_data);
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

        case WM_NOTIFY: {
            const NMHDR *notify = (const NMHDR *)lp;

            if (notify != NULL &&
                notify->idFrom == IDC_STATUS_BLURB &&
                (notify->code == NM_CLICK || notify->code == NM_RETURN))
            {
                const PNMLINK link = (const PNMLINK)lp;
                ShellExecuteW(hwnd, L"open", link->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            break;
        }

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
    common_controls.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
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

void stats_set_shader_catalog(const shader_desc_t *catalog, int count)
{
    HWND list_box;

    if (g_stats_hwnd == NULL) {
        return;
    }

    list_box = GetDlgItem(g_stats_hwnd, IDC_SHADER_LIST);
    if (list_box == NULL) {
        return;
    }

    SendMessageA(list_box, LB_RESETCONTENT, 0, 0);
    g_catalog_count = 0;

    if (catalog == NULL || count <= 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
        const char *label = catalog[i].display_name != NULL ? catalog[i].display_name : catalog[i].id;
        SendMessageA(list_box, LB_ADDSTRING, 0, (LPARAM)label);
        g_catalog_count++;
    }
}

void stats_set_selected_shader(int index)
{
    HWND list_box;

    if (g_stats_hwnd == NULL) {
        return;
    }

    list_box = GetDlgItem(g_stats_hwnd, IDC_SHADER_LIST);
    if (list_box == NULL) {
        return;
    }

    if (index < 0 || index >= g_catalog_count) {
        SendMessageA(list_box, LB_SETCURSEL, (WPARAM)-1, 0);
        return;
    }

    SendMessageA(list_box, LB_SETCURSEL, (WPARAM)index, 0);
}

void stats_update(const stats_state_t *state)
{
    char shader_text[128];
    char expect_text[256];
    char blurb_text[768];
    char render_text[64];
    char perf_text[128];
    char time_text[64];
    char workers_text[64];
    char display_text[128];
    char execute_text[32];
    float popup_scale;

    if (g_stats_hwnd == NULL || state == NULL) {
        return;
    }

    snprintf(shader_text, sizeof(shader_text), "%s", state->shader_name != NULL ? state->shader_name : "unknown");
    snprintf(expect_text, sizeof(expect_text), "%s", state->selected_expectations != NULL ? state->selected_expectations : "Select a shader.");
    snprintf(blurb_text, sizeof(blurb_text), "%s", state->selected_blurb != NULL ? state->selected_blurb : "Select a shader to inspect its details.");
    snprintf(perf_text, sizeof(perf_text), "%.1f fps, %.2f ms avg/%d", state->fps, state->milliseconds, state->frame_sample_count);
    snprintf(time_text, sizeof(time_text), "%.2f s", state->time_seconds);
    snprintf(workers_text, sizeof(workers_text), "%d render + gui", state->total_workers);
    if (state->render_width > 0 && state->render_height > 0) {
        snprintf(render_text, sizeof(render_text), "%d x %d", state->render_width, state->render_height);
    } else {
        snprintf(render_text, sizeof(render_text), "not running");
    }

    if (state->render_width > 0 && state->render_height > 0 && state->popup_width > 0 && state->popup_height > 0) {
        popup_scale = (float)state->popup_width / (float)state->render_width;
        snprintf(
            display_text,
            sizeof(display_text),
            (fabsf(popup_scale - 1.0f) > 0.001f) ? "%d x %d | x%.2f | right-drag to move" : "%d x %d | right-drag to move",
            state->popup_width,
            state->popup_height,
            popup_scale);
    } else {
        snprintf(display_text, sizeof(display_text), "not running");
    }
    snprintf(execute_text, sizeof(execute_text), "%s", state->shader_running ? "Stop" : "Execute");

    stats_update_text(IDC_STATUS_SHADER, g_shader_cache, sizeof(g_shader_cache), shader_text);
    stats_update_text(IDC_STATUS_EXPECT, g_expect_cache, sizeof(g_expect_cache), expect_text);
    stats_update_text(IDC_STATUS_BLURB, g_blurb_cache, sizeof(g_blurb_cache), blurb_text);
    stats_update_text(IDC_STATUS_RENDER, g_render_cache, sizeof(g_render_cache), render_text);
    stats_update_text(IDC_STATUS_PERF, g_perf_cache, sizeof(g_perf_cache), perf_text);
    stats_update_text(IDC_STATUS_TIME, g_time_cache, sizeof(g_time_cache), time_text);
    stats_update_text(IDC_STATUS_WORKERS, g_workers_cache, sizeof(g_workers_cache), workers_text);
    stats_update_text(IDC_STATUS_DISPLAY, g_display_cache, sizeof(g_display_cache), display_text);
    if (strcmp(g_execute_cache, execute_text) != 0) {
        SetDlgItemTextA(g_stats_hwnd, IDC_BUTTON_EXECUTE, execute_text);
        snprintf(g_execute_cache, sizeof(g_execute_cache), "%s", execute_text);
    }

    EnableWindow(GetDlgItem(g_stats_hwnd, IDC_BUTTON_EXECUTE), state->can_execute ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_stats_hwnd, IDC_BUTTON_RESET), state->can_reset ? TRUE : FALSE);

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
