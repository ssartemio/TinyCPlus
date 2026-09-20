#ifndef TC_GUI_COCOA_IMPLEMENTATION
#define TC_GUI_COCOA_IMPLEMENTATION

#include <dlfcn.h>

typedef void *TcCocoaId;
typedef void *TcCocoaClass;
typedef void *TcCocoaSel;
typedef unsigned char TcCocoaBool;
typedef struct { double x, y; } TcCocoaPoint;
typedef struct { double width, height; } TcCocoaSize;
typedef struct { TcCocoaPoint origin; TcCocoaSize size; } TcCocoaRect;

typedef struct TcCocoaApi {
    void *objc_handle, *appkit_handle, *cg_handle;
    TcCocoaClass (*get_class)(const char *);
    TcCocoaSel (*sel_register)(const char *);
    void *msg_send;
    TcCocoaClass (*allocate_class_pair)(TcCocoaClass, const char *, size_t);
    TcCocoaBool (*class_add_method)(TcCocoaClass, TcCocoaSel, void *, const char *);
    void (*register_class_pair)(TcCocoaClass);
    void *run_mode;
    void *(*color_space_rgb)(void);
    void (*color_space_release)(void *);
    void *(*provider_create)(void *, const void *, size_t, void (*)(void *, const void *, size_t));
    void (*provider_release)(void *);
    void *(*image_create)(size_t, size_t, size_t, size_t, size_t, void *, uint32_t, void *,
                          const double *, bool, int);
    void (*image_release)(void *);
    void (*context_draw_image)(void *, TcCocoaRect, void *);
} TcCocoaApi;

typedef struct TcCocoaMap {
    TcGuiWindow *window;
    void *native_window;
    void *view;
    struct TcCocoaMap *next;
} TcCocoaMap;

static TcCocoaApi tc_cocoa;
static int tc_cocoa_loaded;
static TcCocoaClass tc_cocoa_view_class;
static TcCocoaClass tc_cocoa_delegate_class;
static TcCocoaMap *tc_cocoa_windows;

static int tc_cocoa_symbol(void *handle, const char *name, void *field, size_t size) {
    void *symbol = dlsym(handle, name);
    if (!symbol)
        return 0;
    memcpy(field, &symbol, size);
    return 1;
}
#define TC_COCOA_LOAD(handle, field, name)     do { if (!tc_cocoa_symbol((handle), (name), &tc_cocoa.field, sizeof(tc_cocoa.field))) return 0; } while (0)

static int tc_cocoa_load(void) {
    void **mode;
    if (tc_cocoa_loaded)
        return 1;
    memset(&tc_cocoa, 0, sizeof(tc_cocoa));
    tc_cocoa.objc_handle = dlopen("/usr/lib/libobjc.A.dylib", RTLD_NOW | RTLD_GLOBAL);
    tc_cocoa.appkit_handle =
        dlopen("/System/Library/Frameworks/AppKit.framework/AppKit", RTLD_NOW | RTLD_GLOBAL);
    tc_cocoa.cg_handle =
        dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", RTLD_NOW | RTLD_GLOBAL);
    if (!tc_cocoa.objc_handle || !tc_cocoa.appkit_handle || !tc_cocoa.cg_handle)
        return 0;
    TC_COCOA_LOAD(tc_cocoa.objc_handle, get_class, "objc_getClass");
    TC_COCOA_LOAD(tc_cocoa.objc_handle, sel_register, "sel_registerName");
    TC_COCOA_LOAD(tc_cocoa.objc_handle, msg_send, "objc_msgSend");
    TC_COCOA_LOAD(tc_cocoa.objc_handle, allocate_class_pair, "objc_allocateClassPair");
    TC_COCOA_LOAD(tc_cocoa.objc_handle, class_add_method, "class_addMethod");
    TC_COCOA_LOAD(tc_cocoa.objc_handle, register_class_pair, "objc_registerClassPair");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, color_space_rgb, "CGColorSpaceCreateDeviceRGB");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, color_space_release, "CGColorSpaceRelease");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, provider_create, "CGDataProviderCreateWithData");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, provider_release, "CGDataProviderRelease");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, image_create, "CGImageCreate");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, image_release, "CGImageRelease");
    TC_COCOA_LOAD(tc_cocoa.cg_handle, context_draw_image, "CGContextDrawImage");
    mode = (void **)dlsym(tc_cocoa.appkit_handle, "NSDefaultRunLoopMode");
    if (!mode || !*mode)
        return 0;
    tc_cocoa.run_mode = *mode;
    tc_cocoa_loaded = 1;
    return 1;
}
#undef TC_COCOA_LOAD

