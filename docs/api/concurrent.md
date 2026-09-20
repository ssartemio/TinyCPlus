# API reference

Source: `std/concurrent.tc`

Module: `std.concurrent`

## tc_thread_start

```c
void* tc_thread_start(func<void(void*)> action, void* argument);
```

## tc_thread_join

```c
i32 tc_thread_join(void* thread);
```

## tc_thread_destroy

```c
void tc_thread_destroy(void* thread);
```

## tc_sleep_ms

```c
void tc_sleep_ms(u64 milliseconds);
```

## tc_clock_ms

```c
u64 tc_clock_ms();
```

## tc_mutex_create

```c
void* tc_mutex_create();
```

## tc_mutex_lock

```c
void tc_mutex_lock(void* mutex);
```

## tc_mutex_unlock

```c
void tc_mutex_unlock(void* mutex);
```

## tc_mutex_destroy

```c
void tc_mutex_destroy(void* mutex);
```

## tc_condition_create

```c
void* tc_condition_create();
```

## tc_condition_wait

```c
void tc_condition_wait(void* condition, void* mutex);
```

## tc_condition_signal

```c
void tc_condition_signal(void* condition);
```

## tc_condition_broadcast

```c
void tc_condition_broadcast(void* condition);
```

## tc_condition_destroy

```c
void tc_condition_destroy(void* condition);
```

## tc_semaphore_create

```c
void* tc_semaphore_create(u64 count);
```

## tc_semaphore_wait

```c
void tc_semaphore_wait(void* semaphore);
```

## tc_semaphore_post

```c
void tc_semaphore_post(void* semaphore);
```

## tc_semaphore_destroy

```c
void tc_semaphore_destroy(void* semaphore);
```

## tc_event_create

```c
void* tc_event_create();
```

## tc_event_wait

```c
void tc_event_wait(void* event);
```

## tc_event_set

```c
void tc_event_set(void* event);
```

## tc_event_reset

```c
void tc_event_reset(void* event);
```

## tc_event_destroy

```c
void tc_event_destroy(void* event);
```

## tc_atomic_create

```c
void* tc_atomic_create(i64 value);
```

## tc_atomic_load

```c
i64 tc_atomic_load(void* atomic);
```

## tc_atomic_store

```c
void tc_atomic_store(void* atomic, i64 value);
```

## tc_atomic_add

```c
i64 tc_atomic_add(void* atomic, i64 value);
```

## tc_atomic_destroy

```c
void tc_atomic_destroy(void* atomic);
```

## tc_pool_create

```c
void* tc_pool_create(i32 workers);
```

## tc_pool_submit

```c
i32 tc_pool_submit(void* pool, func<void(void*)> action, void* argument);
```

## tc_pool_destroy

```c
void tc_pool_destroy(void* pool);
```

## tc_future_create

```c
void* tc_future_create(u64 size);
```

## tc_future_retain

```c
void tc_future_retain(void* future);
```

## tc_future_release

```c
void tc_future_release(void* future);
```

## tc_future_complete

```c
i32 tc_future_complete(void* future, void* value, i32 error);
```

## tc_future_ready

```c
i32 tc_future_ready(void* future);
```

## tc_future_get

```c
i32 tc_future_get(void* future, void* destination);
```

## tc_future_then

```c
i32 tc_future_then(void* future, void* pool, func<void(void*)> continuation, void* argument);
```

## tc_channel_create

```c
void* tc_channel_create(u64 capacity, u64 element_size);
```

## tc_channel_send

```c
i32 tc_channel_send(void* channel, void* value);
```

## tc_channel_receive

```c
i32 tc_channel_receive(void* channel, void* destination);
```

## tc_channel_close

```c
void tc_channel_close(void* channel);
```

## tc_channel_destroy

```c
void tc_channel_destroy(void* channel);
```

## Thread

An OS thread. join releases its native handle. Join before borrowed data expires.

```c
class Thread
```

### Thread.handle

```c
void* handle;
```

### Thread.start

```c
static Thread start(func<void(void*)> action, void* argument = null);
```

