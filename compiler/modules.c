/*
 * Carga de fuentes e imports, prelude y calificación de nombres.
 * load_program reúne módulos en un programa y normaliza sus tipos.
 * La detección de tareas incorpora std.concurrent cuando es necesario.
 */
#include "tiny.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
extern char *realpath(const char *, char *);
#endif

static const char *search_paths[32];
static int search_path_count;
int module_path_option(const char *path) {
    if (search_path_count == 32)
        return 1;
    search_paths[search_path_count++] = path;
    return 0;
}
static int module_exists(const char *path) {
    FILE *file = tc_fopen(path, "rb");
    if (!file)
        return 0;
    fclose(file);
    return 1;
}

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
    wchar_t wide[32768], *input = tc_utf16(path);
    DWORD n = input ? GetFullPathNameW(input, 32768, wide, NULL) : 0;
    char *utf8;
    free(input);
    utf8 = n && n < 32768 ? tc_utf8(wide) : NULL;
    if (!utf8 || strlen(utf8) >= sizeof(canonical)) {
        free(utf8);
        Loc l = {path, 1, 1};
        tc_error(c, l, "module path is too long or invalid");
    }
    strcpy(canonical, utf8); free(utf8);
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
                if (!module_exists(import_path)) {
                    int i;
                    for (i = 0; i < search_path_count; i++) {
                        const char *candidate = tc_format(c, "%s/%s.tc", search_paths[i], relative);
                        if (module_exists(candidate)) {
                            import_path = candidate;
                            break;
                        }
                    }
                }
            }
            if (!module_exists(import_path))
                tc_error(c, n->loc, "cannot find module '%s'", n->name);
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
        if (t->kind == TY_NAMED && !t->decl) {
            const char *dot = strrchr(t->name, '.');
            const char *short_name = dot ? dot + 1 : t->name;
            Type *selected = NULL;
            int matches = 0;
            for (actual = c->types; actual; actual = actual->next)
                if (actual->decl && !actual->items && !strcmp(actual->name, short_name)) {
                    const char *module = module_name(c, actual->decl->loc.file);
                    if (dot && (strlen(module) != (size_t)(dot - t->name) ||
                        strncmp(module, t->name, (size_t)(dot - t->name)))) continue;
                    if (!dot && t->module_file && !strcmp(actual->decl->loc.file, t->module_file)) {
                        selected = actual; matches = 1; break;
                    }
                    selected = actual; matches++;
                }
            if (matches > 1) {
                Loc loc = t->origin.file ? t->origin : (Loc){t->module_file, 1, 1};
                tc_error(c, loc, "ambiguous imported type '%s'; qualify it with its module", t->name);
            }
            if (!selected && t->items) {
                Node *function, *match = NULL;
                int functions = 0;
                for (function = c->program->body; function; function = function->next)
                    if (function->kind == N_FUNCTION && function->args && !strcmp(function->name, short_name)) {
                        const char *module = module_name(c, function->loc.file);
                        if (dot && (strlen(module) != (size_t)(dot - t->name) ||
                            strncmp(module, t->name, (size_t)(dot - t->name)))) continue;
                        match = function; functions++;
                        if (!dot && t->module_file && !strcmp(t->module_file, function->loc.file)) { functions = 1; break; }
                    }
                if (functions > 1) tc_error(c, t->origin, "ambiguous imported generic '%s'; qualify it with its module", t->name);
                if (match) {
                    t->name = match->name; t->module_file = match->loc.file;
                    t->cname = tc_format(c, "tc_%s_%d", match->name, t->id);
                    continue;
                }
            }
            if (!selected) {
                if (dot) tc_error(c, t->origin, "unknown qualified type '%s'", t->name);
                continue; /* Type parameters and unknown types are checked later. */
            }
            if (!t->items) t->alias = selected;
            else {
                t->name = selected->name;
                t->module_file = selected->module_file;
                t->cname = tc_format(c, "tc_%s_%d", selected->name, t->id);
            }
        }
    /* Foreign declarations use the C namespace, shared across TinyC+ modules. */
    for (t = c->types; t; t = t->next)
        if (!t->alias && t->decl && !t->items && (t->decl->flags & NF_EXTERN))
            for (actual = t->next; actual; actual = actual->next)
                if (!actual->alias && actual->decl && !actual->items &&
                    (actual->decl->flags & NF_EXTERN) && !strcmp(t->cname, actual->cname)) {
                    Node *left = t->decl->body, *right = actual->decl->body;
                    if (t->kind != actual->kind)
                        tc_error(c, t->decl->loc, "conflicting extern C type '%s'", t->name);
                    t->alias = actual; /* Allows recursive pointers to the same C type. */
                    for (; left && right; left = left->next, right = right->next) {
                        Node *lp, *rp;
                        if (left->kind != right->kind || strcmp(left->name, right->name) ||
                            !type_equal(canonical_type(left->decl_type), canonical_type(right->decl_type)) ||
                            (t->kind == TY_ENUM && strcmp(left->text, right->text)))
                            tc_error(c, t->decl->loc, "conflicting extern C layout for '%s'", t->name);
                        for (lp = left->params, rp = right->params; lp && rp; lp = lp->next, rp = rp->next)
                            if (!type_equal(canonical_type(lp->decl_type), canonical_type(rp->decl_type)))
                                tc_error(c, t->decl->loc, "conflicting extern C signature for '%s'", t->name);
                        if (lp || rp) tc_error(c, t->decl->loc, "conflicting extern C signature for '%s'", t->name);
                    }
                    if (left || right) tc_error(c, t->decl->loc, "conflicting extern C layout for '%s'", t->name);
                    break;
                }
    /* Preserve old C names for unique declarations; disambiguate collisions. */
    for (t = c->types; t; t = t->next)
        if (t->decl && !t->items && !(t->decl->flags & NF_EXTERN)) {
            for (actual = c->types; actual; actual = actual->next)
                if (actual != t && actual->decl && !actual->items && !strcmp(actual->name, t->name)) {
                    Node *value;
                    t->cname = tc_format(c, "tc_%s_m%d", t->name, t->id);
                    if (t->kind == TY_ENUM)
                        for (value = t->decl->body; value; value = value->next)
                            value->cname = tc_format(c, "%s_%s", t->cname, value->name);
                    break;
                }
        }
    for (t = c->types; t; t = t->next)
        canonical_type(t);
    for (t = c->types; t; t = t->next)
        if (t->kind == TY_NAMED && t->items && !t->alias) {
            for (actual = t->next; actual; actual = actual->next)
                if (actual->kind == TY_NAMED && actual->items && !actual->alias &&
                    !strcmp(t->name, actual->name) &&
                    ((!t->module_file && !actual->module_file) ||
                     (t->module_file && actual->module_file && !strcmp(t->module_file, actual->module_file)))) {
                    for (a = t->items, b = actual->items; a && b && type_equal(a->type, b->type);
                         a = a->next, b = b->next) {
                    }
                    if (!a && !b) {
                        t->alias = actual;
                        break;
                    }
                }
        }
    /* Canonicalizing imported children can make structural wrappers equal. */
    for (t = c->types; t; t = t->next)
        if (!t->alias && t->kind != TY_NAMED && t->kind != TY_ENUM &&
            t->kind >= TY_PTR)
            for (actual = t->next; actual; actual = actual->next)
                if (!actual->alias && type_equal(t, actual)) { t->alias = actual; break; }
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
