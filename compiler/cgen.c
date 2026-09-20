#include "tiny.h"

typedef struct Cleanup {
    Node *node;
    struct Cleanup *next;
} Cleanup;
typedef struct Loop {
    int id;
    Cleanup *stop;
    struct Loop *parent;
} Loop;
typedef struct CG {
    Context *c;
    Buffer out, frame;
    int indent, serial, asynchronous, states;
    Cleanup *cleanup;
    Loop *loop;
    Node *fn, *fields, *returning;
} CG;
typedef struct ArgValue {
    Node *arg;
    const char *value;
    struct ArgValue *next;
} ArgValue;
static const char *expression(CG *, Node *);
static const char *value(CG *, Node *);
static void statement(CG *, Node *);
static void scoped(CG *, Node *);
static void cleanup_emit(CG *, Cleanup *);
static void line(CG *g, const char *fmt, ...) {
    va_list a;
    int i, n;
    char tmp[4096];
    for (i = 0; i < g->indent; i++)
        buf_add(&g->out, "    ");
    va_start(a, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, a);
    va_end(a);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        tc_error(g->c, g->fn ? g->fn->loc : g->c->program->loc,
                 "generated statement exceeds length limit");
    buf_add(&g->out, tmp);
    buf_add(&g->out, "\n");
}
static const char *temp(CG *g) {
    return tc_format(g->c, g->asynchronous ? "tc_async->tc_tmp_%d" : "tc_tmp_%d", ++g->serial);
}
static void frame_field(CG *g, const char *type, const char *name) {
    Node *n;
    if (!strncmp(name, "tc_async->", 10))
        name += 10;
    for (n = g->fields; n; n = n->next)
        if (!strcmp(n->name, name))
            return;
    n = node(g->c, N_VAR, g->fn->loc);
    n->name = name;
    n->next = g->fields;
    g->fields = n;
    if (!strncmp(type, "const ", 6) && !strchr(type, '*'))
        type += 6;
    buf_printf(&g->frame, "    %s %s;\n", type, name);
}
static void define_value(CG *g, const char *type, const char *name, const char *initial) {
    if (g->asynchronous) {
        frame_field(g, type, name);
        line(g, "%s = %s%s%s;", name, initial[0] == '{' ? "(" : "", initial[0] == '{' ? type : "",
             initial[0] == '{' ? tc_format(g->c, ")%s", initial) : initial);
    } else
        line(g, "%s %s = %s;", type, name, initial);
}
static void async_finish(CG *g, const char *result, const char *error) {
    line(g, "tc_future_complete(tc_async->future, %s, %s);",
         result ? tc_format(g->c, "&(%s)", result) : "NULL", error);
    line(g, "tc_future_release(tc_async->future);");
    line(g, "tc_task_end();");
    line(g, "free(tc_async);");
    line(g, "return;");
}
static const char *quote(Context *c, const char *s) {
    Buffer b = {0};
    const unsigned char *p = (const unsigned char *)s;
    char *r;
    buf_add(&b, "\"");
    for (; *p; p++) {
        if (*p == '\\' || *p == '"')
            buf_printf(&b, "\\%c", *p);
        else if (*p < 32)
            buf_printf(&b, "\\%03o", *p);
        else
            buf_printf(&b, "%c", *p);
    }
    buf_add(&b, "\"");
    r = tc_str(c, b.data);
    free(b.data);
    return r;
}
static const char *literal(Context *c, const char *s) {
    Buffer b = {0};
    const char *p;
    char *result;
    for (p = s; *p; p++) {
        if (*p == '\\' && p[1] == 'x') {
            char digits[3] = {p[2], p[3], 0};
            buf_printf(&b, "\\%03lo", strtoul(digits, NULL, 16));
            p += 3;
        } else if (*p == '\\' && p[1]) {
            buf_printf(&b, "\\%c", p[1]);
            p++;
        } else
            buf_printf(&b, "%c", *p);
    }
    result = tc_str(c, b.data);
    free(b.data);
    return result;
}
static const char *convert(CG *g, Type *to, Type *from, const char *s) {
    if (to->kind == TY_SLICE && from->kind == TY_ARRAY)
        return tc_format(g->c, "(%s){(%s).data, %zu}", to->cname, s, from->count);
    if (to->decl && to->decl->kind == N_INTERFACE && from->kind == TY_PTR)
        return tc_format(g->c, "(%s){%s, &tc_vtable_%d_%d}", to->cname, s, to->id, from->base->id);
    if (to->kind == TY_CLOSURE && from->kind == TY_CLOSURE && to != from)
        return tc_format(g->c, "(%s){(%s).env, (%s).invoke, (%s).owned}", to->cname, s, s, s);
    return s;
}
static const char *value(CG *g, Node *n) {
    const char *s = expression(g, n), *v;
    if (n->type->kind == TY_VOID) {
        line(g, "%s;", s);
        return "0";
    }
    v = temp(g);
    define_value(g, n->type->cname, v, s);
    return v;
}
static const char *object_pointer(CG *g, Node *n) {
    if (n->type->kind == TY_PTR)
        return value(g, n);
    return tc_format(g->c, "&(%s)", expression(g, n));
}
static ArgValue *arg_values(CG *g, Node *args) {
    ArgValue *head = NULL, **tail = &head;
    Node *a;
    for (a = args; a; a = a->next) {
        ArgValue *v = (ArgValue *)tc_alloc(g->c, sizeof(ArgValue));
        v->arg = a;
        v->value = value(g, a);
        *tail = v;
        tail = &v->next;
    }
    return head;
}
static void append_args(CG *g, Buffer *b, ArgValue *values, Node *params, int first) {
    Node *p;
    ArgValue *v;
    if (params) {
        for (p = params; p; p = p->next) {
            if (!strcmp(p->name, "...")) {
                for (v = values; v; v = v->next)
                    if (!v->arg->parameter) {
                        if (!first)
                            buf_add(b, ", ");
                        first = 0;
                        buf_add(b, v->value);
                    }
                break;
            }
            for (v = values; v && v->arg->parameter != p; v = v->next) {
            }
            if (v) {
                if (!first)
                    buf_add(b, ", ");
                first = 0;
                buf_add(b, convert(g, p->decl_type, v->arg->type, v->value));
            }
        }
    } else
        for (v = values; v; v = v->next) {
            if (!first)
                buf_add(b, ", ");
            first = 0;
            buf_add(b, v->value);
        }
}
static const char *stream_invoke(CG *g, Node *fn, const char *callback, const char *first,
                                 const char *second) {
    if (fn->type->kind == TY_CLOSURE)
        return tc_format(g->c, "%s.invoke(%s.env, %s%s%s)", callback, callback, first,
                         second ? ", " : "", second ? second : "");
    return tc_format(g->c, "%s(%s%s%s)", callback, first, second ? ", " : "", second ? second : "");
}
static const char *stream_expression(CG *g, Node *n) {
    Node *stages[128], *base = n->a->a, *callback = NULL;
    int count = 0, i;
    Context *c = g->c;
    const char *source, *data, *length, *index, *item, *result = NULL, *terminal = NULL;
    Type *source_type;
    while (strcmp(base->a->name, "stream")) {
        stages[count++] = base;
        base = base->a->a;
    }
    source_type = base->a->a->type;
    source = expression(g, base->a->a);
    if (source_type->kind == TY_ARRAY) {
        data = temp(g);
        define_value(g, tc_format(c, "%s *", base->decl_type->cname), data,
                     tc_format(c, "(%s).data", source));
        length = tc_format(c, "%zu", source_type->count);
    } else {
        const char *saved = temp(g);
        define_value(g, source_type->cname, saved, source);
        data = tc_format(c, "%s.data", saved);
        length = tc_format(c, "%s.length", saved);
    }
    for (i = count - 1; i >= 0; i--)
        stages[i]->cname = value(g, stages[i]->args);
    if (!strcmp(n->text, "stream_reduce")) {
        result = temp(g);
        define_value(g, n->type->cname, result, value(g, n->args));
        callback = n->args->next;
    } else if (n->type->kind != TY_VOID) {
        result = temp(g);
        define_value(g, n->type->cname, result, !strcmp(n->text, "stream_all") ? "1" : "{0}");
    }
    if (!strcmp(n->text, "stream_forEach") || !strcmp(n->text, "stream_any") ||
        !strcmp(n->text, "stream_all"))
        callback = n->args;
    if (callback)
        terminal = value(g, callback);
    if (!strcmp(n->text, "stream_collect"))
        terminal = value(g, n->args);
    index = temp(g);
    define_value(g, "uint64_t", index, "0");
    line(g, "/* Fused stream: one traversal, no intermediate collections. */");
    line(g, "for (; %s < %s; ++%s) {", index, length, index);
    g->indent++;
    item = temp(g);
    define_value(g, base->decl_type->cname, item, tc_format(c, "%s[%s]", data, index));
    for (i = count - 1; i >= 0; i--) {
        Node *stage = stages[i];
        const char *call = stream_invoke(g, stage->args, stage->cname, item, NULL);
        if (!strcmp(stage->a->name, "filter"))
            line(g, "if (!(%s)) continue;", call);
        else {
            item = temp(g);
            define_value(g, stage->decl_type->cname, item, call);
        }
    }
    if (!strcmp(n->text, "stream_count"))
        line(g, "++%s;", result);
    else if (!strcmp(n->text, "stream_first"))
        line(g, "%s.v0 = %s; %s.v1 = true; break;", result, item, result);
    else if (!strcmp(n->text, "stream_collect"))
        line(g, "%s_push(%s, %s);", n->args->type->base->cname, terminal, item);
    else {
        const char *call =
            stream_invoke(g, callback, terminal, !strcmp(n->text, "stream_reduce") ? result : item,
                          !strcmp(n->text, "stream_reduce") ? item : NULL);
        if (!strcmp(n->text, "stream_forEach"))
            line(g, "%s;", call);
        else if (!strcmp(n->text, "stream_reduce"))
            line(g, "%s = %s;", result, call);
        else if (!strcmp(n->text, "stream_any"))
            line(g, "if (%s) { %s = true; break; }", call, result);
        else
            line(g, "if (!(%s)) { %s = false; break; }", call, result);
    }
    g->indent--;
    line(g, "}");
    return result ? result : "(void)0";
}
static const char *expression(CG *g, Node *n) {
    Context *c = g->c;
    const char *a, *b, *v;
    Buffer out = {0};
    Node *p;
    ArgValue *args;
    switch (n->kind) {
    case N_INT:
        return tc_format(c, "%s%s", n->text,
                         n->type->kind == TY_U64   ? "ULL"
                         : n->type->kind == TY_I64 ? "LL"
                                                   : "");
    case N_FLOAT:
    case N_BOOL:
        return n->text;
    case N_CHAR:
        return literal(c, n->text);
    case N_STRING:
        return tc_format(c, "TC_STRING(%s)", literal(c, n->text));
    case N_NULL:
        return "NULL";
    case N_ID:
        if (n->resolved && n->resolved->kind == N_PROPERTY && n->resolved->a)
            return tc_format(c, "%s_get_%s(tc_self)", n->resolved->owner->type->cname,
                             n->resolved->name);
        if (n->sym && (!n->cname || strncmp(n->cname, "tc_env->", 8)))
            return n->sym->cname;
        return n->cname ? n->cname : n->name;
    case N_BINARY:
        if (!strcmp(n->text, "&&") || !strcmp(n->text, "||")) {
            a = value(g, n->a);
            v = temp(g);
            define_value(g, "bool", v, tc_format(c, "!!(%s)", a));
            line(g, "if (%s%s) {", !strcmp(n->text, "||") ? "!" : "", v);
            g->indent++;
            b = value(g, n->b);
            line(g, "%s = !!(%s);", v, b);
            g->indent--;
            line(g, "}");
            return v;
        }
        if (!strcmp(n->text, "=") ||
            (strlen(n->text) == 2 && n->text[1] == '=' && strchr("+-*/%&|^", n->text[0]))) {
            a = expression(g, n->a);
            v = temp(g);
            define_value(g, tc_format(c, "%s *", n->a->type->cname), v, tc_format(c, "&(%s)", a));
            b = value(g, n->b);
            return tc_format(c, "(*%s %s %s)", v, n->text, convert(g, n->a->type, n->b->type, b));
        }
        a = value(g, n->a);
        b = value(g, n->b);
        if (n->a->type->kind == TY_STRING)
            return tc_format(c, "%stc_string_equal(%s, %s)", !strcmp(n->text, "!=") ? "!" : "", a,
                             b);
        return tc_format(c, "(%s %s %s)", a, n->text, b);
    case N_UNARY:
        if (!strcmp(n->text, "&") || !strcmp(n->text, "++") || !strcmp(n->text, "--"))
            a = expression(g, n->a);
        else
            a = value(g, n->a);
        return tc_format(c, n->flags & NF_POST ? "((%s)%s)" : "(%s(%s))",
                         n->flags & NF_POST ? a : n->text, n->flags & NF_POST ? n->text : a);
    case N_CALL:
        if (n->text) {
            if (!strncmp(n->text, "stream_", 7))
                return stream_expression(g, n);
            if (!strncmp(n->text, "task_", 5)) {
                a = value(g, n->a->a);
                if (!strcmp(n->text, "task_ready"))
                    return tc_format(c, "tc_future_ready(%s.handle)", a);
                if (!strcmp(n->text, "task_destroy"))
                    return tc_format(c, "tc_future_release(%s.handle)", a);
                if (!strcmp(n->text, "task_retain"))
                    return tc_format(c, "tc_future_retain(%s.handle)", a);
                if (!strcmp(n->text, "task_result")) {
                    if (n->type->kind != TY_TUPLE)
                        return tc_format(c, "tc_future_get(%s.handle, NULL)", a);
                    v = temp(g);
                    define_value(g, n->type->cname, v, "{0}");
                    line(g, "%s.v1 = tc_future_get(%s.handle, &(%s.v0));", v, a, v);
                    return v;
                }
                v = n->type->kind == TY_VOID ? NULL : temp(g);
                if (v)
                    define_value(g, n->type->cname, v, "{0}");
                line(g, "tc_assert(tc_future_get(%s.handle, %s) == 0, %s, %d);", a,
                     v ? tc_format(c, "&(%s)", v) : "NULL", quote(c, n->loc.file), n->loc.line);
                return v ? v : "(void)0";
            }
            if (!strcmp(n->text, "owned"))
                return expression(g, n->args);
            if (!strcmp(n->text, "closure_destroy")) {
                a = expression(g, n->a->a);
                return tc_format(c, "tc_closure_destroy((%s).env, (%s).owned)", a, a);
            }
            a = value(g, n->args);
            if (!strcmp(n->text, "hash")) {
                if (n->args->type->kind == TY_STRING)
                    return tc_format(c, "tc_hash_string(%s)", a);
                if (n->args->type->kind == TY_PTR)
                    return tc_format(c, "tc_hash_u64((uint64_t)(uintptr_t)(%s))", a);
                return tc_format(c, "tc_hash_u64((uint64_t)(%s))", a);
            }
            if (!strcmp(n->text, "len")) {
                if (n->args->type->kind == TY_ARRAY)
                    return tc_format(c, "%zuULL", n->args->type->count);
                return tc_format(c, "(%s).length", a);
            }
            if (!strcmp(n->text, "assert"))
                return tc_format(c, "tc_assert(!!(%s), %s, %d)", a, quote(c, n->loc.file),
                                 n->loc.line);
            b = n->args->type->kind == TY_STRING                                      ? "string"
                : n->args->type->kind == TY_PTR || n->args->type->kind == TY_NULL     ? "pointer"
                : n->args->type->kind == TY_FLOAT || n->args->type->kind == TY_DOUBLE ? "float"
                : n->args->type->kind == TY_U64 || n->args->type->kind == TY_U32 ||
                        n->args->type->kind == TY_U16 || n->args->type->kind == TY_U8
                    ? "unsigned"
                    : "integer";
            return tc_format(c, "tc_print_%s(%s, %d)", b, a, !strcmp(n->text, "println"));
        }
        if (n->a->type && n->a->type->kind == TY_CLOSURE) {
            a = value(g, n->a);
            args = arg_values(g, n->args);
            buf_printf(&out, "%s.invoke(%s.env", a, a);
            append_args(g, &out, args, n->resolved ? n->resolved->params : NULL, 0);
            buf_add(&out, ")");
            break;
        }
        if (n->a->kind == N_MEMBER && n->resolved && n->resolved->owner &&
            n->resolved->owner->kind == N_INTERFACE) {
            a = value(g, n->a->a);
            args = arg_values(g, n->args);
            buf_printf(&out, "%s.vtable->%s(%s.object", a, n->resolved->name, a);
            append_args(g, &out, args, n->resolved->params, 0);
            buf_add(&out, ")");
            break;
        }
        if (n->a->kind == N_MEMBER && n->a->resolved && !(n->a->resolved->flags & NF_STATIC))
            a = object_pointer(g, n->a->a);
        else if (n->a->kind == N_ID && n->a->resolved && n->a->resolved->kind == N_FUNCTION &&
                 !(n->a->resolved->flags & NF_STATIC))
            a = "tc_self";
        else
            a = NULL;
        b = n->resolved && ((n->a->sym && n->a->sym->node->kind == N_FUNCTION) || n->a->resolved)
                ? n->resolved->cname
                : value(g, n->a);
        args = arg_values(g, n->args);
        buf_printf(&out, "%s(", b);
        if (a)
            buf_add(&out, a);
        append_args(g, &out, args, n->resolved ? n->resolved->params : NULL, a == NULL);
        buf_add(&out, ")");
        break;
    case N_MEMBER:
        if (n->resolved && n->resolved->owner && n->resolved->owner->kind == N_ENUM)
            return n->resolved->cname;
        if (n->resolved && n->resolved->kind == N_PROPERTY && n->resolved->a)
            return tc_format(c, "%s_get_%s(%s)", n->resolved->owner->type->cname, n->name,
                             object_pointer(g, n->a));
        a = expression(g, n->a);
        if (n->a->type->kind == TY_ARRAY && !strcmp(n->name, "length"))
            return tc_format(c, "%zuULL", n->a->type->count);
        return tc_format(c, "(%s)%s%s", a, n->a->type->kind == TY_PTR ? "->" : ".", n->name);
    case N_INDEX: {
        Type *t = n->a->type;
        const char *ptr, *len;
        a = expression(g, n->a);
        if (t->kind == TY_ARRAY) {
            ptr = temp(g);
            define_value(g, tc_format(c, "%s *", t->base->cname), ptr,
                         tc_format(c, "(%s).data", a));
            len = tc_format(c, "%zu", t->count);
        } else {
            v = temp(g);
            define_value(g, t->cname, v, a);
            ptr = t->kind == TY_PTR ? v : tc_format(c, "(%s).data", v);
            len = t->kind == TY_PTR ? NULL : tc_format(c, "(%s).length", v);
        }
        b = value(g, n->b);
        if (len && c->bounds)
            b = tc_format(c, "tc_index((int64_t)(%s), %s, %s, %d)", b, len, quote(c, n->loc.file),
                          n->loc.line);
        return tc_format(c, "(%s)[%s]", ptr, b);
    }
    case N_SLICE: {
        Type *t = n->a->type;
        const char *ptr, *len, *lo, *hi;
        a = expression(g, n->a);
        v = temp(g);
        if (t->kind == TY_ARRAY) {
            define_value(g, tc_format(c, "%s *", t->base->cname), v, tc_format(c, "(%s).data", a));
            ptr = v;
            len = tc_format(c, "%zu", t->count);
        } else {
            define_value(g, t->cname, v, a);
            ptr = tc_format(c, "%s.data", v);
            len = tc_format(c, "%s.length", v);
        }
        lo = n->b ? value(g, n->b) : "0";
        hi = n->c ? value(g, n->c) : len;
        if (c->bounds)
            line(g, "tc_slice_check((int64_t)(%s), (int64_t)(%s), %s, %s, %d);", lo, hi, len,
                 quote(c, n->loc.file), n->loc.line);
        return tc_format(c, "(%s){%s + (%s), (size_t)((%s) - (%s))}", n->type->cname, ptr, lo, hi,
                         lo);
    }
    case N_INIT:
    case N_TUPLE: {
        ArgValue *av;
        int first = 1;
        args = arg_values(g, n->args);
        buf_printf(&out, "(%s){%s", n->type->cname, n->kind == N_INIT ? "{" : "");
        if (!args)
            buf_add(&out, "0");
        for (av = args; av; av = av->next) {
            if (!first)
                buf_add(&out, ", ");
            first = 0;
            buf_add(&out, av->value);
        }
        buf_add(&out, n->kind == N_INIT ? "}}" : "}");
        break;
    }
    case N_NEW: {
        Type *t = n->decl_type;
        const char *obj = temp(g), *ptr;
        ArgValue *av;
        args = arg_values(g, n->args);
        if (n->flags & NF_CONSTRUCTOR) {
            define_value(g, t->cname, obj, "{0}");
            ptr = tc_format(c, "&%s", obj);
        } else {
            define_value(g, tc_format(c, "%s *", t->cname), obj,
                         tc_format(c, "(%s *)tc_alloc_checked(sizeof(%s))", t->cname, t->cname));
            ptr = obj;
        }
        if (n->resolved) {
            Buffer call = {0};
            buf_printf(&call, "%s(%s", n->resolved->cname, ptr);
            append_args(g, &call, args, n->resolved->params, 0);
            buf_add(&call, ");");
            line(g, "%s", call.data);
            free(call.data);
        } else if (t->kind == TY_NAMED) {
            for (av = args; av; av = av->next)
                line(g, "(%s)->%s = %s;", ptr, av->arg->parameter->name, av->value);
        } else if (args)
            line(g, "*(%s) = %s;", ptr, args->value);
        return obj;
    }
    case N_CAST:
        a = value(g, n->a);
        return tc_format(c, "((%s)(%s))", n->type->cname, a);
    case N_SIZEOF:
        return tc_format(c, "sizeof(%s)", n->decl_type->cname);
    case N_LAMBDA: {
        Node *capture;
        const char *env = temp(g);
        if (n->type->kind == TY_FUNC)
            return n->cname;
        if (n->flags & NF_OWNED) {
            define_value(g, tc_format(c, "struct tc_env_%d *", n->id), env,
                         tc_format(c,
                                   "(struct tc_env_%d *)tc_alloc_checked(sizeof(struct tc_env_%d))",
                                   n->id, n->id));
            a = env;
        } else {
            define_value(g, tc_format(c, "struct tc_env_%d", n->id), env, "{0}");
            a = tc_format(c, "&%s", env);
        }
        for (capture = n->args; capture; capture = capture->next) {
            const char *captured = capture->sym->cname;
            if (g->fn && g->fn->kind == N_LAMBDA && capture->sym->function != g->fn)
                captured = tc_format(c, "tc_env->capture_%d", capture->sym->node->id);
            line(g, "(%s)->capture_%d = %s;", a, capture->sym->node->id, captured);
        }
        return tc_format(c, "(%s){%s, %s, %d}", n->type->cname, a, n->cname,
                         !!(n->flags & NF_OWNED));
    }
    case N_AWAIT: {
        int state = ++g->states;
        const char *result;
        if (!g->asynchronous)
            tc_error(c, n->loc, "await outside an asynchronous lowering context");
        a = value(g, n->a);
        line(g, "tc_async->waiting = %s.handle;", a);
        if (n->a->kind == N_ID || n->a->kind == N_MEMBER)
            line(g, "tc_future_retain(tc_async->waiting);");
        line(g, "tc_async->state = %d;", state);
        line(g, "if (!tc_future_ready(tc_async->waiting)) {");
        g->indent++;
        line(g,
             "tc_assert(tc_future_then(tc_async->waiting, tc_scheduler_pool(), %s_step, tc_async) "
             "== 0, %s, %d);",
             g->fn->cname, quote(c, n->loc.file), n->loc.line);
        line(g, "return;");
        g->indent--;
        line(g, "}");
        line(g, "tc_state_%d:;", state);
        result = n->type->kind == TY_VOID ? NULL : temp(g);
        if (result)
            define_value(g, n->type->cname, result, "{0}");
        line(g, "tc_async->error = tc_future_get(tc_async->waiting, %s);",
             result ? tc_format(c, "&(%s)", result) : "NULL");
        line(g, "tc_future_release(tc_async->waiting); tc_async->waiting = NULL;");
        line(g, "if (tc_async->error) {");
        g->indent++;
        cleanup_emit(g, NULL);
        async_finish(g, NULL, "tc_async->error");
        g->indent--;
        line(g, "}");
        return result ? result : "(void)0";
    }
    case N_SPAWN:
        if (n->text)
            return expression(g, n->a);
        a = value(g, n->a->a);
        args = arg_values(g, n->a->args);
        buf_printf(&out, "tc_spawn_%d(%s", n->id, a);
        append_args(g, &out, args, NULL, 0);
        buf_add(&out, ")");
        break;
    default:
        tc_error(c, n->loc, "unimplemented expression lowering");
    }
    a = tc_str(c, out.data ? out.data : "");
    free(out.data);
    (void)p;
    return a;
}
static Node *destructor(Type *t) {
    Node *m;
    if (t->kind != TY_NAMED || !t->decl)
        return NULL;
    for (m = t->decl->body; m; m = m->next)
        if (m->flags & NF_DESTRUCTOR)
            return m;
    return NULL;
}
static int returned_local(Node *expression, Node *local) {
    Node *item;
    if (!expression)
        return 0;
    if (expression->kind == N_ID && expression->sym && expression->sym->node == local)
        return 1;
    if (expression->kind == N_TUPLE)
        for (item = expression->args; item; item = item->next)
            if (returned_local(item, local))
                return 1;
    return 0;
}
static void cleanup_emit(CG *g, Cleanup *stop) {
    Cleanup *p;
    for (p = g->cleanup; p != stop; p = p->next) {
        Node *n = p->node;
        if (n->kind == N_DEFER) {
            const char *s = expression(g, n->a);
            line(g, "%s;", s);
        } else {
            Node *d = destructor(n->type);
            if (d && !returned_local(g->returning, n))
                line(g, "%s(&%s);", d->cname, n->cname);
        }
    }
}
static void cleanup_add(CG *g, Node *n) {
    Cleanup *p = (Cleanup *)tc_alloc(g->c, sizeof(Cleanup));
    p->node = n;
    p->next = g->cleanup;
    g->cleanup = p;
}
static void scoped(CG *g, Node *n) {
    Cleanup *stop = g->cleanup;
    Node *p;
    line(g, "{");
    g->indent++;
    if (n && n->kind == N_BLOCK) {
        for (p = n->body; p; p = p->next)
            statement(g, p);
    } else if (n)
        statement(g, n);
    cleanup_emit(g, stop);
    g->cleanup = stop;
    g->indent--;
    line(g, "}");
}
static void statement(CG *g, Node *n) {
    const char *a, *b;
    Context *c = g->c;
    line(g, "#line %d %s", n->loc.line, quote(c, n->loc.file));
    switch (n->kind) {
    case N_BLOCK:
        scoped(g, n);
        break;
    case N_VAR:
        if (n->params) {
            Node *p;
            int i = 1;
            a = value(g, n->a);
            define_value(g, n->type->cname, n->cname, tc_format(c, "%s.v0", a));
            for (p = n->params; p; p = p->next)
                define_value(g, p->type->cname, p->cname, tc_format(c, "%s.v%d", a, i++));
            if (destructor(n->type))
                cleanup_add(g, n);
            for (p = n->params; p; p = p->next)
                if (destructor(p->type))
                    cleanup_add(g, p);
        } else {
            a = n->a ? expression(g, n->a) : "{0}";
            define_value(g, n->type->cname, n->cname,
                         n->a ? convert(g, n->type, n->a->type, a) : a);
            if (destructor(n->type))
                cleanup_add(g, n);
        }
        break;
    case N_EXPR:
        if (n->a) {
            a = expression(g, n->a);
            line(g, "%s;", a);
        }
        break;
    case N_RETURN:
        a = n->a ? value(g, n->a) : NULL;
        /* Returning a local transfers its value and suppresses that local's automatic drop. */
        g->returning = n->a;
        cleanup_emit(g, NULL);
        g->returning = NULL;
        if (g->asynchronous) {
            async_finish(g, a, "0");
            break;
        }
        if (a)
            line(g, "return %s;", convert(g, g->fn->decl_type, n->a->type, a));
        else
            line(g, "return;");
        break;
    case N_IF:
        a = value(g, n->a);
        line(g, "if (%s)", a);
        scoped(g, n->body);
        if (n->b) {
            line(g, "else");
            scoped(g, n->b);
        }
        break;
    case N_SWITCH: {
        Node *item, *v, *fallback = NULL, *last = NULL;
        int first = 1, guard = n->a->type->kind == TY_ENUM;
        const char *subject = value(g, n->a);
        Buffer all = {0};
        for (item = n->body; item; item = item->next) {
            Buffer test = {0};
            if (item->text) {
                fallback = item;
                guard = 0;
                continue;
            }
            last = item;
            for (v = item->args; v; v = v->next) {
                const char *label = expression(g, v);
                if (test.len)
                    buf_add(&test, " || ");
                if (n->a->type->kind == TY_STRING)
                    buf_printf(&test, "tc_string_equal(%s, %s)", subject, label);
                else
                    buf_printf(&test, "(%s) == (%s)", subject, label);
            }
            item->label = tc_str(c, test.data);
            buf_printf(&all, "%s%s", all.len ? " || " : "", test.data);
            free(test.data);
        }
        /* An exhaustive enum switch rejects out-of-range values up front, so the
           final case can be a plain else and C sees every path covered. */
        if (guard && last)
            line(g, "if (!(%s)) tc_panic(\"switch value matches no enum case\", %s, %d);",
                 all.data, quote(c, n->loc.file), n->loc.line);
        for (item = n->body; item; item = item->next) {
            if (item->text)
                continue;
            if (guard && item == last && !first)
                line(g, "else");
            else if (!(guard && item == last))
                line(g, "%sif (%s)", first ? "" : "else ", item->label);
            first = 0;
            scoped(g, item->body);
        }
        if (fallback) {
            if (!first)
                line(g, "else");
            scoped(g, fallback->body);
        }
        free(all.data);
        break;
    }
    case N_WHILE:
    case N_FOR:
    case N_RANGE: {
        Loop loop;
        Cleanup *stop = g->cleanup;
        const char *range = NULL, *len = NULL, *index = NULL;
        line(g, "{");
        g->indent++;
        if (n->kind == N_FOR && n->a)
            statement(g, n->a);
        if (n->kind == N_RANGE) {
            Type *t = n->a->type;
            range = expression(g, n->a);
            if (t->kind == TY_ARRAY) {
                a = temp(g);
                define_value(g, tc_format(c, "%s *", t->base->cname), a,
                             tc_format(c, "(%s).data", range));
                range = a;
                len = tc_format(c, "%zu", t->count);
            } else {
                a = temp(g);
                define_value(g, t->cname, a, range);
                range = tc_format(c, "%s.data", a);
                len = tc_format(c, "%s.length", a);
            }
            index = n->b ? n->b->cname : temp(g);
            define_value(g, "uint64_t", index, "0");
        }
        loop.id = ++g->serial;
        loop.parent = g->loop;
        loop.stop = g->cleanup;
        g->loop = &loop;
        line(g, "for (;;) {");
        g->indent++;
        if (n->kind == N_RANGE) {
            line(g, "if (%s >= %s) break;", index, len);
            define_value(g, n->c->type->cname, n->c->cname, tc_format(c, "%s[%s]", range, index));
        } else if ((n->kind == N_FOR && n->b) || n->kind == N_WHILE) {
            a = value(g, n->kind == N_WHILE ? n->a : n->b);
            line(g, "if (!(%s)) break;", a);
        }
        scoped(g, n->body);
        line(g, "tc_continue_%d:;", loop.id);
        if (n->kind == N_FOR && n->c) {
            a = expression(g, n->c);
            line(g, "%s;", a);
        }
        if (n->kind == N_RANGE)
            line(g, "++%s;", index);
        g->indent--;
        line(g, "}");
        line(g, "tc_break_%d:;", loop.id);
        g->loop = loop.parent;
        cleanup_emit(g, stop);
        g->cleanup = stop;
        g->indent--;
        line(g, "}");
        break;
    }
    case N_BREAK:
    case N_CONTINUE:
        cleanup_emit(g, g->loop->stop);
        line(g, "goto tc_%s_%d;", n->kind == N_BREAK ? "break" : "continue", g->loop->id);
        break;
    case N_DEFER:
        cleanup_add(g, n);
        break;
    case N_DELETE: {
        Node *d = destructor(n->a->type->base);
        a = value(g, n->a);
        if (d)
            line(g, "if (%s) %s(%s);", a, d->cname, a);
        line(g, "free(%s);", a);
        break;
    }
    default:
        tc_error(c, n->loc, "unimplemented statement lowering");
    }
    (void)b;
}
static int emittable(Type *t) {
    TypeLink *l;
    if (!t)
        return 1;
    if (t->kind == TY_NAMED && ((!t->decl && strcmp(t->name, "Task")) ||
                                (t->decl && t->decl->kind == N_CLASS && t->decl->params)))
        return 0;
    if (t->base && !emittable(t->base))
        return 0;
    for (l = t->items; l; l = l->next)
        if (!emittable(l->type))
            return 0;
    return 1;
}
static void emit_type(CG *g, Type *t) {
    TypeLink *l;
    Node *m;
    int i = 0;
    if (t && (t->alias || t->qualified))
        return;
    if (!t || !emittable(t) || t->is_const == 2 || t->kind <= TY_NULL || t->kind == TY_AUTO ||
        (t->decl && t->decl->kind == N_CLASS && t->decl->params) ||
        (t->decl && t->decl->kind == N_FUNCTION && t->kind == TY_NAMED))
        return;
    if (t->is_const == 1)
        tc_error(g->c, t->decl ? t->decl->loc : g->c->program->loc,
                 "recursive value layout for '%s'; use a pointer", t->name);
    t->is_const = 1;
    if (t->kind == TY_PTR) {
        t->is_const = 2;
        return;
    }
    if (t->base && t->kind != TY_SLICE)
        emit_type(g, t->base);
    if (t->kind == TY_TUPLE || t->kind == TY_FUNC || t->kind == TY_CLOSURE)
        for (l = t->items; l; l = l->next)
            emit_type(g, l->type);
    if (t->kind == TY_NAMED && !strcmp(t->name, "Task") && t->items)
        line(g, "typedef struct { void *handle; } %s;", t->cname);
    else if (t->kind == TY_ENUM) {
        line(g, "typedef int32_t %s;", t->cname);
        line(g, "enum {");
        g->indent++;
        for (m = t->decl->body; m; m = m->next)
            line(g, "%s = %s%s", m->cname, m->text, m->next ? "," : "");
        g->indent--;
        line(g, "};");
    } else if (t->kind == TY_ARRAY)
        line(g, "typedef struct { %s data[%zu]; } %s;", t->base->cname, t->count, t->cname);
    else if (t->kind == TY_SLICE)
        line(g, "typedef struct { %s *data; size_t length; } %s;", t->base->cname, t->cname);
    else if (t->kind == TY_TUPLE) {
        line(g, "typedef struct {");
        g->indent++;
        for (l = t->items; l; l = l->next)
            line(g, "%s v%d;", l->type->cname, i++);
        g->indent--;
        line(g, "} %s;", t->cname);
    } else if (t->kind == TY_CLOSURE) {
        Buffer b = {0};
        buf_printf(&b, "typedef struct { void *env; %s (*invoke)(void *", t->base->cname);
        for (l = t->items; l; l = l->next)
            buf_printf(&b, ", %s", l->type->cname);
        buf_printf(&b, "); bool owned; } %s;", t->cname);
        line(g, "%s", b.data);
        free(b.data);
    } else if (t->kind == TY_FUNC) {
        Buffer b = {0};
        buf_printf(&b, "typedef %s (%s*%s)(", t->base->cname, t->callconv ? "TC_STDCALL " : "",
                   t->cname);
        if (!t->items)
            buf_add(&b, "void");
        for (l = t->items; l; l = l->next) {
            if (i++)
                buf_add(&b, ", ");
            buf_add(&b, l->type->cname);
        }
        buf_add(&b, ");");
        line(g, "%s", b.data);
        free(b.data);
    } else if (t->kind == TY_NAMED && t->decl && t->decl->kind == N_INTERFACE) {
        for (m = t->decl->body; m; m = m->next) {
            Node *p;
            emit_type(g, m->type->base);
            for (p = m->params; p; p = p->next)
                emit_type(g, p->decl_type);
        }
        line(g, "typedef struct %s_vtable {", t->cname);
        g->indent++;
        for (m = t->decl->body; m; m = m->next) {
            Buffer b = {0};
            Node *p;
            buf_printf(&b, "%s (*%s)(void *object", m->type->base->cname, m->name);
            for (p = m->params; p; p = p->next)
                buf_printf(&b, ", %s", p->decl_type->cname);
            buf_add(&b, ");");
            line(g, "%s", b.data);
            free(b.data);
        }
        g->indent--;
        line(g, "} %s_vtable;", t->cname);
        line(g, "struct %s { void *object; const %s_vtable *vtable; };", t->cname, t->cname);
    } else if (t->kind == TY_NAMED && t->decl) {
        for (m = t->decl->body; m; m = m->next)
            if (m->kind == N_VAR || (m->kind == N_PROPERTY && !m->a))
                emit_type(g, m->decl_type);
        line(g, "struct %s {", t->cname);
        g->indent++;
        for (m = t->decl->body; m; m = m->next)
            if (m->kind == N_VAR || (m->kind == N_PROPERTY && !m->a)) {
                line(g, "%s %s;", m->decl_type->cname, m->name);
                i++;
            }
        if (!i)
            line(g, "unsigned char tc_empty;");
        g->indent--;
        line(g, "};");
    }
    t->is_const = 2;
}
static void signature(CG *g, Node *n, int prototype) {
    Node *p;
    Buffer b = {0};
    int first = 1;
    buf_printf(&b, "%s %s%s(", n->task_type ? n->task_type->cname : n->decl_type->cname,
               n->flags & NF_STDCALL ? "TC_STDCALL " : "", n->cname);
    if (n->owner && !(n->flags & NF_STATIC)) {
        buf_printf(&b, "%s *tc_self", n->owner->type->cname);
        first = 0;
    }
    for (p = n->params; p; p = p->next) {
        if (!first)
            buf_add(&b, ", ");
        first = 0;
        if (!strcmp(p->name, "..."))
            buf_add(&b, "...");
        else
            buf_printf(&b, "%s %s", p->decl_type->cname, p->cname ? p->cname : p->name);
    }
    if (first)
        buf_add(&b, "void");
    buf_add(&b, prototype ? ");" : ")");
    line(g, "%s", b.data);
    free(b.data);
}
static void function(CG *g, Node *n) {
    Node *p;
    if (!n->body)
        return;
    g->fn = n;
    g->cleanup = NULL;
    signature(g, n, 0);
    line(g, "{");
    g->indent++;
    if (n->body->kind == N_BLOCK)
        for (p = n->body->body; p; p = p->next)
            statement(g, p);
    else
        statement(g, n->body);
    cleanup_emit(g, NULL);
    if (!strcmp(n->name, "main"))
        line(g, "return 0;");
    g->indent--;
    line(g, "}");
    line(g, "");
    g->cleanup = NULL;
}
static void adapters(CG *g) {
    Node *a, *m, *p;
    for (a = g->c->adapters; a; a = a->next) {
        Type *iface = a->type, *concrete = a->decl_type->base;
        for (m = iface->decl->body; m; m = m->next) {
            Buffer sig = {0}, call = {0};
            int index = 0;
            buf_printf(&sig, "static %s tc_adapter_%d_%d_%s(void *object", m->type->base->cname,
                       iface->id, concrete->id, m->name);
            buf_printf(&call, "%s%s_%s((%s *)object",
                       m->type->base->kind == TY_VOID ? "" : "return ", concrete->cname, m->name,
                       concrete->cname);
            for (p = m->params; p; p = p->next) {
                buf_printf(&sig, ", %s p%d", p->decl_type->cname, index);
                buf_printf(&call, ", p%d", index++);
            }
            buf_add(&sig, ") {");
            buf_add(&call, ");");
            line(g, "%s", sig.data);
            g->indent++;
            line(g, "%s", call.data);
            g->indent--;
            line(g, "}");
            free(sig.data);
            free(call.data);
        }
        line(g, "static const %s_vtable tc_vtable_%d_%d = {", iface->cname, iface->id,
             concrete->id);
        g->indent++;
        for (m = iface->decl->body; m; m = m->next)
            line(g, "tc_adapter_%d_%d_%s%s", iface->id, concrete->id, m->name, m->next ? "," : "");
        g->indent--;
        line(g, "};");
    }
}
static void lambdas(CG *g) {
    Node *entry, *reversed = NULL;
    for (entry = g->c->lambdas; entry;) {
        Node *next = entry->next;
        entry->next = reversed;
        reversed = entry;
        entry = next;
    }
    g->c->lambdas = reversed;
    for (entry = g->c->lambdas; entry; entry = entry->next) {
        Node *n = entry->a, *p;
        Buffer sig = {0};
        int first = 1;
        if (n->type->kind == TY_CLOSURE) {
            line(g, "struct tc_env_%d {", n->id);
            g->indent++;
            if (!n->args)
                line(g, "unsigned char empty;");
            for (p = n->args; p; p = p->next) {
                const char *type = p->type->cname;
                if (p->type->qualified)
                    type += 6;
                line(g, "%s capture_%d;", type, p->sym->node->id);
            }
            g->indent--;
            line(g, "};");
        }
        buf_printf(&sig, "static %s %s(", n->decl_type->cname, n->cname);
        if (n->type->kind == TY_CLOSURE) {
            buf_add(&sig, "void *tc_environment");
            first = 0;
        }
        for (p = n->params; p; p = p->next) {
            if (!first)
                buf_add(&sig, ", ");
            first = 0;
            buf_printf(&sig, "%s %s", p->type->cname, p->cname);
        }
        if (first)
            buf_add(&sig, "void");
        buf_add(&sig, ") {");
        line(g, "%s", sig.data);
        free(sig.data);
        g->indent++;
        g->fn = n;
        if (n->type->kind == TY_CLOSURE)
            line(g, "struct tc_env_%d *tc_env = (struct tc_env_%d *)tc_environment;", n->id, n->id);
        if (n->a) {
            const char *v = expression(g, n->a);
            line(g, n->decl_type->kind == TY_VOID ? "%s;" : "return %s;", v);
        } else
            scoped(g, n->body);
        g->indent--;
        line(g, "}");
    }
}
static void rebind_async(Context *c, Node *n, Node *fn) {
    for (; n; n = n->next) {
        if (n->sym && n->sym->function == fn) {
            if (strncmp(n->sym->cname, "tc_async->", 10))
                n->sym->cname = tc_format(c, "tc_async->%s", n->sym->cname);
            if (!n->cname || strncmp(n->cname, "tc_env->", 8))
                n->cname = n->sym->cname;
        }
        rebind_async(c, n->a, fn);
        rebind_async(c, n->b, fn);
        rebind_async(c, n->c, fn);
        rebind_async(c, n->body, fn);
        rebind_async(c, n->params, fn);
        if (n->kind != N_LAMBDA)
            rebind_async(c, n->args, fn);
    }
}
static void async_function(CG *g, Node *n) {
    CG step;
    Node *p;
    int i;
    Buffer sig = {0};
    const char *frame = tc_format(g->c, "%s_frame", n->cname);
    memset(&step, 0, sizeof(step));
    step.c = g->c;
    step.fn = n;
    step.asynchronous = 1;
    step.indent = 1;
    for (p = n->params; p; p = p->next)
        p->text = p->cname;
    rebind_async(g->c, n->params, n);
    rebind_async(g->c, n->body, n);
    for (p = n->params; p; p = p->next)
        frame_field(&step, p->type->cname, p->cname);
    line(&step, "tc_state_0:;");
    if (n->body->kind == N_BLOCK)
        for (p = n->body->body; p; p = p->next)
            statement(&step, p);
    else
        statement(&step, n->body);
    cleanup_emit(&step, NULL);
    async_finish(&step, NULL, "0");
    line(g, "struct %s {", frame);
    line(g, "    int state, error; void *future, *waiting;");
    if (n->owner && !(n->flags & NF_STATIC))
        line(g, "    %s *self;", n->owner->type->cname);
    if (step.frame.data)
        buf_add(&g->out, step.frame.data);
    line(g, "};");
    line(g, "static void %s_step(void *raw) {", n->cname);
    g->indent++;
    line(g, "struct %s *tc_async = (struct %s *)raw;", frame, frame);
    if (n->owner && !(n->flags & NF_STATIC))
        line(g, "%s *tc_self = tc_async->self;", n->owner->type->cname);
    line(g, "switch (tc_async->state) {");
    g->indent++;
    for (i = 0; i <= step.states; i++)
        line(g, "case %d: goto tc_state_%d;", i, i);
    line(g, "default: abort();");
    g->indent--;
    line(g, "}");
    buf_add(&g->out, step.out.data);
    g->indent--;
    line(g, "}");
    buf_printf(&sig, "%s %s(", n->task_type->cname, n->cname);
    i = 0;
    if (n->owner && !(n->flags & NF_STATIC)) {
        buf_printf(&sig, "%s *tc_self", n->owner->type->cname);
        i++;
    }
    for (p = n->params; p; p = p->next) {
        if (i++)
            buf_add(&sig, ", ");
        buf_printf(&sig, "%s %s", p->type->cname, p->text);
    }
    if (!i)
        buf_add(&sig, "void");
    buf_add(&sig, ") {");
    line(g, "%s", sig.data);
    g->indent++;
    line(g, "struct %s *tc_async = (struct %s *)tc_alloc_checked(sizeof(struct %s));", frame, frame,
         frame);
    line(g, "tc_async->future = tc_future_create(%s);",
         n->decl_type->kind == TY_VOID ? "0" : tc_format(g->c, "sizeof(%s)", n->decl_type->cname));
    line(g, "tc_assert(tc_async->future != NULL, \"<async>\", 0);");
    line(g, "tc_future_retain(tc_async->future);");
    for (p = n->params; p; p = p->next)
        line(g, "%s = %s;", p->cname, p->text);
    if (n->owner && !(n->flags & NF_STATIC))
        line(g, "tc_async->self = tc_self;");
    line(g, "%s result = {tc_async->future};", n->task_type->cname);
    line(g, "tc_task_begin();");
    line(g,
         "tc_assert(tc_pool_submit(tc_scheduler_pool(), %s_step, tc_async) == 0, \"<async>\", 0);",
         n->cname);
    line(g, "return result;");
    g->indent--;
    line(g, "}");
    free(sig.data);
    free(step.out.data);
    free(step.frame.data);
}
static void spawns(CG *g) {
    Node *entry;
    for (entry = g->c->spawns; entry; entry = entry->next) {
        Node *n = entry->a, *arg;
        Type *result = n->type->items->type;
        Buffer call = {0}, sig = {0};
        ArgValue *values = NULL, **tail = &values;
        int i = 0;
        line(g, "struct tc_spawn_frame_%d {", n->id);
        g->indent++;
        line(g, "void *future;");
        line(g, "%s function;", n->a->a->type->cname);
        for (arg = n->a->args; arg; arg = arg->next)
            line(g, "%s a%d;", arg->type->cname, i++);
        g->indent--;
        line(g, "};");
        line(g, "static void tc_spawn_step_%d(void *raw) {", n->id);
        g->indent++;
        line(g, "struct tc_spawn_frame_%d *frame = (struct tc_spawn_frame_%d *)raw;", n->id, n->id);
        i = 0;
        for (arg = n->a->args; arg; arg = arg->next) {
            ArgValue *v = (ArgValue *)tc_alloc(g->c, sizeof(ArgValue));
            v->arg = arg;
            v->value = tc_format(g->c, "frame->a%d", i++);
            *tail = v;
            tail = &v->next;
        }
        buf_add(&call, "frame->function(");
        append_args(g, &call, values, n->a->resolved ? n->a->resolved->params : NULL, 1);
        buf_add(&call, ")");
        if (result->kind == TY_VOID)
            line(g, "%s;", call.data);
        else
            line(g, "%s result = %s;", result->cname, call.data);
        line(g, "tc_future_complete(frame->future, %s, 0);",
             result->kind == TY_VOID ? "NULL" : "&result");
        line(g, "tc_future_release(frame->future); free(frame); tc_task_end();");
        g->indent--;
        line(g, "}");
        buf_printf(&sig, "static %s tc_spawn_%d(%s function", n->type->cname, n->id,
                   n->a->a->type->cname);
        i = 0;
        for (arg = n->a->args; arg; arg = arg->next) {
            buf_add(&sig, ", ");
            buf_printf(&sig, "%s a%d", arg->type->cname, i++);
        }
        buf_add(&sig, ") {");
        line(g, "%s", sig.data);
        g->indent++;
        line(g,
             "struct tc_spawn_frame_%d *frame = (struct tc_spawn_frame_%d "
             "*)tc_alloc_checked(sizeof(*frame));",
             n->id, n->id);
        line(g, "frame->future = tc_future_create(%s);",
             result->kind == TY_VOID ? "0" : tc_format(g->c, "sizeof(%s)", result->cname));
        line(g, "frame->function = function;");
        line(g,
             "tc_assert(frame->future != NULL, \"<spawn>\", 0); tc_future_retain(frame->future);");
        i = 0;
        for (arg = n->a->args; arg; arg = arg->next) {
            line(g, "frame->a%d = a%d;", i, i);
            i++;
        }
        line(g, "%s result = {frame->future};", n->type->cname);
        line(g, "tc_task_begin();");
        line(g,
             "tc_assert(tc_pool_submit(tc_scheduler_pool(), tc_spawn_step_%d, frame) == 0, "
             "\"<spawn>\", 0);",
             n->id);
        line(g, "return result;");
        g->indent--;
        line(g, "}");
        free(call.data);
        free(sig.data);
    }
}
char *generate_c(Context *c) {
    CG g;
    Node *n, *m;
    Type *t;
    char *result;
    int has_main = 0;
    memset(&g, 0, sizeof(g));
    g.c = c;
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_FUNCTION && !strcmp(n->name, "main")) {
            n->cname = "tc_user_main";
            n->sym->cname = n->cname;
            has_main = 1;
        }
    line(&g, "/* Generated by TinyC+. Inspectable C11; do not edit. */");
    line(&g, "#include \"tiny_runtime.h\"");
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_MODULE && !strcmp(n->name, "std.concurrent")) {
            line(&g, "#include \"concurrent.c\"");
            break;
        }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_MODULE &&
            (!strcmp(n->name, "std.string") || !strcmp(n->name, "std.fs") ||
             !strcmp(n->name, "std.io") || !strcmp(n->name, "std.process"))) {
            line(&g, "#include \"library.c\"");
            break;
        }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_MODULE && !strcmp(n->name, "std.net")) {
            line(&g, "#include \"net.c\"");
            c->uses_io = 1;
            break;
        }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_MODULE && !strcmp(n->name, "std.grpc")) {
            line(&g, "#include \"grpc.c\"");
            c->uses_grpc = 1;
            break;
        }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_MODULE && !strcmp(n->name, "std.protobuf")) {
            line(&g, "#include \"protobuf.c\"");
            break;
        }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_MODULE && !strcmp(n->name, "std.tui")) {
            line(&g, "#include \"tui.c\"");
            break;
        }
    if (c->repl_mode) {
        line(&g, "extern void *tc_repl_slot(const char *name, size_t size);");
        line(&g, "extern int tc_repl_initialize(const char *name, size_t size);");
    }
    for (t = c->types; t; t = t->next) {
        t->is_const = 0;
        if (!t->alias && !t->qualified && t->kind == TY_NAMED && t->decl &&
            (t->decl->kind == N_CLASS || t->decl->kind == N_INTERFACE) && !t->decl->params)
            line(&g, "typedef struct %s %s;", t->cname, t->cname);
    }
    for (t = c->types; t; t = t->next)
        emit_type(&g, t);
    for (n = c->program->body; n; n = n->next) {
        if (n->kind == N_FUNCTION && !n->args)
            signature(&g, n, 1);
        if (n->kind == N_CLASS && !n->params)
            for (m = n->body; m; m = m->next) {
                if (m->kind == N_FUNCTION)
                    signature(&g, m, 1);
                if (m->kind == N_PROPERTY && m->a)
                    line(&g, "%s %s_get_%s(%s *tc_self);", m->decl_type->cname, n->type->cname,
                         m->name, n->type->cname);
            }
    }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_VAR) {
            if (c->repl_mode)
                line(&g, "#define %s (*((%s *)tc_repl_slot(%s, sizeof(%s))))", n->cname,
                     n->type->cname, quote(c, n->name), n->type->cname);
            else {
                if (n->a && n->a->kind > N_NULL)
                    tc_error(c, n->loc, "global initializer must be a literal");
                statement(&g, n);
            }
        }
    adapters(&g);
    lambdas(&g);
    spawns(&g);
    for (n = c->program->body; n; n = n->next) {
        if (n->kind == N_FUNCTION && !n->args) {
            if (n->flags & NF_ASYNC)
                async_function(&g, n);
            else
                function(&g, n);
        }
        if (n->kind == N_CLASS && !n->params)
            for (m = n->body; m; m = m->next) {
                if (m->kind == N_FUNCTION) {
                    if (m->flags & NF_ASYNC)
                        async_function(&g, m);
                    else
                        function(&g, m);
                }
                if (m->kind == N_PROPERTY && m->a) {
                    const char *s;
                    g.fn = m;
                    line(&g, "%s %s_get_%s(%s *tc_self) {", m->decl_type->cname, n->type->cname,
                         m->name, n->type->cname);
                    g.indent++;
                    s = expression(&g, m->a);
                    line(&g, "return %s;", s);
                    g.indent--;
                    line(&g, "}");
                }
            }
    }
    if (has_main || c->test_mode) {
        line(&g, "int main(int argc, char **argv) {");
        g.indent++;
        line(&g, "tc_program_argc = argc; tc_program_argv = argv;");
        if (c->uses_tasks) {
            line(&g, "const char *setting = getenv(\"TINY_WORKERS\"); int workers = setting ? "
                     "atoi(setting) : 4;");
            line(&g, "if (workers < 1 || workers > 256) { fputs(\"TINY_WORKERS must be "
                     "1..256\\n\", stderr); return 2; }");
            line(&g, "tc_scheduler_start(workers);");
            if (c->uses_io)
                line(&g, "tc_io_start();");
        }
        if (c->repl_mode)
            for (n = c->program->body; n; n = n->next)
                if (n->kind == N_VAR && n->a) {
                    line(&g, "if (tc_repl_initialize(%s, sizeof(%s))) {", quote(c, n->name),
                         n->type->cname);
                    g.indent++;
                    {
                        const char *initial = expression(&g, n->a), *copy = temp(&g);
                        define_value(&g, n->type->cname, copy,
                                     convert(&g, n->type, n->a->type, initial));
                        line(&g, "memcpy((void *)&(%s), &(%s), sizeof(%s));", n->cname, copy,
                             n->type->cname);
                    }
                    g.indent--;
                    line(&g, "}");
                }
        if (c->test_mode) {
            int tests = 0;
            line(&g, "int result = 0;");
            for (n = c->program->body; n; n = n->next)
                if (n->kind == N_FUNCTION && (n->flags & NF_TEST)) {
                    if (n->decl_type->kind == TY_VOID)
                        line(&g, "%s(); puts(\"PASS %s\");", n->cname, n->name);
                    else
                        line(&g,
                             "if (%s() != 0) { puts(\"FAIL %s\"); result = 1; } else puts(\"PASS "
                             "%s\");",
                             n->cname, n->name, n->name);
                    tests++;
                }
            if (!tests) {
                line(&g, "puts(\"No @test functions found.\"); result = 1;");
            } else
                line(&g,
                     "printf(\"%d tests executed; %%s\\n\", result ? \"failures\" : \"passed\");",
                     tests);
        } else
            line(&g, "int result = tc_user_main();");
        if (c->uses_tasks)
            line(&g, "tc_scheduler_shutdown();");
        if (c->uses_io)
            line(&g, "tc_io_shutdown();");
        line(&g, "if (tc_program_cleanup) tc_program_cleanup();");
        line(&g, "return result;");
        g.indent--;
        line(&g, "}");
    }
    result = tc_str(c, g.out.data);
    free(g.out.data);
    return result;
}