static TcCocoaSel tc_cocoa_sel(const char *name) {
    return tc_cocoa.sel_register(name);
}
static void *tc_cocoa_id0(void *object, const char *selector) {
    return ((void *(*)(void *, TcCocoaSel))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector));
}
static void tc_cocoa_void0(void *object, const char *selector) {
    ((void (*)(void *, TcCocoaSel))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector));
}
static void tc_cocoa_void_id(void *object, const char *selector, void *value) {
    ((void (*)(void *, TcCocoaSel, void *))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector), value);
}
static void tc_cocoa_void_bool(void *object, const char *selector, TcCocoaBool value) {
    ((void (*)(void *, TcCocoaSel, TcCocoaBool))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector), value);
}
static uint64_t tc_cocoa_u64(void *object, const char *selector) {
    return ((uint64_t (*)(void *, TcCocoaSel))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector));
}
static TcCocoaBool tc_cocoa_bool(void *object, const char *selector) {
    return ((TcCocoaBool (*)(void *, TcCocoaSel))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector));
}
static double tc_cocoa_double(void *object, const char *selector) {
    return ((double (*)(void *, TcCocoaSel))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector));
}
static double tc_gui_cocoa_backing_scale(TcGuiWindow *window) {
    double scale;
    if (!window || !window->native_window)
        return 1.0;
    scale = tc_cocoa_double(window->native_window, "backingScaleFactor");
    return scale > 0.0 ? scale : 1.0;
}
static TcCocoaPoint tc_cocoa_point(void *object, const char *selector) {
    return ((TcCocoaPoint (*)(void *, TcCocoaSel))tc_cocoa.msg_send)(object, tc_cocoa_sel(selector));
}
static TcCocoaMap *tc_cocoa_find_window(void *native) {
    TcCocoaMap *entry;
    for (entry = tc_cocoa_windows; entry; entry = entry->next)
        if (entry->native_window == native)
            return entry;
    return NULL;
}
static TcGuiWindow *tc_cocoa_find_view(void *view) {
    TcCocoaMap *entry;
    for (entry = tc_cocoa_windows; entry; entry = entry->next)
        if (entry->view == view)
            return entry->window;
    return NULL;
}
static void tc_cocoa_map_add(TcGuiWindow *window) {
    TcCocoaMap *entry = (TcCocoaMap *)malloc(sizeof(*entry));
    if (!entry)
        abort();
    entry->window = window;
    entry->native_window = window->native_window;
    entry->view = window->native_view;
    entry->next = tc_cocoa_windows;
    tc_cocoa_windows = entry;
}
static void tc_cocoa_map_remove(TcGuiWindow *window) {
    TcCocoaMap **link = &tc_cocoa_windows;
    while (*link) {
        if ((*link)->window == window) {
            TcCocoaMap *dead = *link;
            *link = dead->next;
            free(dead);
            return;
        }
        link = &(*link)->next;
    }
}
static void tc_cocoa_drop_image(TcGuiWindow *window) {
    if (window->native_image)
        tc_cocoa.image_release(window->native_image);
    if (window->native_provider)
        tc_cocoa.provider_release(window->native_provider);
    if (window->native_color_space)
        tc_cocoa.color_space_release(window->native_color_space);
    window->native_image = NULL;
    window->native_provider = NULL;
    window->native_color_space = NULL;
}
static int tc_cocoa_make_image(TcGuiWindow *window) {
    TcGuiSurface *front = &window->buffer->front;
    const uint32_t bitmap_info = (2u << 12) | 6u; /* 32-bit little-endian, skip alpha first */
    window->native_color_space = tc_cocoa.color_space_rgb();
    if (!window->native_color_space)
        return 9;
    window->native_provider =
        tc_cocoa.provider_create(NULL, front->pixels,
                                 (size_t)front->stride * (size_t)front->height * sizeof(TcGuiPixel),
                                 NULL);
    if (!window->native_provider) {
        tc_cocoa_drop_image(window);
        return 9;
    }
    window->native_image =
        tc_cocoa.image_create((size_t)front->width, (size_t)front->height, 8, 32,
                              (size_t)front->stride * sizeof(TcGuiPixel),
                              window->native_color_space, bitmap_info, window->native_provider,
                              NULL, false, 0);
    if (!window->native_image) {
        tc_cocoa_drop_image(window);
        return 9;
    }
    return 0;
}
static void tc_cocoa_draw_rect(void *self, TcCocoaSel command, TcCocoaRect dirty) {
    TcGuiWindow *window = tc_cocoa_find_view(self);
    void *graphics, *context;
    TcCocoaRect area;
    (void)command;
    (void)dirty;
    if (!window || !window->native_image)
        return;
    graphics = tc_cocoa_id0((void *)tc_cocoa.get_class("NSGraphicsContext"), "currentContext");
    if (!graphics)
        return;
    context = tc_cocoa_id0(graphics, "CGContext");
    if (!context)
        return;
    area.origin.x = 0;
    area.origin.y = 0;
    area.size.width = window->buffer->front.width;
    area.size.height = window->buffer->front.height;
    tc_cocoa.context_draw_image(context, area, window->native_image);
    tc_gui_surface_damage_clear(&window->buffer->front);
}
static TcCocoaBool tc_cocoa_is_flipped(void *self, TcCocoaSel command) {
    (void)self;
    (void)command;
    return 1;
}
static int tc_cocoa_view_class_ready(void) {
    TcCocoaClass base;
    if (tc_cocoa_view_class)
        return 1;
    tc_cocoa_view_class = tc_cocoa.get_class("TinyCPlusGuiView");
    if (tc_cocoa_view_class)
        return 1;
    base = tc_cocoa.get_class("NSView");
    if (!base)
        return 0;
    tc_cocoa_view_class = tc_cocoa.allocate_class_pair(base, "TinyCPlusGuiView", 0);
    if (!tc_cocoa_view_class)
        return 0;
    if (!tc_cocoa.class_add_method(tc_cocoa_view_class, tc_cocoa_sel("drawRect:"),
                                    (void *)tc_cocoa_draw_rect,
                                    "v@:{CGRect={CGPoint=dd}{CGSize=dd}}") ||
        !tc_cocoa.class_add_method(tc_cocoa_view_class, tc_cocoa_sel("isFlipped"),
                                    (void *)tc_cocoa_is_flipped, "B@:"))
        return 0;
    tc_cocoa.register_class_pair(tc_cocoa_view_class);
    return 1;
}
static TcCocoaRect tc_cocoa_rect(void *object, const char *selector) {
#if defined(__x86_64__)
    TcCocoaRect result;
    void *stret = dlsym(tc_cocoa.objc_handle, "objc_msgSend_stret");
    if (!stret) {
        memset(&result, 0, sizeof(result));
        return result;
    }
    ((void (*)(TcCocoaRect *, void *, TcCocoaSel))stret)
        (&result, object, tc_cocoa_sel(selector));
    return result;
#else
    return ((TcCocoaRect (*)(void *, TcCocoaSel))tc_cocoa.msg_send)
        (object, tc_cocoa_sel(selector));
#endif
}
static void tc_cocoa_resize_notification(void *self, TcCocoaSel command, void *notification) {
    void *native_window;
    TcCocoaMap *entry;
    TcGuiWindow *window;
    void *view;
    TcCocoaRect bounds;
    int32_t width, height;
    int result;
    TcGuiEvent event;
    (void)self;
    (void)command;
    native_window = tc_cocoa_id0(notification, "object");
    entry = tc_cocoa_find_window(native_window);
    if (!entry)
        return;
    window = entry->window;
    view = tc_cocoa_id0(native_window, "contentView");
    if (!view)
        return;
    bounds = tc_cocoa_rect(view, "bounds");
    width = (int32_t)(bounds.size.width + 0.5);
    height = (int32_t)(bounds.size.height + 0.5);
    if (width < 1 || height < 1 ||
        (width == window->buffer->front.width && height == window->buffer->front.height))
        return;
    tc_cocoa_drop_image(window);
    result = tc_gui_buffer_resize(window->buffer, width, height);
    if (result != 0) {
        tc_cocoa_make_image(window);
        return;
    }
    if (tc_cocoa_make_image(window) != 0)
        return;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_RESIZE;
    event.width = width;
    event.height = height;
    tc_gui_event_push(window, event);
    tc_cocoa_void_bool(window->native_view, "setNeedsDisplay:", 1);
}
static void tc_cocoa_close_notification(void *self, TcCocoaSel command, void *notification) {
    void *native_window;
    TcCocoaMap *entry;
    TcGuiEvent event;
    (void)self;
    (void)command;
    native_window = tc_cocoa_id0(notification, "object");
    entry = tc_cocoa_find_window(native_window);
    if (!entry || !entry->window->open)
        return;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_CLOSE;
    tc_gui_event_push(entry->window, event);
    entry->window->open = 0;
}
static int tc_cocoa_delegate_class_ready(void) {
    TcCocoaClass base;
    if (tc_cocoa_delegate_class)
        return 1;
    tc_cocoa_delegate_class = tc_cocoa.get_class("TinyCPlusWindowDelegate");
    if (tc_cocoa_delegate_class)
        return 1;
    base = tc_cocoa.get_class("NSObject");
    if (!base)
        return 0;
    tc_cocoa_delegate_class = tc_cocoa.allocate_class_pair(base, "TinyCPlusWindowDelegate", 0);
    if (!tc_cocoa_delegate_class)
        return 0;
    if (!tc_cocoa.class_add_method(tc_cocoa_delegate_class, tc_cocoa_sel("windowDidResize:"),
                                    (void *)tc_cocoa_resize_notification, "v@:@") ||
        !tc_cocoa.class_add_method(tc_cocoa_delegate_class, tc_cocoa_sel("windowWillClose:"),
                                    (void *)tc_cocoa_close_notification, "v@:@"))
        return 0;
    tc_cocoa.register_class_pair(tc_cocoa_delegate_class);
    return 1;
}
static uint32_t tc_cocoa_utf8_next(const char *text, size_t length, size_t *offset) {
    const unsigned char *s = (const unsigned char *)text;
    size_t start = *offset;
    uint32_t cp;
    int need, i;
    if (!s || start >= length)
        return 0;
    if (s[start] < 0x80) {
        *offset = start + 1;
        return s[start];
    }
    if (s[start] >= 0xc2 && s[start] <= 0xdf) {
        cp = s[start] & 0x1fu; need = 1;
    } else if (s[start] >= 0xe0 && s[start] <= 0xef) {
        cp = s[start] & 0x0fu; need = 2;
    } else if (s[start] >= 0xf0 && s[start] <= 0xf4) {
        cp = s[start] & 0x07u; need = 3;
    } else {
        *offset = start + 1;
        return 0xfffdu;
    }
    if (start + (size_t)need >= length) {
        *offset = length;
        return 0xfffdu;
    }
    for (i = 1; i <= need; i++) {
        if ((s[start + (size_t)i] & 0xc0u) != 0x80u) {
            *offset = start + (size_t)i;
            return 0xfffdu;
        }
        cp = (cp << 6) | (s[start + (size_t)i] & 0x3fu);
    }
    *offset = start + (size_t)need + 1;
    if (cp > 0x10ffffu || (cp >= 0xd800u && cp <= 0xdfffu))
        return 0xfffdu;
    return cp;
}

