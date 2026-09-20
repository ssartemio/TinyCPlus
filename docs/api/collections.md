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

## Map

A manually owned hash map with open addressing and linear probing.
Keys may be integers, char, bool, enums, pointers or strings; they are
hashed with the builtin hash() and compared with ==. String keys are views:
the map does not copy their bytes, so the text must outlive the entry.
Copies alias the same storage. Call destroy exactly once after the final use.

```c
class Map<K, V>
```

### Map.keyData

```c
K* keyData;
```

### Map.valueData

```c
V* valueData;
```

### Map.stateData

```c
u8* stateData;
```

### Map.length

```c
u64 length;
```

### Map.capacity

```c
u64 capacity;
```

### Map.tombstones

```c
u64 tombstones;
```

### Map.create

```c
static Map<K, V> create();
```

### Map.find

Returns the slot holding key, or capacity when the key is absent.

```c
u64 find(K key);
```

### Map.rehash

```c
void rehash(u64 next);
```

### Map.insert

Stores a key known to be absent; capacity must already have room.

```c
void insert(K key, V value);
```

### Map.put

Inserts key or replaces its value. Returns true when the key is new.

```c
bool put(K key, V value);
```

### Map.get

Returns (value, true), or (zero value, false) when key is absent.

```c
(V, bool) get(K key);
```

### Map.getOr

```c
V getOr(K key, V fallback);
```

### Map.contains

```c
bool contains(K key);
```

### Map.remove

Removes key. Returns true when it was present.

```c
bool remove(K key);
```

### Map.keys

Appends every key to destination, in unspecified order.

```c
void keys(Array<K>* destination);
```

### Map.values

Appends every value to destination, in the same order as keys().

```c
void values(Array<V>* destination);
```

### Map.clear

```c
void clear();
```

### Map.destroy

```c
void destroy();
```

