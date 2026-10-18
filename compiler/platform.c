/* UTF-8 filesystem boundary for the native frontend. */
#include "tiny.h"
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#include <direct.h>
wchar_t *tc_utf16(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *result;
    if (!count) { errno = EINVAL; return NULL; }
    result = (wchar_t *)malloc((size_t)count * sizeof(wchar_t));
    if (!result) { errno = ENOMEM; return NULL; }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, count)) {
        free(result); errno = EINVAL; return NULL;
    }
    return result;
}
char *tc_utf8(const wchar_t *text) {
    int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *result = count ? (char *)malloc((size_t)count) : NULL;
    if (!result) return NULL;
    if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, result, count, NULL, NULL)) { free(result); return NULL; }
    return result;
}
FILE *tc_fopen(const char *path, const char *mode) {
    wchar_t *p = tc_utf16(path), *m = tc_utf16(mode);
    FILE *file = p && m ? _wfopen(p, m) : NULL;
    int error = errno;
    free(p); free(m); errno = error; return file;
}
int tc_remove(const char *path) {
    wchar_t *p = tc_utf16(path);
    int result = p ? _wremove(p) : -1;
    free(p); return result;
}
int tc_mkdir(const char *path) {
    wchar_t *p = tc_utf16(path);
    int result = p ? _wmkdir(p) : -1;
    free(p); return result;
}
#else
FILE *tc_fopen(const char *path, const char *mode) { return fopen(path, mode); }
int tc_remove(const char *path) { return remove(path); }
#endif
