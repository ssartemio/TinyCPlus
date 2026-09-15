#include "concurrent.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION TcMutex;
typedef struct {
    HANDLE semaphore;
    CRITICAL_SECTION lock;
    LONG waiters;
} TcCond;
typedef HANDLE TcNativeThread;
static void mutex_init(TcMutex *m) {
    InitializeCriticalSection(m);
}
static void mutex_lock(TcMutex *m) {
    EnterCriticalSection(m);
}
static void mutex_unlock(TcMutex *m) {
    LeaveCriticalSection(m);
}
static void mutex_dispose(TcMutex *m) {
    DeleteCriticalSection(m);
}
static void cond_init(TcCond *c) {
    c->semaphore = CreateSemaphoreA(NULL, 0, LONG_MAX, NULL);
    if (!c->semaphore)
        abort();
    InitializeCriticalSection(&c->lock);
    c->waiters = 0;
}
static void cond_wait(TcCond *c, TcMutex *m) {
    EnterCriticalSection(&c->lock);
    c->waiters++;
    LeaveCriticalSection(&c->lock);
    mutex_unlock(m);
    WaitForSingleObject(c->semaphore, INFINITE);
    mutex_lock(m);
}
static void cond_signal(TcCond *c) {
    EnterCriticalSection(&c->lock);
    if (c->waiters) {
        c->waiters--;
        ReleaseSemaphore(c->semaphore, 1, NULL);
    }
    LeaveCriticalSection(&c->lock);
}
static void cond_broadcast(TcCond *c) {
    EnterCriticalSection(&c->lock);
    if (c->waiters) {
        ReleaseSemaphore(c->semaphore, c->waiters, NULL);
        c->waiters = 0;
    }
    LeaveCriticalSection(&c->lock);
}
static void cond_dispose(TcCond *c) {
    CloseHandle(c->semaphore);
    DeleteCriticalSection(&c->lock);
}
#else
#include <pthread.h>
#include <time.h>
#include <errno.h>
typedef pthread_mutex_t TcMutex;
typedef pthread_cond_t TcCond;
typedef pthread_t TcNativeThread;
static void mutex_init(TcMutex *m) {
    pthread_mutex_init(m, NULL);
}
static void mutex_lock(TcMutex *m) {
    pthread_mutex_lock(m);
}
static void mutex_unlock(TcMutex *m) {
    pthread_mutex_unlock(m);
}
static void mutex_dispose(TcMutex *m) {
    pthread_mutex_destroy(m);
}
static void cond_init(TcCond *c) {
    pthread_cond_init(c, NULL);
}
static void cond_wait(TcCond *c, TcMutex *m) {
    pthread_cond_wait(c, m);
}
static void cond_signal(TcCond *c) {
    pthread_cond_signal(c);
}
static void cond_broadcast(TcCond *c) {
    pthread_cond_broadcast(c);
}
static void cond_dispose(TcCond *c) {
    pthread_cond_destroy(c);
}
#endif
typedef struct TcThread {
    TcNativeThread native;
    TcAction action;
    void *argument;
    int joined;
#ifdef _WIN32
    DWORD id;
#endif
} TcThread;
#ifdef _WIN32
static DWORD WINAPI thread_entry(LPVOID arg) {
    TcThread *t = (TcThread *)arg;
    t->action(t->argument);
    return 0;
}
#else
static void *thread_entry(void *arg) {
    TcThread *t = (TcThread *)arg;
    t->action(t->argument);
    return NULL;
}
#endif
void *tc_thread_start(TcAction action, void *argument) {
    TcThread *t;
    if (!action)
        return NULL;
    t = (TcThread *)calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    t->action = action;
    t->argument = argument;
#ifdef _WIN32
    t->native = CreateThread(NULL, 0, thread_entry, t, 0, &t->id);
    if (!t->native) {
        free(t);
        return NULL;
    }
#else
    if (pthread_create(&t->native, NULL, thread_entry, t)) {
        free(t);
        return NULL;
    }
#endif
    return t;
}
int32_t tc_thread_join(void *handle) {
    TcThread *t = (TcThread *)handle;
    if (!t)
        return 1;
    if (t->joined)
        return 0;
#ifdef _WIN32
    if (t->id == GetCurrentThreadId())
        return 2;
    if (WaitForSingleObject(t->native, INFINITE) != WAIT_OBJECT_0)
        return 1;
    CloseHandle(t->native);
#else
    if (pthread_equal(t->native, pthread_self()))
        return 2;
    if (pthread_join(t->native, NULL))
        return 1;
#endif
    t->joined = 1;
    return 0;
}
void tc_thread_destroy(void *handle) {
    if (handle && tc_thread_join(handle) == 0)
        free(handle);
}
void tc_sleep_ms(uint64_t ms) {
#ifdef _WIN32
    while (ms > 0xfffffffeU) {
        Sleep(0xfffffffeU);
        ms -= 0xfffffffeU;
    }
    Sleep((DWORD)ms);
#else
    struct timespec t;
    t.tv_sec = (time_t)(ms / 1000);
    t.tv_nsec = (long)((ms % 1000) * 1000000);
    while (nanosleep(&t, &t) && errno == EINTR) {
    }
#endif
}
uint64_t tc_clock_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER ticks, frequency;
    QueryPerformanceCounter(&ticks);
    QueryPerformanceFrequency(&frequency);
    return (uint64_t)(ticks.QuadPart / frequency.QuadPart) * 1000 +
           (uint64_t)(ticks.QuadPart % frequency.QuadPart) * 1000 / (uint64_t)frequency.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
