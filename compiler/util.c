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
}
void tc_error(Context *c, Loc l, const char *fmt, ...) {
    va_list a;
    if (!c->quiet) {
        fprintf(stderr, "%s:%d:%d: error: ", l.file ? l.file : "<source>", l.line, l.col);
        va_start(a, fmt);
        vfprintf(stderr, fmt, a);
        va_end(a);
        fputc('\n', stderr);
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
    FILE *f = fopen(path, "rb");
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
    FILE *f = fopen(path, "wb");
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
