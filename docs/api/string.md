# API reference

Source: `std/string.tc`

Module: `std.string`

## tc_string_view

```c
string tc_string_view(const char* data, u64 length);
```

## tc_string_copy

```c
string tc_string_copy(string source);
```

## tc_string_concat

```c
string tc_string_concat(string first, string second);
```

## tc_string_free

```c
void tc_string_free(string value);
```

## tc_string_find

```c
i64 tc_string_find(string text, string pattern);
```

## tc_string_starts

```c
i32 tc_string_starts(string text, string prefix);
```

## tc_string_ends

```c
i32 tc_string_ends(string text, string suffix);
```

## tc_parse_int

```c
i64 tc_parse_int(string text, i32* error);
```

## tc_format_int

```c
string tc_format_int(i64 value);
```

## tc_format_double

```c
string tc_format_double(double value);
```

## tc_bytes_copy

```c
void tc_bytes_copy(char* destination, string source);
```

## OwnedString

Explicit ownership. view() borrows bytes; destroy invalidates all views.

```c
class OwnedString
```

### OwnedString.value

```c
string value;
```

### OwnedString.view

```c
string view();
```

### OwnedString.destroy

```c
void destroy();
```

## String

```c
class String
```

### String.copy

```c
static OwnedString copy(string source);
```

### String.concat

```c
static OwnedString concat(string first, string second);
```

### String.fromDouble

Shortest decimal text that reads back as exactly the same double.

```c
static OwnedString fromDouble(double value);
```

### String.fromInt

```c
static OwnedString fromInt(i64 value);
```

### String.parseInt

```c
static (i64, i32) parseInt(string text);
```

### String.indexOf

```c
static i64 indexOf(string text, string pattern);
```

### String.contains

```c
static bool contains(string text, string pattern);
```

### String.startsWith

```c
static bool startsWith(string text, string prefix);
```

### String.endsWith

```c
static bool endsWith(string text, string suffix);
```

## StringBuilder

A growable byte buffer for building text without quadratic copying.
view() borrows the current bytes; it is invalidated by the next append,
clear or destroy. Copies alias the same buffer. Call destroy exactly once.

```c
class StringBuilder
```

### StringBuilder.data

```c
char* data;
```

### StringBuilder.length

```c
u64 length;
```

### StringBuilder.capacity

```c
u64 capacity;
```

### StringBuilder.create

```c
static StringBuilder create();
```

### StringBuilder.reserve

Ensures room for extra more bytes plus a terminating NUL.

```c
void reserve(u64 extra);
```

### StringBuilder.append

```c
void append(string text);
```

### StringBuilder.appendChar

```c
void appendChar(char value);
```

### StringBuilder.appendUnsigned

```c
void appendUnsigned(u64 value);
```

### StringBuilder.appendInt

```c
void appendInt(i64 value);
```

### StringBuilder.appendHex

Lowercase hexadecimal without prefix.

```c
void appendHex(u64 value);
```

### StringBuilder.appendDouble

```c
void appendDouble(double value);
```

### StringBuilder.view

Borrowed view of the current contents.

```c
string view();
```

### StringBuilder.toOwned

Independent copy that outlives the builder.

```c
OwnedString toOwned();
```

### StringBuilder.clear

```c
void clear();
```

### StringBuilder.destroy

```c
void destroy();
```

