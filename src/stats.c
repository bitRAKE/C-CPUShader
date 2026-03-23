#include "defines.h"
#include "stats.h"
#include "resource.h"

#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <string.h>

#define MAX_TREE_ITEMS 256

#define STATS_DIAGNOSTIC_LIMIT 48
#define STATS_INFO_COLOR RGB(78, 106, 156)

typedef struct {
    char     text[512];
    COLORREF color;
} stats_message_t;

static HWND              g_stats_hwnd = NULL;
static stats_callbacks_t g_callbacks = {0};
static void             *g_user_data = NULL;
static char              g_title_base[128];
static char              g_backend_name[32];
static HMODULE           g_msftedit_module = NULL;

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
static HTREEITEM         g_tree_items[MAX_TREE_ITEMS];
static int               g_tree_item_count = 0;
static stats_message_t    g_diagnostic_messages[STATS_DIAGNOSTIC_LIMIT];
static int               g_diagnostic_count = 0;
static const char       *g_default_help_lines[] = {
    "Select a shader, inspect its description, then execute.",
    "Execute switches to Stop while a shader is running.",
    "Capture writes sequential PNG frames to the captures directory.",
    "Set the small count field beside Capture to choose sequential frames.",
    "Capture while idle arms the next Execute to start from frame 0.",
    "PgUp and PgDn change the display multiplier level.",
    "V toggles vsync, R resets the active shader, Esc exits.",
    "Right mouse drag moves the display window.",
};

