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
        Type *base;
        for (t = c->types; t; t = t->next)
            if (t->kind == TY_U64 && t->name && !strcmp(t->name, "size_t"))
                return t;
        /*
         * size_t keeps u64 semantics in TinyC+, but must retain the host C
         * spelling at ABI boundaries.  Create the canonical u64 exactly as
         * the old alias did, then attach a spelling-only view without
         * consuming another type ID.  It intentionally is not a Type.alias:
         * module canonicalization follows aliases and would erase the ABI spelling.
         * This keeps generated symbol IDs stable while preserving size_t in C.
         */
        base = type_primitive(c, TY_U64);
        t = (Type *)tc_alloc(c, sizeof(Type));
        *t = *base;
        t->name = "size_t";
        t->cname = "size_t";
        t->next = c->types;
        c->types = t;
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
/* --emit-types: the type table in creation order, one line per entry. References to other types
   are positions in that order. It exposes the state the parser builds while it reads a file (ids
   shared with syntax nodes, interning, declarations), which the self-hosted compiler must
   reproduce exactly. */
static long type_position(Type **list, size_t n, const Type *t) {
    size_t i;
    for (i = 0; i < n; i++)
        if (list[i] == t)
            return (long)i;
    return -1;
}
static void print_reference(FILE *out, Type **list, size_t n, const Type *t) {
    if (!t)
        fputc('-', out);
    else
        fprintf(out, "%ld", type_position(list, n, t));
}
void dump_types(Context *c, FILE *out) {
    static const char *kinds[] = {"Void",  "Bool",  "I8",    "U8",      "I16",   "U16",
                                  "I32",   "U32",   "I64",   "U64",     "Float", "Double",
                                  "Char",  "String", "Null", "Ptr",     "Array", "Slice",
                                  "Tuple", "Func",  "Closure", "Named", "Enum",  "Auto"};
    Type *t, **list;
    size_t n = 0, i;
    TypeLink *item;
    for (t = c->types; t; t = t->next)
        n++;
    list = (Type **)malloc((n ? n : 1) * sizeof(Type *));
    if (!list)
        return;
    for (i = n, t = c->types; t; t = t->next)
        list[--i] = t;
    for (i = 0; i < n; i++) {
        t = list[i];
        fprintf(out, "%zu id=%d %s name=%s cname=%s base=", i, t->id, kinds[t->kind],
                t->name ? t->name : "-", t->cname ? t->cname : "-");
        print_reference(out, list, n, t->base);
        fputs(" items=[", out);
        for (item = t->items; item; item = item->next) {
            print_reference(out, list, n, item->type);
            if (item->next)
                fputc(',', out);
        }
        fprintf(out, "] count=%zu qualified=%d callconv=%d decl=", t->count, t->qualified,
                t->callconv);
        if (t->decl)
            fprintf(out, "%s@%d:%d", t->decl->name ? t->decl->name : "-", t->decl->loc.line,
                    t->decl->loc.col);
        else
            fputc('-', out);
        fputs(" alias=", out);
        print_reference(out, list, n, t->alias);
        fputc('\n', out);
    }
    free(list);
}