#endif
}
void *tc_mutex_create(void) {
    TcMutex *m = (TcMutex *)malloc(sizeof(*m));
    if (m)
        mutex_init(m);
    return m;
}
void tc_mutex_lock(void *m) {
    mutex_lock((TcMutex *)m);
}
void tc_mutex_unlock(void *m) {
    mutex_unlock((TcMutex *)m);
}
void tc_mutex_destroy(void *m) {
    if (m) {
        mutex_dispose((TcMutex *)m);
        free(m);
    }
}
void *tc_condition_create(void) {
    TcCond *c = (TcCond *)malloc(sizeof(*c));
    if (c)
        cond_init(c);
    return c;
}
void tc_condition_wait(void *c, void *m) {
    cond_wait((TcCond *)c, (TcMutex *)m);
}
void tc_condition_signal(void *c) {
    cond_signal((TcCond *)c);
}
void tc_condition_broadcast(void *c) {
    cond_broadcast((TcCond *)c);
}
void tc_condition_destroy(void *c) {
    if (c) {
        cond_dispose((TcCond *)c);
        free(c);
    }
}
typedef struct TcSemaphore {
    TcMutex lock;
    TcCond changed;
    uint64_t count;
} TcSemaphore;
void *tc_semaphore_create(uint64_t count) {
    TcSemaphore *s = (TcSemaphore *)calloc(1, sizeof(*s));
    if (s) {
        mutex_init(&s->lock);
        cond_init(&s->changed);
        s->count = count;
    }
    return s;
}
void tc_semaphore_wait(void *h) {
    TcSemaphore *s = (TcSemaphore *)h;
    mutex_lock(&s->lock);
    while (!s->count)
        cond_wait(&s->changed, &s->lock);
    s->count--;
    mutex_unlock(&s->lock);
}
void tc_semaphore_post(void *h) {
    TcSemaphore *s = (TcSemaphore *)h;
    mutex_lock(&s->lock);
    if (s->count < UINT64_MAX)
        s->count++;
    cond_signal(&s->changed);
    mutex_unlock(&s->lock);
}
void tc_semaphore_destroy(void *h) {
    TcSemaphore *s = (TcSemaphore *)h;
    if (s) {
        mutex_dispose(&s->lock);
        cond_dispose(&s->changed);
        free(s);
    }
}
void *tc_event_create(void) {
    return tc_semaphore_create(0);
}
void tc_event_wait(void *h) {
    TcSemaphore *s = (TcSemaphore *)h;
    mutex_lock(&s->lock);
    while (!s->count)
        cond_wait(&s->changed, &s->lock);
    mutex_unlock(&s->lock);
}
void tc_event_set(void *h) {
    TcSemaphore *s = (TcSemaphore *)h;
    mutex_lock(&s->lock);
    s->count = 1;
    cond_broadcast(&s->changed);
    mutex_unlock(&s->lock);
}
void tc_event_reset(void *h) {
    TcSemaphore *s = (TcSemaphore *)h;
    mutex_lock(&s->lock);
    s->count = 0;
    mutex_unlock(&s->lock);
}
void tc_event_destroy(void *h) {
    tc_semaphore_destroy(h);
}
typedef struct TcAtomic {
    TcMutex lock;
    int64_t value;
} TcAtomic;
void *tc_atomic_create(int64_t value) {
    TcAtomic *a = (TcAtomic *)malloc(sizeof(*a));
    if (a) {
        mutex_init(&a->lock);
        a->value = value;
    }
    return a;
}
int64_t tc_atomic_load(void *h) {
    TcAtomic *a = (TcAtomic *)h;
    int64_t v;
    mutex_lock(&a->lock);
    v = a->value;
    mutex_unlock(&a->lock);
    return v;
}
void tc_atomic_store(void *h, int64_t v) {
    TcAtomic *a = (TcAtomic *)h;
    mutex_lock(&a->lock);
    a->value = v;
    mutex_unlock(&a->lock);
}
int64_t tc_atomic_add(void *h, int64_t v) {
    TcAtomic *a = (TcAtomic *)h;
    int64_t old;
    mutex_lock(&a->lock);
    old = a->value;
    a->value = (int64_t)((uint64_t)old + (uint64_t)v);
    mutex_unlock(&a->lock);
    return old;
}
void tc_atomic_destroy(void *h) {
    TcAtomic *a = (TcAtomic *)h;
    if (a) {
        mutex_dispose(&a->lock);
        free(a);
    }
}