### Thread.join

```c
i32 join();
```

### Thread.destroy

```c
void destroy();
```

## Mutex

```c
class Mutex
```

### Mutex.handle

```c
void* handle;
```

### Mutex.create

```c
static Mutex create();
```

### Mutex.lock

```c
void lock();
```

### Mutex.unlock

```c
void unlock();
```

### Mutex.destroy

```c
void destroy();
```

## Condition

```c
class Condition
```

### Condition.handle

```c
void* handle;
```

### Condition.create

```c
static Condition create();
```

### Condition.wait

```c
void wait(Mutex* mutex);
```

### Condition.signal

```c
void signal();
```

### Condition.broadcast

```c
void broadcast();
```

### Condition.destroy

```c
void destroy();
```

## Semaphore

```c
class Semaphore
```

### Semaphore.handle

```c
void* handle;
```

### Semaphore.create

```c
static Semaphore create(u64 count = 0);
```

### Semaphore.wait

```c
void wait();
```

### Semaphore.post

```c
void post();
```

### Semaphore.destroy

```c
void destroy();
```

## Event

```c
class Event
```

### Event.handle

```c
void* handle;
```

### Event.create

```c
static Event create();
```

### Event.wait

```c
void wait();
```

### Event.set

```c
void set();
```

### Event.reset

```c
void reset();
```

### Event.destroy

```c
void destroy();
```

## Atomic

Sequentially consistent integer operations, using a portable mutex fallback.

```c
class Atomic
```

### Atomic.handle

```c
void* handle;
```

### Atomic.create

```c
static Atomic create(i64 value = 0);
```

### Atomic.load

```c
i64 load();
```

### Atomic.store

```c
void store(i64 value);
```

### Atomic.add

```c
i64 add(i64 value);
```

### Atomic.destroy

```c
void destroy();
```

## CancellationToken

```c
class CancellationToken
```

### CancellationToken.state

```c
Atomic state;
```

### CancellationToken.create

```c
static CancellationToken create();
```

### CancellationToken.cancel

```c
void cancel();
```

### CancellationToken.cancelled

```c
bool cancelled();
```

### CancellationToken.destroy

```c
void destroy();
```

## WorkerPool

```c
class WorkerPool
```

### WorkerPool.handle

```c
void* handle;
```

### WorkerPool.create

```c
static WorkerPool create(i32 workers = 4);
```

### WorkerPool.submit

```c
i32 submit(func<void(void*)> action, void* argument);
```

### WorkerPool.destroy

```c
void destroy();
```

## Channel

Buffered channel. Close wakes blocked operations; join users before destroy.

```c
class Channel<T>
```

### Channel.handle

```c
void* handle;
```

### Channel.create

```c
static Channel<T> create(u64 capacity);
```

### Channel.send

```c
i32 send(T value);
```

### Channel.receive

```c
(T, i32) receive();
```

### Channel.close

```c
void close();
```

### Channel.destroy

```c
void destroy();
```

## Future

```c
class Future<T>
```

### Future.handle

```c
void* handle;
```

### Future.create

```c
static Future<T> create();
```

### Future.ready

```c
bool ready();
```

### Future.complete

```c
i32 complete(T value, i32 error = 0);
```

### Future.get

```c
(T, i32) get();
```

### Future.retain

```c
void retain();
```

### Future.destroy

```c
void destroy();
```

## TaskGroup

Owns retained child handles. wait/destroy must run outside the child worker pool.
Children may poll cancellation(); wait returns the first error in insertion order.

```c
class TaskGroup<T>
```

### TaskGroup.children

```c
Array<void*> children;
```

### TaskGroup.token

```c
CancellationToken token;
```

### TaskGroup.create

```c
static TaskGroup<T> create();
```

### TaskGroup.add

```c
void add(Task<T> task);
```

### TaskGroup.cancellation

```c
CancellationToken* cancellation();
```

### TaskGroup.cancel

```c
void cancel();
```

### TaskGroup.wait

```c
i32 wait();
```

### TaskGroup.destroy

```c
void destroy();
```

