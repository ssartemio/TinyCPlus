#ifndef TC_GUI_IMPLEMENTATION
#define TC_GUI_IMPLEMENTATION
#include "tiny_runtime.h"
#ifdef _WIN32
#include <windows.h>
#endif

typedef uint32_t TcGuiPixel;

typedef struct TcGuiSurface {
    int32_t width, height, stride;
    TcGuiPixel *pixels;
    int dirty;
    int32_t x0, y0, x1, y1;
} TcGuiSurface;

typedef struct TcGuiBuffer {
    TcGuiSurface front;
    TcGuiSurface back;
} TcGuiBuffer;

static int tc_gui_valid_size(int32_t width, int32_t height) {
    size_t w, h;
    if (width < 1 || height < 1 || width > 16384 || height > 16384)
        return 0;
    w = (size_t)width;
    h = (size_t)height;
    return h <= SIZE_MAX / w && w * h <= SIZE_MAX / sizeof(TcGuiPixel);
}

static int tc_gui_surface_init(TcGuiSurface *surface, int32_t width, int32_t height) {
    size_t count;
    memset(surface, 0, sizeof(*surface));
    if (!tc_gui_valid_size(width, height))
        return 3;
    count = (size_t)width * (size_t)height;
    surface->pixels = (TcGuiPixel *)calloc(count, sizeof(TcGuiPixel));
    if (!surface->pixels)
        return 8;
    surface->width = width;
    surface->height = height;
    surface->stride = width;
    return 0;
}

static void tc_gui_surface_release(TcGuiSurface *surface) {
    free(surface->pixels);
    memset(surface, 0, sizeof(*surface));
}

static void tc_gui_damage(TcGuiSurface *surface, int32_t x, int32_t y, int32_t width, int32_t height) {
    int32_t x1, y1;
    if (!surface || width <= 0 || height <= 0)
        return;
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x >= surface->width || y >= surface->height || width <= 0 || height <= 0)
        return;
    if (width > surface->width - x)
        width = surface->width - x;
    if (height > surface->height - y)
        height = surface->height - y;
    x1 = x + width;
    y1 = y + height;
    if (!surface->dirty) {
        surface->x0 = x;
        surface->y0 = y;
        surface->x1 = x1;
        surface->y1 = y1;
        surface->dirty = 1;
    } else {
        if (x < surface->x0) surface->x0 = x;
        if (y < surface->y0) surface->y0 = y;
        if (x1 > surface->x1) surface->x1 = x1;
        if (y1 > surface->y1) surface->y1 = y1;
    }
}

uint32_t tc_gui_rgba(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha) {
    return ((alpha & 255u) << 24) | ((red & 255u) << 16) | ((green & 255u) << 8) | (blue & 255u);
}

void *tc_gui_surface_create(int32_t width, int32_t height, int32_t *error) {
    TcGuiSurface *surface = (TcGuiSurface *)malloc(sizeof(*surface));
    int result;
    if (error) *error = 0;
    if (!surface) {
        if (error) *error = 8;
        return NULL;
    }
    result = tc_gui_surface_init(surface, width, height);
    if (result) {
        if (error) *error = result;
        free(surface);
        return NULL;
    }
    return surface;
}

void tc_gui_surface_destroy(void *handle) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    if (!surface)
        return;
    tc_gui_surface_release(surface);
    free(surface);
}

int32_t tc_gui_surface_width(void *handle) {
    return handle ? ((TcGuiSurface *)handle)->width : 0;
}

int32_t tc_gui_surface_height(void *handle) {
    return handle ? ((TcGuiSurface *)handle)->height : 0;
}

void tc_gui_surface_clear(void *handle, uint32_t color) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    size_t i, count;
    if (!surface)
        return;
    count = (size_t)surface->width * (size_t)surface->height;
    for (i = 0; i < count; i++)
        surface->pixels[i] = color;
    tc_gui_damage(surface, 0, 0, surface->width, surface->height);
}

uint32_t tc_gui_surface_get(void *handle, int32_t x, int32_t y) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    if (!surface || x < 0 || y < 0 || x >= surface->width || y >= surface->height)
        return 0;
    return surface->pixels[(size_t)y * (size_t)surface->stride + (size_t)x];
}

void tc_gui_surface_set(void *handle, int32_t x, int32_t y, uint32_t color) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    if (!surface || x < 0 || y < 0 || x >= surface->width || y >= surface->height)
        return;
    surface->pixels[(size_t)y * (size_t)surface->stride + (size_t)x] = color;
    tc_gui_damage(surface, x, y, 1, 1);
}

