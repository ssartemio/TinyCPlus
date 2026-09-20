#include "tiny.h"
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define PATH_SEP '\\'
#else
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#define PATH_SEP '/'
#endif

typedef struct TCCState TCCState;
typedef struct TccApi {
    TCCState *(*create)(void);
    void (*destroy)(TCCState *);
    void (*lib_path)(TCCState *, const char *);
    void (*error_func)(TCCState *, void *, void (*)(void *, const char *));
    int (*include_path)(TCCState *, const char *);
    int (*output_type)(TCCState *, int);
    int (*compile)(TCCState *, const char *);
    int (*run)(TCCState *, int, char **);
    int (*output_file)(TCCState *, const char *);
    int (*library)(TCCState *, const char *);
    int (*add_file)(TCCState *, const char *);
    void (*options)(TCCState *, const char *);
    int (*symbol)(TCCState *, const char *, const void *);
    int (*library_path)(TCCState *, const char *);
} TccApi;
static const char *extra_options[4][32];
static int extra_counts[4];
int backend_option(int kind, const char *value) {
    if (kind < 0 || kind > 3 || extra_counts[kind] >= 32)
        return 1;
    extra_options[kind][extra_counts[kind]++] = value;
    return 0;
}
typedef struct ReplSlot {
    char *name;
    void *data;
    size_t size;
    int initialized;
    struct ReplSlot *next;
} ReplSlot;
typedef struct ReplCode {
    TccApi api;
    TCCState *state;
    void *handle;
    struct ReplCode *next;
} ReplCode;
static ReplSlot *repl_slots;
static ReplCode *repl_code;
static size_t repl_bytes;
static int repl_cells;
static ReplSlot *repl_slot_find(const char *name, size_t size) {
    ReplSlot *slot;
    for (slot = repl_slots; slot; slot = slot->next)
        if (!strcmp(slot->name, name)) {
            if (slot->size != size) {
                fputs("tiny repl: variable layout changed; use :reset\n", stderr);
                exit(2);
            }
            return slot;
        }
    if (size > 64u * 1024u * 1024u - repl_bytes) {
        fputs("tiny repl: persistent state exceeds 64 MiB\n", stderr);
        exit(2);
    }
    slot = (ReplSlot *)calloc(1, sizeof(*slot));
    if (!slot)
        abort();
    slot->name = (char *)malloc(strlen(name) + 1);
    slot->data = calloc(1, size ? size : 1);
    if (!slot->name || !slot->data)
        abort();
    strcpy(slot->name, name);
    slot->size = size;
    slot->next = repl_slots;
    repl_slots = slot;
    repl_bytes += size;
    return slot;
}
static void *tc_repl_slot(const char *name, size_t size) {
    return repl_slot_find(name, size)->data;
}
static int tc_repl_initialize(const char *name, size_t size) {
    ReplSlot *slot = repl_slot_find(name, size);
    int fresh = !slot->initialized;
    slot->initialized = 1;
    return fresh;
}
static void tcc_error(void *opaque, const char *msg) {
    (void)opaque;
    fprintf(stderr, "%s\n", msg);
}
#ifdef _WIN32
static void win_arg(Buffer *b, const char *s) {
    size_t slashes = 0;
    buf_add(b, "\"");
    for (; *s; s++) {
        if (*s == '\\') {
            slashes++;
            continue;
        }
        if (*s == '"') {
            while (slashes) {
                buf_add(b, "\\\\");
                slashes--;
            }
            buf_add(b, "\\\"");
        } else {
            while (slashes) {
                buf_add(b, "\\");
                slashes--;
            }
            buf_printf(b, "%c", *s);
        }
    }
    while (slashes) {
        buf_add(b, "\\\\");
        slashes--;
    }
    buf_add(b, "\"");
}
#endif
int tc_process(char **args) {
#ifdef _WIN32
    Buffer command = {0};
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code;
    int i;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    for (i = 0; args[i]; i++) {
        if (i)
            buf_add(&command, " ");
        win_arg(&command, args[i]);
    }
    if (!CreateProcessA(NULL, command.data, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "tiny: cannot start '%s' (Windows error %lu)\n", args[0],
                (unsigned long)GetLastError());
        free(command.data);
        return 127;
    }
    free(command.data);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
#else
    pid_t pid = fork();
    int status;
    if (pid < 0)
        return 127;
    if (pid == 0) {
        execvp(args[0], args);
        perror(args[0]);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0)
        return 127;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
}
static int load_tcc(Context *c, const char *root, TccApi *api, void **handle) {
    const char *path;
#ifdef _WIN32
    HMODULE h;
    path = tc_format(c, "%s/third_party/tcc/libtcc.dll", root);
    h = LoadLibraryA(path);
    if (!h)
        return 0;
#define TC_LOAD(field, name)                                                                       \
    do {                                                                                           \
        FARPROC p = GetProcAddress(h, name);                                                       \
        memcpy(&api->field, &p, sizeof(api->field));                                               \
        if (!api->field) {                                                                         \
            FreeLibrary(h);                                                                        \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)
#else
    void *h;
#ifdef __APPLE__
#define TC_SHARED_LIBRARY "libtcc.dylib"
#else
#define TC_SHARED_LIBRARY "libtcc.so"
#endif
    path = getenv("LIBTCC_PATH");
    if (!path)
        path = tc_format(c, "%s/third_party/tcc/" TC_SHARED_LIBRARY, root);
    h = dlopen(path, RTLD_NOW);
    if (!h)
        h = dlopen(TC_SHARED_LIBRARY, RTLD_NOW);
    if (!h)
        return 0;
#define TC_LOAD(field, name)                                                                       \
    do {                                                                                           \
        void *p = dlsym(h, name);                                                                  \
        memcpy(&api->field, &p, sizeof(api->field));                                               \
        if (!api->field) {                                                                         \
            dlclose(h);                                                                            \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)
#endif
    TC_LOAD(create, "tcc_new");
    TC_LOAD(destroy, "tcc_delete");
    TC_LOAD(lib_path, "tcc_set_lib_path");
    TC_LOAD(error_func, "tcc_set_error_func");
    TC_LOAD(include_path, "tcc_add_include_path");
    TC_LOAD(output_type, "tcc_set_output_type");
    TC_LOAD(compile, "tcc_compile_string");
    TC_LOAD(run, "tcc_run");
    TC_LOAD(output_file, "tcc_output_file");
    TC_LOAD(library, "tcc_add_library");
    TC_LOAD(add_file, "tcc_add_file");
    TC_LOAD(options, "tcc_set_options");
    TC_LOAD(symbol, "tcc_add_symbol");
    TC_LOAD(library_path, "tcc_add_library_path");
#undef TC_LOAD
    *handle = (void *)h;
    return 1;
}
static void unload_tcc(void *h) {
#ifdef _WIN32
    FreeLibrary((HMODULE)h);
#else
    dlclose(h);
#endif
}
void tc_repl_clear(void) {
    while (repl_code) {
        ReplCode *next = repl_code->next;
        repl_code->api.destroy(repl_code->state);
        unload_tcc(repl_code->handle);
        free(repl_code);
        repl_code = next;
    }
    while (repl_slots) {
        ReplSlot *next = repl_slots->next;
        free(repl_slots->name);
        free(repl_slots->data);
        free(repl_slots);
        repl_slots = next;
    }
    repl_bytes = 0;
    repl_cells = 0;
}
static const char *temporary(Context *c) {
#ifdef _WIN32
    char dir[MAX_PATH + 1], file[MAX_PATH + 1];
    if (!GetTempPathA(MAX_PATH, dir) || !GetTempFileNameA(dir, "tcp", 0, file))
        return NULL;
    return tc_str(c, file);
#else
    char path[] = "/tmp/tinycplus-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;
    close(fd);
    return tc_str(c, path);
#endif
}
int backend(Context *c, const char *code, const char *root, const char *output, int run,
            int assembly, int argc, char **argv, const char *cc) {
    TccApi api;
    void *handle = NULL;
    int result = 1;
    char *sources[128];
    int source_count = 0, j;
    if (c->repl_mode && repl_cells >= 128) {
        fputs("tiny repl: 128 compiled cells reached; use :reset\n", stderr);
        return 1;
    }
    if (c->uses_grpc) {
        char *manifest = read_file(tc_format(c, "%s/third_party/nghttp2/sources.txt", root)), *p;
        if (!manifest) {
            fputs("tiny: missing nghttp2 sources manifest\n", stderr);
            return 1;
        }
        p = strtok(manifest, "\r\n");
        while (p && source_count < 64) {
            sources[source_count++] = tc_format(c, "%s/third_party/nghttp2/src/%s", root, p);
            p = strtok(NULL, "\r\n");
        }
        free(manifest);
    }
    for (j = 0; j < extra_counts[0]; j++)
        sources[source_count++] = (char *)extra_options[0][j];
    if (!cc && !assembly && load_tcc(c, root, &api, &handle)) {
        TCCState *s = api.create();
        const char *libroot = tc_format(c, "%s/third_party/tcc", root);
        char *default_args[] = {"tiny-program", NULL};
        if (!s) {
            unload_tcc(handle);
            fputs("tiny: cannot create libtcc state\n", stderr);
            return 1;
        }
        api.lib_path(s, libroot);
        api.error_func(s, NULL, tcc_error);
#ifndef _WIN32
        api.options(s, "-D_POSIX_C_SOURCE=200809L");
#endif
        api.include_path(s, tc_format(c, "%s/runtime", root));
        for (j = 0; j < extra_counts[1]; j++)
            api.include_path(s, extra_options[1][j]);
        for (j = 0; j < extra_counts[3]; j++)
            api.library_path(s, extra_options[3][j]);
        if (c->uses_grpc) {
            api.include_path(s, tc_format(c, "%s/third_party/nghttp2/src", root));
            api.include_path(s, tc_format(c, "%s/third_party/nghttp2/src/includes", root));
            api.options(s, "-DNGHTTP2_STATICLIB -DHAVE_CONFIG_H");
        }
        if (api.output_type(s, run ? 1 : 2) >= 0 && api.compile(s, code) >= 0) {
            if (c->repl_mode) {
                api.symbol(s, "tc_repl_slot", (const void *)tc_repl_slot);
                api.symbol(s, "tc_repl_initialize", (const void *)tc_repl_initialize);
            }
            for (j = 0; j < source_count; j++)
                if (api.add_file(s, sources[j]) < 0) {
                    api.destroy(s);
                    unload_tcc(handle);
                    return 1;
                }
            for (j = 0; j < extra_counts[2]; j++)
                if (api.library(s, extra_options[2][j]) < 0) {
                    api.destroy(s);
                    unload_tcc(handle);
                    return 1;
                }
#ifdef _WIN32
            if (c->uses_io)
                api.library(s, "ws2_32");
            if (c->uses_gui) {
                api.library(s, "user32");
                api.library(s, "gdi32");
            }
#endif
#ifndef _WIN32
            api.library(s, "m");
            api.library(s, "pthread");
#endif
            result = run ? api.run(s, argc ? argc : 1, argc ? argv : default_args)
                         : api.output_file(s, output);
            /* A cell can publish function pointers before returning nonzero. */
            if (c->repl_mode) {
                ReplCode *cell = (ReplCode *)malloc(sizeof(*cell));
                if (!cell)
                    abort();
                cell->api = api;
                cell->state = s;
                cell->handle = handle;
                cell->next = repl_code;
                repl_code = cell;
                repl_cells++;
                return result;
            }
        }
        api.destroy(s);
        unload_tcc(handle);
        return result;
    }
    if (c->repl_mode) {
        fputs("tiny repl: libtcc is required\n", stderr);
        return 1;
    }
    {
        const char *tmp = temporary(c), *exe;
        char *args[384];
        int k = 0;
        if (!tmp) {
            fputs("tiny: cannot create temporary file\n", stderr);
            return 1;
        }
        if (!write_file(tmp, code)) {
            remove(tmp);
            return 1;
        }
#ifdef _WIN32
        exe = run ? tc_format(c, "%s.exe", tmp) : output;
#else
        exe = run ? tc_format(c, "%s.out", tmp) : output;
#endif
        if (!cc) {
            if (assembly) {
                fputs("tiny: --emit-asm requires --cc gcc or --cc clang; libtcc emits machine code "
                      "directly\n",
                      stderr);
                remove(tmp);
                return 1;
            }
            cc = getenv("CC");
            if (!cc)
                cc = "cc";
        }
        args[k++] = (char *)cc;
        args[k++] = "-x";
        args[k++] = "c";
        args[k++] = "-std=c11";
#ifndef _WIN32
        args[k++] = "-D_POSIX_C_SOURCE=200809L";
#endif
        if (assembly)
            args[k++] = "-S";
        args[k++] = (char *)tmp;
        args[k++] = "-I";
        args[k++] = tc_format(c, "%s/runtime", root);
        args[k++] = "-o";
        args[k++] = (char *)exe;
        args[k++] = "-x";
        args[k++] = "none";
        for (j = 0; j < extra_counts[1]; j++) {
            args[k++] = "-I";
            args[k++] = (char *)extra_options[1][j];
        }
        for (j = 0; j < extra_counts[3]; j++) {
            args[k++] = "-L";
            args[k++] = (char *)extra_options[3][j];
        }
        if (c->uses_grpc) {
            if (assembly) {
                fputs("tiny: gRPC assembly output requires compiling emitted C and nghttp2 "
                      "separately\n",
                      stderr);
                remove(tmp);
                return 1;
            }
            args[k++] = "-DNGHTTP2_STATICLIB";
            args[k++] = "-DHAVE_CONFIG_H";
            args[k++] = "-I";
            args[k++] = tc_format(c, "%s/third_party/nghttp2/src", root);
            args[k++] = "-I";
            args[k++] = tc_format(c, "%s/third_party/nghttp2/src/includes", root);
        }
        for (j = 0; j < source_count; j++)
            args[k++] = sources[j];
        for (j = 0; j < extra_counts[2]; j++)
            args[k++] = tc_format(c, "-l%s", extra_options[2][j]);
#ifdef _WIN32
        if (c->uses_io)
            args[k++] = "-lws2_32";
        if (c->uses_gui) {
            args[k++] = "-luser32";
            args[k++] = "-lgdi32";
        }
#endif
#ifndef _WIN32
        args[k++] = "-lm";
        args[k++] = "-pthread";
#endif
        args[k] = NULL;
        result = tc_process(args);
        remove(tmp);
        if (result == 0 && run) {
            char **runargs = (char **)tc_alloc(c, (size_t)(argc + 2) * sizeof(char *));
            int i;
            runargs[0] = (char *)exe;
            for (i = 1; i < argc; i++)
                runargs[i] = argv[i];
            runargs[argc ? argc : 1] = NULL;
            result = tc_process(runargs);
            remove(exe);
        }
    }
    return result;
}
