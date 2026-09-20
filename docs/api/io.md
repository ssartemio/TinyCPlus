# API reference

Source: `std/io.tc`

Module: `std.io`

## tc_read_line

```c
string tc_read_line();
```

## tc_write_error

```c
void tc_write_error(string text);
```

## Console

```c
class Console
```

### Console.readLine

```c
static OwnedString readLine();
```

### Console.writeError

Writes text to standard error, after flushing standard output.

```c
static void writeError(string text);
```

### Console.writeErrorLine

```c
static void writeErrorLine(string text);
```

