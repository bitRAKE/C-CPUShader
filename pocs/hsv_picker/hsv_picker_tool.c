//Standalone HSV picker tool using the shared shader logic

#include "../../src/defines.h"
#include "hsv_picker.h"

#define TOOL_WINDOW_CLASS "HsvPickerToolWindow"
#define TOOL_RENDER_WIDTH 256
#define TOOL_RENDER_HEIGHT 256
#define TOOL_CLIENT_WIDTH 384
#define TOOL_CLIENT_HEIGHT 384

typedef struct {
    HWND                hwnd;
    HDC                 memory_dc;
    HBITMAP             dib_bitmap;
    HGDIOBJ             old_bitmap;
    BITMAPINFO          bitmap_info;
    unsigned int       *pixels;
    shader_uniforms_t   uniforms;
    hsv_picker_state_t  picker_state;
    hsv_picker_region_t active_region;
    bool                left_button_down;
    vec2_t              mouse_anchor;
    vec2_t              mouse_current;
} picker_tool_t;

static picker_tool_t g_tool = {0};

static vec2_t clamp_render_pixel(vec2_t pixel)
{
    if (pixel.x < 0.0f) {
        pixel.x = 0.0f;
    } else if (pixel.x > (float)(TOOL_RENDER_WIDTH - 1)) {
        pixel.x = (float)(TOOL_RENDER_WIDTH - 1);
    }

    if (pixel.y < 0.0f) {
        pixel.y = 0.0f;
    } else if (pixel.y > (float)(TOOL_RENDER_HEIGHT - 1)) {
        pixel.y = (float)(TOOL_RENDER_HEIGHT - 1);
    }

    return pixel;
}

static vec2_t map_client_to_render(HWND hwnd, LPARAM lp)
{
    RECT client_rect = {0};
    vec2_t pixel = vec2_o(0.0f);
    int client_width;
    int client_height;
    int x = (int)(short)LOWORD(lp);
    int y = (int)(short)HIWORD(lp);

    if (!GetClientRect(hwnd, &client_rect)) {
        return pixel;
    }

    client_width = client_rect.right - client_rect.left;
    client_height = client_rect.bottom - client_rect.top;
    if (client_width <= 0 || client_height <= 0) {
        return pixel;
    }

    pixel.x = ((float)x / (float)client_width) * (float)TOOL_RENDER_WIDTH;
    pixel.y = ((float)(client_height - 1 - y) / (float)client_height) * (float)TOOL_RENDER_HEIGHT;
    return clamp_render_pixel(pixel);
}

static void set_hover_mouse(vec2_t pixel)
{
    g_tool.uniforms.mouse.x = pixel.x;
    g_tool.uniforms.mouse.y = pixel.y;
    g_tool.uniforms.mouse.z = -1.0f;
    g_tool.uniforms.mouse.w = -1.0f;
}

static void set_drag_mouse(void)
{
    g_tool.uniforms.mouse.x = g_tool.mouse_anchor.x;
    g_tool.uniforms.mouse.y = g_tool.mouse_anchor.y;
    g_tool.uniforms.mouse.z = g_tool.mouse_current.x;
    g_tool.uniforms.mouse.w = g_tool.mouse_current.y;
}

