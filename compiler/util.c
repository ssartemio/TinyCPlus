/*
 * Infraestructura del frontend: arena, cadenas, buffers e I/O de fuentes.
 * tc_alloc devuelve memoria asociada a Context; tc_free libera la unidad.
 * Buffer crece con reserva propia. tc_error informa Loc y salta al manejador
 * establecido por el llamador; no se debe usar sin inicializar failure.
 */
#include "tiny.h"

struct Arena {
    Arena *next;
    max_align_t alignment;
    unsigned char data[1];
};
void *tc_alloc(Context *c, size_t n) {
    Arena *a;
    if (n > SIZE_MAX - sizeof(Arena)) {
        fputs("tiny: allocation overflow\n", stderr);
        exit(2);
    }
    a = (Arena *)calloc(1, sizeof(Arena) + n);
    if (!a) {
        fputs("tiny: out of memory\n", stderr);
        exit(2);
    }
    a->next = c->arena;
    c->arena = a;
    return a->data;
}
char *tc_strn(Context *c, const char *s, size_t n) {
    char *p = (char *)tc_alloc(c, n + 1);
    memcpy(p, s, n);
    return p;
}
char *tc_str(Context *c, const char *s) {
    return tc_strn(c, s, strlen(s));
}
char *tc_format(Context *c, const char *fmt, ...) {
    va_list a, b;
    int n;
    char *p;
    va_start(a, fmt);
    va_copy(b, a);
    n = vsnprintf(NULL, 0, fmt, b);
    va_end(b);
    if (n < 0) {
        va_end(a);
        return tc_str(c, "");
    }
    p = (char *)tc_alloc(c, (size_t)n + 1);
    vsnprintf(p, (size_t)n + 1, fmt, a);
    va_end(a);
    return p;
}
void tc_free(Context *c) {
    Arena *p = c->arena;
    while (p) {
        Arena *q = p->next;
        free(p);
        p = q;
    }
    free(c->tokens);
    c->tokens = NULL;
    c->arena = NULL;
    c->sources = NULL;
    c->diagnostic_trace = NULL;
}
void tc_source(Context *c, const char *file, const char *text) {
    SourceFile *source;
    for (source = c->sources; source; source = source->next)
        if (!strcmp(source->file, file))
            return;
    source = (SourceFile *)tc_alloc(c, sizeof(*source));
    source->file = tc_str(c, file);
    if (strlen(text) >= 3 && (unsigned char)text[0] == 0xef &&
        (unsigned char)text[1] == 0xbb && (unsigned char)text[2] == 0xbf) text += 3;
    source->text = tc_str(c, text);
    source->next = c->sources;
    c->sources = source;
}
/* Render bounded source excerpts; columns from the lexer are byte offsets.
   Tabs expand at 8-column stops, controls are escaped, UTF-8 continuation bytes
   do not consume an extra caret column. No terminal color/escape sequences. */