void tc_gui_fill_rect(void *handle, int32_t x, int32_t y, int32_t width, int32_t height,
                      uint32_t color) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    int32_t row, column, x0 = x, y0 = y, x1, y1;
    if (!surface || width <= 0 || height <= 0)
        return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    x1 = x + width;
    y1 = y + height;
    if (x1 > surface->width) x1 = surface->width;
    if (y1 > surface->height) y1 = surface->height;
    if (x0 >= x1 || y0 >= y1)
        return;
    for (row = y0; row < y1; row++) {
        TcGuiPixel *pixels = surface->pixels + (size_t)row * (size_t)surface->stride;
        for (column = x0; column < x1; column++)
            pixels[column] = color;
    }
    tc_gui_damage(surface, x0, y0, x1 - x0, y1 - y0);
}

void tc_gui_rect(void *handle, int32_t x, int32_t y, int32_t width, int32_t height,
                 uint32_t color) {
    if (width <= 0 || height <= 0)
        return;
    tc_gui_fill_rect(handle, x, y, width, 1, color);
    if (height > 1)
        tc_gui_fill_rect(handle, x, y + height - 1, width, 1, color);
    if (height > 2) {
        tc_gui_fill_rect(handle, x, y + 1, 1, height - 2, color);
        if (width > 1)
            tc_gui_fill_rect(handle, x + width - 1, y + 1, 1, height - 2, color);
    }
}

