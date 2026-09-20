#ifndef TC_GUI_COCOA_IMPLEMENTATION
#define TC_GUI_COCOA_IMPLEMENTATION
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreGraphics/CoreGraphics.h>

typedef struct TcCocoaPoint {
    double x, y;
} TcCocoaPoint;

typedef struct TcCocoaSize {
    double width, height;
} TcCocoaSize;

typedef struct TcCocoaRect {
    TcCocoaPoint origin;
    TcCocoaSize size;
} TcCocoaRect;

typedef struct TcGuiCocoa {
    id application;
    id pool;
    id window;
    id view;
} TcGuiCocoa;

static Class tc_gui_cocoa_view_class;

static SEL tc_cocoa_sel(const char *name) {
    return sel_registerName(name);
}
static id tc_cocoa_id0(id object, const char *name) {
    return ((id (*)(id, SEL))objc_msgSend)(object, tc_cocoa_sel(name));
}
static id tc_cocoa_id1(id object, const char *name, id value) {
    return ((id (*)(id, SEL, id))objc_msgSend)(object, tc_cocoa_sel(name), value);
}
static void tc_cocoa_void0(id object, const char *name) {
    ((void (*)(id, SEL))objc_msgSend)(object, tc_cocoa_sel(name));
}
static void tc_cocoa_void_bool(id object, const char *name, int value) {
    ((void (*)(id, SEL, signed char))objc_msgSend)(object, tc_cocoa_sel(name), (signed char)value);
}
static long tc_cocoa_long0(id object, const char *name) {
    return ((long (*)(id, SEL))objc_msgSend)(object, tc_cocoa_sel(name));
}
static int tc_cocoa_bool0(id object, const char *name) {
    return ((signed char (*)(id, SEL))objc_msgSend)(object, tc_cocoa_sel(name)) != 0;
}
static const char *tc_cocoa_utf8(id string) {
    return string ? ((const char *(*)(id, SEL))objc_msgSend)(string, tc_cocoa_sel("UTF8String")) : NULL;
}
static TcCocoaPoint tc_cocoa_point0(id object, const char *name) {
    return ((TcCocoaPoint (*)(id, SEL))objc_msgSend)(object, tc_cocoa_sel(name));
}
static TcCocoaRect tc_cocoa_rect0(id object, const char *name) {
#if defined(__x86_64__)
    TcCocoaRect result;
    ((void (*)(TcCocoaRect *, id, SEL))objc_msgSend_stret)(&result, object, tc_cocoa_sel(name));
    return result;
#else
    return ((TcCocoaRect (*)(id, SEL))objc_msgSend)(object, tc_cocoa_sel(name));
#endif
}

static TcGuiWindow *tc_gui_cocoa_view_window(id view) {
    Ivar ivar = class_getInstanceVariable(object_getClass(view), "_tcWindow");
    return ivar ? (TcGuiWindow *)object_getIvar(view, ivar) : NULL;
}

static void tc_gui_cocoa_draw_rect(id self, SEL command, TcCocoaRect dirty) {
    TcGuiWindow *window = tc_gui_cocoa_view_window(self);
    TcGuiSurface *surface;
    id ns_context;
    CGContextRef context;
    CGColorSpaceRef color_space;
    CGDataProviderRef provider;
    CGImageRef image;
    CGRect target;
    (void)command;
    (void)dirty;
    if (!window || !window->buffer)
        return;
    surface = &window->buffer->front;
    if (!surface->pixels)
        return;
    ns_context = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSGraphicsContext"),
                                                  tc_cocoa_sel("currentContext"));
    context = ns_context
                  ? ((CGContextRef (*)(id, SEL))objc_msgSend)(ns_context, tc_cocoa_sel("CGContext"))
                  : NULL;
    if (!context)
        return;
    color_space = CGColorSpaceCreateDeviceRGB();
    provider = CGDataProviderCreateWithData(NULL, surface->pixels,
                                            (size_t)surface->stride *
                                                (size_t)surface->height * sizeof(TcGuiPixel),
                                            NULL);
    if (!color_space || !provider) {
        if (provider) CGDataProviderRelease(provider);
        if (color_space) CGColorSpaceRelease(color_space);
        return;
    }
    image = CGImageCreate((size_t)surface->width, (size_t)surface->height, 8, 32,
                          (size_t)surface->stride * sizeof(TcGuiPixel), color_space,
                          kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                          provider, NULL, false, kCGRenderingIntentDefault);
    if (image) {
        target = CGRectMake(0, 0, surface->width, surface->height);
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, 0, surface->height);
        CGContextScaleCTM(context, 1, -1);
        CGContextDrawImage(context, target, image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
    }
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(color_space);
    tc_gui_surface_damage_clear(surface);
}