static size_t diagnostic_width(unsigned char ch, size_t column, int print) {
    size_t width = 1, i;
    if (ch == '\t') {
        width = 8 - column % 8;
        if (print) for (i = 0; i < width; i++) fputc(' ', stderr);
    } else if (ch < 32 || ch == 127) {
        width = 4;
        if (print) fprintf(stderr, "\\x%02x", ch);
    } else {
        if ((ch & 0xc0) == 0x80) width = 0;
        if (print) fputc(ch, stderr);
    }
    return width;
}
static void diagnostic_source(Context *c, Loc loc) {
    SourceFile *source;
    const char *line, *end;
    size_t size, offset, start, stop, i, column = 0, caret = 0, width = 1, token_end;
    int number, digits;
    if (!loc.file || loc.line < 1 || loc.col < 1) return;
    for (source = c->sources; source; source = source->next)
        if (!strcmp(source->file, loc.file)) break;
    if (!source) return;
    line = source->text;
    for (number = 1; number < loc.line; number++) {
        line = strchr(line, '\n');
        if (!line) return;
        line++;
    }
    end = strchr(line, '\n');
    size = end ? (size_t)(end - line) : strlen(line);
    if (size && line[size - 1] == '\r') size--;
    offset = (size_t)loc.col - 1;
    if (offset > size) offset = size;
    start = offset > 60 ? offset - 60 : 0;
    while (start && ((unsigned char)line[start] & 0xc0) == 0x80) start--;
    stop = size < start + 140 ? size : start + 140;
    while (stop < size && ((unsigned char)line[stop] & 0xc0) == 0x80) stop++;
    token_end = offset;
    while (token_end < stop && (isalnum((unsigned char)line[token_end]) || line[token_end] == '_'))
        token_end++;
    if (offset < stop && (line[offset] == '"' || line[offset] == '\'')) {
        char quote = line[offset];
        token_end = offset + 1;
        while (token_end < stop) {
            char ch = line[token_end++];
            if (ch == quote) break;
            if (ch == '\\' && token_end < stop) token_end++;
        }
    }
    fprintf(stderr, " %d | %s", loc.line, start ? "..." : "");
    for (i = start; i < stop; i++) {
        if (i == offset) caret = column;
        column += diagnostic_width((unsigned char)line[i], column, 1);
    }
    if (offset == stop) caret = column;
    digits = snprintf(NULL, 0, "%d", loc.line);
    fprintf(stderr, "%s\n %*s | %s", stop < size ? "..." : "", digits, "", start ? "   " : "");
    for (i = 0; i < caret; i++) fputc(' ', stderr);
    fputc('^', stderr);
    if (token_end > offset) {
        width = 0;
        for (i = offset; i < token_end; i++)
            width += diagnostic_width((unsigned char)line[i], caret + width, 0);
    }
    for (i = 1; i < width; i++) fputc('~', stderr);
    fputc('\n', stderr);
}
void tc_json_string(FILE *out, const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    fputc('"', out);
    for (; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
        else if (*p < 32) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}
void tc_error(Context *c, Loc l, const char *fmt, ...) {
    va_list a;
    if (c->diagnostic_json && !c->quiet) {
        char message[4096];
        va_start(a, fmt); vsnprintf(message, sizeof(message), fmt, a); va_end(a);
        fputs("{\"schemaVersion\":1,\"ok\":false,\"diagnostics\":[{\"severity\":\"error\",\"file\":", stdout);
        tc_json_string(stdout, l.file);
        fprintf(stdout, ",\"line\":%d,\"column\":%d,\"message\":", l.line, l.col);
        tc_json_string(stdout, message);
        fputs(",\"instantiations\":[", stdout);
        {
            Instantiation *trace = c->diagnostic_trace;
            int depth = 0;
            while (trace && depth < 16) {
                if (depth++) fputc(',', stdout);
                fputs("{\"name\":", stdout); tc_json_string(stdout, trace->name);
                fputs(",\"file\":", stdout); tc_json_string(stdout, trace->loc.file);
                fprintf(stdout, ",\"line\":%d,\"column\":%d}", trace->loc.line, trace->loc.col);
                trace = trace->parent;
            }
        }
        fputs("]}]}\n", stdout);
    } else if (!c->quiet) {
        fprintf(stderr, "%s:%d:%d: error: ", l.file ? l.file : "<source>", l.line, l.col);
        va_start(a, fmt);
        vfprintf(stderr, fmt, a);
        va_end(a);
        fputc('\n', stderr);
        diagnostic_source(c, l);
        {
            Instantiation *trace = c->diagnostic_trace;
            int depth = 0;
            while (trace && depth++ < 16) {
                fprintf(stderr, "%s:%d:%d: note: while instantiating '%s'\n",
                        trace->loc.file ? trace->loc.file : "<source>", trace->loc.line,
                        trace->loc.col, trace->name);
                diagnostic_source(c, trace->loc);
                trace = trace->parent;
            }
            if (trace) fputs("note: further generic instantiation context omitted\n", stderr);
        }
    }
    c->errors++;
    longjmp(c->failure, 1);
}
static void buf_grow(Buffer *b, size_t n) {
    size_t cap;
    char *p;
    if (n > SIZE_MAX - b->len - 1) {
        fputs("tiny: buffer overflow\n", stderr);
        exit(2);
    }
    if (b->len + n + 1 <= b->cap)
        return;
    cap = b->cap ? b->cap : 256;
    while (cap < b->len + n + 1) {
        if (cap > SIZE_MAX / 2) {
            cap = b->len + n + 1;
            break;
        }
        cap *= 2;
    }
    p = (char *)realloc(b->data, cap);
    if (!p) {
        fputs("tiny: out of memory\n", stderr);
        exit(2);
    }
    b->data = p;
    b->cap = cap;
}
void buf_add(Buffer *b, const char *s) {
    size_t n = strlen(s);
    buf_grow(b, n);
    memcpy(b->data + b->len, s, n + 1);
    b->len += n;
}
void buf_printf(Buffer *b, const char *fmt, ...) {
    va_list a, d;
    int n;
    va_start(a, fmt);
    va_copy(d, a);
    n = vsnprintf(NULL, 0, fmt, d);
    va_end(d);
    if (n >= 0) {
        buf_grow(b, (size_t)n);
        vsnprintf(b->data + b->len, (size_t)n + 1, fmt, a);
        b->len += (size_t)n;
    }
    va_end(a);
}
char *read_file(const char *path) {
    FILE *f = tc_fopen(path, "rb");
    long n;
    char *s;
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || n > 32 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    s = (char *)malloc((size_t)n + 1);
    if (!s) {
        fclose(f);
        return NULL;
    }
    if (fread(s, 1, (size_t)n, f) != (size_t)n) {
        free(s);
        fclose(f);
        return NULL;
    }
    s[n] = 0;
    fclose(f);
    return s;
}
int write_file(const char *path, const char *s) {
    FILE *f = tc_fopen(path, "wb");
    size_t n = strlen(s);
    int ok;
    if (!f)
        return 0;
    ok = fwrite(s, 1, n, f) == n;
    if (fclose(f) != 0)
        ok = 0;
    return ok;
}
Node *node(Context *c, NodeKind k, Loc l) {
    Node *n = (Node *)tc_alloc(c, sizeof(Node));
    n->kind = k;
    n->loc = l;
    n->id = ++c->next_id;
    return n;
}
