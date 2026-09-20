#include "tiny.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef struct Formatter {
    Buffer output;
    int indent, paren, brace, brace_paren[256];
    int in_case[256], label, glue, previous_op;
    const char *previous;
} Formatter;
static void fmt_newline(Formatter *f) {
    while (f->output.len &&
           (f->output.data[f->output.len - 1] == ' ' || f->output.data[f->output.len - 1] == '\t'))
        f->output.data[--f->output.len] = 0;
    if (f->output.len && f->output.data[f->output.len - 1] != '\n')
        buf_add(&f->output, "\n");
    f->previous = "";
}
static void fmt_indent(Formatter *f) {
    int i;
    if (!f->output.len || f->output.data[f->output.len - 1] == '\n')
        for (i = 0; i < f->indent; i++)
            buf_add(&f->output, "    ");
}
static void fmt_comments(Formatter *f, const char *source, size_t begin, size_t end) {
    size_t p = begin;
    while (p < end) {
        if (source[p] == '/' && source[p + 1] == '/') {
            size_t start = p;
            while (p < end && source[p] != '\n')
                p++;
            fmt_newline(f);
            fmt_indent(f);
            buf_printf(&f->output, "%.*s", (int)(p - start), source + start);
            fmt_newline(f);
        } else if (source[p] == '/' && source[p + 1] == '*') {
            size_t start = p;
            int depth = 1;
            p += 2;
            while (p < end && depth) {
                if (source[p] == '/' && source[p + 1] == '*') {
                    depth++;
                    p += 2;
                } else if (source[p] == '*' && source[p + 1] == '/') {
                    depth--;
                    p += 2;
                } else
                    p++;
            }
            fmt_newline(f);
            fmt_indent(f);
            buf_printf(&f->output, "%.*s", (int)(p - start), source + start);
            fmt_newline(f);
        } else
            p++;
    }
}
/* Mirrors the parser: case and default are contextual, so they open a label only when a
   label can follow (a literal, a name or '-' after case; ':' after default). */
