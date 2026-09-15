#include "../runtime/concurrent.c"
#include <assert.h>
#include <stdio.h>

typedef struct Work {
    void *counter;
    void *channel;
    int count;
} Work;
static void increment(void *p) {
    Work *w = (Work *)p;
    int i;
    for (i = 0; i < w->count; i++)
        tc_atomic_add(w->counter, 1);
}
static void producer(void *p) {
    Work *w = (Work *)p;
    int i;
    for (i = 1; i <= w->count; i++)
        assert(tc_channel_send(w->channel, &i) == 0);
    tc_channel_close(w->channel);
}
static void future_job(void *p) {
    int value = 42;
    assert(tc_future_complete(p, &value, 0) == 0);
}
static void continuation(void *p) {
    tc_atomic_add(p, 1);
}
int main(void) {
    Work work;
    void *threads[4], *pool, *future;
    int i, sum = 0, x, error;
    uint64_t start = tc_clock_ms();
    work.counter = tc_atomic_create(0);
    work.count = 5000;
    assert(work.counter);
    for (i = 0; i < 4; i++) {
        threads[i] = tc_thread_start(increment, &work);
        assert(threads[i]);
    }
    for (i = 0; i < 4; i++) {
        assert(tc_thread_join(threads[i]) == 0);
        tc_thread_destroy(threads[i]);
    }
    assert(tc_atomic_load(work.counter) == 20000);
    work.channel = tc_channel_create(7, sizeof(int));
    work.count = 1000;
    assert(work.channel);
    threads[0] = tc_thread_start(producer, &work);
    assert(threads[0]);
    while ((error = tc_channel_receive(work.channel, &x)) == 0)
        sum += x;
    assert(error == 1 && sum == 500500);
    assert(tc_channel_send(work.channel, &x) == 1);
    tc_thread_destroy(threads[0]);
    tc_channel_destroy(work.channel);
    pool = tc_pool_create(1);
    future = tc_future_create(sizeof(int));
    assert(pool && future);
    assert(!tc_future_ready(future));
    assert(tc_future_then(future, pool, continuation, work.counter) == 0);
    assert(tc_pool_submit(pool, future_job, future) == 0);
    assert(tc_future_get(future, &x) == 0 && x == 42);
    tc_pool_destroy(pool);
    assert(tc_atomic_load(work.counter) == 20001);
    assert(tc_future_complete(future, &x, 0) == 1);
    tc_future_release(future);
    {
        void *e = tc_event_create();
        tc_event_set(e);
        tc_event_wait(e);
        tc_event_reset(e);
        tc_event_destroy(e);
    }
    {
        void *s = tc_semaphore_create(2);
        tc_semaphore_wait(s);
        tc_semaphore_wait(s);
        tc_semaphore_post(s);
        tc_semaphore_wait(s);
        tc_semaphore_destroy(s);
    }
    tc_atomic_destroy(work.counter);
    assert(tc_clock_ms() >= start);
    puts("runtime concurrency: 20000 atomic updates, channel backpressure, future continuation and "
         "lifecycle tests passed");
    return 0;
}
