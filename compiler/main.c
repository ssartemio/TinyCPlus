#include "tiny.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

static void usage(FILE *f) {
    fputs("TinyC+ development compiler\n"
          "Usage: tiny <run|build|check|test|fmt|doc> source.tc [options] [-- args]\n"
          "       tiny repl\n"
          "       tinyc [options] source.tc\n"
          "Options:\n"
          "  --emit-c             Print readable generated C\n"
          "  --emit-ast           Print syntax tree\n"
          "  --emit-tokens        Print lexer tokens\n"
          "  --emit-asm           Generate assembler with --cc gcc/clang\n"
          "  --cc PATH            Use an external C compiler instead of libtcc\n"
          "  --gui-backend NAME   GUI backend: headless, x11 (Linux), cocoa (macOS)\n"
          "  --c-source PATH      Compile and link a C source or object file\n"
          "  -I PATH / -L PATH    Add C include / library search paths\n"
          "  -l NAME              Link a C library\n"
          "  --no-bounds-check    Disable bounds checks (unsafe)\n"
          "  -o PATH              Write generated C, assembler or executable\n"
          "  --home PATH          Set TinyC+ installation root\n"
          "  --version            Print compiler version\n",
          f);
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
        fprintf(stderr, "tiny: cannot read '%s' (maximum source size 32 MiB)\n", path);
        free(c);
        return 1;
    }
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
                if (mode == 1)
                    result = 0;
                else {
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
int main(int argc, char **argv) {
    const char *input = NULL, *output = NULL, *cc = NULL, *root = getenv("TINY_HOME");
    int mode = 6, bounds = 1, i, nargs = 0, batch = 0;
    char **runargs = NULL;
    char homebuf[4096];
    if (!root) {
#ifdef _WIN32
        DWORD n = GetModuleFileNameA(NULL, homebuf, sizeof(homebuf) - 1);
        homebuf[n] = 0;
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
        else if (!strcmp(a, "doc"))
            mode = 11;
        else if (!strcmp(a, "repl"))
            mode = 12;
        else if (!strcmp(a, "--emit-c"))
            mode = 2;
        else if (!strcmp(a, "--emit-tokens"))
            mode = 3;
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
            if (++i == argc || backend_gui_backend(argv[i]))
                return 2;
        } else if (!strcmp(a, "-o") || !strcmp(a, "--cc") || !strcmp(a, "--home")) {
            if (++i == argc) {
                fprintf(stderr, "tiny: %s requires an argument\n", a);
                return 2;
            }
            if (!strcmp(a, "-o"))
                output = argv[i];
            else if (!strcmp(a, "--cc"))
                cc = argv[i];
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
    if (mode == 12)
        return tc_repl(root);
    if (!input) {
        fputs("tiny: source file required\n", stderr);
        return 2;
    }
    if (mode == 10)
        return tc_format_file(input, output ? output : input);
    if (mode == 11)
        return tc_document_file(input, output);
    if (batch) {
        FILE *manifest = fopen(input, "rb");
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
