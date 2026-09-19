#ifndef TC_TUI_IMPLEMENTATION
#define TC_TUI_IMPLEMENTATION
#include "gapbuffer.c"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

enum { TC_UI_KEY = 1, TC_UI_RESIZE = 2, TC_UI_TIMER = 3, TC_UI_MOUSE = 4, TC_UI_CUSTOM = 5 };
enum {
    TC_KEY_LEFT = 1001,
    TC_KEY_RIGHT,
    TC_KEY_UP,
    TC_KEY_DOWN,
    TC_KEY_HOME,
    TC_KEY_END,
    TC_KEY_DELETE,
    TC_KEY_PAGE_UP,
    TC_KEY_PAGE_DOWN
};
enum {
    TC_WIDGET_LABEL = 1,
    TC_WIDGET_BUTTON,
    TC_WIDGET_TEXTBOX,
    TC_WIDGET_PANEL,
    TC_WIDGET_ROW,
    TC_WIDGET_COLUMN,
    TC_WIDGET_STACK
};
typedef struct TcUiCell {
    uint32_t codepoint, foreground, background;
    uint8_t attributes, width;
} TcUiCell;
typedef struct TcUiEvent {
    int32_t kind, key, x, y, width, height;
    uint32_t codepoint;
} TcUiEvent;
typedef struct TcUiClosure {
    void *environment;
    void (*invoke)(void *);
    bool owned;
} TcUiClosure;
typedef struct TcWidget {
    int kind, x, y, width, height, preferred_width, preferred_height, scroll_line, scroll_column;
    TinyString text;
    TcGap *buffer;
    TcUiClosure callback;
    struct TcWidget *parent, *children, *last, *next;
} TcWidget;
typedef struct TcUi {
    int width, height, headless, active, changed, focus_index;
    TcUiCell *front, *back;
    TcWidget *root, *focus;
    TcUiEvent events[64];
    unsigned event_read, event_write;
#ifdef _WIN32
    HANDLE input, output;
    DWORD input_mode, output_mode;
    UINT input_cp, output_cp;
    uint32_t surrogate;
#else
    struct termios saved;
#endif
} TcUi;
typedef struct TcUiBytes {
    char *data;
    size_t length, capacity;
} TcUiBytes;
static TcUi *tc_active_terminal;

