# API reference

Source: `std/process.tc`

Module: `std.process`

## tc_process_start

```c
void* tc_process_start(string* arguments, u64 count, i32* error);
```

## tc_process_wait

```c
i32 tc_process_wait(void* process, u64 timeout, i32* error);
```

## tc_process_cancel

```c
i32 tc_process_cancel(void* process);
```

## tc_process_destroy

```c
void tc_process_destroy(void* process);
```

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

### Process.runShell

Executes a shell command explicitly; return status follows the host C runtime.

```c
static i32 runShell(string command);
```

### Process.run

Compatibility alias for runShell. Prefer argv via spawn/start.

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

## ProcessHandle

Owned child. destroy kills and reaps a running child; wait does not consume it.
Methods require a live handle and must not be called concurrently.

```c
class ProcessHandle
```

### ProcessHandle.handle

```c
void* handle;
```

### ProcessHandle.start

```c
static (ProcessHandle, i32) start(Array<string>* arguments);
```

### ProcessHandle.wait

timeout=0 means no deadline; timeout returns (-1, 4) and leaves the child alive.

```c
(i32, i32) wait(u64 timeout = 0);
```

### ProcessHandle.cancel

```c
i32 cancel();
```

### ProcessHandle.destroy

```c
void destroy();
```