static int label_start(const Token *token, const Token *next) {
    if (token->kind != TK_ID)
        return 0;
    if (!strcmp(token->text, "default"))
        return next->kind == TK_OP && !strcmp(next->text, ":");
    if (!strcmp(token->text, "case"))
        return next->kind != TK_OP || !strcmp(next->text, "-");
    return 0;
}
int tc_format_file(const char *path, const char *output) {
    char *source = read_file(path);
    Context *c = (Context *)calloc(1, sizeof(*c));
    Formatter *f = (Formatter *)calloc(1, sizeof(*f));
    volatile int result = 1;
    if (!source || !c || !f) {
        free(source);
        free(c);
        free(f);
        fprintf(stderr, "tiny fmt: cannot read %s\n", path);
        return 1;
    }
    f->previous = "";
    if (setjmp(c->failure) == 0) {
        size_t i, position = 0, previous_end = 0;
        int line_number = 1, column = 1;
        if ((unsigned char)source[0] == 0xef && (unsigned char)source[1] == 0xbb &&
            (unsigned char)source[2] == 0xbf)
            position = previous_end = 3;
        lex(c, path, source);
        for (i = 0; i + 1 < c->ntokens; i++) {
            Token *token = &c->tokens[i];
            const char *text = token->text, *next = c->tokens[i + 1].text;
            int tight;
            while (source[position] && (line_number < token->loc.line || column < token->loc.col)) {
                if (source[position++] == '\n') {
                    line_number++;
                    column = 1;
                } else
                    column++;
            }
            fmt_comments(f, source, previous_end, position);
            previous_end = position + strlen(text);
            column += (int)strlen(text);
            position = previous_end;
            if (!strcmp(text, "}") && token->kind == TK_OP) {
                if (f->brace) {
                    if (f->in_case[f->brace]) {
                        f->in_case[f->brace] = 0;
                        f->indent--;
                    }
                    f->brace--;
                    f->indent--;
                }
                fmt_newline(f);
            }
            /* A case/default label at the start of a statement closes the previous
               case body; its statements are indented one level under the label. */
            if (label_start(token, &c->tokens[i + 1]) && !f->paren && f->brace &&
                (!f->output.len || f->output.data[f->output.len - 1] == '\n')) {
                if (f->in_case[f->brace]) {
                    f->in_case[f->brace] = 0;
                    f->indent--;
                }
                f->label = 1;
            }
            fmt_indent(f);
            tight = !strcmp(text, ";") || !strcmp(text, ",") || !strcmp(text, ")") ||
                    !strcmp(text, "]") || !strcmp(text, ".") || !strcmp(text, "->") ||
                    !strcmp(text, "::") || !strcmp(text, ":") || !strcmp(text, "[") ||
                    !strcmp(f->previous, ".") || !strcmp(f->previous, "->") ||
                    !strcmp(f->previous, "::") || !strcmp(f->previous, "(") ||
                    !strcmp(f->previous, "[") || !strcmp(f->previous, "@");
            if (!strcmp(text, "(") && strcmp(f->previous, "if") && strcmp(f->previous, "for") &&
                strcmp(f->previous, "while") && strcmp(f->previous, "switch"))
                tight = 1;
            if (f->glue)
                tight = 1;
            if (*f->previous && !tight)
                buf_add(&f->output, " ");
            buf_add(&f->output, text);
            /* Prefix operators stay attached to their operand: return -1, case -1, !ok. */
            f->glue = token->kind == TK_OP &&
                      (!strcmp(text, "-") || !strcmp(text, "+") || !strcmp(text, "!") ||
                       !strcmp(text, "~")) &&
                      (!*f->previous || !strcmp(f->previous, "return") ||
                       (f->label && !strcmp(f->previous, "case")) ||
                       (f->previous_op && strcmp(f->previous, ")") && strcmp(f->previous, "]")));
            f->previous_op = token->kind == TK_OP;
            f->previous = text;
            if (!strcmp(text, "("))
                f->paren++;
            if (!strcmp(text, ")") && f->paren)
                f->paren--;
            if (f->label && !f->paren && !strcmp(text, ":")) {
                f->label = 0;
                if (strcmp(next, "{")) {
                    f->in_case[f->brace] = 1;
                    f->indent++;
                    fmt_newline(f);
                }
            } else if (!strcmp(text, "{")) {
                if (f->brace >= 255)
                    tc_error(c, token->loc, "formatter nesting limit exceeded");
                f->brace_paren[f->brace++] = f->paren;
                f->in_case[f->brace] = 0;
                f->indent++;
                fmt_newline(f);
            } else if (!strcmp(text, ";") &&
                       (!f->paren || (f->brace && f->paren == f->brace_paren[f->brace - 1])))
                fmt_newline(f);
            else if (!strcmp(text, "}") && strcmp(next, ";") && strcmp(next, ",") &&
                     strcmp(next, ")") && strcmp(next, "]") && strcmp(next, "else"))
                fmt_newline(f);
        }
        fmt_comments(f, source, previous_end, strlen(source));
        fmt_newline(f);
        result = write_file(output, f->output.data ? f->output.data : "") ? 0 : 1;
    }
    tc_free(c);
    free(c);
    free(source);
    free(f->output.data);
    free(f);
    return result;
}