static void tc_ui_append(TcUiBytes *b, const char *data, size_t n) {
    size_t capacity;
    if (b->length + n + 1 > b->capacity) {
        capacity = b->capacity ? b->capacity : 256;
        while (capacity < b->length + n + 1)
            capacity *= 2;
        b->data = (char *)realloc(b->data, capacity);
        if (!b->data)
            abort();
        b->capacity = capacity;
    }
    memcpy(b->data + b->length, data, n);
    b->length += n;
    b->data[b->length] = 0;
}
static int tc_utf8_encode(uint32_t cp, char bytes[4]) {
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        cp = 0xfffd;
    if (cp < 128) {
        bytes[0] = (char)cp;
        return 1;
    }
    if (cp < 2048) {
        bytes[0] = (char)(0xc0 | (cp >> 6));
        bytes[1] = (char)(0x80 | (cp & 63));
        return 2;
    }
    if (cp < 65536) {
        bytes[0] = (char)(0xe0 | (cp >> 12));
        bytes[1] = (char)(0x80 | ((cp >> 6) & 63));
        bytes[2] = (char)(0x80 | (cp & 63));
        return 3;
    }
    bytes[0] = (char)(0xf0 | (cp >> 18));
    bytes[1] = (char)(0x80 | ((cp >> 12) & 63));
    bytes[2] = (char)(0x80 | ((cp >> 6) & 63));
    bytes[3] = (char)(0x80 | (cp & 63));
    return 4;
}
static uint32_t tc_utf8_decode(TinyString s, size_t *position) {
    size_t p = *position;
    uint32_t cp;
    int continuation, i;
    unsigned char first;
    if (p >= s.length)
        return 0;
    first = (unsigned char)s.data[p++];
    *position = p;
    if (first < 128)
        return first;
    if (first >= 0xc2 && first <= 0xdf) {
        cp = first & 31;
        continuation = 1;
    } else if (first >= 0xe0 && first <= 0xef) {
        cp = first & 15;
        continuation = 2;
    } else if (first >= 0xf0 && first <= 0xf4) {
        cp = first & 7;
        continuation = 3;
    } else
        return 0xfffd;
    if (p + (size_t)continuation > s.length)
        return 0xfffd;
    for (i = 0; i < continuation; i++) {
        unsigned char next = (unsigned char)s.data[p++];
        if ((next & 0xc0) != 0x80)
            return 0xfffd;
        cp = (cp << 6) | (next & 63);
    }
    if ((continuation == 1 && cp < 128) || (continuation == 2 && cp < 2048) ||
        (continuation == 3 && cp < 65536) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return 0xfffd;
    *position = p;
    return cp;
}
static int tc_ui_width(uint32_t cp) {
    return (cp >= 0x1100 &&
            ((cp <= 0x115f) || (cp >= 0x2329 && cp <= 0x232a) || (cp >= 0x2e80 && cp <= 0xa4cf) ||
             (cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
             (cp >= 0xfe10 && cp <= 0xfe19) || (cp >= 0xfe30 && cp <= 0xfe6f) ||
             (cp >= 0xff00 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) ||
             (cp >= 0x1f300 && cp <= 0x1faff) || (cp >= 0x20000 && cp <= 0x3fffd)))
               ? 2
               : 1;
}
static int tc_ui_resize_impl(TcUi *ui, int width, int height) {
    TcUiCell *front, *back;
    size_t cells;
    if (width < 1 || height < 1 || width > 1000 || height > 1000)
        return 3;
    cells = (size_t)width * height;
    front = (TcUiCell *)malloc(cells * sizeof(*front));
    back = (TcUiCell *)calloc(cells, sizeof(*back));
    if (!front || !back) {
        free(front);
        free(back);
        return 8;
    }
    memset(front, 255, cells * sizeof(*front));
    free(ui->front);
    free(ui->back);
    ui->front = front;
    ui->back = back;
    ui->width = width;
    ui->height = height;
    return 0;
}
static void tc_ui_restore(void) {
    TcUi *ui = tc_active_terminal;
    if (!ui || !ui->active)
        return;
    fputs("\033[0m\033[?25h\033[?1000l\033[?1006l\033[?1049l", stdout);
    fflush(stdout);
#ifdef _WIN32
    SetConsoleMode(ui->input, ui->input_mode);
    SetConsoleMode(ui->output, ui->output_mode);
    SetConsoleCP(ui->input_cp);
    SetConsoleOutputCP(ui->output_cp);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ui->saved);
#endif
    ui->active = 0;
    tc_active_terminal = NULL;
}
void *tc_ui_create(int32_t width, int32_t height, int32_t headless, int32_t *error) {
    TcUi *ui = (TcUi *)calloc(1, sizeof(*ui));
    *error = 0;
    if (!ui) {
        *error = 8;
        return NULL;
    }
    ui->headless = headless;
    if (!headless) {
        if (tc_active_terminal) {
            *error = 1;
            free(ui);
            return NULL;
        }
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        ui->input = GetStdHandle(STD_INPUT_HANDLE);
        ui->output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleMode(ui->input, &ui->input_mode) ||
            !GetConsoleMode(ui->output, &ui->output_mode)) {
            *error = 2;
            free(ui);
            return NULL;
        }
        ui->input_cp = GetConsoleCP();
        ui->output_cp = GetConsoleOutputCP();
        if (!SetConsoleMode(ui->output, ui->output_mode | 4)) {
            *error = 2;
            free(ui);
            return NULL;
        }
        if (!SetConsoleMode(ui->input, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | 0x0080)) {
            SetConsoleMode(ui->output, ui->output_mode);
            *error = 2;
            free(ui);
            return NULL;
        }
        SetConsoleCP(65001);
        SetConsoleOutputCP(65001);
        if (GetConsoleScreenBufferInfo(ui->output, &info)) {
            width = info.srWindow.Right - info.srWindow.Left + 1;
            height = info.srWindow.Bottom - info.srWindow.Top + 1;
        }
#else
        struct termios raw;
        struct winsize size;
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) ||
            tcgetattr(STDIN_FILENO, &ui->saved)) {
            *error = 2;
            free(ui);
            return NULL;
        }
        raw = ui->saved;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_oflag &= ~OPOST;
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
            *error = 2;
            free(ui);
            return NULL;
        }
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
            width = size.ws_col;
            height = size.ws_row;
        }
