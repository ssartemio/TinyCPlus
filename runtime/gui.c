#ifndef TC_GUI_IMPLEMENTATION
#define TC_GUI_IMPLEMENTATION
#include "tiny_runtime.h"

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
#endif