static void doc_comment(Buffer *out, const char *source, int line) {
    const char *p = source, *line_start = source, *block = NULL;
    int current = 1;
    while (*p && current < line) {
        if (*p == '\n') {
            const char *text = line_start;
            while (text < p && (*text == ' ' || *text == '\t'))
                text++;
            if (p - text >= 3 && !strncmp(text, "///", 3)) {
                if (!block)
                    block = line_start;
            } else
                block = NULL;
            current++;
            line_start = p + 1;
        }
        p++;
    }
    if (block) {
        while (block < line_start) {
            const char *end = strchr(block, '\n'), *text = block;
            if (!end)
                end = line_start;
            while (*text == ' ' || *text == '\t')
                text++;
            text += 3;
            if (*text == ' ')
                text++;
            buf_printf(out, "%.*s\n", (int)(end - text), text);
            block = end + 1;
        }
        buf_add(out, "\n");
    }
}
static void doc_type(Context *c, Buffer *out, Type *type) {
    TypeLink *item;
    if (!type) {
        buf_add(out, "<unknown>");
        return;
    }
    if (type->qualified) {
        Type plain = *type;
        plain.qualified = 0;
        plain.name += 6;
        buf_add(out, "const ");
        doc_type(c, out, &plain);
        return;
    }
    if (type->kind == TY_PTR || type->kind == TY_ARRAY) {
        doc_type(c, out, type->base);
        if (type->kind == TY_PTR)
            buf_add(out, "*");
        else
            buf_printf(out, "[%zu]", type->count);
    } else if (type->kind == TY_SLICE) {
        buf_add(out, "Slice<");
        doc_type(c, out, type->base);
        buf_add(out, ">");
    } else if (type->kind == TY_FUNC || type->kind == TY_CLOSURE) {
        buf_add(out, type->kind == TY_FUNC ? "func<" : "closure<");
        doc_type(c, out, type->base);
        buf_add(out, "(");
        for (item = type->items; item; item = item->next) {
            doc_type(c, out, item->type);
            if (item->next)
                buf_add(out, ", ");
        }
        buf_add(out, ")>");
    } else {
        if (type->kind != TY_TUPLE)
            buf_add(out, type_name(c, type));
        if (type->items || type->kind == TY_TUPLE) {
            buf_add(out, type->kind == TY_TUPLE ? "(" : "<");
            for (item = type->items; item; item = item->next) {
                doc_type(c, out, item->type);
                if (item->next)
                    buf_add(out, ", ");
            }
            buf_add(out, type->kind == TY_TUPLE ? ")" : ">");
        }
    }
}
static void doc_signature(Context *c, Buffer *out, Node *n) {
    Node *p;
    int first = 1;
    if (n->kind == N_ENUM) {
        buf_printf(out, "enum %s {\n", n->name);
        for (p = n->body; p; p = p->next)
            buf_printf(out, "    %s = %s%s\n", p->name, p->text, p->next ? "," : "");
        buf_add(out, "}\n");
        return;
    }
    if (n->flags & NF_STATIC)
        buf_add(out, "static ");
    if (n->flags & NF_ASYNC)
        buf_add(out, "async ");
    doc_type(c, out, n->decl_type);
    buf_printf(out, " %s", n->name);
    if (n->kind == N_FUNCTION) {
        buf_add(out, "(");
        for (p = n->params; p; p = p->next) {
            if (!first)
                buf_add(out, ", ");
            first = 0;
            if (p->decl_type) {
                doc_type(c, out, p->decl_type);
                buf_add(out, " ");
            }
            buf_add(out, p->name);
            if (p->a && p->a->text)
                buf_printf(out, " = %s", p->a->text);
        }
        buf_add(out, ")");
    }
    buf_add(out, ";\n");
}
int tc_document_file(const char *path, const char *output) {
    char *source = read_file(path);
    Context *c = (Context *)calloc(1, sizeof(*c));
    Buffer *out = (Buffer *)calloc(1, sizeof(*out));
    volatile int result = 1;
    if (!source || !c || !out) {
        free(source);
        free(c);
        free(out);
        fprintf(stderr, "tiny doc: cannot read %s\n", path);
        return 1;
    }
    if (setjmp(c->failure) == 0) {
        Node *n, *m;
        lex(c, path, source);
        parse(c);
        buf_printf(out, "# API reference\n\nSource: `%s`\n\n", path);
        for (n = c->program->body; n; n = n->next) {
            if (n->kind == N_MODULE) {
                buf_printf(out, "Module: `%s`\n\n", n->name);
                continue;
            }
            if (n->kind == N_IMPORT)
                continue;
            buf_printf(out, "## %s\n\n", n->name);
            doc_comment(out, source, n->loc.line);
            if (n->kind == N_CLASS || n->kind == N_INTERFACE || n->kind == N_EXTENSION) {
                buf_add(out, "```c\n");
                buf_printf(out, "%s %s",
                           n->kind == N_INTERFACE   ? "interface"
                           : n->kind == N_EXTENSION ? "extension"
                                                    : "class",
                           n->name);
                if (n->params) {
                    buf_add(out, "<");
                    for (m = n->params; m; m = m->next)
                        buf_printf(out, "%s%s", m->name, m->next ? ", " : "");
                    buf_add(out, ">");
                }
                buf_add(out, "\n```\n\n");
                for (m = n->body; m; m = m->next) {
                    buf_printf(out, "### %s.%s\n\n", n->name, m->name);
                    doc_comment(out, source, m->loc.line);
                    buf_add(out, "```c\n");
                    doc_signature(c, out, m);
                    buf_add(out, "```\n\n");
                }
            } else {
                buf_add(out, "```c\n");
                doc_signature(c, out, n);
                buf_add(out, "```\n\n");
            }
        }
        if (output)
            result = write_file(output, out->data) ? 0 : 1;
        else {
            fputs(out->data, stdout);
            result = 0;
        }
    }
    tc_free(c);
    free(c);
    free(source);
    free(out->data);
    free(out);
    return result;
}

