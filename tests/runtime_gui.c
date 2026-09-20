#include "../runtime/gui.c"
#include <assert.h>
#include <stdio.h>

#if defined(_WIN32) || defined(TC_GUI_X11_BACKEND)
static void gui_native_drain(TcGuiWindow *window) {
    TcGuiEvent event;
    do {
        memset(&event, 0, sizeof(event));
        tc_gui_window_next(window, 0, &event.kind, &event.key, &event.x, &event.y, &event.width,
                           &event.height, &event.button, &event.pressed, &event.codepoint);
    } while (event.kind != TC_GUI_EVENT_NONE);
}
#endif

int main(void) {
    int error = 0, x = 0, y = 0, width = 0, height = 0;
    uint32_t black = tc_gui_rgba(0, 0, 0, 255);
    uint32_t red = tc_gui_rgba(255, 0, 0, 255);
    uint32_t green = tc_gui_rgba(0, 255, 0, 255);
    uint32_t blue = tc_gui_rgba(0, 0, 255, 255);
    TcGuiSurface *surface = (TcGuiSurface *)tc_gui_surface_create(8, 6, &error);
    TcGuiSurface *source;
    TcGuiBuffer *buffer;
    TcGuiWindow *window;
    TcGuiEvent event;
    void *textbox;
    TinyString box_text;
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
    tc_gui_surface_damage_clear(surface);
    assert(tc_gui_text_width(TC_STRING("ABC"), 1) == 17);
    tc_gui_text(surface, 0, 0, TC_STRING("A1?"), tc_gui_rgba(255, 255, 255, 255), 1);
    assert(tc_gui_surface_damage(surface, &x, &y, &width, &height));
    assert(width > 0 && height > 0);

    buffer = (TcGuiBuffer *)tc_gui_buffer_create(4, 3, &error);
    assert(buffer && error == 0);
    tc_gui_surface_clear(tc_gui_buffer_back(buffer), black);
    changed = tc_gui_buffer_present(buffer);
    assert(changed == 12);
    assert(tc_gui_buffer_present(buffer) == 0);
    tc_gui_surface_damage_clear(tc_gui_buffer_front(buffer));
    tc_gui_surface_set(tc_gui_buffer_back(buffer), 2, 1, red);
    assert(tc_gui_buffer_present(buffer) == 1);
    assert(tc_gui_surface_get(tc_gui_buffer_front(buffer), 2, 1) == red);
    assert(tc_gui_surface_damage(tc_gui_buffer_front(buffer), &x, &y, &width, &height));
    assert(x == 2 && y == 1 && width == 1 && height == 1);
    tc_gui_surface_damage_clear(tc_gui_buffer_front(buffer));
    tc_gui_surface_set(tc_gui_buffer_back(buffer), 2, 1, red);
    assert(tc_gui_buffer_present(buffer) == 0);
    assert(!tc_gui_surface_damage(tc_gui_buffer_front(buffer), &x, &y, &width, &height));

    window = (TcGuiWindow *)tc_gui_window_create(5, 4, TC_STRING("headless"), 1, &error);
    assert(window && error == 0 && tc_gui_window_open(window));
    tc_gui_surface_clear(tc_gui_window_surface(window), green);
    assert(tc_gui_window_present(window) == 20);
    assert(tc_gui_window_post(window, TC_GUI_EVENT_CUSTOM, 42, 1, 2, 3, 4, 5, 1, 'Z') == 0);
    memset(&event, 0, sizeof(event));
    tc_gui_window_next(window, 0, &event.kind, &event.key, &event.x, &event.y, &event.width,
                       &event.height, &event.button, &event.pressed, &event.codepoint);
    assert(event.kind == TC_GUI_EVENT_CUSTOM && event.key == 42 && event.x == 1 && event.y == 2);
    assert(event.width == 3 && event.height == 4 && event.button == 5 && event.pressed == 1);
    assert(event.codepoint == 'Z');
    tc_gui_window_close(window);
    assert(!tc_gui_window_open(window));
    tc_gui_window_destroy(window);

#ifdef _WIN32
    {
        TcGuiWindow *native;
        TcGuiEvent native_event;
        native = (TcGuiWindow *)tc_gui_window_create(64, 48, TC_STRING("TinyC+ CI"), 0, &error);
        assert(native && error == 0 && native->hwnd && tc_gui_window_open(native));
        ShowWindow(native->hwnd, SW_HIDE);
        gui_native_drain(native);

        {
            int32_t native_width = tc_gui_window_width(native);
            int32_t native_height = tc_gui_window_height(native);
            assert(native_width > 0 && native_height > 0);
            tc_gui_surface_clear(tc_gui_window_surface(native), blue);
            assert(tc_gui_window_present(native) == native_width * native_height);
        }

        memset(&native_event, 0, sizeof(native_event));
        PostMessageA(native->hwnd, WM_KEYDOWN, VK_LEFT, 0);
        tc_gui_window_next(native, 250, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        assert(native_event.kind == TC_GUI_EVENT_KEY && native_event.key == TC_KEY_LEFT);

        memset(&native_event, 0, sizeof(native_event));
        PostMessageA(native->hwnd, WM_CHAR, 'A', 0);
        tc_gui_window_next(native, 250, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        assert(native_event.kind == TC_GUI_EVENT_TEXT && native_event.codepoint == 'A');

        memset(&native_event, 0, sizeof(native_event));
        PostMessageA(native->hwnd, WM_LBUTTONUP, 0, MAKELPARAM(7, 9));
        tc_gui_window_next(native, 250, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        assert(native_event.kind == TC_GUI_EVENT_MOUSE && native_event.button == 1);
        assert(native_event.x == 7 && native_event.y == 9 && native_event.pressed == 0);

        tc_gui_window_close(native);
        memset(&native_event, 0, sizeof(native_event));
        tc_gui_window_next(native, 250, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        assert(native_event.kind == TC_GUI_EVENT_CLOSE && !tc_gui_window_open(native));
        tc_gui_window_destroy(native);
    }
#endif

#ifdef TC_GUI_X11_BACKEND
    {
        TcGuiWindow *native;
        TcGuiEvent native_event;
        Display *display;
        XEvent sent;
        Window xwindow;
        native = (TcGuiWindow *)tc_gui_window_create(96, 64, TC_STRING("TinyC+ X11 CI"), 0, &error);
        assert(native && error == 0 && native->native_display && native->native_window);
        display = (Display *)native->native_display;
        xwindow = (Window)native->native_window;
        gui_native_drain(native);

        tc_gui_surface_clear(tc_gui_window_surface(native), blue);
        assert(tc_gui_window_present(native) == tc_gui_window_width(native) * tc_gui_window_height(native));

        memset(&sent, 0, sizeof(sent));
        sent.xkey.type = KeyPress;
        sent.xkey.display = display;
        sent.xkey.window = xwindow;
        sent.xkey.root = DefaultRootWindow(display);
        sent.xkey.same_screen = True;
        sent.xkey.keycode = XKeysymToKeycode(display, XK_Left);
        assert(XSendEvent(display, xwindow, True, KeyPressMask, &sent));
        XFlush(display);
        memset(&native_event, 0, sizeof(native_event));
        tc_gui_window_next(native, 250, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        assert(native_event.kind == TC_GUI_EVENT_KEY && native_event.key == TC_KEY_LEFT);

        memset(&sent, 0, sizeof(sent));
        sent.xbutton.type = ButtonRelease;
        sent.xbutton.display = display;
        sent.xbutton.window = xwindow;
        sent.xbutton.root = DefaultRootWindow(display);
        sent.xbutton.same_screen = True;
        sent.xbutton.button = Button1;
        sent.xbutton.x = 7;
        sent.xbutton.y = 9;
        assert(XSendEvent(display, xwindow, True, ButtonReleaseMask, &sent));
        XFlush(display);
        memset(&native_event, 0, sizeof(native_event));
        tc_gui_window_next(native, 250, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        assert(native_event.kind == TC_GUI_EVENT_MOUSE && native_event.button == 1);
        assert(native_event.x == 7 && native_event.y == 9 && native_event.pressed == 0);

        XResizeWindow(display, xwindow, 120, 72);
        XFlush(display);
        memset(&native_event, 0, sizeof(native_event));
        tc_gui_window_next(native, 500, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        while (native_event.kind != TC_GUI_EVENT_RESIZE && native_event.kind != TC_GUI_EVENT_NONE) {
            memset(&native_event, 0, sizeof(native_event));
            tc_gui_window_next(native, 500, &native_event.kind, &native_event.key, &native_event.x,
                               &native_event.y, &native_event.width, &native_event.height,
                               &native_event.button, &native_event.pressed, &native_event.codepoint);
        }
        assert(native_event.kind == TC_GUI_EVENT_RESIZE);
        assert(native_event.width == 120 && native_event.height == 72);
        assert(tc_gui_window_width(native) == 120 && tc_gui_window_height(native) == 72);

        memset(&sent, 0, sizeof(sent));
        sent.xclient.type = ClientMessage;
        sent.xclient.display = display;
        sent.xclient.window = xwindow;
        sent.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        sent.xclient.format = 32;
        sent.xclient.data.l[0] = (long)native->native_delete;
        sent.xclient.data.l[1] = CurrentTime;
        assert(XSendEvent(display, xwindow, False, NoEventMask, &sent));
        XFlush(display);
        memset(&native_event, 0, sizeof(native_event));
        tc_gui_window_next(native, 500, &native_event.kind, &native_event.key, &native_event.x,
                           &native_event.y, &native_event.width, &native_event.height,
                           &native_event.button, &native_event.pressed, &native_event.codepoint);
        while (native_event.kind != TC_GUI_EVENT_CLOSE && native_event.kind != TC_GUI_EVENT_NONE) {
            memset(&native_event, 0, sizeof(native_event));
            tc_gui_window_next(native, 500, &native_event.kind, &native_event.key, &native_event.x,
                               &native_event.y, &native_event.width, &native_event.height,
                               &native_event.button, &native_event.pressed, &native_event.codepoint);
        }
        assert(native_event.kind == TC_GUI_EVENT_CLOSE && !tc_gui_window_open(native));
        tc_gui_window_destroy(native);
    }
#endif

    textbox = tc_gui_textbox_create(TC_STRING("abc"));
    assert(textbox);
    assert(tc_gui_textbox_event(textbox, TC_GUI_EVENT_TEXT, 0, 'X') == 0);
    box_text = tc_gui_textbox_text(textbox);
    assert(tc_string_equal(box_text, TC_STRING("Xabc")));
    tc_string_free(box_text);
    assert(tc_gui_textbox_dirty(textbox));
    tc_gui_textbox_draw(surface, textbox, 0, 0, 8, 6, 1, 0xffffffffu, black, green);
    tc_gui_textbox_clean(textbox);
    assert(!tc_gui_textbox_dirty(textbox));
    tc_gui_textbox_destroy(textbox);

    tc_gui_buffer_destroy(buffer);
    tc_gui_surface_destroy(source);
    tc_gui_surface_destroy(surface);
    puts("GUI runtime verified: pixels, clipping, lines, blit, bitmap text, textbox editing, damage, double buffering and window events");
    return 0;
}
