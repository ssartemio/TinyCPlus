#include "tiny.h"

typedef struct Substitution {
    const char *name;
    Type *type;
    struct Substitution *next;
} Substitution;
static Type *sub_type(Context *c, Type *t, Substitution *map) {
    Substitution *s;
    TypeLink *items = NULL, **tail = &items, *it;
    if (!t)
        return NULL;
    if (t->kind == TY_NAMED && !t->items)
        for (s = map; s; s = s->next)
            if (!strcmp(s->name, t->name))
                return s->type;
    if (t->kind == TY_PTR || t->kind == TY_ARRAY || t->kind == TY_SLICE)
        return type_derive(c, t->kind, sub_type(c, t->base, map), t->count);
    if (t->items) {
        for (it = t->items; it; it = it->next) {
            TypeLink *l = (TypeLink *)tc_alloc(c, sizeof(TypeLink));
            l->type = sub_type(c, it->type, map);
            *tail = l;
            tail = &l->next;
        }
        if (t->kind == TY_TUPLE)
            return type_tuple(c, items);
        if (t->kind == TY_NAMED)
            return type_generic(c, t->name, items);
    }
    return t;
}
static Node *clone(Context *c, Node *n, Substitution *map) {
    Node *p;
    if (!n)
        return NULL;
    p = node(c, n->kind, n->loc);
    *p = *n;
    p->id = ++c->next_id;
    p->sym = NULL;
    p->cname = NULL;
    p->resolved = NULL;
    p->type = sub_type(c, n->type, map);
    p->decl_type = sub_type(c, n->decl_type, map);
    p->a = clone(c, n->a, map);
    p->b = clone(c, n->b, map);
    p->c = clone(c, n->c, map);
    p->body = clone(c, n->body, map);
    p->params = clone(c, n->params, map);
    p->args = clone(c, n->args, map);
    p->next = clone(c, n->next, map);
    return p;
}
static int concrete(Type *t) {
    TypeLink *i;
    if (t->kind == TY_NAMED && !t->decl && !t->items)
        return 0;
    if (t->base && !concrete(t->base))
        return 0;
    for (i = t->items; i; i = i->next)
        if (!concrete(i->type))
            return 0;
    return 1;
}
static Node *template_for(Context *c, Type *t) {
    Node *n;
    for (n = c->program->body; n; n = n->next)
        if (n->name && !strcmp(n->name, t->name) &&
            ((n->kind == N_CLASS && n->params) || (n->kind == N_FUNCTION && n->args)))
            return n;
    return NULL;
}
void expand_generics(Context *c) {
    int changed = 1, instances = 0;
    while (changed) {
        Type *t;
        changed = 0;
        for (t = c->types; t; t = t->next) {
            Node *src, *dst, *formals, *m, **tail;
            TypeLink *actuals;
            Substitution *map = NULL;
            if (t->kind != TY_NAMED || t->alias || !t->items || t->decl || !concrete(t))
                continue;
            src = template_for(c, t);
            if (!src)
                continue;
            if (++instances > 512)
                tc_error(c, src->loc, "generic instantiation limit (512) exceeded");
            formals = src->kind == N_CLASS ? src->params : src->args;
            actuals = t->items;
            while (formals && actuals) {
                Substitution *s = (Substitution *)tc_alloc(c, sizeof(Substitution));
                s->name = formals->name;
                s->type = actuals->type;
                s->next = map;
                map = s;
                formals = formals->next;
                actuals = actuals->next;
            }
            if (formals || actuals)
                tc_error(c, src->loc, "wrong number of type arguments for '%s'", src->name);
            /* Clone one declaration, not its sibling chain. */
            {
                Node shallow = *src;
                shallow.next = NULL;
                dst = clone(c, &shallow, map);
            }
            dst->name = tc_format(c, "%s_inst_%d", src->name, t->id);
            if (src->kind == N_CLASS) {
                dst->type = t;
                dst->params = NULL;
                t->decl = dst;
                for (m = dst->body; m; m = m->next)
                    m->owner = dst;
            } else {
                dst->args = NULL;
                t->decl = dst;
            }
            tail = &c->program->body;
            while (*tail)
                tail = &(*tail)->next;
            *tail = dst;
            changed = 1;
        }
    }
}