static unsigned int pack_bgra8(vec4_t color)
{
    unsigned int r = (unsigned int)(saturate(color.x) * 255.0f + 0.5f);
    unsigned int g = (unsigned int)(saturate(color.y) * 255.0f + 0.5f);
    unsigned int b = (unsigned int)(saturate(color.z) * 255.0f + 0.5f);

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static void render_picker_surface(void)
{
    if (g_tool.pixels == NULL) {
        return;
    }

    for (int y = 0; y < TOOL_RENDER_HEIGHT; y++) {
        size_t row_base = (size_t)y * (size_t)TOOL_RENDER_WIDTH;

        for (int x = 0; x < TOOL_RENDER_WIDTH; x++) {
            vec4_t color = hsv_picker_render(
                vec2((float)x, (float)y),
                &g_tool.uniforms,
                &g_tool.picker_state,
                g_tool.active_region);

            g_tool.pixels[row_base + (size_t)x] = pack_bgra8(color);
        }
    }
}

static void present_picker(HDC hdc)
{
    RECT client_rect = {0};
    int client_width;
    int client_height;

    if (hdc == NULL || g_tool.memory_dc == NULL || g_tool.dib_bitmap == NULL || g_tool.pixels == NULL) {
        return;
    }

    render_picker_surface();

    if (!GetClientRect(g_tool.hwnd, &client_rect)) {
        return;
    }

    client_width = client_rect.right - client_rect.left;
    client_height = client_rect.bottom - client_rect.top;

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(
        hdc,
        0,
        0,
        client_width,
        client_height,
        g_tool.memory_dc,
        0,
        0,
        TOOL_RENDER_WIDTH,
        TOOL_RENDER_HEIGHT,
        SRCCOPY);

    g_tool.uniforms.frame++;
}

static bool create_render_surface(void)
{
    HDC window_dc;

    ZeroMemory(&g_tool.bitmap_info, sizeof(g_tool.bitmap_info));
    g_tool.bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_tool.bitmap_info.bmiHeader.biWidth = TOOL_RENDER_WIDTH;
    g_tool.bitmap_info.bmiHeader.biHeight = TOOL_RENDER_HEIGHT;
    g_tool.bitmap_info.bmiHeader.biPlanes = 1;
    g_tool.bitmap_info.bmiHeader.biBitCount = 32;
    g_tool.bitmap_info.bmiHeader.biCompression = BI_RGB;

    window_dc = GetDC(g_tool.hwnd);
    if (window_dc == NULL) {
        return false;
    }

    g_tool.memory_dc = CreateCompatibleDC(window_dc);
    g_tool.dib_bitmap = CreateDIBSection(
        window_dc,
        &g_tool.bitmap_info,
        DIB_RGB_COLORS,
        (void **)&g_tool.pixels,
        NULL,
        0);
    ReleaseDC(g_tool.hwnd, window_dc);

    if (g_tool.memory_dc == NULL || g_tool.dib_bitmap == NULL || g_tool.pixels == NULL) {
        return false;
    }

    g_tool.old_bitmap = SelectObject(g_tool.memory_dc, g_tool.dib_bitmap);
    return true;
}

static void destroy_render_surface(void)
{
    if (g_tool.memory_dc != NULL && g_tool.old_bitmap != NULL) {
        SelectObject(g_tool.memory_dc, g_tool.old_bitmap);
        g_tool.old_bitmap = NULL;
    }

    if (g_tool.dib_bitmap != NULL) {
        DeleteObject(g_tool.dib_bitmap);
        g_tool.dib_bitmap = NULL;
    }

    if (g_tool.memory_dc != NULL) {
        DeleteDC(g_tool.memory_dc);
        g_tool.memory_dc = NULL;
    }

    g_tool.pixels = NULL;
}

static void apply_interaction(void)
{
    if (g_tool.active_region != HSV_PICKER_REGION_NONE) {
        g_tool.picker_state = hsv_picker_preview_state(&g_tool.uniforms, &g_tool.picker_state, g_tool.active_region);
    }
}

static void request_redraw(void)
{
    InvalidateRect(g_tool.hwnd, NULL, TRUE);
}

static LRESULT CALLBACK PickerWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_MOUSEMOVE: {
            vec2_t pixel = map_client_to_render(hwnd, lp);

            g_tool.mouse_current = pixel;
            if (g_tool.left_button_down) {
                set_drag_mouse();
                apply_interaction();
            } else {
                set_hover_mouse(pixel);
            }

            request_redraw();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            vec2_t pixel = map_client_to_render(hwnd, lp);

            SetCapture(hwnd);
            g_tool.left_button_down = true;
            g_tool.mouse_anchor = pixel;
            g_tool.mouse_current = pixel;
            g_tool.active_region = hsv_picker_hit_test(pixel, g_tool.uniforms.resolution, &g_tool.picker_state);
            set_drag_mouse();
            apply_interaction();
            request_redraw();
            return 0;
        }

        case WM_LBUTTONUP: {
            vec2_t pixel = map_client_to_render(hwnd, lp);

            g_tool.mouse_current = pixel;
            set_drag_mouse();
            apply_interaction();
            g_tool.left_button_down = false;
            g_tool.active_region = HSV_PICKER_REGION_NONE;
            ReleaseCapture();
            set_hover_mouse(pixel);
            request_redraw();
            return 0;
        }

        case WM_CAPTURECHANGED:
            if (g_tool.left_button_down) {
                g_tool.left_button_down = false;
                g_tool.active_region = HSV_PICKER_REGION_NONE;
                g_tool.uniforms.mouse.z = -1.0f;
                g_tool.uniforms.mouse.w = -1.0f;
                request_redraw();
            }
            return 0;

        case WM_ERASEBKGND:
            present_picker((HDC)wp);
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;

            BeginPaint(hwnd, &ps);
            SendMessageA(hwnd, WM_ERASEBKGND, (WPARAM)ps.hdc, 0);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd)
{
    WNDCLASSA window_class = {0};
    RECT client_rect = {0, 0, TOOL_CLIENT_WIDTH, TOOL_CLIENT_HEIGHT};
    MSG msg;

    (void)prev_instance;
    (void)cmd_line;
    (void)show_cmd;

    g_tool.uniforms.resolution = vec2((float)TOOL_RENDER_WIDTH, (float)TOOL_RENDER_HEIGHT);
    g_tool.uniforms.time = 0.0f;
    g_tool.uniforms.frame = 0;
    g_tool.uniforms.mouse = vec4(-1.0f, -1.0f, -1.0f, -1.0f);
    g_tool.picker_state = hsv_picker_default_state();
    g_tool.active_region = HSV_PICKER_REGION_NONE;

    window_class.lpfnWndProc = PickerWndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = TOOL_WINDOW_CLASS;
    window_class.hbrBackground = 0; // Note: need WM_ERASEBKGND messages
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 1;
    }

    AdjustWindowRectEx(&client_rect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_TOOLWINDOW);
    g_tool.hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        TOOL_WINDOW_CLASS,
        "HSV Picker Tool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top,
        NULL,
        NULL,
        instance,
        NULL);

    if (g_tool.hwnd == NULL) {
        return 1;
    }

    if (!create_render_surface()) {
        DestroyWindow(g_tool.hwnd);
        return 1;
    }

    ShowWindow(g_tool.hwnd, SW_SHOW);
    UpdateWindow(g_tool.hwnd);
    request_redraw();

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    destroy_render_surface();
    return (int)msg.wParam;
}