static int tc_gui_cocoa_register_view(void) {
    if (tc_gui_cocoa_view_class)
        return 1;
    tc_gui_cocoa_view_class = objc_getClass("TinyCPlusSurfaceView");
    if (tc_gui_cocoa_view_class)
        return 1;
    tc_gui_cocoa_view_class =
        objc_allocateClassPair(objc_getClass("NSView"), "TinyCPlusSurfaceView", 0);
    if (!tc_gui_cocoa_view_class)
        return 0;
    if (!class_addIvar(tc_gui_cocoa_view_class, "_tcWindow", sizeof(void *),
                       sizeof(void *) == 8 ? 3 : 2, "^v"))
        return 0;
    if (!class_addMethod(tc_gui_cocoa_view_class, tc_cocoa_sel("drawRect:"),
                         (IMP)tc_gui_cocoa_draw_rect,
                         "v@:{CGRect={CGPoint=dd}{CGSize=dd}}"))
        return 0;
    objc_registerClassPair(tc_gui_cocoa_view_class);
    return 1;
}

static id tc_gui_cocoa_string(TinyString value) {
    char *copy = (char *)malloc(value.length + 1);
    id result;
    if (!copy)
        return nil;
    memcpy(copy, value.data, value.length);
    copy[value.length] = 0;
    result = ((id (*)(id, SEL, const char *))objc_msgSend)(
        (id)objc_getClass("NSString"), tc_cocoa_sel("stringWithUTF8String:"), copy);
    free(copy);
    return result;
}

static void tc_gui_cocoa_check_resize(TcGuiWindow *window) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)window->platform;
    TcCocoaRect bounds;
    TcGuiEvent event;
    int width, height;
    if (!cocoa || !cocoa->view)
        return;
    bounds = tc_cocoa_rect0(cocoa->view, "bounds");
    width = (int)bounds.size.width;
    height = (int)bounds.size.height;
    if (width < 1 || height < 1 ||
        (width == window->buffer->back.width && height == window->buffer->back.height))
        return;
    if (tc_gui_buffer_resize(window->buffer, width, height) != 0)
        return;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_RESIZE;
    event.width = width;
    event.height = height;
    tc_gui_event_push(window, event);
}

static int32_t tc_gui_cocoa_key(long key_code, const char *characters) {
    switch (key_code) {
    case 123: return TC_KEY_LEFT;
    case 124: return TC_KEY_RIGHT;
    case 126: return TC_KEY_UP;
    case 125: return TC_KEY_DOWN;
    case 115: return TC_KEY_HOME;
    case 119: return TC_KEY_END;
    case 117: return TC_KEY_DELETE;
    case 116: return TC_KEY_PAGE_UP;
    case 121: return TC_KEY_PAGE_DOWN;
    case 51: return 8;
    case 48: return 9;
    case 36:
    case 76: return 13;
    case 53: return 27;
    default:
        if (characters && characters[0] && !characters[1])
            return (unsigned char)characters[0];
        return 0;
    }
}

