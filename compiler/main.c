/*
 * Interfaz de línea de comandos y ciclo de compilación.
 * tc_compile crea un Context por unidad, captura diagnósticos con setjmp,
 * ejecuta las fases solicitadas y libera la arena incluso tras un error.
 */
#include "tiny.h"
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

static int ownership_check;
static int json_diagnostics, test_list, test_isolate, explain_build;
static const char *test_filter;
static void usage(FILE *f) {
    fputs("TinyC+ development compiler\n"
          "Usage: tiny <run|build|check|test|fmt|doc> source.tc [options] [-- args]\n"
          "       tiny repl\n"
          "       tiny <build|run|check|test> [--project tiny.project]\n"
          "       tinyc [options] source.tc\n"
          "Options:\n"
          "  --emit-c             Print readable generated C\n"
          "  --emit-ast           Print syntax tree\n"
          "  --emit-typed-ast     Export analyzed AST as JSON schema 1\n"
          "  --json               Machine-readable check diagnostics (NDJSON)\n"
          "  --ownership-check    Diagnose direct copies and local resource misuse\n"
          "  --check              Non-mutating formatter check (fmt only)\n"
          "  --list / --filter S  List/filter tests by qualified-name substring\n"
          "  --isolate            Run each test in a separate process\n"
          "  --explain            Print effective backend/link options\n"
          "  --gui-backend NAME   headless, x11 (Linux), cocoa (macOS)\n"
          "  doctor / capabilities  Probe toolchain / describe capabilities\n"
          "  --emit-tokens        Print lexer tokens\n"
          "  --emit-asm           Generate assembler with --cc gcc/clang\n"
          "  --cc PATH            Use an external C compiler instead of libtcc\n"
          "  --c-source PATH      Compile and link a C source or object file\n"
          "  -I PATH / -L PATH    Add C include / library search paths\n"
          "  -l NAME              Link a C library\n"
          "  --no-bounds-check    Disable bounds checks (unsafe)\n"
          "  -o PATH              Write generated C, assembler or executable\n"
          "  --home PATH          Set TinyC+ installation root\n"
          "  --project PATH       Load a native project manifest (default: tiny.project)\n"
          "  --module-path PATH   Add a TinyC+ module search directory\n"
          "  --version            Print compiler version\n",
          f);
}
int tc_json_mode(void) { return json_diagnostics; }
int test_selected(Context *c, Node *n) {
    if (n->kind != N_FUNCTION || !(n->flags & NF_TEST)) return 0;
    if (c->test_function) return c->test_function == n;
    return !c->test_filter || strstr(n->name, c->test_filter) ||
        strstr(tc_format(c, "%s.%s", module_name(c, n->loc.file), n->name), c->test_filter);
}
int tc_compile(const char *path, const char *output, int mode, int bounds, const char *root,
               int argc, char **argv, const char *cc) {
    Context *c = (Context *)calloc(1, sizeof(Context));
    char *src;
    volatile int result = 1;
    if (!c)
        return 2;
    src = read_file(path);
    if (!src) {
        if (json_diagnostics) {
            fputs("{\"schemaVersion\":1,\"ok\":false,\"diagnostics\":[{\"severity\":\"error\",\"file\":", stdout);
            tc_json_string(stdout, path);
            fputs(",\"line\":1,\"column\":1,\"message\":\"cannot read source (maximum size 32 MiB)\"}]}\n", stdout);
        } else fprintf(stderr, "tiny: cannot read '%s' (maximum source size 32 MiB)\n", path);
        free(c);
        return 1;
    }
    c->diagnostic_json = json_diagnostics;
    c->test_filter = test_filter; c->test_isolate = test_isolate; c->explain = explain_build;
    c->bounds = bounds;
    c->test_mode = mode == 8;
    c->repl_mode = mode == 9;
    if (setjmp(c->failure) == 0) {
        char *code;
        const char *filename = tc_str(c, path);
        lex(c, filename, src);
        if (mode == 3) {
            dump_tokens(c, stdout);
            result = 0;
        } else {
            if (mode == 4)
                parse(c);
            else
                load_program(c, path, root);
            if (mode == 4) {
                dump_ast(c->program, stdout, 0);
                result = 0;
            } else {
                expand_generics(c);
                analyze(c);
                if (ownership_check) check_local_ownership(c);
                if (mode == 1) {
                    if (json_diagnostics) {
                        fputs("{\"schemaVersion\":1,\"ok\":true,\"file\":", stdout);
                        tc_json_string(stdout, filename);
                        fputs(",\"diagnostics\":[]}\n", stdout);
                    }
                    result = 0;
                } else if (mode == 13) {
                    FILE *out = output ? tc_fopen(output, "wb") : stdout;
                    if (out) { dump_typed_ast(c->program, out); result = 0; if (output) fclose(out); }
                } else if (mode == 8 && test_list) {
                    Node *test;
                    int found = 0;
                    result = 0;
                    for (test = c->program->body; test; test = test->next)
                        if (test_selected(c, test)) {
                            found++;
                            if (test_list) printf("%s.%s\n", module_name(c, test->loc.file), test->name);
                            else {
                                c->test_function = test;
                                code = generate_c(c);
                                if (backend(c, code, root, output, 1, 0, argc, argv, cc)) result = 1;
                                c->test_function = NULL;
                            }
                        }
                    if (!found) { fputs("No @test functions matched.\n", stderr); result = 1; }
                } else {
                    code = generate_c(c);
                    if (mode == 2) {
                        if (output)
                            result = write_file(output, code) ? 0 : 1;
                        else {
                            fputs(code, stdout);
                            result = 0;
                        }
                    } else
                        result =
                            backend(c, code, root,
                                    output      ? output
                                    : mode == 7 ? "out.s"
                                                :
#ifdef _WIN32
                                                "a.exe",
#else
                                                "a.out",
#endif
                                    mode == 5 || mode == 8 || mode == 9, mode == 7, argc, argv, cc);
                }
            }
        }
    }
    tc_free(c);
    free(c);
    free(src);
    return result;
}
static int tiny_main(int argc, char **argv) {
    int format_check = 0;
    const char *input = NULL, *output = NULL, *cc = NULL, *root = getenv("TINY_HOME");
    const char *project = NULL;
    int mode = 6, bounds = 1, i, nargs = 0, batch = 0;
    char **runargs = NULL;
    char homebuf[4096];
    if (!root) {
#ifdef _WIN32
        wchar_t wide[32768];
        DWORD n = GetModuleFileNameW(NULL, wide, 32768);
        char *utf8 = n && n < 32768 ? tc_utf8(wide) : NULL;
        if (!utf8 || strlen(utf8) >= sizeof(homebuf)) {
            free(utf8); fputs("tiny: installation path is too long or invalid\n", stderr); return 2;
        }
        strcpy(homebuf, utf8); free(utf8);
#else
#ifdef __APPLE__
        uint32_t capacity = sizeof(homebuf);
        ssize_t n = _NSGetExecutablePath(homebuf, &capacity) == 0 ? (ssize_t)strlen(homebuf) : -1;
#else
        ssize_t n = readlink("/proc/self/exe", homebuf, sizeof(homebuf) - 1);
#endif
        if (n >= 0)
            homebuf[n] = 0;
        else {
            snprintf(homebuf, sizeof(homebuf), "%s", argv[0]);
        }
#endif
        {
            char *last = NULL, *p;
            for (p = homebuf; *p; p++) {
                if (*p == '\\')
                    *p = '/';
                if (*p == '/')
                    last = p;
            }
            if (last)
                *last = 0;
            last = strrchr(homebuf, '/');
            if (last && !strcmp(last + 1, "bin"))
                *last = 0;
        }
        root = homebuf;
    }
    if (argc == 1) {
        usage(stdout);
        return 0;
    }
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--")) {
            nargs = argc - i;
            runargs = argv + i;
            runargs[0] = input ? (char *)input : "tiny-program";
            break;
        }
        if (!strcmp(a, "run"))
            mode = 5;
        else if (!strcmp(a, "doctor")) mode = 14;
        else if (!strcmp(a, "capabilities")) mode = 15;
        else if (!strcmp(a, "--list")) test_list = 1;
        else if (!strcmp(a, "--isolate")) test_isolate = 1;
        else if (!strcmp(a, "--explain")) explain_build = 1;
        else if (!strcmp(a, "--filter")) {
            if (++i == argc) { fputs("tiny: --filter requires a value\n", stderr); return 2; }
            test_filter = argv[i];
        }
        else if (!strcmp(a, "--batch"))
            batch = 1;
        else if (!strcmp(a, "build"))
            mode = 6;
        else if (!strcmp(a, "check"))
            mode = 1;
        else if (!strcmp(a, "test"))
            mode = 8;
        else if (!strcmp(a, "fmt"))
            mode = 10;
        else if (!strcmp(a, "--check"))
            format_check = 1;
        else if (!strcmp(a, "doc"))
            mode = 11;
        else if (!strcmp(a, "repl"))
            mode = 12;
        else if (!strcmp(a, "--emit-c"))
            mode = 2;
        else if (!strcmp(a, "--emit-tokens"))
            mode = 3;
        else if (!strcmp(a, "--ownership-check"))
            ownership_check = 1;
        else if (!strcmp(a, "--json"))
            json_diagnostics = 1;
        else if (!strcmp(a, "--emit-typed-ast"))
            mode = 13;
        else if (!strcmp(a, "--emit-ast"))
            mode = 4;
        else if (!strcmp(a, "--emit-asm"))
            mode = 7;
        else if (!strcmp(a, "--no-bounds-check"))
            bounds = 0;
        else if (!strcmp(a, "--c-source") || !strcmp(a, "-I") || !strcmp(a, "-l") ||
                 !strcmp(a, "-L")) {
            int kind = !strcmp(a, "--c-source") ? 0
                       : !strcmp(a, "-I")       ? 1
                       : !strcmp(a, "-l")       ? 2
                                                : 3;
            if (++i == argc || backend_option(kind, argv[i])) {
                fprintf(stderr, "tiny: %s needs a value (at most 32 per option)\n", a);
                return 2;
            }
        } else if (!strcmp(a, "--gui-backend")) {
            if (++i == argc || backend_gui_backend(argv[i])) return 2;
        } else if (!strcmp(a, "--module-path")) {
            if (++i == argc || module_path_option(argv[i])) {
                fputs("tiny: --module-path needs a value (at most 32 paths)\n", stderr);
                return 2;
            }
        } else if (!strcmp(a, "-o") || !strcmp(a, "--cc") || !strcmp(a, "--home") ||
                   !strcmp(a, "--project")) {
            if (++i == argc) {
                fprintf(stderr, "tiny: %s requires an argument\n", a);
                return 2;
            }
            if (!strcmp(a, "-o"))
                output = argv[i];
            else if (!strcmp(a, "--cc"))
                cc = argv[i];
            else if (!strcmp(a, "--project"))
                project = argv[i];
            else
                root = argv[i];
        } else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            usage(stdout);
            return 0;
        } else if (!strcmp(a, "--version")) {
            puts("TinyC+ 1.0.0-rc.1 (C11 frontend; libtcc backend)");
            return 0;
        } else if (a[0] == '-') {
            fprintf(stderr, "tiny: unknown option '%s'\n", a);
            return 2;
        } else if (!input)
            input = a;
        else {
            fprintf(stderr, "tiny: unexpected argument '%s'\n", a);
            return 2;
        }
    }
    if ((test_list || test_isolate || test_filter) && mode != 8) {
        fputs("tiny: test options require test\n", stderr); return 2;
    }
    if (mode == 14 || mode == 15) {
        if (input || project) { fputs("tiny: doctor/capabilities take no source\n", stderr); return 2; }
        if (mode == 15) return backend_capabilities(root, cc);
        else {
            Context doctor = {0};
            int status;
            doctor.test_isolate = 1; doctor.explain = 1;
            status = backend(&doctor, "int main(void) { return 0; }\n", root, NULL, 1, 0, 0, NULL, cc);
            tc_free(&doctor);
            puts(status ? "doctor: toolchain compile/run failed" : "doctor: toolchain compile/run passed");
            return status;
        }
    }
    if (project && (input || batch || mode == 10 || mode == 11 || mode == 12)) {
        fputs("tiny: --project cannot be combined with a source, batch, fmt, doc or repl\n", stderr);
        return 2;
    }
    if (json_diagnostics && mode != 1) {
        fputs("tiny: --json requires check\n", stderr); return 2;
    }
    if (format_check && (mode != 10 || output)) {
        fputs("tiny: --check requires fmt without -o\n", stderr);
        return 2;
    }
    if (mode == 12)
        return tc_repl(root);
    if (project || (!input && !batch && mode != 10 && mode != 11))
        return tc_project(project ? project : "tiny.project", output, mode, bounds, root,
                          nargs, runargs, cc);
    if (!input) {
        fputs("tiny: source file required\n", stderr);
        return 2;
    }
    if (mode == 10)
        return tc_format_file(input, format_check ? NULL : output ? output : input);
    if (mode == 11)
        return tc_document_file(input, output);
    if (batch) {
        FILE *manifest = tc_fopen(input, "rb");
        char entry[8192];
        int index = 0, failed = 0;
        if (!manifest) {
            fprintf(stderr, "tiny: cannot read batch manifest\n");
            return 2;
        }
        while (fgets(entry, sizeof(entry), manifest)) {
            char *path = strchr(entry, '\t'), *end;
            int result, entrymode = atoi(entry);
            if (!path || entrymode < 1 || entrymode > 7) {
                fclose(manifest);
                return 2;
            }
            path++;
            end = strpbrk(path, "\r\n");
            if (end)
                *end = 0;
            printf("@@BEGIN %d\n", index);
            fflush(stdout);
            result = tc_compile(path, NULL, entrymode, bounds, root, 0, NULL, cc);
            printf("@@END %d %d\n", index++, result);
            fflush(stdout);
            if (result)
                failed = 1;
        }
        fclose(manifest);
        return failed;
    }
    return tc_compile(input, output, mode, bounds, root, nargs, runargs, cc);
}

