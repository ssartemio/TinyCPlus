#ifndef TINY_RUNTIME_H
#define TINY_RUNTIME_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#if defined(_WIN32) && !defined(_WIN64)
#define TC_STDCALL __attribute__((stdcall))
#else
#define TC_STDCALL
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
typedef struct {
    const char *data;
    size_t length;
} TinyString;
static int tc_program_argc;
static char **tc_program_argv;
static void (*tc_program_cleanup)(void);
#define TC_STRING(s) ((TinyString){(s), sizeof(s) - 1})
static void tc_panic(const char *message, const char *file, int line) {
    if (tc_program_cleanup)
        tc_program_cleanup();
    fprintf(stderr, "%s:%d: runtime error: %s\n", file, line, message);
    exit(101);
}
static void *tc_alloc_checked(size_t n) {
    void *p = calloc(1, n);
    if (!p)
        tc_panic("out of memory", "<runtime>", 0);
    return p;
}
static void tc_assert(int ok, const char *file, int line) {
    if (!ok)
        tc_panic("assertion failed", file, line);
}
static size_t tc_index(int64_t i, size_t n, const char *file, int line) {
    if (i < 0 || (uint64_t)i >= n)
        tc_panic("index out of bounds", file, line);
    return (size_t)i;
}
static void tc_slice_check(int64_t lo, int64_t hi, size_t n, const char *file, int line) {
    if (lo < 0 || hi < lo || (uint64_t)hi > n)
        tc_panic("slice out of bounds", file, line);
}
static bool tc_string_equal(TinyString a, TinyString b) {
    return a.length == b.length && (!a.length || memcmp(a.data, b.data, a.length) == 0);
}
/* hash(): FNV-1a for string contents, SplitMix64 finalizer for scalar values. */
static uint64_t tc_hash_u64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}
static uint64_t tc_hash_string(TinyString s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    size_t i;
    for (i = 0; i < s.length; i++) {
        h ^= (unsigned char)s.data[i];
        h *= 0x100000001b3ULL;
    }
    return tc_hash_u64(h);
}
static void tc_print_string(TinyString s, int newline) {
    if (s.length)
        fwrite(s.data, 1, s.length, stdout);
    if (newline)
        fputc('\n', stdout);
}
static void tc_print_integer(int64_t n, int newline) {
    printf("%lld%s", (long long)n, newline ? "\n" : "");
}
static void tc_print_unsigned(uint64_t n, int newline) {
    printf("%llu%s", (unsigned long long)n, newline ? "\n" : "");
}
static void tc_print_float(double n, int newline) {
    printf("%.17g%s", n, newline ? "\n" : "");
}
static void tc_print_pointer(const void *p, int newline) {
    printf("%p%s", p, newline ? "\n" : "");
}
static void tc_closure_destroy(void *environment, bool owned) {
    if (owned)
        free(environment);
}
#endif