#endif
        ui->active = 1;
        tc_active_terminal = ui;
        tc_program_cleanup = tc_ui_restore;
        fputs("\033[?1049h\033[?25l\033[?1000h\033[?1006h", stdout);
        fflush(stdout);
    }
    *error = tc_ui_resize_impl(ui, width, height);
    if (*error) {
        tc_ui_restore();
        free(ui);
        return NULL;
    }
    return ui;
}
int32_t tc_ui_width_value(void *window) {
    return ((TcUi *)window)->width;
}
int32_t tc_ui_height_value(void *window) {
    return ((TcUi *)window)->height;
}
int32_t tc_ui_resize(void *window, int32_t width, int32_t height) {
    return tc_ui_resize_impl((TcUi *)window, width, height);
}
void tc_ui_clear(void *window, uint32_t foreground, uint32_t background) {
    TcUi *ui = (TcUi *)window;
    size_t i, n = (size_t)ui->width * ui->height;
    for (i = 0; i < n; i++) {
        TcUiCell cell;
        memset(&cell, 0, sizeof(cell));
        cell.codepoint = ' ';
        cell.foreground = foreground;
        cell.background = background;
        cell.width = 1;
        ui->back[i] = cell;
    }
}
void tc_ui_put(void *window, int32_t x, int32_t y, uint32_t codepoint, uint32_t foreground,
               uint32_t background) {
    TcUi *ui = (TcUi *)window;
    TcUiCell cell;
    int width = tc_ui_width(codepoint);
    if (x < 0 || x >= ui->width || y < 0 || y >= ui->height)
        return;
    if (codepoint < 32 || (codepoint >= 127 && codepoint < 160))
        codepoint = ' ';
    if (width == 2 && x + 1 >= ui->width) {
        codepoint = ' ';
        width = 1;
    }
    memset(&cell, 0, sizeof(cell));
    cell.codepoint = codepoint;
    cell.foreground = foreground;
    cell.background = background;
    cell.width = (uint8_t)width;
    ui->back[y * ui->width + x] = cell;
    if (width == 2) {
        cell.codepoint = 0;
        cell.width = 0;
        ui->back[y * ui->width + x + 1] = cell;
    }
}
static void tc_ui_text_clip(TcUi *ui, int x, int y, TinyString text, uint32_t fg, uint32_t bg,
                            int width) {
    size_t p = 0;
    int start = x;
    while (p < text.length && x - start < width) {
        uint32_t cp = tc_utf8_decode(text, &p);
        int cw = tc_ui_width(cp);
        if (cp == '\n')
            break;
        if (cp == '\t') {
            int spaces = 4 - ((x - start) % 4);
            while (spaces-- && x - start < width)
                tc_ui_put(ui, x++, y, ' ', fg, bg);
            continue;
        }
        if (x - start + cw > width)
            break;
        tc_ui_put(ui, x, y, cp, fg, bg);
        x += cw;
    }
}
void tc_ui_text(void *window, int32_t x, int32_t y, TinyString text, uint32_t foreground,
                uint32_t background) {
    TcUi *ui = (TcUi *)window;
    tc_ui_text_clip(ui, x, y, text, foreground, background, ui->width - x);
}
TinyString tc_ui_render(void *window) {
    TcUi *ui = (TcUi *)window;
    TcUiBytes bytes = {0};
    int x, y, last_x = -2, last_y = -2;
    uint32_t fg = ~0u, bg = ~0u;
    ui->changed = 0;
    for (y = 0; y < ui->height; y++)
        for (x = 0; x < ui->width; x++) {
            size_t i = (size_t)y * ui->width + x;
            TcUiCell *cell = &ui->back[i];
            char temp[96];
            int n;
            if (!memcmp(cell, &ui->front[i], sizeof(*cell)))
                continue;
            ui->front[i] = *cell;
            ui->changed++;
            if (!cell->width)
                continue;
            if (last_y != y || last_x != x) {
                n = snprintf(temp, sizeof(temp), "\033[%d;%dH", y + 1, x + 1);
                tc_ui_append(&bytes, temp, (size_t)n);
            }
            if (fg != cell->foreground || bg != cell->background) {
                fg = cell->foreground;
                bg = cell->background;
                n = snprintf(temp, sizeof(temp), "\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um",
                             (fg >> 16) & 255, (fg >> 8) & 255, fg & 255, (bg >> 16) & 255,
                             (bg >> 8) & 255, bg & 255);
                tc_ui_append(&bytes, temp, (size_t)n);
            }
            n = tc_utf8_encode(cell->codepoint ? cell->codepoint : ' ', temp);
            tc_ui_append(&bytes, temp, (size_t)n);
            last_x = x + cell->width;
            last_y = y;
        }
    if (!bytes.data)
        tc_ui_append(&bytes, "", 0);
    {
        TinyString result = {bytes.data, bytes.length};
        return result;
    }
}
int32_t tc_ui_changed(void *window) {
    return ((TcUi *)window)->changed;
}
void tc_ui_refresh(void *window) {
    TinyString ansi = tc_ui_render(window);
    if (ansi.length)
        fwrite(ansi.data, 1, ansi.length, stdout);
    fflush(stdout);
    tc_string_free(ansi);
}
TinyString tc_ui_snapshot(void *window) {
    TcUi *ui = (TcUi *)window;
    TcUiBytes bytes = {0};
    int x, y;
    for (y = 0; y < ui->height; y++) {
        for (x = 0; x < ui->width; x++) {
            TcUiCell *cell = &ui->back[y * ui->width + x];
            char encoded[4];
            int n;
            if (!cell->width)
                continue;
            n = tc_utf8_encode(cell->codepoint ? cell->codepoint : ' ', encoded);
            tc_ui_append(&bytes, encoded, (size_t)n);
        }
        tc_ui_append(&bytes, "\n", 1);
    }
    {
        TinyString result = {bytes.data, bytes.length};
        return result;
    }
}
int32_t tc_ui_post(void *window, int32_t kind, int32_t key, int32_t x, int32_t y,
                   uint32_t codepoint) {
    TcUi *ui = (TcUi *)window;
    TcUiEvent event;
    if (ui->event_write - ui->event_read >= 64)
        return 8;
    memset(&event, 0, sizeof(event));
    event.kind = kind;
    event.key = key;
    event.x = x;
    event.y = y;
    event.codepoint = codepoint;
    ui->events[ui->event_write++ % 64] = event;
    return 0;
}
#ifndef _WIN32
static int tc_ui_read_byte(int timeout) {
    fd_set set;
    struct timeval time;
    unsigned char byte;
    int ready;
    memset(&set, 0, sizeof(set));
    FD_SET(STDIN_FILENO, &set);
    time.tv_sec = timeout / 1000;
    time.tv_usec = (timeout % 1000) * 1000;
    ready = select(STDIN_FILENO + 1, &set, NULL, NULL, timeout < 0 ? NULL : &time);
    return ready > 0 && read(STDIN_FILENO, &byte, 1) == 1 ? byte : -1;
}
#endif
static TcUiEvent tc_ui_event(TcUi *ui, int timeout) {
    TcUiEvent event;
    memset(&event, 0, sizeof(event));
    if (ui->event_read != ui->event_write)
        return ui->events[ui->event_read++ % 64];
    event.kind = TC_UI_TIMER;
    if (ui->headless)
        return event;
#ifdef _WIN32
    {
        INPUT_RECORD record;
        DWORD count;
        int repeat = 0;
        while (repeat++ < 128 &&
               WaitForSingleObject(ui->input, timeout < 0 ? INFINITE : (DWORD)timeout) ==
                   WAIT_OBJECT_0) {
            if (!ReadConsoleInputW(ui->input, &record, 1, &count) || !count)
                return event;
            if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                CONSOLE_SCREEN_BUFFER_INFO info;
                if (GetConsoleScreenBufferInfo(ui->output, &info)) {
                    event.width = info.srWindow.Right - info.srWindow.Left + 1;
                    event.height = info.srWindow.Bottom - info.srWindow.Top + 1;
                    if (event.width != ui->width || event.height != ui->height) {
                        event.kind = TC_UI_RESIZE;
                        return event;
                    }
                }
            } else if (record.EventType == MOUSE_EVENT) {
                MOUSE_EVENT_RECORD *m = &record.Event.MouseEvent;
                if (m->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                    event.kind = TC_UI_MOUSE;
                    event.x = m->dwMousePosition.X;
                    event.y = m->dwMousePosition.Y;
                    event.key = 1;
                    return event;
                }
            } else if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                KEY_EVENT_RECORD *key = &record.Event.KeyEvent;
                uint32_t cp = key->uChar.UnicodeChar;
                event.kind = TC_UI_KEY;
                switch (key->wVirtualKeyCode) {
                case VK_LEFT:
                    event.key = TC_KEY_LEFT;
                    break;
                case VK_RIGHT:
                    event.key = TC_KEY_RIGHT;
                    break;
                case VK_UP:
                    event.key = TC_KEY_UP;
                    break;
                case VK_DOWN:
                    event.key = TC_KEY_DOWN;
                    break;
                case VK_HOME:
                    event.key = TC_KEY_HOME;
                    break;
                case VK_END:
                    event.key = TC_KEY_END;
                    break;
                case VK_DELETE:
                    event.key = TC_KEY_DELETE;
                    break;
                case VK_PRIOR:
                    event.key = TC_KEY_PAGE_UP;
                    break;
                case VK_NEXT:
                    event.key = TC_KEY_PAGE_DOWN;
                    break;
                default:
                    event.key = cp < 128 ? (int)cp : 0;
                    break;
                }
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    ui->surrogate = cp;
                    continue;
                }
                if (cp >= 0xdc00 && cp <= 0xdfff && ui->surrogate)
                    cp = 0x10000 + ((ui->surrogate - 0xd800) << 10) + (cp - 0xdc00);
                ui->surrogate = 0;
                event.codepoint = cp;
                if (event.key || cp)
                    return event;
            }
            timeout = 0;
        }
    }
