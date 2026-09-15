# API reference

Source: `std/process.tc`

Module: `std.process`

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

### Process.environment

Borrowed environment view; no allocation.

```c
static string environment(string name);
```

