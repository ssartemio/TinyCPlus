# API reference

Source: `std/collections.tc`

Module: `std.collections`

## Array

A manually owned contiguous array. Copies alias the same allocation.
Call destroy exactly once after the final use of the allocation.

```c
class Array<T>
```

### Array.data

```c
T* data;
```

### Array.length

```c
u64 length;
```

### Array.capacity

```c
u64 capacity;
```

### Array.create

```c
static Array<T> create();
```

### Array.reserve

```c
void reserve(u64 required);
```

### Array.push

```c
void push(T value);
```

### Array.pop

```c
T pop();
```

### Array.get

```c
T get(u64 index);
```

### Array.set

```c
void set(u64 index, T value);
```

### Array.clear

```c
void clear();
```

### Array.destroy

```c
void destroy();
```