static bool stats_append_wide_text(HWND diagnostics, const WCHAR *text, COLORREF color)
{
    CHARFORMAT2W format;

    if (diagnostics == NULL || text == NULL) {
        return false;
    }

    ZeroMemory(&format, sizeof(format));
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color;

    SendMessageW(diagnostics, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(diagnostics, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    SendMessageW(diagnostics, EM_REPLACESEL, FALSE, (LPARAM)text);
    return true;
}

static bool stats_append_text(HWND diagnostics, const char *text, COLORREF color)
{
    WCHAR wide_text[1024];

    if (diagnostics == NULL || text == NULL) {
        return false;
    }

    if (MultiByteToWideChar(CP_ACP, 0, text, -1, wide_text, (int)(sizeof(wide_text) / sizeof(wide_text[0]))) <= 0) {
        return false;
    }

    return stats_append_wide_text(diagnostics, wide_text, color);
}

static void stats_refresh_caption(void)
{
    char caption[256];

    if (g_stats_hwnd == NULL) {
        return;
    }

    if (g_backend_name[0] != '\0') {
        snprintf(caption, sizeof(caption), "%s Status [Presentation via %s]", g_title_base, g_backend_name);
    } else {
        snprintf(caption, sizeof(caption), "%s Status", g_title_base);
    }

    SetWindowTextA(g_stats_hwnd, caption);
}

static void stats_refresh_diagnostics(void)
{
    HWND diagnostics;
    static const WCHAR kBlankLine[] = L"\r\n";

    if (g_stats_hwnd == NULL) {
        return;
    }

    diagnostics = GetDlgItem(g_stats_hwnd, IDC_STATUS_DIAGNOSTICS);
    if (diagnostics == NULL) {
        return;
    }

    SendMessageW(diagnostics, EM_SETBKGNDCOLOR, 0, GetSysColor(COLOR_3DFACE));
    SetWindowTextW(diagnostics, L"");

    for (int index = 0; index < g_diagnostic_count; index++) {
        stats_append_text(diagnostics, g_diagnostic_messages[index].text, g_diagnostic_messages[index].color);
        stats_append_wide_text(diagnostics, L"\r\n", g_diagnostic_messages[index].color);
    }

    if (g_diagnostic_count > 0) {
        stats_append_wide_text(diagnostics, kBlankLine, STATS_INFO_COLOR);
    }

    for (int index = 0; index < (int)(sizeof(g_default_help_lines) / sizeof(g_default_help_lines[0])); index++) {
        stats_append_text(diagnostics, g_default_help_lines[index], STATS_INFO_COLOR);
        stats_append_wide_text(diagnostics, L"\r\n", STATS_INFO_COLOR);
    }

    SendMessageW(diagnostics, EM_SETSEL, 0, 0);
    SendMessageW(diagnostics, EM_SCROLLCARET, 0, 0);
}

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
    g_diagnostic_count = 0;
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
            SendMessageW(GetDlgItem(hwnd, IDC_STATUS_DIAGNOSTICS), EM_SETBKGNDCOLOR, 0, GetSysColor(COLOR_3DFACE));
            SetDlgItemTextA(hwnd, IDC_EDIT_CAPTURE_COUNT, "1");
            return TRUE;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            if ((HWND)lp == GetDlgItem(hwnd, IDC_STATUS_DIAGNOSTICS)) {
                HDC dc = (HDC)wp;
                SetBkColor(dc, GetSysColor(COLOR_3DFACE));
                SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
                return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_BUTTON_EXECUTE:
                    if (g_callbacks.on_execute_shader != NULL) {
                        g_callbacks.on_execute_shader(g_user_data);
                    }
                    return TRUE;

                case IDC_BUTTON_CAPTURE:
                    if (g_callbacks.on_capture != NULL) {
                        BOOL translated = FALSE;
                        UINT value = GetDlgItemInt(hwnd, IDC_EDIT_CAPTURE_COUNT, &translated, FALSE);
                        int capture_count = translated ? (int)value : 0;
                        g_callbacks.on_capture(capture_count, g_user_data);
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

            if (notify != NULL &&
                notify->idFrom == IDC_SHADER_TREE &&
                (notify->code == TVN_SELCHANGEDA || notify->code == TVN_SELCHANGEDW))
            {
                const NMTREEVIEWA *tv = (const NMTREEVIEWA *)lp;
                if (tv->itemNew.lParam >= 0 && g_callbacks.on_select_shader != NULL) {
                    g_callbacks.on_select_shader((int)tv->itemNew.lParam, g_user_data);
                }
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

    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&common_controls);

    if (g_msftedit_module == NULL) {
        g_msftedit_module = LoadLibraryA("Msftedit.dll");
        if (g_msftedit_module == NULL) {
            return false;
        }
    }

    if (callbacks != NULL) {
        g_callbacks = *callbacks;
    } else {
        ZeroMemory(&g_callbacks, sizeof(g_callbacks));
    }
    g_user_data = user_data;
    stats_reset_cache();
    snprintf(g_title_base, sizeof(g_title_base), "%s", title != NULL ? title : "Renderer");
    g_backend_name[0] = '\0';

    g_stats_hwnd = CreateDialogParamA(instance, MAKEINTRESOURCEA(IDD_STATUS_DIALOG), NULL, StatsDlgProc, 0);
    if (g_stats_hwnd == NULL) {
        return false;
    }

    stats_refresh_caption();
    stats_refresh_diagnostics();
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

void stats_set_backend_name(const char *backend_name)
{
    snprintf(g_backend_name, sizeof(g_backend_name), "%s", backend_name != NULL ? backend_name : "");
    stats_refresh_caption();
}

void stats_prepend_message(const char *text, COLORREF color)
{
    char combined[4096];
    SYSTEMTIME local_time;
    int index;

    if (text == NULL || text[0] == '\0') {
        return;
    }

    GetLocalTime(&local_time);
    snprintf(
        combined,
        sizeof(combined),
        "[%02u:%02u:%02u] %s",
        (unsigned)local_time.wHour,
        (unsigned)local_time.wMinute,
        (unsigned)local_time.wSecond,
        text);

    if (g_diagnostic_count == STATS_DIAGNOSTIC_LIMIT) {
        g_diagnostic_count--;
    }

    for (index = g_diagnostic_count; index > 0; index--) {
        g_diagnostic_messages[index] = g_diagnostic_messages[index - 1];
    }

    snprintf(g_diagnostic_messages[0].text, sizeof(g_diagnostic_messages[0].text), "%s", combined);
    g_diagnostic_messages[0].color = color;
    g_diagnostic_count++;
    stats_refresh_diagnostics();
}

void stats_clear_diagnostics(void)
{
    g_diagnostic_count = 0;
    stats_refresh_diagnostics();
}

void stats_set_shader_catalog(const shader_desc_t *catalog, int count)
{
    /* Legacy flat population -- puts all shaders under a single "Shaders" root. */
    HWND tree;
    TVINSERTSTRUCTA insert;
    HTREEITEM branch;

    if (g_stats_hwnd == NULL) {
        return;
    }

    tree = GetDlgItem(g_stats_hwnd, IDC_SHADER_TREE);
    if (tree == NULL) {
        return;
    }

    TreeView_DeleteAllItems(tree);
    g_catalog_count = 0;
    g_tree_item_count = 0;

    if (catalog == NULL || count <= 0) {
        return;
    }

    /* Insert a single root branch. */
    ZeroMemory(&insert, sizeof(insert));
    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    insert.item.pszText = (LPSTR)"Shaders";
    insert.item.lParam = (LPARAM)-1;
    branch = (HTREEITEM)SendMessageA(tree, TVM_INSERTITEMA, 0, (LPARAM)&insert);

    for (int i = 0; i < count && g_tree_item_count < MAX_TREE_ITEMS; i++) {
        const char *label = catalog[i].display_name != NULL ? catalog[i].display_name : catalog[i].id;
        ZeroMemory(&insert, sizeof(insert));
        insert.hParent = branch;
        insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM;
        insert.item.pszText = (LPSTR)label;
        insert.item.lParam = (LPARAM)i;
        g_tree_items[g_tree_item_count] = (HTREEITEM)SendMessageA(tree, TVM_INSERTITEMA, 0, (LPARAM)&insert);
        g_tree_item_count++;
        g_catalog_count++;
    }

    TreeView_Expand(tree, branch, TVE_EXPAND);
}

void stats_set_shader_catalog_grouped(
    const shader_desc_t *shaders, int shader_count,
    const shader_collection_t *collections, int collection_count,
    int (*get_collection)(int shader_index))
{
    HWND tree;
    TVINSERTSTRUCTA insert;
    HTREEITEM branches[64];  /* max collection branches */

    if (g_stats_hwnd == NULL) {
        return;
    }

    tree = GetDlgItem(g_stats_hwnd, IDC_SHADER_TREE);
    if (tree == NULL) {
        return;
    }

    TreeView_DeleteAllItems(tree);
    g_catalog_count = 0;
    g_tree_item_count = 0;

    if (shaders == NULL || shader_count <= 0) {
        return;
    }

    /* Insert collection branches. */
    for (int c = 0; c < collection_count && c < 64; c++) {
        const char *name = collections[c].name != NULL ? collections[c].name : "Unknown";
        ZeroMemory(&insert, sizeof(insert));
        insert.hParent = TVI_ROOT;
        insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM;
        insert.item.pszText = (LPSTR)name;
        insert.item.lParam = (LPARAM)-1;
        branches[c] = (HTREEITEM)SendMessageA(tree, TVM_INSERTITEMA, 0, (LPARAM)&insert);
    }

    /* Insert shader leaves under their collection branches. */
    for (int i = 0; i < shader_count && g_tree_item_count < MAX_TREE_ITEMS; i++) {
        int col = get_collection(i);
        HTREEITEM parent = (col >= 0 && col < collection_count) ? branches[col] : TVI_ROOT;
        const char *label = shaders[i].display_name != NULL ? shaders[i].display_name : shaders[i].id;

        ZeroMemory(&insert, sizeof(insert));
        insert.hParent = parent;
        insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM;
        insert.item.pszText = (LPSTR)label;
        insert.item.lParam = (LPARAM)i;
        g_tree_items[g_tree_item_count] = (HTREEITEM)SendMessageA(tree, TVM_INSERTITEMA, 0, (LPARAM)&insert);
        g_tree_item_count++;
        g_catalog_count++;
    }

    /* Expand all collection branches so shaders are visible. */
    for (int c = 0; c < collection_count && c < 64; c++) {
        TreeView_Expand(tree, branches[c], TVE_EXPAND);
    }
}

void stats_set_selected_shader(int index)
{
    HWND tree;

    if (g_stats_hwnd == NULL) {
        return;
    }

    tree = GetDlgItem(g_stats_hwnd, IDC_SHADER_TREE);
    if (tree == NULL) {
        return;
    }

    if (index < 0 || index >= g_tree_item_count) {
        TreeView_SelectItem(tree, NULL);
        return;
    }

    TreeView_SelectItem(tree, g_tree_items[index]);
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
        if (state->display_multiplier > 1 && fabsf(popup_scale - (float)state->display_multiplier) > 0.001f) {
            snprintf(
                display_text,
                sizeof(display_text),
                "%d x %d | x%.2f (req x%d) | right-drag to move",
                state->popup_width,
                state->popup_height,
                popup_scale,
                state->display_multiplier);
        } else if (fabsf(popup_scale - 1.0f) > 0.001f) {
            snprintf(
                display_text,
                sizeof(display_text),
                "%d x %d | x%.2f | right-drag to move",
                state->popup_width,
                state->popup_height,
                popup_scale);
        } else {
            snprintf(
                display_text,
                sizeof(display_text),
                "%d x %d | right-drag to move",
                state->popup_width,
                state->popup_height);
        }
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
    EnableWindow(GetDlgItem(g_stats_hwnd, IDC_BUTTON_CAPTURE), state->can_capture ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_stats_hwnd, IDC_EDIT_CAPTURE_COUNT), TRUE);
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