static void tc_gui_cocoa_translate(TcGuiWindow *window, id native_event) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)window->platform;
    long type = tc_cocoa_long0(native_event, "type");
    TcGuiEvent event;
    memset(&event, 0, sizeof(event));
    if (type == 10) {
        id chars = tc_cocoa_id0(native_event, "characters");
        const char *utf8 = tc_cocoa_utf8(chars);
        event.kind = TC_GUI_EVENT_KEY;
        event.key = tc_gui_cocoa_key(tc_cocoa_long0(native_event, "keyCode"), utf8);
        tc_gui_event_push(window, event);
        if (utf8 && *utf8) {
            TinyString text = {utf8, strlen(utf8)};
            size_t offset = 0;
            uint32_t cp = tc_gui_text_next(text, &offset);
            if (cp >= 32 && cp != 127) {
                memset(&event, 0, sizeof(event));
                event.kind = TC_GUI_EVENT_TEXT;
                event.codepoint = cp;
                tc_gui_event_push(window, event);
            }
        }
    } else if (type == 1 || type == 2 || type == 3 || type == 4 ||
               type == 5 || type == 6 || type == 7 || type == 25 ||
               type == 26 || type == 27) {
        TcCocoaPoint point = tc_cocoa_point0(native_event, "locationInWindow");
        event.kind = TC_GUI_EVENT_MOUSE;
        event.x = (int32_t)point.x;
        event.y = window->buffer->back.height - 1 - (int32_t)point.y;
        if (type == 1 || type == 2 || type == 6) event.button = 1;
        else if (type == 3 || type == 4 || type == 7) event.button = 2;
        else if (type == 25 || type == 26 || type == 27)
            event.button = (int32_t)tc_cocoa_long0(native_event, "buttonNumber") + 1;
        event.pressed = (type == 1 || type == 3 || type == 25) ? 1 : 0;
        tc_gui_event_push(window, event);
    }
    ((void (*)(id, SEL, id))objc_msgSend)(cocoa->application, tc_cocoa_sel("sendEvent:"),
                                           native_event);
    tc_gui_cocoa_check_resize(window);
    if (window->open && !tc_cocoa_bool0(cocoa->window, "isVisible")) {
        memset(&event, 0, sizeof(event));
        event.kind = TC_GUI_EVENT_CLOSE;
        tc_gui_event_push(window, event);
        window->open = 0;
    }
}

static int tc_gui_cocoa_create(TcGuiWindow *window, int32_t width, int32_t height,
                               TinyString title, int32_t *error) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)calloc(1, sizeof(*cocoa));
    TcCocoaRect rect;
    id string;
    id window_object;
    Ivar ivar;
    if (!cocoa) {
        if (error) *error = 8;
        return 0;
    }
    cocoa->pool = tc_cocoa_id0((id)objc_getClass("NSAutoreleasePool"), "alloc");
    cocoa->pool = tc_cocoa_id0(cocoa->pool, "init");
    cocoa->application =
        tc_cocoa_id0((id)objc_getClass("NSApplication"), "sharedApplication");
    ((void (*)(id, SEL, long))objc_msgSend)(cocoa->application,
                                            tc_cocoa_sel("setActivationPolicy:"), 0L);
    tc_cocoa_void0(cocoa->application, "finishLaunching");
    if (!tc_gui_cocoa_register_view()) {
        if (error) *error = 9;
        tc_cocoa_void0(cocoa->pool, "drain");
        free(cocoa);
        return 0;
    }
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.width = width;
    rect.size.height = height;
    window_object = tc_cocoa_id0((id)objc_getClass("NSWindow"), "alloc");
    window_object =
        ((id (*)(id, SEL, TcCocoaRect, unsigned long, unsigned long, signed char))objc_msgSend)(
            window_object, tc_cocoa_sel("initWithContentRect:styleMask:backing:defer:"),
            rect, 15UL, 2UL, 0);
    if (!window_object) {
        if (error) *error = 9;
        tc_cocoa_void0(cocoa->pool, "drain");
        free(cocoa);
        return 0;
    }
    cocoa->window = window_object;
    tc_cocoa_void_bool(cocoa->window, "setReleasedWhenClosed:", 0);
    string = tc_gui_cocoa_string(title);
    if (string)
        tc_cocoa_id1(cocoa->window, "setTitle:", string);
    cocoa->view = tc_cocoa_id0((id)tc_gui_cocoa_view_class, "alloc");
    cocoa->view = ((id (*)(id, SEL, TcCocoaRect))objc_msgSend)(
        cocoa->view, tc_cocoa_sel("initWithFrame:"), rect);
    if (!cocoa->view) {
        tc_cocoa_void0(cocoa->window, "release");
        tc_cocoa_void0(cocoa->pool, "drain");
        free(cocoa);
        if (error) *error = 9;
        return 0;
    }
    ivar = class_getInstanceVariable(tc_gui_cocoa_view_class, "_tcWindow");
    object_setIvar(cocoa->view, ivar, (id)window);
    tc_cocoa_id1(cocoa->window, "setContentView:", cocoa->view);
    tc_cocoa_id1(cocoa->window, "makeKeyAndOrderFront:", nil);
    tc_cocoa_void_bool(cocoa->application, "activateIgnoringOtherApps:", 1);
    window->platform = cocoa;
    return 1;
}