#else
    {
        struct winsize size;
        int byte;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 &&
            (size.ws_col != ui->width || size.ws_row != ui->height)) {
            event.kind = TC_UI_RESIZE;
            event.width = size.ws_col;
            event.height = size.ws_row;
            return event;
        }
        byte = tc_ui_read_byte(timeout);
        if (byte < 0)
            return event;
        event.kind = TC_UI_KEY;
        event.key = byte;
        event.codepoint = (uint32_t)byte;
        if (byte == 27) {
            char sequence[40];
            int length = 0, next = tc_ui_read_byte(25);
            if (next == '[' || next == 'O') {
                while (length < 39 && (next = tc_ui_read_byte(25)) >= 0) {
                    sequence[length++] = (char)next;
                    if ((next >= 'A' && next <= 'Z') || next == '~' || next == 'm')
                        break;
                }
                sequence[length] = 0;
                event.codepoint = 0;
                if (!strcmp(sequence, "A"))
                    event.key = TC_KEY_UP;
                else if (!strcmp(sequence, "B"))
                    event.key = TC_KEY_DOWN;
                else if (!strcmp(sequence, "C"))
                    event.key = TC_KEY_RIGHT;
                else if (!strcmp(sequence, "D"))
                    event.key = TC_KEY_LEFT;
                else if (!strcmp(sequence, "H") || !strcmp(sequence, "1~"))
                    event.key = TC_KEY_HOME;
                else if (!strcmp(sequence, "F") || !strcmp(sequence, "4~"))
                    event.key = TC_KEY_END;
                else if (!strcmp(sequence, "3~"))
                    event.key = TC_KEY_DELETE;
                else if (!strcmp(sequence, "5~"))
                    event.key = TC_KEY_PAGE_UP;
                else if (!strcmp(sequence, "6~"))
                    event.key = TC_KEY_PAGE_DOWN;
                else if (sequence[0] == '<') {
                    int button, x, y;
                    if (sscanf(sequence, "<%d;%d;%dM", &button, &x, &y) == 3 &&
                        sequence[length - 1] == 'M') {
                        event.kind = TC_UI_MOUSE;
                        event.key = button + 1;
                        event.x = x - 1;
                        event.y = y - 1;
                    }
                }
            }
        } else if (byte >= 128) {
            char encoded[4];
            int n = byte < 224 ? 2 : byte < 240 ? 3 : 4, i;
            size_t p = 0;
            TinyString s;
            encoded[0] = (char)byte;
            for (i = 1; i < n; i++) {
                int next = tc_ui_read_byte(25);
                if (next < 0)
                    break;
                encoded[i] = (char)next;
            }
            s.data = encoded;
            s.length = (size_t)i;
            event.key = 0;
            event.codepoint = tc_utf8_decode(s, &p);
        }
    }
