# API reference

Source: `std/collections.tc`

Module: `std.collections`

## Array

A manually owned contiguous array. Copies alias the same allocation.
Call destroy exactly once after the final use of the allocation.

```c
class Array<T>
```

### Array.version

```c
u64 version;
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

### Array.tryPop

Empty/absent results contain a zero-initialized value and false.

```c
(T, bool) tryPop();
```

### Array.tryPeek

```c
(T, bool) tryPeek();
```

### Array.tryGet

```c
(T, bool) tryGet(u64 index);
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

### Array.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.
Transfers storage, empties this owner and invalidates its iterators.

```c
Array<T> take();
```

### Array.clone

```c
Array<T> clone();
```

### Array.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
ArrayIterator<T> iterator();
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

### Map.version

```c
u64 version;
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

### Map.tryGet

Returns (value, true), or (zero value, false) when key is absent.

```c
(V, bool) tryGet(K key);
```

### Map.get

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

### Map.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.
Transfers storage, empties this owner and invalidates its iterators.

```c
Map<K, V> take();
```

### Map.clone

```c
Map<K, V> clone();
```

### Map.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
MapIterator<K, V> iterator();
```

## Set

Hash set with Map's key restrictions and borrowed string keys.
Copies alias storage; destroy once. Elements are never destroyed recursively.

```c
class Set<T>
```

### Set.version

```c
u64 version;
```

### Set.entries

```c
Map<T, bool> entries;
```

### Set.length

```c
u64 length;
```

### Set.create

```c
static Set<T> create();
```

### Set.add

```c
bool add(T value);
```

### Set.contains

```c
bool contains(T value);
```

### Set.remove

```c
bool remove(T value);
```

### Set.values

Appends elements in unspecified order. Destination must not alias storage.

```c
void values(Array<T>* destination);
```

### Set.clear

```c
void clear();
```

### Set.destroy

```c
void destroy();
```

### Set.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.

```c
Set<T> take();
```

### Set.clone

```c
Set<T> clone();
```

### Set.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
SetIterator<T> iterator();
```

## Deque

Circular buffer. End operations are O(1) amortized; reserve is O(n).
Growth invalidates pointers. Copies alias storage; destroy once, not elements.
Empty pops and invalid indices assert, as in Array. Not synchronized.

```c
class Deque<T>
```

### Deque.version

```c
u64 version;
```

### Deque.data

```c
T* data;
```

### Deque.length

```c
u64 length;
```

### Deque.capacity

```c
u64 capacity;
```

### Deque.head

```c
u64 head;
```

### Deque.create

```c
static Deque<T> create();
```

### Deque.slot

Maps a logical offset without overflowing head + offset.

```c
u64 slot(u64 offset);
```

### Deque.reserve

```c
void reserve(u64 required);
```

### Deque.pushBack

```c
void pushBack(T value);
```

### Deque.pushFront

```c
void pushFront(T value);
```

### Deque.tryPopFront

```c
(T, bool) tryPopFront();
```

### Deque.tryPopBack

```c
(T, bool) tryPopBack();
```

### Deque.tryPeek

```c
(T, bool) tryPeek();
```

### Deque.tryGet

```c
(T, bool) tryGet(u64 index);
```

### Deque.popFront

```c
T popFront();
```

### Deque.popBack

```c
T popBack();
```

### Deque.get

```c
T get(u64 index);
```

### Deque.set

```c
void set(u64 index, T value);
```

### Deque.clear

```c
void clear();
```

### Deque.destroy

```c
void destroy();
```

### Deque.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.
Transfers storage, empties this owner and invalidates its iterators.

```c
Deque<T> take();
```

### Deque.clone

```c
Deque<T> clone();
```

### Deque.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
DequeIterator<T> iterator();
```

## PriorityQueue

Binary heap. before(a,b) must define a strict weak order: true puts a first.
Use a < b for a min-heap, a > b for a max-heap. Equal priorities are not stable.
Construct with create before push. push/pop are O(log n), peek O(1).
Copies alias storage; destroy once. Growth invalidates element pointers.

