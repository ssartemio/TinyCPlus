#include "../runtime/gui.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
    int error = 0, x = 0, y = 0, width = 0, height = 0;
    uint32_t black = tc_gui_rgba(0, 0, 0, 255);
    uint32_t red = tc_gui_rgba(255, 0, 0, 255);
    uint32_t green = tc_gui_rgba(0, 255, 0, 255);
    uint32_t blue = tc_gui_rgba(0, 0, 255, 255);
    TcGuiSurface *surface = (TcGuiSurface *)tc_gui_surface_create(8, 6, &error);
    TcGuiSurface *source;
    TcGuiBuffer *buffer;
    int changed;

    assert(surface && error == 0);
    assert(tc_gui_surface_width(surface) == 8 && tc_gui_surface_height(surface) == 6);
    tc_gui_surface_clear(surface, black);
    assert(tc_gui_surface_damage(surface, &x, &y, &width, &height));
    assert(x == 0 && y == 0 && width == 8 && height == 6);
    tc_gui_surface_damage_clear(surface);

    tc_gui_fill_rect(surface, -1, 1, 4, 3, red);
    assert(tc_gui_surface_get(surface, 0, 1) == red);
    assert(tc_gui_surface_get(surface, 2, 3) == red);
    assert(tc_gui_surface_get(surface, 3, 2) == black);
    assert(tc_gui_surface_damage(surface, &x, &y, &width, &height));
    assert(x == 0 && y == 1 && width == 3 && height == 3);

    tc_gui_surface_damage_clear(surface);
    tc_gui_line(surface, 0, 0, 7, 5, green);
    assert(tc_gui_surface_get(surface, 0, 0) == green);
    assert(tc_gui_surface_get(surface, 7, 5) == green);

    source = (TcGuiSurface *)tc_gui_surface_create(2, 2, &error);
    assert(source && error == 0);
    tc_gui_surface_clear(source, blue);
    tc_gui_blit(surface, 5, 1, source, 0, 0, 2, 2);
    assert(tc_gui_surface_get(surface, 5, 1) == blue);
    assert(tc_gui_surface_get(surface, 6, 2) == blue);
    assert(tc_gui_surface_checksum(surface) != 0);

    buffer = (TcGuiBuffer *)tc_gui_buffer_create(4, 3, &error);
    assert(buffer && error == 0);
    tc_gui_surface_clear(tc_gui_buffer_back(buffer), black);
    changed = tc_gui_buffer_present(buffer);
    assert(changed == 12);
    assert(tc_gui_buffer_present(buffer) == 0);
    tc_gui_surface_set(tc_gui_buffer_back(buffer), 2, 1, red);
    assert(tc_gui_buffer_present(buffer) == 1);
    assert(tc_gui_surface_get(tc_gui_buffer_front(buffer), 2, 1) == red);

    tc_gui_buffer_destroy(buffer);
    tc_gui_surface_destroy(source);
    tc_gui_surface_destroy(surface);
    puts("GUI runtime verified: pixels, clipping, lines, blit, damage and double buffering");
    return 0;
}