#endif
    return event;
}
void tc_ui_next(void *window, int32_t timeout, int32_t *kind, int32_t *key, int32_t *x, int32_t *y,
                int32_t *width, int32_t *height, uint32_t *codepoint) {
    TcUiEvent event = tc_ui_event((TcUi *)window, timeout);
    *kind = event.kind;
    *key = event.key;
    *x = event.x;
    *y = event.y;
    *width = event.width;
    *height = event.height;
    *codepoint = event.codepoint;
}
int32_t tc_ui_replay(void *window, TinyString input, uint64_t *offset) {
    size_t position = (size_t)*offset;
    uint32_t cp;
    if (*offset >= input.length)
        return 1;
    cp = tc_utf8_decode(input, &position);
    *offset = position;
    return tc_ui_post(window, TC_UI_KEY, cp < 128 ? (int32_t)cp : 0, 0, 0, cp);
}

void *tc_widget_create(int32_t kind, TinyString text, int32_t width, int32_t height) {
    TcWidget *w = (TcWidget *)tc_alloc_checked(sizeof(*w));
    w->kind = kind;
    w->preferred_width = width;
    w->preferred_height = height;
    w->text = tc_string_copy(text);
    if (kind == TC_WIDGET_TEXTBOX)
        w->buffer = (TcGap *)tc_gap_create(text);
    return w;
}
static void tc_widget_free(TcWidget *w) {
    TcWidget *child = w->children;
    while (child) {
        TcWidget *next = child->next;
        tc_widget_free(child);
        child = next;
    }
    if (w->callback.owned)
        free(w->callback.environment);
    tc_gap_destroy(w->buffer);
    tc_string_free(w->text);
    free(w);
}
void tc_widget_destroy(void *widget) {
    TcWidget *w = (TcWidget *)widget;
    if (w && !w->parent)
        tc_widget_free(w);
}
int32_t tc_widget_add(void *parent, void *child) {
    TcWidget *p = (TcWidget *)parent, *c = (TcWidget *)child, *ancestor;
    if (!p || !c || c->parent || p->kind < TC_WIDGET_PANEL)
        return 1;
    for (ancestor = p; ancestor; ancestor = ancestor->parent)
        if (ancestor == c)
            return 1;
    c->parent = p;
    if (p->last)
        p->last->next = c;
    else
        p->children = c;
    p->last = c;
    return 0;
}
void tc_widget_text(void *widget, TinyString text) {
    TcWidget *w = (TcWidget *)widget;
    TinyString copy = tc_string_copy(text);
    tc_string_free(w->text);
    w->text = copy;
    if (w->buffer) {
        tc_gap_destroy(w->buffer);
        w->buffer = (TcGap *)tc_gap_create(text);
    }
}
TinyString tc_widget_get_text(void *widget) {
    TcWidget *w = (TcWidget *)widget;
    return w->buffer ? tc_gap_text(w->buffer) : tc_string_copy(w->text);
}
void *tc_widget_buffer(void *widget) {
    return ((TcWidget *)widget)->buffer;
}
void tc_widget_on_click(void *widget, void *closure) {
    TcWidget *w = (TcWidget *)widget;
    if (w->callback.owned)
        free(w->callback.environment);
    w->callback = *(TcUiClosure *)closure;
}
void tc_widget_size(void *widget, int32_t width, int32_t height) {
    TcWidget *w = (TcWidget *)widget;
    w->preferred_width = width;
    w->preferred_height = height;
}
static int tc_widget_natural(TcWidget *w, int vertical) {
    int wanted = vertical ? w->preferred_height : w->preferred_width;
    if (wanted > 0)
        return wanted;
    if (w->kind == TC_WIDGET_LABEL || w->kind == TC_WIDGET_BUTTON)
        return vertical ? 1 : (int)w->text.length + (w->kind == TC_WIDGET_BUTTON ? 4 : 0);
    return 0;
}
static void tc_widget_layout(TcWidget *w, int x, int y, int width, int height) {
    TcWidget *child;
    int vertical = w->kind != TC_WIDGET_ROW, total = 0, flex = 0, position = 0, available;
    w->x = x;
    w->y = y;
    w->width = width;
    w->height = height;
    if (w->kind == TC_WIDGET_PANEL) {
        x++;
        y++;
        width -= 2;
        height -= 2;
    }
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    available = vertical ? height : width;
    for (child = w->children; child; child = child->next) {
        int size = tc_widget_natural(child, vertical);
        if (size)
            total += size;
        else
            flex++;
    }
    for (child = w->children; child; child = child->next) {
        int size = tc_widget_natural(child, vertical);
        if (w->kind == TC_WIDGET_STACK) {
            tc_widget_layout(child, x, y, width, height);
            continue;
        }
        if (!size) {
            size = flex ? (available - total) / flex : 0;
            if (size < 0)
                size = 0;
            total += size;
            flex--;
        }
        if (size > available - position)
            size = available - position;
        if (size < 0)
            size = 0;
        tc_widget_layout(child, x + (vertical ? 0 : position), y + (vertical ? position : 0),
                         vertical ? width : size, vertical ? size : height);
        position += size;
    }
}
static void tc_widget_border(TcUi *ui, TcWidget *w) {
    int x, y;
    if (w->width < 2 || w->height < 2)
        return;
    for (x = w->x; x < w->x + w->width; x++) {
        tc_ui_put(ui, x, w->y, '-', 0x607d8b, 0x101820);
        tc_ui_put(ui, x, w->y + w->height - 1, '-', 0x607d8b, 0x101820);
    }
    for (y = w->y; y < w->y + w->height; y++) {
        tc_ui_put(ui, w->x, y, '|', 0x607d8b, 0x101820);
        tc_ui_put(ui, w->x + w->width - 1, y, '|', 0x607d8b, 0x101820);
    }
    tc_ui_text_clip(ui, w->x + 2, w->y, w->text, 0x8fd3ff, 0x101820, w->width - 4);
}
static void tc_widget_draw(TcUi *ui, TcWidget *w) {
    TcWidget *child;
    uint32_t fg = 0xe6edf3, bg = 0x101820;
    if (w->width <= 0 || w->height <= 0)
        return;
    if (w->kind == TC_WIDGET_PANEL)
        tc_widget_border(ui, w);
    if (w->kind == TC_WIDGET_LABEL)
        tc_ui_text_clip(ui, w->x, w->y, w->text, fg, bg, w->width);
    if (w->kind == TC_WIDGET_BUTTON) {
        int x;
        if (ui->focus == w) {
            bg = 0x64b5f6;
            fg = 0x101820;
        }
        for (x = 0; x < w->width; x++)
            tc_ui_put(ui, w->x + x, w->y, ' ', fg, bg);
        tc_ui_put(ui, w->x, w->y, '[', fg, bg);
        tc_ui_text_clip(ui, w->x + 2, w->y, w->text, fg, bg, w->width - 4);
        tc_ui_put(ui, w->x + w->width - 1, w->y, ']', fg, bg);
    }
    if (w->kind == TC_WIDGET_TEXTBOX && w->buffer) {
        TinyString text = tc_gap_text(w->buffer);
        size_t p = 0;
        int line = 0, column = 0, caret_line = tc_gap_line(w->buffer),
            caret_column = tc_gap_column(w->buffer);
        {
            size_t cursor = 0;
            caret_column = 0;
            while (cursor < w->buffer->start) {
                uint32_t cp = tc_utf8_decode(text, &cursor);
                if (cp == '\n')
                    caret_column = 0;
                else
                    caret_column += cp == '\t' ? 4 - (caret_column % 4) : tc_ui_width(cp);
            }
        }
        if (caret_line < w->scroll_line)
            w->scroll_line = caret_line;
        if (caret_line >= w->scroll_line + w->height)
            w->scroll_line = caret_line - w->height + 1;
        if (caret_column < w->scroll_column)
            w->scroll_column = caret_column;
        if (caret_column >= w->scroll_column + w->width)
            w->scroll_column = caret_column - w->width + 1;
        while (p < text.length && line < w->scroll_line + w->height) {
            uint32_t cp = tc_utf8_decode(text, &p);
            int cw = tc_ui_width(cp), j;
            if (cp == '\n') {
                line++;
                column = 0;
                continue;
            }
            if (cp == '\t')
                cw = 4 - (column % 4);
            if (line >= w->scroll_line && column >= w->scroll_column &&
                column + cw <= w->scroll_column + w->width) {
                if (cp == '\t')
                    for (j = 0; j < cw; j++)
                        tc_ui_put(ui, w->x + column - w->scroll_column + j,
                                  w->y + line - w->scroll_line, ' ', fg, bg);
                else
                    tc_ui_put(ui, w->x + column - w->scroll_column, w->y + line - w->scroll_line,
                              cp, fg, bg);
            }
            column += cw;
        }
        if (ui->focus == w) {
            int x = w->x + caret_column - w->scroll_column, y = w->y + caret_line - w->scroll_line;
            if (x >= w->x && x < w->x + w->width && y >= w->y && y < w->y + w->height) {
                TcUiCell *cell = &ui->back[y * ui->width + x];
                cell->foreground = 0x101820;
                cell->background = 0xe6edf3;
            }
        }
        tc_string_free(text);
    }
    for (child = w->children; child; child = child->next)
        tc_widget_draw(ui, child);
}
static TcWidget *tc_widget_focus_at(TcWidget *w, int *index, int target) {
    TcWidget *child, *found;
    if (w->kind == TC_WIDGET_BUTTON || w->kind == TC_WIDGET_TEXTBOX) {
        if ((*index)++ == target)
            return w;
    }
    for (child = w->children; child; child = child->next) {
        found = tc_widget_focus_at(child, index, target);
        if (found)
            return found;
    }
    return NULL;
}
void tc_ui_focus(void *window, void *widget) {
    ((TcUi *)window)->focus = (TcWidget *)widget;
}
void tc_ui_content(void *window, void *widget) {
    TcUi *ui = (TcUi *)window;
    int index = 0;
    TcWidget *root = (TcWidget *)widget;
    if (root && root->parent)
        return;
    if (ui->root && ui->root != root)
        tc_widget_free(ui->root);
    ui->root = root;
    ui->focus = root ? tc_widget_focus_at(root, &index, 0) : NULL;
    ui->focus_index = 0;
}
void tc_ui_draw(void *window) {
    TcUi *ui = (TcUi *)window;
    tc_ui_clear(ui, 0xe6edf3, 0x101820);
    if (ui->root) {
        tc_widget_layout(ui->root, 0, 0, ui->width, ui->height);
        tc_widget_draw(ui, ui->root);
    }
}
static TcWidget *tc_widget_hit(TcWidget *w, int x, int y) {
    TcWidget *child, *hit, *last = NULL;
    if (x < w->x || x >= w->x + w->width || y < w->y || y >= w->y + w->height)
        return NULL;
    for (child = w->children; child; child = child->next) {
        hit = tc_widget_hit(child, x, y);
        if (hit)
            last = hit;
    }
    return last ? last : (w->kind == TC_WIDGET_BUTTON || w->kind == TC_WIDGET_TEXTBOX) ? w : NULL;
}
int32_t tc_ui_dispatch(void *window, int32_t kind, int32_t key, int32_t x, int32_t y, int32_t width,
                       int32_t height, uint32_t cp) {
    TcUi *ui = (TcUi *)window;
    TcWidget *w = ui->focus;
    if (kind == TC_UI_RESIZE)
        return tc_ui_resize_impl(ui, width, height);
    if (kind == TC_UI_MOUSE && ui->root) {
        w = tc_widget_hit(ui->root, x, y);
        if (w)
            ui->focus = w;
        if (w && w->kind == TC_WIDGET_BUTTON && w->callback.invoke)
            w->callback.invoke(w->callback.environment);
        return 0;
    }
    if (kind != TC_UI_KEY)
        return 0;
    if (key == 9 && ui->root) {
        int index = 0, target = 0;
        TcWidget *candidate;
        while ((candidate = tc_widget_focus_at(ui->root, &index, target)) != NULL) {
            if (candidate == ui->focus)
                break;
            target++;
            index = 0;
        }
        index = 0;
        ui->focus_index = target + 1;
        ui->focus = tc_widget_focus_at(ui->root, &index, ui->focus_index);
        if (!ui->focus) {
            index = 0;
            ui->focus_index = 0;
            ui->focus = tc_widget_focus_at(ui->root, &index, 0);
        }
        return 0;
    }
    if (!w)
        return 0;
    if (w->kind == TC_WIDGET_BUTTON && (key == 13 || key == 32 || key == 10)) {
        if (w->callback.invoke)
            w->callback.invoke(w->callback.environment);
        return 0;
    }
    if (w->kind == TC_WIDGET_TEXTBOX && w->buffer) {
        if (key == TC_KEY_LEFT)
            tc_gap_left(w->buffer);
        else if (key == TC_KEY_RIGHT)
            tc_gap_right(w->buffer);
        else if (key == TC_KEY_UP)
            tc_gap_vertical(w->buffer, -1);
        else if (key == TC_KEY_DOWN)
            tc_gap_vertical(w->buffer, 1);
        else if (key == TC_KEY_HOME)
            tc_gap_home(w->buffer);
        else if (key == TC_KEY_END)
            tc_gap_end(w->buffer);
        else if (key == TC_KEY_PAGE_UP || key == TC_KEY_PAGE_DOWN) {
            int i;
            for (i = 0; i < w->height; i++)
                tc_gap_vertical(w->buffer, key == TC_KEY_PAGE_UP ? -1 : 1);
        } else if (key == 8 || key == 127)
            tc_gap_backspace(w->buffer);
        else if (key == TC_KEY_DELETE)
            tc_gap_delete(w->buffer);
        else if (key == 13 || key == 10) {
            if (w->height > 1)
                return tc_gap_insert(w->buffer, TC_STRING("\n"));
        } else if (cp >= 32 && cp != 127) {
            char encoded[4];
            TinyString s;
            s.length = (size_t)tc_utf8_encode(cp, encoded);
            s.data = encoded;
            return tc_gap_insert(w->buffer, s);
        }
    }
    return 0;
}
void tc_ui_destroy(void *window) {
    TcUi *ui = (TcUi *)window;
    if (!ui)
        return;
    if (tc_active_terminal == ui)
        tc_ui_restore();
    if (ui->root)
        tc_widget_free(ui->root);
    free(ui->front);
    free(ui->back);
    free(ui);
}
#endif