```c
class PriorityQueue<T>
```

### PriorityQueue.version

```c
u64 version;
```

### PriorityQueue.items

```c
Array<T> items;
```

### PriorityQueue.before

```c
func<bool(T, T)> before;
```

### PriorityQueue.initialized

```c
bool initialized;
```

### PriorityQueue.length

```c
u64 length;
```

### PriorityQueue.create

```c
static PriorityQueue<T> create(func<bool(T, T)> compare);
```

### PriorityQueue.push

```c
void push(T value);
```

### PriorityQueue.tryPeek

```c
(T, bool) tryPeek();
```

### PriorityQueue.tryPop

```c
(T, bool) tryPop();
```

### PriorityQueue.peek

```c
T peek();
```

### PriorityQueue.pop

```c
T pop();
```

### PriorityQueue.clear

```c
void clear();
```

### PriorityQueue.destroy

```c
void destroy();
```

### PriorityQueue.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.

```c
PriorityQueue<T> take();
```

### PriorityQueue.clone

```c
PriorityQueue<T> clone();
```

### PriorityQueue.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
PriorityQueueIterator<T> iterator();
```

## BitSet

Fixed-size set of bit indices [0, length). New bits are initially clear.
Out-of-range indices and unequal-size binary operations assert.
Copies alias storage; destroy once. Binary operations mutate this set.

```c
class BitSet
```

### BitSet.version

```c
u64 version;
```

### BitSet.data

```c
u64* data;
```

### BitSet.length

```c
u64 length;
```

### BitSet.wordCount

```c
u64 wordCount;
```

### BitSet.create

```c
static BitSet create(u64 bits);
```

### BitSet.contains

```c
bool contains(u64 index);
```

### BitSet.set

```c
void set(u64 index);
```

### BitSet.reset

```c
void reset(u64 index);
```

### BitSet.count

```c
u64 count();
```

### BitSet.unionWith

```c
void unionWith(BitSet* other);
```

### BitSet.intersectWith

```c
void intersectWith(BitSet* other);
```

### BitSet.differenceWith

```c
void differenceWith(BitSet* other);
```

### BitSet.clear

```c
void clear();
```

### BitSet.destroy

```c
void destroy();
```

### BitSet.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.
Transfers storage, empties this owner and invalidates its iterators.

```c
BitSet take();
```

### BitSet.clone

```c
BitSet clone();
```

### BitSet.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
BitSetIterator iterator();
```

## OrderedMap

Insertion-ordered hash map. Updating a key preserves its position;
removing then reinserting appends it. Lookup/put are amortized O(1),
removal O(n) to preserve dense order. Indexed access is O(1).
Same key, borrowing and ownership rules as Map. No synchronized access.

```c
class OrderedMap<K, V>
```

### OrderedMap.version

```c
u64 version;
```

### OrderedMap.positions

```c
Map<K, u64> positions;
```

### OrderedMap.keyData

```c
Array<K> keyData;
```

### OrderedMap.valueData

```c
Array<V> valueData;
```

### OrderedMap.length

```c
u64 length;
```

### OrderedMap.create

```c
static OrderedMap<K, V> create();
```

### OrderedMap.put

```c
bool put(K key, V value);
```

### OrderedMap.contains

```c
bool contains(K key);
```

### OrderedMap.tryGet

```c
(V, bool) tryGet(K key);
```

### OrderedMap.get

```c
(V, bool) get(K key);
```

### OrderedMap.getOr

```c
V getOr(K key, V fallback);
```

### OrderedMap.keyAt

```c
K keyAt(u64 index);
```

### OrderedMap.valueAt

```c
V valueAt(u64 index);
```

### OrderedMap.remove

```c
bool remove(K key);
```

### OrderedMap.keys

Appends a snapshot in insertion order; destination must not alias storage.