static void tc_cocoa_push_text(TcGuiWindow *window, const char *utf8) {
    size_t offset = 0, length;
    if (!utf8)
        return;
    length = strlen(utf8);
    while (offset < length) {
        TcGuiEvent event;
        uint32_t cp = tc_cocoa_utf8_next(utf8, length, &offset);
        if (!cp)
            break;
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_TEXT;
        event.codepoint = cp;
        tc_gui_event_push(window, event);
    }
}
static int32_t tc_cocoa_key(uint64_t keycode) {
    switch (keycode) {
    case 123: return TC_KEY_LEFT;
    case 124: return TC_KEY_RIGHT;
    case 125: return TC_KEY_DOWN;
    case 126: return TC_KEY_UP;
    case 115: return TC_KEY_HOME;
    case 119: return TC_KEY_END;
    case 117: return TC_KEY_DELETE;
    case 116: return TC_KEY_PAGE_UP;
    case 121: return TC_KEY_PAGE_DOWN;
    case 36: return 13;
    case 48: return 9;
    case 51: return 8;
    case 53: return 27;
    case 49: return 32;
    default: return 0;
    }
}
static void tc_cocoa_process_event(void *native_event) {
    uint64_t type;
    void *native_window;
    TcCocoaMap *entry;
    TcGuiWindow *window;
    TcGuiEvent event;
    if (!native_event)
        return;
    type = tc_cocoa_u64(native_event, "type");
    native_window = tc_cocoa_id0(native_event, "window");
    entry = tc_cocoa_find_window(native_window);
    if (!entry)
        return;
    window = entry->window;
    memset(&event, 0, sizeof(event));
    if (type == 10) { /* keyDown */
        void *characters;
        const char *utf8;
        event.kind = TC_GUI_EVENT_KEY;
        event.key = tc_cocoa_key(tc_cocoa_u64(native_event, "keyCode"));
        tc_gui_event_push(window, event);
        characters = tc_cocoa_id0(native_event, "characters");
        utf8 = characters ? ((const char *(*)(void *, TcCocoaSel))tc_cocoa.msg_send)
                            (characters, tc_cocoa_sel("UTF8String")) : NULL;
        if (utf8 && *utf8)
            tc_cocoa_push_text(window, utf8);
    } else if (type == 22) { /* scrollWheel */
        TcCocoaPoint point = tc_cocoa_point(native_event, "locationInWindow");
        event.kind = TC_GUI_EVENT_SCROLL;
        event.x = (int32_t)point.x;
        event.y = window->buffer->front.height - 1 - (int32_t)point.y;
        event.scroll_x = tc_cocoa_double(native_event, "scrollingDeltaX");
        event.scroll_y = tc_cocoa_double(native_event, "scrollingDeltaY");
        tc_gui_event_push(window, event);
    } else if (type == 1 || type == 2 || type == 3 || type == 4 ||
               type == 5 || type == 6 || type == 7 || type == 25 || type == 26 || type == 27) {
        TcCocoaPoint point = tc_cocoa_point(native_event, "locationInWindow");
        event.kind = TC_GUI_EVENT_MOUSE;
        event.x = (int32_t)point.x;
        event.y = window->buffer->front.height - 1 - (int32_t)point.y;
        if (type == 1 || type == 2 || type == 6)
            event.button = 1;
        else if (type == 3 || type == 4 || type == 7)
            event.button = 2;
        else if (type == 25 || type == 26 || type == 27)
            event.button = (int32_t)tc_cocoa_u64(native_event, "buttonNumber") + 1;
        event.pressed = type == 1 || type == 3 || type == 25 || type == 6 || type == 7 || type == 27;
        tc_gui_event_push(window, event);
    }
}
static int tc_gui_cocoa_create(TcGuiWindow *window, TinyString title) {
    TcCocoaClass app_class, pool_class, nswindow_class, string_class;
    void *app, *pool, *native, *view, *caption;
    TcCocoaRect rect;
    unsigned long style = 1u | 2u | 4u | 8u;
    void *delegate;
    if (!tc_cocoa_load() || !tc_cocoa_view_class_ready() || !tc_cocoa_delegate_class_ready())
        return 9;
    pool_class = tc_cocoa.get_class("NSAutoreleasePool");
    app_class = tc_cocoa.get_class("NSApplication");
    nswindow_class = tc_cocoa.get_class("NSWindow");
    string_class = tc_cocoa.get_class("NSString");
    if (!pool_class || !app_class || !nswindow_class || !string_class)
        return 9;
    pool = tc_cocoa_id0((void *)pool_class, "alloc");
    pool = tc_cocoa_id0(pool, "init");
    app = tc_cocoa_id0((void *)app_class, "sharedApplication");
    if (!pool || !app)
        return 9;
    ((void (*)(void *, TcCocoaSel, long))tc_cocoa.msg_send)
        (app, tc_cocoa_sel("setActivationPolicy:"), 0);
    tc_cocoa_void0(app, "finishLaunching");
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.width = window->buffer->front.width;
    rect.size.height = window->buffer->front.height;
    native = tc_cocoa_id0((void *)nswindow_class, "alloc");
    native = ((void *(*)(void *, TcCocoaSel, TcCocoaRect, unsigned long, unsigned long, TcCocoaBool))
              tc_cocoa.msg_send)
        (native, tc_cocoa_sel("initWithContentRect:styleMask:backing:defer:"), rect, style, 2, 0);
    if (!native) {
        tc_cocoa_void0(pool, "release");
        return 9;
    }
    tc_cocoa_void_bool(native, "setReleasedWhenClosed:", 0);
    view = tc_cocoa_id0((void *)tc_cocoa_view_class, "alloc");
    view = ((void *(*)(void *, TcCocoaSel, TcCocoaRect))tc_cocoa.msg_send)
        (view, tc_cocoa_sel("initWithFrame:"), rect);
    if (!view) {
        tc_cocoa_void0(native, "release");
        tc_cocoa_void0(pool, "release");
        return 9;
    }
    caption = tc_cocoa_id0((void *)string_class, "alloc");
    caption = ((void *(*)(void *, TcCocoaSel, const void *, unsigned long, unsigned long))
               tc_cocoa.msg_send)
        (caption, tc_cocoa_sel("initWithBytes:length:encoding:"), title.data,
         (unsigned long)title.length, 4u);
    if (caption)
        tc_cocoa_void_id(native, "setTitle:", caption);
    tc_cocoa_void_id(native, "setContentView:", view);
    tc_cocoa_void_bool(native, "setAcceptsMouseMovedEvents:", 1);
    window->native_app = app;
    window->native_pool = pool;
    window->native_window = native;
    window->native_view = view;
    delegate = tc_cocoa_id0((void *)tc_cocoa_delegate_class, "alloc");
    delegate = tc_cocoa_id0(delegate, "init");
    if (!delegate) {
        if (caption) tc_cocoa_void0(caption, "release");
        tc_cocoa_void0(view, "release");
        tc_cocoa_void0(native, "release");
        tc_cocoa_void0(pool, "release");
        window->native_app = NULL;
        window->native_pool = NULL;
        window->native_window = NULL;
        window->native_view = NULL;
        return 9;
    }
    window->native_delegate = delegate;
    if (tc_cocoa_make_image(window) != 0) {
        if (caption) tc_cocoa_void0(caption, "release");
        tc_cocoa_void0(view, "release");
        tc_cocoa_void0(native, "release");
        tc_cocoa_void0(pool, "release");
        tc_cocoa_void0(delegate, "release");
        window->native_app = NULL;
        window->native_pool = NULL;
        window->native_window = NULL;
        window->native_view = NULL;
        window->native_delegate = NULL;
        return 9;
    }
    tc_cocoa_map_add(window);
    tc_cocoa_void_id(native, "setDelegate:", delegate);
    tc_cocoa_void_id(native, "makeKeyAndOrderFront:", NULL);
    tc_cocoa_void_bool(app, "activateIgnoringOtherApps:", 1);
    tc_cocoa_void0(view, "display");
    if (caption) tc_cocoa_void0(caption, "release");
    tc_cocoa_void0(view, "release"); /* retained by NSWindow */
    return 0;
}
static void tc_gui_cocoa_present(TcGuiWindow *window) {
    if (!window->native_view)
        return;
    tc_cocoa_void_bool(window->native_view, "setNeedsDisplay:", 1);
    tc_cocoa_void0(window->native_view, "displayIfNeeded");
}
static void tc_gui_cocoa_wait(TcGuiWindow *window, int32_t timeout) {
    void *date_class, *until, *event;
    if (!window || !window->native_app || window->event_read != window->event_write)
        return;
    date_class = (void *)tc_cocoa.get_class("NSDate");
    if (timeout < 0)
        until = tc_cocoa_id0(date_class, "distantFuture");
    else if (timeout == 0)
        until = tc_cocoa_id0(date_class, "distantPast");
    else
        until = ((void *(*)(void *, TcCocoaSel, double))tc_cocoa.msg_send)
            (date_class, tc_cocoa_sel("dateWithTimeIntervalSinceNow:"), (double)timeout / 1000.0);
    event = ((void *(*)(void *, TcCocoaSel, uint64_t, void *, void *, TcCocoaBool))
             tc_cocoa.msg_send)
        (window->native_app, tc_cocoa_sel("nextEventMatchingMask:untilDate:inMode:dequeue:"),
         ~(uint64_t)0, until, tc_cocoa.run_mode, 1);
    if (event) {
        void *event_window = tc_cocoa_id0(event, "window");
        TcCocoaMap *entry = tc_cocoa_find_window(event_window);
        tc_cocoa_process_event(event);
        tc_cocoa_void_id(window->native_app, "sendEvent:", event);
        tc_cocoa_void0(window->native_app, "updateWindows");
        if (entry && entry->window->open && !tc_cocoa_bool(entry->native_window, "isVisible")) {
            TcGuiEvent close_event;
            memset(&close_event, 0, sizeof(close_event));
            close_event.kind = TC_GUI_EVENT_CLOSE;
            tc_gui_event_push(entry->window, close_event);
            entry->window->open = 0;
        }
    }
}
static void tc_gui_cocoa_close(TcGuiWindow *window) {
    TcGuiEvent event;
    if (!window || !window->native_window)
        return;
    if (window->open)
        tc_cocoa_void0(window->native_window, "close");
    if (window->open) {
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_CLOSE;
        tc_gui_event_push(window, event);
        window->open = 0;
    }
}
static void tc_gui_cocoa_destroy(TcGuiWindow *window) {
    if (!window)
        return;
    if (window->native_window)
        tc_cocoa_void_id(window->native_window, "setDelegate:", NULL);
    tc_cocoa_map_remove(window);
    tc_cocoa_drop_image(window);
    if (window->native_window) {
        if (tc_cocoa_bool(window->native_window, "isVisible"))
            tc_cocoa_void0(window->native_window, "close");
        tc_cocoa_void0(window->native_window, "release");
    }
    if (window->native_delegate)
        tc_cocoa_void0(window->native_delegate, "release");
    if (window->native_pool)
        tc_cocoa_void0(window->native_pool, "release");
    window->native_app = NULL;
    window->native_window = NULL;
    window->native_view = NULL;
    window->native_delegate = NULL;
    window->native_pool = NULL;
}
#endif
