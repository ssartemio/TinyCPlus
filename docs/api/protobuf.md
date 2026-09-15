# API reference

Source: `std/protobuf.tc`

Module: `std.protobuf`

## tc_pb_valid_utf8

```c
i32 tc_pb_valid_utf8(string text);
```

## tc_pb_writer

```c
void* tc_pb_writer();
```

## tc_pb_writer_destroy

```c
void tc_pb_writer_destroy(void* writer);
```

## tc_pb_write_varint

```c
void tc_pb_write_varint(void* writer, u64 value);
```

## tc_pb_write_fixed

```c
void tc_pb_write_fixed(void* writer, u64 value, i32 count);
```

## tc_pb_write_bytes

```c
void tc_pb_write_bytes(void* writer, string value);
```

## tc_pb_writer_error

```c
i32 tc_pb_writer_error(void* writer);
```

## tc_pb_writer_take

```c
string tc_pb_writer_take(void* writer);
```

## tc_pb_reader

```c
void* tc_pb_reader(string input);
```

## tc_pb_reader_destroy

```c
void tc_pb_reader_destroy(void* reader);
```

## tc_pb_reader_error

```c
i32 tc_pb_reader_error(void* reader);
```

## tc_pb_reader_fail

```c
void tc_pb_reader_fail(void* reader);
```

## tc_pb_reader_done

```c
i32 tc_pb_reader_done(void* reader);
```

## tc_pb_read_varint

```c
u64 tc_pb_read_varint(void* reader);
```

## tc_pb_read_fixed

```c
u64 tc_pb_read_fixed(void* reader, i32 count);
```

## tc_pb_read_bytes

```c
string tc_pb_read_bytes(void* reader);
```

## tc_pb_next

```c
u32 tc_pb_next(void* reader);
```

## tc_pb_wire

```c
u32 tc_pb_wire(void* reader);
```

## tc_pb_skip

```c
void tc_pb_skip(void* reader, u32 field);
```

## tc_pb_zigzag

```c
u64 tc_pb_zigzag(i64 value);
```

## tc_pb_unzigzag

```c
i64 tc_pb_unzigzag(u64 value);
```

## tc_pb_double_bits

```c
u64 tc_pb_double_bits(double value);
```

## tc_pb_float_bits

```c
u32 tc_pb_float_bits(float value);
```

## tc_pb_bits_double

```c
double tc_pb_bits_double(u64 bits);
```

## tc_pb_bits_float

```c
float tc_pb_bits_float(u32 bits);
```

## ProtoWriter

```c
class ProtoWriter
```

### ProtoWriter.handle

```c
void* handle;
```

### ProtoWriter.create

```c
static ProtoWriter create();
```

### ProtoWriter.tag

```c
void tag(u32 field, u32 wire);
```

### ProtoWriter.number

```c
void number(u64 value);
```

### ProtoWriter.fixed32

```c
void fixed32(u64 value);
```

### ProtoWriter.fixed64

```c
void fixed64(u64 value);
```

### ProtoWriter.bytes

```c
void bytes(string value);
```

### ProtoWriter.error

```c
i32 error();
```

### ProtoWriter.take

```c
OwnedString take();
```

### ProtoWriter.destroy

```c
void destroy();
```

## ProtoReader

```c
class ProtoReader
```

### ProtoReader.handle

```c
void* handle;
```

### ProtoReader.create

```c
static ProtoReader create(string input);
```

### ProtoReader.next

```c
u32 next();
```

### ProtoReader.wire

```c
u32 wire();
```

### ProtoReader.number

```c
u64 number();
```

### ProtoReader.fixed32

```c
u64 fixed32();
```

### ProtoReader.fixed64

```c
u64 fixed64();
```

### ProtoReader.bytes

```c
string bytes();
```

### ProtoReader.done

```c
bool done();
```

### ProtoReader.error

```c
i32 error();
```

### ProtoReader.fail

```c
void fail();
```

### ProtoReader.skip

```c
void skip(u32 field);
```

### ProtoReader.destroy

```c
void destroy();
```

