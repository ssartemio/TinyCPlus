# TinyC+ implementation ledger

Requested scope: the full roadmap through 1.0 in TinyCPlus_SPEC_Codex.md.

The supplied document is the product specification; its future examples are design
inputs, not evidence of existing functionality. Status is backed by tests.

## Implemented and exercised

- [x] C11 compiler: lexer, typed AST, diagnostics, C lowering, native backend
- [x] Core: variables, control flow, memory, defer, arrays, slices, strings, tuples
- [x] Objects: classes, destruction, properties, extensions, interfaces, named calls
- [x] Generics, collections, C FFI, lambdas, closures, range and fused streams
- [x] Modules, formatter, documentation, persistent REPL, native @test runner
- [x] Threads, synchronization, worker tasks, futures, channels, cancellation
- [x] Async state machines, TaskGroup, asynchronous TCP/DNS/files and UDP receive
- [x] TCP/UDP/DNS and HTTP/2 transport
- [x] Protobuf generation and interoperable unary gRPC client/server
- [x] Terminal UI, layout, events and gap-buffer editor
- [x] Integration, negative, differential, golden lowering, deterministic fuzz tests
- [x] Offline dependency bootstrap, reference docs, local benchmarks, portable package

## Validation record, 2026-09-15

The integrated GCC-built frontend passed 22/22 stages of tests/run_all.py,
including 1,163 compiler checks (1,000 deterministic fuzz cases and all executable
language cases compared against GCC). Runtime concurrency, network and TUI tests
passed with TinyCC and GCC. Protobuf and gRPC interoperability passed against
the independent Python implementations using both native backends.

After the documentation formatter improvement, the TinyCC-built frontend passed
1,099 compiler checks and the tooling tests. API references were regenerated from
the 15 standard-library modules. Original dependency archives were checksum-verified
and successfully rebuilt offline with GCC; TinyCC core remains unchanged.

The portable ZIP passed every file checksum and ten relocated checks, including
rebuilding the compiler, JIT examples, generated gRPC, FFI, REPL and editor replay.
TinyEdit also ran through the native Windows ConPTY driver: Unicode/multiline
input, Ctrl-S save verified on disk, Ctrl-Q exit 0 and cursor restoration.

Known limits and the distinction between implemented features and release
qualification are explicit in docs/STATUS.md. Linux/macOS and sanitizers require
their corresponding test environments. The
release is marked 1.0.0-rc.1; it is not presented as a certified stable release.
