#ifndef TC_GUI_X11_IMPLEMENTATION
#define TC_GUI_X11_IMPLEMENTATION

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/select.h>
#include <unistd.h>

static Display *tc_gui_x11_display(TcGuiWindow *window) {
    return (Display *)window->native_display;
}
static Window tc_gui_x11_window(TcGuiWindow *window) {
    return (Window)window->native_window;
}
static GC tc_gui_x11_gc(TcGuiWindow *window) {
    return (GC)window->native_gc;
}
static int tc_gui_x11_host_lsb(void) {
    const uint16_t value = 1;
    return *(const uint8_t *)&value == 1;
}

static void tc_gui_x11_drop_image(TcGuiWindow *window) {
    XImage *image = (XImage *)window->native_image;
    if (!image)
        return;
    image->data = NULL;
    XDestroyImage(image);
    window->native_image = NULL;
}
static int tc_gui_x11_make_image(TcGuiWindow *window) {
    Display *display = tc_gui_x11_display(window);
    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    TcGuiSurface *front = &window->buffer->front;
    XImage *image;
    if (!visual || visual->red_mask != 0x00ff0000UL || visual->green_mask != 0x0000ff00UL ||
        visual->blue_mask != 0x000000ffUL)
        return 9;
    tc_gui_x11_drop_image(window);
    image = XCreateImage(display, visual, (unsigned)DefaultDepth(display, screen), ZPixmap, 0,
                         (char *)front->pixels, (unsigned)front->width, (unsigned)front->height,
                         32, front->stride * (int)sizeof(TcGuiPixel));
    if (!image)
        return 9;
    if (image->bits_per_pixel != 32 ||
        image->byte_order != (tc_gui_x11_host_lsb() ? LSBFirst : MSBFirst)) {
        image->data = NULL;
        XDestroyImage(image);
        return 9;
    }
    window->native_image = image;
    return 0;
}
static int32_t tc_gui_x11_key(KeySym key) {
    switch (key) {
    case XK_Left: return TC_KEY_LEFT;
    case XK_Right: return TC_KEY_RIGHT;
    case XK_Up: return TC_KEY_UP;
    case XK_Down: return TC_KEY_DOWN;
    case XK_Home: return TC_KEY_HOME;
    case XK_End: return TC_KEY_END;
    case XK_Delete: return TC_KEY_DELETE;
    case XK_Page_Up: return TC_KEY_PAGE_UP;
    case XK_Page_Down: return TC_KEY_PAGE_DOWN;
    case XK_Return: return 13;
    case XK_Tab: return 9;
    case XK_BackSpace: return 8;
    case XK_Escape: return 27;
    default:
        return key <= 0xff ? (int32_t)key : 0;
    }
}
static uint32_t tc_gui_x11_utf8(const char *data, int length) {
    const unsigned char *s = (const unsigned char *)data;
    uint32_t cp;
    int need, i;
    if (length <= 0)
        return 0;
    if (s[0] < 0x80)
        return s[0];
    if (s[0] >= 0xc2 && s[0] <= 0xdf) {
        cp = s[0] & 0x1fu;
        need = 1;
    } else if (s[0] >= 0xe0 && s[0] <= 0xef) {
        cp = s[0] & 0x0fu;
        need = 2;
    } else if (s[0] >= 0xf0 && s[0] <= 0xf4) {
        cp = s[0] & 0x07u;
        need = 3;
    } else
        return 0xfffdu;
    if (length < need + 1)
        return 0xfffdu;
    for (i = 1; i <= need; i++) {
        if ((s[i] & 0xc0u) != 0x80u)
            return 0xfffdu;
        cp = (cp << 6) | (s[i] & 0x3fu);
    }
    return cp <= 0x10ffffu && !(cp >= 0xd800u && cp <= 0xdfffu) ? cp : 0xfffdu;
}
static void tc_gui_x11_repaint(TcGuiWindow *window, int full) {
    Display *display = tc_gui_x11_display(window);
    XImage *image = (XImage *)window->native_image;
    TcGuiSurface *front = &window->buffer->front;
    int x = 0, y = 0, width = front->width, height = front->height;
    if (!display || !image || !window->native_window)
        return;
    if (!full && front->dirty) {
        x = front->x0;
        y = front->y0;
        width = front->x1 - front->x0;
        height = front->y1 - front->y0;
    }
    if (width > 0 && height > 0)
        XPutImage(display, tc_gui_x11_window(window), tc_gui_x11_gc(window), image, x, y, x, y,
                  (unsigned)width, (unsigned)height);
    XFlush(display);
    tc_gui_surface_damage_clear(front);
}
static void tc_gui_x11_push_key(TcGuiWindow *window, XKeyEvent *key) {
    char bytes[16];
    KeySym symbol = NoSymbol;
    int length = XLookupString(key, bytes, (int)sizeof(bytes), &symbol, NULL);
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_KEY;
    event.key = tc_gui_x11_key(symbol);
    tc_gui_event_push(window, event);
    if (length > 0) {
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_TEXT;
        event.codepoint = tc_gui_x11_utf8(bytes, length);
        if (event.codepoint)
            tc_gui_event_push(window, event);
    }
}
static void tc_gui_x11_process(TcGuiWindow *window, XEvent *native) {
    Display *display = tc_gui_x11_display(window);
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    switch (native->type) {
    case Expose:
        if (native->xexpose.count == 0)
            tc_gui_x11_repaint(window, 1);
        break;
    case ConfigureNotify:
        if (native->xconfigure.width > 0 && native->xconfigure.height > 0 &&
            (native->xconfigure.width != window->buffer->front.width ||
             native->xconfigure.height != window->buffer->front.height) &&
            tc_gui_buffer_resize(window->buffer, native->xconfigure.width,
                                 native->xconfigure.height) == 0 &&
            tc_gui_x11_make_image(window) == 0) {
            event.kind = TC_GUI_EVENT_RESIZE;
            event.width = native->xconfigure.width;
            event.height = native->xconfigure.height;
            tc_gui_event_push(window, event);
        }
        break;
    case KeyPress:
        tc_gui_x11_push_key(window, &native->xkey);
        break;
    case MotionNotify:
        event.kind = TC_GUI_EVENT_MOUSE;
        event.x = native->xmotion.x;
        event.y = native->xmotion.y;
        tc_gui_event_push(window, event);
        break;
    case ButtonPress:
    case ButtonRelease:
        event.kind = TC_GUI_EVENT_MOUSE;
        event.x = native->xbutton.x;
        event.y = native->xbutton.y;
        event.button = native->xbutton.button == Button1 ? 1 :
                       native->xbutton.button == Button3 ? 2 :
                       native->xbutton.button == Button2 ? 3 : (int32_t)native->xbutton.button;
        event.pressed = native->type == ButtonPress;
        tc_gui_event_push(window, event);
        break;
    case ClientMessage:
        if ((Atom)native->xclient.data.l[0] == (Atom)window->native_delete) {
            event.kind = TC_GUI_EVENT_CLOSE;
            tc_gui_event_push(window, event);
            window->open = 0;
            if (window->native_window) {
                XDestroyWindow(display, tc_gui_x11_window(window));
                window->native_window = 0;
            }
        }
        break;
    case DestroyNotify:
        window->native_window = 0;
        window->open = 0;
        break;
    default:
        break;
    }
}
static int tc_gui_x11_create(TcGuiWindow *window, TinyString title) {
    Display *display = XOpenDisplay(NULL);
    int screen;
    Window native;
    GC gc;
    Atom wm_delete;
    char *caption;
    if (!display)
        return 9;
    screen = DefaultScreen(display);
    native = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0,
                                 (unsigned)window->buffer->front.width,
                                 (unsigned)window->buffer->front.height, 0,
                                 BlackPixel(display, screen), BlackPixel(display, screen));
    if (!native) {
        XCloseDisplay(display);
        return 9;
    }
    gc = XCreateGC(display, native, 0, NULL);
    if (!gc) {
        XDestroyWindow(display, native);
        XCloseDisplay(display);
        return 9;
    }
    window->native_display = display;
    window->native_window = native;
    window->native_gc = gc;
    caption = (char *)malloc(title.length + 1);
    if (!caption) {
        XFreeGC(display, gc);
        XDestroyWindow(display, native);
        XCloseDisplay(display);
        window->native_display = NULL;
        window->native_window = 0;
        window->native_gc = NULL;
        return 8;
    }
    memcpy(caption, title.data, title.length);
    caption[title.length] = 0;
    XStoreName(display, native, caption);
    free(caption);
    XSelectInput(display, native, ExposureMask | KeyPressMask | ButtonPressMask |
                                  ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    window->native_delete = (unsigned long)wm_delete;
    XSetWMProtocols(display, native, &wm_delete, 1);
    if (tc_gui_x11_make_image(window) != 0) {
        XFreeGC(display, gc);
        XDestroyWindow(display, native);
        XCloseDisplay(display);
        window->native_display = NULL;
        window->native_window = 0;
        window->native_gc = NULL;
        return 9;
    }
    XMapWindow(display, native);
    XFlush(display);
    return 0;
}
static void tc_gui_x11_present(TcGuiWindow *window, int full) {
    tc_gui_x11_repaint(window, full);
}
static void tc_gui_x11_wait(TcGuiWindow *window, int32_t timeout) {
    Display *display = tc_gui_x11_display(window);
    int fd;
    if (!display)
        return;
    for (;;) {
        while (XPending(display) > 0 && window->event_read == window->event_write) {
            XEvent event;
            XNextEvent(display, &event);
            tc_gui_x11_process(window, &event);
        }
        if (window->event_read != window->event_write || timeout == 0)
            return;
        fd = ConnectionNumber(display);
        {
            fd_set readfds;
            struct timeval value;
            struct timeval *timeptr = NULL;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            if (timeout >= 0) {
                value.tv_sec = timeout / 1000;
                value.tv_usec = (timeout % 1000) * 1000;
                timeptr = &value;
            }
            if (select(fd + 1, &readfds, NULL, NULL, timeptr) <= 0)
                return;
        }
        timeout = 0;
    }
}
static void tc_gui_x11_close(TcGuiWindow *window) {
    Display *display = tc_gui_x11_display(window);
    TcGuiEvent event;
    if (!display)
        return;
    if (window->open) {
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_CLOSE;
        tc_gui_event_push(window, event);
    }
    window->open = 0;
    if (window->native_window) {
        XDestroyWindow(display, tc_gui_x11_window(window));
        window->native_window = 0;
        XFlush(display);
    }
}
static void tc_gui_x11_destroy(TcGuiWindow *window) {
    Display *display = tc_gui_x11_display(window);
    if (!display)
        return;
    if (window->native_window)
        XDestroyWindow(display, tc_gui_x11_window(window));
    tc_gui_x11_drop_image(window);
    if (window->native_gc)
        XFreeGC(display, tc_gui_x11_gc(window));
    XCloseDisplay(display);
    window->native_display = NULL;
    window->native_window = 0;
    window->native_gc = NULL;
}
#endif
