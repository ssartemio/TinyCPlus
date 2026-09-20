#ifndef TC_GUI_X11_IMPLEMENTATION
#define TC_GUI_X11_IMPLEMENTATION
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/select.h>

typedef struct TcGuiX11 {
    Display *display;
    Window window;
    GC gc;
    Atom wm_delete;
} TcGuiX11;

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
    default:
        if (key >= XK_space && key <= XK_asciitilde)
            return (int32_t)key;
        return 0;
    }
}

static void tc_gui_x11_draw(TcGuiWindow *window, int whole) {
    TcGuiX11 *x11 = (TcGuiX11 *)window->platform;
    TcGuiSurface *surface = &window->buffer->front;
    XImage *image;
    int x = 0, y = 0, width = surface->width, height = surface->height;
    if (!x11 || !x11->display || !x11->window || !surface->pixels)
        return;
    if (!whole) {
        if (!surface->dirty)
            return;
        x = surface->x0;
        y = surface->y0;
        width = surface->x1 - surface->x0;
        height = surface->y1 - surface->y0;
        if (width <= 0 || height <= 0)
            return;
    }
    image = XCreateImage(x11->display, DefaultVisual(x11->display, DefaultScreen(x11->display)),
                         (unsigned)DefaultDepth(x11->display, DefaultScreen(x11->display)),
                         ZPixmap, 0, (char *)surface->pixels, (unsigned)surface->width,
                         (unsigned)surface->height, 32, surface->stride * 4);
    if (!image)
        return;
    XPutImage(x11->display, x11->window, x11->gc, image, x, y, x, y,
              (unsigned)width, (unsigned)height);
    image->data = NULL;
    XDestroyImage(image);
    XFlush(x11->display);
    tc_gui_surface_damage_clear(surface);
}

static void tc_gui_x11_push_mouse(TcGuiWindow *window, XButtonEvent *source, int pressed) {
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_MOUSE;
    event.x = source->x;
    event.y = source->y;
    event.button = (int32_t)source->button;
    event.pressed = pressed;
    tc_gui_event_push(window, event);
}

static void tc_gui_x11_dispatch(TcGuiWindow *window, XEvent *source) {
    TcGuiX11 *x11 = (TcGuiX11 *)window->platform;
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    switch (source->type) {
    case Expose:
        if (source->xexpose.count == 0)
            tc_gui_x11_draw(window, 1);
        break;
    case ConfigureNotify:
        if (source->xconfigure.width > 0 && source->xconfigure.height > 0 &&
            (source->xconfigure.width != window->buffer->back.width ||
             source->xconfigure.height != window->buffer->back.height) &&
            tc_gui_buffer_resize(window->buffer, source->xconfigure.width,
                                 source->xconfigure.height) == 0) {
            event.kind = TC_GUI_EVENT_RESIZE;
            event.width = source->xconfigure.width;
            event.height = source->xconfigure.height;
            tc_gui_event_push(window, event);
        }
        break;
    case KeyPress: {
        char bytes[16];
        KeySym symbol = NoSymbol;
        int count = XLookupString(&source->xkey, bytes, (int)sizeof(bytes), &symbol, NULL);
        event.kind = TC_GUI_EVENT_KEY;
        event.key = tc_gui_x11_key(symbol);
        tc_gui_event_push(window, event);
        if (count > 0) {
            TinyString text = {bytes, (size_t)count};
            size_t offset = 0;
            uint32_t codepoint = tc_gui_text_next(text, &offset);
            if (codepoint) {
                memset(&event, 0, sizeof(event));
                event.kind = TC_GUI_EVENT_TEXT;
                event.codepoint = codepoint;
                tc_gui_event_push(window, event);
            }
        }
        break;
    }
    case MotionNotify:
        event.kind = TC_GUI_EVENT_MOUSE;
        event.x = source->xmotion.x;
        event.y = source->xmotion.y;
        tc_gui_event_push(window, event);
        break;
    case ButtonPress:
        tc_gui_x11_push_mouse(window, &source->xbutton, 1);
        break;
    case ButtonRelease:
        tc_gui_x11_push_mouse(window, &source->xbutton, 0);
        break;
    case ClientMessage:
        if ((Atom)source->xclient.data.l[0] == x11->wm_delete) {
            event.kind = TC_GUI_EVENT_CLOSE;
            tc_gui_event_push(window, event);
            window->open = 0;
        }
        break;
    case DestroyNotify:
        window->open = 0;
        x11->window = 0;
        break;
    default:
        break;
    }
}

