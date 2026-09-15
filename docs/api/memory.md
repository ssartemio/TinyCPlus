# API reference

Source: `std/memory.tc`

Module: `std.memory`

## Memory

```c
class Memory
```

### Memory.allocate

```c
static void* allocate(u64 bytes);
```

### Memory.zeroed

```c
static void* zeroed(u64 count, u64 size);
```

### Memory.resize

```c
static void* resize(void* pointer, u64 bytes);
```

### Memory.release

```c
static void release(void* pointer);
```

