# API reference

Source: `std/errors.tc`

Module: `std.errors`

## tc_system_error_category

```c
i32 tc_system_error_category(i32 error);
```

## ErrorCategory

Stable categories independent of native errno/Winsock numbers.

```c
enum ErrorCategory {
    None = 0,
    Cancelled = 1,
    NotFound = 2,
    Invalid = 3,
    Timeout = 4,
    Exhausted = 5,
    Permission = 6,
    Io = 7,
    Closed = 8
}
```

## TaskError

```c
enum TaskError {
    None = 0,
    Cancelled = 1,
    Invalid = 3,
    Timeout = 4,
    Exhausted = 8,
    Io = 13
}
```

## ErrorInfo

```c
class ErrorInfo
```

### ErrorInfo.category

```c
ErrorCategory category;
```

### ErrorInfo.nativeError

```c
i32 nativeError;
```

### ErrorInfo.context

```c
string context;
```

### ErrorInfo.ok

```c
bool ok();
```

## Errors

```c
class Errors
```

### Errors.system

```c
static ErrorInfo system(i32 code, string context = "");
```

### Errors.task

Supply Task.nativeError() after result() to preserve a native I/O failure.

```c
static ErrorInfo task(i32 code, i32 nativeError = 0, string context = "");
```