typedef struct TcJob {
    TcAction action;
    void *argument;
    struct TcJob *next;
} TcJob;
typedef struct TcPool {
    TcMutex lock;
    TcCond available;
    TcJob *head, *tail;
    void **threads;
    int workers, stopping;
} TcPool;
static void pool_worker(void *arg) {
    TcPool *pool = (TcPool *)arg;
    for (;;) {
        TcJob *job;
        mutex_lock(&pool->lock);
        while (!pool->head && !pool->stopping)
            cond_wait(&pool->available, &pool->lock);
        if (!pool->head && pool->stopping) {
            mutex_unlock(&pool->lock);
            break;
        }
        job = pool->head;
        pool->head = job->next;
        if (!pool->head)
            pool->tail = NULL;
        mutex_unlock(&pool->lock);
        job->action(job->argument);
        free(job);
    }
}
void *tc_pool_create(int32_t workers) {
    TcPool *p;
    int i;
    if (workers < 1 || workers > 256)
        return NULL;
    p = (TcPool *)calloc(1, sizeof(*p));
    if (!p)
        return NULL;
    p->threads = (void **)calloc((size_t)workers, sizeof(void *));
    if (!p->threads) {
        free(p);
        return NULL;
    }
    mutex_init(&p->lock);
    cond_init(&p->available);
    for (i = 0; i < workers; i++) {
        p->threads[i] = tc_thread_start(pool_worker, p);
        if (!p->threads[i]) {
            tc_pool_destroy(p);
            return NULL;
        }
        p->workers++;
    }
    return p;
}
int32_t tc_pool_submit(void *handle, TcAction action, void *argument) {
    TcPool *p = (TcPool *)handle;
    TcJob *j;
    if (!p || !action)
        return 1;
    j = (TcJob *)malloc(sizeof(*j));
    if (!j)
        return 2;
    j->action = action;
    j->argument = argument;
    j->next = NULL;
    mutex_lock(&p->lock);
    if (p->stopping) {
        mutex_unlock(&p->lock);
        free(j);
        return 3;
    }
    if (p->tail)
        p->tail->next = j;
    else
        p->head = j;
    p->tail = j;
    cond_signal(&p->available);
    mutex_unlock(&p->lock);
    return 0;
}
void tc_pool_destroy(void *handle) {
    TcPool *p = (TcPool *)handle;
    int i;
    if (!p)
        return;
    mutex_lock(&p->lock);
    p->stopping = 1;
    cond_broadcast(&p->available);
    mutex_unlock(&p->lock);
    for (i = 0; i < p->workers; i++)
        tc_thread_destroy(p->threads[i]);
    cond_dispose(&p->available);
    mutex_dispose(&p->lock);
    free(p->threads);
    free(p);
}
typedef struct TcContinuation {
    TcAction action;
    void *argument;
    TcPool *pool;
    struct TcContinuation *next;
} TcContinuation;
typedef struct TcFuture {
    TcMutex lock;
    TcCond ready;
    uint64_t refs;
    size_t size;
    int complete, error;
    TcContinuation *continuations;
    unsigned char data[1];
} TcFuture;
void *tc_future_create(uint64_t size) {
    TcFuture *f;
    if (size > SIZE_MAX - sizeof(TcFuture))
        return NULL;
    f = (TcFuture *)calloc(1, sizeof(*f) + (size_t)size);
    if (!f)
        return NULL;
    f->size = (size_t)size;
    f->refs = 1;
    mutex_init(&f->lock);
    cond_init(&f->ready);
    return f;
}
void tc_future_retain(void *h) {
    TcFuture *f = (TcFuture *)h;
    mutex_lock(&f->lock);
    f->refs++;
    mutex_unlock(&f->lock);
}
void tc_future_release(void *h) {
    TcFuture *f = (TcFuture *)h;
    int dispose;
    if (!f)
        return;
    mutex_lock(&f->lock);
    dispose = --f->refs == 0;
    mutex_unlock(&f->lock);
    if (dispose) {
        TcContinuation *p = f->continuations;
        while (p) {
            TcContinuation *q = p->next;
            free(p);
            p = q;
        }
        cond_dispose(&f->ready);
        mutex_dispose(&f->lock);
        free(f);
    }
}
int32_t tc_future_complete(void *h, void *value, int32_t error) {
    TcFuture *f = (TcFuture *)h;
    TcContinuation *list;
    mutex_lock(&f->lock);
    if (f->complete) {
        mutex_unlock(&f->lock);
        return 1;
    }
    if (value && f->size)
        memcpy(f->data, value, f->size);
    f->complete = 1;
    f->error = error;
    list = f->continuations;
    f->continuations = NULL;
    cond_broadcast(&f->ready);
    mutex_unlock(&f->lock);
    while (list) {
        TcContinuation *next = list->next;
        if (!list->pool || tc_pool_submit(list->pool, list->action, list->argument))
            list->action(list->argument);
        free(list);
        list = next;
    }
    return 0;
}
int32_t tc_future_ready(void *h) {
    TcFuture *f = (TcFuture *)h;
    int ready;
    mutex_lock(&f->lock);
    ready = f->complete;
    mutex_unlock(&f->lock);
    return ready;
}
int32_t tc_future_get(void *h, void *destination) {
    TcFuture *f = (TcFuture *)h;
    int error;
    mutex_lock(&f->lock);
    while (!f->complete)
        cond_wait(&f->ready, &f->lock);
    if (destination && f->size)
        memcpy(destination, f->data, f->size);
    error = f->error;
    mutex_unlock(&f->lock);
    return error;
}
int32_t tc_future_then(void *h, void *pool, TcAction action, void *argument) {
    TcFuture *f = (TcFuture *)h;
    TcContinuation *k = (TcContinuation *)malloc(sizeof(*k));
    if (!k)
        return 2;
    k->pool = (TcPool *)pool;
    k->action = action;
    k->argument = argument;
    mutex_lock(&f->lock);
    if (f->complete) {
        mutex_unlock(&f->lock);
        free(k);
        if (!pool || tc_pool_submit(pool, action, argument))
            action(argument);
        return 0;
    }
    k->next = f->continuations;
    f->continuations = k;
    mutex_unlock(&f->lock);
    return 0;
}
typedef struct TcChannel {
    TcMutex lock;
    TcCond readable, writable;
    size_t capacity, element_size, head, count;
    int closed;
    unsigned char *data;
} TcChannel;
void *tc_channel_create(uint64_t capacity, uint64_t size) {
    TcChannel *c;
    if (!capacity || !size || size > SIZE_MAX || capacity > SIZE_MAX / size)
        return NULL;
    c = (TcChannel *)calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    c->data = (unsigned char *)malloc((size_t)(capacity * size));
    if (!c->data) {
        free(c);
        return NULL;
    }
    c->capacity = (size_t)capacity;
    c->element_size = (size_t)size;
    mutex_init(&c->lock);
    cond_init(&c->readable);
    cond_init(&c->writable);
    return c;
}
int32_t tc_channel_send(void *h, void *value) {
    TcChannel *c = (TcChannel *)h;
    size_t index;
    mutex_lock(&c->lock);
    while (c->count == c->capacity && !c->closed)
        cond_wait(&c->writable, &c->lock);
    if (c->closed) {
        mutex_unlock(&c->lock);
        return 1;
    }
    index = (c->head + c->count) % c->capacity;
    memcpy(c->data + index * c->element_size, value, c->element_size);
    c->count++;
    cond_signal(&c->readable);
    mutex_unlock(&c->lock);
    return 0;
}
int32_t tc_channel_receive(void *h, void *destination) {
    TcChannel *c = (TcChannel *)h;
    mutex_lock(&c->lock);
    while (!c->count && !c->closed)
        cond_wait(&c->readable, &c->lock);
    if (!c->count) {
        mutex_unlock(&c->lock);
        return 1;
    }
    memcpy(destination, c->data + c->head * c->element_size, c->element_size);
    c->head = (c->head + 1) % c->capacity;
    c->count--;
    cond_signal(&c->writable);
    mutex_unlock(&c->lock);
    return 0;
}
void tc_channel_close(void *h) {
    TcChannel *c = (TcChannel *)h;
    mutex_lock(&c->lock);
    c->closed = 1;
    cond_broadcast(&c->readable);
    cond_broadcast(&c->writable);
    mutex_unlock(&c->lock);
}
void tc_channel_destroy(void *h) {
    TcChannel *c = (TcChannel *)h;
    if (c) {
        tc_channel_close(c);
        cond_dispose(&c->readable);
        cond_dispose(&c->writable);
        mutex_dispose(&c->lock);
        free(c->data);
        free(c);
    }
}