static int tc_gui_x11_create(TcGuiWindow *window, int32_t width, int32_t height, TinyString title,
                             int32_t *error) {
    TcGuiX11 *x11 = (TcGuiX11 *)calloc(1, sizeof(*x11));
    char *caption;
    int screen;
    if (!x11) {
        if (error) *error = 8;
        return 0;
    }
    x11->display = XOpenDisplay(NULL);
    if (!x11->display) {
        free(x11);
        if (error) *error = 9;
        return 0;
    }
    screen = DefaultScreen(x11->display);
    x11->window = XCreateSimpleWindow(x11->display, RootWindow(x11->display, screen), 0, 0,
                                      (unsigned)width, (unsigned)height, 0,
                                      BlackPixel(x11->display, screen),
                                      BlackPixel(x11->display, screen));
    if (!x11->window) {
        XCloseDisplay(x11->display);
        free(x11);
        if (error) *error = 9;
        return 0;
    }
    x11->gc = XCreateGC(x11->display, x11->window, 0, NULL);
    x11->wm_delete = XInternAtom(x11->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x11->display, x11->window, &x11->wm_delete, 1);
    XSelectInput(x11->display, x11->window,
                 ExposureMask | KeyPressMask | PointerMotionMask | ButtonPressMask |
                 ButtonReleaseMask | StructureNotifyMask);
    caption = (char *)malloc(title.length + 1);
    if (!caption) {
        XFreeGC(x11->display, x11->gc);
        XDestroyWindow(x11->display, x11->window);
        XCloseDisplay(x11->display);
        free(x11);
        if (error) *error = 8;
        return 0;
    }
    memcpy(caption, title.data, title.length);
    caption[title.length] = 0;
    XStoreName(x11->display, x11->window, caption);
    free(caption);
    XMapWindow(x11->display, x11->window);
    XFlush(x11->display);
    window->platform = x11;
    return 1;
}

static void tc_gui_x11_present(TcGuiWindow *window) {
    tc_gui_x11_draw(window, 0);
}

static void tc_gui_x11_next(TcGuiWindow *window, int32_t timeout, TcGuiEvent *event) {
    TcGuiX11 *x11 = (TcGuiX11 *)window->platform;
    if (!x11 || !x11->display)
        return;
    if (tc_gui_event_pop(window, event))
        return;
    for (;;) {
        while (XPending(x11->display) > 0) {
            XEvent source;
            XNextEvent(x11->display, &source);
            tc_gui_x11_dispatch(window, &source);
            if (tc_gui_event_pop(window, event))
                return;
        }
        if (timeout <= 0)
            return;
        {
            int fd = ConnectionNumber(x11->display);
            fd_set readfds;
            struct timeval wait;
            int ready;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            wait.tv_sec = timeout / 1000;
            wait.tv_usec = (timeout % 1000) * 1000;
            ready = select(fd + 1, &readfds, NULL, NULL, &wait);
            if (ready <= 0)
                return;
        }
        timeout = 0;
    }
}

static void tc_gui_x11_close(TcGuiWindow *window) {
    TcGuiX11 *x11 = (TcGuiX11 *)window->platform;
    TcGuiEvent event;
    if (!x11 || !x11->display)
        return;
    window->open = 0;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_CLOSE;
    tc_gui_event_push(window, event);
    if (x11->window) {
        XDestroyWindow(x11->display, x11->window);
        x11->window = 0;
        XFlush(x11->display);
    }
}

static void tc_gui_x11_destroy(TcGuiWindow *window) {
    TcGuiX11 *x11 = (TcGuiX11 *)window->platform;
    if (!x11)
        return;
    if (x11->display) {
        if (x11->window)
            XDestroyWindow(x11->display, x11->window);
        if (x11->gc)
            XFreeGC(x11->display, x11->gc);
        XCloseDisplay(x11->display);
    }
    free(x11);
    window->platform = NULL;
}
#endif
