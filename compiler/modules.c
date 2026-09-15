#include "tiny.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
extern char *realpath(const char *, char *);
#endif

const char *module_name(Context *c, const char *file) {
    Node *m;
    for (m = c->loaded_modules; m; m = m->next)
        if (!strcmp(m->name, file))
            return m->label ? m->label : "";
    return "";
}
static const char *canonical_path(Context *c, const char *path) {
    char canonical[32768], *p;
#ifdef _WIN32
    DWORD n = GetFullPathNameA(path, sizeof(canonical), canonical, NULL);
    if (!n || n >= sizeof(canonical)) {
        Loc l = {path, 1, 1};
        tc_error(c, l, "module path is too long or invalid");
    }
#else
    if (!realpath(path, canonical)) {
        Loc l = {path, 1, 1};
        tc_error(c, l, "cannot resolve module path");
    }
#endif
    for (p = canonical; *p; p++)
        if (*p == '\\')
            *p = '/';
    return tc_str(c, canonical);
}

static Node *load_module(Context *c, const char *path, const char *root) {
    Node *mark, *program, *n, *combined = NULL, **tail = &combined;
    char *source;
    int count = 0;
    path = canonical_path(c, path);
    for (mark = c->loaded_modules; mark; mark = mark->next) {
        count++;
#ifdef _WIN32
        if (!_stricmp(mark->name, path))
            return NULL;
#else
        if (!strcmp(mark->name, path))
            return NULL;
#endif
    }
    if (count >= 256) {
        Loc l = {path, 1, 1};
        tc_error(c, l, "module limit (256) exceeded");
    }
    {
        Loc l = {path, 1, 1};
        mark = node(c, N_MODULE, l);
        mark->name = path;
        mark->next = c->loaded_modules;
        c->loaded_modules = mark;
    }
    source = read_file(path);
    if (!source)
        tc_error(c, mark->loc, "cannot load module '%s'", path);
    {
        const char *saved = tc_str(c, source);
        free(source);
        c->ntokens = 0;
        c->pos = 0;
        lex(c, path, saved);
        program = parse(c);
    }
    for (n = program->body; n; n = n->next)
        if (n->kind == N_MODULE) {
            mark->label = n->name;
            break;
        }
    if (!mark->label) {
        const char *base = strrchr(path, '/'), *dot;
        base = base ? base + 1 : path;
        dot = strrchr(base, '.');
        mark->label = tc_strn(c, base, dot ? (size_t)(dot - base) : strlen(base));
    }
    for (n = program->body; n; n = n->next)
        if (n->kind == N_IMPORT) {
            const char *import_path;
            char *relative = tc_str(c, n->name), *p;
            Node *dep;
            for (p = relative; *p; p++)
                if (*p == '.')
                    *p = '/';
            if (!strncmp(relative, "std/", 4))
                import_path = tc_format(c, "%s/%s.tc", root, relative);
            else {
                const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
                size_t dirlen;
                if (back && (!slash || back > slash))
                    slash = back;
                dirlen = slash ? (size_t)(slash - path) : 0;
                import_path = dirlen ? tc_format(c, "%.*s/%s.tc", (int)dirlen, path, relative)
                                     : tc_format(c, "%s.tc", relative);
            }
            dep = load_module(c, import_path, root);
            if (dep) {
                *tail = dep->body;
                while (*tail)
                    tail = &(*tail)->next;
            }
        }
    *tail = program->body;
    program->body = combined;
    return program;
}
static Type *canonical_type(Type *t) {
    TypeLink *item;
    if (!t)
        return NULL;
    while (t->alias)
        t = t->alias;
    if (t->base)
        t->base = canonical_type(t->base);
    for (item = t->items; item; item = item->next)
        item->type = canonical_type(item->type);
    return t;
}
static void canonical_nodes(Node *n) {
    for (; n; n = n->next) {
        n->type = canonical_type(n->type);
        n->decl_type = canonical_type(n->decl_type);
        canonical_nodes(n->a);
        canonical_nodes(n->b);
        canonical_nodes(n->c);
        canonical_nodes(n->body);
        canonical_nodes(n->params);
        canonical_nodes(n->args);
    }
}
static void qualified_types(Context *c) {
    Type *t, *actual;
    TypeLink *a, *b;
    for (t = c->types; t; t = t->next)
        if (t->kind == TY_NAMED && !t->decl && strchr(t->name, '.')) {
            const char *dot = strrchr(t->name, '.');
            for (actual = c->types; actual; actual = actual->next)
                if (actual->decl && !strcmp(actual->name, dot + 1)) {
                    const char *module = module_name(c, actual->decl->loc.file);
                    if (strlen(module) == (size_t)(dot - t->name) &&
                        !strncmp(module, t->name, (size_t)(dot - t->name)))
                        break;
                }
            if (!actual) {
                Loc l = {c->active_file, 1, 1};
                tc_error(c, l, "unknown qualified type '%s'", t->name);
            }
            if (!t->items)
                t->alias = actual;
            else {
                t->name = actual->name;
                t->cname = tc_format(c, "tc_%s_%d", actual->name, t->id);
            }
        }
    for (t = c->types; t; t = t->next)
        canonical_type(t);
    for (t = c->types; t; t = t->next)
        if (t->kind == TY_NAMED && t->items && !t->alias) {
            for (actual = t->next; actual; actual = actual->next)
                if (actual->kind == TY_NAMED && actual->items && !actual->alias &&
                    !strcmp(t->name, actual->name)) {
                    for (a = t->items, b = actual->items; a && b && type_equal(a->type, b->type);
                         a = a->next, b = b->next) {
                    }
                    if (!a && !b) {
                        t->alias = actual;
                        break;
                    }
                }
        }
    canonical_nodes(c->program);
    for (t = c->types; t; t = t->next) {
        canonical_type(t);
        if (t->kind == TY_PTR)
            t->cname = tc_format(c, "%s *", t->base->cname);
    }
}
static int has_tasks(Node *n) {
    for (; n; n = n->next)
        if ((n->flags & NF_ASYNC) || n->kind == N_AWAIT || n->kind == N_SPAWN || has_tasks(n->a) ||
            has_tasks(n->b) || has_tasks(n->c) || has_tasks(n->body) || has_tasks(n->args))
            return 1;
    return 0;
}
Node *load_program(Context *c, const char *path, const char *root) {
    const char *prelude = tc_format(c, "%s/std/prelude.tc", root);
    Node *base, *user, **tail;
    base = load_module(c, prelude, root);
    user = load_module(c, tc_str(c, path), root);
    if (!user)
        tc_error(c, c->program->loc, "source cannot be the prelude itself");
    tail = &base->body;
    while (*tail)
        tail = &(*tail)->next;
    if (has_tasks(user->body)) {
        Node *concurrent = load_module(c, tc_format(c, "%s/std/concurrent.tc", root), root);
        if (concurrent) {
            *tail = concurrent->body;
            while (*tail)
                tail = &(*tail)->next;
        }
    }
    *tail = user->body;
    c->program = base;
    c->active_file = user->loc.file;
    qualified_types(c);
    return base;
}
