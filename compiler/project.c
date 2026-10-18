/* Native tiny.project reader. Paths are relative to the manifest, never shell
   fragments. Repeated keys describe lists; all input is validated before build. */
#include "tiny.h"
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

typedef struct Project {
    const char *entry, *output, *cc;
    const char *tests[32];
    int test_count;
    const char *base;
} Project;

static const char *project_path(Context *c, Project *project, const char *value) {
    if (value[0] == '/' || value[0] == '\\' ||
        (isalpha((unsigned char)value[0]) && value[1] == ':'))
        return value;
    return tc_format(c, "%s/%s", project->base, value);
}

static const char *setting(Context *c, Loc loc, const char *p) {
    int quoted = 0;
    /* Allocate the scratch space in the arena so diagnostics cannot leak it. */
    char *value = (char *)tc_alloc(c, strlen(p) + 1);
    size_t size = 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '"') { quoted = 1; p++; }
    if (quoted) {
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++;
                if (*p != '\\' && *p != '"')
                    tc_error(c, loc, "unsupported escape in project value");
            }
            value[size++] = *p++;
        }
        if (*p != '"') tc_error(c, loc, "unterminated project value");
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p && *p != '#') tc_error(c, loc, "unexpected text after project value");
    } else {
        while (*p && *p != '#') value[size++] = *p++;
        while (size && isspace((unsigned char)value[size - 1])) size--;
    }
    if (!size) tc_error(c, loc, "project value must not be empty");
    value[size] = 0;
    return value;
}

static void parent_directories(Context *c, const char *path, Loc loc) {
    char *copy = tc_str(c, path), *p;
    for (p = copy + 1; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
        /* Skip the separator in a Windows drive prefix. */
        if (p == copy + 2 && copy[1] == ':') continue;
        {
            char separator = *p;
            int status;
            *p = 0;
#ifdef _WIN32
            status = tc_mkdir(copy);
#else
            status = mkdir(copy, 0777);
#endif
            if (status && errno != EEXIST)
                tc_error(c, loc, "cannot create output directory '%s'", copy);
            *p = separator;
        }
    }
}

static void parse_project(Context *c, const char *path, const char *source, Project *project) {
    tc_source(c, path, source);
    const char *cursor = source, *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    int line = 1;
    if (back && (!slash || back > slash)) slash = back;
    project->base = slash ? tc_strn(c, path, (size_t)(slash - path)) : ".";
    if (!*project->base) project->base = "/";
    while (*cursor) {
        const char *end = strchr(cursor, '\n'), *value, *key;
        char *text, *equal, *tail;
        volatile Loc loc = {path, line++, 1};
        size_t size = end ? (size_t)(end - cursor) : strlen(cursor);
        text = tc_strn(c, cursor, size);
        cursor += size + (end != NULL);
        while (*text && isspace((unsigned char)*text)) text++;
        if (!*text || *text == '#') continue;
        equal = strchr(text, '=');
        if (!equal) tc_error(c, loc, "expected key = value in project manifest");
        tail = equal;
        while (tail > text && isspace((unsigned char)tail[-1])) tail--;
        key = tc_strn(c, text, (size_t)(tail - text));
        value = setting(c, loc, equal + 1);
        if (!strcmp(key, "entry") || !strcmp(key, "output") || !strcmp(key, "cc")) {
            const char **target = !strcmp(key, "entry") ? &project->entry
                                : !strcmp(key, "output") ? &project->output : &project->cc;
            if (*target) tc_error(c, loc, "duplicate project key '%s'", key);
            *target = !strcmp(key, "cc") ? value : project_path(c, project, value);
            /* Compiler names use PATH; explicit compiler paths use the project base. */
            if (!strcmp(key, "cc") && (strchr(value, '/') || strchr(value, '\\')))
                *target = project_path(c, project, value);
        } else if (!strcmp(key, "test")) {
            if (project->test_count == 32) tc_error(c, loc, "at most 32 test entries are supported");
            project->tests[project->test_count++] = project_path(c, project, value);
        } else if (!strcmp(key, "module_path")) {
            if (module_path_option(project_path(c, project, value)))
                tc_error(c, loc, "at most 32 module paths are supported");
        } else {
            int kind = !strcmp(key, "c_source") ? 0 : !strcmp(key, "include") ? 1
                     : !strcmp(key, "library") ? 2 : !strcmp(key, "library_path") ? 3 : -1;
            if (kind < 0) tc_error(c, loc, "unknown project key '%s'", key);
            if (backend_option(kind, kind == 2 ? value : project_path(c, project, value)))
                tc_error(c, loc, "at most 32 entries per compiler option are supported");
        }
    }
}

int tc_project(const char *path, const char *output, int mode, int bounds,
               const char *root, int argc, char **argv, const char *cc) {
    Context *c = (Context *)calloc(1, sizeof(Context));
    char *source = read_file(path);
    volatile int result = 2;
    if (!c) {
        fprintf(stderr, "tiny: cannot read project manifest '%s'\n", path);
        free(c); free(source);
        return 2;
    }
    c->diagnostic_json = tc_json_mode();
    if (setjmp(c->failure) == 0) {
        Project project = {0};
        if (!source) { Loc loc = {path, 1, 1}; tc_error(c, loc, "cannot read project manifest"); }
        parse_project(c, path, source, &project);
        if (mode == 8) {
            int i;
            if (!project.test_count) {
                Loc loc = {path, 1, 1};
                tc_error(c, loc, "project has no test entries");
            }
            result = 0;
            for (i = 0; i < project.test_count; i++) {
                int status;
                printf("Project test: %s\n", project.tests[i]);
                fflush(stdout);
                status = tc_compile(project.tests[i], NULL, mode, bounds, root, 0, NULL,
                                    cc ? cc : project.cc);
                if (status && !result) result = status;
            }
        } else {
            /* The manifest names an executable, not an emit-c/emit-asm target. */
            const char *target = output ? output : mode == 6 ? project.output : NULL;
            Loc loc = {path, 1, 1};
            if (!project.entry) tc_error(c, loc, "project requires an entry source");
            if (!target && mode == 6)
                target = project_path(c, &project,
#ifdef _WIN32
                                      "build/app.exe"
#else
                                      "build/app"
#endif
                );
            if (target && mode == 6) parent_directories(c, target, loc);
            if (argc) argv[0] = (char *)project.entry;
            result = tc_compile(project.entry, target, mode, bounds, root, argc, argv,
                                cc ? cc : project.cc);
        }
    }
    tc_free(c); free(c); free(source);
    return result;
}
