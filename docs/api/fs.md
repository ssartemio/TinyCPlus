# API reference

Source: `std/fs.tc`

Module: `std.fs`

## tc_file_read_all

```c
string tc_file_read_all(string path, i32* error);
```

## tc_file_write_all

```c
i32 tc_file_write_all(string path, string contents);
```

## tc_file_write_atomic

```c
i32 tc_file_write_atomic(string path, string contents);
```

## tc_file_exists

```c
i32 tc_file_exists(string path);
```

## tc_file_remove

```c
i32 tc_file_remove(string path);
```

## tc_file_open

```c
void* tc_file_open(string path, string mode);
```

## tc_file_read

```c
i64 tc_file_read(void* handle, void* data, u64 length);
```

## tc_file_write

```c
i64 tc_file_write(void* handle, void* data, u64 length);
```

## tc_file_close

```c
i32 tc_file_close(void* handle);
```

## File

```c
class File
```

### File.handle

```c
void* handle;
```

### File.open

```c
static (File, i32) open(string path, string mode = "rb");
```

### File.readAll

```c
static (OwnedString, i32) readAll(string path);
```

### File.writeAll

```c
static i32 writeAll(string path, string contents);
```

### File.writeAtomic

Writes a sibling temporary file, flushes it, then replaces the destination.

```c
static i32 writeAtomic(string path, string contents);
```

### File.exists

```c
static bool exists(string path);
```

### File.remove

```c
static i32 remove(string path);
```

### File.read

```c
i64 read(Slice<u8> destination);
```

### File.write

```c
i64 write(Slice<u8> source);
```

### File.close

```c
i32 close();
```

