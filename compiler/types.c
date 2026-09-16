#include "tiny.h"
static const char *names[] = {"void", "bool", "i8",    "u8",     "i16",  "u16",    "i32", "u32",
                              "i64",  "u64",  "float", "double", "char", "string", "null"};
static const char *cnames[] = {"void",     "bool",    "int8_t",   "uint8_t",    "int16_t",
                               "uint16_t", "int32_t", "uint32_t", "int64_t",    "uint64_t",
                               "float",    "double",  "char",     "TinyString", "void *"};
static Type *add_type(Context *c, TypeKind kind) {
    Type *t = (Type *)tc_alloc(c, sizeof(Type));
    t->kind = kind;
    t->id = ++c->next_id;
    t->next = c->types;
    c->types = t;
    return t;
}
Type *type_primitive(Context *c, TypeKind k) {
    Type *t = c->builtin[k];
    if (t)
        return t;
    t = add_type(c, k);
    t->name = k <= TY_NULL ? names[k] : "var";
    t->cname = k <= TY_NULL ? cnames[k] : "void";
    c->builtin[k] = t;
    return t;
}
Type *type_named(Context *c, const char *name) {
    Type *t;
    static const struct {
        const char *name;
        TypeKind k;
    } aliases[] = {{"int", TY_I32},    {"uint", TY_U32},  {"byte", TY_U8},
                   {"short", TY_I16},  {"long", TY_I64},  {"ulong", TY_U64},
                   {"Error", TY_I32}, {"StringView", TY_STRING},
                   {NULL, TY_VOID}};
    int i;
    for (i = 0; i <= TY_NULL; i++)
        if (!strcmp(name, names[i]))
            return type_primitive(c, (TypeKind)i);
    if (!strcmp(name, "size_t")) {
        Type *base = type_primitive(c, TY_U64);
        for (t = c->types; t; t = t->next)
            if (t->kind == TY_U64 && !strcmp(t->name, "size_t"))
                return t;
        t = (Type *)tc_alloc(c, sizeof(Type));
        *t = *base;
        t->next = c->types;
        c->types = t;
        t->name = "size_t";
        t->cname = "size_t";
        return t;
    }
    for (i = 0; aliases[i].name; i++)
        if (!strcmp(name, aliases[i].name))
            return type_primitive(c, aliases[i].k);
    if (!strcmp(name, "var"))
        return type_primitive(c, TY_AUTO);
    for (t = c->types; t; t = t->next)
        if ((t->kind == TY_NAMED || t->kind == TY_ENUM) && !t->items && !strcmp(t->name, name))
            return t;
    t = add_type(c, TY_NAMED);
    t->name = name;
    t->cname = tc_format(c, "tc_%s", name);
    return t;
}
Type *type_derive(Context *c, TypeKind k, Type *base, size_t count) {
    Type *t;
    for (t = c->types; t; t = t->next)
        if (t->kind == k && t->base == base && t->count == count)
            return t;
    t = add_type(c, k);
    t->base = base;
    t->count = count;
    if (k == TY_PTR) {
        t->name = tc_format(c, "%s*", type_name(c, base));
        t->cname = tc_format(c, "%s *", base->cname);
    } else {
        t->name = tc_format(c, k == TY_ARRAY ? "%s[%zu]" : "Slice<%s>", type_name(c, base), count);
        t->cname = tc_format(c, k == TY_ARRAY ? "tc_array_%d" : "tc_slice_%d", t->id);
    }
    return t;
}
int type_equal(Type *a, Type *b) {
    TypeLink *x, *y;
    if (a == b)
        return 1;
    if (!a || !b || a->kind != b->kind || a->qualified != b->qualified)
        return 0;
    if (a->kind == TY_PTR || a->kind == TY_ARRAY || a->kind == TY_SLICE)
        return a->count == b->count && type_equal(a->base, b->base);
    if (a->kind == TY_ENUM)
        return !strcmp(a->name, b->name);
    if (a->kind == TY_TUPLE || a->kind == TY_FUNC || a->kind == TY_CLOSURE) {
        if (a->callconv != b->callconv)
            return 0;
        if (!type_equal(a->base, b->base))
            return 0;
        for (x = a->items, y = b->items; x && y; x = x->next, y = y->next)
            if (!type_equal(x->type, y->type))
                return 0;
        return !x && !y;
    }
    if (a->kind == TY_NAMED) {
        if (strcmp(a->name, b->name))
            return 0;
        for (x = a->items, y = b->items; x && y; x = x->next, y = y->next)
            if (!type_equal(x->type, y->type))
                return 0;
        return !x && !y;
    }
    return 1;
}
Type *type_tuple(Context *c, TypeLink *items) {
    Type *t, probe;
    memset(&probe, 0, sizeof(probe));
    probe.kind = TY_TUPLE;
    probe.items = items;
    for (t = c->types; t; t = t->next)
        if (type_equal(t, &probe))
            return t;
    t = add_type(c, TY_TUPLE);
    t->items = items;
    t->cname = tc_format(c, "tc_tuple_%d", t->id);
    t->name = t->cname;
    return t;
}
const char *type_name(Context *c, Type *t) {
    (void)c;
    return t ? t->name : "<unknown>";
}
Type *type_generic(Context *c, const char *name, TypeLink *items) {
    Type *t;
    TypeLink *a, *b;
    for (t = c->types; t; t = t->next) {
        if (t->kind != TY_NAMED || !t->items || strcmp(t->name, name))
            continue;
        for (a = t->items, b = items; a && b && type_equal(a->type, b->type);
             a = a->next, b = b->next) {
        }
        if (!a && !b)
            return t;
    }
    t = add_type(c, TY_NAMED);
    t->name = name;
    t->items = items;
    t->cname = tc_format(c, "tc_%s_%d", name, t->id);
    return t;
}
Type *type_const(Context *c, Type *base) {
    Type *t;
    if (base->qualified)
        return base;
    for (t = c->types; t; t = t->next)
        if (t->qualified && t->kind == base->kind && t->base == base->base &&
            !strcmp(t->name, tc_format(c, "const %s", base->name)))
            return t;
    t = add_type(c, base->kind);
    {
        int id = t->id;
        Type *next = t->next;
        *t = *base;
        t->id = id;
        t->next = next;
    }
    t->qualified = 1;
    t->name = tc_format(c, "const %s", base->name);
    t->cname = tc_format(c, "const %s", base->cname);
    return t;
}
int type_numeric(Type *t) {
    return t &&
           (t->kind == TY_ENUM || t->kind == TY_BOOL || (t->kind >= TY_I8 && t->kind <= TY_CHAR));
}
int type_integer(Type *t) {
    return type_numeric(t) && t->kind != TY_FLOAT && t->kind != TY_DOUBLE;
}
