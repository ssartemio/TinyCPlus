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