/* One scheduler per generated program. main starts it before user code and waits
   for all child tasks before releasing the executable's code and runtime. */
static TcPool *tc_default_pool;
static TcMutex tc_task_lock;
static TcCond tc_tasks_finished;
static uint64_t tc_pending_tasks;
void tc_scheduler_start(int32_t workers) {
    if (tc_default_pool)
        return;
    mutex_init(&tc_task_lock);
    cond_init(&tc_tasks_finished);
    tc_pending_tasks = 0;
    tc_default_pool = (TcPool *)tc_pool_create(workers);
    if (!tc_default_pool)
        abort();
}
void *tc_scheduler_pool(void) {
    return tc_default_pool;
}
void tc_task_begin(void) {
    mutex_lock(&tc_task_lock);
    tc_pending_tasks++;
    mutex_unlock(&tc_task_lock);
}
void tc_task_end(void) {
    mutex_lock(&tc_task_lock);
    if (--tc_pending_tasks == 0)
        cond_broadcast(&tc_tasks_finished);
    mutex_unlock(&tc_task_lock);
}
void tc_scheduler_shutdown(void) {
    if (!tc_default_pool)
        return;
    mutex_lock(&tc_task_lock);
    while (tc_pending_tasks)
        cond_wait(&tc_tasks_finished, &tc_task_lock);
    mutex_unlock(&tc_task_lock);
    tc_pool_destroy(tc_default_pool);
    tc_default_pool = NULL;
    cond_dispose(&tc_tasks_finished);
    mutex_dispose(&tc_task_lock);
}
