/*
 * Buffer de edición con un hueco móvil entre dos regiones de texto.
 * Insertar cerca del cursor aprovecha el hueco; moverlo desplaza bytes.
 * Los movimientos de caracteres respetan las secuencias UTF-8; las posiciones
 * del almacenamiento se expresan en bytes. El handle tiene destroy explícito.
 */
#ifndef TC_GAP_IMPLEMENTATION
#define TC_GAP_IMPLEMENTATION
#include "library.c"

typedef struct TcGapSnapshot {
    char *text;
    size_t size, position;
    uint64_t revision;
    struct TcGapSnapshot *next;
} TcGapSnapshot;
typedef struct TcGap {
    char *data;
    size_t start, end, capacity;
    int dirty, history_enabled;
    uint64_t revision, saved_revision, next_revision;
    TcGapSnapshot *undo, *redo;
} TcGap;
static size_t tc_gap_size(TcGap *g) {
    return g->capacity - (g->end - g->start);
}
static unsigned char tc_gap_byte(TcGap *g, size_t position) {
    return (unsigned char)g->data[position < g->start ? position : position + g->end - g->start];
}
static void tc_gap_history_free(TcGapSnapshot *snapshot) {
    while (snapshot) { TcGapSnapshot *next = snapshot->next; free(snapshot->text); free(snapshot); snapshot = next; }
}
static TcGapSnapshot *tc_gap_capture(TcGap *g) {
    size_t size = tc_gap_size(g);
    TcGapSnapshot *s = (TcGapSnapshot *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->text = (char *)malloc(size + 1);
    if (!s->text) { free(s); return NULL; }
    memcpy(s->text, g->data, g->start);
    memcpy(s->text + g->start, g->data + g->end, g->capacity - g->end);
    s->size = size; s->position = g->start; s->revision = g->revision;
    return s;
}
static int tc_gap_record(TcGap *g) {
    TcGapSnapshot *s, **cursor;
    size_t count = 0, bytes = 0;
    if (g->history_enabled) {
        if (tc_gap_size(g) > 8u * 1024u * 1024u) {
            tc_gap_history_free(g->undo); g->undo = NULL;
        } else {
            s = tc_gap_capture(g);
            if (!s) return 8;
            s->next = g->undo; g->undo = s;
            cursor = &g->undo;
            while ((s = *cursor) != NULL) {
                if (++count > 64 || s->size > 8u * 1024u * 1024u - bytes) {
                    tc_gap_history_free(s); *cursor = NULL; break;
                }
                bytes += s->size; cursor = &s->next;
            }
        }
        tc_gap_history_free(g->redo); g->redo = NULL;
    }
    g->revision = ++g->next_revision;
    return 0;
}
void tc_gap_history(void *buffer, int32_t enabled) {
    TcGap *g = (TcGap *)buffer;
    tc_gap_history_free(g->undo); tc_gap_history_free(g->redo);
    g->undo = g->redo = NULL; g->history_enabled = enabled != 0;
}
int32_t tc_gap_undo_redo(void *buffer, int32_t redo) {
    TcGap *g = (TcGap *)buffer;
    TcGapSnapshot **from = redo ? &g->redo : &g->undo;
    TcGapSnapshot **to = redo ? &g->undo : &g->redo;
    TcGapSnapshot *saved = *from, *current;
    if (!saved) return 1;
    current = tc_gap_capture(g);
    if (!current) return 8;
    *from = saved->next; current->next = *to; *to = current;
    g->start = saved->position; g->end = g->capacity - (saved->size - saved->position);
    memcpy(g->data, saved->text, g->start);
    memcpy(g->data + g->end, saved->text + g->start, saved->size - g->start);
    g->revision = saved->revision; g->dirty = g->revision != g->saved_revision;
    free(saved->text); free(saved); return 0;
}
static void tc_gap_seek_impl(TcGap *g, size_t position) {
    if (position > tc_gap_size(g))
        position = tc_gap_size(g);
    if (position < g->start) {
        size_t n = g->start - position;
        memmove(g->data + g->end - n, g->data + position, n);
        g->start -= n;
        g->end -= n;
    } else if (position > g->start) {
        size_t n = position - g->start;
        memmove(g->data + g->start, g->data + g->end, n);
        g->start += n;
        g->end += n;
    }
}
void *tc_gap_create(TinyString input) {
    TcGap *g = (TcGap *)tc_alloc_checked(sizeof(*g));
    if (input.length > 64u * 1024u * 1024u) {
        free(g);
        return NULL;
    }
    g->capacity = input.length + 128;
    g->data = (char *)tc_alloc_checked(g->capacity);
    g->end = g->capacity - input.length;
    if (input.length)
        memcpy(g->data + g->end, input.data, input.length);
    return g;
}
void tc_gap_destroy(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    if (g) {
        free(g->data);
        tc_gap_history_free(g->undo); tc_gap_history_free(g->redo);
        free(g);
    }
}
uint64_t tc_gap_length(void *buffer) {
    return tc_gap_size((TcGap *)buffer);
}
uint64_t tc_gap_position(void *buffer) {
    return ((TcGap *)buffer)->start;
}
void tc_gap_seek(void *buffer, uint64_t position) {
    TcGap *g = (TcGap *)buffer;
    if (position > tc_gap_size(g))
        position = tc_gap_size(g);
    tc_gap_seek_impl(g, (size_t)position);
}
int32_t tc_gap_insert(void *buffer, TinyString input) {
    TcGap *g = (TcGap *)buffer;
    size_t size = tc_gap_size(g);
    if (input.length > 64u * 1024u * 1024u - size)
        return 8;
    if (input.length > g->end - g->start) {
        size_t capacity = g->capacity * 2;
        char *data;
        if (capacity < size + input.length + 128)
            capacity = size + input.length + 128;
        data = (char *)malloc(capacity);
        if (!data)
            return 8;
        memcpy(data, g->data, g->start);
        memcpy(data + capacity - (g->capacity - g->end), g->data + g->end, g->capacity - g->end);
        g->end = capacity - (g->capacity - g->end);
        g->capacity = capacity;
        free(g->data);
        g->data = data;
    }
    if (input.length) {
        if (tc_gap_record(g)) return 8;
        memcpy(g->data + g->start, input.data, input.length);
        g->start += input.length;
        g->dirty = 1;
        if (g->history_enabled && size + input.length > 8u * 1024u * 1024u) {
            tc_gap_history_free(g->undo); tc_gap_history_free(g->redo); g->undo = g->redo = NULL;
        }
    }
    return 0;
}
static size_t tc_gap_previous(TcGap *g, size_t position) {
    if (position) {
        position--;
        while (position && (tc_gap_byte(g, position) & 0xc0) == 0x80)
            position--;
    }
    return position;
}
static size_t tc_gap_next(TcGap *g, size_t position) {
    size_t size = tc_gap_size(g);
    if (position < size) {
        position++;
        while (position < size && (tc_gap_byte(g, position) & 0xc0) == 0x80)
            position++;
    }
    return position;
}
void tc_gap_left(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    tc_gap_seek_impl(g, tc_gap_previous(g, g->start));
}
void tc_gap_right(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    tc_gap_seek_impl(g, tc_gap_next(g, g->start));
}
int32_t tc_gap_backspace(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    size_t start = tc_gap_previous(g, g->start);
    if (start != g->start) {
        if (tc_gap_record(g)) return 8;
        g->start = start;
        g->dirty = 1;
    }
    return 0;
}
int32_t tc_gap_delete(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    size_t n = tc_gap_next(g, g->start) - g->start;
    if (n && tc_gap_record(g)) return 8;
    g->end += n;
    if (n)
        g->dirty = 1;
    return 0;
}
static size_t tc_gap_line_start(TcGap *g, size_t p) {
    while (p && tc_gap_byte(g, p - 1) != '\n')
        p--;
    return p;
}
static size_t tc_gap_line_end(TcGap *g, size_t p) {
    size_t n = tc_gap_size(g);
    while (p < n && tc_gap_byte(g, p) != '\n')
        p++;
    return p;
}
void tc_gap_home(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    tc_gap_seek_impl(g, tc_gap_line_start(g, g->start));
}
void tc_gap_end(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    tc_gap_seek_impl(g, tc_gap_line_end(g, g->start));
}
int32_t tc_gap_line(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    size_t p;
    int line = 0;
    for (p = 0; p < g->start; p++)
        if (tc_gap_byte(g, p) == '\n')
            line++;
    return line;
}
int32_t tc_gap_column(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    size_t p = tc_gap_line_start(g, g->start);
    int column = 0;
    while (p < g->start) {
        p = tc_gap_next(g, p);
        column++;
    }
    return column;
}
void tc_gap_vertical(void *buffer, int32_t direction) {
    TcGap *g = (TcGap *)buffer;
    size_t p = tc_gap_line_start(g, g->start), end;
    int column = tc_gap_column(g), i;
    if (direction < 0) {
        if (!p)
            return;
        p = tc_gap_line_start(g, p - 1);
    } else {
        p = tc_gap_line_end(g, p);
        if (p == tc_gap_size(g))
            return;
        p++;
    }
    end = tc_gap_line_end(g, p);
    for (i = 0; i < column && p < end; i++)
        p = tc_gap_next(g, p);
    tc_gap_seek_impl(g, p);
}
void tc_gap_goto_line(void *buffer, int32_t line) {
    TcGap *g = (TcGap *)buffer;
    size_t p = 0, n = tc_gap_size(g);
    int current = 0;
    while (p < n && current < line)
        if (tc_gap_byte(g, p++) == '\n')
            current++;
    tc_gap_seek_impl(g, p);
}
int32_t tc_gap_dirty(void *buffer) {
    return ((TcGap *)buffer)->dirty;
}
void tc_gap_clean(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    g->dirty = 0; g->saved_revision = g->revision;
}
TinyString tc_gap_text(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    size_t n = tc_gap_size(g);
    char *text = (char *)malloc(n + 1);
    TinyString result = {NULL, 0};
    if (!text)
        return result;
    memcpy(text, g->data, g->start);
    memcpy(text + g->start, g->data + g->end, g->capacity - g->end);
    text[n] = 0;
    result.data = text;
    result.length = n;
    return result;
}
int64_t tc_gap_find(void *buffer, TinyString needle, uint64_t start) {
    TcGap *g = (TcGap *)buffer;
    size_t n = tc_gap_size(g), p, j;
    if (!needle.length || start > n || needle.length > n)
        return -1;
    for (p = (size_t)start; p <= n - needle.length; p++) {
        for (j = 0; j < needle.length && tc_gap_byte(g, p + j) == (unsigned char)needle.data[j];
             j++) {
        }
        if (j == needle.length)
            return (int64_t)p;
    }
    return -1;
}
TinyString tc_gap_cut_line(void *buffer) {
    TcGap *g = (TcGap *)buffer;
    size_t start = tc_gap_line_start(g, g->start), end = tc_gap_line_end(g, g->start), i;
    TinyString result = {NULL, 0};
    char *data;
    if (end < tc_gap_size(g))
        end++;
    data = (char *)malloc(end - start + 1);
    if (!data)
        return result;
    for (i = start; i < end; i++)
        data[i - start] = (char)tc_gap_byte(g, i);
    data[end - start] = 0;
    result.data = data;
    result.length = end - start;
    if (end > start && tc_gap_record(g)) { free(data); result.data = NULL; result.length = 0; return result; }
    tc_gap_seek_impl(g, start);
    g->end += end - start;
    if (end > start)
        g->dirty = 1;
    return result;
}
#endif
