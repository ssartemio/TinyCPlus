/* Native argv execution. A handle owns the child and reaps it exactly once. */
#ifndef TC_PROCESS_IMPLEMENTATION
#define TC_PROCESS_IMPLEMENTATION
#include "library.c"
#ifndef _WIN32
#include <signal.h>
#include <time.h>
#endif
typedef struct TcProcess {
#ifdef _WIN32
    HANDLE process;
#else
    pid_t pid;
#endif
    int done, status;
} TcProcess;
static uint64_t tc_process_clock(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
#endif
}
void *tc_process_start(TinyString *argv, uint64_t count, int32_t *error) {
    TcProcess *process;
    uint64_t i;
    *error = 0;
    if (!argv || !count || count > SIZE_MAX / sizeof(char *) - 1) { *error = EINVAL; return NULL; }
    for (i = 0; i < count; i++)
        if ((!argv[i].data && argv[i].length) || argv[i].length >= SIZE_MAX ||
            (argv[i].length && memchr(argv[i].data, 0, argv[i].length))) { *error = EINVAL; return NULL; }
    process = (TcProcess *)calloc(1, sizeof(*process));
    if (!process) { *error = ENOMEM; return NULL; }
    fflush(stdout); fflush(stderr);
#ifdef _WIN32
    {
        char *command = NULL;
        wchar_t *wide;
        size_t length = 0, capacity = 0;
        STARTUPINFOW startup;
        PROCESS_INFORMATION child;
        for (i = 0; i < count; i++)
            if (!tc_spawn_quote(&command, &length, &capacity, argv[i].data, argv[i].length)) {
                free(command); free(process); *error = ENOMEM; return NULL;
            }
        wide = tc_utf8_wide(command); free(command);
        if (!wide) { free(process); *error = EINVAL; return NULL; }
        memset(&startup, 0, sizeof(startup)); startup.cb = sizeof(startup);
        memset(&child, 0, sizeof(child));
        if (!CreateProcessW(NULL, wide, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &child)) {
            *error = (int32_t)GetLastError(); free(wide); free(process); return NULL;
        }
        free(wide); CloseHandle(child.hThread); process->process = child.hProcess;
    }
#else
    {
        char **args = (char **)calloc((size_t)count + 1, sizeof(char *));
        int report[2], native = 0;
        ssize_t got;
        if (!args) { free(process); *error = ENOMEM; return NULL; }
        for (i = 0; i < count; i++) if (!(args[i] = tc_cstring(argv[i]))) break;
        if (i < count || pipe(report)) {
            *error = errno ? errno : ENOMEM;
            for (i = 0; i < count; i++) free(args[i]);
            free(args); free(process); return NULL;
        }
        if (fcntl(report[1], F_SETFD, FD_CLOEXEC) < 0) {
            *error = errno; close(report[0]); close(report[1]);
            for (i = 0; i < count; i++) free(args[i]);
            free(args); free(process); return NULL;
        }
        process->pid = fork();
        if (!process->pid) {
            close(report[0]); execvp(args[0], args); native = errno;
            while (write(report[1], &native, sizeof(native)) < 0 && errno == EINTR) {}
            _exit(127);
        }
        native = errno;
        close(report[1]);
        for (i = 0; i < count; i++) free(args[i]);
        free(args);
        if (process->pid < 0) { close(report[0]); free(process); *error = native; return NULL; }
        do { got = read(report[0], &native, sizeof(native)); } while (got < 0 && errno == EINTR);
        close(report[0]);
        if (got != 0) {
            int status;
            while (waitpid(process->pid, &status, 0) < 0 && errno == EINTR) {}
            free(process); *error = got > 0 ? native : EIO; return NULL;
        }
    }
#endif
    return process;
}
/* timeout=0 waits indefinitely. A timeout never consumes or kills the child. */
int32_t tc_process_wait(void *handle, uint64_t timeout, int32_t *error) {
    TcProcess *process = (TcProcess *)handle;
    uint64_t start = tc_process_clock();
    *error = 0;
    if (!process) { *error = EINVAL; return -1; }
    while (!process->done) {
#ifdef _WIN32
        DWORD result = WaitForSingleObject(process->process, 1), code;
        if (result == WAIT_OBJECT_0 && GetExitCodeProcess(process->process, &code)) {
            process->status = (int32_t)code; process->done = 1;
        } else if (result == WAIT_FAILED) { *error = (int32_t)GetLastError(); return -1; }
#else
        int status;
        pid_t result = waitpid(process->pid, &status, WNOHANG);
        if (result == process->pid) {
            process->status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            process->done = 1;
        } else if (result < 0 && errno != EINTR) { *error = errno; return -1; }
        else { struct timespec pause = {0, 1000000}; nanosleep(&pause, NULL); }
#endif
        if (!process->done && timeout && tc_process_clock() - start >= timeout) { *error = 4; return -1; }
    }
    return process->status;
}
int32_t tc_process_cancel(void *handle) {
    TcProcess *process = (TcProcess *)handle;
    int32_t error;
    if (!process) return EINVAL;
    if (process->done) return 0;
    tc_process_wait(handle, 1, &error); /* Reap an already exited child before signalling. */
    if (process->done) return 0;
#ifdef _WIN32
    return TerminateProcess(process->process, 1) ? 0 : (int32_t)GetLastError();
#else
    return kill(process->pid, SIGKILL) == 0 ? 0 : errno;
#endif
}
void tc_process_destroy(void *handle) {
    TcProcess *process = (TcProcess *)handle;
    int32_t error;
    if (!process) return;
    if (!process->done) { tc_process_cancel(handle); tc_process_wait(handle, 0, &error); }
#ifdef _WIN32
    CloseHandle(process->process);
#endif
    free(process);
}
#endif
