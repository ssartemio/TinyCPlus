/* Adversarial cases that ordinary small loopback transfers cannot exercise. */
#include "../runtime/concurrent.c"
#include "../runtime/net.c"
#include <assert.h>

typedef struct Reader { int64_t socket; char *bytes; uint64_t size; } Reader;
static void read_all(void *opaque) {
    Reader *reader = (Reader *)opaque;
    int32_t error;
    assert(tc_socket_transfer(reader->socket, reader->bytes, reader->size, 0, 1, &error) == (int64_t)reader->size);
    assert(error == 0);
}
static void gate(void *opaque) { tc_semaphore_wait(opaque); }
static void count_job(void *opaque) { tc_atomic_add(opaque, 1); }
int main(void) {
    void *pool, *semaphore, *count, *future, *thread;
    void *handles[2], *token;
    int32_t error;
    int64_t listener, client, peer, progress;
    int small = 4096;
    uint64_t size = 2u * 1024u * 1024u;
    char *source = (char *)malloc((size_t)size), *destination = (char *)malloc((size_t)size);
    Reader reader;
    assert(source && destination);
    memset(source, 0xa5, (size_t)size);
    tc_scheduler_start(1); tc_io_start();
    listener = tc_tcp_listen(TC_STRING("127.0.0.1"), 0, 4, &error); assert(!error);
    client = tc_tcp_connect(TC_STRING("localhost"), tc_socket_port(listener), &error); assert(!error);
    peer = tc_tcp_accept(listener, &error); assert(!error);
    setsockopt((TcSocket)client, SOL_SOCKET, SO_SNDBUF, (const char *)&small, sizeof(small));
    reader.socket = peer; reader.bytes = destination; reader.size = size;
    thread = tc_thread_start(read_all, &reader); assert(thread);
    assert(tc_socket_transfer(client, source, size, 1, 1, &error) == (int64_t)size && !error);
    tc_thread_destroy(thread); assert(!memcmp(source, destination, (size_t)size));
    /* Read-exact accumulates fragmented writes and reports progress at clean EOF. */
    future = tc_socket_read_exact_async(peer, destination, 6, 1000, NULL);
    assert(tc_socket_write(client, source, 2) == 2);
    tc_sleep_ms(5); assert(!tc_future_ready(future));
    assert(tc_socket_write(client, source + 2, 4) == 4);
    assert(tc_future_get(future, &progress) == 0 && progress == 6); tc_future_release(future);
    /* Stop reading: an all-write must expose partial progress when its deadline expires. */
    future = tc_socket_write_async(client, source, size, 30, NULL);
    assert(tc_future_get(future, &progress) == 4 && progress > 0 && progress < (int64_t)size);
    tc_future_release(future); tc_socket_close(client); tc_socket_close(peer); tc_socket_close(listener);
    /* Deterministic saturation: a blocked worker cannot consume queued jobs. */
    pool = tc_pool_create(1); semaphore = tc_semaphore_create(0); count = tc_atomic_create(0);
    assert(pool && semaphore && count);
    assert(tc_pool_limits(pool, 2, 16) == 0);
    assert(tc_pool_submit(pool, gate, semaphore) == 0);
    /* Wait until the gate job is executing, inspecting only under the pool lock. */
    for (;;) {
        int executing;
        mutex_lock(&((TcPool *)pool)->lock); executing = ((TcPool *)pool)->pending == 0;
        mutex_unlock(&((TcPool *)pool)->lock);
        if (executing) break;
        tc_sleep_ms(1);
    }
    assert(tc_pool_submit_sized(pool, count_job, count, 8) == 0);
    assert(tc_pool_submit_sized(pool, count_job, count, 9) == 8);
    assert(tc_pool_submit_sized(pool, count_job, count, 8) == 0);
    assert(tc_pool_submit(pool, count_job, count) == 8);
    tc_semaphore_post(semaphore); tc_pool_destroy(pool);
    assert(tc_atomic_load(count) == 2); tc_atomic_destroy(count); tc_semaphore_destroy(semaphore);
    handles[0] = tc_future_create(0); handles[1] = tc_future_create(0);
    assert(tc_future_wait_any(handles, 2, 2, NULL, &error) == -1 && error == 4);
    tc_future_complete(handles[1], NULL, 0);
    assert(tc_future_wait_any(handles, 2, 10, NULL, &error) == 1 && !error);
    token = tc_atomic_create(1);
    assert(tc_future_wait_any(handles, 2, 10, token, &error) == -1 && error == 1);
    tc_atomic_destroy(token); tc_future_release(handles[0]); tc_future_release(handles[1]);
    tc_io_shutdown(); tc_scheduler_shutdown(); free(source); free(destination);
    puts("Review runtime: backpressure, exact reads, partial progress, queue limits, race timeout/cancellation passed");
    return 0;
}
