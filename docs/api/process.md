# API reference

Source: `std/process.tc`

Module: `std.process`

## tc_process_spawn

```c
i32 tc_process_spawn(string* arguments, u64 count, i32* error);
```

## tc_process_command

```c
i32 tc_process_command(string command);
```

## tc_environment

```c
string tc_environment(string name);
```

## tc_argument_count

```c
i32 tc_argument_count();
```

## tc_argument

```c
string tc_argument(i32 index);
```

## Arguments

```c
class Arguments
```

### Arguments.count

```c
static i32 count();
```

### Arguments.get

```c
static string get(i32 index);
```

## Process

```c
class Process
```

### Process.run

Executes a shell command explicitly; return status follows the host C runtime.

```c
static i32 run(string command);
```

### Process.spawn

Runs arguments[0] with the remaining arguments, without a shell, and
waits for it. Returns (exit status, error): error is 0 when the program
ran, non-zero when it could not be started. On POSIX a signal gives 128+n.

```c
static (i32, i32) spawn(Array<string>* arguments);
```

### Process.environment

Borrowed environment view; no allocation.

```c
static string environment(string name);
```