static void tc_gui_cocoa_present(TcGuiWindow *window) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)window->platform;
    if (!cocoa || !cocoa->view)
        return;
    tc_cocoa_void_bool(cocoa->view, "setNeedsDisplay:", 1);
    tc_cocoa_void0(cocoa->view, "displayIfNeeded");
    tc_cocoa_void0(cocoa->application, "updateWindows");
}

static void tc_gui_cocoa_next(TcGuiWindow *window, int32_t timeout, TcGuiEvent *event) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)window->platform;
    id date_class, date, mode, native_event;
    if (!cocoa)
        return;
    if (tc_gui_event_pop(window, event))
        return;
    date_class = (id)objc_getClass("NSDate");
    date = ((id (*)(id, SEL, double))objc_msgSend)(
        date_class, tc_cocoa_sel("dateWithTimeIntervalSinceNow:"),
        timeout > 0 ? (double)timeout / 1000.0 : 0.0);
    mode = ((id (*)(id, SEL, const char *))objc_msgSend)(
        (id)objc_getClass("NSString"), tc_cocoa_sel("stringWithUTF8String:"),
        "kCFRunLoopDefaultMode");
    native_event =
        ((id (*)(id, SEL, unsigned long long, id, id, signed char))objc_msgSend)(
            cocoa->application,
            tc_cocoa_sel("nextEventMatchingMask:untilDate:inMode:dequeue:"),
            ~0ULL, date, mode, 1);
    if (native_event)
        tc_gui_cocoa_translate(window, native_event);
    else
        tc_gui_cocoa_check_resize(window);
    tc_gui_event_pop(window, event);
}

static void tc_gui_cocoa_close(TcGuiWindow *window) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)window->platform;
    TcGuiEvent event;
    if (!cocoa)
        return;
    window->open = 0;
    memset(&event, 0, sizeof(event));
    event.kind = TC_GUI_EVENT_CLOSE;
    tc_gui_event_push(window, event);
    tc_cocoa_void0(cocoa->window, "close");
}

static void tc_gui_cocoa_destroy(TcGuiWindow *window) {
    TcGuiCocoa *cocoa = (TcGuiCocoa *)window->platform;
    if (!cocoa)
        return;
    if (cocoa->window && tc_cocoa_bool0(cocoa->window, "isVisible"))
        tc_cocoa_void0(cocoa->window, "close");
    if (cocoa->window)
        tc_cocoa_id1(cocoa->window, "setContentView:", nil);
    if (cocoa->view)
        tc_cocoa_void0(cocoa->view, "release");
    if (cocoa->window)
        tc_cocoa_void0(cocoa->window, "release");
    if (cocoa->pool)
        tc_cocoa_void0(cocoa->pool, "drain");
    free(cocoa);
    window->platform = NULL;
}
#endif