static int repl_declaration(const char *history, const char *input) {
    Context *c = (Context *)calloc(1, sizeof(*c));
    Buffer source = {0};
    volatile int declaration = 0;
    if (!c)
        return 0;
    c->quiet = 1;
    buf_add(&source, history);
    buf_add(&source, input);
    if (setjmp(c->failure) == 0) {
        lex(c, "<repl>", source.data);
        parse(c);
        declaration = 1;
    }
    tc_free(c);
    free(c);
    free(source.data);
    return declaration;
}
static int repl_balance(const char *source) {
    int depth = 0, quote = 0, block = 0;
    const char *p;
    for (p = source; *p; p++) {
        if (quote) {
            if (*p == '\\' && p[1])
                p++;
            else if (*p == quote)
                quote = 0;
            continue;
        }
        if (block) {
            if (*p == '/' && p[1] == '*') {
                block++;
                p++;
            } else if (*p == '*' && p[1] == '/') {
                block--;
                p++;
            }
            continue;
        }
        if (*p == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;
            if (!*p)
                break;
            continue;
        }
        if (*p == '/' && p[1] == '*') {
            block++;
            p++;
            continue;
        }
        if (*p == '"' || *p == '\'') {
            quote = *p;
            continue;
        }
        if (*p == '{' || *p == '(' || *p == '[')
            depth++;
        if (*p == '}' || *p == ')' || *p == ']')
            depth--;
    }
    return depth > 0 || block || quote;
}
int tc_repl(const char *root) {
    Buffer history = {0}, input = {0};
    char line[8192], path[4096];
    int result = 0;
#ifdef _WIN32
    if (!GetTempFileNameA(".", "tcr", 0, path)) {
        fputs("tiny repl: cannot create session source\n", stderr);
        return 1;
    }
#else
    {
        int fd;
        strcpy(path, "./.tiny-repl-XXXXXX");
        fd = mkstemp(path);
        if (fd < 0)
            return 1;
        close(fd);
    }
#endif
    puts("TinyC+ REPL | :help :reset :quit");
    for (;;) {
        size_t length;
        int declaration;
        Buffer source = {0};
        fputs(input.len ? "... " : ">>> ", stdout);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        if (!input.len && line[0] == ':') {
            line[strcspn(line, "\r\n")] = 0;
            if (!strcmp(line, ":quit"))
                break;
            if (!strcmp(line, ":reset")) {
                tc_repl_clear();
                free(history.data);
                memset(&history, 0, sizeof(history));
                puts("Session reset.");
                continue;
            }
            if (!strcmp(line, ":help")) {
                puts("Declare variables/functions; evaluate scalar/string expressions without "
                     "';'.\nStatements execute once. State and compiled function pointers "
                     "persist.\nFree manually owned resources before :reset or :quit. Maximum 128 "
                     "cells / 64 MiB state.");
                continue;
            }
            puts("Unknown command. Use :help.");
            continue;
        }
        buf_add(&input, line);
        if (input.len > 1024u * 1024u) {
            fputs("tiny repl: cell exceeds 1 MiB\n", stderr);
            result = 1;
            break;
        }
        if (repl_balance(input.data))
            continue;
        length = input.len;
        while (length && isspace((unsigned char)input.data[length - 1]))
            length--;
        input.data[length] = 0;
        input.len = length;
        if (!length)
            continue;
        declaration = repl_declaration(history.data ? history.data : "", input.data);
        buf_add(&source, history.data ? history.data : "");
        if (declaration) {
            buf_add(&source, input.data);
            buf_add(&source, "\nint main() { return 0; }\n");
        } else {
            buf_add(&source, "\nint main() {\n");
            if (input.data[length - 1] != ';' && input.data[length - 1] != '}') {
                buf_add(&source, "println(");
                buf_add(&source, input.data);
                buf_add(&source, ");\n");
            } else {
                buf_add(&source, input.data);
                buf_add(&source, "\n");
            }
            buf_add(&source, "return 0;\n}\n");
        }
        if (write_file(path, source.data) &&
            tc_compile(path, NULL, 9, 1, root, 0, NULL, NULL) == 0 && declaration) {
            buf_add(&history, input.data);
            buf_add(&history, "\n");
        }
        free(source.data);
        free(input.data);
        memset(&input, 0, sizeof(input));
    }
    tc_repl_clear();
    remove(path);
    free(history.data);
    free(input.data);
    return result;
}