int main(int argc, char **argv) {
#ifdef _WIN32
    int count, i, status;
    typedef wchar_t **(WINAPI *ArgvFunction)(const wchar_t *, int *);
    HMODULE shell = LoadLibraryW(L"shell32.dll");
    FARPROC address = shell ? GetProcAddress(shell, "CommandLineToArgvW") : NULL;
    ArgvFunction split;
    wchar_t **wide;
    memcpy(&split, &address, sizeof(split));
    wide = split ? split(GetCommandLineW(), &count) : NULL;
    if (shell) FreeLibrary(shell);
    char **owned, **arguments;
    if (!wide) return 2;
    owned = (char **)calloc((size_t)count + 1, sizeof(char *));
    arguments = (char **)calloc((size_t)count + 1, sizeof(char *));
    if (!owned || !arguments) { free(owned); free(arguments); LocalFree(wide); return 2; }
    for (i = 0; i < count; i++) {
        owned[i] = arguments[i] = tc_utf8(wide[i]);
        if (!owned[i]) break;
    }
    LocalFree(wide);
    status = i == count ? tiny_main(count, arguments) : 2;
    for (i = 0; i < count; i++) free(owned[i]);
    free(owned); free(arguments);
    (void)argc; (void)argv;
    return status;
#else
    return tiny_main(argc, argv);
#endif
}
