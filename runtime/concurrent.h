#ifndef TINY_CONCURRENT_H
#define TINY_CONCURRENT_H
#include <stdint.h>
#include <stddef.h>

typedef void (*TcAction)(void *);
typedef void (*TcTaskFn)(void *, void *);
void *tc_thread_start(TcAction action, void *argument);
int32_t tc_thread_join(void *thread);
void tc_thread_destroy(void *thread);
void tc_sleep_ms(uint64_t milliseconds);
uint64_t tc_clock_ms(void);
void *tc_mutex_create(void);
void tc_mutex_lock(void *mutex);
void tc_mutex_unlock(void *mutex);
void tc_mutex_destroy(void *mutex);
void *tc_condition_create(void);
void tc_condition_wait(void *condition, void *mutex);
void tc_condition_signal(void *condition);
void tc_condition_broadcast(void *condition);
void tc_condition_destroy(void *condition);
void *tc_semaphore_create(uint64_t count);
void tc_semaphore_wait(void *semaphore);
void tc_semaphore_post(void *semaphore);
void tc_semaphore_destroy(void *semaphore);
void *tc_event_create(void);
void tc_event_wait(void *event);
void tc_event_set(void *event);
void tc_event_reset(void *event);
void tc_event_destroy(void *event);
void *tc_atomic_create(int64_t value);
int64_t tc_atomic_load(void *atomic);
void tc_atomic_store(void *atomic, int64_t value);
int64_t tc_atomic_add(void *atomic, int64_t value);
void tc_atomic_destroy(void *atomic);
void *tc_pool_create(int32_t workers);
int32_t tc_pool_submit(void *pool, TcAction action, void *argument);
void tc_pool_destroy(void *pool);
void *tc_future_create(uint64_t size);
void tc_future_retain(void *future);
void tc_future_release(void *future);
int32_t tc_future_complete(void *future, void *value, int32_t error);
int32_t tc_future_ready(void *future);
int32_t tc_future_get(void *future, void *destination);
int32_t tc_future_then(void *future, void *pool, TcAction continuation, void *argument);
void *tc_channel_create(uint64_t capacity, uint64_t element_size);
int32_t tc_channel_send(void *channel, void *value);
int32_t tc_channel_receive(void *channel, void *destination);
void tc_channel_close(void *channel);
void tc_channel_destroy(void *channel);
void tc_scheduler_start(int32_t workers);
void *tc_scheduler_pool(void);
void tc_task_begin(void);
void tc_task_end(void);
void tc_scheduler_shutdown(void);
#endif
