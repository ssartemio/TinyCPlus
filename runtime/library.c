#ifndef TC_LIBRARY_IMPLEMENTATION
#define TC_LIBRARY_IMPLEMENTATION
#include "tiny_runtime.h"
void tc_flush_stdout(void) {
    fflush(stdout);
}
int32_t tc_argument_count(void) {
    return tc_program_argc;
}
TinyString tc_argument(int32_t index) {
    TinyString value = {NULL, 0};
    if (index >= 0 && index < tc_program_argc) {
        value.data = tc_program_argv[index];
        value.length = strlen(value.data);
    }
    return value;
}
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#endif

static char *tc_cstring(TinyString s) {
    char *p;
    if (s.length == SIZE_MAX || (!s.data && s.length) || (s.length && memchr(s.data, 0, s.length)))
        return NULL;
    p = (char *)malloc(s.length + 1);
    if (!p)
        return NULL;
    if (s.length)
        memcpy(p, s.data, s.length);
    p[s.length] = 0;
    return p;
}
TinyString tc_string_view(const char *data, uint64_t length) {
    TinyString s = {data, (size_t)length};
    return s;
}
TinyString tc_string_copy(TinyString source) {
    TinyString s = {NULL, 0};
    char *p;
    if (source.length == SIZE_MAX)
        return s;
    p = (char *)malloc(source.length + 1);
    if (!p)
        return s;
    if (source.length)
        memcpy(p, source.data, source.length);
    p[source.length] = 0;
    s.data = p;
    s.length = source.length;
    return s;
}
void tc_string_free(TinyString value) {
    free((void *)value.data);
}
TinyString tc_string_concat(TinyString a, TinyString b) {
    TinyString s = {NULL, 0};
    char *p;
    if (b.length >= SIZE_MAX - a.length)
        return s;
    p = (char *)malloc(a.length + b.length + 1);
    if (!p)
        return s;
    if (a.length)
        memcpy(p, a.data, a.length);
    if (b.length)
        memcpy(p + a.length, b.data, b.length);
    p[a.length + b.length] = 0;
    s.data = p;
    s.length = a.length + b.length;
    return s;
}
int64_t tc_string_find(TinyString haystack, TinyString needle) {
    size_t i;
    if (needle.length > haystack.length)
        return -1;
    for (i = 0; i <= haystack.length - needle.length; i++)
        if (!needle.length || memcmp(haystack.data + i, needle.data, needle.length) == 0)
            return (int64_t)i;
    return -1;
}
int32_t tc_string_starts(TinyString text, TinyString prefix) {
    return prefix.length <= text.length &&
           (!prefix.length || memcmp(text.data, prefix.data, prefix.length) == 0);
}
int32_t tc_string_ends(TinyString text, TinyString suffix) {
    return suffix.length <= text.length &&
           (!suffix.length ||
            memcmp(text.data + text.length - suffix.length, suffix.data, suffix.length) == 0);
}
int64_t tc_parse_int(TinyString text, int32_t *error) {
    char *s = tc_cstring(text), *end;
    long long result;
    if (!s) {
        *error = 1;
        return 0;
    }
    errno = 0;
    result = strtoll(s, &end, 10);
    *error = errno == ERANGE ? 2 : end == s || *end ? 1 : 0;
    free(s);
    return *error ? 0 : (int64_t)result;
}
TinyString tc_format_int(int64_t value) {
    char buffer[32];
    int n = snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    TinyString s = {buffer, (size_t)n};
    return tc_string_copy(s);
}
TinyString tc_read_line(void) {
    size_t length = 0, capacity = 128;
    char *p = (char *)malloc(capacity);
    int ch;
    TinyString result = {NULL, 0};
    if (!p)
        return result;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (length + 1 == capacity) {
            char *next;
            if (capacity > SIZE_MAX / 2) {
                free(p);
                return result;
            }
            capacity *= 2;
            next = (char *)realloc(p, capacity);
            if (!next) {
                free(p);
                return result;
            }
            p = next;
        }
        p[length++] = (char)ch;
    }
    if (ch == EOF && !length) {
        free(p);
        return result;
    }
    if (length && p[length - 1] == '\r')
        length--;
    p[length] = 0;
    result.data = p;
    result.length = length;
    return result;
}
TinyString tc_file_read_all(TinyString path, int32_t *error) {
    TinyString result = {NULL, 0};
    char *name = tc_cstring(path), *data;
    FILE *f;
    long size;
    *error = 0;
    if (!name) {
        *error = 1;
        return result;
    }
    f = fopen(name, "rb");
    free(name);
    if (!f) {
        *error = errno ? errno : 1;
        return result;
    }
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        *error = 1;
        fclose(f);
        return result;
    }
    data = (char *)malloc((size_t)size + 1);
    if (!data) {
        *error = 2;
        fclose(f);
        return result;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        *error = 1;
        free(data);
        fclose(f);
        return result;
    }
    data[size] = 0;
    result.data = data;
    result.length = (size_t)size;
    fclose(f);
    return result;
}
int32_t tc_file_write_all(TinyString path, TinyString data) {
    char *name = tc_cstring(path);
    FILE *f;
    int error;
    if (!name)
        return 1;
    f = fopen(name, "wb");
    free(name);
    if (!f)
        return errno ? errno : 1;
    error = fwrite(data.data, 1, data.length, f) == data.length ? 0 : 1;
    if (fclose(f))
        error = 1;
    return error;
}
int32_t tc_file_exists(TinyString path) {
    char *name = tc_cstring(path);
    struct stat st;
    int result;
    if (!name)
        return 0;
    result = stat(name, &st) == 0;
    free(name);
    return result;
}
int32_t tc_file_remove(TinyString path) {
    char *name = tc_cstring(path);
    int result;
    if (!name)
        return 1;
    result = remove(name);
    free(name);
    return result ? 1 : 0;
}
void *tc_file_open(TinyString path, TinyString mode) {
    char *p = tc_cstring(path), *m = tc_cstring(mode);
    FILE *f = NULL;
    if (p && m)
        f = fopen(p, m);
    free(p);
    free(m);
    return f;
}
int64_t tc_file_read(void *handle, void *data, uint64_t capacity) {
    FILE *f = (FILE *)handle;
    size_t n;
    if (!f || capacity > SIZE_MAX)
        return -1;
    n = fread(data, 1, (size_t)capacity, f);
    return ferror(f) ? -1 : (int64_t)n;
}
int64_t tc_file_write(void *handle, void *data, uint64_t length) {
    FILE *f = (FILE *)handle;
    size_t n;
    if (!f || length > SIZE_MAX)
        return -1;
    n = fwrite(data, 1, (size_t)length, f);
    return ferror(f) ? -1 : (int64_t)n;
}
int32_t tc_file_close(void *handle) {
    return handle ? fclose((FILE *)handle) : 1;
}
int32_t tc_file_write_atomic(TinyString path, TinyString contents) {
    char *name = tc_cstring(path), *temporary;
    size_t written = 0;
    int error = 0;
    if (!name)
        return 1;
    temporary = (char *)malloc(strlen(name) + 80);
    if (!temporary) {
        free(name);
        return 8;
    }
#ifdef _WIN32
    {
        static LONG counter;
        HANDLE file = INVALID_HANDLE_VALUE;
        int attempt;
        for (attempt = 0; attempt < 32; attempt++) {
            snprintf(temporary, strlen(name) + 80, "%s.tiny-%lu-%ld.tmp", name,
                     (unsigned long)GetCurrentProcessId(), (long)InterlockedIncrement(&counter));
            file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                               NULL);
            if (file != INVALID_HANDLE_VALUE)
                break;
            if (GetLastError() != ERROR_FILE_EXISTS) {
                error = 1;
                break;
            }
        }
        if (file == INVALID_HANDLE_VALUE)
            error = 1;
        else {
            while (written < contents.length) {
                DWORD count = 0, chunk = (DWORD)((contents.length - written) > 1048576
                                                     ? 1048576
                                                     : contents.length - written);
                if (!WriteFile(file, contents.data + written, chunk, &count, NULL) || !count) {
                    error = 1;
                    break;
                }
                written += count;
            }
            if (!error && !FlushFileBuffers(file))
                error = 1;
            if (!CloseHandle(file))
                error = 1;
            if (!error &&
                !MoveFileExA(temporary, name, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                error = 1;
            if (error)
                DeleteFileA(temporary);
        }
    }
#else
    {
        int file;
        struct stat previous;
        snprintf(temporary, strlen(name) + 80, "%s.tiny-XXXXXX", name);
        file = mkstemp(temporary);
        if (file < 0)
            error = 1;
        else {
            if (stat(name, &previous) == 0 && fchmod(file, previous.st_mode & 0777))
                error = 1;
            while (!error && written < contents.length) {
                ssize_t n = write(file, contents.data + written, contents.length - written);
                if (n < 0 && errno == EINTR)
                    continue;
                if (n <= 0) {
                    error = 1;
                    break;
                }
                written += (size_t)n;
            }
            if (!error && fsync(file))
                error = 1;
            if (close(file))
                error = 1;
            if (!error && rename(temporary, name))
                error = 1;
            if (error)
                unlink(temporary);
        }
    }
#endif
    free(temporary);
    free(name);
    return error;
}
int32_t tc_process_command(TinyString command) {
    char *s = tc_cstring(command);
    int result;
    if (!s)
        return -1;
    result = system(s);
    free(s);
    return result;
}
TinyString tc_environment(TinyString key) {
    char *name = tc_cstring(key);
    const char *value;
    TinyString result = {NULL, 0};
    if (!name)
        return result;
    value = getenv(name);
    free(name);
    if (value) {
        result.data = value;
        result.length = strlen(value);
    }
    return result;
}
/* ---- Support for StringBuilder, Console.writeError and Process.spawn ---- */
void tc_bytes_copy(char *destination, TinyString source) {
    if (source.length)
        memcpy(destination, source.data, source.length);
}
/* Shortest %.*g text that reads back as exactly the same double. */
TinyString tc_format_double(double value) {
    char buffer[40];
    int precision, n = 0;
    TinyString s;
    for (precision = 1; precision <= 17; precision++) {
        n = snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        if (value != value || strtod(buffer, NULL) == value)
            break;
    }
    s.data = buffer;
    s.length = (size_t)n;
    return tc_string_copy(s);
}
void tc_write_error(TinyString text) {
    fflush(stdout);
    if (text.length)
        fwrite(text.data, 1, text.length, stderr);
    fflush(stderr);
}
#ifdef _WIN32
static int tc_spawn_quote(char **out, size_t *length, size_t *capacity, const char *s, size_t n) {
    size_t i, slashes = 0, need = *length + 3 + 2 * n + 1;
    char *p = *out;
    if (need > *capacity) {
        size_t next = *capacity ? *capacity : 256;
        while (next < need)
            next *= 2;
        p = (char *)realloc(p, next);
        if (!p)
            return 0;
        *out = p;
        *capacity = next;
    }
    if (*length)
        p[(*length)++] = ' ';
    p[(*length)++] = '"';
    for (i = 0; i < n; i++) {
        if (s[i] == '\\') {
            slashes++;
            continue;
        }
        if (s[i] == '"') {
            while (slashes) {
                p[(*length)++] = '\\';
                p[(*length)++] = '\\';
                slashes--;
            }
            p[(*length)++] = '\\';
            p[(*length)++] = '"';
        } else {
            while (slashes) {
                p[(*length)++] = '\\';
                slashes--;
            }
            p[(*length)++] = s[i];
        }
    }
    while (slashes) {
        p[(*length)++] = '\\';
        p[(*length)++] = '\\';
        slashes--;
    }
    p[(*length)++] = '"';
    p[*length] = 0;
    return 1;
}
#endif
/*
 * Runs argv[0] with the given arguments, without a shell, and waits for it.
 * Returns the exit status (128 + signal on POSIX). *error is 0 on success and
 * non-zero when the program could not be started at all.
 */
int32_t tc_process_spawn(TinyString *argv, uint64_t count, int32_t *error) {
    uint64_t i;
    *error = 0;
    if (!argv || count == 0) {
        *error = 1;
        return -1;
    }
    for (i = 0; i < count; i++)
        if (argv[i].length && memchr(argv[i].data, 0, argv[i].length)) {
            *error = 1;
            return -1;
        }
    fflush(stdout);
    fflush(stderr);
#ifdef _WIN32
    {
        char *command = NULL;
        size_t length = 0, capacity = 0;
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        DWORD code = 0;
        for (i = 0; i < count; i++)
            if (!tc_spawn_quote(&command, &length, &capacity, argv[i].data, argv[i].length)) {
                free(command);
                *error = 1;
                return -1;
            }
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));
        if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            free(command);
            *error = (int32_t)GetLastError();
            if (!*error)
                *error = 1;
            return -1;
        }
        free(command);
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (int32_t)code;
    }
