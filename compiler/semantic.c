#include "tiny.h"
#include <errno.h>
#include <limits.h>

static Type *check_expr(Context *, Node *, Type *);
static void check_stmt(Context *, Node *);
static void check_function(Context *, Node *);
static Node *member(Type *, const char *);
static Type *element(Type *t) {
    return t->kind == TY_STRING ? NULL : t->kind == TY_NAMED && t->items ? t->items->type : t->base;
}
static int collection(Type *t) {
    return t->kind == TY_ARRAY || t->kind == TY_SLICE || t->kind == TY_STRING ||
           (t->kind == TY_NAMED && t->items && !strcmp(t->name, "Array"));
}
static Scope *scope_push(Context *c) {
    Scope *s = (Scope *)tc_alloc(c, sizeof(Scope));
    s->parent = c->scope;
    c->scope = s;
    return s;
}
static void scope_pop(Context *c) {
    c->scope = c->scope->parent;
}
static Symbol *lookup(Context *c, const char *name) {
    Scope *scope;
    Symbol *v, *candidate = NULL;
    int matches = 0;
    const char *file = c->current_fn ? c->current_fn->loc.file : c->active_file;
    for (scope = c->scope; scope; scope = scope->parent)
        for (v = scope->symbols; v; v = v->next)
            if (!strcmp(v->name, name)) {
                if (scope != c->global || (file && !strcmp(v->node->loc.file, file)))
                    return v;
                candidate = v;
                matches++;
            }
    if (matches > 1) {
        Loc l = {file, 1, 1};
        tc_error(c, l, "ambiguous imported name '%s'; qualify it with its module", name);
    }
    return candidate;
}
static Symbol *bind(Context *c, Node *n, Type *t, const char *cname) {
    Symbol *s;
    for (s = c->scope->symbols; s; s = s->next)
        if (!strcmp(s->name, n->name)) {
            if (c->scope == c->global && strcmp(s->node->loc.file, n->loc.file) &&
                !(n->flags & NF_EXTERN) && !(s->node->flags & NF_EXTERN) && strcmp(n->name, "main"))
                continue;
            if ((n->flags & NF_EXTERN) && (s->node->flags & NF_EXTERN) && type_equal(t, s->type)) {
                n->sym = s;
                n->type = s->type;
                n->cname = s->cname;
                return s;
            }
            tc_error(c, n->loc, "duplicate declaration '%s'", n->name);
        }
    if (!(n->flags & NF_EXTERN) && (!strncmp(n->name, "tc_", 3) || !strncmp(n->name, "__", 2)))
        tc_error(c, n->loc, "name '%s' uses a reserved compiler prefix", n->name);
    s = (Symbol *)tc_alloc(c, sizeof(Symbol));
    s->name = n->name;
    s->type = t;
    s->node = n;
    s->function = c->current_fn;
    s->cname = cname ? cname : tc_format(c, "tc_v_%d_%s", n->id, n->name);
    s->next = c->scope->symbols;
    c->scope->symbols = s;
    n->sym = s;
    n->type = t;
    n->cname = s->cname;
    return s;
}
static void valid_type(Context *c, Type *t, Loc l) {
    TypeLink *it;
    if (!t)
        tc_error(c, l, "missing type");
    if (t->kind == TY_AUTO)
        return;
    if (t->kind == TY_NAMED) {
        if (t->items && (!strcmp(t->name, "Array") || !strcmp(t->name, "Channel") ||
                         !strcmp(t->name, "Task") || !strcmp(t->name, "Future"))) {
            if (t->items->next)
                tc_error(c, l, "%s requires one type argument", t->name);
            if (!strcmp(t->name, "Task"))
                c->uses_tasks = 1;
            valid_type(c, t->items->type, l);
            return;
        }
        if (!t->decl)
            tc_error(c, l, "unknown type '%s'", t->name);
    }
    if (t->base) {
        valid_type(c, t->base, l);
        if ((t->kind == TY_ARRAY || t->kind == TY_SLICE) && t->base->kind == TY_VOID)
            tc_error(c, l, "collection element cannot be void");
    }
    for (it = t->items; it; it = it->next)
        valid_type(c, it->type, l);
}
static int scalar(Type *t) {
    return type_numeric(t) || t->kind == TY_PTR || t->kind == TY_NULL;
}
static TypeKind promote(TypeKind kind) {
    return kind == TY_ENUM || kind == TY_BOOL || kind == TY_CHAR ||
                   (kind >= TY_I8 && kind <= TY_U16)
               ? TY_I32
               : kind;
}
static TypeKind numeric_common(TypeKind a, TypeKind b) {
    a = promote(a);
    b = promote(b);
    if (a == TY_DOUBLE || b == TY_DOUBLE)
        return TY_DOUBLE;
    if (a == TY_FLOAT || b == TY_FLOAT)
        return TY_FLOAT;
    return a > b ? a : b;
}
static int compatible(Type *dst, Type *src) {
    Type unqualified_dst, unqualified_src;
    /* Top-level const affects mutation, not copying a value. Nested pointer const remains. */
    if (dst->qualified) {
        unqualified_dst = *dst;
        unqualified_dst.qualified = 0;
        unqualified_dst.name += 6;
        dst = &unqualified_dst;
    }
    if (src->qualified) {
        unqualified_src = *src;
        unqualified_src.qualified = 0;
        unqualified_src.name += 6;
        src = &unqualified_src;
    }
    if (type_equal(dst, src))
        return 1;
    if (type_numeric(dst) && type_numeric(src))
        return 1;
    if (dst->kind == TY_PTR &&
        (src->kind == TY_NULL ||
         (src->kind == TY_PTR && (dst->base->kind == TY_VOID || src->base->kind == TY_VOID))))
        return 1;
    if (dst->kind == TY_PTR && src->kind == TY_PTR && dst->base->qualified &&
        !src->base->qualified && dst->base->kind == src->base->kind) {
        Type copy = *dst->base;
        copy.qualified = 0;
        copy.name += 6;
        return type_equal(&copy, src->base);
    }
    if (dst->kind == TY_SLICE && src->kind == TY_ARRAY && type_equal(dst->base, src->base))
        return 1;
    return 0;
}
static void require(Context *c, Type *want, Type *got, Loc l) {
    if (want->kind == TY_NAMED && want->decl && want->decl->kind == N_INTERFACE &&
        got->kind == TY_PTR && got->base->decl) {
        Node *m, *a;
        for (m = want->decl->body; m; m = m->next) {
            Node *implementation = member(got->base, m->name);
            if (m->kind != N_FUNCTION || !implementation || implementation->kind != N_FUNCTION ||
                !type_equal(m->type, implementation->type))
                tc_error(c, l,
                         "%s does not satisfy interface %s: method '%s' is missing or incompatible",
                         got->base->name, want->name, m->name);
        }
        for (a = c->adapters; a; a = a->next)
            if (a->type == want && a->decl_type == got)
                return;
        a = node(c, N_INTERFACE, l);
        a->type = want;
        a->decl_type = got;
        a->next = c->adapters;
        c->adapters = a;
        return;
    }
    if (!compatible(want, got))
        tc_error(c, l, "expected %s, found %s", type_name(c, want), type_name(c, got));
}
static void condition(Context *c, Node *n) {
    Type *t = check_expr(c, n, NULL);
    if (!scalar(t))
        tc_error(c, n->loc, "condition requires a scalar, found %s", t->name);
}
static Type *function_type(Context *c, Node *n) {
    Type *t = (Type *)tc_alloc(c, sizeof(Type));
    Node *p;
    TypeLink **tail = &t->items;
    t->kind = TY_FUNC;
    t->id = ++c->next_id;
    t->base = n->decl_type;
    t->name = tc_format(c, "func<%s>", n->name);
    t->cname = tc_format(c, "tc_func_%d", t->id);
    t->decl = n;
    t->callconv = !!(n->flags & NF_STDCALL);
    if (n->flags & NF_ASYNC) {
        TypeLink *result = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
        result->type = n->decl_type;
        n->task_type = type_generic(c, "Task", result);
        t->base = n->task_type;
        c->uses_tasks = 1;
    }
    for (p = n->params; p; p = p->next) {
        TypeLink *x;
        if (!strcmp(p->name, "..."))
            break;
        x = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
        x->type = p->decl_type;
        *tail = x;
        tail = &x->next;
    }
    t->next = c->types;
    c->types = t;
    return t;
}
static Node *member(Type *t, const char *name) {
    Node *m;
    if (t->kind == TY_PTR)
        t = t->base;
    if (!t->decl)
        return NULL;
    for (m = t->decl->body; m; m = m->next)
        if (m->name && !strcmp(m->name, name))
            return m;
    return NULL;
}
static const char *dotted_name(Context *c, Node *n) {
    const char *base;
    if (n->kind == N_ID)
        return n->name;
    if (n->kind != N_MEMBER || (base = dotted_name(c, n->a)) == NULL)
        return NULL;
    return tc_format(c, "%s.%s", base, n->name);
}
static int qualified_value(Context *c, Node *n) {
    const char *full = dotted_name(c, n), *dot;
    Node *module;
    Symbol *symbol;
    Type *type;
    if (!full || (dot = strrchr(full, '.')) == NULL)
        return 0;
    {
        Scope *scope;
        size_t first = (size_t)(strchr(full, '.') - full);
        for (scope = c->scope; scope; scope = scope->parent)
            for (symbol = scope->symbols; symbol; symbol = symbol->next)
                if (strlen(symbol->name) == first && !strncmp(symbol->name, full, first))
                    return 0;
    }
    for (module = c->loaded_modules; module; module = module->next)
        if (module->label && strlen(module->label) == (size_t)(dot - full) &&
            !strncmp(module->label, full, (size_t)(dot - full)))
            break;
    if (!module)
        return 0;
    for (symbol = c->global->symbols; symbol; symbol = symbol->next)
        if (!strcmp(symbol->name, dot + 1) && !strcmp(symbol->node->loc.file, module->name)) {
            n->kind = N_ID;
            n->name = symbol->name;
            n->sym = symbol;
            n->cname = symbol->cname;
            n->type = symbol->type;
            n->flags |= NF_CHECKED;
            if (symbol->node->kind == N_VAR)
                n->flags |= NF_LVALUE;
            return 1;
        }
    for (type = c->types; type; type = type->next)
        if (type->decl && !strcmp(type->name, dot + 1) &&
            !strcmp(type->decl->loc.file, module->name)) {
            n->kind = N_ID;
            n->name = type->name;
            n->type = type;
            n->flags |= NF_STATIC;
            return 1;
        }
    tc_error(c, n->loc, "module '%s' has no declaration '%s'", module->label, dot + 1);
    return 0;
}
static void check_args(Context *c, Node *call, Node *params) {
    Node *arg, *p;
    int named = 0;
    p = params;
    for (arg = call->args; arg; arg = arg->next) {
        Node *target = p, *earlier;
        if (arg->label) {
            named = 1;
            for (target = params; target; target = target->next)
                if (!strcmp(target->name, arg->label))
                    break;
            if (!target)
                tc_error(c, arg->loc, "unknown argument label '%s'", arg->label);
        } else if (named)
            tc_error(c, arg->loc, "positional argument cannot follow named arguments");
        if (!target)
            tc_error(c, arg->loc, "too many arguments");
        if (!strcmp(target->name, "...")) {
            check_expr(c, arg, NULL);
            continue;
        }
        for (earlier = call->args; earlier != arg; earlier = earlier->next)
            if (earlier->parameter == target)
                tc_error(c, arg->loc, "argument '%s' supplied twice", target->name);
        arg->parameter = target;
        require(c, target->decl_type, check_expr(c, arg, target->decl_type), arg->loc);
        if (!arg->label)
            p = p->next;
    }
    for (p = params; p; p = p->next) {
        if (!strcmp(p->name, "..."))
            break;
        for (arg = call->args; arg && arg->parameter != p; arg = arg->next) {
        }
        if (!arg && p->a) {
            Node **tail = &call->args;
            arg = node(c, p->a->kind, p->a->loc);
            *arg = *p->a;
            arg->next = NULL;
            arg->parameter = p;
            if (arg->kind < N_INT || arg->kind > N_NULL)
                tc_error(c, p->loc, "default arguments must be literals");
            require(c, p->decl_type, check_expr(c, arg, p->decl_type), arg->loc);
            while (*tail)
                tail = &(*tail)->next;
            *tail = arg;
        }
        if (!arg)
            tc_error(c, call->loc, "missing argument '%s'", p->name);
    }
}
static Type *stream_callback(Context *c, Node *fn, Type *input, Type *second, Type *result) {
    Type expected = {0}, *actual;
    TypeLink first = {0}, next = {0}, *p;
    expected.kind = TY_FUNC;
    expected.base = result ? result : type_primitive(c, TY_AUTO);
    expected.items = &first;
    first.type = input;
    if (second) {
        first.next = &next;
        next.type = second;
    }
    actual = check_expr(c, fn, &expected);
    if (actual->kind != TY_FUNC && actual->kind != TY_CLOSURE)
        tc_error(c, fn->loc, "stream operation requires a function");
    p = actual->items;
    if (!p || !type_equal(p->type, input))
        tc_error(c, fn->loc, "stream callback input type mismatch");
    p = p->next;
    if (second) {
        if (!p || !type_equal(p->type, second))
            tc_error(c, fn->loc, "stream reducer input type mismatch");
        p = p->next;
    }
    if (p)
        tc_error(c, fn->loc, "stream callback has too many parameters");
    if (result)
        require(c, result, actual->base, fn->loc);
    return actual->base;
}
static Type *stream_stage(Context *c, Node *n) {
    Type *input;
    if (n->kind != N_CALL || n->a->kind != N_MEMBER)
        tc_error(c, n->loc, "invalid stream pipeline");
    if (!strcmp(n->a->name, "stream")) {
        Type *source = check_expr(c, n->a->a, NULL);
        if (n->args || !collection(source))
            tc_error(c, n->loc, "stream() requires an array, slice or string");
        n->decl_type = source->kind == TY_STRING ? type_primitive(c, TY_CHAR) : element(source);
        return n->decl_type;
    }
    input = stream_stage(c, n->a->a);
    if (!n->args || n->args->next)
        tc_error(c, n->loc, "stream stage requires one callback");
    if (!strcmp(n->a->name, "filter")) {
        stream_callback(c, n->args, input, NULL, type_primitive(c, TY_BOOL));
        n->decl_type = input;
    } else if (!strcmp(n->a->name, "map")) {
        n->decl_type = stream_callback(c, n->args, input, NULL, NULL);
        if (n->decl_type->kind == TY_VOID)
            tc_error(c, n->loc, "stream map cannot return void");
    } else
        tc_error(c, n->loc, "unknown stream stage '%s'", n->a->name);
    return n->decl_type;
}
static int stream_call(Context *c, Node *n) {
    Node *stage;
    Type *item;
    const char *name;
    int depth = 0;
    if (n->a->kind != N_MEMBER)
        return 0;
    stage = n->a->a;
    while (stage && stage->kind == N_CALL && stage->a->kind == N_MEMBER) {
        if (++depth > 128)
            tc_error(c, n->loc, "stream stage limit exceeded");
        if (!strcmp(stage->a->name, "stream"))
            break;
        stage = stage->a->a;
    }
    if (!stage || stage->kind != N_CALL || stage->a->kind != N_MEMBER ||
        strcmp(stage->a->name, "stream"))
        return 0;
    name = n->a->name;
    if (!strcmp(name, "map") || !strcmp(name, "filter"))
        tc_error(c, n->loc, "stream pipeline needs a terminal operation");
    item = stream_stage(c, n->a->a);
    n->decl_type = item;
    if (!strcmp(name, "count")) {
        if (n->args)
            tc_error(c, n->loc, "count takes no arguments");
        n->type = type_primitive(c, TY_U64);
    } else if (!strcmp(name, "first")) {
        TypeLink *a = (TypeLink *)tc_alloc(c, sizeof(TypeLink)),
                 *b = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
        if (n->args)
            tc_error(c, n->loc, "first takes no arguments");
        a->type = item;
        a->next = b;
        b->type = type_primitive(c, TY_BOOL);
        n->type = type_tuple(c, a);
    } else if (!strcmp(name, "forEach") || !strcmp(name, "any") || !strcmp(name, "all")) {
        Type *ret = type_primitive(c, !strcmp(name, "forEach") ? TY_VOID : TY_BOOL);
        if (!n->args || n->args->next)
            tc_error(c, n->loc, "stream terminal requires one callback");
        stream_callback(c, n->args, item, NULL, ret);
        n->type = ret;
    } else if (!strcmp(name, "reduce")) {
        if (!n->args || !n->args->next || n->args->next->next)
            tc_error(c, n->loc, "reduce requires an initial value and reducer");
        n->type = check_expr(c, n->args, NULL);
        stream_callback(c, n->args->next, n->type, item, n->type);
    } else if (!strcmp(name, "collect")) {
        Type *target;
        if (!n->args || n->args->next)
            tc_error(c, n->loc, "collect requires a pointer to an Array destination");
        target = check_expr(c, n->args, NULL);
        if (target->kind != TY_PTR || target->base->kind != TY_NAMED ||
            strcmp(target->base->name, "Array") || !target->base->items ||
            !type_equal(target->base->items->type, item))
            tc_error(c, n->loc, "collect destination must be Array<element>*");
        n->type = type_primitive(c, TY_VOID);
    } else
        tc_error(c, n->loc, "unknown stream terminal '%s'", name);
    n->text = tc_format(c, "stream_%s", name);
    return 1;
}
static int builtin_call(Context *c, Node *n) {
    const char *name;
    Node *a;
    Type *t;
    int count = 0;
    if (stream_call(c, n))
        return 1;
    if (n->a->kind == N_MEMBER && (!strcmp(n->a->name, "get") || !strcmp(n->a->name, "result") ||
                                   !strcmp(n->a->name, "retain") || !strcmp(n->a->name, "ready") ||
                                   !strcmp(n->a->name, "destroy"))) {
        Type *receiver = check_expr(c, n->a->a, NULL);
        if (receiver->kind == TY_CLOSURE && !strcmp(n->a->name, "destroy")) {
            if (n->args)
                tc_error(c, n->loc, "closure destroy takes no arguments");
            n->text = "closure_destroy";
            n->type = type_primitive(c, TY_VOID);
            return 1;
        }
        if (receiver->kind == TY_NAMED && receiver->items && !strcmp(receiver->name, "Task")) {
            if (n->args)
                tc_error(c, n->loc, "Task method takes no arguments");
            n->text = tc_format(c, "task_%s", n->a->name);
            n->type = !strcmp(n->a->name, "get")
                          ? receiver->items->type
                          : type_primitive(c, !strcmp(n->a->name, "ready") ? TY_BOOL : TY_VOID);
            if (!strcmp(n->a->name, "result")) {
                if (receiver->items->type->kind == TY_VOID)
                    n->type = type_primitive(c, TY_I32);
                else {
                    TypeLink *value = (TypeLink *)tc_alloc(c, sizeof(TypeLink)),
                             *error = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
                    value->type = receiver->items->type;
                    value->next = error;
                    error->type = type_primitive(c, TY_I32);
                    n->type = type_tuple(c, value);
                }
            }
            return 1;
        }
    }
    if (n->a->kind != N_ID)
        return 0;
    name = n->a->name;
    if (!strcmp(name, "owned")) {
        if (!n->args || n->args->next || n->args->kind != N_LAMBDA)
            tc_error(c, n->loc, "owned expects one lambda literal");
        n->args->flags |= NF_OWNED;
        n->type = check_expr(c, n->args, NULL);
        n->flags |= NF_OWNED;
        n->text = "owned";
        return 1;
    }
    if (strcmp(name, "println") && strcmp(name, "print") && strcmp(name, "assert") &&
        strcmp(name, "len") && strcmp(name, "hash"))
        return 0;
    /* hash is a builtin only when nothing else is called hash: a symbol in scope or a
       method of the current class wins, exactly as check_expr resolves N_ID. */
    if (!strcmp(name, "hash") &&
        (lookup(c, name) || (c->current_class && member(c->current_class->type, name))))
        return 0;
    for (a = n->args; a; a = a->next) {
        check_expr(c, a, NULL);
        count++;
    }
    if (count != 1)
        tc_error(c, n->loc, "%s expects one argument", name);
    t = n->args->type;
    if (!strcmp(name, "assert")) {
        if (!scalar(t))
            tc_error(c, n->loc, "assert expects a scalar");
        n->type = type_primitive(c, TY_VOID);
    } else if (!strcmp(name, "hash")) {
        if (!type_integer(t) && t->kind != TY_BOOL && t->kind != TY_CHAR && t->kind != TY_ENUM &&
            t->kind != TY_PTR && t->kind != TY_STRING)
            tc_error(c, n->loc,
                     "hash supports integers, char, bool, enums, pointers and strings, not %s",
                     t->name);
        n->type = type_primitive(c, TY_U64);
    } else if (!strcmp(name, "len")) {
        if (t->kind != TY_STRING && t->kind != TY_ARRAY && t->kind != TY_SLICE &&
            !(t->kind == TY_NAMED && !strcmp(t->name, "Array")))
            tc_error(c, n->loc, "len expects a string, array or slice");
        n->type = type_primitive(c, TY_U64);
    } else {
        if (!scalar(t) && t->kind != TY_STRING)
            tc_error(c, n->loc, "%s does not support %s", name, t->name);
        n->type = type_primitive(c, TY_VOID);
    }
    n->text = name;
    return 1;
}
static Type *check_expr(Context *c, Node *n, Type *expected) {
    Type *a, *b;
    Node *it;
    TypeLink *items, **tail;
    if (!n)
        return type_primitive(c, TY_VOID);
    if (++c->depth > 256)
        tc_error(c, n->loc, "expression nesting limit exceeded");
    switch (n->kind) {
    case N_INT: {
        unsigned long long v;
        errno = 0;
        v = strtoull(n->text, NULL,
                     !strncmp(n->text, "0x", 2) || !strncmp(n->text, "0X", 2) ? 16 : 10);
        if (errno)
            tc_error(c, n->loc, "integer literal is out of range");
        n->type = type_primitive(c, v <= INT32_MAX ? TY_I32 : v <= INT64_MAX ? TY_I64 : TY_U64);
        break;
    }
    case N_FLOAT:
        n->type = type_primitive(c, TY_DOUBLE);
        break;
    case N_CHAR:
        n->type = type_primitive(c, TY_CHAR);
        break;
    case N_BOOL:
        n->type = type_primitive(c, TY_BOOL);
        break;
    case N_STRING:
        n->type = type_primitive(c, TY_STRING);
        break;
    case N_NULL:
        n->type = type_primitive(c, TY_NULL);
        break;
    case N_ID: {
        if (n->flags & NF_CHECKED)
            break;
        Symbol *s = lookup(c, n->name);
        if (n->decl_type && n->decl_type->decl && n->decl_type->decl->kind == N_FUNCTION)
            s = lookup(c, n->decl_type->decl->name);
        if (!s && c->current_class) {
            Node *m = member(c->current_class->type, n->name);
            if (m) {
                n->resolved = m;
                n->cname = m->kind == N_FUNCTION ? m->cname : tc_format(c, "tc_self->%s", m->name);
                n->type = m->kind == N_FUNCTION ? m->type : m->decl_type;
                n->flags |= NF_LVALUE;
                break;
            }
        }
        if (!s) {
            if (!strcmp(n->name, "this") && c->current_class) {
                n->type = type_derive(c, TY_PTR, c->current_class->type, 0);
                n->cname = "tc_self";
                break;
            }
            a = n->decl_type ? n->decl_type : type_named(c, n->name);
            if (a->decl || a->items) {
                n->type = a;
                n->flags |= NF_STATIC;
                break;
            }
            tc_error(c, n->loc, "undefined identifier '%s'", n->name);
        }
        n->sym = s;
        n->type = s->type;
        n->cname = s->cname;
        n->flags |= s->node->flags & NF_OWNED;
        if (c->current_fn && c->current_fn->kind == N_LAMBDA && s->function &&
            s->function != c->current_fn) {
            Node *capture, *fn;
            for (fn = c->current_fn; fn && fn->kind == N_LAMBDA && fn != s->function;
                 fn = fn->owner) {
                for (capture = fn->args; capture && capture->sym != s; capture = capture->next) {
                }
                if (!capture) {
                    capture = node(c, N_ID, n->loc);
                    capture->name = s->name;
                    capture->sym = s;
                    capture->type = s->type;
                    capture->cname = s->cname;
                    capture->next = fn->args;
                    fn->args = capture;
                }
            }
            n->cname = tc_format(c, "tc_env->capture_%d", s->node->id);
        }
        if (s->node->kind == N_VAR)
            n->flags |= NF_LVALUE;
        break;
    }
    case N_BINARY:
        a = check_expr(c, n->a, NULL);
        b = check_expr(c, n->b, a);
        if (!strcmp(n->text, "=") ||
            (strlen(n->text) == 2 && n->text[1] == '=' && strchr("+-*/%&|^", n->text[0]))) {
            if (!(n->a->flags & NF_LVALUE))
                tc_error(c, n->loc, "assignment requires a mutable location");
            if (n->a->type->qualified)
                tc_error(c, n->loc, "cannot assign through a const location");
            if (n->a->sym && (n->a->sym->node->flags & NF_CONST))
                tc_error(c, n->loc, "cannot assign to const variable");
            require(c, a, b, n->loc);
            if (strcmp(n->text, "=") && (!type_numeric(a) || !type_numeric(b)))
                tc_error(c, n->loc, "compound assignment requires numeric operands");
            n->type = a;
        } else if (!strcmp(n->text, "&&") || !strcmp(n->text, "||")) {
            if (!scalar(a) || !scalar(b))
                tc_error(c, n->loc, "logical operator requires scalars");
            n->type = type_primitive(c, TY_BOOL);
        } else if (!strcmp(n->text, "==") || !strcmp(n->text, "!=")) {
            if (!compatible(a, b) && !compatible(b, a))
                tc_error(c, n->loc, "incompatible comparison");
            if (!scalar(a) && a->kind != TY_STRING)
                tc_error(c, n->loc, "type has no equality operation");
            n->type = type_primitive(c, TY_BOOL);
        } else if (a->kind == TY_PTR && type_integer(b) &&
                   (!strcmp(n->text, "+") || !strcmp(n->text, "-"))) {
            if (a->base->kind == TY_VOID)
                tc_error(c, n->loc, "pointer arithmetic on void pointer");
            n->type = a;
        } else {
            if (!type_numeric(a) || !type_numeric(b))
                tc_error(c, n->loc, "operator '%s' requires numbers", n->text);
            if (strchr("%&|^", n->text[0]) || !strcmp(n->text, "<<") || !strcmp(n->text, ">>")) {
                if (!type_integer(a) || !type_integer(b))
                    tc_error(c, n->loc, "operator requires integers");
            }
            n->type = type_primitive(c, !strcmp(n->text, "<<") || !strcmp(n->text, ">>")
                                            ? promote(a->kind)
                                            : numeric_common(a->kind, b->kind));
            if (n->text[0] == '<' || n->text[0] == '>') {
                if (strlen(n->text) < 2 || n->text[1] == '=')
                    n->type = type_primitive(c, TY_BOOL);
            }
        }
        break;
    case N_UNARY:
        a = check_expr(c, n->a, NULL);
        n->type = a;
        if (!strcmp(n->text, "&")) {
            if (!(n->a->flags & NF_LVALUE))
                tc_error(c, n->loc, "address-of requires a location");
            n->type = type_derive(c, TY_PTR, a, 0);
        } else if (!strcmp(n->text, "*")) {
            if (a->kind != TY_PTR || a->base->kind == TY_VOID)
                tc_error(c, n->loc, "cannot dereference this type");
            n->type = a->base;
            n->flags |= NF_LVALUE;
        } else if (!strcmp(n->text, "!")) {
            if (!scalar(a))
                tc_error(c, n->loc, "logical not requires a scalar");
            n->type = type_primitive(c, TY_BOOL);
        } else {
            if (!type_numeric(a))
                tc_error(c, n->loc, "unary operator requires a number");
            if (!strcmp(n->text, "~") && !type_integer(a))
                tc_error(c, n->loc, "bitwise not requires integer");
            if (!strcmp(n->text, "++") || !strcmp(n->text, "--")) {
                if (!(n->a->flags & NF_LVALUE) || a->qualified)
                    tc_error(c, n->loc, "increment requires a mutable location");
            } else
                n->type = type_primitive(c, promote(a->kind));
        }
        break;
    case N_MEMBER:
        if (qualified_value(c, n))
            break;
        a = check_expr(c, n->a, NULL);
        if (a->kind == TY_PTR)
            a = a->base;
        if (a->kind == TY_NAMED && !strcmp(a->name, "Task") && !strcmp(n->name, "handle")) {
            n->type = type_derive(c, TY_PTR, type_primitive(c, TY_VOID), 0);
            n->flags |= NF_LVALUE;
        } else if (!strcmp(n->name, "length") &&
                   (a->kind == TY_ARRAY || a->kind == TY_SLICE || a->kind == TY_STRING ||
                    (a->kind == TY_NAMED && a->items && !strcmp(a->name, "Array"))))
            n->type = type_primitive(c, TY_U64);
        else if (!strcmp(n->name, "data") &&
                 (a->kind == TY_ARRAY || a->kind == TY_SLICE || a->kind == TY_STRING ||
                  (a->kind == TY_NAMED && a->items && !strcmp(a->name, "Array")))) {
            n->type = type_derive(c, TY_PTR,
                                  a->kind == TY_STRING ? type_const(c, type_primitive(c, TY_CHAR))
                                  : a->items           ? a->items->type
                                                       : a->base,
                                  0);
        } else {
            Node *m = member(a, n->name);
            if (!m)
                tc_error(c, n->loc, "%s has no member '%s'", a->name, n->name);
            n->resolved = m;
            n->type = m->kind == N_FUNCTION ? m->type : m->decl_type;
            if ((n->a->flags & NF_STATIC) && m->kind == N_FUNCTION && !(m->flags & NF_STATIC))
                tc_error(c, n->loc, "instance method requires an object");
            if (a->qualified && m->kind != N_FUNCTION)
                n->type = type_const(c, n->type);
            if ((m->kind == N_VAR && !(m->flags & NF_CONST)) || (m->kind == N_PROPERTY && !m->a))
                n->flags |= NF_LVALUE;
        }
        break;
    case N_CALL:
        if (n->a->kind == N_MEMBER)
            qualified_value(c, n->a);
        if (builtin_call(c, n))
            break;
        a = check_expr(c, n->a, NULL);
        if (a && a->kind == TY_NAMED && a->decl && a->decl->kind == N_CLASS &&
            (n->a->flags & NF_STATIC)) {
            n->kind = N_NEW;
            n->decl_type = a;
            n->flags |= NF_CONSTRUCTOR;
            c->depth--;
            return check_expr(c, n, expected);
        }
        if (!a || (a->kind != TY_FUNC && a->kind != TY_CLOSURE))
            tc_error(c, n->loc, "expression is not callable");
        n->resolved = a->decl;
        if (a->decl)
            check_args(c, n, a->decl->params);
        else {
            TypeLink *p = a->items;
            for (it = n->args; it; it = it->next) {
                if (!p)
                    tc_error(c, n->loc, "too many arguments");
                require(c, p->type, check_expr(c, it, p->type), it->loc);
                p = p->next;
            }
            if (p)
                tc_error(c, n->loc, "missing function argument");
        }
        n->type = a->base;
        if (n->type->kind == TY_CLOSURE)
            n->flags |= NF_OWNED;
        break;
    case N_INDEX:
        a = check_expr(c, n->a, NULL);
        b = check_expr(c, n->b, NULL);
        if (!type_integer(b))
            tc_error(c, n->loc, "index must be an integer");
        if (a->kind != TY_PTR && !collection(a))
            tc_error(c, n->loc, "value is not indexable");
        n->type = a->kind == TY_STRING ? type_const(c, type_primitive(c, TY_CHAR)) : element(a);
        if (a->kind == TY_ARRAY && a->qualified)
            n->type = type_const(c, n->type);
        if (a->kind != TY_STRING)
            n->flags |= NF_LVALUE;
        break;
    case N_SLICE:
        a = check_expr(c, n->a, NULL);
        if (!collection(a))
            tc_error(c, n->loc, "slicing requires array, slice or string");
        if (n->b && !type_integer(check_expr(c, n->b, NULL)))
            tc_error(c, n->loc, "slice bound must be integer");
        if (n->c && !type_integer(check_expr(c, n->c, NULL)))
            tc_error(c, n->loc, "slice bound must be integer");
        n->type = a->kind == TY_STRING ? a : type_derive(c, TY_SLICE, element(a), 0);
        break;
    case N_INIT:
        if (!expected || expected->kind != TY_ARRAY)
            tc_error(c, n->loc, "initializer list requires a static array type");
        {
            size_t count = 0;
            for (it = n->args; it; it = it->next) {
                require(c, expected->base, check_expr(c, it, expected->base), it->loc);
                count++;
            }
            if (count > expected->count)
                tc_error(c, n->loc, "too many array initializers");
        }
        n->type = expected;
        break;
    case N_TUPLE:
        items = NULL;
        tail = &items;
        for (it = n->args; it; it = it->next) {
            TypeLink *l = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
            l->type = check_expr(c, it, NULL);
            *tail = l;
            tail = &l->next;
        }
        n->type = type_tuple(c, items);
        break;
    case N_NEW:
        valid_type(c, n->decl_type, n->loc);
        a = n->decl_type;
        if (a->kind == TY_VOID || a->kind == TY_FUNC)
            tc_error(c, n->loc, "cannot allocate this type");
        if (a->kind == TY_NAMED) {
            Node *ctor = member(a, "init");
            if (ctor) {
                n->resolved = ctor;
                check_args(c, n, ctor->params);
            } else {
                Node *field = a->decl ? a->decl->body : NULL;
                for (it = n->args; it; it = it->next) {
                    while (field && field->kind != N_VAR && field->kind != N_PROPERTY)
                        field = field->next;
                    if (!field)
                        tc_error(c, n->loc, "too many constructor arguments");
                    require(c, field->decl_type, check_expr(c, it, field->decl_type), it->loc);
                    it->parameter = field;
                    field = field->next;
                }
            }
        } else {
            if (n->args) {
                require(c, a, check_expr(c, n->args, a), n->loc);
                if (n->args->next)
                    tc_error(c, n->loc, "scalar new takes at most one initializer");
            }
        }
        n->type = (n->flags & NF_CONSTRUCTOR) ? a : type_derive(c, TY_PTR, a, 0);
        break;
    case N_CAST:
        valid_type(c, n->decl_type, n->loc);
        a = check_expr(c, n->a, NULL);
        if (!(scalar(a) && scalar(n->decl_type)))
            tc_error(c, n->loc, "cast requires scalar types");
        n->type = n->decl_type;
        break;
    case N_SIZEOF:
        valid_type(c, n->decl_type, n->loc);
        if (n->decl_type->kind == TY_VOID)
            tc_error(c, n->loc, "sizeof(void) is invalid");
        n->type = type_primitive(c, TY_U64);
        break;
    case N_LAMBDA: {
        Node *previous_fn = c->current_fn, *previous_class = c->current_class, *p, *entry;
        TypeLink *pt = expected && (expected->kind == TY_FUNC || expected->kind == TY_CLOSURE)
                           ? expected->items
                           : NULL;
        int previous_loop = c->loop_depth, previous_switch = c->switch_loop;
        if (n->flags & NF_CHECKED)
            break;
        n->owner = previous_fn;
        n->name = tc_format(c, "lambda_%d", n->id);
        n->cname = tc_format(c, "tc_%s", n->name);
        n->decl_type = expected && (expected->kind == TY_FUNC || expected->kind == TY_CLOSURE)
                           ? expected->base
                           : type_primitive(c, TY_AUTO);
        c->current_fn = n;
        c->current_class = NULL;
        c->loop_depth = 0;
        c->switch_loop = 0;
        scope_push(c);
        for (p = n->params; p; p = p->next) {
            if (p->decl_type->kind == TY_AUTO) {
                if (!pt)
                    tc_error(c, p->loc,
                             "lambda parameter needs an explicit type or a typed call context");
                p->decl_type = pt->type;
            }
            valid_type(c, p->decl_type, p->loc);
            bind(c, p, p->decl_type, NULL);
            if (pt)
                pt = pt->next;
        }
        if (n->a)
            n->decl_type = check_expr(c, n->a, n->decl_type->kind == TY_AUTO ? NULL : n->decl_type);
        else {
            check_stmt(c, n->body);
            if (n->decl_type->kind == TY_AUTO)
                n->decl_type = type_primitive(c, TY_VOID);
        }
        scope_pop(c);
        c->current_fn = previous_fn;
        c->current_class = previous_class;
        c->loop_depth = previous_loop;
        c->switch_loop = previous_switch;
        n->type = function_type(c, n);
        if (n->args || (n->flags & NF_OWNED) || (expected && expected->kind == TY_CLOSURE))
            n->type->kind = TY_CLOSURE;
        entry = node(c, N_LAMBDA, n->loc);
        entry->a = n;
        entry->next = c->lambdas;
        c->lambdas = entry;
        n->flags |= NF_CHECKED;
        break;
    }
    case N_AWAIT:
        if (!c->current_fn || !(c->current_fn->flags & NF_ASYNC))
            tc_error(c, n->loc, "await is only valid inside an async function");
        a = check_expr(c, n->a, NULL);
        if (a->kind != TY_NAMED || !a->items ||
            (strcmp(a->name, "Task") && strcmp(a->name, "Future")))
            tc_error(c, n->loc, "await requires Task<T> or Future<T>");
        n->type = a->items->type;
        c->uses_tasks = 1;
        break;
    case N_SPAWN: {
        TypeLink *result;
        Node *entry;
        if (n->a->kind != N_CALL)
            tc_error(c, n->loc, "spawn requires a function call");
        a = check_expr(c, n->a, NULL);
        if (n->a->a->type->kind != TY_FUNC || (n->a->resolved && n->a->resolved->owner))
            tc_error(c, n->loc, "spawn requires a global function or function pointer");
        if (n->a->resolved && (n->a->resolved->flags & NF_ASYNC)) {
            n->type = a;
            n->text = "async";
            break;
        }
        result = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
        result->type = a;
        n->type = type_generic(c, "Task", result);
        n->task_type = n->type;
        entry = node(c, N_SPAWN, n->loc);
        entry->a = n;
        entry->next = c->spawns;
        c->spawns = entry;
        c->uses_tasks = 1;
        break;
    }
    default:
        tc_error(c, n->loc, "invalid expression node");
    }
    c->depth--;
    return n->type;
}
static void body_scope(Context *c, Node *n) {
    if (!n)
        return;
    scope_push(c);
    check_stmt(c, n);
    scope_pop(c);
}
/* Resolves a case label to a comparable constant key; returns NULL if it is not constant. */
static const char *case_key(Node *v) {
    if (v->kind == N_INT || v->kind == N_CHAR || v->kind == N_BOOL)
        return v->text;
    if (v->kind == N_STRING)
        return v->text;
    if (v->kind == N_UNARY && v->text && !strcmp(v->text, "-") && v->a && v->a->kind == N_INT)
        return v->a->text;
    if (v->kind == N_MEMBER && v->resolved && v->resolved->owner &&
        v->resolved->owner->kind == N_ENUM)
        return v->resolved->text;
    return NULL;
}
static int case_negative(Node *v) {
    return v->kind == N_UNARY;
}
static int case_same(Node *a, Node *b) {
    const char *x = case_key(a), *y = case_key(b);
    if (a->kind == N_MEMBER || b->kind == N_MEMBER)
        return !strcmp(x, y);
    if (a->kind == N_INT || case_negative(a)) {
        unsigned long long p = strtoull(x, NULL, 0), q = strtoull(y, NULL, 0);
        if (!strncmp(x, "0x", 2) || !strncmp(x, "0X", 2))
            p = strtoull(x, NULL, 16);
        if (!strncmp(y, "0x", 2) || !strncmp(y, "0X", 2))
            q = strtoull(y, NULL, 16);
        return p == q && case_negative(a) == case_negative(b);
    }
    return !strcmp(x, y);
}
static void check_switch(Context *c, Node *n) {
    Type *t = check_expr(c, n->a, NULL);
    Node *item, *v, *other_item, *other;
    int has_default = 0, saved_switch = c->switch_loop;
    if (!type_integer(t) && t->kind != TY_BOOL && t->kind != TY_CHAR && t->kind != TY_ENUM &&
        t->kind != TY_STRING)
        tc_error(c, n->a->loc, "switch requires an integer, char, bool, enum or string, found %s",
                 t->name);
    for (item = n->body; item; item = item->next) {
        if (item->text)
            has_default = 1;
        for (v = item->args; v; v = v->next) {
            require(c, t, check_expr(c, v, t), v->loc);
            if (!case_key(v))
                tc_error(c, v->loc, "case label must be a literal or an enum member");
            if (t->kind == TY_ENUM && v->kind != N_MEMBER)
                tc_error(c, v->loc, "case label for enum %s must be an enum member", t->name);
            for (other_item = n->body; other_item; other_item = other_item->next) {
                for (other = other_item->args; other && other != v; other = other->next)
                    if (case_same(v, other))
                        tc_error(c, v->loc, "duplicate case label");
                if (other == v)
                    break;
            }
        }
    }
    if (t->kind == TY_ENUM && !has_default) {
        Node *m;
        for (m = t->decl->body; m; m = m->next) {
            int covered = 0;
            for (item = n->body; item && !covered; item = item->next)
                for (v = item->args; v && !covered; v = v->next)
                    if (!strcmp(v->resolved->text, m->text))
                        covered = 1;
            if (!covered)
                tc_error(c, n->loc, "switch on %s is not exhaustive: missing %s.%s (add the case or a default)",
                         t->name, t->name, m->name);
        }
    }
    if (has_default || t->kind == TY_ENUM)
        n->flags |= NF_RETURNS;
    for (item = n->body; item; item = item->next) {
        c->switch_loop = c->loop_depth + 1;
        body_scope(c, item->body);
    }
    c->switch_loop = saved_switch;
}
static void check_stmt(Context *c, Node *n) {
    Node *p;
    Type *t;
    if (!n)
        return;
    if (++c->depth > 256)
        tc_error(c, n->loc, "statement nesting limit exceeded");
    switch (n->kind) {
    case N_BLOCK:
        scope_push(c);
        for (p = n->body; p; p = p->next)
            check_stmt(c, p);
        scope_pop(c);
        break;
    case N_VAR:
        valid_type(c, n->decl_type, n->loc);
        if (n->decl_type->kind == TY_AUTO) {
            if (!n->a)
                tc_error(c, n->loc, "var requires an initializer");
            t = check_expr(c, n->a, NULL);
            if (t->kind == TY_VOID || t->kind == TY_NULL)
                tc_error(c, n->loc, "cannot infer variable type from %s", t->name);
        } else {
            t = n->decl_type;
            if (t->kind == TY_VOID)
                tc_error(c, n->loc, "variable cannot have void type");
            if (n->a)
                require(c, t, check_expr(c, n->a, t), n->loc);
        }
        if (n->params) {
            TypeLink *l;
            if (t->kind != TY_TUPLE)
                tc_error(c, n->loc, "destructuring requires multiple return values");
            l = t->items;
            n->decl_type = l->type;
            bind(c, n, l->type, NULL);
            l = l->next;
            for (p = n->params; p; p = p->next) {
                if (!l)
                    tc_error(c, p->loc, "too many destructuring variables");
                p->decl_type = l->type;
                bind(c, p, l->type, NULL);
                l = l->next;
            }
            if (l)
                tc_error(c, n->loc, "not enough destructuring variables");
        } else {
            n->decl_type = t;
            bind(c, n, t, NULL);
        }
        if (n->a)
            n->flags |= n->a->flags & NF_OWNED;
        break;
    case N_EXPR:
        if (n->a)
            check_expr(c, n->a, NULL);
        break;
    case N_RETURN:
        t = c->current_fn->decl_type;
        if (t->kind == TY_AUTO) {
            t = n->a ? check_expr(c, n->a, NULL) : type_primitive(c, TY_VOID);
            c->current_fn->decl_type = t;
        }
        if (t->kind == TY_VOID && n->a)
            tc_error(c, n->loc, "void function cannot return a value");
        if (t->kind != TY_VOID && !n->a)
            tc_error(c, n->loc, "return value required");
        if (n->a) {
            require(c, t, check_expr(c, n->a, t), n->loc);
            if (t->kind == TY_CLOSURE && !(n->a->flags & NF_OWNED))
                tc_error(
                    c, n->loc,
                    "returning a borrowed closure would escape its environment; use owned(lambda)");
        }
        n->flags |= NF_RETURNS;
        break;
    case N_IF:
        condition(c, n->a);
        body_scope(c, n->body);
        body_scope(c, n->b);
        break;
    case N_WHILE:
        condition(c, n->a);
        c->loop_depth++;
        body_scope(c, n->body);
        c->loop_depth--;
        break;
    case N_FOR:
        scope_push(c);
        if (n->a)
            check_stmt(c, n->a);
        if (n->b)
            condition(c, n->b);
        if (n->c)
            check_expr(c, n->c, NULL);
        c->loop_depth++;
        body_scope(c, n->body);
        c->loop_depth--;
        scope_pop(c);
        break;
    case N_RANGE: {
        Node *index, *value;
        t = check_expr(c, n->a, NULL);
        if (!collection(t))
            tc_error(c, n->loc, "range requires array, slice or string");
        scope_push(c);
        index = node(c, N_VAR, n->loc);
        index->name = n->name;
        if (n->text) {
            bind(c, index, type_primitive(c, TY_U64), NULL);
            n->b = index;
            value = node(c, N_VAR, n->loc);
            value->name = n->text;
        } else
            value = index;
        bind(c, value, t->kind == TY_STRING ? type_primitive(c, TY_CHAR) : element(t), NULL);
        n->c = value;
        c->loop_depth++;
        body_scope(c, n->body);
        c->loop_depth--;
        scope_pop(c);
        break;
    }
    case N_BREAK:
    case N_CONTINUE:
        if (n->kind == N_BREAK && c->switch_loop && c->loop_depth == c->switch_loop - 1)
            tc_error(c, n->loc,
                     "break is not allowed directly in a switch case; cases never fall through");
        if (!c->loop_depth)
            tc_error(c, n->loc, "loop control used outside a loop");
        break;
    case N_SWITCH:
        check_switch(c, n);
        break;
    case N_DEFER:
        if (n->body)
            tc_error(c, n->loc, "defer currently requires a single expression");
        check_expr(c, n->a, NULL);
        break;
    case N_DELETE:
        t = check_expr(c, n->a, NULL);
        if (t->kind != TY_PTR)
            tc_error(c, n->loc, "delete requires a pointer");
        break;
    default:
        tc_error(c, n->loc, "invalid statement");
    }
    c->depth--;
}
static int returns(Node *n) {
    Node *p;
    if (!n)
        return 0;
    if (n->kind == N_RETURN)
        return 1;
    if (n->kind == N_BLOCK) {
        for (p = n->body; p; p = p->next)
            if (returns(p))
                return 1;
    }
    if (n->kind == N_IF)
        return returns(n->body) && returns(n->b);
    if (n->kind == N_SWITCH) {
        if (!(n->flags & NF_RETURNS))
            return 0;
        for (p = n->body; p; p = p->next)
            if (!returns(p->body))
                return 0;
        return 1;
    }
    return 0;
}
static void check_function(Context *c, Node *n) {
    Node *p;
    if ((n->flags & NF_ASYNC) && !n->body)
        tc_error(c, n->loc, "async function requires a body");
    if ((n->flags & NF_ASYNC) && (n->flags & NF_STDCALL))
        tc_error(c, n->loc, "async functions cannot use a C calling convention");
    if (!n->body)
        return;
    c->current_fn = n;
    c->current_class = n->owner;
    scope_push(c);
    for (p = n->params; p; p = p->next) {
        if (!p->decl_type || p->decl_type->kind == TY_VOID)
            tc_error(c, p->loc, "invalid parameter type");
        bind(c, p, p->decl_type, NULL);
    }
    check_stmt(c, n->body);
    if (n->decl_type->kind != TY_VOID && !returns(n->body) && strcmp(n->name, "main"))
        tc_error(c, n->loc, "not all paths return a value");
    scope_pop(c);
    c->current_fn = NULL;
    c->current_class = NULL;
}
void analyze(Context *c) {
    Node *n, *m, *p;
    scope_push(c);
    c->global = c->scope;
    for (n = c->program->body; n; n = n->next) {
        if (n->kind == N_EXTENSION) {
            Node **tail;
            if (!n->type->decl)
                tc_error(c, n->loc, "extension type does not exist");
            tail = &n->type->decl->body;
            while (*tail)
                tail = &(*tail)->next;
            for (m = n->body; m; m = m->next) {
                if (m->kind != N_FUNCTION && m->kind != N_PROPERTY)
                    tc_error(c, m->loc, "extension cannot add stored fields");
                m->owner = n->type->decl;
            }
            *tail = n->body;
            n->body = NULL;
        }
    }
    for (n = c->program->body; n; n = n->next) {
        if (n->kind == N_FUNCTION) {
            if (n->args)
                continue;
            if ((n->flags & NF_TEST) &&
                (n->params || !n->body || (n->flags & NF_ASYNC) ||
                 (n->decl_type->kind != TY_VOID && n->decl_type->kind != TY_I32)))
                tc_error(
                    c, n->loc,
                    "@test requires a synchronous void/int function with no parameters and a body");
            valid_type(c, n->decl_type, n->loc);
            for (p = n->params; p; p = p->next)
                if (p->decl_type)
                    valid_type(c, p->decl_type, p->loc);
            n->type = function_type(c, n);
            bind(c, n, n->type,
                 (n->flags & NF_EXTERN) || !strcmp(n->name, "main")
                     ? n->name
                     : tc_format(c, "tc_f_%d_%s", n->id, n->name));
            if (!strcmp(n->name, "main") && (n->decl_type->kind != TY_I32 || n->params))
                tc_error(c, n->loc, "main must have signature int main()");
        } else if (n->kind == N_ENUM) {
            for (m = n->body; m; m = m->next) {
                Node *other;
                m->type = n->type;
                for (other = n->body; other != m; other = other->next)
                    if (!strcmp(m->name, other->name))
                        tc_error(c, m->loc, "duplicate enum value '%s'", m->name);
            }
        } else if (n->kind == N_CLASS || n->kind == N_INTERFACE) {
            if (n->params)
                continue;
            for (m = n->body; m; m = m->next) {
                Node *other;
                valid_type(c, m->decl_type, m->loc);
                for (other = n->body; other != m; other = other->next)
                    if (!strcmp(m->name, other->name))
                        tc_error(c, m->loc, "duplicate member '%s'", m->name);
                if (m->kind == N_FUNCTION) {
                    m->type = function_type(c, m);
                    m->cname = tc_format(c, "%s_%s", n->type->cname, m->name);
                    for (p = m->params; p; p = p->next)
                        valid_type(c, p->decl_type, p->loc);
                }
            }
        }
    }
    for (n = c->program->body; n; n = n->next)
        if (n->kind == N_VAR) {
            c->active_file = n->loc.file;
            check_stmt(c, n);
        }
    for (n = c->program->body; n; n = n->next) {
        if (n->kind == N_FUNCTION && !n->args)
            check_function(c, n);
        else if (n->kind == N_CLASS && !n->params)
            for (m = n->body; m; m = m->next) {
                if (m->kind == N_FUNCTION)
                    check_function(c, m);
                if (m->kind == N_PROPERTY && m->a) {
                    c->current_class = n;
                    require(c, m->decl_type, check_expr(c, m->a, m->decl_type), m->loc);
                    c->current_class = NULL;
                }
            }
    }
}
