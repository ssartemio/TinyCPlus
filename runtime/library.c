/*
 * Puente C de strings, consola, archivos, procesos y utilidades estándar.
 * Convierte vistas TinyString a cadenas C cuando la API del host lo requiere.
 * Las reservas devueltas al lenguaje se liberan mediante sus wrappers,
 * por ejemplo OwnedString.destroy; una vista no transfiere propiedad.
 */
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
    if (s.length == SIZE_MAX || (!s.data && s.length) || (s.length && memchr(s.data, 0, s.length))) {
        errno = EINVAL; return NULL;
    }
    p = (char *)malloc(s.length + 1);
    if (!p) { errno = ENOMEM; return NULL; }
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
#ifdef _WIN32
/* TinyString paths and process commands are UTF-8 at the public boundary. */
static int tc_windows_last_error(void) { int error = (int)GetLastError(); return error ? error : EIO; }
static wchar_t *tc_utf8_wide(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *wide;
    if (!count) { errno = EINVAL; return NULL; }
    wide = (wchar_t *)malloc((size_t)count * sizeof(wchar_t));
    if (!wide) { errno = ENOMEM; return NULL; }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count)) {
        free(wide); errno = EINVAL; return NULL;
    }
    return wide;
}
static FILE *tc_host_fopen(const char *path, const char *mode) {
    wchar_t *p = tc_utf8_wide(path), *m = tc_utf8_wide(mode);
    FILE *file = p && m ? _wfopen(p, m) : NULL;
    free(p); free(m); return file;
}
static int tc_host_remove(const char *path) {
    wchar_t *p = tc_utf8_wide(path);
    int result = p ? _wremove(p) : -1; free(p); return result;
}
static int tc_host_exists(const char *path) {
    wchar_t *p = tc_utf8_wide(path);
    int result = p && GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
    free(p); return result;
}
#else
#define tc_host_fopen fopen
#define tc_host_remove remove
static int tc_host_exists(const char *path) { struct stat st; return stat(path, &st) == 0; }
#endif
/* Stable error categories; native codes are retained separately by callers. */
int32_t tc_system_error_category(int32_t error) {
    if (!error) return 0;
#ifdef _WIN32
    if (error == 10013) return 6;
    if (error == 10022) return 3;
    if (error == 10060) return 4;
    if (error == 10055) return 5;
#endif
    if (error == ENOENT) return 2;
    if (error == EINVAL) return 3;
    if (error == ENOMEM) return 5;
    if (error == EACCES || error == EPERM) return 6;
#ifdef ETIMEDOUT
    if (error == ETIMEDOUT) return 4;
#endif
    return 7;
}
int32_t tc_file_atomic_category(int32_t error) {
#ifdef _WIN32
    if (!error) return 0;
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return 2;
    if (error == ERROR_ACCESS_DENIED) return 6;
    if (error == ERROR_NOT_ENOUGH_MEMORY || error == ERROR_OUTOFMEMORY) return 5;
    if (error == ERROR_INVALID_PARAMETER || error == ERROR_NO_UNICODE_TRANSLATION) return 3;
    return 7;
#else
    return tc_system_error_category(error);
#endif
}
int32_t tc_file_last_error(void) { return errno ? errno : EIO; }
TinyString tc_file_read_all(TinyString path, int32_t *error) {
    TinyString result = {NULL, 0};
    char *name = tc_cstring(path), *data;
    FILE *f;
    long size;
    *error = 0;
    if (!name) {
        *error = errno ? errno : EIO;
        return result;
    }
    f = tc_host_fopen(name, "rb");
    free(name);
    if (!f) {
        *error = errno ? errno : EIO;
        return result;
    }
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        *error = errno ? errno : EIO;
        fclose(f);
        return result;
    }
    data = (char *)malloc((size_t)size + 1);
    if (!data) {
        *error = ENOMEM;
        fclose(f);
        return result;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        *error = errno ? errno : EIO;
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
    if (!name) return errno ? errno : EINVAL;
    f = tc_host_fopen(name, "wb");
    free(name);
    if (!f)
        return errno ? errno : EIO;
    error = fwrite(data.data, 1, data.length, f) == data.length ? 0 : (errno ? errno : EIO);
    if (fclose(f) && !error) error = errno ? errno : EIO;
    return error;
}
int32_t tc_file_exists(TinyString path) {
    char *name = tc_cstring(path);
    int result;
    if (!name)
        return 0;
    result = tc_host_exists(name);
    free(name);
    return result;
}
int32_t tc_file_remove(TinyString path) {
    char *name = tc_cstring(path);
    int result;
    if (!name)
        return 1;
    result = tc_host_remove(name);
    free(name);
    return result ? (errno ? errno : EIO) : 0;
}
void *tc_file_open(TinyString path, TinyString mode) {
    char *p = tc_cstring(path), *m = tc_cstring(mode);
    FILE *f = NULL;
    if (p && m)
        f = tc_host_fopen(p, m);
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
int64_t tc_file_transfer(void *handle, void *data, uint64_t length, int32_t writing,
                          int32_t exactly, int32_t *error) {
    FILE *file = (FILE *)handle;
    uint64_t offset = 0;
    *error = 0;
    if (!file || length > SIZE_MAX || length > INT64_MAX || (!data && length)) {
        *error = EINVAL; return 0;
    }
    while (offset < length) {
        size_t count = writing ? fwrite((char *)data + offset, 1, (size_t)(length - offset), file)
                               : fread((char *)data + offset, 1, (size_t)(length - offset), file);
        offset += count;
        if (ferror(file)) { *error = errno ? errno : EIO; break; }
        if (!count || !exactly) break;
    }
    return (int64_t)offset;
}
int32_t tc_file_close(void *handle) {
    return handle && fclose((FILE *)handle) ? (errno ? errno : EIO) : 0;
}
int32_t tc_file_write_atomic(TinyString path, TinyString contents) {
    char *name = tc_cstring(path), *temporary;
    size_t written = 0;
    int error = 0;
    if (!name) {
#ifdef _WIN32
        return errno == ENOMEM ? ERROR_NOT_ENOUGH_MEMORY : ERROR_INVALID_PARAMETER;
#else
        return errno ? errno : EINVAL;
#endif
    }
    temporary = (char *)malloc(strlen(name) + 80);
    if (!temporary) {
        free(name);
#ifdef _WIN32
        return ERROR_NOT_ENOUGH_MEMORY;
#else
        return ENOMEM;
#endif
    }
#ifdef _WIN32
    {
        static LONG counter;
        HANDLE file = INVALID_HANDLE_VALUE;
        wchar_t *wide_name = tc_utf8_wide(name), *wide_temporary = NULL;
        int attempt;
        if (!wide_name) { free(name); free(temporary); return ERROR_NO_UNICODE_TRANSLATION; }
        for (attempt = 0; attempt < 32; attempt++) {
            snprintf(temporary, strlen(name) + 80, "%s.tiny-%lu-%ld.tmp", name,
                     (unsigned long)GetCurrentProcessId(), (long)InterlockedIncrement(&counter));
            free(wide_temporary);
            wide_temporary = tc_utf8_wide(temporary);
            if (!wide_temporary) { error = ERROR_NO_UNICODE_TRANSLATION; break; }
            file = CreateFileW(wide_temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                               NULL);
            if (file != INVALID_HANDLE_VALUE)
                break;
            if (GetLastError() != ERROR_FILE_EXISTS) {
                error = tc_windows_last_error();
                break;
            }
        }
        if (file == INVALID_HANDLE_VALUE)
            error = tc_windows_last_error();
        else {
            while (written < contents.length) {
                DWORD count = 0, chunk = (DWORD)((contents.length - written) > 1048576
                                                     ? 1048576
                                                     : contents.length - written);
                if (!WriteFile(file, contents.data + written, chunk, &count, NULL) || !count) {
                    error = tc_windows_last_error();
                    break;
                }
                written += count;
            }
            if (!error && !FlushFileBuffers(file))
                error = tc_windows_last_error();
            if (!CloseHandle(file))
                error = tc_windows_last_error();
            if (!error &&
                !MoveFileExW(wide_temporary, wide_name, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                error = tc_windows_last_error();
            if (error)
                DeleteFileW(wide_temporary);
        }
        free(wide_name); free(wide_temporary);
    }
#else
    {
        int file;
        struct stat previous;
        snprintf(temporary, strlen(name) + 80, "%s.tiny-XXXXXX", name);
        file = mkstemp(temporary);
        if (file < 0)
            error = errno ? errno : EIO;
        else {
            if (stat(name, &previous) == 0 && fchmod(file, previous.st_mode & 0777))
                error = errno ? errno : EIO;
            while (!error && written < contents.length) {
                ssize_t n = write(file, contents.data + written, contents.length - written);
                if (n < 0 && errno == EINTR)
                    continue;
                if (n <= 0) {
                    error = errno ? errno : EIO;
                    break;
                }
                written += (size_t)n;
            }
            if (!error && fsync(file))
                error = errno ? errno : EIO;
            if (close(file))
                error = errno ? errno : EIO;
            if (!error && rename(temporary, name))
                error = errno ? errno : EIO;
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
/* Preserve an interior source offset before realloc invalidates borrowed views. */
int32_t tc_builder_append(char **data, uint64_t *length, uint64_t *capacity, TinyString text) {
    uintptr_t base = (uintptr_t)*data, source = (uintptr_t)text.data;
    uint64_t offset = 0, required, next;
    int aliases = *data && source >= base && source - base <= *length;
    char *fresh;
    if (!text.length) return 0;
    if (!text.data || text.length >= UINT64_MAX - *length ||
        text.length >= SIZE_MAX - *length) return 3;
    if (aliases) {
        offset = (uint64_t)(source - base);
        if (text.length > *length - offset) return 3;
    }
    required = *length + text.length + 1;
    if (required > *capacity) {
        next = *capacity <= UINT64_MAX / 2 ? *capacity * 2 : required;
        if (next < 64) next = 64;
        if (next < required) next = required;
        fresh = (char *)realloc(*data, (size_t)next);
        if (!fresh) return 8;
        *data = fresh; *capacity = next;
    }
    memmove(*data + *length, aliases ? *data + offset : text.data, (size_t)text.length);
    *length += text.length;
    (*data)[*length] = 0;
    return 0;
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
    size_t i, slashes = 0, need;
    char *p = *out;
    if (*length > SIZE_MAX - 4 || n > (SIZE_MAX - *length - 4) / 2) return 0;
    need = *length + 4 + 2 * n;
    if (need > *capacity) {
        size_t next = *capacity ? *capacity : 256;
        while (next < need) {
            if (next > SIZE_MAX / 2) { next = need; break; }
            next *= 2;
        }
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
#include "process.c"
int32_t tc_process_spawn(TinyString *argv, uint64_t count, int32_t *error) {
    void *process = tc_process_start(argv, count, error);
    int32_t status;
    if (!process) return -1;
    status = tc_process_wait(process, 0, error);
    tc_process_destroy(process); return status;
}
#endif