#else
    {
        char **args = (char **)calloc((size_t)count + 1, sizeof(char *));
        int report[2], status = 0, child_errno = 0;
        ssize_t got;
        pid_t pid;
        if (!args) {
            *error = ENOMEM;
            return -1;
        }
        for (i = 0; i < count; i++) {
            args[i] = (char *)malloc(argv[i].length + 1);
            if (!args[i])
                break;
            if (argv[i].length)
                memcpy(args[i], argv[i].data, argv[i].length);
            args[i][argv[i].length] = 0;
        }
        if (i < count || pipe(report) != 0) {
            *error = errno ? errno : ENOMEM;
            for (i = 0; i < count; i++)
                free(args[i]);
            free(args);
            return -1;
        }
        fcntl(report[1], F_SETFD, FD_CLOEXEC);
        pid = fork();
        if (pid == 0) {
            close(report[0]);
            execvp(args[0], args);
            child_errno = errno;
            if (write(report[1], &child_errno, sizeof(child_errno)) < 0)
                _exit(127);
            _exit(127);
        }
        close(report[1]);
        for (i = 0; i < count; i++)
            free(args[i]);
        free(args);
        if (pid < 0) {
            *error = errno ? errno : 1;
            close(report[0]);
            return -1;
        }
        do
            got = read(report[0], &child_errno, sizeof(child_errno));
        while (got < 0 && errno == EINTR);
        close(report[0]);
        while (waitpid(pid, &status, 0) < 0)
            if (errno != EINTR) {
                *error = errno;
                return -1;
            }
        if (got == (ssize_t)sizeof(child_errno)) {
            *error = child_errno ? child_errno : 1;
            return -1;
        }
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return 128 + WTERMSIG(status);
    }
#endif
}
#endif