void tc_gui_line(void *handle, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy_abs = y1 > y0 ? y1 - y0 : y0 - y1;
    int32_t dy = -dy_abs;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        tc_gui_surface_set(handle, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        {
            int32_t twice = error * 2;
            if (twice >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twice <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
}

void tc_gui_blit(void *destination_handle, int32_t dx, int32_t dy, void *source_handle,
                 int32_t sx, int32_t sy, int32_t width, int32_t height) {
    TcGuiSurface *destination = (TcGuiSurface *)destination_handle;
    TcGuiSurface *source = (TcGuiSurface *)source_handle;
    int32_t row, column;
    if (!destination || !source || width <= 0 || height <= 0)
        return;
    for (row = 0; row < height; row++)
        for (column = 0; column < width; column++) {
            int32_t tx = dx + column, ty = dy + row;
            int32_t ux = sx + column, uy = sy + row;
            if (tx < 0 || ty < 0 || tx >= destination->width || ty >= destination->height ||
                ux < 0 || uy < 0 || ux >= source->width || uy >= source->height)
                continue;
            destination->pixels[(size_t)ty * (size_t)destination->stride + (size_t)tx] =
                source->pixels[(size_t)uy * (size_t)source->stride + (size_t)ux];
        }
    tc_gui_damage(destination, dx, dy, width, height);
}

int32_t tc_gui_surface_damage(void *handle, int32_t *x, int32_t *y, int32_t *width, int32_t *height) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    if (!surface || !surface->dirty)
        return 0;
    if (x) *x = surface->x0;
    if (y) *y = surface->y0;
    if (width) *width = surface->x1 - surface->x0;
    if (height) *height = surface->y1 - surface->y0;
    return 1;
}

void tc_gui_surface_damage_clear(void *handle) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    if (surface)
        surface->dirty = 0;
}

uint64_t tc_gui_surface_checksum(void *handle) {
    TcGuiSurface *surface = (TcGuiSurface *)handle;
    uint64_t hash = 1469598103934665603ULL;
    size_t i, count;
    if (!surface)
        return 0;
    count = (size_t)surface->width * (size_t)surface->height;
    for (i = 0; i < count; i++) {
        uint32_t pixel = surface->pixels[i];
        int byte;
        for (byte = 0; byte < 4; byte++) {
            hash ^= (uint8_t)(pixel & 255u);
            hash *= 1099511628211ULL;
            pixel >>= 8;
        }
    }
    return hash;
}

void *tc_gui_buffer_create(int32_t width, int32_t height, int32_t *error) {
    TcGuiBuffer *buffer = (TcGuiBuffer *)calloc(1, sizeof(*buffer));
    int result;
    if (error) *error = 0;
    if (!buffer) {
        if (error) *error = 8;
        return NULL;
    }
    result = tc_gui_surface_init(&buffer->front, width, height);
    if (!result)
        result = tc_gui_surface_init(&buffer->back, width, height);
    if (result) {
        tc_gui_surface_release(&buffer->front);
        tc_gui_surface_release(&buffer->back);
        free(buffer);
        if (error) *error = result;
        return NULL;
    }
    return buffer;
}

void tc_gui_buffer_destroy(void *handle) {
    TcGuiBuffer *buffer = (TcGuiBuffer *)handle;
    if (!buffer)
        return;
    tc_gui_surface_release(&buffer->front);
    tc_gui_surface_release(&buffer->back);
    free(buffer);
}

void *tc_gui_buffer_front(void *handle) {
    return handle ? &((TcGuiBuffer *)handle)->front : NULL;
}

void *tc_gui_buffer_back(void *handle) {
    return handle ? &((TcGuiBuffer *)handle)->back : NULL;
}

int32_t tc_gui_buffer_present(void *handle) {
    TcGuiBuffer *buffer = (TcGuiBuffer *)handle;
    TcGuiSurface *front, *back;
    int32_t x, y, changed = 0;
    if (!buffer)
        return 0;
    front = &buffer->front;
    back = &buffer->back;
    if (!back->dirty)
        return 0;
    for (y = back->y0; y < back->y1; y++)
        for (x = back->x0; x < back->x1; x++) {
            size_t index = (size_t)y * (size_t)back->stride + (size_t)x;
            if (front->pixels[index] != back->pixels[index]) {
                front->pixels[index] = back->pixels[index];
                changed++;
            }
        }
    tc_gui_damage(front, back->x0, back->y0, back->x1 - back->x0, back->y1 - back->y0);
    back->dirty = 0;
    return changed;
}

int32_t tc_gui_buffer_resize(void *handle, int32_t width, int32_t height) {
    TcGuiBuffer *buffer = (TcGuiBuffer *)handle;
    TcGuiSurface front, back;
    int result;
    if (!buffer)
        return 3;
    result = tc_gui_surface_init(&front, width, height);
    if (result)
        return result;
    result = tc_gui_surface_init(&back, width, height);
    if (result) {
        tc_gui_surface_release(&front);
        return result;
    }
    tc_gui_surface_release(&buffer->front);
    tc_gui_surface_release(&buffer->back);
    buffer->front = front;
    buffer->back = back;
    return 0;
}

enum {
    TC_GUI_EVENT_NONE = 0,
    TC_GUI_EVENT_KEY = 1,
    TC_GUI_EVENT_TEXT = 2,
    TC_GUI_EVENT_MOUSE = 3,
    TC_GUI_EVENT_RESIZE = 4,
    TC_GUI_EVENT_CLOSE = 5,
    TC_GUI_EVENT_CUSTOM = 6
};

typedef struct TcGuiEvent {
    int32_t kind, key, x, y, width, height, button, pressed;
    uint32_t codepoint;
} TcGuiEvent;

typedef struct TcGuiWindow {
    TcGuiBuffer *buffer;
    int headless, open;
    TcGuiEvent events[64];
    unsigned event_read, event_write;
#ifdef _WIN32
    HWND hwnd;
    uint32_t surrogate;
#endif
} TcGuiWindow;

static int tc_gui_event_push(TcGuiWindow *window, TcGuiEvent event) {
    unsigned next;
    if (!window)
        return 3;
    next = (window->event_write + 1u) % 64u;
    if (next == window->event_read)
        return 8;
    window->events[window->event_write] = event;
    window->event_write = next;
    return 0;
}

static int tc_gui_event_pop(TcGuiWindow *window, TcGuiEvent *event) {
    if (!window || window->event_read == window->event_write)
        return 0;
    *event = window->events[window->event_read];
    window->event_read = (window->event_read + 1u) % 64u;
    return 1;
}

#ifdef _WIN32
static const char tc_gui_window_class[] = "TinyCPlusGuiWindow";
static ATOM tc_gui_window_atom;

static int32_t tc_gui_mouse_x(LPARAM value) {
    return (int32_t)(int16_t)(value & 0xffff);
}
static int32_t tc_gui_mouse_y(LPARAM value) {
    return (int32_t)(int16_t)((value >> 16) & 0xffff);
}
static void tc_gui_push_mouse(TcGuiWindow *window, LPARAM value, int button, int pressed) {
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_MOUSE;
    event.x = tc_gui_mouse_x(value);
    event.y = tc_gui_mouse_y(value);
    event.button = button;
    event.pressed = pressed;
    tc_gui_event_push(window, event);
}
static LRESULT CALLBACK tc_gui_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    TcGuiWindow *window = (TcGuiWindow *)(uintptr_t)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTA *create = (CREATESTRUCTA *)(uintptr_t)lparam;
        window = (TcGuiWindow *)create->lpCreateParams;
        window->hwnd = hwnd;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)(uintptr_t)window);
    }
    if (!window)
        return DefWindowProcA(hwnd, message, wparam, lparam);
    switch (message) {
    case WM_CLOSE: {
        TcGuiEvent event;
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_CLOSE;
        tc_gui_event_push(window, event);
        window->open = 0;
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        window->hwnd = NULL;
        window->open = 0;
        return 0;
    case WM_SIZE: {
        int width = (int)(uint16_t)(lparam & 0xffff);
        int height = (int)(uint16_t)((lparam >> 16) & 0xffff);
        if (width > 0 && height > 0 &&
            (width != window->buffer->front.width || height != window->buffer->front.height) &&
            tc_gui_buffer_resize(window->buffer, width, height) == 0) {
            TcGuiEvent event;
            memset(&event, 0, sizeof(event));
            event.kind = TC_GUI_EVENT_RESIZE;
            event.width = width;
            event.height = height;
            tc_gui_event_push(window, event);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        TcGuiEvent event;
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_KEY;
        event.key = (int32_t)wparam;
        tc_gui_event_push(window, event);
        return 0;
    }
    case WM_CHAR: {
        uint32_t cp = (uint32_t)wparam;
        if (cp >= 0xd800 && cp <= 0xdbff) {
            window->surrogate = cp;
            return 0;
        }
        if (cp >= 0xdc00 && cp <= 0xdfff && window->surrogate) {
            cp = 0x10000u + ((window->surrogate - 0xd800u) << 10) + (cp - 0xdc00u);
            window->surrogate = 0;
        } else
            window->surrogate = 0;
        {
            TcGuiEvent event;
            memset(&event, 0, sizeof(event));
            event.kind = TC_GUI_EVENT_TEXT;
            event.codepoint = cp;
            tc_gui_event_push(window, event);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        tc_gui_push_mouse(window, lparam, 0, 0);
        return 0;
    case WM_LBUTTONDOWN:
        tc_gui_push_mouse(window, lparam, 1, 1);
        return 0;
    case WM_LBUTTONUP:
        tc_gui_push_mouse(window, lparam, 1, 0);
        return 0;
    case WM_RBUTTONDOWN:
        tc_gui_push_mouse(window, lparam, 2, 1);
        return 0;
    case WM_RBUTTONUP:
        tc_gui_push_mouse(window, lparam, 2, 0);
        return 0;
    case WM_MBUTTONDOWN:
        tc_gui_push_mouse(window, lparam, 3, 1);
        return 0;
    case WM_MBUTTONUP:
        tc_gui_push_mouse(window, lparam, 3, 0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        TcGuiSurface *surface = &window->buffer->front;
        BITMAPINFO bitmap;
        memset(&bitmap, 0, sizeof(bitmap));
        bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
        bitmap.bmiHeader.biWidth = surface->width;
        bitmap.bmiHeader.biHeight = -surface->height;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(dc, 0, 0, surface->width, surface->height, 0, 0, surface->width,
                      surface->height, surface->pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(hwnd, &paint);
        tc_gui_surface_damage_clear(surface);
        return 0;
    }
    default:
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}
static int tc_gui_register_window(void) {
    WNDCLASSA type;
    if (tc_gui_window_atom)
        return 1;
    memset(&type, 0, sizeof(type));
    type.lpfnWndProc = tc_gui_window_proc;
    type.hInstance = GetModuleHandleA(NULL);
    type.hCursor = LoadCursorA(NULL, IDC_ARROW);
    type.lpszClassName = tc_gui_window_class;
    tc_gui_window_atom = RegisterClassA(&type);
    return tc_gui_window_atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
#endif

void *tc_gui_window_create(int32_t width, int32_t height, TinyString title, int32_t headless,
                           int32_t *error) {
    TcGuiWindow *window = (TcGuiWindow *)calloc(1, sizeof(*window));
    if (error) *error = 0;
    if (!window) {
        if (error) *error = 8;
        return NULL;
    }
    window->buffer = (TcGuiBuffer *)tc_gui_buffer_create(width, height, error);
    if (!window->buffer) {
        free(window);
        return NULL;
    }
    window->headless = !!headless;
    window->open = 1;
#ifdef _WIN32
    if (!window->headless) {
        RECT area = {0, 0, width, height};
        char *caption;
        if (!tc_gui_register_window()) {
            if (error) *error = 9;
            tc_gui_buffer_destroy(window->buffer);
            free(window);
            return NULL;
        }
        caption = (char *)malloc(title.length + 1);
        if (!caption) {
            if (error) *error = 8;
            tc_gui_buffer_destroy(window->buffer);
            free(window);
            return NULL;
        }
        memcpy(caption, title.data, title.length);
        caption[title.length] = 0;
        AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
        window->hwnd = CreateWindowExA(
            0, tc_gui_window_class, caption, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            area.right - area.left, area.bottom - area.top, NULL, NULL, GetModuleHandleA(NULL),
            window);
        free(caption);
        if (!window->hwnd) {
            if (error) *error = 9;
            tc_gui_buffer_destroy(window->buffer);
            free(window);
            return NULL;
        }
        ShowWindow(window->hwnd, SW_SHOW);
        UpdateWindow(window->hwnd);
    }
#else
    (void)title;
    if (!window->headless) {
        if (error) *error = 9;
        tc_gui_buffer_destroy(window->buffer);
        free(window);
        return NULL;
    }
#endif
    return window;
}

int32_t tc_gui_window_width(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    return window ? window->buffer->back.width : 0;
}
int32_t tc_gui_window_height(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    return window ? window->buffer->back.height : 0;
}
int32_t tc_gui_window_open(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    return window && window->open;
}
void *tc_gui_window_surface(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    return window ? &window->buffer->back : NULL;
}
int32_t tc_gui_window_present(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    int32_t changed;
    if (!window)
        return 0;
    changed = tc_gui_buffer_present(window->buffer);
#ifdef _WIN32
    if (!window->headless && window->hwnd && changed) {
        InvalidateRect(window->hwnd, NULL, FALSE);
        UpdateWindow(window->hwnd);
    }
#endif
    return changed;
}
int32_t tc_gui_window_post(void *handle, int32_t kind, int32_t key, int32_t x, int32_t y,
                           int32_t width, int32_t height, int32_t button, int32_t pressed,
                           uint32_t codepoint) {
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = kind;
    event.key = key;
    event.x = x;
    event.y = y;
    event.width = width;
    event.height = height;
    event.button = button;
    event.pressed = pressed;
    event.codepoint = codepoint;
    return tc_gui_event_push((TcGuiWindow *)handle, event);
}
void tc_gui_window_next(void *handle, int32_t timeout, int32_t *kind, int32_t *key, int32_t *x,
                        int32_t *y, int32_t *width, int32_t *height, int32_t *button,
                        int32_t *pressed, uint32_t *codepoint) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
#ifdef _WIN32
    if (window && !window->headless) {
        DWORD start = GetTickCount();
        for (;;) {
            MSG message;
            while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageA(&message);
                if (tc_gui_event_pop(window, &event))
                    goto ready;
            }
            if (tc_gui_event_pop(window, &event))
                goto ready;
            if (timeout <= 0 || (int32_t)(GetTickCount() - start) >= timeout)
                break;
            MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
        }
    } else
#endif
    if (window)
        tc_gui_event_pop(window, &event);
#ifdef _WIN32
ready:
#endif
    if (kind) *kind = event.kind;
    if (key) *key = event.key;
    if (x) *x = event.x;
    if (y) *y = event.y;
    if (width) *width = event.width;
    if (height) *height = event.height;
    if (button) *button = event.button;
    if (pressed) *pressed = event.pressed;
    if (codepoint) *codepoint = event.codepoint;
}
void tc_gui_window_close(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    if (!window)
        return;
    window->open = 0;
#ifdef _WIN32
    if (!window->headless && window->hwnd)
        PostMessageA(window->hwnd, WM_CLOSE, 0, 0);
#endif
}
void tc_gui_window_destroy(void *handle) {
    TcGuiWindow *window = (TcGuiWindow *)handle;
    if (!window)
        return;
#ifdef _WIN32
    if (window->hwnd)
        DestroyWindow(window->hwnd);
#endif
    tc_gui_buffer_destroy(window->buffer);
    free(window);
}
#endif
