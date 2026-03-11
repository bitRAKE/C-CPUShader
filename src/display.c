#include "defines.h"
#include "display.h"

#define DISPLAY_WINDOW_CLASS "RendererDisplayWindow"
#define DISPLAY_WINDOW_STYLE WS_POPUP
#define DISPLAY_WINDOW_EX_STYLE WS_EX_TOOLWINDOW

static HWND                g_display_hwnd = NULL;
static display_callbacks_t g_callbacks = {0};
static void               *g_user_data = NULL;
static bool                g_drag_active = false;
static bool                g_left_button_down = false;
static bool                g_has_mouse_position = false;
static POINT               g_drag_cursor = {0};
static POINT               g_drag_origin = {0};
static POINT               g_mouse_current = {0};
static POINT               g_mouse_anchor = {0};

static void sync_capture(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }

    if (g_drag_active || g_left_button_down) {
        if (GetCapture() != hwnd) {
            SetCapture(hwnd);
        }
    } else if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

static POINT clamp_client_point(POINT point)
{
    RECT rect = {0};

    if (g_display_hwnd != NULL && GetClientRect(g_display_hwnd, &rect)) {
        if (rect.right > 0) {
            point.x = max(0, min(point.x, rect.right - 1));
        }
        if (rect.bottom > 0) {
            point.y = max(0, min(point.y, rect.bottom - 1));
        }
    }

    return point;
}

static void update_mouse_from_lparam(LPARAM lp)
{
    g_mouse_current.x = (int)(short)LOWORD(lp);
    g_mouse_current.y = (int)(short)HIWORD(lp);
    g_has_mouse_position = true;
}

static void clear_drag(void)
{
    if (g_drag_active) {
        g_drag_active = false;
        sync_capture(g_display_hwnd);
    }
}

static void begin_drag(HWND hwnd)
{
    RECT rect;

    if (hwnd == NULL) {
        return;
    }

    GetCursorPos(&g_drag_cursor);
    GetWindowRect(hwnd, &rect);
    g_drag_origin.x = rect.left;
    g_drag_origin.y = rect.top;
    g_drag_active = true;
    sync_capture(hwnd);
    SetFocus(hwnd);
}

static void continue_drag(HWND hwnd)
{
    POINT cursor;
    int delta_x;
    int delta_y;

    if (!g_drag_active || hwnd == NULL) {
        return;
    }

    GetCursorPos(&cursor);
    delta_x = cursor.x - g_drag_cursor.x;
    delta_y = cursor.y - g_drag_cursor.y;

    SetWindowPos(
        hwnd,
        NULL,
        g_drag_origin.x + delta_x,
        g_drag_origin.y + delta_y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK DisplayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;

    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (g_callbacks.on_keydown != NULL && g_callbacks.on_keydown(hwnd, wp, g_user_data)) {
                return 0;
            }
            break;

        case WM_RBUTTONDOWN:
            begin_drag(hwnd);
            return 0;

        case WM_LBUTTONDOWN:
            update_mouse_from_lparam(lp);
            g_mouse_anchor = g_mouse_current;
            g_left_button_down = true;
            sync_capture(hwnd);
            SetFocus(hwnd);
            return 0;

        case WM_LBUTTONUP:
            update_mouse_from_lparam(lp);
            g_left_button_down = false;
            sync_capture(hwnd);
            return 0;

        case WM_MOUSEMOVE:
            update_mouse_from_lparam(lp);
            continue_drag(hwnd);
            return 0;

        case WM_RBUTTONUP:
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            g_left_button_down = false;
            clear_drag();
            sync_capture(hwnd);
            return 0;

        case WM_CONTEXTMENU:
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CLOSE:
            if (g_callbacks.on_close != NULL) {
                g_callbacks.on_close(hwnd, g_user_data);
            }
            return 0;

        case WM_DESTROY:
            if (hwnd == g_display_hwnd) {
                g_display_hwnd = NULL;
            }
            clear_drag();
            g_left_button_down = false;
            g_has_mouse_position = false;
            return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

static bool register_display_class(HINSTANCE instance)
{
    WNDCLASSA wc = {0};

    wc.lpfnWndProc = DisplayWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = DISPLAY_WINDOW_CLASS;

    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    return true;
}

bool display_create(
    HINSTANCE instance,
    const char *title,
    HWND owner,
    int x,
    int y,
    int width,
    int height,
    const display_callbacks_t *callbacks,
    void *user_data)
{
    if (!register_display_class(instance)) {
        return false;
    }

    if (callbacks != NULL) {
        g_callbacks = *callbacks;
    } else {
        ZeroMemory(&g_callbacks, sizeof(g_callbacks));
    }
    g_user_data = user_data;

    g_display_hwnd = CreateWindowExA(
        DISPLAY_WINDOW_EX_STYLE,
        DISPLAY_WINDOW_CLASS,
        title,
        DISPLAY_WINDOW_STYLE,
        x,
        y,
        width,
        height,
        owner,
        NULL,
        instance,
        NULL);

    return g_display_hwnd != NULL;
}

void display_destroy(void)
{
    HWND hwnd = g_display_hwnd;

    if (hwnd != NULL && IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }
}

void display_show(void)
{
    if (g_display_hwnd != NULL) {
        ShowWindow(g_display_hwnd, SW_SHOW);
        UpdateWindow(g_display_hwnd);
    }
}

void display_focus(void)
{
    if (g_display_hwnd != NULL) {
        SetForegroundWindow(g_display_hwnd);
        SetFocus(g_display_hwnd);
    }
}

void display_get_mouse_uniform(vec4_t *mouse_out, int render_height)
{
    vec4_t mouse = vec4(-1.0f, -1.0f, -1.0f, -1.0f);

    if (mouse_out == NULL) {
        return;
    }

    if (g_display_hwnd != NULL && g_has_mouse_position) {
        POINT current = clamp_client_point(g_mouse_current);

        if (g_left_button_down) {
            POINT anchor = clamp_client_point(g_mouse_anchor);

            mouse.x = (float)anchor.x;
            mouse.y = (float)(render_height - 1 - anchor.y);
            mouse.z = (float)current.x;
            mouse.w = (float)(render_height - 1 - current.y);
        } else {
            mouse.x = (float)current.x;
            mouse.y = (float)(render_height - 1 - current.y);
        }
    }

    *mouse_out = mouse;
}

bool display_get_window_rect(RECT *rect_out)
{
    if (g_display_hwnd == NULL || rect_out == NULL) {
        return false;
    }

    return GetWindowRect(g_display_hwnd, rect_out);
}

bool display_is_window(HWND hwnd)
{
    return hwnd != NULL && hwnd == g_display_hwnd;
}

HWND display_window(void)
{
    return g_display_hwnd;
}