```c
void keys(Array<K>* destination);
```

### OrderedMap.values

```c
void values(Array<V>* destination);
```

### OrderedMap.clear

```c
void clear();
```

### OrderedMap.destroy

```c
void destroy();
```

### OrderedMap.take

Copies backing storage; elements and borrowed keys are copied by value.
The caller owns the result. Source iterators remain valid.

```c
OrderedMap<K, V> take();
```

### OrderedMap.clone

```c
OrderedMap<K, V> clone();
```

### OrderedMap.iterator

Borrows this owner; keep it alive and unmodified until iteration ends.

```c
OrderedMapIterator<K, V> iterator();
```

## ArrayIterator

Borrowed cursor for Array. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class ArrayIterator<T>
```

### ArrayIterator.owner

```c
Array<T>* owner;
```

### ArrayIterator.expectedVersion

```c
u64 expectedVersion;
```

### ArrayIterator.position

```c
u64 position;
```

### ArrayIterator.ordinal

```c
u64 ordinal;
```

### ArrayIterator.next

```c
(u64, T, bool) next();
```

## MapIterator

Borrowed cursor for Map. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class MapIterator<K, V>
```

### MapIterator.owner

```c
Map<K, V>* owner;
```

### MapIterator.expectedVersion

```c
u64 expectedVersion;
```

### MapIterator.position

```c
u64 position;
```

### MapIterator.ordinal

```c
u64 ordinal;
```

### MapIterator.next

```c
(K, V, bool) next();
```

## SetIterator

Borrowed cursor for Set. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class SetIterator<T>
```

### SetIterator.owner

```c
Set<T>* owner;
```

### SetIterator.expectedVersion

```c
u64 expectedVersion;
```

### SetIterator.position

```c
u64 position;
```

### SetIterator.ordinal

```c
u64 ordinal;
```

### SetIterator.next

```c
(u64, T, bool) next();
```

## DequeIterator

Borrowed cursor for Deque. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class DequeIterator<T>
```

### DequeIterator.owner

```c
Deque<T>* owner;
```

### DequeIterator.expectedVersion

```c
u64 expectedVersion;
```

### DequeIterator.position

```c
u64 position;
```

### DequeIterator.ordinal

```c
u64 ordinal;
```

### DequeIterator.next

```c
(u64, T, bool) next();
```

## PriorityQueueIterator

Borrowed cursor for PriorityQueue. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class PriorityQueueIterator<T>
```

### PriorityQueueIterator.owner

```c
PriorityQueue<T>* owner;
```

### PriorityQueueIterator.expectedVersion

```c
u64 expectedVersion;
```

### PriorityQueueIterator.position

```c
u64 position;
```

### PriorityQueueIterator.ordinal

```c
u64 ordinal;
```

### PriorityQueueIterator.next

```c
(u64, T, bool) next();
```

## BitSetIterator

Borrowed cursor for BitSet. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class BitSetIterator
```

### BitSetIterator.owner

```c
BitSet* owner;
```

### BitSetIterator.expectedVersion

```c
u64 expectedVersion;
```

### BitSetIterator.position

```c
u64 position;
```

### BitSetIterator.ordinal

```c
u64 ordinal;
```

### BitSetIterator.next

```c
(u64, u64, bool) next();
```

## OrderedMapIterator

Borrowed cursor for OrderedMap. No allocation or destroy is required.
next returns (key/index, value, available); exhausted cursors stay exhausted.
Mutating owner methods invalidate the cursor, even when they do no work.

```c
class OrderedMapIterator<K, V>
```

### OrderedMapIterator.owner

```c
OrderedMap<K, V>* owner;
```

### OrderedMapIterator.expectedVersion

```c
u64 expectedVersion;
```

### OrderedMapIterator.position

```c
u64 position;
```

### OrderedMapIterator.ordinal

```c
u64 ordinal;
```

### OrderedMapIterator.next

```c
(K, V, bool) next();
```

